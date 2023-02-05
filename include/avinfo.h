/** 
 * VisualScores header file: avinfo.h
 * Defines struct AVInfo, which contains data for (de)muxing and (de)coding.
 */

#ifndef AVINFO_H
#define AVINFO_H

#include <stdbool.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/file.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#define REPETITION_LIMIT 50  /* maximum number of repetition times of an image file */

extern const double VS_framerate;
extern const int VS_samplerate;
extern const int WAV_samplerate;
extern const int MP3_framesize;
extern const int AAC_framesize;
extern const int WAV_framesize;

typedef enum AVType
{
	AVTYPE_IMAGE,
	AVTYPE_AUDIO,
	AVTYPE_BG_IMAGE,
	AVTYPE_BMP,
	AVTYPE_WAV,
	AVTYPE_VIDEO
} AVType;

typedef struct AVInfo
{
	AVFormatContext *fmt_ctx;
	AVCodecContext  *codec_ctx;   /* audio codec context for video file */
	AVCodecContext  *codec_ctx2;  /* video codec context for video file */
	AVPacket *packet;
	AVFrame  *frame;

	AVType type;
	wchar_t *filename;
	char *filename_utf8;

	/* for image track */
	/**
	 * This variable gives the number of repetition times. If the image is to repeat
	 * n times (it actually shows n + 1 times), then nb_repetition = n. 
	 * We add a minus sign to show that this image is the end of a sequence of
	 * repeated images.
	 */
	int nb_repetition;
	double *duration;  /* in seconds; 3.0 by default; is negative if unspecified */
	wchar_t *bmp_filename;  /* The name of bmp file for display. */
	char *bmp_filename_utf8;

	bool partitioned;  /* for audio track */
	int frame_size;    /* the frame size of the audio stream in the video file */

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
} AVInfo;

/* You should always call this function when initializing an AVInfo object. */
extern AVInfo *AVInfo_init();

/* You should always call this function when destroying an AVInfo object. */
extern void AVInfo_free(AVInfo *av_info);

/**
 * Load an image/audio file to an AVInfo object.
 * Return true on success and false on failure.
 * Variables "begin" and "end" is used to determine
 * av_info -> begin / av_info -> end.
 * Variables "width" and "height" is used to determine the size of output file.
 */
extern bool AVInfo_open(AVInfo *av_info, wchar_t *filename, AVType type,
                         int begin, int end, int width, int height);

/* Variable "fmt_short_name" is used to determine muxers/demuxers. */
extern bool AVInfo_open_input(AVInfo *av_info, char *fmt_short_name);
extern bool AVInfo_open_bmp(AVInfo *av_info);
extern bool AVInfo_open_wav(AVInfo *av_info);
extern bool AVInfo_open_video(AVInfo *av_info, char *fmt_short_name);

/* Clear original data and open the file again. */
extern void AVInfo_reopen_input(AVInfo *av_info);

/* Create a temporary bmp file in order to preview images in "partition_audio". */
extern bool AVInfo_create_bmp(AVInfo *av_info);

/* Create a temporary wav file for audition in "partition_audio". */
extern bool AVInfo_create_wav(AVInfo *av_info);

/* Put the frame "src" in the center of "dest". */
extern void put_frame_to_center(AVFrame *dest, AVFrame *src);

/**
  * Decode and convert "image_info -> packet" to "frame" in RGBA pixel format.
  * Variable "width" and "height" are the width and height of the frame.
  */
extern bool decode_image(AVInfo *image_info, AVFrame *frame, int width, int height);

/**
  * Mix images on "frame1", and convert to YUV420P pixel format on frame2.
  * Variable "pos" is the position of the image in the image track.
  */
extern bool mix_images(AVInfo *image_info, AVInfo **bg_info, AVFrame *frame1,
                       AVFrame *frame2, int pos, int bg_count);

/* Encode and write "video_info -> frame" "nb_frames" times. */
extern bool encode_image(AVInfo *video_info, int64_t begin_pts, int nb_frames);

/* Write blank data to audio track. */
extern bool write_blank_audio(AVInfo *video_info, int64_t begin_pts, int nb_frames);

/* Write raw data to FIFO. */
extern bool AVInfo_write_to_fifo(AVAudioFifo *audio_fifo, AVFrame *frame);

/* The whole decoding/encoding process. */
extern bool decode_audio_to_fifo(AVInfo *audio_info, AVInfo *video_info,
                                 AVAudioFifo *audio_fifo, enum AVSampleFormat fmt);
/* Return the final pts value of the whole audio. */
extern int64_t encode_audio_from_fifo(AVInfo *video_info, AVAudioFifo *audio_fifo,
                                      int64_t begin_pts, enum AVSampleFormat fmt);

/* Get the duration of an audio file. The duration is not directly recorded in aac files. */
extern bool get_audio_duration(AVInfo *audio_info);

#endif /* AVINFO_H */
