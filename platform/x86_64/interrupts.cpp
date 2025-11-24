#include "interrupts.h"
#include "kernel/util.hpp"
#include "processor.h"

namespace kernel::tgtspec {

static void IRQHandler(int irqN, PFFrame* stackframe);
extern "C" void kernel_x86_64_KeyboardIRQ(void);

extern "C" unsigned char magic_handler[];

int GetInterruptNumber(PFFrame* stackframeptr)
{
    if (!(stackframeptr->error & 16)) {
        return i686::Interrupt_PF;
    }
    if (stackframeptr->cs != 8) {
        return i686::Interrupt_PF;
    }
    auto intrbase = ptr_cast<std::uintptr_t>(magic_handler);
    auto diff = stackframeptr->rip - intrbase;
    if (diff >= 32) {
        return i686::Interrupt_PF;
    }
    auto secframe = ptr_cast<ErrorFrame*>(stackframeptr);
    stackframeptr->ss = secframe->ss;
    stackframeptr->rsp = secframe->rsp;
    stackframeptr->rflags = secframe->rflags;
    stackframeptr->cs = secframe->cs;
    stackframeptr->rip = secframe->rip;
    stackframeptr->error = secframe->error;
    if (diff != i686::Interrupt_GP) {
        return int(diff);
    }
    if (!(stackframeptr->error & 2)) {
        return i686::Interrupt_GP;
    }
    return int(stackframeptr->error) >> 3;
}

extern "C" void kernel_x86_64_PageFault(PFFrame* stackframeptr)
{
    int intNum = GetInterruptNumber(stackframeptr);
    if (intNum < 0x40) return;
    if (intNum >= 0x50) return;
    IRQHandler(intNum - 0x40, stackframeptr);
}

void IRQHandler(int irqN, PFFrame* stackframe)
{
    switch (irqN) {
    case 1:
        kernel_x86_64_KeyboardIRQ();
        break;
    case 7:
        if (kernel_x86_64_IsSpuriousIRQ(0)) {
            return;
        }
        break;
    case 15:
        if (kernel_x86_64_IsSpuriousIRQ(1)) {
            kernel_x86_64_SendEOI(0);
            return;
        }
        break;
    }

    kernel_x86_64_SendEOI(irqN >= 8);
}

} // namespace kernel::tgtspec
