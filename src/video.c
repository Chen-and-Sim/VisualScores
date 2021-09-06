#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shlobj.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "vslog.h"
#include "visualscores.h"

bool has_video_ext(char *filename)
{
	char *ext = strrchr(filename, '.');
	if(ext == NULL)
		return false;
	else  ++ext;

	const char video_ext[2][5] = {"mov", "mp4"};
	for(int i = 0; i < 4; ++i)
		if(strcmp(ext, video_ext[i]) == 0)
			return true;
	return false;
}

void get_video_filename(char *dest, char *src)
{
	if(src[0] == '\0')
		strcpy(dest, ".\\_video.mp4");
	else
	{
		char first[8] = "\0\0\0\0\0\0\0\0";
		for(int i = 0; i < 7; ++i)
		{
			if(src[i] >= 'A' && src[i] <= 'Z')
				first[i] = src[i] - 'A' + 'a';
			else  first[i] = src[i];
		}
		
		if(strcmp(first, "desktop") == 0)
		{
			char desktop_path[STRING_LIMIT];
			SHGetSpecialFolderPath(0, desktop_path, CSIDL_DESKTOPDIRECTORY, 0);

			if(desktop_path[strlen(desktop_path) - 1] == '\\' || 
			   desktop_path[strlen(desktop_path) - 1] == '/')
				desktop_path[strlen(desktop_path) - 1] = '\0';
			sprintf(dest, "%s%s", desktop_path, src + 7);
		}
		else  strcpy(dest, src);
	}
}

void export_video(VisualScores *vs, char *cmd)
{
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	double total_time = 0;
	for(int i = 0; i < vs -> image_count; ++i)
	{
		if(vs -> image_info[vs -> image_pos[i]] -> duration <= 0)
		{
			VS_print_log(DURATION_NOT_SET, i + 1);
			return;
		}
		total_time += vs -> image_info[vs -> image_pos[i]] -> duration;
	}
	if(total_time > TIME_LIMIT)
		VS_print_log(TIME_LIMIT_EXCEEDED);
	
	char filename[STRING_LIMIT];
	get_video_filename(filename, cmd);
	if(!has_video_ext(filename))
	{
		VS_print_log(UNSUPPORTED_EXTENSION);
		return;
	}
	
	int width = 0, height = 0;
	for(int i = 0; i < vs -> image_count; ++i)
	{
		if(vs -> image_info[i] -> width > width)
			width = vs -> image_info[i] -> width;
		if(vs -> image_info[i] -> height > height)
			height = vs -> image_info[i] -> height;
	}
	for(int i = 0; i < vs -> bg_count; ++i)
	{
		if(vs -> bg_info[i] -> width > width)
			width = vs -> bg_info[i] -> width;
		if(vs -> bg_info[i] -> height > height)
			height = vs -> bg_info[i] -> height;
	}
	width = width / 4 * 4;
	height = height / 4 * 4;
	
	vs -> video_info = AVInfo_init();
	AVInfo_open(vs -> video_info, filename, AVTYPE_VIDEO, -1, -1, width, height);
	
	bool avio_opened = (!(vs -> video_info -> fmt_ctx -> oformat -> flags & AVFMT_NOFILE));
	if(avio_opened && (avio_open(&vs -> video_info -> fmt_ctx -> pb, 
	                              vs -> video_info -> filename, AVIO_FLAG_WRITE) < 0))
	{
		VS_print_log(FAILED_TO_EXPORT);
		AVInfo_free(vs -> video_info);
		return;
	}
	
	if(avformat_write_header(vs -> video_info -> fmt_ctx, NULL) < 0)
	{
		VS_print_log(FAILED_TO_EXPORT);
		AVInfo_free(vs -> video_info);
		return;
	}

	if(!write_image_track(vs))
	{
		VS_print_log(FAILED_TO_EXPORT);
		AVInfo_free(vs -> video_info);
		return;
	}

	if(!write_audio_track(vs))
	{
		VS_print_log(FAILED_TO_EXPORT);
		AVInfo_free(vs -> video_info);
		return;
	}
	
	if(av_write_trailer(vs -> video_info -> fmt_ctx) < 0)
	{
		VS_print_log(FAILED_TO_EXPORT);
		AVInfo_free(vs -> video_info);
		return;
	}

	AVInfo_free(vs -> video_info);
	VS_print_log(VIDEO_EXPORTED);
}

