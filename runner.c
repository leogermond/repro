#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static const char *LOG_NAME = "runner";

#define err(f,...) printf("\033[31;1me\033[0m %s: "f"\n", LOG_NAME, ##__VA_ARGS__)
#define info(f,...) printf("\033[32mi\033[0m %s: "f"\n", LOG_NAME, ##__VA_ARGS__)

#define ASSERT(c,...) if(!(c)) err("assert "#c" failed " __VA_ARGS__)

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

int isanum(char c) {
	return c >= 0x20 && c < 0x7f;
}

int main() {
	int fd_send_prog[2], fd_recv_prog[2];
	int ret = pipe(fd_send_prog);
	if(ret < 0) {
		err("cannot create send pipe: %s\n", strerror(errno));
		exit(2);
	}

	ret = pipe(fd_recv_prog);
	if(ret < 0) {
		err("cannot create recv pipe: %s\n", strerror(errno));
		exit(2);
	}

	int pid = fork();
	if(!pid) {
		LOG_NAME = "child";
		close(fd_send_prog[1]);
		close(fd_recv_prog[0]);
		int fd_prog[] = { fd_send_prog[0], fd_recv_prog[1] };

		int len, i = 0;
		while(1) {
			char c;
			if(read(fd_prog[0], &c, sizeof(c)) != sizeof(c)) continue;
			i += 1;
			if(i <= 4) {
				len += c<<(4-i);
			} else {
				write(fd_prog[1], &c, sizeof(c));
			}
			if(i == len + 4) {
				break;
			}
		}

		info("exit");
		exit(0);
	}

	close(fd_send_prog[0]);
	close(fd_recv_prog[1]);
	write(fd_send_prog[1], "\0\0\0\6coucou", 10);

	while(1) {
		char c;
		int ret = read(fd_recv_prog[0], &c, sizeof(c));
		if(ret == 0) {
			break;
		} if(ret != 1) {
			err("could not read: %s\n", strerror(errno));
			break;
		}
		info("r: %02x '%c'", c, isanum(c)?c:'.');
	}

	info("exit");
	exit(0);
}