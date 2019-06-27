#ifndef GALEFILE_2_MAIN_H_
#define GALEFILE_2_MAIN_H_

#include "gale_exports.h"

#include <stdint.h>

// ===========================================================================
// Quick-use API
// ===========================================================================

// loads first frame's first layer
GAL2DEF uint8_t*				gale2_load (char const *filename, int *width, int *height, int *channelsInFile, int desiredChannels);

// ===========================================================================
// Original-style API
// ===========================================================================

// Interface kept identical to original galefile:

struct gale2File
{
	void* data;
};
enum gale2BoolReturn
{
	kGale2Bool_Success = 1,
	kGale2Bool_Failure = 0,
};
enum gale2ErrorCode
{
	kGale2Error_NoError = 0,
	kGale2Error_FileDoesNotExist = 1,
	kGale2Error_FileFormatInvalid = 2,
	kGale2Error_FileCanNotBeClosed = 3,
	kGale2Error_InvalidGaleFile = 4,
	kGale2Error_InvalidParameter = 5,
};
struct gale2Info
{
	uint32_t		background_color;
	bool			single_palette;
	bool			bottom_layer_opaque;
	uint8_t			bitdepth;
	uint32_t		width;
	uint32_t		height;
};
enum gale2FrameDisposal
{
	kGale2FrameDisposal_Unspecified = 0,
	kGale2FrameDisposal_NotDisposed = 1,
	kGale2FrameDisposal_BackgroundColor = 2,
	kGale2FrameDisposal_RestorePrevious = 3,
};
struct gale2FrameInfo
{
	uint32_t		transparent_color;
	int32_t			millisecond_delay;
	gale2FrameDisposal
					disposal;
};
struct gale2LayerInfo
{
	bool			visible;
	uint32_t		transparent_color;
	uint8_t			opacity;
	bool			uses_alpha_channel;
};

/// Opens a gal file.
///
/// This will load the entire file header into heap memory.
/// The image data itself will be not be decompressed until it is requested.
///
/// @param	path : Filename of the gal file.
/// @return	If the function succeeds, the return value is the address of the gale2 object (an opaque structure).
///			The gale object must be deleted by gale2_close.
///			If the function fails, the return value is NULL.
GAL2DEF gale2File*				gale2_open (char const *path);
/// Deletes a gale2 object.
/// @param	file : The address of the gale object.
/// @return	If the function succeeds, the return value is 1.
///			If the function fails, the return value is 0.
GAL2DEF gale2BoolReturn			gale2_close (gale2File *file);
/// Retrieves the latest error code.
/// @return	Error code. See enum gale2ErrorCode.
GAL2DEF gale2ErrorCode			gale2_getLastError (void);
GAL2DEF int32_t					gale2_getFrameCount (gale2File *file);
GAL2DEF int32_t					gale2_getLayerCount (gale2File *file, int32_t frameNumber);
GAL2DEF gale2BoolReturn			gale2_getInfo (gale2File *file, gale2Info* info);
GAL2DEF gale2BoolReturn			gale2_getFrameInfo (gale2File *file, int32_t frameNumber, gale2FrameInfo *frameInfo);
GAL2DEF int32_t					gale2_getFrameName (gale2File *file, int32_t frameNumber, char *name, int32_t nameLength);
GAL2DEF gale2BoolReturn			gale2_getLayerInfo (gale2File *file, int32_t frameNumber, int32_t layerNumber, gale2LayerInfo *layerInfo);
GAL2DEF int32_t					gale2_getLayerName (gale2File *file, int32_t frameNumber, int32_t layerNumber, char *name, int32_t nameLength);
//returns rgb/layer stuff?
GAL2DEF uint8_t*				gale2_getBitmap (gale2File *file, int32_t frameNumber, int32_t layerNumber);
GAL2DEF uint8_t*				gale2_getAlphaChannel (gale2File *file, int32_t frameNumber, int32_t layerNumber);
GAL2DEF uint8_t*				gale2_getPalette (gale2File *file, int32_t frameNumber, int32_t layerNumber);

GAL2DEF gale2BoolReturn			gale2_exportBitmap (gale2File *file, int32_t frameNumber, int32_t layerNumber, char const *path);
GAL2DEF gale2BoolReturn			gale2_exportAlphaChannel (gale2File *file, int32_t frameNumber, int32_t layerNumber, char const *path);


#endif//GALEFILE_2_MAIN_H_