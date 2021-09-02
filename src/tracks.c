#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "vslog.h"
#include "visualscores.h"

bool has_image_ext(char *filename)
{
	char *ext = strrchr(filename, '.');
	if(ext == NULL)
		return false;
	else  ++ext;

	const char image_ext[8][5] = {"bmp", "jpeg", "jpg", "png", "tiff", "tif", "webp", "ico"};
	for(int i = 0; i < 8; ++i)
		if(strcmp(ext, image_ext[i]) == 0)
			return true;
	return false;
}

bool has_audio_ext(char *filename)
{
	char *ext = strrchr(filename, '.');
	if(ext == NULL)
		return false;
	else  ++ext;

	const char audio_ext[4][5] = {"wav", "flac", "mp3", "aac"};
	for(int i = 0; i < 4; ++i)
		if(strcmp(ext, audio_ext[i]) == 0)
			return true;
	return false;
}

void load(VisualScores *vs, char *cmd)
{
	char filename[STRING_LIMIT];
	int offset = 0;
	bool valid = load_check_input(vs, cmd, filename, &offset);
	if(!valid)  return;

	if(access(filename, X_OK) == -1)
	{
		VS_print_log(NO_PERMISSION);
		return;
	}

	if(!has_image_ext(filename))
	{
		VS_print_log(FAILED_TO_OPEN, filename);
		return;
	}

	vs -> image_info[vs -> image_count] = AVInfo_init();
	bool image_added = AVInfo_open(vs -> image_info[vs -> image_count], NULL,
	   			                   filename, AVTYPE_IMAGE, -1, -1);
	/* -1 is a placeholder. */
	if(image_added)
	{
		++(vs -> image_count);
		for(int i = vs -> image_count - 1; i > offset; --i)
			vs -> image_pos[i] = vs -> image_pos[i - 1];
		vs -> image_pos[offset] = vs -> image_count - 1;

		int in_range_of_audio = -1;
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			if(vs -> audio_info[i] -> begin > offset)
				++(vs -> audio_info[i] -> begin);
			if(vs -> audio_info[i] -> end > offset)
				++(vs -> audio_info[i] -> end);
			if(vs -> audio_info[i] -> begin <= offset && vs -> audio_info[i] -> end >= offset)
				in_range_of_audio = i;
		}
		
		/* set duration to negative if in the range of an audio file */
		if(in_range_of_audio >= 0)
		{
			vs -> image_info[vs -> image_count - 1] -> duration *= (-1);
			int i = in_range_of_audio;
			if(vs -> audio_info[i] -> partitioned)
			{
				char tag[10];
				sprintf(tag, "A%d", i + 1);
				discard_partition(vs, tag);
				VS_print_log(PARTITION_DISCARDED);
			}
		}

		for(int i = 0; i < vs -> bg_count; ++i)
		{
			if(vs -> bg_info[i] -> begin > offset)
				++(vs -> bg_info[i] -> begin);
			if(vs -> bg_info[i] -> end > offset)
				++(vs -> bg_info[i] -> end);
		}
			
		VS_print_log(IMAGE_LOADED);
		settings(vs, "");
	}
	else
	{
		VS_print_log(FAILED_TO_OPEN, filename);
		AVInfo_free(vs -> image_info[vs -> image_count]);
	}
}

bool load_check_input(VisualScores *vs, char *cmd, char *filename, int *offset)
{
	char str_offset[4];
	size_t pos = 0;
	while(pos < strlen(cmd) && cmd[pos] != ' ')
		++pos;
	strncpy(filename, cmd, pos);
	filename[pos] = '\0';
	for(unsigned int i = 0; i < strlen(filename); ++i)
		if(filename[i] == '/')
			filename[i] = '\\';
	
	if(cmd[pos] == '\0')
		*offset = vs -> image_count;
	else
	{
		while(pos < strlen(cmd) && cmd[pos] == ' ')
			++pos;
		strcpy(str_offset, cmd + pos);
		char* pEnd;
		*offset = strtol(str_offset, &pEnd, 10);
		if(str_offset[0] < '0' || str_offset[0] > '9' || *pEnd != '\0' || 
		   *offset < 0 || *offset > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}
	}
	return true;
}

