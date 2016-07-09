#include <stdlib.h>
#include <unistd.h>

int main() {
	int end = 0;
	int i, len = 0;
	while(! end) {
		char c;
		if(read(3, &c, sizeof(c)) != sizeof(c)) continue;
		i += 1;
		if(i <= 4) {
			len += c<<(4-i);
		}

		write(4, &c, sizeof(c));
		if(i == len + 4) {
			break;
		}
	}

	exit(0);
}
