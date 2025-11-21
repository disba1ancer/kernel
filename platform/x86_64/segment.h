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

extern "C" i686::Descriptor gdt[];

#undef EXTERN_C

#endif // SEGMENT_H
