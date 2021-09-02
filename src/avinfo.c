#include <stdbool.h>
#include <windows.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/log.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h> 

#include "avinfo.h"
#include "vslog.h"

AVInfo *AVInfo_init()
{
	AVInfo *av_info = malloc(sizeof(AVInfo));
	
	av_info -> fmt_ctx = NULL;
	av_info -> codec_ctx = NULL;
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
	av_packet_free(&av_info -> packet);
	av_frame_free(&av_info -> frame);
	
	free(av_info -> filename);
	free(av_info -> bmp_filename);
	free(av_info);
}

bool AVInfo_open(AVInfo *av_info, AVInfo *in_av_info, char *filename,
                 AVType type, int begin, int end)
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
			strcpy(av_info -> filename, in_av_info -> bmp_filename);
			strcpy(av_info -> bmp_filename, in_av_info -> bmp_filename);
			break;
	}

	switch(type)
	{
		case AVTYPE_AUDIO:
		case AVTYPE_BG_IMAGE:
			av_info -> begin = begin;
			av_info -> end = end;
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
			break;
		}
		case AVTYPE_BMP:
			ret = AVInfo_open_bmp(av_info, in_av_info);
			break;
		case AVTYPE_VIDEO:
			ret = AVInfo_open_video(av_info, in_av_info);
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
		abort();
	}
	
	av_info -> frame = av_frame_alloc();
	if(!av_info -> frame)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		abort();
	}

	return true;
}

bool AVInfo_open_bmp(AVInfo *out_av_info, AVInfo *in_av_info)
{
	avformat_alloc_output_context2(&out_av_info -> fmt_ctx, NULL, NULL, out_av_info -> filename);
   if(!out_av_info -> fmt_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		abort();
	}

	out_av_info -> fmt_ctx -> url = av_strdup(out_av_info -> filename);
	out_av_info -> fmt_ctx -> oformat = av_guess_format(NULL, out_av_info -> filename, NULL);
   if(!out_av_info -> fmt_ctx -> oformat)
	{
		avformat_free_context(out_av_info -> fmt_ctx);
		return false;
   }
   
   if(!avformat_new_stream(out_av_info -> fmt_ctx, NULL))
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		abort();
   }
   
   AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_BMP);
	if(!encoder)
	{
      avformat_free_context(out_av_info -> fmt_ctx);
      return false;
   }

   out_av_info -> codec_ctx = avcodec_alloc_context3(encoder);
   if(!out_av_info -> codec_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		abort();
   }

	int screen_w = GetSystemMetrics(SM_CXSCREEN);
	int display_window_w = screen_w * 0.45;
	int display_window_h = screen_w * 0.3;
   double scaling_w = (double) display_window_w / in_av_info -> codec_ctx -> width;
   double scaling_h = (double) display_window_h / in_av_info -> codec_ctx -> height;
   double scaling = ((scaling_w < scaling_h) ? scaling_w: scaling_h);
	out_av_info -> codec_ctx -> width  = in_av_info -> codec_ctx -> width * scaling;
	out_av_info -> codec_ctx -> height = in_av_info -> codec_ctx -> height * scaling;
	
   out_av_info -> codec_ctx -> sample_aspect_ratio = (AVRational){1, 1};   
	out_av_info -> codec_ctx -> pix_fmt = AV_PIX_FMT_BGRA;
	out_av_info -> codec_ctx -> time_base = (AVRational){1, 25};
	out_av_info -> fmt_ctx -> streams[0] -> time_base = out_av_info -> codec_ctx -> time_base;
	if(out_av_info -> fmt_ctx -> oformat -> flags & AVFMT_GLOBALHEADER)
   	out_av_info -> codec_ctx -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

   if(avcodec_open2(out_av_info -> codec_ctx, encoder, NULL) < 0)
	{
      avcodec_free_context(&out_av_info -> codec_ctx);
		avformat_free_context(out_av_info -> fmt_ctx);
		return false;
   }

   if(avcodec_parameters_from_context(out_av_info -> fmt_ctx -> streams[0] -> codecpar, out_av_info -> codec_ctx) < 0)
	{
      avcodec_free_context(&out_av_info -> codec_ctx);                
		avformat_free_context(out_av_info -> fmt_ctx);
		return false;
   }
   
  	out_av_info -> packet = av_packet_alloc();
	if(!out_av_info -> packet)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		abort();
	}
	
	out_av_info -> frame = av_frame_alloc();
	if(!out_av_info -> frame)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		abort();
	}
   
   out_av_info -> frame -> format = AV_PIX_FMT_BGRA;
	out_av_info -> frame -> width  = out_av_info -> codec_ctx -> width;
	out_av_info -> frame -> height = out_av_info -> codec_ctx -> height;
	if(av_frame_get_buffer(out_av_info -> frame, 32) < 0)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		abort();
	}
	
   return true;
}

