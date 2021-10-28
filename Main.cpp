#include "Common.h"

#include "WindowsInc.h"

#include <chrono>
#include <cmath>
#include <complex>
#include <vector>
#include <stdint.h>
#include <inttypes.h>

#include <immintrin.h>

#pragma warning (push)
#pragma warning (disable: 4668 4917 4365 4987 4623 4626 5027)
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#pragma warning (pop)

#include <Shlwapi.h>
//#include "resource.h"

#include "BaseTypes.h"
#include "Mem.h"

#include "SP.h"
#include "List.h"
#include "ReadWicTexture.h"

#include <algorithm>

#include <windowsx.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")

#include "ButterworthIIR.h"

#define MAX_LOADSTRING 100

HWND window;

constexpr size_t k_maxFloatAlignment = 8;
constexpr size_t k_maxAlignment = k_maxFloatAlignment * sizeof(float);

template <typename IntT>
constexpr IntT AlignInt(IntT value, size_t requiredAlignment)
  { return IntT(value + requiredAlignment - 1) & ~(requiredAlignment - 1); }

template <typename t_type>
static constexpr t_type k_pi = t_type(3.1415926535897932384626433832795028841971);

static constexpr f32 k_artifactHue = 0.0f;
static constexpr f32 k_decodeHue = 0.0f;

SP<ID3D11Device> device;
SP<IDXGISwapChain> swapChain;
SP<ID3D11DeviceContext> context;
SP<ID3D11Texture2D> backbuffer;
SP<ID3D11RenderTargetView> backbufferView;
SP<ID3D11RasterizerState> rasterState;
SP<ID3D11DepthStencilState> depthState;
SP<ID3D11BlendState> blendState;
SP<ID3D11InputLayout> inputLayout;
SP<ID3D11VertexShader> vs;
SP<ID3D11PixelShader> ps;
SP<ID3D11Buffer> vb;
SP<ID3D11Buffer> ib;
SP<ID3D11SamplerState> sampler;

int screenWidth;
int screenHeight;
bool fullscreen = true;
f32 aspectAdjust = 1.0f;

bool autoZoom = true;
float zoom = 1.0f;
float panX = 0.5f;
float panY = 0.5f;


const char *pShader = R"(
struct VertexInput
{
  float2 position : POSITION;
  float2 tex0 : TEXCOORD0;
};

struct PixelInput
{
  float4 position : SV_POSITION;
  float2 tex0 : TEXCOORD0;
};

sampler SamDefault : register(s0);
Texture2D<float4> Tex : register(t0);

void VS(VertexInput input, out PixelInput output)
{
  output.position = float4(input.position, 0, 1);
  output.tex0 = input.tex0;
}

float4 PS(PixelInput input) : SV_Target
{
  return Tex.Sample(SamDefault, input.tex0);
}
)";

struct Vertex
{
  f32 positionX;
  f32 positionY;
  f32 texX;
  f32 texY;
};


Vertex vertices[4];
SP<ID3D11Texture2D> displayTexture;
SP<ID3D11ShaderResourceView> displaySRV;


struct GeneratorInfo
{
  s32 pixelsPerLine;
  f32 colorCyclesPerPixel;
  s32 initialPhaseTableIndex;
  s32 phaseTableIndexIncrementPerLine;
  s32 phaseTableIndexIncrementPerFrame;
  u32 phaseTableCount;
};

GeneratorInfo NESandSNESGeneratorInfo = { 256, 2.0f/3.0f, 0, 1, 2, 3 };
GeneratorInfo CGA320GeneratorInfo = { 320, 1.0f/2.0f, 1, 0, 0, 2 };
GeneratorInfo CGA640GeneratorInfo = { 640, 1.0f/4.0f, 1, 0, 0, 2 };

GeneratorInfo genToUse = NESandSNESGeneratorInfo;

template <typename Type>
class AlignAllocator
{
public:
  using value_type = Type;
  
  constexpr AlignAllocator() noexcept = default;
  template <typename T>
  constexpr AlignAllocator(const AlignAllocator<T> &) noexcept {};

  Type *allocate(size_t n)
    { return static_cast<Type *>(_mm_malloc(n * sizeof(Type), std::max(alignof(Type), k_maxAlignment))); }

  void deallocate(const Type *p, [[maybe_unused]] const size_t n) noexcept
    { _mm_free(const_cast<Type *>(p)); }

  inline bool operator == (const AlignAllocator &) 
    { return true; }

  inline bool operator != (const AlignAllocator &) 
    { return false; }
};

template <typename t_type>
using AlignedVector = std::vector<t_type, AlignAllocator<f32>>;

