#include "galefile2.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <zlib/zlib.h>

static gale2ErrorCode
					l_lastErrorCode = kGale2Error_NoError;
static const char	kFileStateMagic[] = "GALE";

struct gale2AllFrames;
struct gale2Frame;
struct gale2AllLayers;
struct gale2Layer;

struct gale2Header
{
	char		header_values [8];
	uint32_t	compressed_xml_length;
};

struct gale2Layer
{
	uint32_t	left;
	uint32_t	top;
	uint32_t	visible;
	uint32_t	transColor;
	uint32_t	alpha;
	uint32_t	alphaOn;
	char*		name;
	uint32_t	lock;
	intptr_t	file_offset; // offset in the file to the layer information
};
struct gale2AllLayers
{
	uint32_t	count;
	uint32_t	width;
	uint32_t	height;
	uint32_t	bpp;

	gale2Layer*	layers;
};

struct gale2Frame
{
	char*		name;
	int32_t		transColor;
	uint32_t	delay;
	uint32_t	diposal;

	gale2AllLayers
				all_layers;
};
struct gale2AllFrames
{
	uint32_t	version;
	uint32_t	width;
	uint32_t	height;
	uint32_t	bpp;
	uint32_t	count;
	uint32_t	syncPal;
	uint32_t	randomized;
	uint32_t	compType;
	uint32_t	compLevel;
	uint32_t	bgColor;
	uint32_t	blockWidth;
	uint32_t	blockHeight;
	uint32_t	notFillBG;

	gale2Frame*	frames;
};

struct gale2FileState
{
	char		magic_values [4];
	FILE*		file;
	gale2AllFrames
				all_frames;
};

/// Check if the given gale2File pointer is a valid object, via checking magical values at the front.
/// This helps avoid some issues with bad pointers.
/// @param	file : The file pointer to check.
/// @return	True if the magic values are found, false otherwise.
inline bool gale2_fileIsValid (gale2File *file)
{
	gale2FileState* state = (gale2FileState*)file;
	return (file != NULL)
		&& (strncmp(state->magic_values, kFileStateMagic, 4) == 0);
}

/// Shorthand for decompressing data
/// @param	buffer : Buffer of compressed data
/// @param	buffer_length : Length of the compressed data
/// @param	out_buffer_length : Output for the decompressed data
/// @return	The decompressed data, a buffer of length out_buffer_length.
void* uncompressAllocate (void* buffer, uint32_t buffer_length, uint32_t* out_buffer_length)
{
	// Create static local storage used for decompressing:
	uLongf uncompress_target_length = 1024 * 64; // 64k of data
	static void* uncompress_target = NULL;
	if (uncompress_target == NULL) {
		uncompress_target = malloc(uncompress_target_length);
	}

	// uncompress the data
	uncompress((Bytef*)uncompress_target, &uncompress_target_length, (Bytef*)buffer, buffer_length);

	void* uncompress_data = malloc(uncompress_target_length);
	memcpy(uncompress_data, uncompress_target, uncompress_target_length);

	//free(uncompress_target);

	*out_buffer_length = uncompress_target_length;
	return uncompress_data;
}

struct gale2iParseCursorInfo
{
	uint32_t cursorPosition;
	uint32_t nextTagEnding;
	uint32_t afterTagName;
	char* currentString;
};
struct gale2iParseTag
{
	char name [15];
	bool closing;
};

