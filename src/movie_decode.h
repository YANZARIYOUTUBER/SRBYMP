// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2023 by LJ Sonic
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  movie_decode.h
/// \brief Movie decoding

#ifndef __MOVIE_DECODE__
#define __MOVIE_DECODE__

#include "doomtype.h"

typedef struct movie movie_t;

movie_t *MovieDecode_Play(const char *name, boolean usepatches, boolean usedithering);
void MovieDecode_Stop(movie_t **movieptr);
void MovieDecode_SetPosition(movie_t *movie, INT64 ms);
void MovieDecode_Seek(movie_t *movie, INT64 ms);
void MovieDecode_Update(movie_t *movie);
void MovieDecode_SetImageFormat(movie_t *movie, boolean usepatches);
INT64 MovieDecode_GetDuration(movie_t *movie);
void MovieDecode_GetDimensions(movie_t *movie, INT32 *width, INT32 *height);
UINT8 *MovieDecode_GetImage(movie_t *movie);
INT32 MovieDecode_GetPatchBytes(movie_t *movie);
void MovieDecode_CopyAudioSamples(movie_t *movie, void *mem, size_t size);

#endif
