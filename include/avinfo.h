#ifndef AVINFO_H
#define AVINFO_H

#include <stdbool.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>

typedef enum AVType
{
	AVTYPE_IMAGE,
	AVTYPE_AUDIO,
	AVTYPE_BG_IMAGE,
	AVTYPE_BMP,
	AVTYPE_VIDEO
} AVType;
// Available formats: avi, jpeg, mp4, wmv. //
typedef struct AVInfo
{
	AVFormatContext *fmt_ctx;
	AVCodecContext  *codec_ctx;
	AVPacket *packet;
	AVFrame  *frame;

	AVType type;
	char *filename;

	/* for image track */
	double duration;  /* in seconds; 3.0 by default; is negative if unspecified */
	char *bmp_filename;  /* The name of bmp file for display. */

	/**
	 * for audio & background image track
	 * The audio & background image file corresponds to the [Begin]-th to the [End]-th 
	 * image in the image track.
	 */
	int begin;
	int end;
	
	bool partitioned;  /* for audio track */
} AVInfo;

/* You should always call this function when initializing an AVInfo object. */
extern AVInfo *AVInfo_init();

/* You should always call this function when destroying an AVInfo object. */
extern void AVInfo_free(AVInfo *av_info);

/**
 * Load an image/audio file to an AVInfo object.
 * Return true on success and false on failure.
 * Variable "in_av_info" is the AVInfo object corresponding to a file loaded in some track.
 * This is needed if "av_info" is used for output; otherwise it can be NULL.
 * Variables "begin" and "end" is used to determine
 * av_info -> begin / end / begin / end.
 */
extern bool AVInfo_open(AVInfo *av_info, AVInfo *in_av_info, char *filename,
                        AVType type, int begin, int end);

/* Variable "fmt_short_name" is used to determine demuxers. */
extern bool AVInfo_open_input(AVInfo *av_info, char *fmt_short_name);

/**
 * Variable "out_av_info" is the AVInfo object corresponding to the bmp file.
 * Variable "in_av_info" is the AVInfo object corresponding to the original image.
 */
extern bool AVInfo_open_bmp(AVInfo *out_av_info, AVInfo *in_av_info);

extern bool AVInfo_open_video(AVInfo *out_av_info, AVInfo *in_av_info);

/* Clear original data and open the file again. */
extern void AVInfo_reopen_input(AVInfo *av_info);

/* Creates a temporary bmp file in order to preview images in "partition_audio". */
extern bool AVInfo_create_bmp(AVInfo *av_info);

#endif /* AVINFO_H */
