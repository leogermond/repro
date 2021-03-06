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

void *shm_rd, *shm_wr;

void interpreter(void) {
	char c;
	write(1, "s", 1);
	while(1) {
		while(read(0, &c, sizeof(c)) != sizeof(c)) {}
		
		switch(c) {
		case 'p':
			write(1, "p", 1);
			break;
		case 'q':
			syscall(__NR_exit, EXIT_SUCCESS);
			break;
		case 'l':
			((void(*)(void))shm_rd)();
			write(1, "s", 1);
			break;
		}
	}
}

void interpreter_signal(int signum) {
	interpreter();
}

int main() {
	int shmfd = 3;
	shm_rd = mmap(NULL, PROG_SIZE, PROT_READ | PROT_EXEC, MAP_SHARED, shmfd, 0);
	shm_wr = mmap(NULL, PROG_SIZE, PROT_WRITE, MAP_SHARED, shmfd, PROG_SIZE);

	mprotect(&shm_rd, sizeof(shm_rd), PROT_READ);
	mprotect(&shm_wr, sizeof(shm_wr), PROT_READ);

	signal(SIGINT, interpreter_signal);
	signal(SIGTERM, interpreter_signal);
	signal(SIGILL, interpreter_signal);
	signal(SIGSEGV, interpreter_signal);
	signal(SIGBUS, interpreter_signal);

	prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);

	interpreter();
}
