#include "radaudio_decoder.h"
#include "radaudio_sse.h"

#if defined(_x86_64) || defined( __x86_64__ ) || defined( _M_X64 ) || defined(__x86_64) || defined(_M_AMD64) || defined(__SSE2__) || defined(DO_BUILD_SSE4)

  #include <emmintrin.h>
  #define simdf __m128
  #define simdi32 __m128i
  #define simdi16 __m128i
  #define simdf_float_count 4
  #define SIMD_PATH

  #define simdf_load( ptr ) _mm_loadu_ps(ptr)
  #define simdf_mul_constant( src, scalar ) _mm_mul_ps( (src), _mm_set1_ps( scalar ) )
  #define simdf_min_constant( src, scalar ) _mm_min_ps( (src), _mm_set1_ps( scalar ) )
  #define simdf_max_constant( src, scalar ) _mm_max_ps( (src), _mm_set1_ps( scalar ) )
  #define simdf_to_simdi32( src ) _mm_cvttps_epi32( src )

  #define simdi32_packto16( rega, regb ) _mm_packs_epi32( (rega), (regb) )

  #define simdi16_store( ptr, values ) _mm_storeu_si128( (__m128i *) (ptr), (values) )

  static RADFORCEINLINE void simdi16_store2_interleaved(S16 * ptr, simdi16 rega, simdi16 regb)
  {
    simdi16_store(ptr + 0, _mm_unpacklo_epi16(rega, regb));
    simdi16_store(ptr + 8, _mm_unpackhi_epi16(rega, regb));
  }

  static RADFORCEINLINE void simdi16_store4_interleaved(S16 * ptr, simdi16 rega, simdi16 regb, simdi16 regc, simdi16 regd)
  {
    simdi16 ac0 = _mm_unpacklo_epi16(rega, regc);
    simdi16 ac1 = _mm_unpackhi_epi16(rega, regc);
    simdi16 bd0 = _mm_unpacklo_epi16(regb, regd);
    simdi16 bd1 = _mm_unpackhi_epi16(regb, regd);

    simdi16_store(ptr +  0, _mm_unpacklo_epi16(ac0, bd0));
    simdi16_store(ptr +  8, _mm_unpackhi_epi16(ac0, bd0));
    simdi16_store(ptr + 16, _mm_unpacklo_epi16(ac1, bd1));
    simdi16_store(ptr + 24, _mm_unpackhi_epi16(ac1, bd1));
  }

#elif defined( _M_ARM64 ) || defined( __aarch64__ ) || defined( __arm64__ ) || ((__ARM_NEON_FP & 4) != 0) || defined(__ARM_NEON__) || defined(DO_BUILD_NEON) || defined(__RADARM64__)

  #include <arm_neon.h>

  #define simdf float32x4_t
  #define simdi32 int32x4_t
  #define simdi16 int16x8_t
  #define simdf_float_count 4
  #define SIMD_PATH

  #define simdf_load( ptr ) vld1q_f32(ptr)
  #define simdf_mul_constant( src, scalar ) vmulq_n_f32( src, scalar )
  #define simdf_min_constant( src, scalar ) vminq_f32( src, vdupq_n_f32( scalar ) )
  #define simdf_max_constant( src, scalar ) vmaxq_f32( src, vdupq_n_f32( scalar ) )
  #define simdf_to_simdi32( src ) vcvtq_s32_f32( src )

  #define simdi32_packto16( rega, regb ) vcombine_s16( vmovn_s32( rega ), vmovn_s32( regb ) )

  #define simdi16_store( ptr, values ) vst1q_s16( (ptr), (values) )

  static RADFORCEINLINE void simdi16_store2_interleaved(S16 * ptr, simdi16 rega, simdi16 regb)
  {
    int16x8x2_t values = { rega, regb };
    vst2q_s16(ptr, values);
  }

  static RADFORCEINLINE void simdi16_store4_interleaved(S16 * ptr, simdi16 rega, simdi16 regb, simdi16 regc, simdi16 regd)
  {
    int16x8x4_t values = { rega, regb, regc, regd };
    vst4q_s16(ptr, values);
  }

#endif

#ifdef SIMD_PATH

static RADFORCEINLINE simdi32 simd_load_and_convert(float const * ptr)
{
  simdf floats = simdf_load(ptr);
  simdf scaled = simdf_mul_constant(floats, 32768.0f);
  simdf clamped_lo = simdf_max_constant(scaled, -32768.0f);
  simdf clamped_hi = simdf_min_constant(clamped_lo, 32767.0f);
  simdi32 integer = simdf_to_simdi32(clamped_hi);
  return integer;
}

static RADFORCEINLINE simdi16 simd_load_convert_narrow(float const * ptr)
{
  simdi32 lo = simd_load_and_convert(ptr);
  simdi32 hi = simd_load_and_convert(ptr + simdf_float_count);
  return simdi32_packto16(lo, hi);
}

#endif

