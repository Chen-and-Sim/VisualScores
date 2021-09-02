#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shlobj.h>

#include "vslog.h"
#include "visualscores.h"

bool has_video_ext(char *filename)
{
	char *ext = strrchr(filename, '.');
	if(ext == NULL)
		return false;
	else  ++ext;

	const char video_ext[4][5] = {"avi", "mjpeg", "mp4", "wmv"};
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
	
	
}
