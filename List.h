#pragma once

#pragma warning (push)
#pragma warning (disable: 4987 4623 4626 5027)
#include <utility>
#include <algorithm>
#pragma warning (pop)

#include "BaseTypes.h"
#include "Debug.h"

template<class T>
class DynamicList;

template<class T>
class List
{
    friend class MemoryWriter;

public:
    template<typename ListType, typename TypeType>
    class InternalRev
    {
        friend class List;
        friend class DynamicList<T>;
    
    public:
        class InternalRevIter
        {
            friend class List;
            friend class DynamicList<T>;

        public:
            InternalRevIter & operator ++ ()
            {
                index--;
                return *this;
            }

            bool operator == (const InternalRevIter & b) const
            {
                return list == b.list && index == b.index;
            }

            bool operator != (const InternalRevIter & b) const
            {
                return list != b.list || index != b.index;
            }

            TypeType & operator * ()
            {
                return (*list)[index];
            }

        private:
            InternalRevIter(ListType * l, s32 indexIn)
            : list(l)
            , index(indexIn)
            {
            }

            ListType * list;
            s32 index;
        };

        inline const InternalRevIter begin() const 
        {
            return InternalRevIter(list, s32(list->Count()) - 1);
        }

        inline const InternalRevIter end() const
        {
            return InternalRevIter(list, -1);
        }

    private:
        InternalRev(ListType * listIn)
        : list(listIn)
        {
        }

        ListType * list;
    };

    u32 Count() const
    {
        return length;
    }

    const T & operator[] (u32 i) const
    {
        return data[i];
    }

    const T & LastElement () const
    {
        ASSERT(length > 0);
        return data[length - 1];
    }

    inline const T * begin() const 
    {
        return data;
    }

    inline const T * end() const
    {
        return data + length;
    }

    InternalRev<const List, const T> IterReverse() const
    {
        return InternalRev<const List, const T>(this);
    }
        
    bool Contains(const T & t) const
    {
        for (u32 i = 0; i < length; i++)
        {
            if (data[i] == t)
            {
                return true;
            }
        }
        return false;
    }

    int IndexOf(const T & t, u32 startIndex=0, u32 count = -1) const
    {
        if (startIndex == 0 && count == -1 && length == 0)
        {
            return -1;
        }
        ASSERT(startIndex < length, "Index {0} out of range!", startIndex);

        if (count == u32(-1))
        {
            count = length-startIndex;
        }

        ASSERT(count+startIndex <= length, "Index {0} out of range!", count);

        for (u32 i = startIndex; i < startIndex+count; i++)
        {
            if (data[i] == t)
            {
                return int(i);
            }
        }
        return -1;
    }

    int LastIndexOf(const T & t, u32 startIndex = -1, u32 count = -1) const
    {
        if (startIndex == u32(-1))
        {
            startIndex = length-1;
        }
        ASSERT(startIndex < length, "Index {0} out of range!", startIndex);

        if (count == u32(-1))
        {
            count = startIndex+1;
        }

        ASSERT(startIndex + 1 - count >= 0, "Count {0} out of range!", count);

        for (u32 i = 0; i < count; i++)
        {
            if (data[startIndex-i] == t)
            {
                return int(startIndex-i);
            }
        }
        return -1;
    }

    const T * DataArray() const
    {
        return data;
    }
protected:
    u32 length;
    T * data;
};

template<class T>
class DynamicList : public List<T>
{
    using List<T>::InternalRev;
    using List<T>::length;
    using List<T>::data;

public:    
    DynamicList()
    : DynamicList(DefaultInitialCapacity, DefaultCapacityIncrease, DefaultMaxCapacity)
    {
    }

    DynamicList(u32 initialCapacity)
    : DynamicList(initialCapacity, DefaultCapacityIncrease, DefaultMaxCapacity)
    {
    }

    DynamicList(u32 initialCapacity, u32 growBy)
    : DynamicList(initialCapacity, growBy, DefaultMaxCapacity)
    {
    }

    DynamicList(const std::initializer_list<T> & init)
    : DynamicList(std::max(DefaultInitialCapacity, u32(init.size())), DefaultCapacityIncrease, DefaultMaxCapacity)
    {
        length = u32(init.size());
        u32 i = 0;
        for (auto & t : init)
        {
#pragma push_macro("new")
#undef new
            new (&data[i]) T();
#pragma pop_macro("new")
            data[i] = t;
            i++;
        }
    }

    DynamicList(DynamicList<T> && moveFrom)
    {
        data = moveFrom.data;
        length = moveFrom.length;
        capacity = moveFrom.capacity;
        capacityIncrease = moveFrom.capacityIncrease;
        maxCapacity = moveFrom.maxCapacity;

        moveFrom.data = nullptr;
        moveFrom.length = 0;
        moveFrom.capacity = 0;
    }

    DynamicList(const DynamicList<T> & copyFrom)
    : DynamicList((const List<T> &)copyFrom)
    {
    }

