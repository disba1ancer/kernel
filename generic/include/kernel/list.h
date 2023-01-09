#ifndef LIST_H
#define LIST_H

#include <concepts>
#include "node.h"
#include "util.hpp"

namespace kernel::intrusive {

template <typename Tag = void>
class ListNode;

template <>
class ListNode<void> {
    template <typename T, typename CastPolicyGen>
    friend class List;
    ListNode* prev;
    ListNode* next;
};

template <typename Tag>
class ListNode : public ListNode<> {};

template <typename T, typename CastPolicyGen = BaseClassCastPolicy<void>>
class List;

template <typename T, typename CastPolicyGen>
class List : detail::ContainerNodeRequirments<ListNode, T, CastPolicyGen> {
    using Node = ListNode<>;
    using CastPolicy = typename CastPolicyGen::template Policy<T, ListNode>;
public:
    class Iterator : public detail::BasicIterator<Iterator, T> {
        friend class List;
        Iterator(Node* ptr) noexcept : ptr(ptr) {}
    public:
        Iterator& operator++() noexcept
        {
            ptr = ptr->next;
            return *this;
        }

        Iterator& operator--() noexcept
        {
            ptr = ptr->prev;
            return *this;
        }

        T* operator->() const noexcept
        {
            return CastPolicy::GetPtrFromHeader(ptr);
        }

        bool operator!=(const Iterator& other) const noexcept
        {
            return this->ptr != other.ptr;
        }
    private:
        Node* ptr;
    };

    List() noexcept {
        sentinel.prev = sentinel.next = &sentinel;
    }

    Iterator Insert(Iterator it, T& ref)
        noexcept(noexcept(CastPolicy::GetHeaderFromPtr(AddressOf(ref))))
    {
        Node* elem = CastPolicy::GetHeaderFromPtr(AddressOf(ref));
        Node* next = it.ptr;
        Node* prev = next->prev;
        elem->prev = prev;
        prev->next = elem;
        elem->next = next;
        next->prev = elem;
        return { elem };
    }

    void Erase(Iterator it) noexcept
    {
        Node* elem = it.ptr;
        Node* prev = elem->prev;
        Node* next = elem->next;
        prev->next = next;
        next->prev = prev;
    }

    void Erase(T& ref)
        noexcept(noexcept(IteratorTo(ref)))
    {
        Erase(IteratorTo(ref));
    }

    Iterator Begin() noexcept
    {
        return { sentinel.next };
    }

    Iterator End() noexcept
    {
        return { &sentinel };
    }

    friend Iterator begin(List& list) noexcept
    {
        return list.Begin();
    }

    friend Iterator end(List& list) noexcept
    {
        return list.End();
    }

    void PushBack(T& ref) noexcept
    {
        Insert(End(), ref);
    }

    void PopBack() noexcept
    {
        Erase(--End());
    }

    auto IteratorTo(T& elem)
        noexcept(noexcept(CastPolicy::GetHeaderFromPtr(AddressOf(elem))))
        -> Iterator
    {
        return CastPolicy::GetHeaderFromPtr(AddressOf(elem));
    }
private:
    Node sentinel;
};

}

#endif // LIST_H
