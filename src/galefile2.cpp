#include "galefile2.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static gale2ErrorCode
					l_lastErrorCode = kGale2Error_NoError;
static const char	kFileStateMagic[] = "GALE";

struct gale2FileState
{
	char		magic_values [4];
	FILE*		file;
	uint32_t a;
	uint32_t b;
};

inline bool gale2_fileIsValid (gale2File *file)
{
	gale2FileState* state = (gale2FileState*)file;
	return (file != NULL)
		&& (strncmp(state->magic_values, kFileStateMagic, 4) == 0);
}

gale2File*
gale2_open (char const *path)
{
	// allocate the structure for the state
	gale2FileState* state = (gale2FileState*)malloc(sizeof(gale2FileState));

	// clear it out and set the magic value for ensuring the file is valid
	memset(state, 0, sizeof(gale2FileState));
	memcpy(state->magic_values, kFileStateMagic, 4);

	// return a structure of only the pointer to the state (type-safe on the user's end)
	return (gale2File*)state;
}

gale2BoolReturn
gale2_close (gale2File *file)
{
	if (gale2_fileIsValid(file))
	{
		l_lastErrorCode = kGale2Error_InvalidGaleFile;
		return kGale2Bool_Failure;
	}

	gale2FileState* state = (gale2FileState*)file;
	// Close the open file
	fclose(state->file);

	// Free the state file
	free(state);

	return kGale2Bool_Success;
}