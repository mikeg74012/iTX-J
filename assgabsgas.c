#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main() {
	int yeah[5] = {1, 2, 3, 4, 5};

	int idx = 2;
	int result;

	memcpy(&result, &yeah[idx], sizeof(int));
	memset(&yeah[idx], 0, sizeof(int));

	printf("%d\n", result);
	printf("%d\n", yeah[2]);
	return 0;
}