/** 
 * VisualScores source file: avinfo.c
 * Defines functions which initialize and free AVInfo objects, and initialize
 * contexts inside these AVInfo objects.
 */

#include <locale.h>
#include <stdbool.h>
#include <string.h>
#include <stringapiset.h>
#include <wchar.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "avinfo.h"
#include "vslog.h"

const double VS_framerate = 25.0;
const int VS_samplerate = 48000;
const int WAV_samplerate = 44100;
const int MP3_framesize = 1152;
const int AAC_framesize = 1024;
const int WAV_framesize = 1024;

AVInfo *AVInfo_init()
{
	AVInfo *av_info = malloc(sizeof(AVInfo));
	if(av_info == NULL)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	av_info -> fmt_ctx = NULL;
	av_info -> codec_ctx = NULL;
	av_info -> codec_ctx2 = NULL;
	av_info -> packet = NULL;
	av_info -> frame = NULL;

	av_info -> nb_repetition = 0;
	av_info -> duration = malloc(sizeof(double) * REPETITION_LIMIT);
	av_info -> duration[0] = 3.0;
	av_info -> partitioned = false;
	av_info -> filename = malloc(sizeof(wchar_t) * STRING_LIMIT);
	av_info -> bmp_filename = malloc(sizeof(wchar_t) * STRING_LIMIT);
	av_info -> filename_utf8 = malloc(sizeof(wchar_t) * STRING_LIMIT);
	av_info -> bmp_filename_utf8 = malloc(sizeof(wchar_t) * STRING_LIMIT);

	return av_info;
}

void AVInfo_free(AVInfo *av_info)
{
	if(av_info -> fmt_ctx -> pb != NULL)
		avio_closep(&av_info -> fmt_ctx -> pb);
	switch(av_info -> type)
	{
		case AVTYPE_IMAGE:
		case AVTYPE_AUDIO:
		case AVTYPE_BG_IMAGE:
			if(av_info -> fmt_ctx != NULL)
				avformat_close_input(&av_info -> fmt_ctx);
			break;
		case AVTYPE_BMP:
		case AVTYPE_WAV:
		case AVTYPE_VIDEO:
			if(av_info -> fmt_ctx != NULL)
				avformat_free_context(av_info -> fmt_ctx);
	}
	
	if(av_info -> codec_ctx  != NULL)
		avcodec_free_context(&av_info -> codec_ctx);
	if(av_info -> codec_ctx2 != NULL)
		avcodec_free_context(&av_info -> codec_ctx2);
	av_packet_free(&av_info -> packet);
	av_frame_free(&av_info -> frame);
	
	free(av_info -> duration);
	free(av_info -> filename);
	free(av_info -> bmp_filename);
	free(av_info -> filename_utf8);
	free(av_info -> bmp_filename_utf8);
	free(av_info);
}

