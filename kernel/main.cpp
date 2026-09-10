#include <cstdlib>
#include <cstring>

//using namespace std;

int kmain()
{
    //cout << "Hello World!" << endl;
    void* mem = std::malloc(0x400000);
    std::memset(mem, 0x55, 0x400000);
    std::free(mem);
    while (1) {
        asm volatile ("hlt");
    }
    return 0;
}
