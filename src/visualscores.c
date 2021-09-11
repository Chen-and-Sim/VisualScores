/** 
 * VisualScores source file: visualscores.c
 * Defines basic operations and the main function.
 */

#include <io.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
 
#include "vslog.h"
#include "visualscores.h"

const char short_command[COMMAND_COUNT][5] =
	{"-a", "-h", "-l", "-q", "-x", "-i", "-I", "-o", "-d", "-m", "-r", "-t", "-p", "-D", "-e"};
const char long_command[COMMAND_COUNT][10] =
	{"about",  "help",   "language", "quit",     "settings",  "load",    "loadall", "loadother",
	 "delete", "modify", "repeat",   "duration", "partition", "discard", "export"};
void (*functions[COMMAND_COUNT]) (VisualScores *, char *) =
	{about, help, switch_language, quit, settings, load, load_all, load_other, delete_file,
	 modify_file, set_repetition, set_duration, partition_audio, discard_partition, export_video};

VisualScores *VS_init()
{
	VisualScores *vs = malloc(sizeof(VisualScores));
	
	vs -> image_count = 0;
	vs -> audio_count = 0;
	vs -> bg_count = 0;
	vs -> image_pos = malloc(sizeof(int) * FILE_LIMIT);

	vs -> image_info = malloc(sizeof(AVInfo*) * FILE_LIMIT);
	vs -> audio_info = malloc(sizeof(AVInfo*) * FILE_LIMIT);
	vs -> bg_info = malloc(sizeof(AVInfo*) * FILE_LIMIT);

	if(vs -> bg_info == NULL)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
	return vs;
}

void VS_free(VisualScores *vs)
{
	free(vs -> image_pos);
	
	for(int i = 0; i < vs -> image_count; ++i)
		AVInfo_free(vs -> image_info[i]);
	free(vs -> image_info);

	for(int i = 0; i < vs -> audio_count; ++i)
		AVInfo_free(vs -> audio_info[i]);
	free(vs -> audio_info);

	for(int i = 0; i < vs -> bg_count; ++i)
		AVInfo_free(vs -> bg_info[i]);
	free(vs -> bg_info);

	free(vs);
}

void about(VisualScores *vs, char *cmd)
{
	VS_print_log(ABOUT);
}

void help(VisualScores *vs, char *cmd)
{
	if(language == English)
	{
		printf("\nAvailable commands: (<>: necessary arguments; []: optional arguments)\n"
				 "-a  about      Show the information of the program.\n"
				 "-h  help       Show help.\n"
				 "-l  language   Switch the language of the program.\n"
				 "-q  quit       Quit the program.\n"
				 "-x  settings   Show current settings.\n\n"
				 "-i <Path> [Pos]            load <Path> [Pos]\n"
				 "    Load an image to the image track. \n"
				 "-I <Path> [Pos]            loadall <Path> [Pos]\n"
				 "    Load all images in the folder located at <Path> to the image track.\n"
				 "-o <Path> [Begin] [End]    loadother <Path> [Begin] [End]\n"
				 "    Load an image to the background track or load an audio file to the\n"
				 "    audio track.\n"
				 "-d <Tag>                   delete <Tag>\n"
				 "    Delete the file tagged <Tag>.\n"
				 "-m <Tag> <...>             modify <Tag> <...>\n"
				 "    Modify the file tagged <Tag>.\n"
				 "-r <Begin> <End> <Times>   repeat <Begin> <End> <Times>\n"
				 "    Set the number of repetition times of images from <Begin> to <End> in\n"
				 "    the image track to <Times> times.\n"
				 "-t <Tag> <Time>            duration <Tag> <Time>\n"
				 "    Set the duration of an image file not in the range of any audio file.\n\n"
				 "-p [Tag]                   partition [Tag]\n"
				 "    Partition the audio file tagged [Tag] and determine the duration of\n"
				 "    images in the range of the audio file.\n"
				 "-D <Tag>                   discard <Tag>\n"
				 "    Discard the partition done to the audio file tagged <Tag>.\n"
				 "-e [Path]                  export [Path]\n"
				 "    Export the video file to [Path]. \n\n"
				 "For detailed descriptions please refer to the user manual.\n\n");
	}
	else
	{
		printf("\n可用的指令：（<>：必要参数；[]：可选参数）\n"
				 "-a  about      显示程序信息。\n"
				 "-h  help       显示帮助。\n"
				 "-l  language   切换程序语言。\n"
				 "-q  quit       结束程序。\n"
				 "-x  settings   显示当前设置。\n\n"
				 "-i <Path> [Pos]            load <Path> [Pos]\n"
				 "    载入图片至图片轨。\n"
				 "-I <Path> [Pos]            loadall <Path> [Pos]\n"
				 "    载入路径为 [Path] 的文件夹内的所有图片至图片轨。\n"
				 "-o <Path> [Begin] [End]    loadother <Path> [Begin] [End]\n"
				 "    载入图片至背景轨或载入音频至音频轨。\n"
				 "-d <Tag>                   delete <Tag>\n"
				 "    删除标签为 <Tag> 的文件。\n"
				 "-m <Tag> <...>             modify <Tag> <...>\n"
				 "    修改标签为 <Tag> 的文件。\n"
				 "-r <Begin> <End> <Times>   repeat <Begin> <End> <Times>\n"
				 "    将 <Begin> 至 <End> 范围内的图片设置重复次数 <Times> 次。\n"
				 "-t <Tag> <Time>            duration <Tag> <Time>\n"
				 "    设置不在任何一个音频范围内的图片的时长。\n\n"
				 "-p [Tag]                   partition [Tag]\n"
				 "    划分标签为 [Tag] 的音频文件以决定此音频范围内的图片的时长。\n"
				 "-D <Tag>                   discard <Tag>\n"
				 "    撤销对标签为 <Tag> 的音频文件所做的划分。\n"
				 "-e [Path]                  export [Path]\n"
				 "    导出视频文件至 [Path]。\n\n"
				 "请参阅用户手册以获取详细描述。\n\n");
	}
}

