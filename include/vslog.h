#ifndef VSLOG_H
#define VSLOG_H

#include <stdbool.h>

/* Note that here we have added 1 to the actual number of tags. */
#define VS_LOG_COUNT 34
#define STRING_LIMIT 200  /* maximum length of a string */

typedef enum Language
{
	English = 0,
	Chinese = 1
} Language;
extern Language language;

extern bool muted;

typedef enum VS_log_tag
{
	INSUFFICIENT_MEMORY = 1,
	WRONG_COMMAND,
	SWITCH_LANGUAGE,
	IMAGE_NOT_LOADED,
	IMAGE_TRACK_HEAD,
	AUDIO_TRACK_HEAD,
	BG_IMAGE_TRACK_HEAD,
	TAG_AND_FILENAME,
	DURATION_N_A,
	DURATION,
	BEGIN_AND_END,
	
	INVALID_INPUT,
	NO_PERMISSION,
	FAILED_TO_OPEN,
	PARTITION_DISCARDED,
	IMAGE_LOADED,
	IMAGES_LOADED,
	IMAGE_NOT_FOUND,
	AUDIO_OVERLAP,
	AUDIO_LOADED,
	ANOTHER_FILE_DELETED,
	FILE_DELETED,
	FILE_MODIFIED,

	AUDIO_NOT_LOADED,
	NEED_NO_PARTITION,
	CREATING_BMP,
	FAILED_TO_CREATE_BMP,
	NO_MUSIC_PLAYER_FOUND,
	FAILED_TO_DISPLAY,
	COUNTDOWN,
	TIMES_PARTITIONED,
	TIMED_OUT,
	PARTITION_COMPLETE
} VS_log_tag;

extern const char vs_log[2][VS_LOG_COUNT][STRING_LIMIT];

extern void VS_print_log(VS_log_tag, ...);

#endif /* VSLOG_H */
