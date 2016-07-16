#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <sys/syscall.h>

#define PAGE_SIZE 0x1000
#define PROG_SIZE (10*PAGE_SIZE)
int main() {
	int shmfd = 3;
	void *shm_rd = mmap(NULL, PROG_SIZE, PROT_READ, MAP_SHARED, shmfd, 0);
	void *shm_wr = mmap(NULL, PROG_SIZE, PROT_WRITE, MAP_SHARED, shmfd, PROG_SIZE);
	void *prog = mmap(NULL, 2 * PROG_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE, shmfd, 2 * PROG_SIZE); 

	dprintf(1,"rd=%p wr=%p prog=%p\n", shm_rd, shm_wr, prog);

	FILE *f = fopen("/proc/self/maps", "r");
	char fc[1024];
	size_t flen;
	while((flen = fread(fc, sizeof(char), sizeof(fc)/sizeof(char), f)) > 0) {
		write(1, fc, flen);
	}
	prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
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
