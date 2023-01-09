#ifndef NODE_H
#define NODE_H

#include <concepts>
#include <new>
#include <kernel/util.hpp>

namespace kernel::intrusive {

template <typename Tag = void>
class BasicNode {};

template <typename Tag>
struct BaseClassCastPolicy {
    template <typename T, template<typename = void> typename NodeType>
        requires (std::derived_from<T, NodeType<Tag>>)
    struct Policy {
        static constexpr
        T* GetPtrFromHeader(NodeType<>* header) noexcept
        {
            return static_cast<T*>(static_cast<NodeType<Tag>*>(header));
        }

        static constexpr
        auto GetHeaderFromPtr(T* ptr) noexcept -> NodeType<>*
        {
            return static_cast<NodeType<Tag>*>(ptr);
        }
    };
};

namespace detail {

template <typename T>
concept StandardLayout = std::is_standard_layout_v<T>;

template <typename CP, typename T, template <typename = void> typename NodeType>
concept CastPolicy =
requires(NodeType<>* n, T* t, typename CP::template Policy<T, NodeType> p) {
    { decltype(p)::GetPtrFromHeader(n) } noexcept -> std::same_as<T*>;
    { decltype(p)::GetHeaderFromPtr(t) } noexcept -> std::same_as<NodeType<>*>;
};

template <template <typename = void> typename NodeType, typename T, detail::CastPolicy<T, NodeType> CP>
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
    T operator++(int) noexcept
    {
        auto self = Get();
        auto t = *self;
        ++*self;
        return t;
    }

    T operator--(int) noexcept
    {
        auto self = Get();
        auto t = *self;
        --*self;
        return t;
    }

    auto operator*() const noexcept
        -> ValueType&
    {
        return *((detail::Deref)(*Get()));
    }

    bool operator==(const BasicIterator& other) const noexcept
    {
        return !(*Get() != other.Get());
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

template <std::size_t offset>
struct OffsetCastPolicy {
    template <detail::StandardLayout T, template<typename = void> typename NodeType>
    struct Policy {
        using byte = unsigned char;
        static constexpr
        T* GetPtrFromHeader(NodeType<>* ptr) noexcept
        {
            return As<T*>(ptr_cast<byte*>(ptr) - offset);
        }

        static constexpr
        auto GetHeaderFromPtr(T* ptr) noexcept -> NodeType<>*
        {
            return As<NodeType<>*>(ptr_cast<byte*>(ptr) + offset);
        }
    };
};

} // kernel::intrusive

#endif // NODE_H