    DynamicList(const List<T> & copyFrom)
    : DynamicList(std::max(DefaultInitialCapacity, copyFrom.Count()), DefaultCapacityIncrease, DefaultMaxCapacity)
    {
        length = copyFrom.Count();
        for (u32 i = 0; i < length; i++)
        {
#pragma push_macro("new")
#undef new
            new (&data[i]) T();
#pragma pop_macro("new")
            data[i] = copyFrom[i];
        }
    }

    DynamicList(const List<T> & copyFrom, u32 index, u32 count)
    : DynamicList(std::max(DefaultInitialCapacity, count), DefaultCapacityIncrease, DefaultMaxCapacity)
    {
        ASSERT(index < copyFrom.length, "Index out of range!");
        ASSERT(index+count <= copyFrom.length, "Index and count out of range!");

        length = count;
        for (u32 i = 0; i < length; i++)
        {
            data[i] = data[i+index];
        }
    }

    DynamicList(u32 initialCapacity, u32 growBy, u32 maxCap)
    : capacity(initialCapacity),
      capacityIncrease(growBy),
      maxCapacity(maxCap)
    {
        length = 0;
        if (initialCapacity > 0)
        {
            data = reinterpret_cast<T*>(new u8 [initialCapacity*sizeof(T)]);
        }
        else
        {
            data = nullptr;
        }
    }

    ~DynamicList()
    {
        for (u32 i = 0; i < length; i++)
        {
            data[i].~T();
        }
        delete[] reinterpret_cast<u8 *>(data);
    }

    DynamicList<T> & operator = (const DynamicList<T> & copyFrom)
    {
        // Delete all the existing items and set the length to 0 so EnsureCapacity won't copy anything

        for (u32 i = 0; i < length; i++)
        {
            data[i].~T();
        }
        length = 0;

        EnsureCapacity(copyFrom.length);
        length = copyFrom.length;
        for (u32 i = 0; i < length; i++)
        {
#pragma push_macro("new")
#undef new
            new (&data[i]) T();
#pragma pop_macro("new")
            data[i] = copyFrom[i];
        }
        return *this;
    }

    DynamicList<T> & operator = (const List<T> & copyFrom)
    {
        for (u32 i = 0; i < length; i++)
        {
            data[i].~T();
        }
        length = 0;
        EnsureCapacity(copyFrom.Count());
        length = copyFrom.Count();
        for (u32 i = 0; i < length; i++)
        {
#pragma push_macro("new")
#undef new
            new (&data[i]) T();
#pragma pop_macro("new")
            data[i] = copyFrom[i];
        }
        return *this;
    }

    const T & operator[] (u32 i) const
    {
        return data[i];
    }

    T & operator[] (u32 i)
    {
        return data[i];
    }

    const T & LastElement () const
    {
        ASSERT(length > 0);
        return data[length - 1];
    }

    T & LastElement ()
    {
        ASSERT(length > 0);
        return data[length - 1];
    }

    inline T * begin()
    {
        return data;
    }

    inline T * end()
    {
        return data + length;
    }

    inline const T * begin() const
    {
        return data;
    }

    inline const T * end() const
    {
        return data + length;
    }

    InternalRev<const DynamicList, const T> IterReverse() const
    {
        return InternalRev<const DynamicList, const T>(this);
    }
        
    InternalRev<DynamicList, T> IterReverse()
    {
        return InternalRev<DynamicList, T>(this);
    }
        
    void Clear()
    {
        for (u32 i = 0; i < length; i++)
        {
            data[i].~T();
        }
        length = 0;        
    }

    // Add to the end of the list
    T * Add(const T & t)
    {
        if (length == capacity)
        {
            Grow();
        }
#pragma push_macro("new")
#undef new
        new (&this->data[this->length]) T(t);
#pragma pop_macro("new")
        length++;

        return &this->data[this->length-1];
    }

    T * Add(T && t)
    {
        if (length == capacity)
        {
            Grow();
        }
#pragma push_macro("new")
#undef new
        new (&this->data[this->length]) T(std::move(t));
#pragma pop_macro("new")

        length++;

        return &this->data[this->length-1];
    }

    void AddAll(const List<T> & t)
    {
        EnsureCapacity(length + t.Count());

        for (u32 i = 0; i < t.Count(); i++)
        {
#pragma push_macro("new")
#undef new
            new (&this->data[length]) T(t[i]);
            length++;
#pragma pop_macro("new")
        }
    }

    T * AddNew()
    {
        if (length == capacity)
        {
            Grow();
        }
#pragma push_macro("new")
#undef new
        new (&this->data[this->length]) T();
#pragma pop_macro("new")
        length++;
        return &this->data[this->length-1];
    }

    void Insert(const T & t, u32 index)
    {
        ASSERT(index <= length, "Index out of range!");

        if (length == capacity)
        {
            Grow(); // TODO: this could be more efficient by doing less moves instead of double-moving things.
        }

        length++;
#pragma push_macro("new")
#undef new
        new (&data[length-1]) T();  // Alloc a new temp entry at the new top of the list
#pragma pop_macro("new")
        
        for (u32 i = length-1; i > index; i--)
        {
            data[i] = std::move(data[i-1]); // Move all the things
        }
        data[index] = t; // Copy in the new data
    }

