#include <cstdlib>

namespace kernel::tgtspec {

extern "C" typedef void cxa_atexit_func(void*);
extern "C" [[noreturn]] void kernel_EndlessLoop(void);

namespace {

struct cxa_atexit_entry {
    cxa_atexit_func* func;
    void *arg;
    void *dso;
};

struct cxa_atexit_array {
    size_t allocated;
    size_t used;
    cxa_atexit_entry* entries;
} atexit_array;

void call_entry(cxa_atexit_entry *entry)
{
    if (entry->arg != NULL) {
        (entry->func)(entry->arg);
    } else {
        ((void(*)(void))entry->func)();
    }
}

}

extern "C" void *__dso_handle;

extern "C" void _fini(void);

extern "C" int __cxa_atexit(cxa_atexit_func *func, void *arg, void *dso)
{
    if (atexit_array.entries == nullptr) {
        atexit_array.entries = (cxa_atexit_entry*)std::malloc(sizeof(cxa_atexit_entry[2]));
        if (atexit_array.entries == nullptr) {
            return 1;
        }
        atexit_array.allocated = 2;
        atexit_array.used = 0;
    }
    if (atexit_array.used == atexit_array.allocated) {
        size_t newAllocated = atexit_array.allocated + atexit_array.allocated / 2;
        cxa_atexit_entry *newHandlers;
        newHandlers = (cxa_atexit_entry*)std::realloc(atexit_array.entries, sizeof(cxa_atexit_entry) * newAllocated);
        if (newHandlers == NULL) {
            return 1;
        }
        atexit_array.entries = newHandlers;
    }
    atexit_array.entries[atexit_array.used++] = {
        .func = func,
        .arg = arg,
        .dso = dso,
    };
    return 0;
}

extern "C" void __cxa_finalize(void *dso)
{
    if (atexit_array.entries == NULL || atexit_array.used == 0) {
        return;
    }
    size_t current = atexit_array.used;
    if (dso == NULL) {
        while (current--) {
            cxa_atexit_entry *entry = atexit_array.entries + current;
            call_entry(entry);
        }
    } else {
        while (current--) {
            cxa_atexit_entry *entry = atexit_array.entries + current;
            if (entry->dso == dso) {
                call_entry(entry);
                entry->func = NULL;
            }
        }
        for (size_t i = 0; i < atexit_array.used; ++i) {
            cxa_atexit_entry *entry = atexit_array.entries + i;
            if (entry->func == NULL) {
                continue;
            }
            if (current != i) {
                atexit_array.entries[current] = *entry;
            }
            ++current;
        }
        atexit_array.used = current;
    }
}

extern "C" [[noreturn]] void _Exit(int exit_code)
{
    (void)exit_code;
    kernel_EndlessLoop();
}

extern "C" void exit(int exit_code)
{
    _fini();
    __cxa_finalize(0);
    _Exit(exit_code);
}

extern "C" void atexit(void(*func)(void)) noexcept
{
    __cxa_atexit((cxa_atexit_func*)func, NULL, __dso_handle);
}

extern "C" [[noreturn]] void abort(void)
{
    _Exit(1);
}

} // namespace kernel::tgtspec
