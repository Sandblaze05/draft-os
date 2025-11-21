#include <stdint.h>
#include "panic.h"
#include "print.h"

void kernel_panic(const char* msg) {
    __asm__ volatile("cli");

    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_LIGHT_RED);
    print_clear();

    print_str("\n\n");
    print_str("*****KERNEL PANIC an oopsie was made regretfully*****\n\n");
    print_str(msg);
    print_str("\n\n");
    print_str("System halted.\n");

    while (1) {
        __asm__ volatile("hlt");
    }

    __builtin_unreachable();
}