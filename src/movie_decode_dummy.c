// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2023 by LJ Sonic
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  movie_decode_dummy.c
/// \brief Dummy movie decoding implementation

#include "movie_decode.h"
#include "doomdef.h"

movie_t *MovieDecode_Play(const char *name, boolean usepatches, boolean usedithering)
{
	(void)name;
	(void)usepatches;
	(void)usedithering;

	I_Error("FFmpeg: movie decoding not supported");
	return NULL;
}

void MovieDecode_Stop(movie_t **movieptr)
{
	(void)movieptr;
}

void MovieDecode_SetPosition(movie_t *movie, INT64 ms)
{
	(void)movie;
	(void)ms;
}

void MovieDecode_Seek(movie_t *movie, INT64 ms)
{
	(void)movie;
	(void)ms;
}

void MovieDecode_Update(movie_t *movie)
{
	(void)movie;
}

void MovieDecode_SetImageFormat(movie_t *movie, boolean usepatches)
{
	(void)movie;
	(void)usepatches;
}

INT64 MovieDecode_GetDuration(movie_t *movie)
{
	(void)movie;
	return 0;
}

void MovieDecode_GetDimensions(movie_t *movie, INT32 *width, INT32 *height)
{
	(void)movie;
	(void)width;
	(void)height;

	*width = 0;
	*height = 0;
}

UINT8 *MovieDecode_GetImage(movie_t *movie)
{
	(void)movie;
	return NULL;
}

INT32 MovieDecode_GetPatchBytes(movie_t *movie)
{
	(void)movie;
	return 0;
}

void MovieDecode_CopyAudioSamples(movie_t *movie, void *mem, size_t size)
{
	(void)movie;
	(void)mem;
	(void)size;
}
