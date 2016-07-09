CFLAGS=-Wall -Werror

CC=gcc $(CFLAGS) -c
LD=gcc

BUILD=build

$(BUILD)/%.o: %.c
	mkdir -p $(BUILD)
	$(CC) $< -o $@

EXE=repro parrot
EXE := $(addprefix $(BUILD)/, $(EXE))

repro_OBJS += runner.o
repro_OBJS := $(addprefix $(BUILD)/, $(repro_OBJS))

parrot_OBJS += parrot.o
parrot_OBJS := $(addprefix $(BUILD)/, $(parrot_OBJS))

all: $(EXE)
	@true

$(BUILD)/repro: $(repro_OBJS)
	$(LD) $< -o $@

$(BUILD)/parrot: $(parrot_OBJS)
	$(LD) $< -o $@

clean:
	rm -rf $(BUILD)