void SetZoom(float zoomIn)
{
  if (displayTexture == nullptr)
  {
    return;
  }
  if(zoomIn == 0)
  {
    panX = panY = 0.5f;
    autoZoom = true;
        
    D3D11_TEXTURE2D_DESC desc;
    displayTexture->GetDesc(&desc);

    // 0 zoom == auto-fit
    f32 panelRatio = f32(screenWidth) / f32(screenHeight);
    f32 imageRatio = f32(desc.Width) / f32(desc.Height) / aspectAdjust;
    s32 drawW, drawH;
    if(panelRatio >= imageRatio)
    {
      drawH = screenHeight;
      drawW = s32(f32(screenHeight)*imageRatio);
    }
    else
    {
      drawW = screenWidth;
      drawH = s32(f32(screenWidth) / imageRatio);
    }        
        
    zoom = f32(drawH) / f32(desc.Height);
  }
  else
  {
    autoZoom = false;
    zoom = zoomIn;
  }
}

struct LineGeneratorInfo
{
  u32 widthInPixels;
  f32 colorBurstCycleCountPerPixel; // for NES this is 2/3
  u32 outputSamplesPerPixel;
};



f32 SinPi(f32 x)
{
  // Calculate sin(pi*x)

  // To keep the accuracy as high as we can, we'll restrict the range to -0.5 to 0.5. 
  f32 rounded = std::round(x);
  x -= rounded;
  
  // However, every other -0.5 to 0.5 block of sin flips its sign so determine where we are so we know whether we need to flip or not.
  bool flip = (s32(rounded) & 1) != 0;

  // Great! Now do the taylor series for Sin.
  // The standard taylor series is x - x^3 / 3! + x^5 / 5! + x^7 / 7! + ..., but what we ACTUALLY have is (x/pi) so we need to factor the pi into each coefficient:
  // (x * pi) - (x * pi)^3 / 3! + (x * pi)^5 / 5! + (x * pi)^7 / 7! + ...
  // =  x * pi - x^3 * (pi^3 / 3!) + x^5 * (pi^5 / 5!) + x^7 * (pi^7 / 7!) + ...
  // So that's what these coefficients are.
  
  // I chose 11 coefficients because once I hit 13 it stopped making a difference to the result.
  static constexpr f32 k_coef1 = k_pi<f32>;          //  (pi^1) / 1!
  static constexpr f32 k_coef3 = -5.167712780f;      // -(pi^3) / 3!
  static constexpr f32 k_coef5 = 2.550164040f;       //  (pi^5) / 5!
  static constexpr f32 k_coef7 = -5.992645293e-01f;  // -(pi^7) / 7!
  static constexpr f32 k_coef9 = 8.214588661e-02f;   //  (pi^9) / 9!
  static constexpr f32 k_coef11 = -7.370430946e-03f; // -(pi^11) / 11!

  // Now, for floating point accuracy, do this from the bottom up, multiplying by squared x terms as we go
  f32 xSqr = x * x;
  f32 result = k_coef9 + xSqr * k_coef11;
  result     = k_coef7 + xSqr * result;
  result     = k_coef5 + xSqr * result;
  result     = k_coef3 + xSqr * result;
  result     = k_coef1 + xSqr * result;
  
  // The whole thing is multiplied by one additional x
  result = x * result;

  if (flip)
  {
    result = -result;
  }

  return result;
}


f32 CosPi(f32 x)
{
  // Calculate CosPi in terms of SinPi instead of making a whole other taylor series. This will lose accuracy at VERY LARGE NUMBERS (once 0.5 is off the end of the mantissa)
  //  but for this project I don't care about that at all.
  return SinPi(-std::abs(x) + 0.5f);
}


