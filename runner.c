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
#include <sys/prctl.h>
#include <limits.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

static const char *LOG_NAME = "runner";
static int verbose = 0;

#define dlog(d,color,id,f,...) dprintf(d, "\033["color"m"id"\033[0m %s: "f"\n", LOG_NAME, ##__VA_ARGS__)

#define err(f,...) dlog(2, "31;1", "e", f, ##__VA_ARGS__)
#define err_loc(f,...) err("%s:%d "f, __FILE__, __LINE__, ##__VA_ARGS__)
#define info(f,...) dlog(1,"32", "i", f, ##__VA_ARGS__)
#define dbg(f,...) if(verbose) dlog(1, "34", "d", f, ##__VA_ARGS__)

#define ASSERT(c,...) if(!(c)) err_loc("assert "#c" failed " __VA_ARGS__)

void dhd(int d, void *data, size_t len) {
	int i;
	for(i = 0; i < len; i++) {
		if(i%16 == 0) dprintf(d, "%08x  ", i);
		dprintf(d, "%02x ", ((unsigned char*)data)[i]);
		if(i%16 == 15) dprintf(d, "\n");
		else if(i%8 == 7) dprintf(d, " ");
	}
	if(i%16 != 0) dprintf(d, "\n");
}

void hd(void *data, size_t len) {
	dhd(1, data, len);
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
	int generation;
	void *proto;
};

enum cell_command {
	CELL_CMD_PING = 'p',
	CELL_CMD_QUIT = 'q',
	CELL_CMD_LOAD = 'l',
};

enum cell_response {
	CELL_RESP_PONG = 'p',
	CELL_RESP_START = 's'
};

