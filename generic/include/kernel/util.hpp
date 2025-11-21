#ifndef KERNEL_UTIL_HPP
#define KERNEL_UTIL_HPP

#include <type_traits>
#include <concepts>
#include <cstdint>
#include <new>
#include <utility>

namespace kernel {

template <class T>
struct ReplaceByVoid {
    using Type = void;
};

template <class T>
struct ReplaceByVoid<const T> {
    using Type = const void;
};

template <class T>
struct ReplaceByVoid<volatile T> {
    using Type = volatile void;
};

template <class T>
struct ReplaceByVoid<const volatile T> {
    using Type = const volatile void;
};

template <class T>
using ReplaceByVoidT = typename ReplaceByVoid<T>::Type;

template <class T>
concept PointerType = std::is_pointer_v<T>;

template <class A, class B>
concept SameCVAs = std::same_as<ReplaceByVoidT<A>, ReplaceByVoidT<B>>;

template <class A, class B>
concept CompatCVAs = requires (ReplaceByVoidT<A>* a) {
    static_cast<ReplaceByVoidT<B>*>(a);
};

template <PointerType T>
T ptr_cast(std::uintptr_t iptr)
{
    return static_cast<T>(reinterpret_cast<void*>(iptr));
}

template <std::same_as<std::uintptr_t> = std::uintptr_t, class T>
std::uintptr_t ptr_cast(T* iptr)
{
    return reinterpret_cast<std::uintptr_t>(static_cast<ReplaceByVoidT<T>*>(iptr));
}

template <PointerType D, CompatCVAs<std::remove_pointer_t<D>> S>
constexpr D ptr_cast(S* ptr)
{
    return reinterpret_cast<D>(ptr);
}

template <PointerType D, CompatCVAs<std::remove_pointer_t<D>> S>
D As(S* ptr)
{
    return std::launder(ptr_cast<D>(ptr));
}

template <typename T>
class ScopeExit {
	T final;
public:
	ScopeExit(const T& onFinal) : final(onFinal) {}
	ScopeExit(T&& onFinal) : final(std::move(onFinal)) {}
	~ScopeExit() noexcept(false) { final(); }
};

inline int popcount64(uint64_t v)
{
    v -= ((v >> 1) & 0x5555555555555555);
    v = (v & 0x3333333333333333) + ((v >> 2) & 0x3333333333333333);
    v = (v + (v >> 4)) & 0x0F0F0F0F0F0F0F0F;
    v = (v + (v >> 8));
    v = (v + (v >> 16));
    v = (v + (v >> 32));
    return v & 0x7f;
}

inline int clz64(uint64_t v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    return popcount64(~v);
}

inline int ctz64(uint64_t v)
{
    return popcount64(~v & (v - 1));
}

inline int Log2U64(uint64_t val)
{
    return clz64(val) ^ 63;
}

auto UToStr(char* str, std::size_t size, unsigned long long num, int radix) -> std::size_t;
auto UToStr(char* str, std::size_t size, unsigned long num, int radix) -> std::size_t;
auto UToStr(char* str, std::size_t size, unsigned num, int radix) -> std::size_t;

} // namespace kernel

#endif // KERNEL_UTIL_HPP
