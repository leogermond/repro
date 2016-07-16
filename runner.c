#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/wait.h>

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

#define UUID_SIZE 36

int uuid(char *buf) {
	FILE *f = fopen("/proc/sys/kernel/random/uuid", "r");
	ASSERT(f);
	ASSERT(fread(buf, sizeof(char), UUID_SIZE, f) == UUID_SIZE);
	fclose(f);
	return 0;
}

#define PAGE_SIZE 0x1000
#define PROG_SIZE (10 * PAGE_SIZE)

int main() {
	char shm_name[UUID_SIZE + 2];
	shm_name[0] = '/';
	uuid(shm_name + 1);
	shm_name[sizeof(shm_name) - sizeof(shm_name[0])] = '\0';
	int shmfd = shm_open(shm_name, O_RDWR | O_TRUNC | O_CREAT, 0777);
	info("shm %s => fd %d", shm_name, shmfd);
	ftruncate(shmfd, 4 * PROG_SIZE);

	ASSERT(shmfd != -1);
	int fd_sup_to_cell[2], fd_cell_to_sup[2];
	int ret = pipe(fd_cell_to_sup);
	if(ret < 0) {
		err("cannot create send pipe: %s\n", strerror(errno));
		exit(2);
	}
	ret = pipe(fd_sup_to_cell);
	if(ret < 0) {
		err("cannot create send pipe: %s\n", strerror(errno));
		exit(2);
	}

	int pid = fork();
	struct timespec ts_start;
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	if(!pid) {
		LOG_NAME = "cell";
		info("removing close-on-exec flag from shm fd");
		fcntl(shmfd, F_SETFD, 0);
		info("closing file descriptors and redirecting to pipe");
		close(fd_cell_to_sup[0]);
		close(fd_sup_to_cell[1]);
		dup2(fd_sup_to_cell[0], 0);
		close(fd_sup_to_cell[0]);
		info("radio silence");
		dup2(fd_cell_to_sup[1], 1);
		close(fd_cell_to_sup[1]);
		close(2);

		char *args[] = {NULL}, *env[] = {NULL};
		execve("build/cell", args, env);
		exit(1);
	}
	info("closing pipes");
	close(fd_sup_to_cell[0]);
	close(fd_cell_to_sup[1]);

	info("opening shm segments");
	void *shm_rd = mmap(NULL, PROG_SIZE, PROT_READ, MAP_SHARED, shmfd, PROG_SIZE);
	void *shm_wr = mmap(NULL, PROG_SIZE, PROT_WRITE, MAP_SHARED, shmfd, 0);
	info("read %p write %p", shm_rd, shm_wr);

	info("ping");
	write(fd_sup_to_cell[1], "p", 1);

	((unsigned char*)shm_wr)[0] = 0xc3;
	write(fd_sup_to_cell[1], "lq", 2);
	info("wait for answer");
	int status = -1, rdlen = -1;
	while(status == -1 || rdlen != 0) {
		if(status == -1) {
			waitpid(pid, &status, WNOHANG);
		}
		char c[128];
		rdlen = read(fd_cell_to_sup[0], c, sizeof(c));
		if(rdlen > 0) {
			printf("%.*s", rdlen, c);
		}
	}
	
	info("exit, child = %d (status = %d)", pid, status);
	exit(0);
}
