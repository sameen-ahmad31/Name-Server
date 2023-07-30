#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <ns_limits.h>

#define BUFSZ MAX_MSG_SZ

int
main(void)
{
	char input[BUFSZ];
	size_t sz = 0;

	while ((sz = read(STDIN_FILENO, input, BUFSZ)) > 0) {
		size_t i, written = 0;

		/* time to yell! */
		for (i = 0; i < sz; i++) {
			if (isalpha(input[i])) input[i] = toupper(input[i]);
		}

		while (1) {
			ssize_t ret = write(STDOUT_FILENO, input + written, sz - written);

			if (ret < 0) return 0;

			written += ret;
			if (written == sz) break;
		}
	}

	return 0;
}
