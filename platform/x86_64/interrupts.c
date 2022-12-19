#include "interrupts.h"
#include "processor.h"

void kernel_x86_64_GPExceptionHandler(kernel_x86_64_ExceptionFrame* stackframe);
void kernel_x86_64_UniversalInterruptHandler(int interrupt_index,
    kernel_x86_64_InterruptFrame* stackframe);
static void IRQHandler(int irqN, kernel_x86_64_InterruptFrame* stackframe);
void kernel_x86_64_KeyboardIRQ(void);

void kernel_x86_64_SystemInterruptHandler(int interrupt_index,
    void* stackframe)
{
    switch (interrupt_index) {
        case i686_Interrupt_GP:
            kernel_x86_64_GPExceptionHandler(stackframe);
            break;
        default:
            kernel_x86_64_UniversalInterruptHandler(interrupt_index, stackframe);
            break;
    }
}

void kernel_x86_64_GPExceptionHandler(kernel_x86_64_ExceptionFrame* stackframe)
{
//    if (stackframe->error & 2) {
//        kernel_x86_64_UniversalInterruptHandler((stackframe->error & 0xFFFF) >> 3,
//            stackframe);
//    }
    stackframe->local_rsp += 8;
}

void kernel_x86_64_UniversalInterruptHandler(int interrupt_index,
    kernel_x86_64_InterruptFrame* stackframe)
{
    if (interrupt_index < 0x40) return;
    if (interrupt_index >= 0x50) return;
    IRQHandler(interrupt_index - 0x40, stackframe);
}

void IRQHandler(int irqN,
    kernel_x86_64_InterruptFrame* stackframe)
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
