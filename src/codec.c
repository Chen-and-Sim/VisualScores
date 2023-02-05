/** 
 * VisualScores source file: codec.c
 * Defines functions mainly dealing with format conversion.
 */

#include <stdbool.h>
#include <windows.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "avinfo.h"
#include "vslog.h"

bool AVInfo_create_bmp(AVInfo *av_info)
{
	int screen_w = GetSystemMetrics(SM_CXSCREEN);
	int display_window_w = screen_w * 0.45;
	int display_window_h = screen_w * 0.3;
	double scaling_w = (double) display_window_w / av_info -> width;
	double scaling_h = (double) display_window_h / av_info -> height;
	double scaling = ((scaling_w < scaling_h) ? scaling_w: scaling_h);
	int width  = av_info -> width * scaling;
	int height = av_info -> height * scaling;

	AVInfo* bmp_info = AVInfo_init();
	if(!AVInfo_open(bmp_info, av_info -> bmp_filename, AVTYPE_BMP, -1, -1, width, height))
	{
		free(bmp_info);
		return false;
	}

	if(avformat_write_header(bmp_info -> fmt_ctx, NULL) < 0)
	{
		AVInfo_free(bmp_info);
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
		AVInfo_free(bmp_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	while(true)
	{
		int ret1 = avcodec_send_packet(av_info -> codec_ctx, av_info -> packet) < 0;
		if(ret1 < 0 && ret1 != AVERROR(EAGAIN) && ret1 != AVERROR_EOF)
		{
			AVInfo_free(bmp_info);
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
			AVInfo_free(bmp_info);
			AVInfo_reopen_input(av_info);
			return false;
		}
	}
	
	sws_ctx = sws_getCachedContext(sws_ctx, av_info -> frame -> width, av_info -> frame -> height,
	                               (enum AVPixelFormat)av_info -> frame -> format,
	                               bmp_info -> width, bmp_info -> height,
	                               AV_PIX_FMT_BGRA, SWS_LANCZOS, 0, 0, 0);
	if(!sws_ctx)
	{
		AVInfo_free(bmp_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	sws_scale(sws_ctx, (const uint8_t * const *)av_info -> frame -> data,
	          av_info -> frame -> linesize, 0, av_info -> frame -> height,
	          (uint8_t * const *)bmp_info -> frame -> data, bmp_info -> frame -> linesize);

	while(true)
	{
		int ret3 = avcodec_send_frame(bmp_info -> codec_ctx, bmp_info -> frame);
		if(ret3 < 0 && ret3 != AVERROR(EAGAIN) && ret3 != AVERROR_EOF)
		{
			AVInfo_free(bmp_info);
			sws_freeContext(sws_ctx);
			AVInfo_reopen_input(av_info);
			return false;
		}

		int ret4 = avcodec_receive_packet(bmp_info -> codec_ctx, bmp_info -> packet);
		if(ret4 == AVERROR(EAGAIN))
			continue;
		else if(ret4 == AVERROR_EOF || ret4 == 0)
			break;
		else if(ret4 < 0)
		{
			AVInfo_free(bmp_info);
			sws_freeContext(sws_ctx);
			AVInfo_reopen_input(av_info);
			return false;
		}
	}

	sws_freeContext(sws_ctx);
	bmp_info -> packet -> stream_index = 0;
	bmp_info -> packet -> pts = AV_NOPTS_VALUE;
	bmp_info -> packet -> dts = AV_NOPTS_VALUE;
	bmp_info -> packet -> pos = -1;

	if(av_write_frame(bmp_info -> fmt_ctx, bmp_info -> packet) < 0)
	{
		AVInfo_free(bmp_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	if(av_write_trailer(bmp_info -> fmt_ctx) < 0)
	{
		AVInfo_free(bmp_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	AVInfo_free(bmp_info);
	AVInfo_reopen_input(av_info);
	return true;
}

bool AVInfo_create_wav(AVInfo *av_info)
{
	AVInfo* wav_info = AVInfo_init();
	if(!AVInfo_open(wav_info, L"resource\\audition.wav", AVTYPE_WAV, -1, -1, -1, -1))
	{
		AVInfo_free(wav_info);
		return false;
	}

	bool avio_opened = (!(wav_info -> fmt_ctx -> oformat -> flags & AVFMT_NOFILE));
	if(avio_opened && (avio_open(&wav_info -> fmt_ctx -> pb, wav_info -> filename_utf8, AVIO_FLAG_WRITE) < 0))
	{
		AVInfo_free(wav_info);
		return false;
	}

	if(avformat_write_header(wav_info -> fmt_ctx, NULL) < 0)
	{
		AVInfo_free(wav_info);
		return false;
	}

	AVAudioFifo *audio_fifo = av_audio_fifo_alloc(wav_info -> codec_ctx -> sample_fmt, 
												  wav_info -> codec_ctx -> ch_layout.nb_channels, 1);
	if(!audio_fifo)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(!decode_audio_to_fifo(av_info, wav_info, audio_fifo, wav_info -> codec_ctx -> sample_fmt))
	{
		av_audio_fifo_free(audio_fifo);
		AVInfo_free(wav_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	if(encode_audio_from_fifo(wav_info, audio_fifo, 0, wav_info -> codec_ctx -> sample_fmt) == 0)
	{
		av_audio_fifo_free(audio_fifo);
		AVInfo_free(wav_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	if(av_write_trailer(wav_info -> fmt_ctx) < 0)
	{
		av_audio_fifo_free(audio_fifo);
		AVInfo_free(wav_info);
		AVInfo_reopen_input(av_info);
		return false;
	}

	av_audio_fifo_free(audio_fifo);
	AVInfo_reopen_input(av_info);
	AVInfo_free(wav_info);
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
/*
	while(true)
	{
		int ret1 = avcodec_send_packet(image_info -> codec_ctx, image_info -> packet);
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
*/

	if(avcodec_send_packet(image_info -> codec_ctx, image_info -> packet) < 0)
		return false;
	if(avcodec_receive_frame(image_info -> codec_ctx, image_info -> frame) < 0)
		return false;

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
			int ret3 = avcodec_send_frame(video_info -> codec_ctx2, video_info -> frame);
			int ret4 = 0;
			if(ret3 < 0 && ret3 != AVERROR(EAGAIN) && ret3 != AVERROR_EOF)
				return false;
			else if(ret3 == AVERROR_EOF)
			{
				do {
					ret4 = avcodec_receive_packet(video_info -> codec_ctx2, NULL);
				} while(ret4 != AVERROR_EOF);
				break;
			}
			else
			{
				ret4 = avcodec_receive_packet(video_info -> codec_ctx2, video_info -> packet);
				if(ret4 == 0 || ret4 == AVERROR_EOF)
					break;
				else if(ret4 == AVERROR(EAGAIN))
					continue;
				else  return false;
			}
		}

		AVPacket *packet_copy = av_packet_alloc();
		if(av_packet_ref(packet_copy, video_info -> packet) < 0)
			return false;

		packet_copy -> stream_index = 1;
		packet_copy -> duration = av_rescale_q(1, video_info -> codec_ctx2 -> time_base, 
												  video_info -> fmt_ctx -> streams[1] -> time_base);
		packet_copy -> pos = -1;
		packet_copy -> pts = begin_pts + av_rescale_q(frame, video_info -> codec_ctx2 -> time_base, 
													         video_info -> fmt_ctx -> streams[1] -> time_base);
		packet_copy -> dts = packet_copy -> pts;

		if(av_write_frame(video_info -> fmt_ctx, packet_copy) < 0)
			return false;
		av_packet_unref(packet_copy);
		av_packet_free(&packet_copy);
	}
	return true;
}

bool write_blank_audio(AVInfo *video_info, int64_t begin_pts, int nb_frames)
{
	bool is_avi = false;
	char *ext3 = video_info -> filename_utf8 + strlen(video_info -> filename_utf8) - 3;
	if(strcmp(ext3, "avi") == 0)
		is_avi = true;

	AVInfo *blank_audio_info = AVInfo_init();
	bool ret;
	if(is_avi)
		ret = AVInfo_open(blank_audio_info, L"resource\\blank.mp3", AVTYPE_AUDIO, -1, -1, -1, -1);
	else  ret = AVInfo_open(blank_audio_info, L"resource\\blank.aac", AVTYPE_AUDIO, -1, -1, -1, -1);
	if(!ret)
	{
		AVInfo_free(blank_audio_info);
		return false;
	}

	if(av_read_frame(blank_audio_info -> fmt_ctx, blank_audio_info -> packet) < 0)
	{
		AVInfo_free(blank_audio_info);
		return false;
	}

	int64_t pts = begin_pts;
	for(int frame = 0; frame < nb_frames; ++frame)
	{
		blank_audio_info -> packet -> stream_index = 0;
		blank_audio_info -> packet -> pts = pts;
		blank_audio_info -> packet -> dts = pts;
		blank_audio_info -> packet -> duration = (is_avi ? MP3_framesize : AAC_framesize);
		blank_audio_info -> packet -> pos = -1;
		pts += (is_avi ? MP3_framesize : AAC_framesize);

		if(av_write_frame(video_info -> fmt_ctx, blank_audio_info -> packet) < 0)
			return false;
	}

	AVInfo_free(blank_audio_info);
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
                          AVAudioFifo *audio_fifo, enum AVSampleFormat fmt)
{
	struct SwrContext *swr_ctx = swr_alloc();
	if(!swr_ctx)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	bool first_time = true, finished_reading = false;
	int ret;
	while(true)
	{
		ret = av_read_frame(audio_info -> fmt_ctx, audio_info -> packet);
		if(ret == AVERROR_EOF)
			finished_reading = true;
		else if(ret < 0)
		{
			av_audio_fifo_free(audio_fifo);
			swr_free(&swr_ctx);
			return false;
		}

		while(true)
		{
			ret = avcodec_send_packet(audio_info -> codec_ctx, audio_info -> packet) < 0;
			if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			{
				av_audio_fifo_free(audio_fifo);
				swr_free(&swr_ctx);
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
				swr_free(&swr_ctx);
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
					swr_free(&swr_ctx);
					return false;
				}
			}		
			break;
		}

		av_frame_unref(video_info -> frame);
		video_info -> frame -> ch_layout = audio_info -> frame -> ch_layout;
		video_info -> frame -> format = fmt;
		video_info -> frame -> sample_rate = video_info -> codec_ctx -> sample_rate;
		video_info -> frame -> nb_samples = av_rescale_rnd(audio_info -> frame -> nb_samples, 
														   video_info -> frame -> sample_rate,
		                                                   audio_info -> frame -> sample_rate, AV_ROUND_UP);
		av_channel_layout_default(&video_info -> frame -> ch_layout, 2);

		ret = av_frame_get_buffer(video_info -> frame, 0);
		if(ret < 0)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}

		if(first_time)
		{
			av_channel_layout_default(&audio_info -> frame -> ch_layout, 2);
			ret = swr_alloc_set_opts2(&swr_ctx, &video_info -> frame -> ch_layout, 
									  fmt, video_info -> frame -> sample_rate,
			                          &audio_info -> frame -> ch_layout,
			                          (enum AVSampleFormat)(audio_info -> frame -> format),
			                          audio_info -> frame -> sample_rate, 0, NULL);
			if(ret < 0)
			{
				av_audio_fifo_free(audio_fifo);
				swr_free(&swr_ctx);
				return false;
			}

			if(swr_init(swr_ctx) < 0)
			{
				av_audio_fifo_free(audio_fifo);
				swr_free(&swr_ctx);
				return false;
			}

			if(video_info -> codec_ctx -> frame_size == 0)
				video_info -> codec_ctx -> frame_size = video_info -> frame_size;
			first_time = false;
		}

		ret = swr_convert(swr_ctx, (uint8_t **)video_info -> frame -> data, video_info -> frame -> nb_samples,
		                     (const uint8_t **)audio_info -> frame -> data, audio_info -> frame -> nb_samples);
		if(ret < 0)
		{
			av_audio_fifo_free(audio_fifo);
			swr_free(&swr_ctx);
			return false;
		}
		video_info -> frame -> nb_samples = ret;

		if(!AVInfo_write_to_fifo(audio_fifo, video_info -> frame))
		{
			av_audio_fifo_free(audio_fifo);
			swr_free(&swr_ctx);
			return false;
		}	
	}

	swr_free(&swr_ctx);
	return true;
}

int64_t encode_audio_from_fifo(AVInfo *video_info, AVAudioFifo *audio_fifo,
                               int64_t begin_pts, enum AVSampleFormat fmt)
{
	int ret;
	int64_t pts = begin_pts;
	bool finished_writing = false;
	while(!finished_writing)
	{
		av_frame_unref(video_info -> frame);
		video_info -> frame -> format = fmt;
		video_info -> frame -> sample_rate = video_info -> codec_ctx -> sample_rate;
		video_info -> frame -> nb_samples = video_info -> codec_ctx -> frame_size;
		av_channel_layout_default(&video_info -> frame -> ch_layout, 2);

		ret = av_frame_get_buffer(video_info -> frame, 0);
		if(ret < 0)
		{
			VS_print_log(INSUFFICIENT_MEMORY);
			system("pause >nul 2>&1");
			abort();
		}

		ret = av_audio_fifo_read(audio_fifo, (void **)video_info -> frame -> data,
		                         video_info -> codec_ctx -> frame_size);
		if(ret < 0)
		{
			av_audio_fifo_free(audio_fifo);
			return 0;
		}
		if(av_audio_fifo_size(audio_fifo) == 0)
			finished_writing = true;
		video_info -> frame -> nb_samples = ret;

		while(true)
		{
			ret = avcodec_send_frame(video_info -> codec_ctx, video_info -> frame);
			if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			{
				av_audio_fifo_free(audio_fifo);
				return 0;
			}

			ret = avcodec_receive_packet(video_info -> codec_ctx, video_info -> packet);
			if(ret == AVERROR(EAGAIN))
				continue;
			else if(ret == AVERROR_EOF || ret == 0)
				break;
			else if(ret < 0)
			{
				av_audio_fifo_free(audio_fifo);
				return 0;
			}
		}
		
		video_info -> packet -> stream_index = 0;
		video_info -> packet -> pts = pts;
		video_info -> packet -> dts = pts;
		video_info -> packet -> duration = video_info -> frame -> nb_samples;
		pts += video_info -> frame -> nb_samples;
		video_info -> packet -> pos = -1;

		ret = av_write_frame(video_info -> fmt_ctx, video_info -> packet);
		if(ret < 0)
		{
			av_audio_fifo_free(audio_fifo);
			return 0;
		}
	}

	ret = av_write_frame(video_info -> fmt_ctx, NULL);
	return pts;
}

bool get_audio_duration(AVInfo *audio_info)
{
	audio_info -> duration[0] = (audio_info -> fmt_ctx -> duration) / 1E6;
	char *ext3 = audio_info -> filename_utf8 + strlen(audio_info -> filename_utf8) - 3;
	if(strcmp(ext3, "aac") != 0)
		return true;

	AVInfo *copy = AVInfo_init();
	AVInfo_open(copy, L"resource\\_temp.mp4", AVTYPE_VIDEO, -1, -1, 120, 120);
	// -1 and 120 are placeholders; we use video type because audio type is used for input

	bool avio_opened = (!(copy -> fmt_ctx -> oformat -> flags & AVFMT_NOFILE));
	if(avio_opened && (avio_open(&copy -> fmt_ctx -> pb, 
	                              copy -> filename_utf8, AVIO_FLAG_WRITE) < 0))
	{
		AVInfo_free(copy);
		system("del resource\\_temp.mp4 >nul 2>&1 ");
		return false;
	}

	if(avformat_write_header(copy -> fmt_ctx, NULL) < 0)
	{
		AVInfo_free(copy);
		system("del resource\\_temp.mp4 >nul 2>&1 ");
		return false;
	}

	AVAudioFifo *audio_fifo = av_audio_fifo_alloc(copy -> codec_ctx -> sample_fmt,
												  copy -> codec_ctx -> ch_layout.nb_channels, 1);
	if(!audio_fifo)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	if(!decode_audio_to_fifo(audio_info, copy, audio_fifo, copy -> codec_ctx -> sample_fmt))
	{
		AVInfo_free(copy);
		system("del resource\\_temp.mp4 >nul 2>&1 ");
		AVInfo_reopen_input(audio_info);
		return false;
	}

	int samples = av_audio_fifo_size(audio_fifo);
	av_audio_fifo_free(audio_fifo);
	AVInfo_free(copy);
	system("del resource\\_temp.mp4 >nul 2>&1 ");
	AVInfo_reopen_input(audio_info);
	audio_info -> duration[0] = (double)samples / (double)VS_samplerate;
	return true;
}