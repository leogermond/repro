#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <signal.h>

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

struct timespec timespec_sub(struct timespec a, struct timespec b) {
	struct timespec result;
	result.tv_sec = a.tv_sec - b.tv_sec;
	if(a.tv_nsec < b.tv_nsec) {
		result.tv_nsec = a.tv_nsec + (1000 * 1000 * 1000 - b.tv_nsec);
		result.tv_sec -= 1;
	} else {
		result.tv_nsec = a.tv_nsec - b.tv_nsec;
	}

	return result;
}

int send_program(int fd, const char *pgname) {
	int ret;
	FILE *fpgm = fopen(pgname, "rb");
	if(!fpgm) {
		ret = ENOENT;
		goto out;
	}
	fseek(fpgm, 0L, SEEK_END);
	size_t pgmsz = ftell(fpgm);
	fseek(fpgm, 0L, SEEK_SET);
	info("%s %lu bytes", pgname, pgmsz);
	unsigned char *pgmsz_b = (void*)&pgmsz;
	size_t pgmsz_nbo = pgmsz_b[3] + (pgmsz_b[2]<<8) + (pgmsz_b[1]<<16) + (pgmsz_b[0]<<24);
	write(fd, &pgmsz_nbo, 4);
	unsigned char pgm[1024];
	size_t read = 0;
	while(read < pgmsz) {
		size_t curread = fread(&pgm, sizeof(pgm[0]), sizeof(pgm), fpgm);
		read += curread;
		write(fd, pgm, curread);
	}
	fclose(fpgm);
	ret = 0;
out:
	return ret;
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
	struct timespec ts_start;
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	if(!pid) {
		LOG_NAME = "child";
		close(fd_send_prog[1]);
		close(fd_recv_prog[0]);
		int fd_prog[] = { fd_send_prog[0], dup(fd_recv_prog[1]) };
		close(fd_recv_prog[1]);
		info("-> %d <- %d", fd_prog[0], fd_prog[1]);
		char *args[] = {NULL}, *env[] = {NULL};
		execve("build/asmparrot", args, env);

		info("failed exec");
		exit(0);
	}

	close(fd_send_prog[0]);
	close(fd_recv_prog[1]);
	send_program(fd_send_prog[1], "build/asmparrot");

#define TIMEOUT_MS 10
	int end = 0;
	struct timespec ts_cur, ts_last_read = ts_start;
	int received = 0, len_prog = 0;
	FILE *fchild = fopen("out/child", "wb");
	while(!end) {
		clock_gettime(CLOCK_MONOTONIC, &ts_cur);
		struct timespec ts_elapsed = timespec_sub(ts_cur, ts_last_read);
		if(ts_elapsed.tv_nsec > TIMEOUT_MS * 1000 * 1000) {
			err("more than %dms without any new data: killing child\n", TIMEOUT_MS);
			kill(9, pid);
			break;
		}
		unsigned char c;
		int ret = read(fd_recv_prog[0], &c, sizeof(c));
		if(ret != 0 && ret != 1) {
			err("could not read: %s\n", strerror(errno));
			end = 1;
			break;
		}

		if(ret > 0) {
			received += ret;
			clock_gettime(CLOCK_MONOTONIC, &ts_last_read);
			if(received <= 4) {
				len_prog += c<<(8*(4-received));
			} else {
				fwrite(&c, sizeof(char), ret, fchild);
			}
		}
	}
	fclose(fchild);
	info("written %d bytes\n", len_prog);

	clock_gettime(CLOCK_MONOTONIC, &ts_cur);
	struct timespec ts_elapsed = timespec_sub(ts_cur, ts_start);
	info("exit: %ld.%09lds", ts_elapsed.tv_sec, ts_elapsed.tv_nsec);
	exit(0);
}
