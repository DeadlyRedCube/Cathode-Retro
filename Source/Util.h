#pragma once

#include <cmath>
#include <string.h>
#include <type_traits>
#include <vector>


template <typename T>
void ZeroType(T *t, size_t count = 1)
{
  memset(t, 0, sizeof(T) * count);
}


template <typename T>
void CopyType(T *dest, const T *src, size_t count = 1)
{
  memcpy(dest, src, sizeof(T) * count);
}


template <typename T>
constexpr auto EnumValue(T v)
{
  return static_cast<std::underlying_type_t<T>>(v);
}


// helper to test if a type is an enum class (vs. just an enum)
template <typename Type, typename = Type>
struct is_enum_class : public std::is_enum<Type>
{
};

template <typename Type>
struct is_enum_class<Type, decltype(Type(+std::declval<Type>()))> : public std::false_type
{
};

// Helper to test if a type is a flag enum class (that is, an enum class that has None and All members)
template <typename Type, typename = Type, typename = Type>
struct is_flag_enum_class : public std::false_type
{
};

template <typename Type>
struct is_flag_enum_class<Type, decltype(Type::None), decltype(Type::All)> : public is_enum_class<Type>
{
};


// Unary flag enum class operators
template <typename Type, typename std::enable_if<is_flag_enum_class<Type>::value, int>::type = 0>
constexpr Type operator ~ (Type a)
{
  return static_cast<Type>(~static_cast<std::underlying_type_t<Type>>(a));
}


// Binary flag enum class operators
template <typename Type, typename std::enable_if<is_flag_enum_class<Type>::value, int>::type = 0>
constexpr Type operator | (Type a, Type b)
{
  return static_cast<Type>(static_cast<std::underlying_type_t<Type>>(a) | static_cast<std::underlying_type_t<Type>>(b));
}


template <typename Type, typename std::enable_if<is_flag_enum_class<Type>::value, int>::type = 0>
constexpr Type operator & (Type a, Type b)
{
  return static_cast<Type>(static_cast<std::underlying_type_t<Type>>(a) & static_cast<std::underlying_type_t<Type>>(b));
}


template <typename Type, typename std::enable_if<is_flag_enum_class<Type>::value, int>::type = 0>
constexpr Type operator ^ (Type a, Type b)
{
  return static_cast<Type>(static_cast<std::underlying_type_t<Type>>(a) ^ static_cast<std::underlying_type_t<Type>>(b));
}


// Assigning flag enum class operators
template <typename Type, typename std::enable_if<is_flag_enum_class<Type>::value, int>::type = 0>
constexpr Type &operator |= (Type &a, Type b)
{
  a = a | b;
  return a;
}


template <typename Type, typename std::enable_if<is_flag_enum_class<Type>::value, int>::type = 0>
constexpr Type &operator &= (Type &a, Type b)
{
  a = a & b;
  return a;
}


template <typename Type, typename std::enable_if<is_flag_enum_class<Type>::value, int>::type = 0>
constexpr Type &operator ^= (Type &a, Type b)
{
  a = a ^ b;
  return a;
}


template <typename T> size_t k_arrayLength = 0;
template <typename T> size_t k_arrayLength<T &> = k_arrayLength<T>;
template <typename T, size_t N> size_t k_arrayLength<T[N]> = N;


inline float Gaussian(float x, float sigma)
{
  auto sqrTerm = x / sigma;
  return std::exp(-0.5f *  sqrTerm * sqrTerm);
}


inline std::vector<float> CalculateGaussianKernel(int32_t filterWidth, float sigma)
{
  std::vector<float> kernel;
  kernel.resize(filterWidth);

  float sum = 0.0f;
  for (int32_t i = 0; i < filterWidth; i++)
  {
    kernel[i] = Gaussian(float(i - (filterWidth - 1) / 2), sigma);
    sum += kernel[i];
  }

  for (int32_t i = 0; i < filterWidth; i++)
  {
    kernel[i] /= sum;
  }
  return kernel;
}