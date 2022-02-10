#pragma once

#include <utility>


// A simple "allocate once then forget about it" array structure.
template <typename T>
class SimpleArray
{
public:
  SimpleArray() = default;
  explicit SimpleArray(size_t capacity)
  : length(capacity)
  , buffer(new T[capacity])
    { }

  SimpleArray(const SimpleArray &) = delete;
  void operator = (const SimpleArray &) = delete;

  SimpleArray(SimpleArray &&b)
    : length(std::exchange(b.length, 0))
    , buffer(std::exchange(b.buffer, nullptr))
    { }

  SimpleArray &operator = (SimpleArray &&b)
  {
    if (&b != this)
    {
      length = std::exchange(b.length, 0);
      buffer = std::exchange(b.buffer, nullptr);
    }

    return *this;
  }

  ~SimpleArray()
    { delete buffer; }

  size_t Length() const
    { return length; }

  T &operator[] (size_t i)
    { return buffer[i]; }

  const T &operator[] (size_t i) const
    { return buffer[i]; }

  T *Ptr()
    { return buffer; }

  const T *Ptr() const
    { return buffer; }

protected:
  size_t length = 0;
  T *buffer = nullptr;
};