static int send_cell_command(struct cell *c, char command) {
	dbg("cell %d: command %c", c->pid, command);
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

	if(ret > 0) {
		dbg("message from %d: '%.*s'", c->pid, ret, buffer);
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
	dbg("halt cell %d", c->pid);
	kill(c->pid, SIGINT);
	char start;
	int ret = read_cell(c, &start, 1);
	if(ret <= 0) goto out;

	ret = (start == CELL_RESP_START)? 0:EIO;
out:
	return ret;
}

static int save_cell(struct cell *c) {
	char fname[21 + UUID_SIZE];
	sprintf(fname, "bestiary/%u_", c->generation);
	uuid(fname + strlen(fname));
	FILE *f = fopen(fname, "w+");
	if(f) {
		fwrite(c->proto, 1, PROG_SIZE, f);
		fclose(f);

		info("saved as %s", fname);
		return 0;
	} else {
		err("could not open %s", fname);
		return EIO;
	}
}

static void *generate_random_program(void) {
	int *prog = malloc(PROG_SIZE);
	for(int i = 0; i < PROG_SIZE/sizeof(*prog); i++) {
		prog[i] = rand();
	}
	return prog;
}

static void *generate_random_program_from(const void *proto) {
	int *prog = malloc(PROG_SIZE);
	memcpy(prog, proto, PROG_SIZE);
	for(int i = 0; i < PROG_SIZE/(sizeof(int)*100); i++) {
		if(rand() < INT_MAX/10) {
			for(int j = 0; j < 100/sizeof(int); j++) {
				prog[i*100+j] = rand();
			}
		}
	}
	return prog;
}

static int program_cell(struct cell *c, const void *data) {
	dbg("program cell\n");
	halt_cell(c);
	ASSERT(ping_cell(c) == 0);
	memcpy(c->mem[1], data, PROG_SIZE);
	send_cell_command(c, CELL_CMD_LOAD);
	return 0;
}

static int create_cell(struct cell *c) {
	char shm_name[UUID_SIZE + 2];
	shm_name[0] = '/';
	uuid(shm_name + 1);
	shm_name[sizeof(shm_name) - sizeof(shm_name[0])] = '\0';
	int shmfd = shm_open(shm_name, O_RDWR | O_TRUNC | O_CREAT, 0777);
	dbg("shm %s => fd %d", shm_name, shmfd);
	ftruncate(shmfd, 2 * PROG_SIZE);

	ASSERT(shmfd != -1);
	int fd_sup_to_cell[2], fd_cell_to_sup[2];
	int ret = pipe2(fd_cell_to_sup, O_NONBLOCK);
	if(ret < 0) {
		err("cannot create send pipe: %s\n", strerror(errno));
		exit(1);
	}
	ret = pipe2(fd_sup_to_cell, O_NONBLOCK);
	if(ret < 0) {
		err("cannot create send pipe: %s\n", strerror(errno));
		exit(1);
	}

	int pid = fork();
	struct timespec ts_start;
	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	if(!pid) {
		LOG_NAME = "cell";

		/* kill on death of parent */
		prctl(PR_SET_PDEATHSIG, SIGHUP);

		dbg("keep shm on exec");
		fcntl(shmfd, F_SETFD, 0);
		dbg("closing file descriptors and redirecting to pipe");
		close(fd_cell_to_sup[0]);
		close(fd_sup_to_cell[1]);
		dup2(fd_sup_to_cell[0], 0);
		close(fd_sup_to_cell[0]);
		dbg("radio silence");
		dup2(fd_cell_to_sup[1], 1);
		close(fd_cell_to_sup[1]);
		close(2);

		char *args[] = {NULL}, *env[] = {NULL};
		execve("build/cell", args, env);
		exit(1);
	}
	close(fd_sup_to_cell[0]);
	close(fd_cell_to_sup[1]);

	dbg("opening shm segments");
	void *shm_rd = mmap(NULL, PROG_SIZE, PROT_READ, MAP_SHARED, shmfd, PROG_SIZE);
	void *shm_wr = mmap(NULL, PROG_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	shm_unlink(shm_name);
	close(shmfd);

	struct cell cs = {
		pid,
		{fd_cell_to_sup[0], fd_sup_to_cell[1]},
		{shm_rd, shm_wr}
	};
	*c = cs;
	return 0;
}

static void destroy_cell(struct cell *c) {
	munmap(c->mem[0], PROG_SIZE);
	munmap(c->mem[1], PROG_SIZE);
	close(c->fd[0]);
	close(c->fd[1]);
}

int main(int argc, char **argv) {
	if(argc >= 2 && strcmp(argv[1], "-v") == 0) {
		verbose = 1;
	}

	srand(2);
	struct cell dish[1];
	for(int i = 0; i < ARRAY_SIZE(dish); i++) {
		create_cell(&dish[i]);
	}

	int max_generation = 1;
	char best[PROG_SIZE];
	memcpy(best, generate_random_program(), sizeof(best));
	while(1) {
		int status;
		int pid = waitpid(-1, &status, WNOHANG);
		for(int i = 0; i < ARRAY_SIZE(dish); i++) {
			info("cell %d", i);
			struct cell *c = &dish[i];
			if(pid != 0) {
				if(pid == c->pid) {
					info("dead, resurrect as a new cell");
					destroy_cell(c);
					create_cell(c);
				}
			}

			if(check_cell_start(c) == 0) {
				dbg("ping %d", i);
				if(ping_cell(c) != 0) {
					err("no answer to ping");
					kill(SIGTERM, c->pid);
				}
				c->generation += 1;
				info("generation %d", c->generation);
				if(c->generation > max_generation) {
					max_generation = c->generation;
					info(">>>>>>>>>>>> MAX GENERATION %d >>>>>>>>>>", max_generation);
					hd(c->proto, 64);
					memcpy(best, c->proto, PROG_SIZE);

					if(c->generation > 2) {
						save_cell(c);
					}
				}

				if(c->generation == 1) {
					info("random program and start cell");
					c->proto = generate_random_program_from(best);
					program_cell(c, c->proto);
				} else {
					info("reproduce cell");
					program_cell(c, c->mem[0]);
				}
			}
		}
	}

	info("end of experiment");
	exit(0);
}
