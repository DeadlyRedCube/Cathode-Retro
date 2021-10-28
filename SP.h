#pragma once
#include "Debug.h"

// Strong Pointer - This is the handy auto-cleanupping class
template <class T>
class SP
{
public:

	SP()
	{
		Pointer = nullptr;
	}

	SP(const SP<T> &var)
	{
		Pointer = nullptr;
		*this = var.Pointer;
	}

	SP(SP<T> && var)
	{
		Pointer = var.Pointer;
		var.Pointer = nullptr;
	}

	SP(T *pT)
	{
		Pointer = nullptr;
		*this = pT;
	}

	~SP()
	{
		if (Pointer != nullptr)
		{
			Pointer->Release();
#if DEBUG
			Pointer = nullptr;
#endif
		}
	}

	inline SP<T> &operator = (const SP<T> &var)
	{
		// Add a reference to the other pointer then remove ours (just in case they're the same pointer)

		if (var.Pointer != nullptr)
		{
			var.Pointer->AddRef();
		}
		if (Pointer != nullptr)
		{
			Pointer->Release();
		}

		// Now update our assignment

		Pointer = var.Pointer;
		return *this;
	}

	inline SP<T> & operator = (SP<T> && var)
	{
		if (Pointer != nullptr)
		{
			Pointer->Release();
		}
		Pointer = var.Pointer;
		var.Pointer = nullptr;
		return *this;
	}

	inline SP<T> &operator = (T *var)
	{
		// Much the same as assignment from another SP
		if (var != nullptr)
		{
			var->AddRef();
		}
		if (Pointer != nullptr)
		{
			Pointer->Release();
		}
		Pointer = var;
		return *this;
	}

	// Conversion to the type in question (so that this pointer acts just like a normal pointer)
	inline operator T *() const
	{
		return Pointer;
	}

	inline T* operator -> () const
	{
		ASSERT(Pointer != nullptr);
		return Pointer;
	}

	// Make sure we can compare correctly between SPs
	inline bool operator == (const SP<T> &rhs) const
	{
		return (Pointer == rhs.Pointer);
	}

	inline bool operator == (SP<T> &rhs) const
	{
		return (Pointer == rhs.Pointer);
	}

	inline bool operator == (SP<T> &rhs)
	{
		return (Pointer == rhs.Pointer);
	}

	inline bool operator == (const SP<T> &rhs)
	{
		return (Pointer == rhs.Pointer);
	}

	inline bool operator == (const T *rhs) const
	{
		return (Pointer == rhs);
	}

	inline bool operator == (T *rhs)
	{
		return (Pointer == rhs);
	}

	inline bool operator != (const SP<T> &rhs) const
	{
		return (Pointer != rhs.Pointer);
	}

	inline bool operator != (SP<T> &rhs) const
	{
		return (Pointer != rhs.Pointer);
	}

	inline bool operator != (const SP<T> &rhs)
	{
		return (Pointer != rhs.Pointer);
	}

	inline bool operator != (SP<T> &rhs)
	{
		return (Pointer != rhs.Pointer);
	}

	inline bool operator != (const T *rhs) const
	{
		return (Pointer != rhs);
	}

	inline bool operator != (T *rhs)
	{
		return (Pointer != rhs);
	}

	inline bool operator ! () const
	{
		return (!Pointer);
	}

	inline T * Ptr() const
	{
		return Pointer;
	}

	// Convert to a boolean (for if (Pointer) comparisons)

	// This is for when you need to pass it into a T** parameter to a function.
	// For instance:
	//   Foo(a, b, thisPointer.Address());
	// This assumes a few things, most notably that the pointer passed in T** is given an
	// initial reference count of 1 (Most COM functions do this very thing, so it's a generally safe
	// assumption).
	inline T **Address()
	{
		if (Pointer)
		{
			Pointer->Release();
		}
		Pointer = 0;
		return &Pointer;
	}

	inline T * const * ConstAddress() const
	{
		return &Pointer;
	}

protected:
	T * Pointer;
};

class WPBase
{
	friend class Counted;
	template<class T> friend class WP; // Not sure why this is necessary since WP is derived, but lol.

public:
	virtual void Clear() = 0;
protected:
	WPBase * Next;
	WPBase * Prev;

	WPBase()
		: Next(nullptr),
		Prev(nullptr)
	{
	}


};

template<class T>
class WP : public WPBase
{
public:
	WP()
		: Pointer(nullptr)
	{
	}

	// Constructor (passed another SP)
	WP(const WP<T> &var)
	{
		Pointer = nullptr;
		*this = var.Pointer;
	}

	WP(T *pT)
	{
		Pointer = nullptr;
		*this = pT;
	} // When passed a pointer (can be nullptr)

	  // Destructor.  Release the memory and clear it out.
	virtual ~WP()
	{
		Clear();
	}

	inline WP<T> &operator = (const WP<T> &var)
	{
		*this = var.Pointer;
		return *this;
	}

	inline WP<T> &operator = (T *var)
	{
		// Clear any existing pointers.
		Clear();

		ASSERT(Prev == nullptr && Next == nullptr && Pointer == nullptr, "WP failed to clear");
		Pointer = var;

		if (Pointer != nullptr)
		{
			Next = Pointer->GetFirstWeakPtr();
			if (Next != nullptr)
			{
				Next->Prev = this;
			}
			Pointer->SetFirstWeakPtr(this);
		}
		return *this;
	}

	// Conversion to the type in question (so that this pointer acts just like a normal pointer)
	inline operator T *() const
	{
		return Pointer;
	}

	inline T* operator -> () const
	{
		ASSERT(Pointer != nullptr, "NULL WP");
		return Pointer;
	}

