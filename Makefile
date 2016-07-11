CFLAGS=-Wall -Werror

CC=gcc $(CFLAGS) -c
LD=gcc
AS=gcc -nostdlib
NASM=nasm -f elf64
NASM_LD=ld -s

BUILD=build

$(BUILD)/%.o: %.c
	mkdir -p $(BUILD)
	$(CC) $< -o $@

$(BUILD)/%.o: %.asm
	mkdir -p $(BUILD)
	$(NASM) $< -o $@

EXE = repro parrot asmparrot asmparrot2
EXE := $(addprefix $(BUILD)/, $(EXE))

repro_OBJS += runner.o
repro_OBJS := $(addprefix $(BUILD)/, $(repro_OBJS))

parrot_OBJS += parrot.o
parrot_OBJS := $(addprefix $(BUILD)/, $(parrot_OBJS))

asmparrot_ASM += base.s

asmparrot2_OBJS += base.o
asmparrot2_OBJS:= $(addprefix $(BUILD)/, $(asmparrot2_OBJS))

all: $(EXE)
	@true

$(BUILD)/repro: $(repro_OBJS)
	$(LD) $< -o $@

$(BUILD)/parrot: $(parrot_OBJS)
	$(LD) $< -o $@

$(BUILD)/asmparrot: $(asmparrot_ASM)
	$(AS) $< -o $@

$(BUILD)/asmparrot2: $(asmparrot2_OBJS)
	$(NASM_LD) -o $@ $<

clean:
	rm -rf $(BUILD)

