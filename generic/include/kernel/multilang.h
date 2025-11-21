#ifndef MULTILANG_H
#define MULTILANG_H

#define KERNEL_STRUCT(name) \
typedef struct name name; \
    struct name

#define KERNEL_TYPEDEF_STRUCT(name) \
    typedef struct name name; \
    typedef struct name

#define KERNEL_UNION(name) \
    typedef union name name; \
    union name

#define KERNEL_UNION_STRUCT(name) \
    typedef union name name; \
    typedef union name

#endif // MULTILANG_H
