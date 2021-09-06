#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "vslog.h"

Language language = English;
bool muted = false;

const char vs_log[2][VS_LOG_COUNT][STRING_LIMIT] = {
	{
		"",
		"ERROR: Insufficient memory. Program aborted.\n",
		"Wrong command. Please check your input and try again.\n\n",
		"Language has been switched to English.\n\n",
		"You have to load an image first.\n\n",
		"\nImage track:\n",
		"\nAduio track:\n",
		"\nBackground image track:\n",
		"    %s%d: filename: %s, ",
		"duration: N/A\n",
		"duration: %.2f(s)\n",
		"begin: I%d, end: I%d\n",

		"File limit exceeded. Failed to load file.\n\n",
		"Warning: file limit exceeded. Loading stopped.\n",
		"Invalid input. Please check your input and try again.\n\n",
		"The file/folder does not exist or it does not have read/write permission.\n\n",
		"The filename does not contain an extension or the extension is not supported.\n\n",
		"%s: Failed to open file.\n\n",
		"Warning: The partition of audio file %s is discarded.\n",
		"Successfully loaded image.\n",
		"Successfully loaded %d image(s).\n",
		"Image file not found. Please check your input and try again.\n\n",
		"You can not overlap audio files.\n\n",
		"Successfully loaded audio.\n",
		"Warning: file %s is also deleted.\n",
		"Successfully deleted file.\n",
		"Successfully modified file.\n",
		"The duration of the image file can not be changed since it is in the range of an audio file.\n\n",
		"Successfully set duration.\n",

		"You have to load an audio file first.\n\n",
		"All audio files have been partitioned.\n\n",
		"This audio file does not need to partition.\n\n",
		"Creating bmp files for display...\n",
		"ERROR: Failed to create bmp files.\n\n",
		"ERROR: No music player detected or you have not set default music player.\n\n",
		"ERROR: Failed to display images.\n\n",
		"Music will be played in %d second(s)...\n",
		"    You have partitioned the audio %d time(s). %d time(s) left.\n",
		"\nTimed out. Please retry.\n\n",
		"\nSuccessfully partitioned audio.\nPress enter to close the display window and the music player.\n",
		
		"The duration of image file I%d is not set.\n\n",
		"Time limit exceeded. Failed to export video.\n\n",
		"Writing image track: %d/%d\n",
		"Writing audio track: %d/%d\n",
		"Failed to export video file.\n\n",
		"Export completed.\n\n"
	}, {
		"",
		"错误：内存不足。程序已退出。\n",
		"错误的指令。请检查输入后重试。\n\n",
		"语言已切换为中文。\n\n",
		"您需要先载入一张图片。\n\n",
		"\n图片轨：\n",
		"\n音频轨：\n",
		"\n背景图片轨：\n",
		"    %s%d：文件名：%s，",
		"时长：N/A\n",
		"时长：%.2f（秒）\n",
		"开始：I%d，结束：I%d\n",

		"超过文件数量上限。载入失败。\n\n",
		"警告：超过文件数量上限。停止载入。\n\n",
		"输入错误。请检查输入后重试。\n\n",
		"文件（夹）不存在或无读写权限。\n\n",
		"文件名不包含后缀或不支持此后缀名。\n\n",
		"%s：无法打开文件。\n\n",
		"警告：对音频文件 %s 所做的划分被撤销了。\n",
		"成功加载图片。\n",
		"成功加载%d张图片。\n",
		"未找到图片。请检查输入后重试。\n\n",
		"不能重叠音频文件。\n\n",
		"成功加载音频。\n",
		"警告：文件 %s 也被删除。\n",
		"成功删除文件。\n",
		"成功修改文件。\n",
		"无法改变该图片文件的时长，因为它在某个音频文件的范围中。\n\n",
		"成功设置时长。\n",

		"您需要先载入音频文件。\n\n",
		"所有音频文件都被划分过了。\n\n",
		"该音频文件无需划分。\n\n",
		"正在创建bmp预览图…\n",
		"错误：无法创建bmp预览图。\n\n",
		"错误：未检测到音乐播放器或您未设置默认音乐播放器。\n\n",
		"错误：无法预览图片。\n\n",
		"音乐将在%d秒后播放…\n",
		"    您已经划分了%d次，剩余%d次。\n",
		"\n已超时，请重试。\n\n",
		"\n成功划分音频。\n按回车键关闭预览窗口和音乐播放器。\n",
		
		"未设置图片 I%d 的时长。\n\n",
		"视频时长超过限制。导出视频失败。\n\n",
		"正在导出图片轨：%d/%d\n",
		"正在导出音频轨：%d/%d\n",
		"视频导出失败。\n\n",
		"导出完成。\n\n" 
	}
};

void VS_print_log(VS_log_tag tag, ...)
{
	if(muted)  return;
	
	va_list vl;
	va_start(vl, tag);
	vprintf(vs_log[language][(int)tag], vl);
	va_end(vl);
}
