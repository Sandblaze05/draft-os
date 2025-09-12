#include <stdint.h>
#include "print.h"

extern uint64_t multiboot_info_addr;

struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;  // reserved, must be zero
} __attribute__((packed));

struct multiboot2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot2_mmap_entry entries[];
} __attribute__((packed));

static int is_valid_memory_range(uint64_t addr, uint64_t size) {
    if (addr == 0) return 0;                   // NULL pointer
    if (size == 0) return 0;                   // Zero size
    if (size > 0x10000000) return 0;           // Suspiciously large (256MB+)
    if (addr + size < addr) return 0;          // Overflow
    return 1;
}

void parse_memory_map(uint64_t multiboot_info_addr) {
    if (!is_valid_memory_range(multiboot_info_addr, 16)) {
        print_str("Invalid multiboot info address\n");
        return;
    }
    
    uint8_t* addr = (uint8_t*)multiboot_info_addr;
    
    print_str("Multiboot info at: ");
    print_hex(multiboot_info_addr);
    print_str("\n");
    
    // read total size (first 4 bytes)
    uint32_t total_size = *(uint32_t*)addr;
    print_str("Total multiboot info size: ");
    print_int(total_size);
    print_str("\n");
    
    // sanity check the total size
    if (total_size < 8 || total_size > 0x10000) {  // 64KB max seems reasonable
        print_str("Invalid multiboot info size\n");
        return;
    }
    
    // skip first 8 bytes (total_size + reserved)
    struct multiboot2_tag* tag = (struct multiboot2_tag*)(addr + 8);
    uint8_t* end_addr = addr + total_size;
    
    while ((uint8_t*)tag < end_addr) {
        // bounds check for tag header
        if ((uint8_t*)tag + sizeof(struct multiboot2_tag) > end_addr) {
            print_str("Tag header extends beyond multiboot info\n");
            break;
        }
        
        print_str("Tag type: ");
        print_int(tag->type);
        print_str(", size: ");
        print_int(tag->size);
        print_str("\n");
        
        // check for end tag
        if (tag->type == 0) {
            print_str("End tag found\n");
            break;
        }
        
        // sanity check tag size
        if (tag->size < 8 || tag->size > total_size) {
            print_str("Invalid tag size\n");
            break;
        }
        
        // bounds check for full tag
        if ((uint8_t*)tag + tag->size > end_addr) {
            print_str("Tag extends beyond multiboot info\n");
            break;
        }
        
        if (tag->type == 6) {  // memory map tag
            print_str("Found memory map tag\n");
            
            struct multiboot2_tag_mmap* mmap_tag = (struct multiboot2_tag_mmap*)tag;
            
            // verify we have enough space for the mmap header
            if (tag->size < sizeof(struct multiboot2_tag_mmap)) {
                print_str("Memory map tag too small\n");
                goto next_tag;
            }
            
            print_str("Entry size: ");
            print_int(mmap_tag->entry_size);
            print_str(", Entry version: ");
            print_int(mmap_tag->entry_version);
            print_str("\n");
            
            // calculate number of entries safely
            uint32_t entries_data_size = mmap_tag->size - sizeof(struct multiboot2_tag_mmap);
            
            if (mmap_tag->entry_size == 0) {
                print_str("Invalid entry size (0)\n");
                goto next_tag;
            }
            
            uint32_t entries = entries_data_size / mmap_tag->entry_size;
            
            print_str("Number of memory map entries: ");
            print_int(entries);
            print_str("\n");
            
            // Limit entries to prevent runaway loops
            if (entries > 100) {
                print_str("Too many entries, limiting to 100\n");
                entries = 100;
            }
            
            for (uint32_t i = 0; i < entries; i++) {
                uint8_t* entry_ptr = (uint8_t*)mmap_tag->entries + i * mmap_tag->entry_size;
                
                // Bounds check
                if (entry_ptr + sizeof(struct multiboot2_mmap_entry) > end_addr) {
                    print_str("Entry extends beyond multiboot info\n");
                    break;
                }
                
                struct multiboot2_mmap_entry* entry = (struct multiboot2_mmap_entry*)entry_ptr;
                
                print_str("Memory region ");
                print_int(i);
                print_str(": ");
                print_hex(entry->addr);
                print_str(" - ");
                print_hex(entry->addr + entry->len - 1);
                print_str(" (");
                print_hex(entry->len);
                print_str(" bytes) Type: ");
                print_int(entry->type);
                print_str("\n");
            }
        }
        
        next_tag:
        // Move to next tag with 8-byte alignment
        uint32_t aligned_size = (tag->size + 7) & ~7;
        tag = (struct multiboot2_tag*)((uint8_t*)tag + aligned_size);
    }
}




void kernel_main(void) {
    print_str("Kernel started\n");
    print_str("Multiboot info from global: 0x");
    print_hex(multiboot_info_addr);
    print_str("\n");
    
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