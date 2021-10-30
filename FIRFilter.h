#pragma once

#include "BaseTypes.h"
#include "AlignedVector.h"
#include "MathHelpers.h"


class FIRFilter
{
public:
  static constexpr u32 k_initialZeroPadding = k_maxFloatAlignment - 1;
  u32 Length() const
    { return actualFilterLength; }

  static FIRFilter Convolve(const FIRFilter &a, const FIRFilter &b)
  {
    FIRFilter f;
    f.actualFilterLength = a.Length() + b.Length() - 1;

    // We want to allocate up to a multiple of 16 for efficiency.
    f.coefficients.resize(AlignInt(2*k_initialZeroPadding + f.actualFilterLength, k_maxFloatAlignment));
    memset(f.coefficients.data(), 0, f.coefficients.size() * sizeof(f.coefficients[0]));

    for (u32 aI = 0; aI < a.Length(); aI++)
    {
      for (u32 bI = 0; bI < b.Length(); bI++)
      {
        f.coefficients[aI + bI + k_initialZeroPadding] += a.coefficients[aI + k_initialZeroPadding] * b.coefficients[bI + k_initialZeroPadding];
      }
    }

    return f;
  }

  static FIRFilter CreateLowPass(f32 rolloffEnd)
  {
    s32 lengthGuess = 11;
    s32 prevGuess = -1;
    bool everTooLong = false;

    // Search the space for 
    for (;;)
    {
      FIRFilter f = CreateLowPassFromEndAndLength(rolloffEnd, lengthGuess, true);

      // Now count the number of positive peaks we have. We're aiming for cuttong off JUST after the second positive peak (not counting the center peak)

      s32 centerIndex = s32(f.Length() / 2);

      s32 negativeCrossingCount = 0;
      bool wasNegative = false;
      bool tooLong = false;
      for (s32 i = centerIndex - 1; i >= 0; i--)
      {
        bool isNegative = f.coefficients[i + k_initialZeroPadding] < 0.0f;
        if (isNegative && !wasNegative)
        {
          negativeCrossingCount++;
        }

        if (negativeCrossingCount == 2 
          && f.coefficients[i + k_initialZeroPadding] > f.coefficients[i + 1 + k_initialZeroPadding])
        {
          // We're in the second negative block and we're on the upswing.
          if (f.coefficients[i + 1 + k_initialZeroPadding] > -0.00005f)
          {
            return CreateLowPassFromEndAndLength(rolloffEnd, lengthGuess, true);
          }

          tooLong = true;
          break;
        }

        wasNegative = isNegative;
      }

      if (!tooLong)
      {
        // Not enough peaks found, try again
        // Could attempt to binary search here, but whatever
        if (prevGuess >= 0 && prevGuess > lengthGuess)
        {
          // We reversed direction so take the larger of the two
          return CreateLowPassFromEndAndLength(rolloffEnd, prevGuess, true);
        }

        prevGuess = lengthGuess;

        lengthGuess = (lengthGuess * 2) + 1;
      }
      else
      {
        if (everTooLong && prevGuess >= 0 && prevGuess < lengthGuess)
        {
          // We reversed direction so take the larger of the two
          return CreateLowPassFromEndAndLength(rolloffEnd, lengthGuess, true);
        }

        everTooLong = true;
        prevGuess = lengthGuess;
        lengthGuess -= 2;
      }
    }
  }


  static FIRFilter CreateLowPass(f32 rolloffStart, f32 rolloffEnd)
  {
    ASSERT(rolloffStart < rolloffEnd);

    f32 rolloffCenter = (rolloffStart + rolloffEnd) * 0.5f;
    f32 bandwidth = rolloffEnd - rolloffStart;

    // To get the expected bandwidth, we need to set the filter length. A good approximation for a low-pass filter's bandwidth is:
    //  bandwidth = 4.6 / length
    // However, it's best to ensure an odd-length filter so that we have a perfect center point.
    u32 filterLength = u32(std::ceil(4.6f / bandwidth)) | 1;

    return CreateLowPassFromRolloffCenterAndLength(rolloffCenter, filterLength);
  }

