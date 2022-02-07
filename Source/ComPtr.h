#pragma once

#include <utility>

// Strong m_pointer - Used to hold COM objects
template <typename T>
class ComPtr
{
public:

  ComPtr() = default;
  ComPtr(const ComPtr<T> &var)
  {
    m_pointer = nullptr;
    *this = var.m_pointer;
  }

  ComPtr(ComPtr<T> && var)
  {
    m_pointer = var.m_pointer;
    var.m_pointer = nullptr;
  }
   
  ComPtr(T *pT)
  { 
    m_pointer = nullptr;
    *this = pT;
  }
   
  ~ComPtr()  
  { 
    if(m_pointer != nullptr) 
    {
      m_pointer->Release();
    }
  }

  inline ComPtr<T> &operator = (const ComPtr<T> &var)
  {
    // Add a reference to the other pointer then remove ours (just in case they're the same pointer)

    if(var.m_pointer != nullptr) 
    {
      var.m_pointer->AddRef();
    }
    if(m_pointer != nullptr) 
    {
      m_pointer->Release();
    }

    // Now update our assignment

    m_pointer = var.m_pointer;
    return *this;
  }

  inline ComPtr<T> & operator = (ComPtr<T> && var)
  {
    if (m_pointer != nullptr)
    {
      m_pointer->Release();
    }
    m_pointer = var.m_pointer;
    var.m_pointer = nullptr;
    return *this;
  }
   
  inline ComPtr<T> &operator = (T *var)
  {
    // Much the same as assignment from another ComPtr
    if(var != nullptr) 
    {
      var->AddRef();
    }
    if(m_pointer != nullptr) 
    {
      m_pointer->Release();
    }
    m_pointer = var;
    return *this;
  }

  // Conversion to the type in question (so that this pointer acts just like a normal pointer)
  inline operator T *() const 
    { return m_pointer; }

  inline T *operator -> () const 
  { 
    return m_pointer; 
  }

  // Make sure we can compare correctly between SPs
  inline bool operator == (const ComPtr<T> &rhs) const 
    { return (m_pointer == rhs.m_pointer); }
  
  inline bool operator == (ComPtr<T> &rhs) const 
    { return (m_pointer == rhs.m_pointer); }
  
  inline bool operator == (ComPtr<T> &rhs)
    { return (m_pointer == rhs.m_pointer); }
  
  inline bool operator == (const ComPtr<T> &rhs)
    { return (m_pointer == rhs.m_pointer); }
  
  inline bool operator == (const T *rhs) const
    { return (m_pointer == rhs); }

  inline bool operator == (T *rhs)
    { return (m_pointer == rhs); }

  inline bool operator != (const ComPtr<T> &rhs) const 
    { return (m_pointer != rhs.m_pointer); }

  inline bool operator != (ComPtr<T> &rhs) const 
    { return (m_pointer != rhs.m_pointer); }

  inline bool operator != (const ComPtr<T> &rhs)
    { return (m_pointer != rhs.m_pointer); }

  inline bool operator != (ComPtr<T> &rhs)
    { return (m_pointer != rhs.m_pointer); }

  inline bool operator != (const T *rhs) const
    { return (m_pointer != rhs); }

  inline bool operator != (T *rhs)
    { return (m_pointer != rhs); }

  inline bool operator ! () const
    { return (!m_pointer); }

  inline T *Ptr() const
    { return m_pointer; }
  
  // This is for when you need to pass it into a T** parameter to a function.
  // For instance:
  //   Foo(a, b, thisPointer.Address());
  // This assumes a few things, most notably that the pointer passed in T** is given an
  // initial reference count of 1 (Most COM functions do this very thing, so it's a generally safe
  // assumption).
  inline T **AddressForReplace()
  {
    if(m_pointer)
    {
      m_pointer->Release();
    }
    m_pointer = 0;
    return &m_pointer;
  }
  
  inline T * const * ConstAddress() const
  {
    return &m_pointer; 
  }

private:
  T *m_pointer = nullptr;
};
