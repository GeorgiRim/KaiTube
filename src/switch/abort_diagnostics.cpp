// SPDX-License-Identifier: GPL-3.0-or-later
// Capture explicit abort() callers without allocating or using stdio locks.
#include "newpipe/log_paths.h"
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#ifdef __SWITCH__
#include <switch.h>
#endif

extern "C" char __start__[];
extern "C" void __real_abort(void) __attribute__((noreturn));
static int abort_fd = -1;

extern "C" void kaitubeInitAbortLog(void)
{
    abort_fd = open(KAITUBE_CRASH_LOG, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (abort_fd >= 0) {
        static const char ready[] = "Explicit abort trace ready. Hardware faults are not captured here.\n";
        (void)write(abort_fd, ready, sizeof(ready) - 1);
    }
}

static void write_hex(const char* label, size_t len, uintptr_t value)
{
    char buffer[19];
    buffer[0] = '0'; buffer[1] = 'x';
    for (unsigned i = 0; i < 16; ++i)
        buffer[2 + i] = "0123456789abcdef"[(value >> ((15 - i) * 4)) & 15];
    buffer[18] = '\n';
    (void)write(abort_fd, label, len);
    (void)write(abort_fd, buffer, sizeof(buffer));
}

extern "C" __attribute__((noreturn, noinline)) void __wrap_abort(void)
{
    if (abort_fd >= 0) {
        uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        uintptr_t base = reinterpret_cast<uintptr_t>(__start__);
#ifdef __SWITCH__
        // __start__ can be an absolute linker symbol (zero in the supplied log).
        // Query our executing text mapping instead of treating it as a pointer.
        if (!base) {
            MemoryInfo info{}; u32 page = 0;
            if (R_SUCCEEDED(svcQueryMemory(&info, &page,
                    reinterpret_cast<uintptr_t>(&__wrap_abort)))) base = info.addr;
        }
#endif
        write_hex("abort caller=", 13, caller);
        write_hex("module base=", 12, base);
        write_hex("caller offset=", 14, caller - base);
        close(abort_fd);
        abort_fd = -1;
    }
    __real_abort();
}
