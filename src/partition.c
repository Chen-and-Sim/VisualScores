/** 
 * VisualScores source file: partition.c
 * Defines functions which deal with audio partitioning.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include "vslog.h"
#include "visualscores.h"

void partition_audio(VisualScores *vs, wchar_t *cmd)
{
	if(vs -> audio_count == 0)
	{
		VS_print_log(AUDIO_NOT_LOADED);
		return;
	}
	
	int index;
	bool valid = partition_audio_parse_input(vs, cmd, &index);
	if(!valid)  return;
	
	int begin = vs -> audio_info[index - 1] -> begin;
	int end = vs -> audio_info[index - 1] -> end;
	if(begin == end && vs -> image_info[vs -> image_pos[begin - 1]] -> nb_repetition == 0)
	{
		VS_print_log(NEED_NO_PARTITION);
		return;
	}

	VS_print_log(CREATING_BMP);
	for(int i = begin - 1; i < end; ++i)
	{
		if( !AVInfo_create_bmp(vs -> image_info[vs -> image_pos[i]]) )
		{
			VS_print_log(FAILED_TO_CREATE_BMP);
			system("forfiles /P resource\\ /M _display_*.bmp /C \"cmd /c del @file >nul 2>&1 \"");
			return;
		}
	}
	
	VS_print_log(CREATING_WAV);
	if( !AVInfo_create_wav(vs -> audio_info[index - 1]) )
	{
		VS_print_log(FAILED_TO_CREATE_WAV);
		system("del resource\\audition.wav >nul 2>&1 ");
		return;
	}
	
	int screen_w = GetSystemMetrics(SM_CXSCREEN);
	HWND hWnd = CreateWindow("Preview", "Preview", WS_POPUP | WS_BORDER | WS_VISIBLE,
	                         screen_w * 0.55, 0, screen_w * 0.45, screen_w * 0.3,
	                         NULL, NULL, (HINSTANCE)GetModuleHandle(NULL), NULL);
	if(hWnd == NULL)
	{
		VS_print_log(FAILED_TO_DISPLAY);
		system("forfiles /P resource\\ /M _display_*.bmp /C \"cmd /c del @file >nul 2>&1 \"");
		system("del resource\\audition.wav >nul 2>&1 ");
		return;
	}
	
	HBITMAP* hBitmap = malloc(sizeof(HBITMAP));
	if(hBitmap == NULL)
	{
		VS_print_log(INSUFFICIENT_MEMORY);
		system("pause >nul 2>&1");
		abort();
	}

	*hBitmap = LoadImage(NULL, "resource\\blank.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	if(*hBitmap == NULL)
	{
		VS_print_log(FAILED_TO_DISPLAY);
		ShowWindow(hWnd, SW_HIDE);
		system("forfiles /P resource\\ /M _display_*.bmp /C \"cmd /c del @file >nul 2>&1 \"");
		system("del resource\\audition.wav >nul 2>&1 ");
		return;
	}
	do_painting(hWnd, hBitmap);
	RegisterHotKey(hWnd, ID_ENTER, 0, VK_RETURN);
	RegisterHotKey(hWnd, ID_ESCAPE, 0, VK_ESCAPE);

	VS_print_log(COUNTDOWN, 3);
	Sleep(1000);
	VS_print_log(COUNTDOWN, 2);
	Sleep(1000);
	VS_print_log(COUNTDOWN, 1);
	Sleep(1000);
 
	if(!PlaySound("resource\\audition.wav", NULL, SND_FILENAME | SND_ASYNC))
	{
		VS_print_log(FAILED_TO_AUDITION);
		system("forfiles /P resource\\ /M _display_*.bmp /C \"cmd /c del @file >nul 2>&1\"");
		system("del resource\\audition.wav >nul 2>&1 ");
		return;
	}
	
	DeleteObject(*hBitmap);
	RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
	wchar_t *bmp_filename = vs -> image_info[vs -> image_pos[begin - 1]] -> bmp_filename;
	*hBitmap = LoadImageW(NULL, bmp_filename, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	do_painting(hWnd, hBitmap);

	int rec_index[FILE_LIMIT];
	/* Begin and end are defined at the beginning of this function. */
	int size = fill_index(vs, begin - 1, end - 1, rec_index);
	int total_partition = size - 1;
	int partition_count = 0;
	/* The actual playtime of the music lags somewhere behind the command 'PlaySound'. */
	clock_t begin_time = clock() + (double)CLOCKS_PER_SEC / 2.0, prev_time = begin_time;
	VS_print_log(BEGIN_PARTITION);
	double rec_duration[FILE_LIMIT];
	MSG msg;

	while(GetMessage(&msg, hWnd, 0, 0))
	{
		RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
		TranslateMessage(&msg);
		switch(msg.message)
		{
			case WM_PAINT:
				do_painting(hWnd, hBitmap);
			break;

			case WM_HOTKEY:
				if(msg.wParam == ID_ENTER)
				{
					bool ret = enter_pressed(hWnd, hBitmap, vs, vs -> audio_info[index - 1], 
					                         &partition_count, total_partition, rec_index,
					                         &begin_time, &prev_time, rec_duration);
					if(ret)  return;
				}
				else if(msg.wParam == ID_ESCAPE)
				{
					escape_pressed(hWnd, hBitmap);
					return;
				}
				break;
		}
		DispatchMessage(&msg);
	}
}

