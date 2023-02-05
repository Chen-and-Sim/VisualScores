/** 
 * VisualScores source file: visualscores.c
 * Defines basic operations and the main function.
 */

#include <io.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
 
#include "vslog.h"
#include "visualscores.h"

const wchar_t short_command[COMMAND_COUNT][5] =
	{L"-a", L"-h", L"-l", L"-q", L"-x", L"-i", L"-I", L"-o", L"-d", L"-m", L"-r", L"-t", L"-p", L"-D", L"-e"};
const wchar_t long_command[COMMAND_COUNT][10] =
	{L"about",  L"help",   L"language", L"quit",     L"settings",  L"load",    L"loadall", L"loadother",
	 L"delete", L"modify", L"repeat",   L"duration", L"partition", L"discard", L"export"};
void (*functions[COMMAND_COUNT]) (VisualScores *, wchar_t *) =
	{about, help, switch_language, quit, settings, load, load_all, load_other, delete_file,
	 modify_file, set_repetition, set_duration, partition_audio, discard_partition, export_video};

VisualScores *VS_init()
{
	VisualScores *vs = malloc(sizeof(VisualScores));
	if(vs == NULL)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}
	
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

void about(VisualScores *vs, wchar_t *cmd)
{
	VS_print_log(ABOUT);
}

void help(VisualScores *vs, wchar_t *cmd)
{
	if(language == English)
	{
		wprintf(L"\nAvailable commands: (<>: necessary arguments; []: optional arguments)\n"
		         "-a  about      Show the information of the program.\n"
		         "-h  help       Show help.\n"
		         "-l  language   Toggle the language of the program.\n"
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
		wprintf(L"\n可用的指令：（<>：必要参数；[]：可选参数）\n"
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

void switch_language(VisualScores *vs, wchar_t *cmd)
{
	if(language == English)
		language = Chinese;
	else
		language = English;
	VS_print_log(TOGGLE_LANGUAGE);
}

void quit(VisualScores *vs, wchar_t *cmd)
{
	VS_free(vs);
	exit(0);
}

void settings(VisualScores *vs, wchar_t *cmd)
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
		VS_print_log(TAG_AND_FILENAME, L"I", i + 1, vs -> image_info[pos] -> filename);

		int in_range_of_audio = -1;
		for(int j = 0; j < vs -> audio_count; ++j)
			if(vs -> audio_info[j] -> begin <= i + 1 && vs -> audio_info[j] -> end >= i + 1)
				in_range_of_audio = j;

		double duration = vs -> image_info[pos] -> duration[0];
		if(in_range_of_audio >= 0)
		{
			if(duration > 0)
				VS_print_log(SETTINGS_DURATION_SET);
			else
				VS_print_log(SETTINGS_DURATION_N_A);
		}
		else
			VS_print_log(SETTINGS_DURATION, duration);
	}

	if(vs -> audio_count != 0)
	{
		VS_print_log(AUDIO_TRACK_HEAD);
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			VS_print_log(TAG_AND_FILENAME, L"A", i + 1, vs -> audio_info[i] -> filename);
			VS_print_log(BEGIN_AND_END, vs -> audio_info[i] -> begin, vs -> audio_info[i] -> end);
		}
	}
	
	if(vs -> bg_count != 0)
	{
		VS_print_log(BG_IMAGE_TRACK_HEAD);
		for(int i = 0; i < vs -> bg_count; ++i)
		{
			VS_print_log(TAG_AND_FILENAME, L"BG", i + 1, vs -> bg_info[i] -> filename);
			VS_print_log(BEGIN_AND_END, vs -> bg_info[i] -> begin, vs -> bg_info[i] -> end);
		}
	}

	bool first_time = true;
	for(int i = 0; i < vs -> image_count; ++i)
	{
		int begin, end, nb_repetition;
		if(vs -> image_info[vs -> image_pos[i]] -> nb_repetition < 0)
		{
			end = i;
			nb_repetition = vs -> image_info[vs -> image_pos[i]] -> nb_repetition * (-1);
			for(begin = i - 1; begin >= 0; --begin)
			{
				if(vs -> image_info[vs -> image_pos[begin]] -> nb_repetition <= 0)
					break;
			}
			++begin;

			if(first_time)
				VS_print_log(REPETITION_HEAD);
			VS_print_log(SETTINGS_REPETITION, begin + 1, end + 1, nb_repetition);
			i = end;
			first_time = false;
		}
	}
	
	wprintf(L"\n");
}

int fill_index(VisualScores *vs, int begin, int end, int *rec_index)
{
	int index = begin, size = 0;
	int repeat_begin, repeat_end, nb_repetition;
	while(index <= end)
	{
		if(vs -> image_info[vs -> image_pos[index]] -> nb_repetition == 0)
		{
			rec_index[size] = index;
			++index;  ++size;
			continue;
		}

		repeat_begin = index;
		int j;
		for(j = index; j <= end; ++j)
		{
			if(vs -> image_info[vs -> image_pos[j]] -> nb_repetition < 0)
				break;
		}
		if(j <= end)  repeat_end = j;
		else          repeat_end = end;
		nb_repetition = abs(vs -> image_info[vs -> image_pos[repeat_end]] -> nb_repetition);

		for(int i = 0; i <= nb_repetition; ++i)
		{
			for(j = repeat_begin; j <= repeat_end; ++j)
			{
				rec_index[size] = j;
				++size;
			}
		}
		index = repeat_end + 1;
	}
	return size;
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

	setlocale(LC_ALL, "");
	av_log_set_level(AV_LOG_QUIET);
	VisualScores *vs = VS_init();

	wchar_t null[1] = L"";
	about(vs, null);  help(vs, null);

	while(1)
	{
		wchar_t str[STRING_LIMIT], former_part[20], latter_part[STRING_LIMIT];
		printf("VisualScores> "); fflush(stdout);
		fgetws(str, STRING_LIMIT, stdin);
		if(str[wcslen(str) - 1] == L'\n')
			str[wcslen(str) - 1] = L'\0';

		int begin = 0;
		while(str[begin] == L' ')  ++begin;
		wcscpy_s(str, STRING_LIMIT, str + begin);

		wchar_t *pwc = wcschr(str, L' ');
		if(pwc == NULL)
		{
			wcscpy_s(former_part, 20, str);
			wcscpy_s(latter_part, STRING_LIMIT, L"\0");
		}
		else
		{
			int len = pwc - str;
			wcsncpy_s(former_part, 20, str, len);
			former_part[len] = L'\0';
			while(*pwc == L' ')
				++pwc;
			wcscpy_s(latter_part, STRING_LIMIT, pwc);
		}

		int i;
		for(i = 0; i < COMMAND_COUNT; ++i)
		{
			if(wcscmp(former_part, short_command[i]) == 0 || wcscmp(former_part, long_command[i]) == 0 )
			{
				(*functions[i]) (vs, latter_part);
				break;
			}
		}
		if(i == COMMAND_COUNT && str[0] != L'\0')
			VS_print_log(WRONG_COMMAND);
	}
	return 0;
}