bool gale2i_parseNextTag (gale2iParseCursorInfo* parse_state, gale2iParseTag* tag_output)
{
	// skip to the end of the tag
	parse_state->cursorPosition = parse_state->nextTagEnding;

	// from the current cursor, find the opening of a tag
	while (parse_state->currentString[parse_state->cursorPosition] != 0
		&& parse_state->currentString[parse_state->cursorPosition] != '<')
	{
		parse_state->cursorPosition += 1;
	}
	// if at end, return false
	if (parse_state->currentString[parse_state->cursorPosition] == 0)
		return false;

	// else we are at new tag. find the tag name
	tag_output->closing = false;
	uint32_t name_start = parse_state->cursorPosition + 1;
	while (isspace(parse_state->currentString[name_start]))
	{	// skip spaces
		name_start += 1;
	}
	// check first bit of the name for a closing tag
	if (parse_state->currentString[name_start] == '/')
	{
		tag_output->closing = true;
		name_start += 1;
	}
	// find ending of the name
	parse_state->nextTagEnding = name_start + 1;
	while (parse_state->currentString[parse_state->nextTagEnding] != 0
		&& !isspace(parse_state->currentString[parse_state->nextTagEnding])
		&& parse_state->currentString[parse_state->nextTagEnding] != '>')
	{
		parse_state->nextTagEnding += 1;
	}
	// if at end, return false
	if (parse_state->currentString[parse_state->nextTagEnding] == 0)
		return false;
	// get ending of the tag
	parse_state->afterTagName = parse_state->nextTagEnding;

	// save the tag name
	memcpy(tag_output->name, &parse_state->currentString[name_start], parse_state->nextTagEnding - name_start);
	tag_output->name[parse_state->nextTagEnding - name_start] = 0;

	// find the ending of the tag
	while (parse_state->currentString[parse_state->nextTagEnding] != 0
		&& parse_state->currentString[parse_state->nextTagEnding] != '>')
	{
		parse_state->nextTagEnding += 1;
	}
	// if at end, return false
	if (parse_state->currentString[parse_state->nextTagEnding] == 0)
		return false;

	// otherwise, we got a new tag
	return true;
}

bool gale2i_parseKeyValueDestructively (uint32_t* i, gale2iParseCursorInfo* parse_state, char** key, char** value)
{
	// run forward until we hit the '='
	uint32_t key_start;
	uint32_t key_end;
	while (!isalnum(parse_state->currentString[*i]) && *i < parse_state->nextTagEnding)
		++*i;
	key_start = *i;
	while (parse_state->currentString[*i] != '=' && *i < parse_state->nextTagEnding)
		++*i;
	key_end = *i;

	if (*i >= parse_state->nextTagEnding)
		return false;

	// we now have our key, which we will destroy in-place
	parse_state->currentString[key_end] = 0;
	*key = &parse_state->currentString[key_start];

	// now we parse out the value
	uint32_t value_start;
	uint32_t value_end;
	while (parse_state->currentString[*i] != '"' && *i < parse_state->nextTagEnding)
		++*i;
	value_start = ++*i;
	while (parse_state->currentString[*i] != '"' && *i < parse_state->nextTagEnding)
		++*i;
	value_end = *i;

	if (*i >= parse_state->nextTagEnding)
		return false;

	// we now have our value, which we will destroy in-place
	parse_state->currentString[value_end] = 0;
	*value = &parse_state->currentString[value_start];

	return true;
}

