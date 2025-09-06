#include "print.h"

void kernel_main(void) {
    print_clear();

    print_set_color(PRINT_COLOR_LIGHT_CYAN, PRINT_COLOR_BLACK);
    print_str("Draft OS kernel 64-bit mode\n");

    while (1) __asm__ volatile("hlt");
}


