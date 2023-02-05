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
bool has_video_ext(wchar_t *filename)
{
	wchar_t *ext = wcsrchr(filename, '.');
	if(ext == NULL)
		return false;
	else  ++ext;

	const wchar_t video_ext[3][5] = {L"mp4", L"mov", L"avi"};
	for(int i = 0; i < 3; ++i)
		if(wcscmp(ext, video_ext[i]) == 0)
			return true;
	return false;
}

void get_video_filename(wchar_t *dest, wchar_t *src)
{
	if(src[0] == '\0')
	{
		wcscpy_s(dest, STRING_LIMIT, L".\\video.mp4");
		return;
	}

	wchar_t first[8] = L"\0\0\0\0\0\0\0\0";
	for(int i = 0; i < 7; ++i)
	{
		if(src[i] >= 'A' && src[i] <= 'Z')
			first[i] = src[i] - 'A' + 'a';
		else  first[i] = src[i];
	}
	
	if(wcscmp(first, L"desktop") == 0)
	{
		wchar_t desktop_path[STRING_LIMIT];
		SHGetSpecialFolderPathW(0, desktop_path, CSIDL_DESKTOPDIRECTORY, 0);
	
		wchar_t *ch = desktop_path + wcslen(desktop_path) - 1;
		if(*ch == L'\\' || *ch == L'/')  *ch = L'\0';
		swprintf_s(dest, STRING_LIMIT, L"%ls%ls", desktop_path, src + 7);
	}
	else  wcscpy_s(dest, STRING_LIMIT, src);

	wchar_t last = dest[wcslen(dest) - 1];
	if(last == L'/' || last == L'\\')
		wcscat(dest, L"video.mp4");
	wchar_t *pwc = wcsrchr(dest, '.');
	if(pwc == NULL)
		wcscat_s(dest, STRING_LIMIT, L".mp4");
}

void export_video(VisualScores *vs, wchar_t *cmd)
{
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	double total_time = 0;
	for(int i = 0; i < vs -> image_count; ++i)
	{
		if(vs -> image_info[vs -> image_pos[i]] -> duration[0] <= 0)
		{
			VS_print_log(DURATION_NOT_SET, i + 1);
			return;
		}
		for(int j = 0; j <= vs -> image_info[vs -> image_pos[i]] -> nb_repetition; ++j)
			total_time += vs -> image_info[vs -> image_pos[i]] -> duration[j];
	}
	if(total_time > TIME_LIMIT)
		VS_print_log(TIME_LIMIT_EXCEEDED);
	
	wchar_t filename[STRING_LIMIT];
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
	
	double scaling = FFMIN(1920.5, FFMAX(1280.0, (double)width)) / (double)width;
	width = width * scaling;  /* In the range of 1280 to 1920. */
	height = height * scaling;
	/* It seems like swscale demands that width and height of the video file should be divisible by 4. */
	width = (width + 2) / 4 * 4;
	height = (height + 2) / 4 * 4;
	
	vs -> video_info = AVInfo_init();
	AVInfo_open(vs -> video_info, filename, AVTYPE_VIDEO, -1, -1, width, height);
	vs -> video_info -> fmt_ctx -> duration = (int64_t)(total_time * 1E6);

	bool avio_opened = (!(vs -> video_info -> fmt_ctx -> oformat -> flags & AVFMT_NOFILE));
	if(avio_opened && (avio_open(&vs -> video_info -> fmt_ctx -> pb,
	                              vs -> video_info -> filename_utf8, AVIO_FLAG_WRITE) < 0))
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
	int rec_index[FILE_LIMIT];
	int size = fill_index(vs, 0, vs -> image_count - 1, rec_index);
	int repeated[FILE_LIMIT];
	for(int i = 0; i < FILE_LIMIT; ++i)
		repeated[i] = 0;
	double total_time_to_prev_image = 0.0, total_time_to_cur_image = 0.0;
	int64_t begin_pts = 0;
	
	for(int i = 0; i < size; ++i)
	{
		VS_print_log(WRITING_IMAGE_TRACK, i + 1, size);

		AVInfo *image_info = vs -> image_info[vs -> image_pos[ rec_index[i] ]];
		AVFrame *image_frame = av_frame_alloc();
		if(!decode_image(image_info, image_frame, vs -> video_info -> width, vs -> video_info -> height))
		{
			av_frame_free(&image_frame);
			AVInfo_reopen_input(image_info);
			return false;
		}

		if(!mix_images(image_info, vs -> bg_info, image_frame, vs -> video_info -> frame,
		               vs -> image_pos[rec_index[i]], vs -> bg_count))
		{
			av_frame_free(&image_frame);
			AVInfo_reopen_input(image_info);
			return false;
		}
		av_frame_free(&image_frame);

		total_time_to_cur_image += image_info -> duration[ repeated[rec_index[i]] ];
		++repeated[rec_index[i]];
		int nb_frames = (double)(total_time_to_cur_image - total_time_to_prev_image) * VS_framerate;
		if(!encode_image(vs -> video_info, begin_pts, nb_frames))
		{
			AVInfo_reopen_input(image_info);
			return false;
		}

		begin_pts += av_rescale_q(nb_frames, vs -> video_info -> codec_ctx2 -> time_base, 
		                                     vs -> video_info -> fmt_ctx -> streams[1] -> time_base);
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
	int64_t pts_from_dur = 0, pts_actual = 0;
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
		{
			for(int k = 0; k <= vs -> image_info[vs -> image_pos[j]] -> nb_repetition; ++k)
				begin_time += vs -> image_info[vs -> image_pos[j]] -> duration[k];
		}
		pts_from_dur = begin_time * vs -> video_info -> codec_ctx -> time_base.den;

		if(pts_from_dur > pts_actual)
		{
			/* This may bring up to 13.1 ms of error -- acceptable. */
			int nb_frames = (pts_from_dur - pts_actual + vs -> video_info -> frame_size / 2) / vs -> video_info -> frame_size;
			if(!write_blank_audio(vs -> video_info, pts_actual, nb_frames))
				return false;
			pts_actual += (int64_t)vs -> video_info -> frame_size * nb_frames;
		}
		
		AVAudioFifo *audio_fifo = av_audio_fifo_alloc(vs -> video_info -> codec_ctx -> sample_fmt, 
		                                              vs -> video_info -> codec_ctx -> ch_layout.nb_channels, 1);
		if(!audio_fifo)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}

		if(!decode_audio_to_fifo(vs -> audio_info[index], vs -> video_info, 
								 audio_fifo, vs -> video_info -> codec_ctx -> sample_fmt))
		{
			AVInfo_reopen_input(vs -> audio_info[index]);
			return false;
		}

		pts_actual = encode_audio_from_fifo(vs -> video_info, audio_fifo,
		                                    pts_actual, vs -> video_info -> codec_ctx -> sample_fmt);
		if(pts_actual == 0)
		{
			av_audio_fifo_free(audio_fifo);
			AVInfo_reopen_input(vs -> audio_info[index]);
			return false;
		}

		av_audio_fifo_free(audio_fifo);
		av_packet_unref(vs -> video_info -> packet);
		AVInfo_reopen_input(vs -> audio_info[index]);
	}
	
	return true;
}