bool gale2i_parseTag (gale2iParseCursorInfo* parse_state, gale2AllFrames* all_frames)
{
	// loop through the string to grab key-values
	char* key;
	char* value;

	// start after the current tag's name
	uint32_t i = parse_state->afterTagName;
	while (i < parse_state->nextTagEnding)
	{
		// run forward until we hit text
		if (!isspace(parse_state->currentString[i]))
		{
			if (!gale2i_parseKeyValueDestructively(&i, parse_state, &key, &value))
				return false;

			if (strncmp(key, "Version", 8) == 0)
				all_frames->version = strtoul(value, NULL, 10);
			else if (strncmp(key, "Width", 6) == 0)
				all_frames->width = strtoul(value, NULL, 10);
			else if (strncmp(key, "Height", 7) == 0)
				all_frames->height = strtoul(value, NULL, 10);
			else if (strncmp(key, "Bpp", 4) == 0)
				all_frames->bpp = strtoul(value, NULL, 10);
			else if (strncmp(key, "Count", 6) == 0)
				all_frames->count = strtoul(value, NULL, 10);
			else if (strncmp(key, "SyncPal", 8) == 0) // todo: what is SyncPal
				all_frames->syncPal = strtoul(value, NULL, 10);
			else if (strncmp(key, "Randomized", 11) == 0)
				all_frames->randomized = strtoul(value, NULL, 10);
			else if (strncmp(key, "CompType", 9) == 0)
				all_frames->compType = strtoul(value, NULL, 10);
			else if (strncmp(key, "CompLevel", 10) == 0)
				all_frames->compLevel = strtoul(value, NULL, 10);
			else if (strncmp(key, "BGColor", 8) == 0)
				all_frames->bgColor = strtoul(value, NULL, 10); // bgcolor is a R8G8B8A8 encoded as a Uint32
			else if (strncmp(key, "BlockWidth", 11) == 0)
				all_frames->blockWidth = strtoul(value, NULL, 10); 
			else if (strncmp(key, "BlockHeight", 12) == 0)
				all_frames->blockHeight = strtoul(value, NULL, 10); 
			else if (strncmp(key, "NotFillBG", 10) == 0)
				all_frames->notFillBG = strtoul(value, NULL, 10); 
			//else
			//	todo: handle invalid keys, or should it be simple ignored?
		}
		else
		{
			++i;
		}
	}
	return true;
}

bool gale2i_parseTag (gale2iParseCursorInfo* parse_state, gale2Frame* frame)
{
	// loop through the string to grab key-values
	char* key;
	char* value;

	// start after the current tag's name
	uint32_t i = parse_state->afterTagName;
	while (i < parse_state->nextTagEnding)
	{
		// run forward until we hit text
		if (!isspace(parse_state->currentString[i]))
		{
			if (!gale2i_parseKeyValueDestructively(&i, parse_state, &key, &value))
				return false;

			if (strncmp(key, "Name", 5) == 0)
			{
				size_t name_len = strlen(value);
				frame->name = (char*)malloc(name_len + 1);
				memcpy(frame->name, value, name_len);
				frame->name[name_len] = 0;
			}
			else if (strncmp(key, "TransColor", 11) == 0)
				frame->transColor = strtol(value, NULL, 10);
			else if (strncmp(key, "Delay", 6) == 0)
				frame->delay = strtoul(value, NULL, 10);
			else if (strncmp(key, "Disposal", 9) == 0)
				frame->diposal = strtoul(value, NULL, 10);
			//else
			//	todo: handle invalid keys, or should it be simple ignored?
		}
		else
		{
			++i;
		}
	}
	return true;
}

bool gale2i_parseTag (gale2iParseCursorInfo* parse_state, gale2AllLayers* all_layers)
{
	// loop through the string to grab key-values
	char* key;
	char* value;

	// start after the current tag's name
	uint32_t i = parse_state->afterTagName;
	while (i < parse_state->nextTagEnding)
	{
		// run forward until we hit text
		if (!isspace(parse_state->currentString[i]))
		{
			if (!gale2i_parseKeyValueDestructively(&i, parse_state, &key, &value))
				return false;

			if (strncmp(key, "Count", 6) == 0)
				all_layers->count = strtoul(value, NULL, 10);
			else if (strncmp(key, "Width", 6) == 0)
				all_layers->width = strtoul(value, NULL, 10);
			else if (strncmp(key, "Height", 7) == 0)
				all_layers->height = strtoul(value, NULL, 10);
			else if (strncmp(key, "Bpp", 4) == 0)
				all_layers->bpp = strtoul(value, NULL, 10);
			//else
			//	todo: handle invalid keys, or should it be simple ignored?
		}
		else
		{
			++i;
		}
	}
	return true;
}

