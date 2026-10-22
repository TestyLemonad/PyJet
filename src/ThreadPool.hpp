#ifndef JET_THREAD_POOL_HPP
#define JET_THREAD_POOL_HPP

// A small persistent fork-join pool used by the desktop rasterizer.
//
// The pool deliberately has no task queue API: Scene submits one parallel-for
// batch per frame and the calling thread participates in the work. That keeps
// synchronisation and allocation out of the hot per-tile path and makes the
// worker count predictable. Embedded builds compile this header out entirely
// via JET_THREADED_RASTER.

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Renderer {

class ThreadPool {
public:
    explicit ThreadPool(unsigned requestedWorkers)
    {
        if (requestedWorkers == 0) requestedWorkers = 1;
        // One participant is the calling thread. Spawn the rest.
        const unsigned workers = requestedWorkers > 1 ? requestedWorkers - 1 : 0;
        threads_.reserve(workers);
        try {
            for (unsigned i = 0; i < workers; ++i) {
                threads_.emplace_back([this] { workerLoop(); });
            }
        } catch (...) {
            stop();
            throw;
        }
    }

    ~ThreadPool() { stop(); }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Total participants, including the calling thread.
    unsigned threadCount() const
    {
        return static_cast<unsigned>(threads_.size()) + 1u;
    }

    /// Run fn(0..count-1) across the pool. The calling thread also works.
    /// A nested call runs inline instead of deadlocking.
    void parallelFor(int count, const std::function<void(int)>& fn)
    {
        if (count <= 0) return;
        if (count == 1 || threads_.empty() || busy_.exchange(true)) {
            for (int i = 0; i < count; ++i) fn(i);
            return;
        }

        Batch batch;
        batch.fn = &fn;
        batch.count = count;
        batch.remaining.store(static_cast<int>(threads_.size()) + 1,
                              std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (size_t i = 0; i < threads_.size(); ++i) {
                tasks_.emplace_back([this, &batch] { runBatch(batch); });
            }
        }
        cv_.notify_all();

        runBatch(batch);

        std::exception_ptr error;
        {
            std::unique_lock<std::mutex> lock(doneMutex_);
            doneCv_.wait(lock, [&batch] {
                return batch.remaining.load(std::memory_order_acquire) == 0;
            });
            error = batch.error;
        }
        busy_.store(false, std::memory_order_release);
        if (error) std::rethrow_exception(error);
    }

private:
    struct Batch {
        const std::function<void(int)>* fn = nullptr;
        int count = 0;
        std::atomic<int> next{0};
        std::atomic<int> remaining{0};
        std::exception_ptr error;
    };

    void runBatch(Batch& batch)
    {
        try {
            for (;;) {
                const int index = batch.next.fetch_add(1, std::memory_order_relaxed);
                if (index >= batch.count) break;
                (*batch.fn)(index);
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(doneMutex_);
            if (!batch.error) batch.error = std::current_exception();
        }

        // Decrement the predicate while holding doneMutex_.  Updating it
        // outside the lock lets the notifying thread slip between the
        // waiter's predicate check and its actual block, losing the wakeup.
        bool last = false;
        {
            std::lock_guard<std::mutex> lock(doneMutex_);
            last = batch.remaining.fetch_sub(1, std::memory_order_acq_rel) == 1;
        }
        if (last) doneCv_.notify_all();
    }

    void workerLoop()
    {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
                if (stopped_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.erase(tasks_.begin());
            }
            task();
        }
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopped_) return;
            stopped_ = true;
        }
        cv_.notify_all();
        for (std::thread& thread : threads_) {
            if (thread.joinable()) thread.join();
        }
        threads_.clear();
    }

    std::vector<std::thread> threads_;
    std::vector<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::mutex doneMutex_;
    std::condition_variable doneCv_;
    std::atomic<bool> busy_{false};
    bool stopped_ = false;
};

} // namespace Renderer

#endif // JET_THREAD_POOL_HPP
