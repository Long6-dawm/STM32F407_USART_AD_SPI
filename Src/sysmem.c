#include <errno.h>
#include <stdint.h>

extern uint8_t _end;
extern uint8_t _estack;
extern uint32_t _Min_Stack_Size;

static uint8_t *__sbrk_heap_end = NULL;

void *_sbrk(ptrdiff_t incr)
{
    uint8_t *prev_heap_end;

    if (__sbrk_heap_end == NULL)
    {
        __sbrk_heap_end = &_end;
    }

    prev_heap_end = __sbrk_heap_end;

    if (__sbrk_heap_end + incr > (uint8_t *)&_estack - (uint32_t)&_Min_Stack_Size)
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    __sbrk_heap_end += incr;

    return (void *)prev_heap_end;
}
