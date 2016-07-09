#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char *LOG_NAME = "runner";

#define err(f,...) printf("\033[31;1me\033[0m %s: "f"\n", LOG_NAME, ##__VA_ARGS__)
#define info(f,...) printf("\033[32mi\033[0m %s: "f"\n", LOG_NAME, ##__VA_ARGS__)

void hd(void *data, size_t len) {
	int i;
	for(i = 0; i < len; i++) {
		if(i%16 == 0) printf("%08x  ", i);
		printf("%02x ", ((unsigned char*)data)[i]);
		if(i%16 == 15) printf("\n");
		else if(i%8 == 7) printf(" ");
	}
	if(i%16 != 0) printf("\n");
}

#define SHMSZ (10 * 1024)
int main() {

	info("over");
	exit(0);
}
