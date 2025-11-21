#include <cstring>

namespace kernel {

namespace {

void PutForward(char* str, std::size_t size, int ch, std::size_t& off)
{
    char* end = str + size;
    ++off;
    if (off <= size) {
        *(end - off) = (char)ch;
    }
}

void PutNumForward(char* str, std::size_t size, int ch, std::size_t& off)
{
    PutForward(str, size, ch < 10 ? ch + '0' : ch + 'A' - 10, off);
}

void PutZero(char* str, std::size_t size, std::size_t& off)
{
    PutForward(str, size, '0', off);
}

template <class T>
static void UToStr8(char* str, std::size_t size, T&& num, std::size_t& off) {
    while (num) {
        PutNumForward(str, size, num & 0x7, off);
        num = num >> 3;
    }
}

template <class T>
static void UToStr10(char* str, std::size_t size, T&& num, std::size_t& off) {
    while (num) {
        PutNumForward(str, size, num % 10, off);
        num = num / 10;
    }
}

template <class T>
static void UToStr16(char* str, std::size_t size, T&& num, std::size_t& off) {
    while (num) {
        PutNumForward(str, size, num & 0xF, off);
        num = num >> 4;
    }
}

template <class T>
static void UToStrGen(char* str, std::size_t size, T&& num, int radix, std::size_t& off) {
    while (num) {
        PutNumForward(str, size, (int)(num % (unsigned)radix), off);
        num = num / (unsigned)radix;
    }
}

template <class T>
auto UToStrT(char* str, std::size_t size, T&& num, int radix) -> std::size_t {
    std::size_t result = 0;
    PutForward(str, size, 0, result);
    if (num == 0) {
        PutZero(str, size, result);
    } else {
        switch (radix) {
        case 8:
            UToStr8(str, size, num, result);
            break;
        case 10:
            UToStr10(str, size, num, result);
            break;
        case 16:
            UToStr16(str, size, num, result);
            break;
        default:
            UToStrGen(str, size, num, radix, result);
            break;
        }
    }
    if (result <= size) {
        std::memmove(str, str + size - result, result);
    }
    return result - 1;
}

}

auto UToStr(char* str, std::size_t size, unsigned long long num, int radix) -> std::size_t {
    return UToStrT(str, size, num, radix);
}

auto UToStr(char* str, std::size_t size, unsigned long num, int radix) -> std::size_t {
    return UToStrT(str, size, num, radix);
}

auto UToStr(char* str, std::size_t size, unsigned num, int radix) -> std::size_t {
    return UToStrT(str, size, num, radix);
}

}