void load_all(VisualScores *vs, char *cmd)
{
	char path[STRING_LIMIT];
	int offset = 0;
	bool valid = load_check_input(vs, cmd, path, &offset);
	if(!valid)  return;

	if(path[strlen(path) - 1] == '\\' || path[strlen(path) - 1] == '/')
		path[strlen(path) - 1] = '\0';
	if(access(path, X_OK) == -1)
	{
		VS_print_log(NO_PERMISSION);
		return;
	}

	const char image_ext[8][5] = {"bmp", "jpeg", "jpg", "png", "tiff", "tif", "webp", "ico"};
	int image_added = 0;
	int orig_image_count = vs -> image_count;
	for(int i = 0; i < 8; ++i)
	{
		char pattern[STRING_LIMIT];
		sprintf(pattern, "%s\\*.%s", path, image_ext[i]);
		struct _finddata_t fileinfo;
		intptr_t handle = _findfirst(pattern, &fileinfo);

		if(handle != -1)
		{
			char filename[STRING_LIMIT];
			do{
				if( (strcmp(fileinfo.name, ".") != 0) && (strcmp(fileinfo.name, "..") != 0) )
				{
					sprintf(filename, "%s\\%s", path, fileinfo.name);
					vs -> image_info[vs -> image_count] = AVInfo_init();
					bool added = AVInfo_open(vs -> image_info[vs -> image_count], NULL,
					                         filename, AVTYPE_IMAGE, -1, -1);
					/* -1 is a placeholder. */
					if(added)
					{
						++(vs -> image_count);
						++image_added;
					}
					else
					{
						VS_print_log(FAILED_TO_OPEN, filename);
						AVInfo_free(vs -> image_info[vs -> image_count]);
					}
				}
			}	while(_findnext(handle, &fileinfo) == 0);
		}
		_findclose(handle);
	}
	
	if(image_added > 0)
	{
		for(int i = orig_image_count - 1; i >= offset; --i)
			vs -> image_pos[i + image_added] = vs -> image_pos[i];
		for(int i = 0; i < image_added; ++i)
			vs -> image_pos[i + offset] = i + orig_image_count;

		int in_range_of_audio = -1;
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			if(vs -> audio_info[i] -> begin > offset)
				(vs -> audio_info[i] -> begin) += image_added;
			if(vs -> audio_info[i] -> end > offset)
				(vs -> audio_info[i] -> end) += image_added;
			if(vs -> audio_info[i] -> begin <= offset && vs -> audio_info[i] -> end >= offset)
				in_range_of_audio = i;
		}
		
		/* set duration to negative if in the range of an audio file */
		if(in_range_of_audio >= 0)
		{
			for(int i = orig_image_count; i < vs -> image_count; ++i)
				vs -> image_info[i] -> duration *= (-1);
			int i = in_range_of_audio;
			if(vs -> audio_info[i] -> partitioned)
			{
				char tag[10];
				sprintf(tag, "A%d", i + 1);
				discard_partition(vs, tag);
			}
		}

		for(int i = 0; i < vs -> bg_count; ++i)
		{
			if(vs -> bg_info[i] -> begin > offset)
				(vs -> bg_info[i] -> begin) += image_added;
			if(vs -> bg_info[i] -> end > offset)
				(vs -> bg_info[i] -> end) += image_added;
		}

		VS_print_log(IMAGES_LOADED, image_added);
		settings(vs, "");
	}
	else
		VS_print_log(IMAGE_NOT_FOUND);
}