  static FIRFilter CreateLowPassFromEndAndLength(f32 rolloffEnd, u32 filterLength, bool useWindow = true)
  {
    f32 bandwidth = 4.6f / f32(filterLength);

    return CreateLowPassFromRolloffCenterAndLength(rolloffEnd - bandwidth * 0.5f, filterLength, useWindow);
  }

  static FIRFilter CreateLowPassFromRolloffCenterAndLength(f32 rolloffCenter, u32 filterLength, bool useWindow = true)
  {
    ASSERT((filterLength & 1) != 0);
    FIRFilter f;
    f.actualFilterLength = filterLength;
    f.coefficients.resize(AlignInt(filterLength + 2*k_initialZeroPadding, k_maxFloatAlignment));
    memset(f.coefficients.data(), 0, f.coefficients.size() * sizeof(f.coefficients[0]));

    // We're generating a simple windowed-sinc filter. The sinc function for an ideal lowpass is:
    //  h[n] = 2 * frequencyCutoff * SincPi(2 * frequencyCutoff * (n - (length - 1) / 2))
    //  (However, we're going to drop the 2 * frequencyCutoff term from this because we're going to be renormalizing the filter so it has a sum of 1.0 anyway), so what we're actually
    //   calculating is:
    //  h[n] = SincPi(frequencyCutoff * (2*n - (length - 1)))

    // However we can't generate an ideal lowpass, what with floating point error and needing infinite history and all that, so instead we window it out using a simple 
    //  Blackman window
    // w[n] = 0.42 - 0.5 * CosPi(2n / (length - 1)) + 0.08 CosPi(4n / N - 1)

    // Simply multiply the two terms together to get our final filter result
  
    // If we actually have precision issues it's possible to sort the values by smallest absolute value to largest and calculate the sum from that list, which will get a much more
    //  accurate sum, but this is almost surely fine in practice.
    f32 sum = 0.0f;
    for (u32 i = 0; i < filterLength; i++)
    {
      f32 result = SincPi(rolloffCenter * (2.0f * f32(i) - f32(filterLength - 1)));

      if (useWindow)
      {
        result *= 0.42f - 0.5f * CosPi(2.0f * f32(i) / f32(filterLength - 1)) + 0.08f * CosPi(4.0f * f32(i) / f32(filterLength - 1));
      }

      sum += result;
      f.coefficients[i + k_initialZeroPadding] = result;
    }

    for (u32 i = 0; i < filterLength; i++)
    {
      f.coefficients[i + k_initialZeroPadding] /= sum;
    }

    return f;
  }

  static FIRFilter CreateHighPass(f32 rolloffStart)
  {
    // Generating a high pass is the same as generating a low-pass filter, but then doing a spectral inversion on it (flipping the signs of everything in the
    //  low-pass filter then adding 1.0 to the center value)
    FIRFilter f = CreateLowPass(rolloffStart);

    for (u32 i = 0; i < f.Length(); i++)
    {
      f.coefficients[i + k_initialZeroPadding] *= -1.0f;
    }

    f.coefficients[(f.Length() - 1) / 2 + k_initialZeroPadding] += 1.0f;

    return f;  
  }

  static FIRFilter CreateHighPass(f32 rolloffStart, f32 rolloffEnd)
  {
    ASSERT(rolloffStart > rolloffEnd);
    // Generating a high pass is the same as generating a low-pass filter, but then doing a spectral inversion on it (flipping the signs of everything in the
    //  low-pass filter then adding 1.0 to the center value)
    FIRFilter f = CreateLowPass(rolloffEnd, rolloffStart);

    for (u32 i = 0; i < f.Length(); i++)
    {
      f.coefficients[i + k_initialZeroPadding] *= -1.0f;
    }

    f.coefficients[(f.Length() - 1) / 2 + k_initialZeroPadding] += 1.0f;
    return f;
  }

  static FIRFilter CreateHighPassFromStartAndLength(f32 rolloffStart, u32 filterLength)
  {
    f32 bandwidth = 4.6f / f32(filterLength);

    FIRFilter f = CreateLowPassFromRolloffCenterAndLength(rolloffStart - bandwidth * 0.5f, filterLength);

    for (u32 i = 0; i < f.Length(); i++)
    {
      f.coefficients[i + k_initialZeroPadding] *= -1.0f;
    }

    f.coefficients[(f.Length() - 1) / 2 + k_initialZeroPadding] += 1.0f;
    return f;
  }


