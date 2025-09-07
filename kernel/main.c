#include <stdint.h>
#include "print.h"

void kernel_main(void) {
    print_clear();
    print_set_color(PRINT_COLOR_CYAN, PRINT_COLOR_BLACK);
    print_at(5, 40, "DraftOS Kernel in long mode\n");

    while (1) {
        __asm__ volatile("hlt");
    }
}
