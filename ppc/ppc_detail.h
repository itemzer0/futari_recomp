#pragma once
#include "ppc_config.h"

// Indirect call validator — logs bad calls instead of crashing hard.
// Mirrors the pattern from daytona-xbla-recomp/ppc/ppc_detail.h.
// TODO: tune the import thunk logic once we know the import table layout.

#include <cstdio>

#undef __builtin_trap
#define __builtin_trap() \
    do { \
        static int _warn_count = 0; \
        if (_warn_count < 10) { \
            printf("WARNING: __builtin_trap at LR=0x%08X (count=%d)\n", \
                   (unsigned)ctx.lr.u32, ++_warn_count); \
        } \
    } while (0)

#define PPC_CALL_INDIRECT_FUNC(x) \
    do { \
        uint32_t _target = (x); \
        if (_target == 0) { ctx.r3.u32 = 0; break; } \
        if (_target < PPC_CODE_BASE || _target >= PPC_CODE_BASE + PPC_CODE_SIZE) { \
            static int _oob = 0; \
            if (_oob < 10) \
                printf("WARNING: indirect call out of range: 0x%08X (LR=0x%08X, count=%d)\n", \
                       _target, (unsigned)ctx.lr.u32, ++_oob); \
            ctx.r3.u32 = 0; break; \
        } \
        auto _fn = LookupFunction(_target); \
        if (!_fn) { \
            static int _miss = 0; \
            if (_miss < 10) \
                printf("WARNING: no function for 0x%08X (LR=0x%08X, count=%d)\n", \
                       _target, (unsigned)ctx.lr.u32, ++_miss); \
            ctx.r3.u32 = 0; break; \
        } \
        _fn(ctx, base_address); \
    } while (0)
