#include <stdint.h>
#include <stddef.h>
#include "print.h"

#define PAGE_SIZE 4096
#define MULTIBOOT_MEMORY_AVAILABLE 1

#define BIT_SET(bitmap, bit)   ( (bitmap)[(bit) >> 3] |=  (1 << ((bit) & 7)) )
#define BIT_CLEAR(bitmap, bit) ( (bitmap)[(bit) >> 3] &= ~(1 << ((bit) & 7)) )
#define BIT_TEST(bitmap, bit)  ( (bitmap)[(bit) >> 3] &   (1 << ((bit) & 7)) )

extern uint64_t multiboot_info_addr;

static uint8_t* pmm_bitmap = NULL;
static size_t   pmm_bitmap_size = 0;   /* bytes */
static size_t   pmm_total_pages = 0;

struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct multiboot2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct multiboot2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot2_mmap_entry entries[];
} __attribute__((packed));


void parse_memory_map(uint64_t multiboot_info_addr) {
    uint8_t* base = (uint8_t*) (uintptr_t) multiboot_info_addr;

    if (!base) {
        print_str("MBI addr null\n");
        return;
    }

    uint32_t total_size = *(uint32_t*)base;
    kprintf("Multiboot info at: 0x%x\n", multiboot_info_addr);
    kprintf("Total multiboot info size: %d\n", total_size);
    if (total_size < 8 || total_size > 0x20000) {
        print_str("Suspicious multiboot total size\n");
        return;
    }

    struct multiboot2_tag* tag = (struct multiboot2_tag*)(base + 8);
    uint8_t* end_addr = base + total_size;

    while ((uint8_t*)tag < end_addr) {
        if ((uint8_t*)tag + sizeof(*tag) > end_addr) {
            print_str("Tag header extends beyond MBI\n");
            break;
        }

        kprintf("Tag type: %d, size: %d\n", tag->type, tag->size);

        if (tag->type == 0) { /* end */
            print_str("End tag\n");
            break;
        }

        if (tag->size < 8 || (uint8_t*)tag + tag->size > end_addr) {
            print_str("Invalid tag size\n");
            break;
        }

        if (tag->type == 6) { // memory map
            struct multiboot2_tag_mmap* mmap_tag = (struct multiboot2_tag_mmap*)tag;

            if (mmap_tag->size < sizeof(*mmap_tag)) {
                print_str("MMAP tag too small\n");
                goto next_tag;
            }

            kprintf("Entry size: %d, Entry version: %d\n", mmap_tag->entry_size, mmap_tag->entry_version);

            uint32_t entries_data_bytes = mmap_tag->size - sizeof(*mmap_tag);
            if (mmap_tag->entry_size == 0) {
                print_str("MMAP entry_size == 0\n");
                goto next_tag;
            }
            uint32_t entries = entries_data_bytes / mmap_tag->entry_size;
            kprintf("Number of mmap entries: %d\n", entries);

            if (entries > 1000) {
                kprintf("Clamping entries from %d to 1000\n", entries);
                entries = 1000;
            }

            for (uint32_t i = 0; i < entries; i++) {
                uint8_t* e_ptr = (uint8_t*)mmap_tag->entries + (size_t)i * mmap_tag->entry_size;
                if (e_ptr + sizeof(struct multiboot2_mmap_entry) > end_addr) {
                    print_str("MMAP entry out of bounds\n");
                    break;
                }
                struct multiboot2_mmap_entry* e = (struct multiboot2_mmap_entry*)e_ptr;
                kprintf("Region %d: 0x%x - 0x%x (%x bytes) Type: %d\n",
                        i, e->addr, e->addr + e->len - 1, e->len, e->type);
            }

            pmm_init(mmap_tag);
        }

    next_tag:
        // align tags to 8 bytes
        uint32_t aligned = (tag->size + 7) & ~7U;
        tag = (struct multiboot2_tag*)((uint8_t*)tag + aligned);
    }
}