bool write_image_track(VisualScores *vs)
{
	struct SwsContext *sws_ctx = sws_alloc_context();
	if(!sws_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	double total_time_to_prev_image = 0.0, total_time_to_cur_image = 0.0;
	for(int i = 0; i < vs -> image_count; ++i)
	{
		VS_print_log(WRITING_IMAGE_TRACK, i + 1, vs -> image_count);
		
		vs -> video_info -> frame -> format = AV_PIX_FMT_YUV420P;
		vs -> video_info -> frame -> width  = vs -> video_info -> width;
		vs -> video_info -> frame -> height = vs -> video_info -> height;
		if(av_frame_get_buffer(vs -> video_info -> frame, 0) < 0)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}

		AVInfo *image_info = vs -> image_info[vs -> image_pos[i]];
		if(av_read_frame(image_info -> fmt_ctx, image_info -> packet) < 0)
		{
			AVInfo_reopen_input(image_info);
			return false;
		}

		while(true)
		{
			int ret1 = avcodec_send_packet(image_info -> codec_ctx, image_info -> packet) < 0;
			if(ret1 < 0 && ret1 != AVERROR(EAGAIN) && ret1 != AVERROR_EOF)
			{
				AVInfo_reopen_input(image_info);
				return false;
			}

			int ret2 = avcodec_receive_frame(image_info -> codec_ctx, image_info -> frame);
			if(ret2 == AVERROR(EAGAIN))
				continue;
			else if(ret2 == AVERROR_EOF || ret2 == 0)
				break;
			else if(ret2 < 0)
			{
				AVInfo_reopen_input(image_info);
				return false;
			}
		}

		sws_ctx = sws_getCachedContext(sws_ctx, image_info -> frame -> width, image_info -> frame -> height,
		                               (enum AVPixelFormat)image_info -> frame -> format,
		                               vs -> video_info -> width, vs -> video_info -> height,
												 AV_PIX_FMT_YUV420P, SWS_LANCZOS, 0, 0, 0);
		if(!sws_ctx)
		{
			AVInfo_reopen_input(image_info);
			return false;
		}
 
		sws_scale(sws_ctx, (const uint8_t * const *)image_info -> frame -> data,
		          image_info -> frame -> linesize, 0, image_info -> frame -> height,
		          (uint8_t * const *)vs -> video_info -> frame -> data, vs -> video_info -> frame -> linesize);

		total_time_to_cur_image += image_info -> duration;
		int nb_frames = (double)(total_time_to_cur_image - total_time_to_prev_image) * VS_framerate;

		for(int frame = 0; frame < nb_frames; ++frame)
		{
			while(true)
			{
				int ret3 = avcodec_send_frame(vs -> video_info -> codec_ctx, vs -> video_info -> frame);
				if(ret3 < 0 && ret3 != AVERROR(EAGAIN) && ret3 != AVERROR_EOF)
				{
					sws_freeContext(sws_ctx);
					AVInfo_reopen_input(image_info);
					return false;
				}

				int ret4 = avcodec_receive_packet(vs -> video_info -> codec_ctx, vs -> video_info -> packet);
				if(ret4 == AVERROR(EAGAIN))
					continue;
				else if(ret4 == AVERROR_EOF || ret4 == 0)
					break;
				else if(ret4 < 0)
				{
					sws_freeContext(sws_ctx);
					AVInfo_reopen_input(image_info);
					return false;
				}
			}

			vs -> video_info -> packet -> stream_index = 0;
			vs -> video_info -> packet -> pts = total_time_to_prev_image * vs -> video_info -> codec_ctx -> time_base.den + frame * 1000;
			vs -> video_info -> packet -> dts = vs -> video_info -> packet -> pts;
			vs -> video_info -> packet -> duration = 1000;
			vs -> video_info -> packet -> pos = -1;

			if(av_write_frame(vs -> video_info -> fmt_ctx, vs -> video_info -> packet) < 0)
			{
				sws_freeContext(sws_ctx);
				AVInfo_reopen_input(image_info);
				return false;
			}
			av_packet_unref(vs -> video_info -> packet);
		}

		total_time_to_prev_image += (nb_frames / VS_framerate);
		av_frame_unref(vs -> video_info -> frame);
		AVInfo_reopen_input(image_info);
	}
	
	sws_freeContext(sws_ctx);
	return true;
}

bool write_audio_track(VisualScores *vs)
{
	if(vs -> audio_count > 0)
		printf("\n");

	for(int i = 0; i < vs -> audio_count; ++i)
	{
		VS_print_log(WRITING_AUDIO_TRACK, i + 1, vs -> audio_count);

		double begin_time = 0.0;
		for(int i = 0; i < vs -> audio_info[i] -> begin - 1; ++i)
			begin_time += vs -> image_info[vs -> image_pos[i]] -> duration;
		int64_t pts = begin_time * vs -> video_info -> codec_ctx2 -> time_base.den;

		struct SwrContext *swr_ctx = swr_alloc();
		if(!swr_ctx)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}
		
		AVAudioFifo *audio_fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 
		                                              vs -> video_info -> codec_ctx2 -> channels, 1);
		if(!audio_fifo)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}

		if(!decode_audio_to_fifo(vs -> audio_info[i], vs -> video_info, audio_fifo, swr_ctx))
		{
			swr_free(&swr_ctx);
			AVInfo_reopen_input(vs -> audio_info[i]);
			return false;
		}

		if(!encode_audio_from_fifo(vs -> video_info, audio_fifo, &pts))
		{
			swr_free(&swr_ctx);
			AVInfo_reopen_input(vs -> audio_info[i]);
			return false;
		}
		
		swr_free(&swr_ctx);
		av_audio_fifo_free(audio_fifo);
		av_packet_unref(vs -> video_info -> packet);
		AVInfo_reopen_input(vs -> audio_info[i]);
	}
	
	return true;
}

