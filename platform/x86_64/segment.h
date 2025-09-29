#ifndef SEGMENT_H
#define SEGMENT_H

#ifdef __cplusplus
#define EXTERN_C extern "C"
#define EXTERN extern "C"
#else
#define EXTERN_C
#define EXTERN extern
#endif

#include "processor.h"

EXTERN i686_Descriptor gdt[];

EXTERN_C void InitGDT(void);

#undef EXTERN_C

#endif // SEGMENT_H