void pmm_init(struct multiboot2_tag_mmap* mmap_tag) {
    // calc mem
    uint64_t max_addr = 0;
    uint32_t entries = (mmap_tag->size - sizeof(*mmap_tag)) / mmap_tag->entry_size;
    for (uint32_t i = 0; i < entries; i++) {
        struct multiboot2_mmap_entry* e = (void*)((uint8_t*)mmap_tag->entries + (size_t)i * mmap_tag->entry_size);
        uint64_t top = e->addr + e->len;
        if (top > max_addr) max_addr = top;
    }

    pmm_total_pages = (max_addr + PAGE_SIZE - 1) / PAGE_SIZE;   // round up total pages
    pmm_bitmap_size = (pmm_total_pages + 7) / 8;                // round up bytes

    extern uint8_t _kernel_start;
    extern uint8_t _kernel_end;

    // place bitmap right after kernel_end page-aligned
    uint64_t bitmap_addr = ((uint64_t)&_kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    pmm_bitmap = (uint8_t*)(uintptr_t)bitmap_addr;

    // mark all as used
    for (size_t i = 0; i < pmm_bitmap_size; i++) pmm_bitmap[i] = 0xFF;

    // clear bits for free regions
    for (uint32_t i = 0; i < entries; i++) {
        struct multiboot2_mmap_entry* e = (void*)((uint8_t*)mmap_tag->entries + (size_t)i * mmap_tag->entry_size);
        if (e->type != MULTIBOOT_MEMORY_AVAILABLE) continue;

        uint64_t start_page = e->addr / PAGE_SIZE;
        uint64_t end_page   = (e->addr + e->len + PAGE_SIZE - 1) / PAGE_SIZE; 

        if (end_page > pmm_total_pages) end_page = pmm_total_pages;

        for (uint64_t p = start_page; p < end_page; p++) {
            BIT_CLEAR(pmm_bitmap, p);
        }
    }

    // reserve kernel pages and bitmap
    uint64_t kernel_start_page = ((uint64_t)&_kernel_start) / PAGE_SIZE;
    uint64_t kernel_end_page   = (((uint64_t)&_kernel_end) + PAGE_SIZE - 1) / PAGE_SIZE;

    // kernel reserved
    for (uint64_t p = kernel_start_page; p < kernel_end_page; p++) BIT_SET(pmm_bitmap, p);

    // reserved bitmap area
    uint64_t bmp_start_page = bitmap_addr / PAGE_SIZE;
    uint64_t bmp_end_page   = (bitmap_addr + pmm_bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t p = bmp_start_page; p < bmp_end_page; p++) BIT_SET(pmm_bitmap, p);

    // reserve 0 - 1MB
    for (uint64_t p = 0; p < (0x100000 / PAGE_SIZE); p++) BIT_SET(pmm_bitmap, p);

    kprintf("PMM: total_pages=%d, bitmap_bytes=%d, bitmap_addr=0x%x\n",
            pmm_total_pages, pmm_bitmap_size, (uint64_t)bitmap_addr);
}

void* pmm_alloc_page(void) {
    for (size_t page = 0; page < pmm_total_pages; page++) {
        if (!BIT_TEST(pmm_bitmap, page)) {
            BIT_SET(pmm_bitmap, page);
            return (void*)(page * PAGE_SIZE);
        }
    }
    return NULL; // out of mem -_-
}


void pmm_free_page(void* addr) {
    size_t page = (uint64_t) addr / PAGE_SIZE;
    BIT_CLEAR(pmm_bitmap, page);
}

void kernel_main(void) {
    print_str("Kernel started\n");
    
    if (multiboot_info_addr != 0) {
        parse_memory_map(multiboot_info_addr);
    } 
    else {
        print_str("No multiboot info available\n");
    }

    while (1) {
        __asm__ volatile("hlt");
    }
}