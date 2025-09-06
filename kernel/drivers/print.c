#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static size_t row = 0;
static size_t col = 0;
static uint8_t color = 0x0F; // white on black

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

void print_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    row = 0;
    col = 0;
}

static void scroll(void) {
    // Move each row up by one
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA[(y - 1) * VGA_WIDTH + x] = VGA[y * VGA_WIDTH + x];
        }
    }
    // Clear the last line
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    }
    if (row > 0) row--;
}

void print_char(char c) {
    if (c == '\n') {
        col = 0;
        if (++row == VGA_HEIGHT) {
            scroll();
        }
        return;
    }
    VGA[row * VGA_WIDTH + col] = vga_entry(c, color);
    if (++col == VGA_WIDTH) {
        col = 0;
        if (++row == VGA_HEIGHT) {
            scroll();
        }
    }
}

void print_str(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

void print_set_color(uint8_t fg, uint8_t bg) {
    color = fg | bg << 4;
}

// Convert integer to string and print it
void print_int(int num) {
    char buf[16];
    int i = 0;

    if (num == 0) {
        print_char('0');
        return;
    }

    if (num < 0) {
        print_char('-');
        num = -num;
    }

    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    // print in reverse
    while (i--) {
        print_char(buf[i]);
    }
}
