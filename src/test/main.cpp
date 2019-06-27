#include "galefile2.h"

#include <stdio.h>
#include <assert.h>

int main (void)
{
	const char* kTestFile = "res/test/1x1.gal";

	{
		auto galefile = gale2_open("bad.file");
		printf("galefile hex: %p\n", galefile);
		assert(galefile == NULL);
	}

	{
		auto galefile = gale2_open(kTestFile);
		printf("galefile hex: %p\n", galefile);
		assert(galefile != NULL);

		gale2_close(galefile);
	}

	return 0;
}