bool AVInfo_open_video(AVInfo *out_av_info, AVInfo *in_av_info)
{
	
}

void AVInfo_reopen_input(AVInfo *av_info)
{
	if(av_info -> type == AVTYPE_BMP || av_info -> type == AVTYPE_VIDEO)
		return;
	
	avformat_close_input(&av_info -> fmt_ctx);
	avcodec_free_context(&av_info -> codec_ctx);
	av_packet_free(&av_info -> packet);
	av_frame_free(&av_info -> frame);
	
	int begin = -1, end = -1;
	switch(av_info -> type)
	{
		case AVTYPE_AUDIO:
		case AVTYPE_BG_IMAGE:
			begin = av_info -> begin;
			end = av_info -> end;
	}
	AVInfo_open(av_info, NULL, av_info -> filename, av_info -> type, begin, end);
}

bool AVInfo_create_bmp(AVInfo *av_info)
{
	AVInfo* output_av_info = AVInfo_init();
	if(!AVInfo_open(output_av_info, av_info, av_info -> bmp_filename, AVTYPE_BMP, -1, -1))
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
		abort();
	}
	
	while(av_read_frame(av_info -> fmt_ctx, av_info -> packet) >= 0)
	{
		bool finished_decoding = false, finished_encoding = false;
		while(true)
		{
			if(!finished_decoding)
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
				else if(ret2 == AVERROR_EOF)
					finished_decoding = true;
				else if(ret2 < 0)
				{
					AVInfo_free(output_av_info);
					AVInfo_reopen_input(av_info);
					return false;
				}
			}

      	sws_ctx = sws_getCachedContext(sws_ctx, av_info -> frame -> width, av_info -> frame -> height,
			                               (enum AVPixelFormat)av_info -> frame -> format,
			                               output_av_info -> codec_ctx -> width, output_av_info -> codec_ctx -> height,
													 AV_PIX_FMT_BGRA, SWS_FAST_BILINEAR, 0, 0, 0);
   	   if(!sws_ctx)
			{
				AVInfo_free(output_av_info);
				AVInfo_reopen_input(av_info);
				return false;
			}

  	      sws_scale(sws_ctx, (const uint8_t * const *)av_info -> frame -> data,
			          av_info -> frame -> linesize, 0, av_info -> frame -> height,
			          (uint8_t * const *)output_av_info -> frame -> data, output_av_info -> frame -> linesize);

			if(!finished_encoding)
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
				else if(ret4 == AVERROR_EOF)
					finished_encoding = true;
				else if(ret4 < 0)
				{
					AVInfo_free(output_av_info);
					sws_freeContext(sws_ctx);
					AVInfo_reopen_input(av_info);
					return false;
				}
			}

			output_av_info -> packet -> stream_index = 0;
			output_av_info -> packet -> pts = AV_NOPTS_VALUE;
			output_av_info -> packet -> dts = AV_NOPTS_VALUE;
			output_av_info -> packet -> pos = -1;
			break;
		}
		
		if(av_write_frame(output_av_info -> fmt_ctx, output_av_info -> packet) < 0)
		{
			AVInfo_free(output_av_info);
			sws_freeContext(sws_ctx);
			AVInfo_reopen_input(av_info);
			return false;
		}
	}
	sws_freeContext(sws_ctx);

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