void load_other(VisualScores *vs, char *cmd)
{
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	char filename[STRING_LIMIT];
	int begin = 0, end = 0;
	bool valid = load_other_check_input(vs, cmd, filename, &begin, &end);
	if(!valid)  return;
	
	if(access(filename, X_OK) == -1)
	{
		VS_print_log(NO_PERMISSION);
		return;
	}
	
	bool is_image = has_image_ext(filename);
	bool is_audio = has_audio_ext(filename);
	if(!is_image && !is_audio)
	{
		VS_print_log(FAILED_TO_OPEN, filename);
		return;
	}
	
	if(is_audio)
	{
		bool overlap = false;
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			if( !(begin > vs -> audio_info[i] -> end || end < vs -> audio_info[i] -> begin) )
			{
				overlap = true;
				break;
			}
		}
		if(overlap)
		{
			VS_print_log(AUDIO_OVERLAP);
			return;
		}
		
		vs -> audio_info[vs -> audio_count] = AVInfo_init();
		bool added = AVInfo_open(vs -> audio_info[vs -> audio_count], NULL,
		                         filename, AVTYPE_AUDIO, begin, end);
		if(added)
		{
			/* set the duration of image files in the range of the audio file to negative */
			for(int i = begin - 1; i < end; ++i)
				vs -> image_info[vs -> image_pos[i]] -> duration *= (-1);

			++(vs -> audio_count);
			VS_print_log(AUDIO_LOADED);
			settings(vs, "");
		}
		else
		{
			VS_print_log(FAILED_TO_OPEN, filename);
			AVInfo_free(vs -> audio_info[vs -> audio_count]);
		}
	}

	if(is_image)
	{
		vs -> bg_info[vs -> bg_count] = AVInfo_init();
		bool added = AVInfo_open(vs -> bg_info[vs -> bg_count], NULL,
		                         filename, AVTYPE_BG_IMAGE, begin, end);
		if(added)
		{
			++(vs -> bg_count);
			VS_print_log(IMAGE_LOADED);
			settings(vs, "");
		}
		else
		{
			VS_print_log(FAILED_TO_OPEN, filename);
			AVInfo_free(vs -> bg_info[vs -> bg_count]);
		}
	}
}

bool load_other_check_input(VisualScores *vs, char *cmd, char *filename, int *begin, int *end)
{
	char str_begin[10], str_end[10];
	size_t pos1 = 0, pos2 = 0;
	while(pos1 < strlen(cmd) && cmd[pos1] != ' ')
		++pos1;
	strncpy(filename, cmd, pos1);
	filename[pos1] = '\0';
	for(unsigned int i = 0; i < strlen(filename); ++i)
		if(filename[i] == '/')
			filename[i] = '\\';

	if(cmd[pos1] == '\0')
	{
		*begin = 1;
		*end = vs -> image_count;
	}
	else
	{
		while(pos1 < strlen(cmd) && cmd[pos1] == ' ')
			++pos1;
		pos2 = pos1;
		while(pos2 < strlen(cmd) && cmd[pos2] != ' ')
			++pos2;
		strncpy(str_begin, cmd + pos1, pos2 - pos1);
		str_begin[pos2 - pos1] = '\0';

		char* pEnd;
		*begin = strtol(str_begin, &pEnd, 10);
		if(str_begin[0] < '0' || str_begin[0] > '9' || *pEnd != '\0' ||
		   *begin < 0 || *begin > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}

		while(pos2 < strlen(cmd) && cmd[pos2] == ' ')
			++pos2;
		strcpy(str_end, cmd + pos2);
		*end = strtol(str_end, &pEnd, 10);
		if(str_end[0] < '0' || str_end[0] > '9' || *pEnd != '\0' ||
		   *end < *begin || *end > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}
	}
	return true;
}