void switch_language(VisualScores *vs, char *cmd)
{
	if(language == English)
		language = Chinese;
	else
		language = English;
	VS_print_log(SWITCH_LANGUAGE);
}

void quit(VisualScores *vs, char *cmd)
{
	VS_free(vs);
	exit(0);
}

void settings(VisualScores *vs, char *cmd)
{
	if(muted)  return;
	
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	VS_print_log(IMAGE_TRACK_HEAD);
	for(int i = 0; i < vs -> image_count; ++i)
	{
		int pos = vs -> image_pos[i];
		VS_print_log(TAG_AND_FILENAME, "I", i + 1, vs -> image_info[pos] -> filename);
		double duration = vs -> image_info[pos] -> duration;
		bool in_range_of_audio = (duration < 0);
		if(in_range_of_audio)
			VS_print_log(DURATION_N_A);
		else
			VS_print_log(DURATION, duration);
	}

	if(vs -> audio_count != 0)
	{
		VS_print_log(AUDIO_TRACK_HEAD);
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			VS_print_log(TAG_AND_FILENAME, "A", i + 1, vs -> audio_info[i] -> filename);
			VS_print_log(BEGIN_AND_END, vs -> audio_info[i] -> begin, vs -> audio_info[i] -> end);
		}
	}
	
	if(vs -> bg_count != 0)
	{
		VS_print_log(BG_IMAGE_TRACK_HEAD);
		for(int i = 0; i < vs -> bg_count; ++i)
		{
			VS_print_log(TAG_AND_FILENAME, "BG", i + 1, vs -> bg_info[i] -> filename);
			VS_print_log(BEGIN_AND_END, vs -> bg_info[i] -> begin, vs -> bg_info[i] -> end);
		}
	}
	printf("\n");
}

int main()
{
	int screen_w = GetSystemMetrics(SM_CXSCREEN);
	int screen_h = GetSystemMetrics(SM_CYSCREEN);
	SetWindowPos(GetConsoleWindow(), HWND_TOP, screen_w * 0.05, screen_h * 0.25,
	             screen_w * 0.5, screen_h * 0.5, 0);
	HANDLE hIcon = LoadImage((HINSTANCE)GetModuleHandle(NULL), "resource\\visualscores.ico",
	                         IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
	SendMessage(GetConsoleWindow(), WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
	SendMessage(GetConsoleWindow(), WM_SETICON, ICON_BIG, (LPARAM)hIcon);
	
	WNDCLASSEX wc = {
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = DefWindowProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = (HINSTANCE)GetModuleHandle(NULL),
		.hIcon = hIcon,
		.hCursor = LoadCursor(0, IDC_ARROW),
		.hbrBackground = (HBRUSH)COLOR_WINDOW,
		.lpszMenuName = NULL,
		.lpszClassName = "Preview",
		.hIconSm = NULL
	};
	RegisterClassEx(&wc);

	av_log_set_level(AV_LOG_QUIET);
	VisualScores *vs = VS_init();

	char null[1] = "";
	about(vs, null);  help(vs, null);

	while(1)
	{
		char str[STRING_LIMIT], former_part[20], latter_part[STRING_LIMIT];
		printf("VisualScores> ");
		fgets(str, STRING_LIMIT, stdin);
		if(str[strlen(str) - 1] == '\n')
			str[strlen(str) - 1] = '\0';

		char *pch = strchr(str, ' ');
		if(pch == NULL)
		{
			strcpy(former_part, str);
			strcpy(latter_part, "\0");
		}
		else
		{
			int len = pch - str;
			strncpy(former_part, str, len);
			former_part[len] = '\0';
			while(*pch == ' ') 
				++pch;
			strcpy(latter_part, pch);
		}

		int i;
		for(i = 0; i < COMMAND_COUNT; ++i)
		{
			if(strcmp(former_part, short_command[i]) == 0 || strcmp(former_part, long_command[i]) == 0 )
			{
				(*functions[i]) (vs, latter_part);
				break;
			}
		}
		if(i == COMMAND_COUNT && str[0] != '\0')
			VS_print_log(WRONG_COMMAND);
	}
	return 0;
}

