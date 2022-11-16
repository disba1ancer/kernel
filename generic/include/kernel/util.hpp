#ifndef KERNEL_UTIL_HPP
#define KERNEL_UTIL_HPP

#include <type_traits>

namespace kernel {

template <typename T>
std::remove_reference_t<T>&& Move(T&& in)
{
    return static_cast<std::remove_reference_t<T>&&>(in);
}

} // namespace boot

#endif // KERNEL_UTIL_HPP