void delete_file(VisualScores *vs, char *cmd)
{
	AVType type;
	int index;
	bool valid = delete_file_check_input(vs, cmd, &type, &index);
	if(!valid)  return;
	
	switch(type)
	{
		case AVTYPE_IMAGE:
		{
			int pos_to_delete = vs -> image_pos[index - 1];
			AVInfo_free(vs -> image_info[pos_to_delete]);
			for(int i = pos_to_delete + 1; i < vs -> image_count; ++i)
				vs -> image_info[i - 1] = vs -> image_info[i];
			vs -> image_info[vs -> image_count - 1] = NULL;

			for(int i = index; i < vs -> image_count; ++i)
				vs -> image_pos[i - 1] = vs -> image_pos[i];
			vs -> image_pos[vs -> image_count - 1] = -1;

			--(vs -> image_count);
			for(int i = 0; i < vs -> image_count; ++i)
				if(vs -> image_pos[i] > pos_to_delete)
					--(vs -> image_pos[i]);

			int in_range_of_audio = -1;
			for(int i = 0; i < vs -> audio_count; ++i)
			{
				if(vs -> audio_info[i] -> begin == index && vs -> audio_info[i] -> end == index)
				{
					VS_print_log(ANOTHER_FILE_DELETED, vs -> audio_info[i] -> filename);
					AVInfo_free(vs -> audio_info[i]);
					for(int j = i + 1; j < vs -> audio_count; ++j)
						vs -> audio_info[j - 1] = vs -> audio_info[j];
					vs -> audio_info[vs -> audio_count - 1] = NULL;
					--(vs -> audio_count);    --i;
					continue;
				}

				if(vs -> audio_info[i] -> begin > index)
					--(vs -> audio_info[i] -> begin);
				if(vs -> audio_info[i] -> end  >= index)
					--(vs -> audio_info[i] -> end);
				if(vs -> audio_info[i] -> begin <= index && vs -> audio_info[i] -> end >= index)
					in_range_of_audio = i;
			}

			if(in_range_of_audio >= 0)
			{
				int i = in_range_of_audio;
				if(vs -> audio_info[i] -> partitioned)
				{
					char tag[10];
					sprintf(tag, "A%d", i + 1);
					discard_partition(vs, tag);
				}
			}

			for(int i = 0; i < vs -> bg_count; ++i)
			{
				if(vs -> bg_info[i] -> begin == index && vs -> bg_info[i] -> end == index)
				{
					VS_print_log(ANOTHER_FILE_DELETED, vs -> bg_info[i] -> filename);
					AVInfo_free(vs -> bg_info[i]);
					for(int j = i + 1; j < vs -> bg_count; ++j)
						vs -> bg_info[j - 1] = vs -> bg_info[j];
					vs -> bg_info[vs -> bg_count - 1] = NULL;
					--(vs -> bg_count);    --i;
					continue;
				}

				if(vs -> bg_info[i] -> begin > index)
					--(vs -> bg_info[i] -> begin);
				if(vs -> bg_info[i] -> end  >= index)
					--(vs -> bg_info[i] -> end);
			}
			break;
		}

		case AVTYPE_AUDIO:
		{
			for(int i = vs -> audio_info[index - 1] -> begin - 1; i < vs -> audio_info[index - 1] -> end; ++i)
				vs -> image_info[vs -> image_pos[i]] -> duration = 3.0;
	
			AVInfo_free(vs -> audio_info[index - 1]);
			for(int i = index; i < vs -> audio_count; ++i)
				vs -> audio_info[i - 1] = vs -> audio_info[i];
			vs -> audio_info[vs -> audio_count - 1] = NULL;
			
			--(vs -> audio_count);
			break;
		}

		case AVTYPE_BG_IMAGE:
		{
			AVInfo_free(vs -> bg_info[index - 1]);
			for(int i = index; i < vs -> bg_count; ++i)
				vs -> bg_info[i - 1] = vs -> bg_info[i];
			vs -> bg_info[vs -> bg_count - 1] = NULL;

			--(vs -> bg_count);
			break;
		}
	}
	
	VS_print_log(FILE_DELETED);
	settings(vs, "");
}

bool delete_file_check_input(VisualScores *vs, char *cmd, AVType *type, int *index)
{
	size_t pos;
	int max_index;
	switch(cmd[0])
	{
		case 'I':
			*type = AVTYPE_IMAGE;
			max_index = vs -> image_count;
			pos = 1;
			break;
		case 'A':
			*type = AVTYPE_AUDIO;
			max_index = vs -> audio_count;
			pos = 1;
			break;
		case 'B':
			if(cmd[1] == 'G')
			{
				*type = AVTYPE_BG_IMAGE;
				max_index = vs -> bg_count;
				pos = 2;
				break;
			}
		default:
			VS_print_log(INVALID_INPUT);
			return false;
	}

	if(cmd[pos] < '0' || cmd[pos] > '9')
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}
	
	char* pEnd;
	*index = strtol(cmd + 1, &pEnd, 10);
	if(*pEnd != '\0' || *index <= 0 || *index > max_index)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}
	
	return true;
}

void modify_file(VisualScores *vs, char *cmd)
{
	
}

