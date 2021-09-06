#include <stdbool.h>
#include <windows.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h> 

#include "avinfo.h"
#include "vslog.h"

const double VS_framerate = 30.0;
const int VS_samplerate = 48000;

AVInfo *AVInfo_init()
{
	AVInfo *av_info = malloc(sizeof(AVInfo));
	
	av_info -> fmt_ctx = NULL;
	av_info -> codec_ctx = NULL;
	av_info -> codec_ctx2 = NULL;
	av_info -> packet = NULL;
	av_info -> frame = NULL;

	av_info -> duration = 3.0;
	av_info -> partitioned = false;
	av_info -> filename = malloc(sizeof(char) * STRING_LIMIT);
	av_info -> bmp_filename = malloc(sizeof(char) * STRING_LIMIT);

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
			avformat_close_input(&av_info -> fmt_ctx);
			break;
		case AVTYPE_BMP:
		case AVTYPE_VIDEO:
			avformat_free_context(av_info -> fmt_ctx);
	}
	
	avcodec_free_context(&av_info -> codec_ctx);
	if(av_info -> codec_ctx2 != NULL)
		avcodec_free_context(&av_info -> codec_ctx2);
	av_packet_free(&av_info -> packet);
	av_frame_free(&av_info -> frame);
	
	free(av_info -> filename);
	free(av_info -> bmp_filename);
	free(av_info);
}

