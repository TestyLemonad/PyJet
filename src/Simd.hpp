#ifndef JET_SIMD_HPP
#define JET_SIMD_HPP

// Desktop SIMD helpers for the RGB565 software rasterizer.
//
// AVX2 is selected at runtime on GCC/Clang x86 builds. MSVC builds use the
// guaranteed x86-64 SSE2 baseline unless the translation unit is compiled with
// /arch:AVX2. Non-x86 platforms and embedded targets fall back to scalar code.
// All stores are unaligned because framebuffer spans start on arbitrary x.

#include <cstddef>
#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
    #if defined(_MSC_VER)
        #if defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
            #define JET_SIMD_SSE2 1
        #endif
    #else
        #if defined(__SSE2__)
            #define JET_SIMD_SSE2 1
        #endif
    #endif
    #include <emmintrin.h>
    #if defined(__AVX2__)
        #include <immintrin.h>
        #define JET_SIMD_AVX2_COMPILED 1
    #endif
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    #include <immintrin.h>
    #define JET_SIMD_GCC_AVX2 1
    #define JET_SIMD_AVX2_TARGET __attribute__((target("avx2")))
#else
    #define JET_SIMD_AVX2_TARGET
#endif

namespace Renderer {
namespace Simd {
namespace Detail {

inline void fillRGB565Scalar(uint16_t* dst, size_t count, uint16_t color)
{
    if (count == 0) return;
    // Use 32-bit pair stores when the pointer alignment allows it; this is the
    // fallback on embedded/ARM targets and the tail after the SIMD loops.
    size_t start = 0;
    if ((reinterpret_cast<uintptr_t>(dst) & 2u) != 0) {
        dst[0] = color;
        start = 1;
    }
    const size_t pairs = (count - start) >> 1;
    uint32_t* out = reinterpret_cast<uint32_t*>(dst + start);
    const uint32_t value = static_cast<uint32_t>(color) |
                           (static_cast<uint32_t>(color) << 16);
    for (size_t i = 0; i < pairs; ++i) out[i] = value;
    if (((count - start) & 1u) != 0) dst[count - 1] = color;
}

#if defined(JET_SIMD_SSE2)
inline void fillRGB565SSE2(uint16_t* dst, size_t count, uint16_t color)
{
    const __m128i value = _mm_set1_epi16(static_cast<int16_t>(color));
    size_t i = 0;
    for (; i + 16 <= count; i += 16) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), value);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i + 8), value);
    }
    for (; i + 8 <= count; i += 8) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), value);
    }
    fillRGB565Scalar(dst + i, count - i, color);
}
#endif

#if defined(JET_SIMD_GCC_AVX2)
JET_SIMD_AVX2_TARGET
inline void fillRGB565AVX2(uint16_t* dst, size_t count, uint16_t color)
{
    const __m256i value = _mm256_set1_epi16(static_cast<int16_t>(color));
    size_t i = 0;
    for (; i + 32 <= count; i += 32) {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), value);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 16), value);
    }
    for (; i + 16 <= count; i += 16) {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), value);
    }
    _mm256_zeroupper();
    fillRGB565Scalar(dst + i, count - i, color);
}
#elif defined(JET_SIMD_AVX2_COMPILED)
inline void fillRGB565AVX2(uint16_t* dst, size_t count, uint16_t color)
{
    const __m256i value = _mm256_set1_epi16(static_cast<int16_t>(color));
    size_t i = 0;
    for (; i + 32 <= count; i += 32) {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), value);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i + 16), value);
    }
    for (; i + 16 <= count; i += 16) {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), value);
    }
    _mm256_zeroupper();
    fillRGB565Scalar(dst + i, count - i, color);
}
#endif

inline bool avx2Available()
{
#if defined(JET_SIMD_GCC_AVX2)
    static const bool available = [] {
        __builtin_cpu_init();
        return __builtin_cpu_supports("avx2") != 0;
    }();
    return available;
#else
    return false;
#endif
}

} // namespace Detail

/// Fill count RGB565 pixels with one colour.
inline void fillRGB565(uint16_t* dst, size_t count, uint16_t color)
{
    if (count == 0) return;
#if defined(JET_SIMD_GCC_AVX2) || defined(JET_SIMD_AVX2_COMPILED)
    if (Detail::avx2Available()) {
        Detail::fillRGB565AVX2(dst, count, color);
        return;
    }
#endif
#if defined(JET_SIMD_SSE2)
    Detail::fillRGB565SSE2(dst, count, color);
#else
    Detail::fillRGB565Scalar(dst, count, color);
#endif
}

} // namespace Simd
} // namespace Renderer

#endif // JET_SIMD_HPP
