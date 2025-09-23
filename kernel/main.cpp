#include <cstdlib>
#include <cstring>

//using namespace std;

int kmain()
{
    //cout << "Hello World!" << endl;
    void* mem = std::malloc(0xA00000);
    std::memset(mem, 0x55, 0xA00000);
    while (1) {
        asm volatile ("hlt");
    }
    return 0;
}