	// Make sure we can compare correctly between SPs
	inline bool operator == (const WP<T> &rhs) const
	{
		return (Pointer == rhs.Pointer);
	}

	inline bool operator == (const T *rhs) const
	{
		return (Pointer == rhs);
	}

	inline bool operator != (const WP<T> &rhs) const
	{
		return (Pointer != rhs.Pointer);
	}

	inline bool operator != (const T *rhs) const
	{
		return (Pointer != rhs);
	}

	inline bool operator ! () const
	{
		return (!Pointer);
	}

	inline T * Ptr()
	{
		return Pointer;
	}

	inline const T * Ptr() const
	{
		return Pointer;
	}

protected:

	virtual void Clear()
	{
		// This shit is totally not threadsafe

		if (Pointer == nullptr)
		{
			// If we've already been cleared, ensure we're no longer in a list.
			ASSERT(Next == nullptr, "Non-nullptr Next Pointer");
			ASSERT(Prev == nullptr, "Non-Null Prev Pointer");
			return;
		}

		// Remove from the WP tracking list.

		if (Next != nullptr)
		{
			Next->Prev = Prev;
		}

		if (Prev != nullptr)
		{
			Prev->Next = Next;
		}
		else
		{
			Pointer->SetFirstWeakPtr(Next);
		}

		// Finally, clear all the pointers

		Next = nullptr;
		Prev = nullptr;
		Pointer = nullptr;
	}

	T * Pointer;
};

// Counted Class - Derive all SP-ed objects from this (if they're not
// COM objects or something)
class Counted
{
	template<class T> friend class WP;

protected:
	Counted()
		: Count(0),
		firstWeakPtr(nullptr)
	{
	}

	// This is for use in very rare instances where we need to temp-create a SP to a thing inside of its constructor and
	//  it needs a way to get the ref count back down to zero properly without destructing.

	int DecRefNoRelease()
	{
		if (Count != 0)
		{
			Count--;
		}
		return Count;
	}

public:
	virtual ~Counted()
	{
		// Clear all weak pointers
		while (firstWeakPtr != nullptr)
		{
			firstWeakPtr->Clear();
		}
	}

	int AddRef()
	{
		if (Count >= 0)
		{
			++Count;
		}

		return Count;
	}

	int Release()
	{
		if (Count != 0)
		{
			// Decrement the count (store it in a temp variable because if we 
			// have to delete this class, we can still return the correct value
			int tCount = --Count;
			if (Count == 0)
			{
				Count = -1; // Signal that hey, we're destructed. No more addrefs!
				delete this; // If there are no more references, delete!
			}
			return tCount;
		}
		return 0;
	};

	WPBase * GetFirstWeakPtr()
	{
		return firstWeakPtr;
	}
	void SetFirstWeakPtr(WPBase * ptr)
	{
		firstWeakPtr = ptr;
	}

private:
	int Count;
	WPBase * firstWeakPtr;
};

// Array pointer
template <class T>
class AP
{
public:

	AP()
	{
		Pointer = nullptr;
	}

	AP(T *pT)
	{
		Pointer = nullptr;
		*this = pT;
	}

	AP(AP &&src)
	{
		Pointer = src.Pointer;
		src.Pointer = nullptr;
	}

	~AP()
	{
		if (Pointer != nullptr)
		{
			delete[] Pointer;
#if DEBUG
			Pointer = nullptr;
#endif
		}
	}

	inline AP<T> &operator = (T *var)
	{
		if (Pointer != nullptr)
		{
			delete[] Pointer;
		}
		Pointer = var;
		return *this;
	}

	// Conversion to the type in question (so that this pointer acts just like a normal pointer)
	inline operator T *() const
	{
		return Pointer;
	}

	T *Ptr()
	{
		return Pointer;
	}


	inline T & operator[] (unsigned int index) const
	{
		return Pointer[index];
	}

	inline T & operator[] (int index) const
	{
		return Pointer[index];
	}

	inline bool operator == (const T *rhs) const
	{
		return (Pointer == rhs);
	}

	inline bool operator != (const T *rhs) const
	{
		return (Pointer != rhs);
	}

	inline bool operator ! () const
	{
		return (!Pointer);
	}

protected:
	T * Pointer;

private:
	AP(const AP<T> &var) = delete;
	AP<T> & operator = (const AP<T> & var) = delete;
};

// Owned Pointer - pointer to an object that this has ownership of.
template <class T>
class OwnedP
{
public:

	OwnedP() = delete;
	OwnedP(const OwnedP &) = delete;
	OwnedP(const SP<T> &var) = delete;
	OwnedP(T *pT)
	{
		Pointer = pT;
	}

	~OwnedP()
	{
		delete Pointer;
	}

	inline OwnedP<T> & operator = (const OwnedP<T> & var) = delete;
	inline OwnedP<T> & operator = (OwnedP<T> && var) = delete;
	inline OwnedP<T> & operator = (T * var) = delete;

	// Conversion to the type in question (so that this pointer acts just like a normal pointer)
	inline operator T *() const
	{
		return Pointer;
	}

	inline T * operator -> () const
	{
		ASSERT(Pointer != nullptr, "NULL SP");
		return Pointer;
	}

	// Make sure we can compare correctly between SPs
	inline bool operator == (const T *rhs) const
	{
		return (Pointer == rhs);
	}

	inline bool operator == (T *rhs)
	{
		return (Pointer == rhs);
	}

	inline bool operator != (const T *rhs) const
	{
		return (Pointer != rhs);
	}

	inline bool operator != (T *rhs)
	{
		return (Pointer != rhs);
	}

	inline bool operator ! () const
	{
		return (!Pointer);
	}

	inline T * Ptr()
	{
		return Pointer;
	}

	inline T * const * ConstAddress() const
	{
		return &Pointer;
	}

protected:
	T * Pointer;
};