    void Insert(T && t, u32 index)
    {
        ASSERT(index <= length, "Index out of range!");

        if (length == capacity)
        {
            Grow(); // TODO: this could be more efficient by doing less moves instead of double-moving things.
        }
        length++;
        
#pragma push_macro("new")
#undef new
        new (&data[length-1]) T();
#pragma pop_macro("new")
        
        for (u32 i = length-1; i > index; i--)
        {
            data[i] = std::move(data[i-1]);
        }
        data[index] = std::move(t);
    }

    void Reverse()
    {
        for (u32 i = 0; i < length/2; i++)
        {
            u32 j = length-1-i;
            if (j == i)
            {
                continue;
            }

            T temp = std::move(data[i]);
            data[i] = std::move(data[j]);
            data[j] = std::move(temp);
        }
    }

    // Remove the first instance of a given object
    void Remove(const T & t)
    {
        for (u32 i = 0; i < length; i++)
        {
            if (data[i] == t)
            {
                RemoveAt(i);
                return;
            }
        }
    }

    void RemoveAll(const T & t)
    {
        u32 moveOffset = 0;
        u32 oldLength = length;
        for (u32 i = 0; i < length; i++)
        {
            if (moveOffset > 0)
            {
                data[i] = std::move(data[i+moveOffset]);
            }

            if (data[i] == t)
            {
                length--;
                i--;
                moveOffset++;
            }
        }
        for (u32 i = length; i < oldLength; i++)
        {
            data[i].~T();
        }
    }

    void RemoveAt(u32 index)
    {
        RemoveRange(index, 1);
    }

    void RemoveRange(u32 index, u32 count)
    {
        if (count == 0)
        {
            return;
        }
        ASSERT(index < length, "Index out of range!");
        ASSERT(index+count <= length, "Index and count out of range!");

        for (u32 i = index+count; i < length; i++)
        {
            data[i-count] = std::move(data[i]);
        }

        for (u32 i = length-count; i < length; i++)
        {
            data[i].~T();
        }
        length -= count;
    }

    void Sort()
    {
        std::sort(begin(), end());
    }

    void Sort(bool (*Compare)(const T & a, const T & b))
    {
        std::sort(begin(), end(), Compare);
    }

    void ExpandSizeBy(u32 increaseInCount)
    {
        u32 oldLength = length;
        u32 newLength = length + increaseInCount;
        EnsureCapacity(newLength);
        length = newLength;

        for (u32 i = oldLength; i < newLength; i++)
        {
#pragma push_macro("new")
#undef new
            new (&data[i]) T();
#pragma pop_macro("new")
        }
    }

    void EnsureCapacity(u32 newCapacity)
    {
        ASSERT(newCapacity <= maxCapacity);

        if (capacity < newCapacity)
        {
            // Temporarily modify capacityIncrease to grow it directly to the value we want.

            u32 capInc = capacityIncrease;
            capacityIncrease = newCapacity - capacity;
            Grow();
            capacityIncrease = capInc;
        }
    }

    const T * DataArray() const
    {
        return data;
    }

    T * DataArray()
    {
        return data;
    }

    void SwapWith(DynamicList<T> * other)
    {
        T* tempData = data;
        data = other->data;
        other->data = tempData;

        u32 tempLen = length;
        length = other->length;
        other->length = tempLen;

        u32 tempCapacity = capacity;
        capacity = other->capacity;
        other->capacity = tempCapacity;

        u32 tempCapacityIncrease = capacityIncrease;
        capacityIncrease = other->capacityIncrease;
        other->capacityIncrease = tempCapacityIncrease;

        u32 tempMaxCapacity = maxCapacity;
        maxCapacity = other->maxCapacity;
        other->maxCapacity = tempMaxCapacity;
    }


protected:

    void Grow()
    {
        ASSERT(capacity < maxCapacity, "Can't grow list, is at capacity!");
        T * oldData = data;
        capacity += capacityIncrease;
        
        if (capacity > maxCapacity)
        {
            capacity = maxCapacity;
        }

        data = reinterpret_cast<T *>(new u8[capacity*sizeof(T)]);
        for (u32 i = 0 ; i < length; i++)
        {
#pragma push_macro("new")
#undef new
            new (&data[i]) T(std::move(oldData[i]));
#pragma pop_macro("new")
            oldData[i].~T();
        }

        if (oldData != nullptr)
        {
            delete[] reinterpret_cast<u8 *>(oldData);
        }
    }

    static const u32 DefaultInitialCapacity = 64;
    static const u32 DefaultCapacityIncrease = 128;
    static const u32 DefaultMaxCapacity = 0xFFFFFFFF; // Really huge woo

    u32 capacity;
    u32 capacityIncrease;
    u32 maxCapacity;
};