// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2023 by LJ Sonic
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  movie_decode_libav.h
/// \brief Movie decoding implementation using FFmpeg's libav

#ifndef __MOVIE_DECODE_LIBAV__
#define __MOVIE_DECODE_LIBAV__

#include "movie_decode.h"
#include "doomtype.h"
#include "i_threads.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "v_video.h"

typedef struct {
	INT32 datasize;
	uint8_t *data[4];
	int linesize[4];
} avimage_t;

typedef struct
{
	UINT64 id;
	INT64 pts;
	INT64 duration;

	union {
		avimage_t rgba;
		UINT8 *patch;
	} image;
} movievideoframe_t;

typedef struct
{
	INT64 pts;
	INT64 firstsampleposition;
	UINT8 *samples[2];
	size_t numsamples;
} movieaudioframe_t;

typedef struct
{
	void *data;
	INT32 capacity;
	INT32 slotsize;
	INT32 start;
	INT32 size;
} moviebuffer_t;

typedef struct moviestream_s moviestream_t;

typedef struct
{
	int index;
	AVCodecContext *codeccontext;
	moviebuffer_t framepool;
	moviebuffer_t framequeue;
	moviestream_t *stream; // Hack
} moviedecodeworkerstream_t;

typedef struct
{
	colorlookup_t colorlut;
	boolean usepatches;
	boolean usedithering;
	UINT64 nextframeid;

	avimage_t yuv444image;
	avimage_t rgbaimage;

	AVFrame *frame;
	struct SwsContext *yuv444scalingcontext;
	struct SwsContext *rgbascalingcontext;
	SwrContext *resamplingcontext;

	moviedecodeworkerstream_t videostream;
	moviedecodeworkerstream_t audiostream;

	moviebuffer_t packetpool;
	moviebuffer_t packetqueue;

	boolean flushing;
	boolean stopping;

	I_mutex mutex;
	I_cond cond;
	I_mutex condmutex;
} moviedecodeworker_t;

struct moviestream_s
{
	AVStream *stream;
	int index;
	moviebuffer_t buffer;
	INT32 numplanes;
};

typedef struct movie
{
	UINT64 lastvideoframeusedid;
	boolean usepatches;
	boolean usedithering;
	AVFormatContext *formatcontext;
	moviedecodeworker_t decodeworker;
	boolean seeking;

	moviestream_t videostream;
	moviestream_t audiostream;

	UINT8 *lumpdata;
	size_t lumpsize;
	size_t lumpposition;

	INT64 position;
	INT64 audioposition;
} movie_t;

#endif