bool gale2i_parseTag (gale2iParseCursorInfo* parse_state, gale2Layer* layer)
{
	// loop through the string to grab key-values
	char* key;
	char* value;

	// start after the current tag's name
	uint32_t i = parse_state->afterTagName;
	while (i < parse_state->nextTagEnding)
	{
		// run forward until we hit text
		if (!isspace(parse_state->currentString[i]))
		{
			if (!gale2i_parseKeyValueDestructively(&i, parse_state, &key, &value))
				return false;

			if (strncmp(key, "Name", 5) == 0)
			{
				size_t name_len = strlen(value);
				layer->name = (char*)malloc(name_len + 1);
				memcpy(layer->name, value, name_len);
				layer->name[name_len] = 0;
			}
			else if (strncmp(key, "Left", 5) == 0)
				layer->left = strtoul(value, NULL, 10);
			else if (strncmp(key, "Top", 4) == 0)
				layer->top = strtoul(value, NULL, 10);
			else if (strncmp(key, "Visible", 8) == 0)
				layer->visible = strtoul(value, NULL, 10);
			else if (strncmp(key, "TransColor", 11) == 0)
				layer->transColor = strtoul(value, NULL, 10);
			else if (strncmp(key, "Alpha", 6) == 0)
				layer->alpha = strtoul(value, NULL, 10);
			else if (strncmp(key, "AlphaOn", 8) == 0)
				layer->alphaOn = strtoul(value, NULL, 10);
			else if (strncmp(key, "Lock", 5) == 0)
				layer->lock = strtoul(value, NULL, 10);
			//else
			//	todo: handle invalid keys, or should it be simple ignored?
		}
		else
		{
			++i;
		}
	}
	return true;
}