bool AVInfo_open(AVInfo *av_info, char *filename, AVType type,
                 int begin, int end, int width, int height)
{
	av_info -> type = type;
	strcpy(av_info -> filename, filename);
	switch(type)
	{
		case AVTYPE_IMAGE:
		{
			char *pch = strrchr(av_info -> filename, '\\');
			sprintf(av_info -> bmp_filename, "_file\\_display_%s.bmp", pch + 1);
			break;
		}
		case AVTYPE_BMP:
			strcpy(av_info -> bmp_filename, filename);
			break;
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

	char *pch = strrchr(filename, '.') + 1;
	char fmt_short_name[10];
	strcpy(fmt_short_name, pch);
	if(strcmp(fmt_short_name, "jpg") == 0)
		strcpy(fmt_short_name, "jpeg");
	if(strcmp(fmt_short_name, "tif") == 0)
		strcpy(fmt_short_name, "tiff");
	if((type == AVTYPE_IMAGE || type == AVTYPE_BG_IMAGE) && strcmp(fmt_short_name, "ico") != 0)
		strcat(fmt_short_name, "_pipe");

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
		case AVTYPE_VIDEO:
			ret = AVInfo_open_video(av_info);
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

	av_info -> fmt_ctx -> url = av_strdup(av_info -> filename);
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

	AVCodec *codec = avcodec_find_decoder(av_info -> fmt_ctx -> streams[0] -> codecpar -> codec_id);
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
	avformat_alloc_output_context2(&av_info -> fmt_ctx, NULL, NULL, av_info -> filename);
	if(!av_info -> fmt_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> fmt_ctx -> url = av_strdup(av_info -> filename);
	av_info -> fmt_ctx -> oformat = av_guess_format(NULL, av_info -> filename, NULL);
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
   
	AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_BMP);
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

bool AVInfo_open_video(AVInfo *av_info)
{
	avformat_alloc_output_context2(&av_info -> fmt_ctx, NULL, NULL, av_info -> filename);
	if(!av_info -> fmt_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	av_info -> fmt_ctx -> url = av_strdup(av_info -> filename);
	av_info -> fmt_ctx -> oformat = av_guess_format(NULL, av_info -> filename, NULL);
	if(!av_info -> fmt_ctx -> oformat)
	{
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
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
	av_info -> codec_ctx -> pix_fmt = AV_PIX_FMT_YUV420P;
	av_info -> codec_ctx -> time_base = (AVRational){1, VS_framerate * 1000};
	av_info -> codec_ctx -> framerate = (AVRational){(int)VS_framerate, 1};
	av_info -> codec_ctx -> has_b_frames = 0;
	av_info -> codec_ctx -> max_b_frames = 0;
	av_info -> codec_ctx -> gop_size = 0;
	if(av_info -> fmt_ctx -> oformat -> flags & AVFMT_GLOBALHEADER)
		av_info -> codec_ctx -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if(avcodec_open2(av_info -> codec_ctx, encoder, NULL) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	AVStream *video_stream = avformat_new_stream(av_info -> fmt_ctx, encoder);
	if(!video_stream)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(avcodec_parameters_from_context(video_stream -> codecpar, av_info -> codec_ctx) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}
	video_stream -> time_base = av_info -> codec_ctx -> time_base;

	AVCodec *encoder2 = avcodec_find_encoder(AV_CODEC_ID_AAC);
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

	av_info -> codec_ctx2 -> sample_fmt = AV_SAMPLE_FMT_FLTP;
	av_info -> codec_ctx2 -> sample_rate = VS_samplerate;
	av_info -> codec_ctx2 -> bit_rate = 192000;
	av_info -> codec_ctx2 -> time_base = (AVRational){1, VS_samplerate};
	av_info -> codec_ctx2 -> channel_layout = AV_CH_LAYOUT_STEREO;
	av_info -> codec_ctx2 -> channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
	if(av_info -> fmt_ctx -> oformat -> flags & AVFMT_GLOBALHEADER)
		av_info -> codec_ctx2 -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	if(avcodec_open2(av_info -> codec_ctx2, encoder2, NULL) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avcodec_free_context(&av_info -> codec_ctx2);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}

	AVStream *audio_stream = avformat_new_stream(av_info -> fmt_ctx, encoder2);
	if(!audio_stream)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(avcodec_parameters_from_context(audio_stream -> codecpar, av_info -> codec_ctx2) < 0)
	{
		avcodec_free_context(&av_info -> codec_ctx);
		avcodec_free_context(&av_info -> codec_ctx2);
		avformat_free_context(av_info -> fmt_ctx);
		return false;
	}
	audio_stream -> time_base = av_info -> codec_ctx2 -> time_base;

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
	if(av_info -> type == AVTYPE_BMP || av_info -> type == AVTYPE_VIDEO)
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

bool AVInfo_create_bmp(AVInfo *av_info)
{
	AVInfo* output_av_info = AVInfo_init();
	
	int screen_w = GetSystemMetrics(SM_CXSCREEN);
	int display_window_w = screen_w * 0.45;
	int display_window_h = screen_w * 0.3;
	double scaling_w = (double) display_window_w / av_info -> width;
	double scaling_h = (double) display_window_h / av_info -> height;
	double scaling = ((scaling_w < scaling_h) ? scaling_w: scaling_h);
	int width  = av_info -> width * scaling;
	int height = av_info -> height * scaling;

	if(!AVInfo_open(output_av_info, av_info -> bmp_filename, AVTYPE_BMP, -1, -1, width, height))
	{
		free(output_av_info);
		return false;
	}

	if(avformat_write_header(output_av_info -> fmt_ctx, NULL) < 0)
	{
		AVInfo_free(output_av_info);
		return false;
	}
	
	struct SwsContext *sws_ctx = sws_alloc_context();
	if(!sws_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	if(av_read_frame(av_info -> fmt_ctx, av_info -> packet) < 0)
	{
		AVInfo_free(output_av_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	while(true)
	{
		int ret1 = avcodec_send_packet(av_info -> codec_ctx, av_info -> packet) < 0;
		if(ret1 < 0 && ret1 != AVERROR(EAGAIN) && ret1 != AVERROR_EOF)
		{
			AVInfo_free(output_av_info);
			AVInfo_reopen_input(av_info);
			return false;
		}

		int ret2 = avcodec_receive_frame(av_info -> codec_ctx, av_info -> frame);
		if(ret2 == AVERROR(EAGAIN))
			continue;
		else if(ret2 == AVERROR_EOF || ret2 == 0)
			break;
		else if(ret2 < 0)
		{
			AVInfo_free(output_av_info);
			AVInfo_reopen_input(av_info);
			return false;
		}
	}
	
	sws_ctx = sws_getCachedContext(sws_ctx, av_info -> frame -> width, av_info -> frame -> height,
	                               (enum AVPixelFormat)av_info -> frame -> format,
	                               output_av_info -> width, output_av_info -> height,
											 AV_PIX_FMT_BGRA, SWS_LANCZOS, 0, 0, 0);
	if(!sws_ctx)
	{
		AVInfo_free(output_av_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	sws_scale(sws_ctx, (const uint8_t * const *)av_info -> frame -> data,
	          av_info -> frame -> linesize, 0, av_info -> frame -> height,
	          (uint8_t * const *)output_av_info -> frame -> data, output_av_info -> frame -> linesize);

	while(true)
	{
		int ret3 = avcodec_send_frame(output_av_info -> codec_ctx, output_av_info -> frame);
		if(ret3 < 0 && ret3 != AVERROR(EAGAIN) && ret3 != AVERROR_EOF)
		{
			AVInfo_free(output_av_info);
			sws_freeContext(sws_ctx);
			AVInfo_reopen_input(av_info);
			return false;
		}

		int ret4 = avcodec_receive_packet(output_av_info -> codec_ctx, output_av_info -> packet);
		if(ret4 == AVERROR(EAGAIN))
			continue;
		else if(ret4 == AVERROR_EOF || ret4 == 0)
			break;
		else if(ret4 < 0)
		{
			AVInfo_free(output_av_info);
			sws_freeContext(sws_ctx);
			AVInfo_reopen_input(av_info);
			return false;
		}
	}

	sws_freeContext(sws_ctx);
	output_av_info -> packet -> stream_index = 0;
	output_av_info -> packet -> pts = AV_NOPTS_VALUE;
	output_av_info -> packet -> dts = AV_NOPTS_VALUE;
	output_av_info -> packet -> pos = -1;

	if(av_write_frame(output_av_info -> fmt_ctx, output_av_info -> packet) < 0)
	{
		AVInfo_free(output_av_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	if(av_write_trailer(output_av_info -> fmt_ctx) < 0)
	{
		AVInfo_free(output_av_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	AVInfo_free(output_av_info);
	AVInfo_reopen_input(av_info);
	return true;
}

void put_frame_to_center(AVFrame *dest, AVFrame *src)
{
	int x_min = (dest -> width - src -> width) / 2;
	int x_max = x_min + src -> width;
	int y_min = (dest -> height - src -> height) / 2;
	int y_max = y_min + src -> height;

	for(int x = 0; x < dest -> width; ++x)
	{
		for(int y = 0; y < dest -> height; ++y)
		{
			uint8_t *R = dest -> data[0] + y * dest -> linesize[0] + 4 * x;
			uint8_t *G = dest -> data[0] + y * dest -> linesize[0] + 4 * x + 1;
			uint8_t *B = dest -> data[0] + y * dest -> linesize[0] + 4 * x + 2;
			uint8_t *A = dest -> data[0] + y * dest -> linesize[0] + 4 * x + 3;
			if(x < x_min || x >= x_max || y < y_min || y >= y_max)
			{
				*R = 255;  *G = 255;
				*B = 255;  *A = 0;
			}
			else
			{
				*R = *(src -> data[0] + (y - y_min) * src -> linesize[0] + 4 * (x - x_min));
				*G = *(src -> data[0] + (y - y_min) * src -> linesize[0] + 4 * (x - x_min) + 1);
				*B = *(src -> data[0] + (y - y_min) * src -> linesize[0] + 4 * (x - x_min) + 2);
				*A = *(src -> data[0] + (y - y_min) * src -> linesize[0] + 4 * (x - x_min) + 3);
			}
		}
	}
}

bool decode_image(AVInfo *image_info, AVFrame *frame, int width, int height)
{
	if(av_read_frame(image_info -> fmt_ctx, image_info -> packet) < 0)
		return false;

	while(true)
	{
		int ret1 = avcodec_send_packet(image_info -> codec_ctx, image_info -> packet) < 0;
		if(ret1 < 0 && ret1 != AVERROR(EAGAIN) && ret1 != AVERROR_EOF)
			return false;

		int ret2 = avcodec_receive_frame(image_info -> codec_ctx, image_info -> frame);
		if(ret2 == AVERROR(EAGAIN))
			continue;
		else if(ret2 == AVERROR_EOF || ret2 == 0)
			break;
		else if(ret2 < 0)
			return false;
	}

	double scaling_w = (double)width  / image_info -> frame -> width;
	double scaling_h = (double)height / image_info -> frame -> height;
	double scaling = ((scaling_w < scaling_h) ? scaling_w : scaling_h);
	int scaled_w = image_info -> frame -> width * scaling;
	int scaled_h = image_info -> frame -> height * scaling;
	scaled_w = scaled_w / 4 * 4;
	scaled_h = scaled_h / 4 * 4;

	AVFrame *temp_frame = av_frame_alloc();
	temp_frame -> format = AV_PIX_FMT_RGBA;
	temp_frame -> width  = scaled_w;
	temp_frame -> height = scaled_h;
	if(av_frame_get_buffer(temp_frame, 0) < 0)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	struct SwsContext *sws_ctx = sws_alloc_context();
	if(!sws_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	sws_ctx = sws_getCachedContext(sws_ctx, image_info -> frame -> width, image_info -> frame -> height,
	                               (enum AVPixelFormat)image_info -> frame -> format,
	                               scaled_w, scaled_h, AV_PIX_FMT_RGBA, SWS_LANCZOS, 0, 0, 0);
	if(!sws_ctx)
	{
		av_frame_free(&temp_frame);
		sws_freeContext(sws_ctx);
		return false;
	}

	sws_scale(sws_ctx, (const uint8_t * const *)image_info -> frame -> data,
	          image_info -> frame -> linesize, 0, image_info -> frame -> height,
	          (uint8_t * const *)temp_frame -> data, temp_frame -> linesize);
	sws_freeContext(sws_ctx);

	frame -> format = AV_PIX_FMT_RGBA;
	frame -> width  = width;
	frame -> height = height;
	if(av_frame_get_buffer(frame, 0) < 0)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	put_frame_to_center(frame, temp_frame);
	av_frame_free(&temp_frame);
	return true;
}

bool mix_images(AVInfo *image_info, AVInfo **bg_info, AVFrame *frame1,
                AVFrame *frame2, int pos, int bg_count)
{
	for(int j = 0; j < bg_count; ++j)
	{
		if(bg_info[j] -> begin - 1 <= pos && bg_info[j] -> end - 1 >= pos)
		{
			AVFrame *bg_frame = av_frame_alloc();
			if(!decode_image(bg_info[j], bg_frame, frame1 -> width, frame1 -> height))
			{
				AVInfo_reopen_input(bg_info[j]);
				av_frame_free(&bg_frame);
				return false;
			}

			for(int y = 0; y < frame1 -> height; ++y)
			{
				for(int x = 0; x < frame1 -> linesize[0]; ++x)
				{
					/* skip alpha channels */
					if(x % 4 == 3)  continue;
					
					uint8_t *data1 = frame1 -> data[0] + y * frame1 -> linesize[0] + x;
					uint8_t *data2 = bg_frame -> data[0] + y * bg_frame -> linesize[0] + x;
					if(*data1 > *data2)
						*data1 = *data2;
				}
			}
			
			AVInfo_reopen_input(bg_info[j]);
			av_frame_free(&bg_frame);
		}
	}

	struct SwsContext *sws_ctx = sws_alloc_context();
	if(!sws_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	sws_ctx = sws_getCachedContext(sws_ctx, frame1 -> width, frame1 -> height, AV_PIX_FMT_RGBA,
	                               frame1 -> width, frame1 -> height, AV_PIX_FMT_YUV420P, 
											 SWS_LANCZOS, 0, 0, 0);
	if(!sws_ctx)
	{
		sws_freeContext(sws_ctx);
		return false;
	}

	frame2 -> format = AV_PIX_FMT_YUV420P;
	frame2 -> width  = frame1 -> width;
	frame2 -> height = frame1 -> height;
	if(av_frame_get_buffer(frame2, 0) < 0)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	sws_scale(sws_ctx, (const uint8_t * const *)frame1 -> data,
	          frame1 -> linesize, 0, frame1 -> height,
	          (uint8_t * const *)frame2 -> data, frame2 -> linesize);

	sws_freeContext(sws_ctx);
	return true;
}

bool encode_image(AVInfo *video_info, int64_t begin_pts, int nb_frames)
{
	for(int frame = 0; frame < nb_frames; ++frame)
	{
		while(true)
		{
			int ret3 = avcodec_send_frame(video_info -> codec_ctx, video_info -> frame);
			if(ret3 < 0 && ret3 != AVERROR(EAGAIN) && ret3 != AVERROR_EOF)
				return false;

			int ret4 = avcodec_receive_packet(video_info -> codec_ctx, video_info -> packet);
			if(ret4 == AVERROR(EAGAIN))
				continue;
			else if(ret4 == AVERROR_EOF || ret4 == 0)
				break;
			else if(ret4 < 0)
				return false;
		}

		video_info -> packet -> stream_index = 0;
		video_info -> packet -> pts = begin_pts + frame * 1000;
		video_info -> packet -> dts = video_info -> packet -> pts;
		video_info -> packet -> duration = 1000;
		video_info -> packet -> pos = -1;

		if(av_write_frame(video_info -> fmt_ctx, video_info -> packet) < 0)
			return false;
		av_packet_unref(video_info -> packet);
	}
	return true;
}

bool AVInfo_write_to_fifo(AVAudioFifo *audio_fifo, AVFrame *frame)
{
	if(av_audio_fifo_space(audio_fifo) < frame -> nb_samples)
	{
		if(av_audio_fifo_realloc(audio_fifo, av_audio_fifo_size(audio_fifo) + (1 << 20)) < 0)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}
	}

	if(av_audio_fifo_write(audio_fifo, (void **)frame -> data, frame -> nb_samples) < 0)
		return false;

	return true;
}

bool decode_audio_to_fifo(AVInfo *audio_info, AVInfo *video_info,
                          AVAudioFifo *audio_fifo, struct SwrContext *swr_ctx)
{
	bool first_time = true, finished_reading = false;
	int ret;
	while(true)
	{
		int ret = av_read_frame(audio_info -> fmt_ctx, audio_info -> packet);
		if(ret == AVERROR_EOF)
			finished_reading = true;
		else if(ret < 0)
		{
			av_audio_fifo_free(audio_fifo);
			return false;
		}

		while(true)
		{
			ret = avcodec_send_packet(audio_info -> codec_ctx, audio_info -> packet) < 0;
			if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			{
				av_audio_fifo_free(audio_fifo);
				return false;
			}

			ret = avcodec_receive_frame(audio_info -> codec_ctx, audio_info -> frame);
			if(ret == AVERROR(EAGAIN))
				continue;
			else if(ret == AVERROR_EOF || ret == 0)
				break;
			else if(ret < 0)
			{
				av_audio_fifo_free(audio_fifo);
				return false;
			}
		}

		if(finished_reading)
		{
			int ret1;
			while(true)
			{
				ret1 = swr_convert(swr_ctx, (uint8_t **)video_info -> frame -> data,
				                   video_info -> frame -> nb_samples, 0, 0);
				if(ret1 <= 0)
					break;
				video_info -> frame -> nb_samples = ret1;

				if(!AVInfo_write_to_fifo(audio_fifo, video_info -> frame))
				{
					av_audio_fifo_free(audio_fifo);
					return false;
				}
			}
			break;
		}

		av_frame_unref(video_info -> frame);
		video_info -> frame -> format = AV_SAMPLE_FMT_FLTP;
		video_info -> frame -> sample_rate = VS_samplerate;
		video_info -> frame -> nb_samples = av_rescale_rnd(audio_info -> frame -> nb_samples, VS_samplerate,
		                                                   audio_info -> frame -> sample_rate, AV_ROUND_UP);
		video_info -> frame -> channel_layout = AV_CH_LAYOUT_STEREO;

		ret = av_frame_get_buffer(video_info -> frame, 0);
		if(ret < 0)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}

		if(first_time)
		{
			swr_ctx = swr_alloc_set_opts(swr_ctx, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLTP, VS_samplerate, 
			                             av_get_default_channel_layout(audio_info -> frame -> channels), 
			                             (enum AVSampleFormat)(audio_info -> frame -> format),
			                             audio_info -> frame -> sample_rate, 0, NULL);

			if(swr_init(swr_ctx) < 0)
			{
				av_audio_fifo_free(audio_fifo);
				return false;
			}
			
			if(video_info -> codec_ctx2 -> frame_size == 0)
				video_info -> codec_ctx2 -> frame_size = audio_info -> codec_ctx -> frame_size;
			if(video_info -> codec_ctx2 -> frame_size == 0)
				video_info -> codec_ctx2 -> frame_size = 1024;
			first_time = false;
		}

		ret = swr_convert(swr_ctx, (uint8_t **)video_info -> frame -> data, video_info -> frame -> nb_samples,
		                     (const uint8_t **)audio_info -> frame -> data, audio_info -> frame -> nb_samples);
		if(ret < 0)
		{
			av_audio_fifo_free(audio_fifo);
			return false;
		}
		video_info -> frame -> nb_samples = ret;

		if(!AVInfo_write_to_fifo(audio_fifo, video_info -> frame))
		{
			av_audio_fifo_free(audio_fifo);
			return false;
		}
	}
	
	return true;
}

bool encode_audio_from_fifo(AVInfo *video_info, AVAudioFifo *audio_fifo, int64_t pts)
{
	int ret;
	bool finished_writing = false;
	while(!finished_writing)
	{
		av_frame_unref(video_info -> frame);
		video_info -> frame -> format = AV_SAMPLE_FMT_FLTP;
		video_info -> frame -> sample_rate = VS_samplerate;
		video_info -> frame -> nb_samples = video_info -> codec_ctx2 -> frame_size;
		video_info -> frame -> channel_layout = AV_CH_LAYOUT_STEREO;

		ret = av_frame_get_buffer(video_info -> frame, 0);
		if(ret < 0)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}

		ret = av_audio_fifo_read(audio_fifo, (void **)video_info -> frame -> data,
		                         video_info -> codec_ctx2 -> frame_size);
		if(ret < 0)
		{
			av_audio_fifo_free(audio_fifo);
			return false;
		}
		if(av_audio_fifo_size(audio_fifo) == 0)
			finished_writing = true;
		video_info -> frame -> nb_samples = ret;

		while(true)
		{
			ret = avcodec_send_frame(video_info -> codec_ctx2, video_info -> frame);
			if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			{
				av_audio_fifo_free(audio_fifo);
				return false;
			}

			ret = avcodec_receive_packet(video_info -> codec_ctx2, video_info -> packet);
			if(ret == AVERROR(EAGAIN))
				continue;
			else if(ret == AVERROR_EOF || ret == 0)
				break;
			else if(ret < 0)
			{
				av_audio_fifo_free(audio_fifo);
				return false;
			}
		}

		video_info -> packet -> stream_index = 1;
		video_info -> packet -> pts = pts;
		video_info -> packet -> dts = pts;
		video_info -> packet -> duration = video_info -> frame -> nb_samples;
		pts += video_info -> frame -> nb_samples;
		video_info -> packet -> pos = -1;

		ret = av_write_frame(video_info -> fmt_ctx, video_info -> packet);
		if(ret < 0)
		{
			av_audio_fifo_free(audio_fifo);
			return false;
		}
	}

	return true;
}
