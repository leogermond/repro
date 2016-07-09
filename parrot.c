#include <stdio.h>
#include <stdlib.h>

int main() {
	int size;
	fread(&size, 1, sizeof(size), stdin);
	char c;
	int i;
	for(i = 0; i < size; i++) {
		fread(&c, 1, sizeof(c), stdin);
		fwrite(&c, 1, sizeof(c), stdout);
	}
	exit(0);
}
