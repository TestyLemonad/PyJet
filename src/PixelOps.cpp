#include "PixelOps.hpp"
#include "Simd.hpp"

#include <cstring>

namespace Renderer {
namespace {

// Exact 5/6-bit to 8-bit expansion for every possible channel value.
// This is the same integer rounding used by the historical Python converter:
//   round(c * 255 / max) == (c * 255 + max / 2) / max
inline uint8_t expand5(uint16_t value)
{
    return static_cast<uint8_t>((value * 527u + 23u) >> 6);
}

inline uint8_t expand6(uint16_t value)
{
    return static_cast<uint8_t>((value * 259u + 33u) >> 6);
}

void convertRGB565ToRGB888Scalar(const uint16_t* src, uint8_t* dst, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const uint16_t value = src[i];
        *dst++ = expand5(static_cast<uint16_t>((value >> 11) & 0x1Fu));
        *dst++ = expand6(static_cast<uint16_t>((value >> 5) & 0x3Fu));
        *dst++ = expand5(static_cast<uint16_t>(value & 0x1Fu));
    }
}

void convertRGB565ToRGBA8888Scalar(const uint16_t* src, uint32_t* dst, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const uint16_t value = src[i];
        const uint32_t r = expand5(static_cast<uint16_t>((value >> 11) & 0x1Fu));
        const uint32_t g = expand6(static_cast<uint16_t>((value >> 5) & 0x3Fu));
        const uint32_t b = expand5(static_cast<uint16_t>(value & 0x1Fu));
        dst[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
    }
}

#if defined(JET_SIMD_SSE2)
inline void expand5x8(__m128i values, __m128i& result)
{
    result = _mm_srli_epi16(
        _mm_add_epi16(_mm_mullo_epi16(values, _mm_set1_epi16(527)), _mm_set1_epi16(23)),
        6);
}

inline void expand6x8(__m128i values, __m128i& result)
{
    result = _mm_srli_epi16(
        _mm_add_epi16(_mm_mullo_epi16(values, _mm_set1_epi16(259)), _mm_set1_epi16(33)),
        6);
}

inline void splitChannels(__m128i packed, __m128i& r, __m128i& g, __m128i& b)
{
    const __m128i mask5 = _mm_set1_epi16(0x1F);
    const __m128i mask6 = _mm_set1_epi16(0x3F);
    expand5x8(_mm_and_si128(_mm_srli_epi16(packed, 11), mask5), r);
    expand6x8(_mm_and_si128(_mm_srli_epi16(packed, 5), mask6), g);
    expand5x8(_mm_and_si128(packed, mask5), b);
}

void convertRGB565ToRGBA8888SSE2(const uint16_t* src, uint32_t* dst, size_t count)
{
    const __m128i zero = _mm_setzero_si128();
    const __m128i alpha = _mm_set1_epi32(-16777216); // 0xFF000000
    size_t i = 0;
    for (; i + 8 <= count; i += 8) {
        __m128i r, g, b;
        splitChannels(_mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i)),
                      r, g, b);

        const __m128i rlo = _mm_unpacklo_epi16(r, zero);
        const __m128i glo = _mm_unpacklo_epi16(g, zero);
        const __m128i blo = _mm_unpacklo_epi16(b, zero);
        const __m128i lo = _mm_or_si128(
            _mm_or_si128(rlo, _mm_slli_epi32(glo, 8)),
            _mm_or_si128(_mm_slli_epi32(blo, 16), alpha));

        const __m128i rhi = _mm_unpackhi_epi16(r, zero);
        const __m128i ghi = _mm_unpackhi_epi16(g, zero);
        const __m128i bhi = _mm_unpackhi_epi16(b, zero);
        const __m128i hi = _mm_or_si128(
            _mm_or_si128(rhi, _mm_slli_epi32(ghi, 8)),
            _mm_or_si128(_mm_slli_epi32(bhi, 16), alpha));

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), lo);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i + 4), hi);
    }
    convertRGB565ToRGBA8888Scalar(src + i, dst + i, count - i);
}
#endif // JET_SIMD_SSE2

#if (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
#define JET_PIXELOPS_SSSE3 1
#define JET_PIXELOPS_SSSE3_TARGET __attribute__((target("ssse3")))
#else
#define JET_PIXELOPS_SSSE3 0
#define JET_PIXELOPS_SSSE3_TARGET
#endif

#if JET_PIXELOPS_SSSE3
JET_PIXELOPS_SSSE3_TARGET
void convertRGB565ToRGB888SSSE3(const uint16_t* src, uint8_t* dst, size_t count)
{
    const __m128i maskRg = _mm_setr_epi8(
        0, 1, -1, 2, 3, -1, 4, 5, -1, 6, 7, -1, -1, -1, -1, -1);
    const __m128i maskB = _mm_setr_epi8(
        -1, -1, 0, -1, -1, 2, -1, -1, 4, -1, -1, 6, -1, -1, -1, -1);

    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        const __m128i packed = _mm_loadl_epi64(
            reinterpret_cast<const __m128i*>(src + i));
        __m128i r, g, b;
        splitChannels(packed, r, g, b);

        const __m128i rg = _mm_or_si128(r, _mm_slli_epi16(g, 8));
        const __m128i out = _mm_or_si128(
            _mm_shuffle_epi8(rg, maskRg),
            _mm_shuffle_epi8(b, maskB));

        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst), out);
        const uint32_t tail = static_cast<uint32_t>(
            _mm_cvtsi128_si32(_mm_srli_si128(out, 8)));
        std::memcpy(dst + 8, &tail, sizeof(tail));
        dst += 12;
    }
    convertRGB565ToRGB888Scalar(src + i, dst, count - i);
}

bool ssse3Available()
{
    static const bool available = [] {
        __builtin_cpu_init();
        return __builtin_cpu_supports("ssse3") != 0;
    }();
    return available;
}
#endif // JET_PIXELOPS_SSSE3

} // namespace

void convertRGB565ToRGB888(const uint16_t* src, uint8_t* dst, size_t count)
{
    if (!src || !dst) return;
#if JET_PIXELOPS_SSSE3
    if (ssse3Available()) {
        convertRGB565ToRGB888SSSE3(src, dst, count);
        return;
    }
#endif
    convertRGB565ToRGB888Scalar(src, dst, count);
}

void convertRGB565ToRGBA8888(const uint16_t* src, uint32_t* dst, size_t count)
{
    if (!src || !dst) return;
#if defined(JET_SIMD_SSE2)
    convertRGB565ToRGBA8888SSE2(src, dst, count);
#else
    convertRGB565ToRGBA8888Scalar(src, dst, count);
#endif
}


} // namespace Renderer