bool AVInfo_open(AVInfo *av_info, wchar_t *filename, AVType type,
                 int begin, int end, int width, int height)
{
	wcscpy_s(av_info -> filename, STRING_LIMIT, filename);
	int size = WideCharToMultiByte(CP_UTF8, 0, filename, -1, NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, filename, -1, av_info -> filename_utf8, size, NULL, NULL);

	char fmt_short_name[10];
	wchar_t w_fmt[10];
	wchar_t *pwc = wcsrchr(filename, L'.') + 1;
	wcscpy_s(w_fmt, 10, pwc);
	wcstombs(fmt_short_name, w_fmt, 10);

	if(strcmp(fmt_short_name, "jpg") == 0)
		strcpy_s(fmt_short_name, 10, "jpeg");
	if(strcmp(fmt_short_name, "tif") == 0)
		strcpy_s(fmt_short_name, 10, "tiff");
	if((type == AVTYPE_IMAGE || type == AVTYPE_BG_IMAGE) && strcmp(fmt_short_name, "ico") != 0)
		strcat_s(fmt_short_name, 10, "_pipe");

	av_info -> type = type;
	switch(type)
	{
		case AVTYPE_IMAGE:
		{
			wchar_t *pwc = wcsrchr(av_info -> filename, L'\\');
			swprintf_s(av_info -> bmp_filename, STRING_LIMIT, L"resource\\_display_%ls.bmp", pwc + 1);
			int size = WideCharToMultiByte(CP_UTF8, 0, av_info -> bmp_filename, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_UTF8, 0, av_info -> bmp_filename, -1, av_info -> bmp_filename_utf8, size, NULL, NULL);
			break;
		}
		case AVTYPE_BMP:
		{
			wcscpy_s(av_info -> bmp_filename, STRING_LIMIT, filename);
			int size = WideCharToMultiByte(CP_UTF8, 0, av_info -> bmp_filename, -1, NULL, 0, NULL, NULL);
			WideCharToMultiByte(CP_UTF8, 0, av_info -> bmp_filename, -1, av_info -> bmp_filename_utf8, size, NULL, NULL);
			break;
		}
	}

	switch(type)
	{
		case AVTYPE_AUDIO:
		case AVTYPE_BG_IMAGE:
			av_info -> begin = begin;
			av_info -> end = end;
			break;

		case AVTYPE_BMP:
		case AVTYPE_VIDEO:
			av_info -> width = width;
			av_info -> height = height;
			break;
	}

	bool ret = true;
	switch(type)
	{
		case AVTYPE_IMAGE:
		case AVTYPE_AUDIO:
		case AVTYPE_BG_IMAGE:
		{
			ret = AVInfo_open_input(av_info, fmt_short_name);
			if(ret)
			{
				av_info -> width = av_info -> codec_ctx -> width;
				av_info -> height = av_info -> codec_ctx -> height;
			}
			break;
		}
		case AVTYPE_BMP:
			ret = AVInfo_open_bmp(av_info);
			break;
		case AVTYPE_WAV:
			ret = AVInfo_open_wav(av_info);
			break;
		case AVTYPE_VIDEO:
			ret = AVInfo_open_video(av_info, fmt_short_name);
			break;
	}
	return ret;
}