int   RadAudio1ChToS16( S16 * output, float const * input, U32 samples )
{
  #ifndef SIMD_PATH

  return 0;

  #else

  S16 * end_output;
  float const * end_input_m8;

  // if buffer is so small that we can't do anything, fail
  if ( samples < simdf_float_count*2 )
    return 0;

  // as long as we have at least simdf_float_count*2 samples, this loop handles all of them (no remenants).
  end_input_m8 = input + samples - simdf_float_count*2;
  end_output = output + samples - simdf_float_count*2;
  for(;;)
  {
    simdi16 o = simd_load_convert_narrow(input);
    simdi16_store(output, o);
    input += simdf_float_count*2;
    output += simdf_float_count*2;
    if ( output <= end_output )
      continue;
    if ( output == ( end_output + simdf_float_count*2 ) )
      return 1;
    output = end_output;     // backup and do last couple
    input = end_input_m8;
  }
  #endif
}

int   RadAudioInterleave2ChToS16( S16 * output, float const * input_left, float const * input_right, U32 samples )
{
  #ifndef SIMD_PATH

  return 0;

  #else

  S16 * end_output;
  float const * end_left_m8;
  float const * end_right_m8;

  // if buffer is so small that we can't do anything, fail
  if ( samples < simdf_float_count*2 )
    return 0;

  // as long as we have at least simdf_float_count*2 samples, this loop handles all of them (no remenants).
  end_left_m8  = input_left  + samples - simdf_float_count*2;
  end_right_m8 = input_right + samples - simdf_float_count*2;
  end_output = output + samples*2 - simdf_float_count*2*2; // 2 for two simd regs, and 2 for stereo
  for(;;)
  {
    simdi16 ol = simd_load_convert_narrow(input_left);
    simdi16 or = simd_load_convert_narrow(input_right);
    simdi16_store2_interleaved(output, ol, or);
    input_left += simdf_float_count*2;
    input_right += simdf_float_count*2;
    output += simdf_float_count*2*2;
    if ( output <= end_output )
      continue;
    if ( output == ( end_output + simdf_float_count*2*2 ) )
      return 1;
    output = end_output;     // backup and do last couple
    input_left  = end_left_m8;
    input_right = end_right_m8;
  }

  #endif
}

int   RadAudioInterleave4ChToS16( S16 * output, float const * input_left, float const * input_right, float const * input_back_left, float const * input_back_right, U32 samples )
{
  #ifndef SIMD_PATH

  return 0;

  #else

  S16 * end_output;
  float const * end_fleft_m8;
  float const * end_fright_m8;
  float const * end_bleft_m8;
  float const * end_bright_m8;

  // if buffer is so small that we can't do anything, fail
  if ( samples < simdf_float_count*2 )
    return 0;

  // as long as we have at least simdf_float_count*2 samples, this loop handles all of them (no remenants).
  end_fleft_m8  = input_left  + samples - simdf_float_count*2;
  end_fright_m8 = input_right + samples - simdf_float_count*2;
  end_bleft_m8  = input_back_left  + samples - simdf_float_count*2;
  end_bright_m8 = input_back_right + samples - simdf_float_count*2;
  end_output = output + samples*4 - simdf_float_count*2*4; // 2 for two simd regs, and 4 for quad-chan
  for(;;)
  {
    simdi16 ofl = simd_load_convert_narrow(input_left);
    simdi16 ofr = simd_load_convert_narrow(input_right);
    simdi16 obl = simd_load_convert_narrow(input_back_left);
    simdi16 obr = simd_load_convert_narrow(input_back_right);
    simdi16_store4_interleaved(output, ofl, ofr, obl, obr);

    input_left += simdf_float_count*2;
    input_right += simdf_float_count*2;
    input_back_left += simdf_float_count*2;
    input_back_right += simdf_float_count*2;
    output += simdf_float_count*2*4;
    if ( output <= end_output )
      continue;
    if ( output == ( end_output + simdf_float_count*2*4 ) )
      return 1;
    output = end_output;     // backup and do last couple
    input_left  = end_fleft_m8;
    input_right = end_fright_m8;
    input_back_left  = end_bleft_m8;
    input_back_right = end_bright_m8;
  }

  #endif
}

void   RadAudioInterleave( S16 * output, float const ** inputs, U32 input_count, U32 samples )
{
  U32 SampleIdx;
  switch( input_count )
  {
    case 0:
      break;
    case 1:
      if ( RadAudio1ChToS16( output, inputs[0], samples ) )
        return;
      break;
    case 2:
      if ( RadAudioInterleave2ChToS16( output, inputs[0], inputs[1], samples ) )
        return;
      break;
    case 4:
      if ( RadAudioInterleave4ChToS16( output, inputs[0], inputs[1], inputs[2], inputs[3], samples ) )
        return;
      break;
    default:
      break;
  }

  for (SampleIdx = 0; SampleIdx < samples; SampleIdx++)
  {
    U32 ChannelIdx;
    for (ChannelIdx = 0; ChannelIdx < input_count; ChannelIdx++)
    {
      float InBufferFloat = inputs[ChannelIdx][SampleIdx] * 32768.0f;
      if ( InBufferFloat < -32768.0f )
        InBufferFloat = -32768.0f;
      if ( InBufferFloat > 32767.0f )
        InBufferFloat = 32767.0f;

      *output++ = (S16)InBufferFloat;
    }
  }
}
