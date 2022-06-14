#ifndef _ADAPTER_H_
#define _ADAPTER_H_

#include <printf.h>

struct Log
{
    template <typename... ARGS>
    static void Debug(ARGS&&... args)
    {
        printf(args...);
    }

    template <typename... ARGS>
    static void Warn(ARGS&&... args)
    {
        printf(args...);
    }
};

#endif  //_ADAPTER_H_
