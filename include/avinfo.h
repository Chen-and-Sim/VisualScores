#ifndef AVINFO_H
#define AVINFO_H

#include <stdbool.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/file.h>
#include <libswresample/swresample.h>

extern const double VS_framerate;
extern const int VS_samplerate;

typedef enum AVType
{
	AVTYPE_IMAGE,
	AVTYPE_AUDIO,
	AVTYPE_BG_IMAGE,
	AVTYPE_BMP,
	AVTYPE_VIDEO
} AVType;

typedef struct AVInfo
{
	AVFormatContext *fmt_ctx;
	AVCodecContext  *codec_ctx;   /* video codec context for video file */
	AVCodecContext  *codec_ctx2;  /* audio codec context for video file */
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
	
	/* for all types of file except audio file */
	int width;
	int height;
	
	bool partitioned;  /* for audio track */
} AVInfo;

/* You should always call this function when initializing an AVInfo object. */
extern AVInfo *AVInfo_init();

/* You should always call this function when destroying an AVInfo object. */
extern void AVInfo_free(AVInfo *av_info);

/**
 * Load an image/audio file to an AVInfo object.
 * Return true on success and false on failure.
 * Variables "begin" and "end" is used to determine
 * av_info -> begin / end / begin / end.
 * Variables "width" and "height" is used to determine the size of output file.
 */
extern bool AVInfo_open(AVInfo *av_info, char *filename, AVType type,
                         int begin, int end, int width, int height);

/* Variable "fmt_short_name" is used to determine demuxers. */
extern bool AVInfo_open_input(AVInfo *av_info, char *fmt_short_name);
extern bool AVInfo_open_bmp(AVInfo *av_info);
extern bool AVInfo_open_video(AVInfo *av_info);

/* Clear original data and open the file again. */
extern void AVInfo_reopen_input(AVInfo *av_info);

/* Create a temporary bmp file in order to preview images in "partition_audio". */
extern bool AVInfo_create_bmp(AVInfo *av_info);

/* Write raw data to FIFO. */
extern bool AVInfo_write_to_fifo(AVAudioFifo *audio_fifo, AVFrame *frame);

/* The whole decoding/encoding process. */
extern bool decode_audio_to_fifo(AVInfo *audio_info, AVInfo *video_info,
                                 AVAudioFifo *audio_fifo, struct SwrContext *swr_ctx);
extern bool encode_audio_from_fifo(AVInfo *video_info, AVAudioFifo *audio_fifo, int64_t *pts);

#endif /* AVINFO_H */
