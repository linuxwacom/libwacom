#include <assert.h>
#include <stdio.h>

int main(void) {
	const char *version = LIBWACOM_LT_VERSION;
	int C, R, A;
	int rc;

	rc = sscanf(version, "%d:%d:%d", &C, &R, &A);
	assert(rc == 3);

        /* we don't change the soname anymore, we use symbol maps instead.
           So these can stay fixed until we properly break the ABI and bump
           the soname.  */
	assert(C == 9);
	assert(R == 0);
	assert(A == 0);

	return 0;
}
