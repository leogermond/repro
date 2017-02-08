#define _GNU_SOURCE

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

#define MIN(a,b) (((a)<(b))?(a):(b))

static const char *LOG_NAME = "runner";

#define err(f,...) dprintf(2, "\033[31;1me\033[0m %s: "f"\n", LOG_NAME, ##__VA_ARGS__)
#define err_loc(f,...) err("%s:%d "f, __FILE__, __LINE__, ##__VA_ARGS__)
#define info(f,...) printf("\033[32mi\033[0m %s: "f"\n", LOG_NAME, ##__VA_ARGS__)

#define ASSERT(c,...) if(!(c)) err_loc("assert "#c" failed " __VA_ARGS__)

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
#define CELL_READ_TIMEOUT_MS 100

struct cell {
	pid_t pid;
	int fd[2];
	void *mem[2];
};

enum cell_command {
	CELL_CMD_PING = 'p',
	CELL_CMD_QUIT = 'q',
	CELL_CMD_LOAD = 'l'
};

enum cell_response {
	CELL_RESP_PONG = 'p',
	CELL_RESP_START = 's'
};

static int send_cell_command(struct cell *c, char command) {
	info("cell %d: command %c", c->pid, command);
	return write(c->fd[1], &command, 1);
}

static int read_cell(struct cell *c, char *buffer, size_t buffer_len) {
	int ret;
	struct timespec ts_begin, ts_cur;
	clock_gettime(CLOCK_MONOTONIC, &ts_begin);
	while(0 == (ret = read(c->fd[0], buffer, buffer_len)) || (-1 == ret && EAGAIN == errno)) {
		clock_gettime(CLOCK_MONOTONIC, &ts_cur);
		struct timespec elapsed = timespec_sub(ts_cur, ts_begin);
		if((elapsed.tv_sec * 1000 + elapsed.tv_nsec /(1000 * 1000)) > CELL_READ_TIMEOUT_MS) {
			ret = -ETIMEDOUT;
			break;
		}
	}

	return ret;
}

static int check_cell_start(struct cell *c) {
	char start;
	int ret = read_cell(c, &start, 1);
	if(ret <= 0) {
		goto out;
	}
	ret = (start == CELL_RESP_START)? 0:EIO;
out:
	return ret;
}

static int ping_cell(struct cell *c) {
	send_cell_command(c, CELL_CMD_PING);
	char pong;
	int ret = read_cell(c, &pong, 1);
	if(ret <= 0) {
		goto out;
	}

	ret = (pong == CELL_RESP_PONG)? 0:EIO;
out:
	return ret;
}

static int halt_cell(struct cell *c) {
	info("halt cell %d", c->pid);
	kill(c->pid, SIGINT);
	char start;
	int ret = read_cell(c, &start, 1);
	if(ret <= 0) goto out;

	ret = (start == CELL_RESP_START)? 0:EIO;
out:
	return ret;
}

static int program_cell(struct cell *c, const void *data, size_t len) {
	halt_cell(c);
	ASSERT(ping_cell(c) == 0);
	memcpy(c->mem[1], data, len);
	send_cell_command(c, CELL_CMD_LOAD);
	return 0;
}

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
	int ret = pipe2(fd_cell_to_sup, O_NONBLOCK);
	if(ret < 0) {
		err("cannot create send pipe: %s\n", strerror(errno));
		exit(2);
	}
	ret = pipe2(fd_sup_to_cell, O_NONBLOCK);
	if(ret < 0) {
		err("cannot create send pipe: %s\n", strerror(errno));
		exit(2);
	}

	int pid = fork();
	struct timespec ts_start;
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	if(!pid) {
		LOG_NAME = "cell";
		info("keep shm on exec");
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
	struct cell c = {
		pid,
		{fd_cell_to_sup[0], fd_sup_to_cell[1]},
		{shm_rd, shm_wr}
	};

	if(check_cell_start(&c) != 0) {
		err("cell did not start");
		exit(1);
	}

	info("ping %d", c.pid);
	if(ping_cell(&c) != 0) {
		err("cell did not answer to ping");
		exit(1);
	}

	info("program cell %d", c.pid);
	program_cell(&c, "\x3c", 1);

	send_cell_command(&c, CELL_CMD_QUIT);

	info("wait for cell %d", c.pid);
	int status = -1;
	while(pid != waitpid(pid, &status, 0)) {}

	info("exit, child = %d (status = %d)", pid, status);
	exit(0);
}