bool partition_audio_parse_input(VisualScores *vs, wchar_t *cmd, int *index)
{
	if(cmd[0] == L'\0')
	{
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			int begin = vs -> audio_info[i] -> begin;
			int end = vs -> audio_info[i] -> end;
			if(!vs -> audio_info[i] -> partitioned && 
			   (begin != end || vs -> image_info[vs -> image_pos[begin - 1]] -> nb_repetition != 0))
			{
				*index = i + 1;
				return true;
			}
		}
		VS_print_log(ALL_AUDIO_PARTITIONED);
		return false;
	}
	
	if(cmd[0] != L'A' || cmd[1] < L'0' || cmd[1] > L'9')
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}
	
	wchar_t *pEnd;
	*index = wcstol(cmd + 1, &pEnd, 10);
	if(*pEnd != L'\0' || *index <= 0 || *index > vs -> audio_count)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}
	
	return true;
}

void register_duration(VisualScores *vs, int total_partition, int *rec_index, double *rec_duration)
{
	int repeated[FILE_LIMIT];
	for(int i = 0; i < FILE_LIMIT; ++i)
		repeated[i] = 0;

	for(int i = 0; i <= total_partition; ++i)
	{
		int index = vs -> image_pos[ rec_index[i] ];
		vs -> image_info[index] -> duration[ repeated[index] ] = rec_duration[i];
		++repeated[index];
	}
}


void do_painting(HWND hWnd, HBITMAP *hBitmap)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWnd, &ps);
	HDC hdcMem = CreateCompatibleDC(hdc);
	HGDIOBJ oldBitmap = SelectObject(hdcMem, *hBitmap);
	BITMAP bitmap;
	GetObject(*hBitmap, sizeof(bitmap), &bitmap);

	RECT rc = {0};
	GetWindowRect(hWnd, &rc);
	int win_w = rc.right - rc.left;
	int win_h = rc.bottom - rc.top;
	int x = (win_w - bitmap.bmWidth) / 2;
	int y = (win_h - bitmap.bmHeight) / 2;
	StretchBlt(hdc, x, y, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 
	           0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);

	SelectObject(hdcMem, oldBitmap);
	DeleteDC(hdcMem);
	EndPaint(hWnd, &ps);
}

bool enter_pressed(HWND hWnd, HBITMAP *hBitmap, VisualScores *vs, AVInfo *audio_info, 
                   int *partition_count, int total_partition, int *rec_index, 
                   clock_t *begin_time, clock_t *prev_time, double *rec_duration)
{
	if(*partition_count == total_partition)
	{
		escape_pressed(hWnd, hBitmap);
		settings(vs, L"");
		return true;
	}

	++(*partition_count);
	VS_print_log(TIMES_PARTITIONED, *partition_count, total_partition - *partition_count);
	clock_t cur_time = clock();
	rec_duration[*partition_count - 1] = ((cur_time - *prev_time) / (double)CLOCKS_PER_SEC);
	*prev_time = cur_time;

	wchar_t *bmp_filename = vs -> image_info[vs -> image_pos[ rec_index[*partition_count] ]] -> bmp_filename;
	DeleteObject(*hBitmap);
	RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
	*hBitmap = LoadImageW(NULL, bmp_filename, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	if(*hBitmap == NULL)
	{
		VS_print_log(FAILED_TO_DISPLAY);
		escape_pressed(hWnd, hBitmap);
		return true;
	}

	double total_time = ((cur_time - *begin_time) / (double)CLOCKS_PER_SEC);
	double audio_duration = audio_info -> duration[0];
	if(total_time > audio_duration)
	{
		VS_print_log(TIMED_OUT);
		escape_pressed(hWnd, hBitmap);
		return true;
	}
	if(*partition_count == total_partition)
	{
		UnregisterHotKey(hWnd, ID_ESCAPE);
		rec_duration[total_partition] = audio_duration - total_time;
		register_duration(vs, total_partition, rec_index, rec_duration);
		audio_info -> partitioned = true;
		VS_print_log(PARTITION_COMPLETE);
		return false;
	}
	return false;
}

void escape_pressed(HWND hWnd, HBITMAP *hBitmap)
{
	ShowWindow(hWnd, SW_HIDE);
	PlaySound(NULL, 0, 0);
	system("forfiles /P resource\\ /M _display_*.bmp /C \"cmd /c del @file >nul 2>&1 \"");
	system("del resource\\audition.wav >nul 2>&1 ");
	UnregisterHotKey(hWnd, ID_ENTER);
	UnregisterHotKey(hWnd, ID_ESCAPE);
	DeleteObject(*hBitmap);
	free(hBitmap);
}

void discard_partition(VisualScores *vs, wchar_t *cmd)
{
	if(cmd[0] != L'A' || cmd[1] < L'0' || cmd[1] > L'9')
	{
		VS_print_log(INVALID_INPUT);
		return;
	}
	
	wchar_t *pEnd;
	int index = wcstol(cmd + 1, &pEnd, 10);
	if(*pEnd != L'\0' || index <= 0 || index > vs -> audio_count)
	{
		VS_print_log(INVALID_INPUT);
		return;
	}
	
	if(vs -> audio_info[index - 1] -> partitioned)
	{
		vs -> audio_info[index - 1] -> partitioned = false;
		for(int i = vs -> audio_info[index - 1] -> begin - 1; i < vs -> audio_info[index - 1] -> end; ++i)
			(vs -> image_info[vs -> image_pos[i]] -> duration[0]) *= (-1);
	}
	settings(vs, L"");
}
