CFLAGS=-Wall -Werror

CC=gcc $(CFLAGS) -c
LD=gcc
AS=gcc -nostdlib

BUILD=build

$(BUILD)/%.o: %.c
	mkdir -p $(BUILD)
	$(CC) $< -o $@

EXE = repro parrot asmparrot
EXE := $(addprefix $(BUILD)/, $(EXE))

repro_OBJS += runner.o
repro_OBJS := $(addprefix $(BUILD)/, $(repro_OBJS))

parrot_OBJS += parrot.o
parrot_OBJS := $(addprefix $(BUILD)/, $(parrot_OBJS))

asmparrot_ASM += base.s

all: $(EXE)
	@true

$(BUILD)/repro: $(repro_OBJS)
	$(LD) $< -o $@

$(BUILD)/parrot: $(parrot_OBJS)
	$(LD) $< -o $@

$(BUILD)/asmparrot: $(asmparrot_ASM)
	$(AS) $< -o $@

clean:
	rm -rf $(BUILD)