  static FIRFilter CreateBandPass(f32 lowerStop, f32 lowerStart, f32 upperStart, f32 upperStop)
  {
    return Convolve(
      CreateLowPass(upperStart, upperStop),
      CreateHighPass(lowerStart, lowerStop));
  }


  void Process(const AlignedVector<f32> &valuesIn, AlignedVector<f32> *pValuesOut) const
  {
    switch (k_maxInstructionSet)
    {
    case SIMDInstructionSet::AVX:
      ProcessAvx<false>(valuesIn, pValuesOut);
      return;

    case SIMDInstructionSet::AVX2:
      ProcessAvx<true>(valuesIn, pValuesOut);
      return;

    case SIMDInstructionSet::None:
      break;
    }

    for (u32 i = 0; i < valuesIn.size(); i++)
    {
      (*pValuesOut)[i] = ProcessOneElement(valuesIn.data(), u32(valuesIn.size()), i);
    }
  }

  f32 ProcessOneElement(const f32 *values, u32 valueArrayLength, u32 centerInputIndex) const
  {
    u32 halfLength = (Length() - 1) / 2;
    u32 startFilterIndex = 0;
    u32 startArrayIndex;
    if (centerInputIndex < halfLength)
    {
      // We have a center early enough in the array that we have to clamp to the edge (we assume all values before the array are zeroes)
      startArrayIndex = 0;
      startFilterIndex = halfLength - centerInputIndex;
    }
    else
    {
      // We can use the whole front of the filter, so just start at the corresponding left-hand input
      startArrayIndex = centerInputIndex - halfLength;
    }

    // End the processing at the end of the filter or the end of the input, whichever comes first
    u32 endArrayIndex = std::min(valueArrayLength, centerInputIndex + halfLength + 1);
    u32 count = endArrayIndex - startArrayIndex;
    ASSERT(count <= Length());

    f32 result = 0.0f;
    for (u32 i = 0; i < count; i++)
    {
      result += values[i + startArrayIndex] * coefficients[i + startFilterIndex + k_initialZeroPadding];
    }

    return result;
  }

protected:
template <bool UseAVX2>
  void ProcessAvx(const AlignedVector<f32> &valuesIn, AlignedVector<f32> *pValuesOut) const
  {
    // This implementation is based off of the paper "Efficient Vectorization of the FIR Filter"  
    //  (https://aes.tu-berlin.de/fileadmin/fg196/publication/old-juurlink/efficient_vectorization_of_the_fir_filter.pdf)
    //  ...and it was a nightmare to figure out how to wiggle all the data through as efficiently as possible based on the paper
    
    constexpr u32 k_setSize = 8; // 8 floats for AVX2 processing

    u32 firstFullSet;     // Index of the first input sample where we'll start the FIR from the beginning
    u32 filterSetCount;   // How many sets long our filter is (including the start padding)
    const f32 *coeffs;    // Index into the coefficient array with the correct offset (i.e. the correct amount of initial padding zeros)
    {
      u32 halfLength = (Length() - 1) / 2;
      u32 firstFullSample = AlignInt(halfLength, k_setSize);
      firstFullSet = firstFullSample / k_setSize;
      u32 filterOffset = (firstFullSample - halfLength) % k_setSize;
      filterSetCount = AlignInt(actualFilterLength + filterOffset, k_setSize) / k_setSize;
      coeffs = &coefficients[FIRFilter::k_initialZeroPadding - filterOffset];
    }

    auto inputStart = &valuesIn[0];
    auto output = &(*pValuesOut)[0];

    auto Accumulate = [](__m256 accumulator, __m256 firstInput, __m256 secondInput, const float *fc)
    {
      // This code does the quivalent of: where input[0-7] are firstInput and input [8-15] are secondInput
      //for (u32 i = 0; i < k_setSize; i++)
      //{
      //  accumulator[i] +=
      //      fc[0] * input[0 + i]
      //    + fc[1] * input[1 + i]
      //    + fc[2] * input[2 + i]
      //    + fc[3] * input[3 + i]
      //    + fc[4] * input[4 + i]
      //    + fc[5] * input[5 + i]
      //    + fc[6] * input[6 + i]
      //    + fc[7] * input[7 + i];
      //}

      // To facilitate shifting these around for the following multiplies without having issues with moving data between SIMD lanes, build an additional vector that is the inside 
      __m256 innerInput = _mm256_permute2f128_ps(firstInput, secondInput, 0x21);

      ASSERT(fc[0] >= -1.0f && fc[0] <= 1.0f);
      ASSERT(fc[1] >= -1.0f && fc[1] <= 1.0f);
      ASSERT(fc[2] >= -1.0f && fc[2] <= 1.0f);
      ASSERT(fc[3] >= -1.0f && fc[3] <= 1.0f);
      ASSERT(fc[4] >= -1.0f && fc[4] <= 1.0f);
      ASSERT(fc[5] >= -1.0f && fc[5] <= 1.0f);
      ASSERT(fc[6] >= -1.0f && fc[6] <= 1.0f);
      ASSERT(fc[7] >= -1.0f && fc[7] <= 1.0f);

      __m256 coeff = _mm256_broadcast_ss(&fc[0]);
      accumulator = _mm256_fmadd_ps(coeff, firstInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[1]);
      // Combine our two lane-mixed input values (innerInput and firstInput) into effectively i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]
      __m256 mixedInput;
      if constexpr (UseAVX2)
      {
        mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 4));
      }
      else
      {
        // AVX1 is a little more complex (but only slightly more expensive)
        __m256 tmp = _mm256_permute_ps(firstInput, 0b00111001);     // [a b c d e f g h] -> [b c d a f g h e]
        mixedInput = _mm256_permute_ps(innerInput, 0b00111001);     // [e f g h i j k l] -> [f g h e j k l i]
        mixedInput = _mm256_blend_ps(tmp, mixedInput, 0b10001000);  // -> [b c d e f g h i]
      }

