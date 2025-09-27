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

} // namespace kernel

#endif // KERNEL_UTIL_HPP
