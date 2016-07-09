CFLAGS=-Wall -Werror

CC=gcc $(CFLAGS) -c
LD=gcc

BUILD=build

$(BUILD)/%.o: %.c
	mkdir -p $(BUILD)
	$(CC) $< -o $@

EXE=$(BUILD)/repro

repro_OBJS += runner.o

OBJS += $(addprefix $(BUILD)/, $(repro_OBJS))

$(EXE): $(OBJS)
	$(LD) $(OBJS) -o $(EXE)

all: $(EXE)
	@true

clean:
	rm -rf $(BUILD)