// Sinc of a value that is multiples of pi (where sinc(x) = sin(x) / x, and sinc(0) == 1.0)
f32 SincPi(f32 x)
{
  if (std::abs(x) < 0.01f)
  {
    // Near 0 we need to approximate sin(x)/x using a taylor series to avoid precision issues.
    // Unlike in SinPi we don't have to deal with angle wrapping or sign flips because we know we're near Actual Zero

    // The beauty of this is that every term the sin taylor series has an x term in it, so we can just multiply in one less x (or, in our case, one less x*pi)
    // So the coefficients for this taylor series are the same terms for SinPi(x) except with one less pi factored in.
    // Our actual taylor series looks like 1 - x^2 * (pi^2 / 3!) + x^4 * (pi^4 / 5!) + x^6 * (pi^6 / 7!) + ...

    // I'm also using one less term here than above because we're so close to 0 that our accuracy is already fine by 9 terms.
    static constexpr f32 k_coef3 = -1.644934067e+00f; // -pi^2 / 3!
    static constexpr f32 k_coef5 = 8.117424253e-01f;  //  pi^4 / 5!
    static constexpr f32 k_coef7 = -1.907518241e-01f; // -pi^6 / 7!
    static constexpr f32 k_coef9 = 2.614784782e-02f;  //  pi^8 / 9!

    // Same as above, do this from the bottom up for float accuracy
    f32 xSqr = x * x;
    f32 result = k_coef7 + xSqr * k_coef9;
    result     = k_coef5 + xSqr * result;
    result     = k_coef3 + xSqr * result;
    result     = 1.0f    + xSqr * result; // coefficient 1 is just 1.0

    // No additional multiply by x needed here since it's Sin(x)/x
    return result;
  }
  else
  {
    // Outside of this range it's better to do the calculation directly because Sin is periodic but 1/x is very not.
    return SinPi(x) / (x * k_pi<f32>);
  }
}


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


  void ProcessAvx2_2(const AlignedVector<f32> &valuesIn, AlignedVector<f32> *pValuesOut) const
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
      __m256 innerInput = _mm256_castsi256_ps(_mm256_permute2x128_si256(_mm256_castps_si256(firstInput), _mm256_castps_si256(secondInput), 0x21));

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
      __m256 mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 4));
      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[2]);
      // Mix the next shift up (i2, i3, i4, i5, i6, i7, i8, i9)
      mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 8));
      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[3]);
      mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 12));
      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[4]);
      mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(innerInput), _mm256_castps_si256(firstInput), 16));
      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      // now we have to switch to shifting between the inner input and the second one to keep the shifts going
      coeff = _mm256_broadcast_ss(&fc[5]);
      mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 4));
      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[6]);
      mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 8));
      accumulator = _mm256_fmadd_ps(coeff, mixedInput, accumulator);

      coeff = _mm256_broadcast_ss(&fc[7]);
      mixedInput = _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(secondInput), _mm256_castps_si256(innerInput), 12));
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
        __m256 firstInput = _mm256_set1_ps(0.0f); // These first sets start with an initial set of zeros
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

    u32 inputEndSet = (valuesIn.size()/k_setSize) - 1; // !!! If I pad the ending up to a set size then add an extra set size, the -1 goes away
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


  void Process(const AlignedVector<f32> &valuesIn, AlignedVector<f32> *pValuesOut) const
  {
    for (u32 i = 0; i < valuesIn.size(); i++)
    {
      (*pValuesOut)[i] = ProcessOneElement(valuesIn.data(), valuesIn.size(), i);
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
  std::vector<f32, AlignAllocator<f32>> coefficients;
  u32 actualFilterLength;
};


f32 WangHashAndXorShift(u32 seed)
{
  // wang hash
  seed = (seed ^ 61) ^ (seed >> 16);
  seed *= 9;
  seed = seed ^ (seed >> 4);
  seed *= 0x27d4eb2d;
  seed = seed ^ (seed >> 15);

  // xorshift
  seed ^= (seed << 13);
  seed ^= (seed >> 17);
  seed ^= (seed << 5);
  return f32(f64(seed) * (1.0 / 4294967296.0));
}


#if 0
struct CGAColorMapping
{
  u8 oldR;
  u8 oldG;
  u8 oldB;
  
  u8 newR;
  u8 newG;
  u8 newB;
};

const CGAColorMapping OldCGAemap[] =
{
  {   0,   0,   0,     0,   0,   0, },
  {   0,   0, 170,    75,  65, 251, },
  {   0, 170,   0,    43, 139,   0, },
  {   0, 170, 170,     0, 140,  65, },
  { 170,   0,   0,   175,  35, 114, },
  { 170,   0, 170,   130,  36, 226, },
  { 170,  85,   0,   101, 110,   0, },
  { 170, 170, 170,   183, 187, 181, },
  {  85,  85,  85,    71,  68,  72, },
  {  85,  85, 255,   145, 135, 255, },
  {  85, 255,  85,   114, 209,  22, },
  {  85, 255, 255,    69, 210, 136, },
  { 255,  85,  85,   245, 105, 184, },
  { 255,  85, 255,   200, 106, 255, },
  { 255, 255,  85,   171, 180,   0, },
  { 255, 255, 255,   253, 255, 251, }
};
#endif

void ProcessRGBToNTSCLine(
  [[maybe_unused]] const u32 *linePixelsIn, 
  const LineGeneratorInfo &info, 
  const AlignedVector<f32> &sinTable,
  const AlignedVector<f32> &cosTable,
  AlignedVector<f32> *lumaSignalOut, 
  AlignedVector<f32> *chromaSignalOut, 
  [[maybe_unused]] u32 yCoord)
{
  ASSERT(lumaSignalOut->size() == info.widthInPixels * info.outputSamplesPerPixel);
  ASSERT(lumaSignalOut->size() == chromaSignalOut->size());
  s32 phaseIndex = 0;

  f32 *lumaSignalDst = lumaSignalOut->data();
  f32 *chromaSignalDst = chromaSignalOut->data();
  for (u32 x = 0; x < info.widthInPixels; x++) // PER PIXEL
  {
    u32 abgr = linePixelsIn[x];

    u8 r8 = u8(abgr & 0x000000FF);
    u8 g8 = u8((abgr & 0x0000FF00) >> 8);
    u8 b8 = u8((abgr & 0x00FF0000) >> 16);

#if 0
    {
      s32 nearestI = -1;
      f32 difference = std::numeric_limits<f32>::max();
      for (u32 i = 0; i < 16; i++)
      {
        f32 dr = f32(r8) - f32(OldCGAemap[i].oldR);
        f32 dg = f32(g8) - f32(OldCGAemap[i].oldG);
        f32 db = f32(b8) - f32(OldCGAemap[i].oldB);

        f32 d = dr * dr + dg * dg + db * db;
        if (d < difference)
        {
          difference = d;
          nearestI = i;
        }
      }

      r8 = OldCGAemap[nearestI].newR;
      g8 = OldCGAemap[nearestI].newG;
      b8 = OldCGAemap[nearestI].newB;
    }
#endif

    f32 r = f32(r8) / 255.0f;
    f32 g = f32(g8) / 255.0f;
    f32 b = f32(b8)/ 255.0f;
    
    // Convert RGB to YIQ
    [[maybe_unused]] f32 y = 0.3000f * r + 0.5900f * g + 0.1100f * b;
    [[maybe_unused]] f32 i = 0.5990f * r - 0.2773f * g - 0.3217f * b;
    [[maybe_unused]] f32 q = 0.2130f * r - 0.5251f * g + 0.3121f * b;

    //q *= 0.72f;
    //y *= 0.28f;
    // Generate the signal for this pixel into the output

    //u32 waveLength = u32(std::round(f32(info.outputSamplesPerPixel) / info.colorBurstCycleCountPerPixel));
    //u32 halfWaveLength = waveLength / 2;
    //ASSERT((waveLength & 3) == 0);
    for (u32 s = 0; s < info.outputSamplesPerPixel; s++)
    {
      *lumaSignalDst = y; // + (WangHashAndXorShift(yCoord * info.widthInPixels + x) - 0.5f) * 0.05f;
      *chromaSignalDst = -sinTable[phaseIndex] * i + cosTable[phaseIndex] * q; // SinPi(2.0f * phase + k_artifactHue) * i + CosPi(2.0f * phase + k_artifactHue) * q;
      lumaSignalDst++;
      chromaSignalDst++;
      phaseIndex++;
    }
  }
}


void GenerateNTSCFilters(
  f32 colorBurstCycleCountPerOutputSample,
  [[maybe_unused]] f32 colorBurstCycleFrequency,
  [[maybe_unused]] f32 upperMaxFrequency,
  FIRFilter *lumaFilterOut,
  FIRFilter *chromaFilterOut,
  ButterworthIIR::Filter *chromaFilterIIROut,
  FIRFilter *chromaDemodulateLowPassOut,
  ButterworthIIR::Filter *chromaDemodulateIIROut)
{
  *lumaFilterOut = FIRFilter::CreateLowPass(colorBurstCycleCountPerOutputSample);
  *chromaFilterOut = FIRFilter::Convolve(
    FIRFilter::CreateHighPass(
      colorBurstCycleCountPerOutputSample,
      2.5f * colorBurstCycleCountPerOutputSample / colorBurstCycleFrequency),
    FIRFilter::CreateLowPass(colorBurstCycleCountPerOutputSample, 6.0f * colorBurstCycleCountPerOutputSample / colorBurstCycleFrequency));

  *chromaFilterIIROut = ButterworthIIR::Filter::CreateBandPass(
    3,
    3.0f * colorBurstCycleCountPerOutputSample / colorBurstCycleCountPerOutputSample * 2.0f,
    4.0f * colorBurstCycleCountPerOutputSample / colorBurstCycleCountPerOutputSample * 2.0f);
  
  *chromaDemodulateLowPassOut = FIRFilter::CreateLowPass(1.5f * colorBurstCycleCountPerOutputSample, 2.0f * colorBurstCycleCountPerOutputSample);

  auto lowFreq = (colorBurstCycleCountPerOutputSample * 2.0f) * 2.5f / 3.58f;
  auto highFreq = (colorBurstCycleCountPerOutputSample * 2.0f) * 4.0f / 3.58f;

  *chromaFilterIIROut = ButterworthIIR::Filter::CreateBandPass(2, lowFreq, highFreq);

  *chromaDemodulateIIROut = ButterworthIIR::Filter::CreateLowPass(3, 1.2f * colorBurstCycleCountPerOutputSample);
  //*chromaDemodulateIIROut = ButterworthIIR::Filter::CreateLowPass(2, colorBurstCycleCountPerOutputSample * 0.3f);

  chromaFilterIIROut->MeasureLatency();
  chromaDemodulateIIROut->MeasureLatency();
}
  
void SeparateLumaAndChroma(const FIRFilter &lumaFilter, const FIRFilter &chromaFilter, ButterworthIIR::Filter &, const AlignedVector<f32> &compositeSignalIn, AlignedVector<f32> *lumaOut, AlignedVector<f32> *chromaOut)
{
  // Low pass away the chroma data
  lumaFilter.ProcessAvx2_2(compositeSignalIn, lumaOut);
  //chromaFilter.ProcessAvx2_2(compositeSignalIn, chromaOut);
  chromaFilter.Process(compositeSignalIn, chromaOut);
}


void ProcessForNTSC(const std::vector<u32> &pixelsIn, u32 widthIn, u32 heightIn, std::vector<u32> *pixelsOut, u32 *pWidthOut, u32 *pHeightOut, f32 *pAspectAdjustOut)
{
  const LineGeneratorInfo lineInfo = 
  {
    widthIn,
    genToUse.colorCyclesPerPixel,
    //genToUse.initialPhaseOffsetX,
    3,    // outputSamplesPerPixel
  };

  FIRFilter lumaFilter;
  FIRFilter chromaFilter;
  FIRFilter chromaDemodulateLowPass;
  ButterworthIIR::Filter chromaIIR;
  ButterworthIIR::Filter chromaDemodulateIIR;
  GenerateNTSCFilters(
    lineInfo.colorBurstCycleCountPerPixel / f32(lineInfo.outputSamplesPerPixel),
    315.0f / 88.0f,
    4.5f,
    &lumaFilter,
    &chromaFilter,
    &chromaIIR,
    &chromaDemodulateLowPass,
    &chromaDemodulateIIR);

  pixelsOut->resize(widthIn * lineInfo.outputSamplesPerPixel * heightIn);

  u32 lineArraySize = widthIn * lineInfo.outputSamplesPerPixel;

  std::vector<AlignedVector<f32>> sinTables;
  std::vector<AlignedVector<f32>> cosTables;

  for (u32 i = 0; i < genToUse.phaseTableCount; i++)
  {
    AlignedVector<f32> sinTable;
    AlignedVector<f32> cosTable;
    f32 phase = f32(i) / f32(genToUse.phaseTableCount);
    f32 phaseIncrement = lineInfo.colorBurstCycleCountPerPixel / f32(lineInfo.outputSamplesPerPixel);

    sinTable.resize(lineArraySize);
    cosTable.resize(lineArraySize);

    for (u32 x = 0; x < lineArraySize; x++)
    {
      sinTable[x] = SinPi(2.0f * phase + k_artifactHue);
      cosTable[x] = CosPi(2.0f * phase + k_artifactHue);

      phase += phaseIncrement;
    }

    sinTables.push_back(std::move(sinTable));
    cosTables.push_back(std::move(cosTable));
  }

  AlignedVector<f32> lineArray;
  AlignedVector<f32> lineChromaArray;
  lineArray.resize(lineArraySize);
  lineChromaArray.resize(lineArraySize);

  *pWidthOut = lineArraySize;
  *pHeightOut = heightIn;
  *pAspectAdjustOut = f32(lineInfo.outputSamplesPerPixel) * 7.0f / 8.0f; // !!!

  AlignedVector<f32> yData;
  yData.resize(lineArraySize);

  AlignedVector<f32> chromaPassData;
  chromaPassData.resize(lineArraySize);

  AlignedVector<f32> qPassData;
  qPassData.resize(lineArraySize);

  AlignedVector<f32> iPassData;
  iPassData.resize(lineArraySize);

  std::chrono::high_resolution_clock c;
  auto start = c.now();

  [[maybe_unused]] auto sinDecodeHue = SinPi(k_decodeHue);
  [[maybe_unused]] auto cosDecodeHue = CosPi(k_decodeHue);

  // for (u32 iteration = 0; iteration < 1000; iteration++)
  {
    s32 phaseTableIndex = genToUse.initialPhaseTableIndex; // !!! Per-frame initial index goes here
    for (u32 y = 0; y < heightIn; y++)
    {
      auto &sinTable = sinTables[phaseTableIndex];
      auto &cosTable = cosTables[phaseTableIndex];
      // Generate the luma and chroma data from the RGB line
      ProcessRGBToNTSCLine(pixelsIn.data() + widthIn*y, lineInfo, sinTable, cosTable, &lineArray, &lineChromaArray, y);
      for (u32 x = 0; x < lineArraySize; x++)
      {
        // Combine the two into a single signal, like composite video
        lineArray[x] += lineChromaArray[x];
      }


      // Now that we did that, pull them immediately back apart because it's funny
      SeparateLumaAndChroma(lumaFilter, chromaFilter, chromaIIR, lineArray, &yData, &chromaPassData);

      chromaDemodulateIIR.ResetHistory();
      {
        for (u32 x = 0; x < lineArraySize; x++)
        {
          // Cos([2*phase + artifactHue] + decodeHue) -> Cos(A + decodeHue) -> Cos(A)*Cos(decodeHue) - Sin(A)*Sin(DecodeHue);
          f32 co = cosTable[x] * cosDecodeHue - sinTable[x] * sinDecodeHue;
           lineArray[x] = chromaPassData[x] * co;
        }

        //chromaDemodulateLowPass.ProcessAvx2(lineArray, &qPassData);
        chromaDemodulateIIR.Process(lineArray, &qPassData);
      }

      chromaDemodulateIIR.ResetHistory();
      {
        for (u32 x = 0; x < lineArraySize; x++)
        {
          // Sin([2*phase + artifactHue] + decodeHue) -> Sin(A + decodeHue) -> Sin(A)*Cos(decodeHue) + Cos(A)*Sin(DecodeHue);
          f32 si = -(sinTable[x] * cosDecodeHue + cosTable[x] * sinDecodeHue);
           lineArray[x] = chromaPassData[x] * si; // -SinPi(2.0f * phase + k_artifactHue + k_decodeHue);
        }

        //chromaDemodulateLowPass.ProcessAvx2(lineArray, &iPassData);
        chromaDemodulateIIR.Process(lineArray, &iPassData);
      }

      for (u32 x = 0; x < lineArraySize; x++)
      {
        [[maybe_unused]] f32 Y = yData[x];
        [[maybe_unused]] f32 i = iPassData[x] * 2.0f;
        [[maybe_unused]] f32 q = qPassData[x] * 2.0f;

        //i = 0;
        //q = 0;

        f32 r = Y + i * 0.946882f + q * 0.623557f; //std::abs(qPassData[x]);
        f32 g = Y - i * 0.274788f - q * 0.635691f; //std::abs(qPassData[x]);
        f32 b = Y - i * 1.108545f + q * 1.7090047f; //std::abs(qPassData[x]);

        r = std::floor(std::clamp(r * 255.0f, 0.0f, 255.0f));
        g = std::floor(std::clamp(g * 255.0f, 0.0f, 255.0f));
        b = std::floor(std::clamp(b * 255.0f, 0.0f, 255.0f));

        u32 abgr = 0xFF000000 | (u32(r)) | (u32(g) << 8) | (u32(b) << 16);
        (*pixelsOut)[y * lineArraySize + x] = abgr;
      }

      phaseTableIndex = (phaseTableIndex + genToUse.phaseTableIndexIncrementPerLine) % genToUse.phaseTableCount;
    }
  }
  auto end = c.now();

  auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  char buffer[512];
  sprintf_s(buffer, "%" PRId64 "\n", time);
  OutputDebugStringA(buffer);
}


void LoadTexture(wchar_t *path)
{
  if (path == nullptr || path[0] == L'\0')
  {
    displayTexture = nullptr;
    displaySRV = nullptr;
    return;
  }
  std::vector<u32> dataFromTex;
  u32 width;
  u32 height;
  ReadWicTexture(path, &dataFromTex, &width, &height);

  std::vector<u32> dataFromNTSC;
  ProcessForNTSC(dataFromTex, width, height, &dataFromNTSC, &width, &height, &aspectAdjust);
  // aspectAdjust = 1; // !!!

  D3D11_TEXTURE2D_DESC desc;
  ZeroType(&desc);
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  D3D11_SUBRESOURCE_DATA resDat;
  ZeroType(&resDat);
  resDat.pSysMem = dataFromNTSC.data();
  resDat.SysMemPitch = width * sizeof(u32);
  device->CreateTexture2D(&desc, &resDat, displayTexture.Address());
  device->CreateShaderResourceView(displayTexture, nullptr, displaySRV.Address());

  SetZoom(autoZoom ? 0.0f : 1.0f);
}


bool dragging = false;
s32 dragX = 0;
s32 dragY = 0;


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_LBUTTONDOWN:
    if (!dragging && displayTexture != nullptr)
    {
      SetCapture(hWnd);
      dragX = GET_X_LPARAM(lParam);
      dragY = GET_Y_LPARAM(lParam);
      SetCursor(nullptr);

      dragging = true;
    }
    break;

  case WM_MOUSEMOVE:
    if (dragging)
    {
      s32 x = GET_X_LPARAM(lParam);
      s32 y = GET_Y_LPARAM(lParam);

      if (x == dragX && y == dragY)
      {
        break;
      }

      D3D11_TEXTURE2D_DESC desc;
      displayTexture->GetDesc(&desc);
      panX -= 3*f32(x - dragX) / zoom / f32(desc.Width) / f32(aspectAdjust);
      panY -= 3*f32(y - dragY) / zoom / f32(desc.Height);
      panX = std::clamp(panX, 0.0f, 1.0f);
      panY = std::clamp(panY, 0.0f, 1.0f);
      InvalidateRect(window, nullptr, true);
      UpdateWindow(window);

      POINT pt;
      pt.x = dragX;
      pt.y = dragY;
      ClientToScreen(hWnd, &pt);
      SetCursorPos(pt.x, pt.y);
    }
    break;

  case WM_LBUTTONUP:
    if (dragging)
    {
      dragging = false;
      SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    break;

  case WM_KEYDOWN:
    switch (wParam)
    {
    case 'O':
      {
        wchar_t filename[1024] = {0};
        OPENFILENAME ofn;
        ZeroType(&ofn);
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hInstance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));
        ofn.hwndOwner = hWnd;
        ofn.lpstrFilter = L"Images (*.jpg;*.jpeg;*.png)\0*.jpg;*.jpeg;*.png\0\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = ArrayLength(filename);
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileName(&ofn))
        {
          LoadTexture(filename);
          InvalidateRect(window, nullptr, true);
          UpdateWindow(window);
        }
      }
      break;

    case VK_ADD:
      if (zoom >= 1)
      {
        zoom = f32(s32(zoom + 1));
      }
      else
      {
        zoom = 1;
      }
      SetZoom(zoom);
      InvalidateRect(window, nullptr, true);
      UpdateWindow(hWnd);
      break;

    case VK_SUBTRACT:
      if (zoom > 1)
      {
        zoom = f32(s32(zoom - 1));
      }
      SetZoom(zoom);
      InvalidateRect(window, nullptr, true);
      UpdateWindow(hWnd);
      break;

    case VK_DIVIDE:
      SetZoom(0);
      InvalidateRect(window, nullptr, true);
      UpdateWindow(hWnd);
      break;

    case VK_MULTIPLY:
      SetZoom(1);
      InvalidateRect(window, nullptr, true);
      UpdateWindow(hWnd);
      break;

    case VK_ESCAPE:
      DestroyWindow(hWnd);
      break;

    case VK_F11:
      fullscreen = !fullscreen;
      if (fullscreen)
      {
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
      }
      else
      {
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
      }
      PostMessage(hWnd, WM_SIZE, 0, 0);
      break;
    }
    break;

  case WM_SIZE:
    {
      RECT r;
      GetClientRect(hWnd, &r);
      int width = r.right - r.left;
      int height = r.bottom - r.top;
      if (width == 0 || height == 0)
      {
        break;
      }

      backbuffer = nullptr;
      backbufferView = nullptr;

      screenWidth = width;
      screenHeight = height;

      if (swapChain != nullptr)
      {
        swapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        swapChain->GetBuffer(0,  __uuidof((ID3D11Texture2D*)(backbuffer)), reinterpret_cast<void**>(backbuffer.Address()));
        device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.Address());
      }
    }
    break;

  case WM_PAINT:
    {
      PAINTSTRUCT paintst;
      if (BeginPaint(hWnd, &paintst))
      {
        EndPaint(hWnd, &paintst);
      }

      float black[] = {0.0f, 0.0f, 0.0f, 0.0f};
      context->ClearRenderTargetView(backbufferView, black);


      if (displayTexture != nullptr)
      {
        D3D11_TEXTURE2D_DESC desc;
        displayTexture->GetDesc(&desc);

        f32 drawW = f32(desc.Width) * zoom / aspectAdjust;
        f32 drawH = f32(desc.Height) * zoom;
        f32 panXRemainder = std::max(0.0f, drawW - f32(screenWidth));
        f32 panYRemainder = std::max(0.0f, drawH - f32(screenHeight));

        f32 posX = -panX * panXRemainder;
        f32 posY = -panY * panYRemainder;

        if (panXRemainder == 0)
        {
          posX = (f32(screenWidth) - drawW) * 0.5f;
        }
        if (panYRemainder == 0)
        {
          posY = (f32(screenHeight) - drawH) * 0.5f;
        }

        posX /= f32(screenWidth);
        posY /= f32(screenHeight);
        drawW /= f32(screenWidth);
        drawH /= f32(screenHeight);

        posX = posX*2-1;
        posY = -posY*2+1;
        drawW = drawW*2;
        drawH = -drawH*2;

        D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD;
        D3D11_MAPPED_SUBRESOURCE subres;
        context->Map(vb, 0, mapType, 0, &subres);
        auto vbData = reinterpret_cast<Vertex*>(subres.pData);
        vbData[0] = {posX,         posY,         0, 0};
        vbData[1] = {posX + drawW, posY,         1, 0};
        vbData[2] = {posX + drawW, posY + drawH, 1, 1};
        vbData[3] = {posX,         posY + drawH, 0, 1};
        context->Unmap(vb, 0);

        D3D11_VIEWPORT vp;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        vp.Width = float(screenWidth);
        vp.Height = float(screenHeight);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->OMSetRenderTargets(1, backbufferView.ConstAddress(), nullptr);
        context->RSSetViewports(1, &vp);

        u32 stride = sizeof(Vertex);
        u32 offsets = 0;
        context->IASetVertexBuffers(0, 1, vb.ConstAddress(), &stride, &offsets);
        context->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->OMSetBlendState(blendState, black, 0xffffffff);
        context->OMSetDepthStencilState(depthState, 0);
        context->RSSetState(rasterState);
        context->VSSetShader(vs, nullptr, 0);
        context->PSSetShader(ps, nullptr, 0);
        context->IASetInputLayout(inputLayout);

        context->PSSetSamplers(0, 1, sampler.ConstAddress());
        context->PSSetShaderResources(0, 1, displaySRV.ConstAddress());

        context->DrawIndexed(6, 0, 0);
      }
      swapChain->Present(0, 0);
    }
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}


