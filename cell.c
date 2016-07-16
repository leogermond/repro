#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>
#include <signal.h>

#define PAGE_SIZE 0x1000
#define PROG_SIZE (10*PAGE_SIZE)

void *shm_rd, *shm_wr, *prog;

void interpreter(void) {
	char c;
	while(1) {
		while(read(0, &c, sizeof(c)) != sizeof(c)) {}
		
		switch(c) {
		case 'p':
			write(1, "pong\n", 5);
			break;
		case 'q':
			syscall(__NR_exit, EXIT_SUCCESS);
			break;
		case 'l':
			memcpy(prog, shm_rd, PROG_SIZE);
			((void(*)(void))prog)();
			break;
		}
	}
}

void interpreter_signal(int signum) {
	interpreter();
}

int main() {
	int shmfd = 3;
	shm_rd = mmap(NULL, PROG_SIZE, PROT_READ, MAP_SHARED, shmfd, 0);
	shm_wr = mmap(NULL, PROG_SIZE, PROT_WRITE, MAP_SHARED, shmfd, PROG_SIZE);
	prog = mmap(NULL, 2 * PROG_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE, shmfd, 2 * PROG_SIZE);

	mprotect(&shm_rd, sizeof(shm_rd), PROT_READ);
	mprotect(&shm_wr, sizeof(shm_wr), PROT_READ);
	mprotect(&prog, sizeof(prog), PROT_READ);

	signal(SIGUSR1, interpreter_signal);

	prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);

	interpreter();
}
