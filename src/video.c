/** 
 * VisualScores source file: video.c
 * Defines functions which deal with exporting video.
 */

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

#include "vslog.h"
#include "visualscores.h"

/* Add more extensions here later. */
bool has_video_ext(char *filename)
{
	char *ext = strrchr(filename, '.');
	if(ext == NULL)
		return false;
	else  ++ext;

	const char video_ext[1][5] = {"mp4"};
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
/* Temporary added because now we only support .mp4 files. */
	char *pch = strrchr(dest, '.');
	if(pch == NULL)
		strcat(dest, ".mp4");
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
	/* It seems like swscale demands that width and height of the video file should be divisible by 4. */
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
	double total_time_to_prev_image = 0.0, total_time_to_cur_image = 0.0;
	for(int i = 0; i < vs -> image_count; ++i)
	{
		VS_print_log(WRITING_IMAGE_TRACK, i + 1, vs -> image_count);

		AVInfo *image_info = vs -> image_info[vs -> image_pos[i]];
		AVFrame *image_frame = av_frame_alloc();
		if(!decode_image(image_info, image_frame, vs -> video_info -> width, vs -> video_info -> height))
		{
			av_frame_free(&image_frame);
			AVInfo_reopen_input(image_info);
			return false;
		}

		if(!mix_images(image_info, vs -> bg_info, image_frame, vs -> video_info -> frame,
		               vs -> image_pos[i], vs -> bg_count))
		{
			av_frame_free(&image_frame);
			AVInfo_reopen_input(image_info);
			return false;
		}
		av_frame_free(&image_frame);

		total_time_to_cur_image += image_info -> duration;
		int nb_frames = (double)(total_time_to_cur_image - total_time_to_prev_image) * VS_framerate;
		int64_t begin_pts = total_time_to_prev_image * vs -> video_info -> codec_ctx2 -> time_base.den;
		if(!encode_image(vs -> video_info, begin_pts, nb_frames))
		{
			AVInfo_reopen_input(image_info);
			return false;
		}

		total_time_to_prev_image += (nb_frames / VS_framerate);
		av_frame_unref(vs -> video_info -> frame);
		AVInfo_reopen_input(image_info);
	}
	return true;
}

bool write_audio_track(VisualScores *vs)
{
	if(vs -> audio_count > 0)
		printf("\n");

	int prev_min_begin = -1, min_begin = FILE_LIMIT, index = -1;
	int64_t pts = 0, prev_end_pts = 0;
	for(int i = 0; i < vs -> audio_count; ++i)
	{
		VS_print_log(WRITING_AUDIO_TRACK, i + 1, vs -> audio_count);

		min_begin = FILE_LIMIT;
		for(int j = 0; j < vs -> audio_count; ++j)
		{
			int begin = vs -> audio_info[j] -> begin;
			if(begin < min_begin && begin > prev_min_begin)
			{
				min_begin = begin;
				index = j;
			}
		}
		prev_min_begin = min_begin;

		double begin_time = 0.0;
		for(int j = 0; j < vs -> audio_info[index] -> begin - 1; ++j)
			begin_time += vs -> image_info[vs -> image_pos[j]] -> duration;
		pts = begin_time * vs -> video_info -> codec_ctx -> time_base.den;

		if(pts > prev_end_pts)
		{
			/* This may bring up to 21.3 ms of error -- acceptable. */
			int nb_frames = (pts - prev_end_pts - 1) / 1024 + 1;
			if(!write_blank_audio(vs -> video_info, prev_end_pts, nb_frames))
				return false;
		}
		
		AVAudioFifo *audio_fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 
		                                              vs -> video_info -> codec_ctx -> channels, 1);
		if(!audio_fifo)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}

		if(!decode_audio_to_fifo(vs -> audio_info[index], vs -> video_info, audio_fifo, AV_SAMPLE_FMT_FLTP))
		{
			AVInfo_reopen_input(vs -> audio_info[index]);
			return false;
		}

		prev_end_pts = encode_audio_from_fifo(vs -> video_info, audio_fifo, pts, AV_SAMPLE_FMT_FLTP);
		if(prev_end_pts == 0)
		{
			AVInfo_reopen_input(vs -> audio_info[index]);
			return false;
		}

		av_audio_fifo_free(audio_fifo);
		av_packet_unref(vs -> video_info -> packet);
		AVInfo_reopen_input(vs -> audio_info[index]);
	}
	
	return true;
}