bool AVInfo_open_input(AVInfo *av_info, char *fmt_short_name)
{
	av_info -> fmt_ctx = avformat_alloc_context();
	if(!av_info -> fmt_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> fmt_ctx -> iformat = av_find_input_format(fmt_short_name);
	if(!av_info -> fmt_ctx -> iformat)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	av_info -> fmt_ctx -> url = av_strdup(av_info -> filename_utf8);
	if(avformat_open_input(&av_info -> fmt_ctx, av_info -> fmt_ctx -> url,
	                        av_info -> fmt_ctx -> iformat, NULL) < 0)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	if(avformat_find_stream_info(av_info -> fmt_ctx, NULL) < 0)
	{
		avformat_close_input(&av_info -> fmt_ctx);
		return false;
	}

	const AVCodec *codec = avcodec_find_decoder(av_info -> fmt_ctx -> streams[0] -> codecpar -> codec_id);
	if(!codec)
	{
		avformat_close_input(&av_info -> fmt_ctx);
		return false;
	}

	av_info -> codec_ctx = avcodec_alloc_context3(codec);
	if(!av_info -> codec_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(avcodec_parameters_to_context(av_info -> codec_ctx, av_info -> fmt_ctx -> streams[0] -> codecpar) < 0)
	{
		avformat_close_input(&av_info -> fmt_ctx);
		avcodec_free_context(&av_info -> codec_ctx);
		return false;
	}

	if(avcodec_open2(av_info -> codec_ctx, NULL, NULL) < 0)
	{
		avformat_close_input(&av_info -> fmt_ctx);
		avcodec_free_context(&av_info -> codec_ctx);
		return false;
	}

	av_info -> packet = av_packet_alloc();
	if(!av_info -> packet)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	av_info -> frame = av_frame_alloc();
	if(!av_info -> frame)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	return true;
}

bool AVInfo_open_bmp(AVInfo *av_info)
{
	avformat_alloc_output_context2(&av_info -> fmt_ctx, NULL, NULL, av_info -> filename_utf8);
	if(!av_info -> fmt_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> fmt_ctx -> url = av_strdup(av_info -> filename_utf8);
	av_info -> fmt_ctx -> oformat = av_guess_format(NULL, av_info -> filename_utf8, NULL);
	if(!av_info -> fmt_ctx -> oformat)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}
   
	if(!avformat_new_stream(av_info -> fmt_ctx, NULL))
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
   
	const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_BMP);
	if(!encoder)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	av_info -> codec_ctx = avcodec_alloc_context3(encoder);
	if(!av_info -> codec_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> codec_ctx -> width  = av_info -> width;
	av_info -> codec_ctx -> height = av_info -> height;
	av_info -> codec_ctx -> sample_aspect_ratio = (AVRational){1, 1};   
	av_info -> codec_ctx -> pix_fmt = AV_PIX_FMT_BGRA;
	av_info -> codec_ctx -> time_base = (AVRational){1, (int)VS_framerate};
	av_info -> fmt_ctx -> streams[0] -> time_base = av_info -> codec_ctx -> time_base;
	if(av_info -> fmt_ctx -> oformat -> flags & AVFMT_GLOBALHEADER)
		av_info -> codec_ctx -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if(avcodec_open2(av_info -> codec_ctx, encoder, NULL) < 0)
	{
    	avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	if(avcodec_parameters_from_context(av_info -> fmt_ctx -> streams[0] -> codecpar, av_info -> codec_ctx) < 0)
	{
    	avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

  	av_info -> packet = av_packet_alloc();
	if(!av_info -> packet)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	av_info -> frame = av_frame_alloc();
	if(!av_info -> frame)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> frame -> format = AV_PIX_FMT_BGRA;
	av_info -> frame -> width  = av_info -> codec_ctx -> width;
	av_info -> frame -> height = av_info -> codec_ctx -> height;
	if(av_frame_get_buffer(av_info -> frame, 32) < 0)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
   return true;
}

bool AVInfo_open_wav(AVInfo *av_info)
{
	avformat_alloc_output_context2(&av_info -> fmt_ctx, NULL, NULL, av_info -> filename_utf8);
	if(!av_info -> fmt_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> fmt_ctx -> url = av_strdup(av_info -> filename_utf8);
	av_info -> fmt_ctx -> oformat = av_guess_format(NULL, av_info -> filename_utf8, NULL);
	if(!av_info -> fmt_ctx -> oformat)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_ADPCM_IMA_WAV);
	if(!encoder)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	av_info -> codec_ctx = avcodec_alloc_context3(encoder);
	if(!av_info -> codec_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> frame_size = WAV_framesize;
	av_info -> codec_ctx -> sample_fmt = AV_SAMPLE_FMT_S16P;
	av_info -> codec_ctx -> sample_rate = WAV_samplerate;
	av_info -> codec_ctx -> bit_rate = 352800;
	av_info -> codec_ctx -> time_base = (AVRational){1, WAV_samplerate};
	av_channel_layout_default(&av_info -> codec_ctx -> ch_layout, 2);
	if(av_info -> fmt_ctx -> oformat -> flags & AVFMT_GLOBALHEADER)
		av_info -> codec_ctx -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if(avcodec_open2(av_info -> codec_ctx, encoder, NULL) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	AVStream *audio_stream = avformat_new_stream(av_info -> fmt_ctx, encoder);
	if(!audio_stream)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(avcodec_parameters_from_context(audio_stream -> codecpar, av_info -> codec_ctx) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}
	audio_stream -> time_base = av_info -> codec_ctx -> time_base;

	av_info -> packet = av_packet_alloc();
	if(!av_info -> packet)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	av_info -> frame = av_frame_alloc();
	if(!av_info -> frame)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	return true;
}

bool AVInfo_open_video(AVInfo *av_info, char *fmt_short_name)
{
	avformat_alloc_output_context2(&av_info -> fmt_ctx, NULL, NULL, av_info -> filename_utf8);
	if(!av_info -> fmt_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> fmt_ctx -> url = av_strdup(av_info -> filename_utf8);
	av_info -> fmt_ctx -> oformat = av_guess_format(NULL, av_info -> filename_utf8, NULL);
	if(!av_info -> fmt_ctx -> oformat)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	const AVCodec *encoder;
	if(strcmp(fmt_short_name, "avi") == 0)
		encoder = avcodec_find_encoder(AV_CODEC_ID_MP3);
	else
		encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);

	if(!encoder)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	av_info -> codec_ctx = avcodec_alloc_context3(encoder);
	if(!av_info -> codec_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(strcmp(fmt_short_name, "avi") == 0)
	{
		av_info -> codec_ctx -> sample_fmt = AV_SAMPLE_FMT_S16;
		av_info -> frame_size = MP3_framesize;
	}
	else
	{
		av_info -> codec_ctx -> sample_fmt = AV_SAMPLE_FMT_FLTP;
		av_info -> frame_size = AAC_framesize;
	}
	av_info -> codec_ctx -> sample_rate = VS_samplerate;
	av_info -> codec_ctx -> bit_rate = 192000;
	av_info -> codec_ctx -> time_base = (AVRational){1, VS_samplerate};
	av_channel_layout_default(&av_info -> codec_ctx -> ch_layout, 2);
	if(av_info -> fmt_ctx -> oformat -> flags & AVFMT_GLOBALHEADER)
		av_info -> codec_ctx -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if(avcodec_open2(av_info -> codec_ctx, encoder, NULL) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	AVStream *audio_stream = avformat_new_stream(av_info -> fmt_ctx, encoder);
	if(!audio_stream)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(avcodec_parameters_from_context(audio_stream -> codecpar, av_info -> codec_ctx) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}
	audio_stream -> time_base = av_info -> codec_ctx -> time_base;

	const AVCodec *encoder2 = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
	if(!encoder2)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	av_info -> codec_ctx2 = avcodec_alloc_context3(encoder2);
	if(!av_info -> codec_ctx2)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> codec_ctx2 -> width  = av_info -> width;
	av_info -> codec_ctx2 -> height = av_info -> height;
	av_info -> codec_ctx2 -> sample_aspect_ratio = (AVRational){1, 1};
	av_info -> codec_ctx2 -> pix_fmt = AV_PIX_FMT_YUV420P;
	av_info -> codec_ctx2 -> time_base = (AVRational){1, (int)VS_framerate};
	av_info -> codec_ctx2 -> framerate = (AVRational){(int)VS_framerate, 1};
	av_info -> codec_ctx2 -> max_b_frames = 0;
	av_info -> codec_ctx2 -> gop_size = 10;
	if(av_info -> fmt_ctx -> oformat -> flags & AVFMT_GLOBALHEADER)
		av_info -> codec_ctx2 -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if(avcodec_open2(av_info -> codec_ctx2, encoder2, NULL) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avcodec_free_context(&av_info -> codec_ctx2);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	AVStream *video_stream = avformat_new_stream(av_info -> fmt_ctx, encoder2);
	if(!video_stream)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(avcodec_parameters_from_context(video_stream -> codecpar, av_info -> codec_ctx2) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avcodec_free_context(&av_info -> codec_ctx2);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}
	video_stream -> time_base = av_info -> codec_ctx2 -> time_base;

	av_info -> packet = av_packet_alloc();
	if(!av_info -> packet)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	av_info -> frame = av_frame_alloc();
	if(!av_info -> frame)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	return true;
}

void AVInfo_reopen_input(AVInfo *av_info)
{
	if(av_info -> type == AVTYPE_BMP || av_info -> type == AVTYPE_WAV || av_info -> type == AVTYPE_VIDEO)
		return;
	
	avformat_close_input(&av_info -> fmt_ctx);
	avcodec_free_context(&av_info -> codec_ctx);
	av_packet_free(&av_info -> packet);
	av_frame_free(&av_info -> frame);
	
	int begin = -1, end = -1, width = -1, height = -1;
	switch(av_info -> type)
	{
		case AVTYPE_AUDIO:
		case AVTYPE_BG_IMAGE:
			begin = av_info -> begin;
			end = av_info -> end;
			break;
	}
	if(av_info -> type != AVTYPE_AUDIO)
	{
		width = av_info -> width;
		height = av_info -> height;
	}
	AVInfo_open(av_info, av_info -> filename, av_info -> type, begin, end, width, height);
}
