#pragma once

#include <cmath>

namespace NTSC::Math
{
  template <typename IntT>
  constexpr IntT AlignInt(IntT value, size_t requiredAlignment)
    { return IntT(value + requiredAlignment - 1) & ~(requiredAlignment - 1); }

  template <typename Type>
  static constexpr Type k_pi = Type(3.1415926535897932384626433832795028841971);


  inline f32 SinPi(f32 x)
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


  inline f32 CosPi(f32 x)
  {
    // Calculate CosPi in terms of SinPi instead of making a whole other taylor series. This will lose accuracy at VERY LARGE NUMBERS (once 0.5 is off the end of the mantissa)
    //  but for this project I don't care about that at all.
    return SinPi(-std::abs(x) + 0.5f);
  }


  // Sinc of a value that is multiples of pi (where sinc(x) = sin(x) / x, and sinc(0) == 1.0)
  inline f32 SincPi(f32 x)
  {
    if (std::abs(x) < 0.01f)
    {
      // Near 0 we need to approximate sin(x)/x using a taylor series to avoid precision issues.
      // Unlike in SinPi we don't have to deal with angle wrapping or sign flips because we know we're near Actual Zero

      // The beauty of this is that every term the sin taylor series has an x term in it, so we can just multiply in one less x (or, in our case, one less x*pi)
      // So the coefficients for this taylor series are the same terms for SinPi(x) except with one less pi factored in.
      // Our actual taylor series looks like 1 - x^2 * (pi^2 / 3!) + x^4 * (pi^4 / 5!) + x^6 * (pi^6 / 7!) + ...

      // I'm also using less terms here than above because we're so close to 0 that our accuracy is already fine by 9 terms.
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
}