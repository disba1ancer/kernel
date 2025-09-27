#ifndef NODE_H
#define NODE_H

#include <concepts>
#include <new>

namespace kernel::intrusive {

template <typename Tag = void>
class BasicNode {};

template <typename NodeTypeArg, typename ItemType>
struct BaseClassCastPolicy {
    using NodeType = NodeTypeArg;
    static auto FromNode(NodeType* node) noexcept -> ItemType*
    {
        return static_cast<ItemType*>(node);
    }
    static auto ToNode(ItemType* item) noexcept -> NodeType*
    {
        return static_cast<NodeType*>(item);
    }
};

namespace detail {

template <typename T>
concept StandardLayout = std::is_standard_layout_v<T>;

template <typename CP, typename T>
concept CastPolicy =
requires { typename CP::NodeType; } &&
requires(T* t, typename CP::NodeType* n) {
    { CP::ToNode(t) } noexcept -> std::same_as<typename CP::NodeType*>;
    { CP::FromNode(n) } noexcept -> std::same_as<T*>;
};

template <typename T, detail::CastPolicy<T> CP>
class ContainerNodeRequirments {};

template <typename T>
T* Deref(T* t) noexcept {
    return t;
}

template <typename T>
auto Deref(T&& t) -> decltype((Deref)(t.operator->()))
{
    return (Deref)(t.operator->());
}

template <typename T, typename ValueType>
struct BasicIterator {
    friend T operator++(BasicIterator& iter, int) noexcept
    {
        auto self = iter.Get();
        auto t = *self;
        ++*self;
        return t;
    }

    friend T operator--(BasicIterator& iter, int) noexcept
    requires(requires(T t) { --t; })
    {
        auto self = iter.Get();
        auto t = *self;
        --*self;
        return t;
    }

    auto operator*() const noexcept
        -> ValueType&
    {
        return *((detail::Deref)(*Get()));
    }
private:
    T* Get() noexcept
    {
        return static_cast<T*>(this);
    }
    auto Get() const noexcept -> const T*
    {
        return static_cast<const T*>(this);
    }
};

}

template <typename NodeTypeArg, typename ItemType, std::size_t offset>
struct OffsetCastPolicy {
    using NodeType = NodeTypeArg;
    using byte = unsigned char;
    static constexpr
    auto FromNode(NodeType* ref) noexcept -> ItemType*
    {
        return std::launder(ptr_cast<ItemType*>(ptr_cast<byte*>(ref) - offset));
    }

    static constexpr
    auto ToNode(ItemType* ref) noexcept -> NodeType*
    {
        return std::launder(ptr_cast<NodeType*>(ptr_cast<byte*>(ref) + offset));
    }
};

template <typename NodeTypeArg>
struct IdentityCastPolicy {
    using NodeType = NodeTypeArg;
    static auto FromNode(NodeType* node) noexcept -> NodeType*
    {
        return node;
    }
    static auto ToNode(NodeType* item) noexcept -> NodeType*
    {
        return item;
    }
};

} // container_test::intrusive

#endif // NODE_H
