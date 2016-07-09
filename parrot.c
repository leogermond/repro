#include <stdlib.h>
#include <unistd.h>

int main() {
	int end = 0;
	int i = 0, len = 0;
	while(! end) {
		char c;
		if(read(3, &c, sizeof(c)) != sizeof(c)) continue;
		i += 1;
		if(i <= 4) {
			len += c<<(8*(4-i));
		}

		write(4, &c, sizeof(c));
		end = (i == len + 4);
	}

	exit(0);
}