gale2File*
gale2_open (char const *path)
{
	// allocate the structure for the state
	gale2FileState* state = (gale2FileState*)malloc(sizeof(gale2FileState));

	// clear it out and set the magic value for ensuring the file is valid
	memset(state, 0, sizeof(gale2FileState));
	memcpy(state->magic_values, kFileStateMagic, 4);

	// open up the file
	state->file = fopen(path, "rb");
	if (state->file == NULL)
	{	// ensure it's valid
		free(state);
		l_lastErrorCode = kGale2Error_FileDoesNotExist;
		return NULL;
	}

	// read in the header
	gale2Header galeHeader;
	fread(&galeHeader, sizeof(gale2Header), 1, state->file);

	// check the beginning of the file for the magic string
	if (strncmp("GaleX200", galeHeader.header_values, 8) != 0)
	{	// Ensure it's valid
		free(state);
		l_lastErrorCode = kGale2Error_FileFormatInvalid;
		return NULL;
	}

	// decompress the xml of the file
	void* compressedBuffer = malloc(galeHeader.compressed_xml_length);
	fread(compressedBuffer, galeHeader.compressed_xml_length, 1, state->file);
	uint32_t uncompressedHeaderLength = 0;

	char* uncompressedXmlString = (char*)uncompressAllocate(compressedBuffer, galeHeader.compressed_xml_length, &uncompressedHeaderLength);
	uncompressedXmlString[uncompressedHeaderLength] = 0;
	//printf("%s", uncompressedHeaderString);

	// we no longer need the compress data, remove it
	free(compressedBuffer);

	// set up the simple parser
	enum gale2iParseState
	{
		kGale2iParseState_LookingForFramesStart,
		kGale2iParseState_LookingForFrames,
		kGale2iParseState_LookingForLayersStart,
		kGale2iParseState_LookingForLayers,
		kGale2iParseState_LookingForFrameEnd,
	};
	gale2iParseState parseState = kGale2iParseState_LookingForFramesStart;
	gale2iParseCursorInfo parseCursorInfo = {};
	gale2iParseTag parseTag;
	bool parseStateValid = true;
	uint32_t parseFrameCount = 0;
	uint32_t parseLayerCount = 0;

	parseCursorInfo.currentString = uncompressedXmlString;
	do
	{
		parseStateValid = gale2i_parseNextTag(&parseCursorInfo, &parseTag);
		if (!parseStateValid) continue;

		if (!parseTag.closing)
		{
			if (parseState == kGale2iParseState_LookingForFramesStart && strncmp(parseTag.name, "Frames", 7) == 0)
			{
				gale2i_parseTag(&parseCursorInfo, &state->all_frames);
				// allocate memory for the frame information
				size_t alloc_size = sizeof(gale2Frame) * state->all_frames.count;
				state->all_frames.frames = (gale2Frame*)malloc(alloc_size);
				memset(state->all_frames.frames, 0, alloc_size);
				// reset frame count
				parseFrameCount = 0;
				// we're inside the Frames group, so look for all frames
				parseState = kGale2iParseState_LookingForFrames;
			}
			else if (parseState == kGale2iParseState_LookingForFrames && strncmp(parseTag.name, "Frame", 6) == 0)
			{
				gale2i_parseTag(&parseCursorInfo, &state->all_frames.frames[parseFrameCount]);
				// we're inside the Frame group, so look for the Layers container
				parseState = kGale2iParseState_LookingForLayersStart;
			}
			else if (parseState == kGale2iParseState_LookingForLayersStart && strncmp(parseTag.name, "Layers", 7) == 0)
			{
				auto& frame = state->all_frames.frames[parseFrameCount];
				gale2i_parseTag(&parseCursorInfo, &frame.all_layers);
				// allocate memory for the layer information
				size_t alloc_size = sizeof(gale2Layer) * frame.all_layers.count;
				frame.all_layers.layers = (gale2Layer*)malloc(alloc_size);
				memset(frame.all_layers.layers, 0, alloc_size);
				// reset layer count
				parseLayerCount = 0;
				parseState = kGale2iParseState_LookingForLayers;
			}
			else if (parseState == kGale2iParseState_LookingForLayers && strncmp(parseTag.name, "Layer", 6) == 0)
			{
				// add a layer
				gale2i_parseTag(&parseCursorInfo, &state->all_frames.frames[parseFrameCount].all_layers.layers[parseLayerCount]);
				// done with the layer, move on
				parseLayerCount += 1;
			}
		}
		else
		{
			if (parseState == kGale2iParseState_LookingForLayers && strncmp(parseTag.name, "Layers", 7) == 0)
			{
				parseState = kGale2iParseState_LookingForFrameEnd;
			}
			else if (parseState == kGale2iParseState_LookingForFrameEnd && strncmp(parseTag.name, "Frame", 6) == 0)
			{
				// done with the frame
				parseFrameCount += 1;
				// go back to looking for frames
				parseState = kGale2iParseState_LookingForFrames;
			}
			else if (parseState == kGale2iParseState_LookingForFrames && strncmp(parseTag.name, "Frames", 7) == 0)
			{
				// we're done.
				break;
			}
		}
	}
	while (parseStateValid);

	// todo: gonna skip thru the file and store the offset to each layer so we can seek to them without needing to do a bunch of writes.

	// free the temp string for the XML info
	free(uncompressedXmlString);

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

	// Loop through all the allocated structures and free them
	for (uint32_t iframe = 0; iframe < state->all_frames.count; ++iframe)
	{
		for (uint32_t ilayer = 0; ilayer < state->all_frames.frames[iframe].all_layers.count; ++ilayer)
		{
			free(state->all_frames.frames[iframe].all_layers.layers[ilayer].name);
		}
		free(state->all_frames.frames[iframe].all_layers.layers);
		free(state->all_frames.frames[iframe].name);
	}
	free(state->all_frames.frames);

	// Free the state file
	free(state);

	return kGale2Bool_Success;
}

gale2ErrorCode
gale2_getLastError (void)
{
	return l_lastErrorCode;
}
