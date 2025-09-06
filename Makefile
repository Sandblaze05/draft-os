# Tools
CC = x86_64-elf-gcc
AS = nasm
LD = x86_64-elf-ld
ISO_DIR = iso

# Flags
CFLAGS = -ffreestanding -O2 -Wall -Wextra -mno-red-zone -mcmodel=kernel -Iinclude
ASFLAGS = -f elf64
LDFLAGS = -nostdlib -z max-page-size=0x1000 -T linker.ld

# Directories
BOOT_DIR = boot
KERNEL_DIR = kernel
INCLUDE_DIR = include
BUILD_DIR = build
ISO_FILE = $(BUILD_DIR)/kernel.iso
GRUB_CFG = $(ISO_DIR)/boot/grub/grub.cfg

# Recursively find all source files
ASM_SOURCES = $(shell find $(BOOT_DIR) -name "*.asm" 2>/dev/null)
C_SOURCES = $(shell find $(KERNEL_DIR) -name "*.c" 2>/dev/null) \
            $(shell find $(INCLUDE_DIR) -name "*.c" 2>/dev/null)

# Generate object files maintaining directory structure
ASM_OBJECTS = $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SOURCES))
C_OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SOURCES))

# All objects
OBJS = $(ASM_OBJECTS) $(C_OBJECTS)

# Final kernel
KERNEL = $(BUILD_DIR)/kernel.elf

# Default target
.PHONY: all clean run debug help

.DEFAULT_GOAL := all

all: $(KERNEL)

# Create build directories as needed
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Auto-create subdirectories for nested source files
$(BUILD_DIR)/%/:
	@mkdir -p $@

# Generic assembly compilation rule
$(BUILD_DIR)/%.o: %.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "Assembling $<..."
	@$(AS) $(ASFLAGS) $< -o $@

# Generic C compilation rule  
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Header dependency tracking
$(BUILD_DIR)/%.o: %.c $(wildcard $(INCLUDE_DIR)/*.h) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "Compiling $< (with headers)..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Link final kernel
$(KERNEL): $(OBJS) linker.ld | $(BUILD_DIR)
	@echo "Linking kernel..."
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "✓ Kernel built successfully: $@"

# Debug: Show what would be built
debug:
	@echo "ASM Sources: $(ASM_SOURCES)"
	@echo "C Sources: $(C_SOURCES)"
	@echo "ASM Objects: $(ASM_OBJECTS)"
	@echo "C Objects: $(C_OBJECTS)"
	@echo "All Objects: $(OBJS)"

# Run in QEMU
run: $(KERNEL)
	@echo "Starting QEMU..."
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio

# Run in QEMU with debugging
run-debug: $(KERNEL)
	@echo "Starting QEMU with GDB support..."
	qemu-system-x86_64 -kernel $(KERNEL) -serial stdio -s -S

# Create bootable ISO
iso: $(ISO_FILE)

$(ISO_FILE): $(KERNEL) | $(BUILD_DIR)
	@echo "Creating bootable ISO..."
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL) $(ISO_DIR)/boot/
	@echo "set timeout=0" > $(GRUB_CFG)
	@echo "set default=0" >> $(GRUB_CFG)
	@echo "" >> $(GRUB_CFG)
	@echo "menuentry \"My OS Kernel\" {" >> $(GRUB_CFG)
	@echo "    multiboot2 /boot/kernel.elf" >> $(GRUB_CFG)
	@echo "    boot" >> $(GRUB_CFG)
	@echo "}" >> $(GRUB_CFG)
	@grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)
	@echo "✓ ISO created: $(ISO_FILE)"

# Run ISO in QEMU
run-iso: $(ISO_FILE)
	@echo "Starting QEMU with ISO..."
	qemu-system-x86_64 -cdrom $(ISO_FILE) -serial stdio -m 512M

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)/* $(ISO_DIR)
	@echo ":D Clean complete"

# Full rebuild
rebuild: clean all

# Help target
help:
	@echo "Available targets:"
	@echo "  all       - Build kernel (default)"
	@echo "  iso       - Create bootable ISO"
	@echo "  run-iso   - Run ISO in QEMU"
	@echo "  clean     - Remove build artifacts"
	@echo "  rebuild   - Clean and build"
	@echo "  run       - Run kernel in QEMU"
	@echo "  run-debug - Run kernel in QEMU with GDB"
	@echo "  debug     - Show source/object file lists"
	@echo "  help      - Show this help"

# Include dependency files if they exist
-include $(C_OBJECTS:.o=.d)

# Generate dependency files for C sources
$(BUILD_DIR)/%.d: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -MM -MT $(BUILD_DIR)/$*.o $< > $@