/** 
 * VisualScores source file: vslog.c
 * Defines output messages and functions.
 */

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
		"VisualScores - Beta\n(c) 2021 Ji-woon Sim\n",
		"Language has been switched to English.\n\n",
		"You have to load an image first.\n\n",
		"\nImage track:\n",
		"\nAduio track:\n",
		"\nBackground image track:\n",
		"\nRepetition(s):\n",
		"    %s%d: filename: %s, ",
		"duration: N/A\n",
		"duration: set\n",
		"duration: %.2f(s)\n",
		"begin: I%d, end: I%d\n",
		"    begin: I%d, end: I%d, %d time(s)\n",

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
		"You can not overlap repetition intervals.\n\n",
		"Successfullt set repetition times.\n",
		"The duration of the image file can not be changed since it is in the range of an audio file.\n\n",
		"Successfully set duration.\n",

		"You have to load an audio file first.\n\n",
		"All audio files have been partitioned.\n\n",
		"This audio file does not need to partition.\n\n",
		"Creating bmp files for display...\n",
		"ERROR: Failed to create bmp files.\n\n",
		"Creating wav file for audition...\n",
		"ERROR: Failed to create wav file.\n\n",
		"ERROR: Failed to display images.\n\n",
		"ERROR: Failed to audition.\n\n",
		"Music will be played in %d second(s)...\n",
		"Begin partition.\n",
		"    You have partitioned the audio %d time(s). %d time(s) left.\n",
		"\nTimed out. Please retry.\n\n",
		"\nSuccessfully partitioned audio.\nPress Enter to close the display window and the music player.\n",
		
		"The duration of image file I%d is not set.\n\n",
		"Time limit exceeded. Failed to export video.\n\n",
		"Writing image track: %d/%d\n",
		"Writing audio track: %d/%d\n",
		"Failed to export video file.\n\n",
		"Export completed.\n\n"
	}, {
		"",
		"????????????????????????????\n",
		"??????????????????????????????\n\n",
		"VisualScores - Beta\n(c) 2021 ??????\n",
		"??????????????????\n\n",
		"??????????????????????\n\n",
		"\n????????\n",
		"\n????????\n",
		"\n????????????\n",
		"\n??????\n",
		"    %s%d??????????%s??",
		"??????N/A\n",
		"????????????\n",
		"??????%.2f??????\n",
		"??????I%d????????I%d\n",
		"??????I%d????????I%d????????%d\n",

		"????????????????????????????\n\n",
		"??????????????????????????????????\n\n",
		"????????????????????????????\n\n",
		"??????????????????????????????\n\n",
		"??????????????????????????????????\n\n",
		"%s????????????????\n\n",
		"???????????????? %s ????????????????????\n",
		"??????????????\n",
		"????????%d????????\n",
		"??????????????????????????????\n\n",
		"??????????????????\n\n",
		"??????????????\n",
		"?????????? %s ??????????\n",
		"??????????????\n",
		"??????????????\n",
		"??????????????????\n\n",
		"??????????????????\n",
		"????????????????????????????????????????????????????????\n\n",
		"??????????????\n",

		"??????????????????????\n\n",
		"??????????????????????????\n\n",
		"????????????????????\n\n",
		"????????bmp????????\n",
		"??????????????bmp????????\n\n",
		"????????wav??????\n",
		"??????????????wav??????\n\n",
		"????????????????????\n\n",
		"????????????????\n\n",
		"????????%d??????????\n",
		"??????????????\n",
		"    ????????????%d????????%d????\n",
		"\n????????????????\n\n",
		"\n??????????????\n??????????????????????????????????\n",
		
		"?????????? I%d ????????\n\n",
		"????????????????????????????????\n\n",
		"????????????????%d/%d\n",
		"????????????????%d/%d\n",
		"??????????????\n\n",
		"??????????\n\n" 
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