      [[maybe_unused]] __m256 doubleFuck =  _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 4));
      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[2]);
      // Mix the next shift up (i2, i3, i4, i5, i6, i7, i8, i9)
      if constexpr (UseAVX2)
      {
        mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 8));
      }
      else
      {
        __m256 tmp = _mm256_permute_ps(firstInput, 0b01001110);     // [a b c d e f g h] -> [c d a b g h e f]
        mixedInput = _mm256_permute_ps(innerInput, 0b01001110);     // [e f g h i j k l] -> [g h e f k l i j]
        mixedInput = _mm256_blend_ps(tmp, mixedInput, 0b11001100);  // -> [c d e f g h i j]
      }

      doubleFuck =  _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 8));

      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[3]);
      if constexpr (UseAVX2)
      {
        mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 12));
      }
      else
      {
        __m256 tmp = _mm256_permute_ps(firstInput, 0b10010011);     // [a b c d e f g h] -> [d a b c h e f g]
        mixedInput = _mm256_permute_ps(innerInput, 0b10010011);     // [e f g h i j k l] -> [h e f g l i j k]
        mixedInput = _mm256_blend_ps(tmp, mixedInput, 0b11101110);  // -> [d e f g h i j k]
      }

      doubleFuck =  _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 12));

      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[4]);
      accumulator = _mm256_fmadd_ps(coeff, innerInput, accumulator);

      // now we have to switch to shifting between the inner input and the second one to keep the shifts going
      coeff = _mm256_broadcast_ss(&fc[5]);
      if constexpr (UseAVX2)
      {
        mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 4));
      }
      else
      {
        __m256 tmp = _mm256_permute_ps(innerInput,  0b00111001);    // [e f g h i j k l] -> [f g h e j k l i]
        mixedInput = _mm256_permute_ps(secondInput, 0b00111001);    // [i j k l m n o p] -> [j k l i n o p m]
        mixedInput = _mm256_blend_ps(tmp, mixedInput, 0b10001000);  // -> [f g h i j k l m]
      }

      doubleFuck =  _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 4));

      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[6]);
      if constexpr (UseAVX2)
      {
        mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 8));
      }
      else
      {
        __m256 tmp = _mm256_permute_ps(innerInput,  0b01001110);    // [e f g h i j k l] -> [g h e f k l i j]
        mixedInput = _mm256_permute_ps(secondInput, 0b01001110);    // [i j k l m n o p] -> [k l i j o p m n]
        mixedInput = _mm256_blend_ps(tmp, mixedInput, 0b11001100);  // -> [g h i j k l m n]
      }

      doubleFuck =  _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 8));

      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[7]);
      if constexpr (UseAVX2)
      {
        mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 12));
      }
      else
      {
        __m256 tmp = _mm256_permute_ps(innerInput,  0b10010011);    // [e f g h i j k l] -> [h e f g l i j k]
        mixedInput = _mm256_permute_ps(secondInput, 0b10010011);    // [i j k l m n o p] -> [l i j k p m n o]
        mixedInput = _mm256_blend_ps(tmp, mixedInput, 0b11101110);  // -> [e f g h i j k l]
      }

      doubleFuck =  _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 12));

      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);      

      return accumulator;
    };

    {
      // First input set needs some special handling
      u32 filterStartSet = firstFullSet-1;
      auto fcStart = &coeffs[filterStartSet * k_setSize];
      for (u32 inputSet = 0; inputSet < firstFullSet; inputSet++)
      {
        auto fc = fcStart;
        auto input = inputStart;
        __m256 firstInput = _mm256_setzero_ps(); // These first sets start with an initial set of zeros
        __m256 accumulator = _mm256_setzero_ps();

        for (u32 filterSet = filterStartSet; filterSet < filterSetCount; filterSet++)
        {
          __m256 secondInput = _mm256_load_ps(input);
          accumulator = Accumulate(accumulator, firstInput, secondInput, fc);
          fc += k_setSize;
          input += k_setSize;
          firstInput = secondInput;
        }

        _mm256_store_ps(output, accumulator);

        // Do one more set on the next go around
        filterStartSet--;
        fcStart -= k_setSize;
        output += k_setSize;
      }
    }

    u32 inputEndSet = u32(valuesIn.size()/k_setSize) - 1; // !!! If I pad the ending up to a set size then add an extra set size, the -1 goes away
    u32 remainingInputSetCount = inputEndSet; // Because we have to read the input "before" the first sample we count them all as valid
    for (u32 inputSet = firstFullSet; inputSet < inputEndSet; inputSet++)
    {
      __m256 accumulator = _mm256_setzero_ps();

      // By now we should have a leftX that is >= 0 and a filterStartSample that is some offset into the filter
      // We want to loop until we hit the end of the filter or we hit one set short of the end (we read two sets in the logic here so
      //  we need one additional set's worth of work at the end to tidy up)
      u32 totalProcessSetCount = std::min(filterSetCount, remainingInputSetCount);

      auto input = inputStart;
      auto fc = &coeffs[0];

      __m256 firstInput = _mm256_load_ps(input);
      input += k_setSize;

      for (u32 set = 0; set < totalProcessSetCount; set++)
      {
        __m256 secondInput = _mm256_load_ps(input);
        accumulator = Accumulate(accumulator, firstInput, secondInput, fc);
        fc += k_setSize;
        input += k_setSize;
        firstInput = secondInput;
      }

      _mm256_store_ps(output, accumulator);

      remainingInputSetCount--;
      inputStart += k_setSize;
      output += k_setSize;
    }

    // !!! $TODO If I just pad the damn input with k_setSize zeros then this whole last bit of logic goes away

    __m256 accumulator = _mm256_setzero_ps();
    auto input = inputStart;
    auto fc = &coeffs[0]; 

    __m256 firstInput = _mm256_load_ps(input);
    input += k_setSize;

    for (u32 set = 0; set < remainingInputSetCount; set++)
    {
      __m256 secondInput = _mm256_load_ps(input);
      accumulator = Accumulate(accumulator, firstInput, secondInput, fc);
      fc += k_setSize;
      input += k_setSize;
      firstInput = secondInput;
    }


    __m256 secondInput = _mm256_setzero_ps();
    accumulator = Accumulate(accumulator, firstInput, secondInput, fc);
    _mm256_store_ps(output, accumulator);
  }

  std::vector<f32, AlignAllocator<f32>> coefficients;
  u32 actualFilterLength;
};


