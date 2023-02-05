/** 
 * VisualScores source file: vslog.c
 * Defines output messages and functions.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <wchar.h>

#include "vslog.h"

Language language = English;
bool muted = false;

const wchar_t vs_log[2][VS_LOG_COUNT][STRING_LIMIT] = {
	{
		L"",
		L"ERROR: Insufficient memory. Program aborted.\n",
		L"Wrong command. Please check your input and try again.\n\n",
		L"VisualScores - 1.1\n(c) 2021-2023 Ji-woon Sim\n",
		L"Language toggled to English.\n\n",
		L"You have to load an image first.\n\n",
		L"\nImage track:\n",
		L"\nAduio track:\n",
		L"\nBackground image track:\n",
		L"\nRepetition(s):\n",
		L"    %ls%d: filename: %ls, ",
		L"duration: N/A\n",
		L"duration: set\n",
		L"duration: %.2f(s)\n",
		L"begin: I%d, end: I%d\n",
		L"    begin: I%d, end: I%d, %d time(s)\n",

		L"File limit exceeded. Failed to load file.\n\n",
		L"Warning: file limit exceeded. Loading stopped.\n",
		L"Invalid input. Please check your input and try again.\n\n",
		L"The file/folder does not exist or it does not have read/write permission.\n\n",
		L"The filename does not contain an extension or the extension is not supported.\n\n",
		L"%ls: Failed to open file.\n\n",
		L"Warning: The partition of audio file %ls is discarded.\n",
		L"Successfully loaded image.\n",
		L"Successfully loaded %d image(s).\n",
		L"Image file not found. Please check your input and try again.\n\n",
		L"You can not overlap audio files.\n\n",
		L"Successfully loaded audio.\n",
		L"Warning: can not determine the duration of the audio file %ls.\n",
		L"Warning: file %ls is also deleted.\n",
		L"Successfully deleted file.\n",
		L"Successfully modified file.\n",
		L"You can not overlap repetition intervals.\n\n",
		L"A repetition interval should be completely inside or outside an audio file.\n\n",
		L"Successfullt set repetition times.\n",
		L"The duration of the image file can not be changed since it is in the range of an audio file.\n\n",
		L"Successfully set duration.\n",

		L"You have to load an audio file first.\n\n",
		L"All audio files have been partitioned.\n\n",
		L"This audio file does not need to partition.\n\n",
		L"Creating bmp files for display...\n",
		L"ERROR: Failed to create bmp files.\n\n",
		L"Creating wav file for audition...\n",
		L"ERROR: Failed to create wav file.\n\n",
		L"ERROR: Failed to display images.\n\n",
		L"ERROR: Failed to audition.\n\n",
		L"Music will be played in %d second(s)...\n",
		L"Begin partition.\n",
		L"    You have partitioned the audio %d time(s). %d time(s) left.\n",
		L"\nTimed out. Please retry.\n\n",
		L"\nSuccessfully partitioned audio.\nPress Enter to close the display window and the music player.\n",
		
		L"The duration of image file I%d is not set.\n\n",
		L"Time limit exceeded. Failed to export video.\n\n",
		L"Writing image track: %d/%d\n",
		L"Writing audio track: %d/%d\n",
		L"Failed to export video file.\n\n",
		L"Export completed.\n\n"
	}, {
		L"",
		L"错误：内存不足。程序已退出。\n",
		L"错误的指令。请检查输入后重试。\n\n",
		L"VisualScores - 1.1\n(c) 2021-2023 沈智云\n",
		L"语言已切换为中文。\n\n",
		L"您需要先载入一张图片。\n\n",
		L"\n图片轨：\n",
		L"\n音频轨：\n",
		L"\n背景图片轨：\n",
		L"\n反复：\n",
		L"    %ls%d：文件名：%ls，",
		L"时长：N/A\n",
		L"时长：已设置\n",
		L"时长：%.2f（秒）\n",
		L"开始：I%d，结束：I%d\n",
		L"    开始：I%d，结束：I%d，次数：%d\n",

		L"超过文件数量上限。载入失败。\n\n",
		L"警告：超过文件数量上限。停止载入。\n\n",
		L"输入错误。请检查输入后重试。\n\n",
		L"文件（夹）不存在或无读写权限。\n\n",
		L"文件名不包含后缀或不支持此后缀名。\n\n",
		L"%ls：无法打开文件。\n\n",
		L"警告：对音频文件 %ls 所做的划分被撤销了。\n",
		L"成功加载图片。\n",
		L"成功加载%d张图片。\n",
		L"未找到图片。请检查输入后重试。\n\n",
		L"不能重叠音频文件。\n\n",
		L"成功加载音频。\n",
		L"警告：无法确定音频文件 %ls 的时长。\n",
		L"警告：文件 %ls 也被删除。\n",
		L"成功删除文件。\n",
		L"成功修改文件。\n",
		L"不能重叠反复区间。\n\n",
		L"反复区间必须完整地落在某个音频文件内或外。\n\n",
		L"成功设置反复次数。\n",
		L"无法改变该图片文件的时长，因为它在某个音频文件的范围中。\n\n",
		L"成功设置时长。\n",

		L"您需要先载入音频文件。\n\n",
		L"所有音频文件都被划分过了。\n\n",
		L"该音频文件无需划分。\n\n",
		L"正在创建bmp预览图…\n",
		L"错误：无法创建bmp预览图。\n\n",
		L"正在创建wav音频…\n",
		L"错误：无法创建wav音频。\n\n",
		L"错误：无法预览图片。\n\n",
		L"错误：无法试听。\n\n",
		L"音乐将在%d秒后播放…\n",
		L"开始划分音频。\n",
		L"    您已经划分了%d次，剩余%d次。\n",
		L"\n已超时，请重试。\n\n",
		L"\n成功划分音频。\n按回车键关闭预览窗口和音乐播放器。\n",

		L"未设置图片 I%d 的时长。\n\n",
		L"视频时长超过限制。导出视频失败。\n\n",
		L"正在导出图片轨：%d/%d\n",
		L"正在导出音频轨：%d/%d\n",
		L"视频导出失败。\n\n",
		L"导出完成。\n\n"
	}
};

void VS_print_log(VS_log_tag tag, ...)
{
	if(muted)  return;
	
	va_list vl;
	va_start(vl, tag);
	vwprintf(vs_log[language][(int)tag], vl);
	va_end(vl);
}
