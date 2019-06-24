#include <assert.h>
#include <stdio.h>

int main(void) {
	const char *version = LIBWACOM_LT_VERSION;
	int C, R, A;
	int rc;

	rc = sscanf(version, "%d:%d:%d", &C, &R, &A);
	assert(rc == 3);

	assert(C >= 8);
	assert(R >= 0);
	assert(A >= 6);

	/* Binary compatibility broken? */
	assert(R != 0 || A != 0);

	/* The first stable API in 0.3 had 2:0:0  */
	assert(C - A == 2);

	return 0;
}
