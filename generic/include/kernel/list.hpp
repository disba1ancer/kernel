#ifndef LIST_H
#define LIST_H

#include "node.hpp"
#include "list_node.hpp"

namespace kernel::intrusive {

template <typename T, typename CastPolicyGen = BaseClassCastPolicy<ListNode<>, T>>
class List;

template <typename T, typename CastPolicyGen>
class List : detail::ContainerNodeRequirments<T, CastPolicyGen> {
    using CastPolicy = CastPolicyGen;
    using NodeType = typename CastPolicy::NodeType;
    using NodeTraits = ListNodeTraits<NodeType>;
public:
    class Iterator : public detail::BasicIterator<Iterator, T> {
        friend class List;
        Iterator(NodeType* ptr) noexcept : ptr(ptr) {}
    public:
        Iterator& operator++() noexcept
        {
            ptr = NodeTraits::GetNext(*ptr);
            return *this;
        }

        Iterator& operator--() noexcept
        {
            ptr = NodeTraits::GetPrev(*ptr);
            return *this;
        }

        T* operator->() const noexcept
        {
            return CastPolicy::FromNode(ptr);
        }

        bool operator==(const Iterator& other) const noexcept
        {
            return this->ptr == other.ptr;
        }
    private:
        NodeType* ptr;
    };

    List() noexcept {
        Clear();
    }

    List(List&& oth) noexcept {
        DoMove(oth);
    }

    List& operator=(List&& oth) noexcept {
        DoMove(oth);
        return *this;
    }

    Iterator Insert(Iterator it, T& ref)
        noexcept(noexcept(CastPolicy::ToNode(AddressOf(ref))))
    {
        using Tr = NodeTraits;
        NodeType* elem = CastPolicy::ToNode(AddressOf(ref));
        NodeType* next = it.ptr;
        NodeType* prev = Tr::GetPrev(*next);
        Tr::SetPrev(*elem, prev);
        Tr::SetNext(*elem, next);
        Tr::SetNext(*prev, elem);
        Tr::SetPrev(*next, elem);
        return { elem };
    }

    void Erase(Iterator it) noexcept
    {
        using Tr = NodeTraits;
        NodeType* elem = it.ptr;
        NodeType* prev = Tr::GetPrev(*elem);
        NodeType* next = Tr::GetNext(*elem);
        Tr::SetNext(*prev, next);
        Tr::SetPrev(*next, prev);
    }

    void Erase(T& ref)
        noexcept(noexcept(IteratorTo(ref)))
    {
        Erase(IteratorTo(ref));
    }

    Iterator Begin() noexcept
    {
        return { NodeTraits::GetNext(sentinel) };
    }

    Iterator End() noexcept
    {
        return { AddressOf(sentinel) };
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
        noexcept(noexcept(CastPolicy::ToNode(AddressOf(elem))))
        -> Iterator
    {
        return CastPolicy::ToNode(AddressOf(elem));
    }

    bool Empty() noexcept
    {
        return (Begin() == End());
    }

    void Clear() noexcept
    {
        using Tr = NodeTraits;
        Tr::SetPrev(sentinel, AddressOf(sentinel));
        Tr::SetNext(sentinel, AddressOf(sentinel));
    }
private:
    void DoMove(List& oth) noexcept {
        if (oth.Empty()) {
            Clear();
            return;
        }
        using Tr = NodeTraits;
        sentinel = oth.sentinel;
        Tr::SetPrev(*(Begin().ptr), AddressOf(sentinel));
        Tr::SetNext(*((--End()).ptr), AddressOf(sentinel));
    }

    NodeType sentinel;
};

}

#endif // LIST_H