void CompileShader(const char *shaderText, const char *entryPoint, const char *compileType, std::vector<u8> *outData)
{
  u32 flags = 0; //D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if DEBUG
  flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

  SP<ID3DBlob> code;
  SP<ID3DBlob> errors;
  HRESULT hr = D3DCompile(
    shaderText,
    strlen(shaderText),
    nullptr,
    nullptr,
    nullptr,
    entryPoint,
    compileType,
    flags,
    0,
    code.Address(),
    errors.Address());

  if (FAILED(hr))
  {
    const char * except = (const char *)errors->GetBufferPointer();
    UNUSED_VAR(except);
    ASSERT(false);
  }

  u32 codeSize = u32(code->GetBufferSize());
  u8 * codeBytes = reinterpret_cast<u8*>(code->GetBufferPointer());

  outData->resize(codeSize);
  memcpy(outData->data(), codeBytes, codeSize);
}


bool StringEquals(const wchar_t *a, const wchar_t *b)
{
  return wcscmp(a, b) == 0;
}


int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
  CoInitialize(nullptr);

  InitDebugger();

  vertices[0] = {100, 100, 0, 0};
  vertices[1] = {500, 100, 1, 0};
  vertices[2] = {500, 800, 1, 1};
  vertices[3] = {100, 800, 0, 1};
  UNUSED_VAR(hPrevInstance);
  UNUSED_VAR(lpCmdLine);

  HMONITOR primaryMon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO info;
  ZeroType(&info);
  info.cbSize = sizeof(MONITORINFO);
  GetMonitorInfo(primaryMon, &info);

  WNDCLASSEXW wcex;
  ZeroType(&wcex);

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  //wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SIMPLEIMAGEVIEW2));
  //wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
  //wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_SIMPLEIMAGEVIEW2);
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = nullptr;
  wcex.lpszClassName = L"NTSCify";

  RegisterClassExW(&wcex);
  HWND hWnd = CreateWindowW(
    L"NTSCify", 
    L"NTSCify", 
    WS_OVERLAPPEDWINDOW, 
    0, 
    0, 
    info.rcMonitor.right - info.rcMonitor.left, 
    info.rcMonitor.bottom - info.rcMonitor.top, 
    nullptr, 
    nullptr, 
    hInstance, 
    nullptr);

  if (!hWnd)
  {
    return FALSE;
  }

  DXGI_SWAP_CHAIN_DESC swapDesc;
  ZeroMemory(&swapDesc, sizeof(swapDesc));
  swapDesc.BufferCount = 1;
  swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapDesc.OutputWindow = hWnd;
  swapDesc.Windowed = true;
  swapDesc.SampleDesc.Count = 1;
    
  swapDesc.BufferDesc.RefreshRate.Numerator = 60;
  swapDesc.BufferDesc.RefreshRate.Denominator = 1;

  swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

  swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  {
    RECT r;
    GetClientRect(hWnd, &r);
    screenWidth = swapDesc.BufferDesc.Width = r.right - r.left;
    screenHeight = swapDesc.BufferDesc.Height = r.bottom - r.top;
  }

  D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

  D3D11CreateDeviceAndSwapChain(
    nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
#if DEBUG
    D3D11_CREATE_DEVICE_DEBUG,
#else
    0,
#endif
    &featureLevel,
    1,
    D3D11_SDK_VERSION,
    &swapDesc,
    swapChain.Address(),
    device.Address(),
    nullptr,
    context.Address());

  swapChain->GetBuffer(0,  __uuidof(backbuffer.Ptr()), reinterpret_cast<void**>(backbuffer.Address()));
  device->CreateRenderTargetView(backbuffer, nullptr, backbufferView.Address());

  {
    D3D11_RASTERIZER_DESC rasterDesc;
    ZeroMemory(&rasterDesc, sizeof(rasterDesc));
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rasterDesc, rasterState.Address());
  }

  {
    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(blendDesc));
    blendDesc.RenderTarget[0].BlendEnable = false;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blendDesc, blendState.Address());
  }

  {
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ZeroMemory(&dsDesc, sizeof(dsDesc));
    device->CreateDepthStencilState(&dsDesc, depthState.Address());
  }

  {
    D3D11_SAMPLER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Filter = D3D11_FILTER_ANISOTROPIC; // D3D11_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MaxAnisotropy = 16;
    desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    device->CreateSamplerState(&desc, sampler.Address());
  }

  {
    D3D11_BUFFER_DESC vbDesc;
    ZeroMemory(&vbDesc, sizeof(vbDesc));
    vbDesc.ByteWidth = sizeof(Vertex) * 4;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&vbDesc, nullptr, vb.Address());

    D3D11_BUFFER_DESC ibDesc;
    ZeroMemory(&ibDesc, sizeof(ibDesc));
    ibDesc.ByteWidth = sizeof(u16) * 6;
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;

    u16 inds[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_SUBRESOURCE_DATA init;
    init.pSysMem = inds;
    init.SysMemPitch = 0;
    init.SysMemSlicePitch = 0;

    device->CreateBuffer(&ibDesc, &init, ib.Address());
  }

  {
    std::vector<u8> data;
    CompileShader(pShader, "VS", "vs_5_0", &data);
    device->CreateVertexShader(data.data(), data.size(), nullptr, vs.Address());

    D3D11_INPUT_ELEMENT_DESC elements[] = 
    {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0}, 
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    device->CreateInputLayout(
      elements,
      2,
      data.data(),
      data.size(),
      inputLayout.Address());
  }

  {
    std::vector<u8> data;
    CompileShader(pShader, "PS", "ps_5_0", &data);
    device->CreatePixelShader(data.data(), data.size(), nullptr, ps.Address());
  }

  window = hWnd;
  //LoadTexture(currentFilename);

  ShowWindow(hWnd, nCmdShow);
  InvalidateRect(window, nullptr, true);
  UpdateWindow(hWnd);

  if (fullscreen)
  {
    SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
  }

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return int(msg.wParam);
}
