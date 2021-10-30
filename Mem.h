#pragma once

#include <type_traits>

namespace NTSC
{
	template<typename T>
	void ZeroType(T * t)
	{
		memset(t, 0, sizeof(T));
	}

	template<typename T, int N>
	void ZeroArray(T(&a)[N])
	{
		memset(a, 0, sizeof(T)*N);
	}

	template<typename T>
	void CopyArray(T * dest, const T * src, u64 count)
	{
		memcpy(dest, src, count * sizeof(T));
	}

	// $TODO we probably don't need the constexpr version of this, and I definitely don't want to send this out with a macro, so fix that.
	template <typename T> struct ArrayLength : public std::integral_constant<size_t, 0> {};
	template <typename T, size_t N> struct ArrayLength<T[N]> : public std::integral_constant<size_t, N> {};
	#define ArrayLength(a) ArrayLength<std::remove_reference_t<decltype(a)>>::value
}