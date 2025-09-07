#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint16_t* const VGA = (uint16_t*)0xB8000;
static size_t row = 0;
static size_t col = 0;
static uint8_t color = 0x0F; // white on black default

#define LOG_SIZE 4096
static char log_buffer[LOG_SIZE];
static size_t log_index = 0;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

/* tells compiler to emit outb instruction, which writes the value in al reg to the specified
   port; "a"(val) constraint loads val into the al reg and "Nd"(port) places the port number in dx */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor(size_t r, size_t c) {
    uint16_t pos = r * VGA_WIDTH + c; // Calculate the cursor position
    outb(0x3D4, 0x0F);                // Tell VGA we are setting the low cursor byte
    outb(0x3D5, (uint8_t)(pos & 0xFF)); // Send the low byte of the cursor position
    outb(0x3D4, 0x0E);               // Tell VGA we are setting the high cursor byte
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF)); // Send the high byte of the cursor position
}

void print_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    row = 0;
    col = 0;
    update_cursor(row, col);
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
    update_cursor(row, col);
}

void print_char(char c) {
    log_buffer[log_index++ % LOG_SIZE] = c;

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
    update_cursor(row, col);
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

void print_hex(uint64_t num) {
    print_str("0x");
    char hex[16];
    int i = 0;

    if (num == 0) {
        print_char('0');
        return;
    }
    while (num > 0 && i < 16) {
        uint8_t digit = num & 0xF;
        hex[i++] = (digit < 10) ? ('0' + digit) : ('A' + (digit - 10));
        num >>= 4;
    }
    while (i--) {
        print_char(hex[i]);
    }
}

void print_at(size_t r, size_t c, const char* str) {
    size_t saved_row = row;
    size_t saved_col = col;

    row = r;
    col = c;
    print_str(str);
    row = saved_row;
    col = saved_col;

    update_cursor(row, col);
}

void kprintf(const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (size_t i=0;fmt[i] != '\0';i++) {
        if (fmt[i] == '%') {
            i++;
            switch (fmt[i]) {
                case 'c': {
                    char c = (char)__builtin_va_arg(args, int);
                    print_char(c);
                    break;
                }
                case 's': {
                    char* s = __builtin_va_arg(args, char*);
                    print_str(s);
                    break;
                }
                case 'd': {
                    int d = __builtin_va_arg(args, int);
                    print_int(d);
                    break;
                }
                case 'x': {
                    uint64_t x = __builtin_va_arg(args, uint64_t);
                    print_hex(x);
                    break;
                }
                case '%': {
                    print_char('%');
                    break;
                }
                default:
                    print_char('%');
                    print_char(fmt[i]);
                    break;
            }
        }
        else {
            print_char(fmt[i]);
        }
    }
}
