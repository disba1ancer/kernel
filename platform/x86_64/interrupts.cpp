#include "interrupts.h"
#include "processor.h"

namespace kernel::tgtspec {

void UniversalExceptionHandler(int interrupt_index, ExceptionFrame* stackframe);
void UniversalInterruptHandler(int interrupt_index, InterruptFrame* stackframe);
static void IRQHandler(int irqN, InterruptFrame* stackframe);
extern "C" void kernel_x86_64_KeyboardIRQ(void);

extern "C" void kernel_x86_64_SystemInterruptHandler(int interrupt_index,
    void* stackframeptr)
{
    switch (interrupt_index) {
    case i686::Interrupt_DF:
    case i686::Interrupt_TS:
    case i686::Interrupt_NP:
    case i686::Interrupt_SS:
    case i686::Interrupt_GP:
    case i686::Interrupt_PF:
    case i686::Interrupt_AC:
        UniversalExceptionHandler(interrupt_index, static_cast<ExceptionFrame*>(stackframeptr));
        break;
    default:
        UniversalInterruptHandler(interrupt_index, static_cast<InterruptFrame*>(stackframeptr));
        break;
    }
}

void UniversalExceptionHandler(int interrupt_index, ExceptionFrame* stackframe)
{
    stackframe->local_rsp += 8;
    while (1);
}

void UniversalInterruptHandler(int interrupt_index, InterruptFrame* stackframe)
{
    if (interrupt_index < 0x40) return;
    if (interrupt_index >= 0x50) return;
    IRQHandler(interrupt_index - 0x40, stackframe);
}

void IRQHandler(int irqN, InterruptFrame* stackframe)
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
