/** 
 * VisualScores source file: tracks.c
 * Defines functions which add, modify or delete files in a track.
 */

#include <io.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vslog.h"
#include "visualscores.h"

bool has_image_ext(wchar_t *filename)
{
	wchar_t *ext = wcsrchr(filename, L'.');
	if(ext == NULL)
		return false;
	else  ++ext;

	const wchar_t image_ext[8][5] = {L"bmp", L"jpeg", L"jpg", L"png", L"tiff", L"tif", L"webp", L"ico"};
	for(int i = 0; i < 8; ++i)
		if(wcscmp(ext, image_ext[i]) == 0)
			return true;
	return false;
}

bool has_audio_ext(wchar_t *filename)
{
	wchar_t *ext = wcsrchr(filename, L'.');
	if(ext == NULL)
		return false;
	else  ++ext;

	const wchar_t audio_ext[4][5] = {L"wav", L"flac", L"mp3", L"aac"};
	for(int i = 0; i < 4; ++i)
		if(wcscmp(ext, audio_ext[i]) == 0)
			return true;
	return false;
}

void load(VisualScores *vs, wchar_t *cmd)
{
	if(vs -> image_count == FILE_LIMIT)
	{
		VS_print_log(FILE_LIMIT_EXCEEDED);
		return;
	}

	wchar_t filename[STRING_LIMIT];
	int offset = 0;
	bool valid = load_parse_input(vs, cmd, filename, &offset);
	if(!valid)  return;

	if(_waccess(filename, 7) == -1) // rwx access
	{
		VS_print_log(NO_PERMISSION);
		return;
	}

	if(!has_image_ext(filename))
	{
		VS_print_log(UNSUPPORTED_EXTENSION);
		return;
	}

	vs -> image_info[vs -> image_count] = AVInfo_init();
	bool image_added = AVInfo_open(vs -> image_info[vs -> image_count], filename,
	   			                   AVTYPE_IMAGE, -1, -1, -1, -1);
	/* -1 is a placeholder. */
	if(image_added)
	{
		++(vs -> image_count);
		for(int i = vs -> image_count - 1; i > offset; --i)
			vs -> image_pos[i] = vs -> image_pos[i - 1];
		vs -> image_pos[offset] = vs -> image_count - 1;

		if(offset == 0)
			vs -> image_info[vs -> image_pos[offset]] -> nb_repetition = 0;
		else
		{
			int left_nb_repetition = vs -> image_info[vs -> image_pos[offset - 1]] -> nb_repetition;
			vs -> image_info[vs -> image_pos[offset]] -> nb_repetition = 
				( (left_nb_repetition > 0) ? left_nb_repetition : 0 );
		}

		int in_range_of_audio = -1;
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			if(vs -> audio_info[i] -> begin > offset)
				++(vs -> audio_info[i] -> begin);
			if(vs -> audio_info[i] -> end > offset)
				++(vs -> audio_info[i] -> end);
			if(vs -> audio_info[i] -> begin <= offset && vs -> audio_info[i] -> end > offset)
				in_range_of_audio = i;
		}
		
		if(in_range_of_audio >= 0)
		{
			int i = in_range_of_audio;
			if(vs -> audio_info[i] -> partitioned)
			{
				bool muted_orig = muted;
				muted = true;
				wchar_t tag[10];
				swprintf(tag, 10, L"A%d", i + 1);
				discard_partition(vs, tag);
				muted = muted_orig;
				VS_print_log(PARTITION_DISCARDED, vs -> audio_info[i] -> filename);
			}
			/* set duration to negative if in the range of an audio file */
			if(vs -> image_info[vs -> image_count - 1] -> duration[0] > 0)
			   vs -> image_info[vs -> image_count - 1] -> duration[0] *= (-1);
		}

		for(int i = 0; i < vs -> bg_count; ++i)
		{
			if(vs -> bg_info[i] -> begin > offset)
				++(vs -> bg_info[i] -> begin);
			if(vs -> bg_info[i] -> end > offset)
				++(vs -> bg_info[i] -> end);
		}
			
		VS_print_log(IMAGE_LOADED);
		settings(vs, L"");
	}
	else
	{
		VS_print_log(FAILED_TO_OPEN, filename);
		AVInfo_free(vs -> image_info[vs -> image_count]);
	}
}

bool load_parse_input(VisualScores *vs, wchar_t *cmd, wchar_t *filename, int *offset)
{
	wchar_t str_offset[4];
	size_t pos = 0;
	while(pos < wcslen(cmd) && cmd[pos] != L' ')
		++pos;
	wcsncpy(filename, cmd, pos);
	filename[pos] = L'\0';

	for(unsigned int i = 0; i < wcslen(filename); ++i)
		if(filename[i] == L'/')
			filename[i] = L'\\';

	bool no_dir = (wcschr(filename, L'\\') == NULL);
	if(no_dir)
	{
		wchar_t temp[STRING_LIMIT];
		wcscpy(temp, filename);
		swprintf(filename, STRING_LIMIT, L".\\%ls", temp);
	}

	if(cmd[pos] == L'\0')
		*offset = vs -> image_count;
	else
	{
		while(pos < wcslen(cmd) && cmd[pos] == L' ')
			++pos;
		wcscpy(str_offset, cmd + pos);
		wchar_t *pEnd;
		*offset = wcstol(str_offset, &pEnd, 10);
		if(str_offset[0] < L'0' || str_offset[0] > L'9' || *pEnd != L'\0' || 
		   *offset < 0 || *offset > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}
	}
	return true;
}

void load_all(VisualScores *vs, wchar_t *cmd)
{
	wchar_t path[STRING_LIMIT];
	int offset = 0;
	bool valid = load_parse_input(vs, cmd, path, &offset);
	if(!valid)  return;

	if(path[wcslen(path) - 1] == L'\\' || path[wcslen(path) - 1] == L'/')
		path[wcslen(path) - 1] = L'\0';
	if(_waccess(path, 7) == -1) // rwx access
	{
		VS_print_log(NO_PERMISSION);
		return;
	}

	const wchar_t image_ext[8][5] = {L"bmp", L"jpeg", L"jpg", L"png", L"tiff", L"tif", L"webp", L"ico"};
	int image_added = 0;
	int orig_image_count = vs -> image_count;
	for(int i = 0; i < 8; ++i)
	{
		wchar_t pattern[STRING_LIMIT];
		swprintf(pattern, STRING_LIMIT, L"%ls\\*.%ls", path, image_ext[i]);
		struct _wfinddata_t fileinfo;
		intptr_t handle = _wfindfirst(pattern, &fileinfo);
		bool exceeded = false;

		if(handle != -1)
		{
			wchar_t filename[STRING_LIMIT];
			do{
				if( (wcscmp(fileinfo.name, L".") != 0) && (wcscmp(fileinfo.name, L"..") != 0) )
				{
					if(vs -> image_count == FILE_LIMIT)
					{
						if(image_added > 0)
						{
							VS_print_log(FILE_LIMIT_EXCEEDED2);
							exceeded = true;
							break;
						}
						else
						{
							VS_print_log(FILE_LIMIT_EXCEEDED);
							return;
						}
					}

					swprintf(filename, STRING_LIMIT, L"%ls\\%ls", path, fileinfo.name);
					vs -> image_info[vs -> image_count] = AVInfo_init();
					bool added = AVInfo_open(vs -> image_info[vs -> image_count],
					                         filename, AVTYPE_IMAGE, -1, -1, -1, -1);
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
			}	while(_wfindnext(handle, &fileinfo) == 0);
		}
		_findclose(handle);
		if(exceeded)  break;
	}
	
	if(image_added > 0)
	{
		for(int i = orig_image_count - 1; i >= offset; --i)
			vs -> image_pos[i + image_added] = vs -> image_pos[i];
		for(int i = 0; i < image_added; ++i)
			vs -> image_pos[i + offset] = i + orig_image_count;

		for(int i = 0; i < image_added; ++i)
		{
			if(i == 0 && offset == 0)
				vs -> image_info[vs -> image_pos[i + offset]] -> nb_repetition = 0;
			else
			{
				int left_nb_repetition = vs -> image_info[vs -> image_pos[i + offset - 1]] -> nb_repetition;
				vs -> image_info[vs -> image_pos[i + offset]] -> nb_repetition = 
					( (left_nb_repetition > 0) ? left_nb_repetition : 0 );
			}
		}

		int in_range_of_audio = -1;
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			if(vs -> audio_info[i] -> begin > offset)
				(vs -> audio_info[i] -> begin) += image_added;
			if(vs -> audio_info[i] -> end > offset)
				(vs -> audio_info[i] -> end) += image_added;
			if(vs -> audio_info[i] -> begin <= offset && vs -> audio_info[i] -> end > offset)
				in_range_of_audio = i;
		}
		
		if(in_range_of_audio >= 0)
		{
			int i = in_range_of_audio;
			if(vs -> audio_info[i] -> partitioned)
			{
				bool muted_orig = muted;
				muted = true;
				wchar_t tag[10];
				swprintf(tag, 10, L"A%d", i + 1);
				discard_partition(vs, tag);
				muted = muted_orig;
			}
			/* set duration to negative if in the range of an audio file */
			for(int i = orig_image_count; i < vs -> image_count; ++i)
				if(vs -> image_info[i] -> duration[0] > 0)
					vs -> image_info[i] -> duration[0] *= (-1);
		}

		for(int i = 0; i < vs -> bg_count; ++i)
		{
			if(vs -> bg_info[i] -> begin > offset)
				(vs -> bg_info[i] -> begin) += image_added;
			if(vs -> bg_info[i] -> end > offset)
				(vs -> bg_info[i] -> end) += image_added;
		}

		VS_print_log(IMAGES_LOADED, image_added);
		settings(vs, L"");
	}
	else
		VS_print_log(IMAGE_NOT_FOUND);
}

void load_other(VisualScores *vs, wchar_t *cmd)
{
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	wchar_t filename[STRING_LIMIT];
	int begin = 0, end = 0;
	bool valid = load_other_parse_input(vs, cmd, filename, &begin, &end);
	if(!valid)  return;
	
	if(_waccess(filename, 7) == -1) // rwx access
	{
		VS_print_log(NO_PERMISSION);
		return;
	}
	
	bool is_image = has_image_ext(filename);
	bool is_audio = has_audio_ext(filename);
	if(!is_image && !is_audio)
	{
		VS_print_log(UNSUPPORTED_EXTENSION);
		return;
	}
	
	if(is_audio)
	{
		if(vs -> audio_count == FILE_LIMIT)
		{
			VS_print_log(FILE_LIMIT_EXCEEDED);
			return;
		}
		
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			if(begin <= vs -> audio_info[i] -> end && end >= vs -> audio_info[i] -> begin)
			{
				VS_print_log(AUDIO_OVERLAP);
				return;
			}
		}

		int repetition_begin = -1, repetition_end = -1;
		for(int j = vs -> image_count - 1; j >= 0; --j)
		{
			if(repetition_end < 0 && vs -> image_info[ vs -> image_pos[j] ] -> nb_repetition < 0)
				repetition_end = j + 1;
			if(repetition_end < 0)
				continue;
			if(j == 0 || vs -> image_info[ vs -> image_pos[j - 1] ] -> nb_repetition <= 0)
				repetition_begin = j + 1;
			if(repetition_begin >= 0)
			{
				if(  begin <= repetition_end && end >= repetition_begin &&
				   !(begin <= repetition_begin && end >= repetition_end) )
				{
					VS_print_log(REPETITION_INTERSECT_AUDIO);
					return;
				}
				repetition_end = -1;
				repetition_begin = -1;
			}
		}
		
		vs -> audio_info[vs -> audio_count] = AVInfo_init();
		bool added = AVInfo_open(vs -> audio_info[vs -> audio_count],
		                         filename, AVTYPE_AUDIO, begin, end, -1, -1);

		if(added)
		{
			if(!get_audio_duration(vs -> audio_info[vs -> audio_count]))
				VS_print_log(AAC_DURATION_NOT_FOUND, filename);
			if(begin == end && vs -> image_info[vs -> image_pos[begin - 1]] -> nb_repetition == 0)
				vs -> image_info[vs -> image_pos[begin - 1]] -> duration[0] = vs -> audio_info[vs -> audio_count] -> duration[0];
			else
			{
				/* set the duration of image files in the range of the audio file to negative */
				for(int i = begin - 1; i < end; ++i)
					vs -> image_info[vs -> image_pos[i]] -> duration[0] *= (-1);
			}

			++(vs -> audio_count);
			VS_print_log(AUDIO_LOADED);

			settings(vs, L"");
		}
		else
		{
			VS_print_log(FAILED_TO_OPEN, filename);
			AVInfo_free(vs -> audio_info[vs -> audio_count]);
		}
	}

	if(is_image)
	{
		if(vs -> bg_count == FILE_LIMIT)
		{
			VS_print_log(FILE_LIMIT_EXCEEDED);
			return;
		}
		
		vs -> bg_info[vs -> bg_count] = AVInfo_init();
		bool added = AVInfo_open(vs -> bg_info[vs -> bg_count], filename,
		                         AVTYPE_BG_IMAGE, begin, end, -1, -1);
		if(added)
		{
			++(vs -> bg_count);
			VS_print_log(IMAGE_LOADED);
			settings(vs, L"");
		}
		else
		{
			VS_print_log(FAILED_TO_OPEN, filename);
			AVInfo_free(vs -> bg_info[vs -> bg_count]);
		}
	}
}

bool load_other_parse_input(VisualScores *vs, wchar_t *cmd, wchar_t *filename, int *begin, int *end)
{
	wchar_t str_begin[10], str_end[10];
	size_t pos1 = 0, pos2 = 0;
	while(pos1 < wcslen(cmd) && cmd[pos1] != L' ')
		++pos1;
	wcsncpy(filename, cmd, pos1);
	filename[pos1] = L'\0';
	for(unsigned int i = 0; i < wcslen(filename); ++i)
		if(filename[i] == L'/')
			filename[i] = L'\\';

	if(cmd[pos1] == L'\0')
	{
		*begin = 1;
		*end = vs -> image_count;
	}
	else
	{
		while(pos1 < wcslen(cmd) && cmd[pos1] == L' ')
			++pos1;
		pos2 = pos1;
		while(pos2 < wcslen(cmd) && cmd[pos2] != L' ')
			++pos2;
		wcsncpy(str_begin, cmd + pos1, pos2 - pos1);
		str_begin[pos2 - pos1] = L'\0';

		wchar_t *pEnd;
		*begin = wcstol(str_begin, &pEnd, 10);
		if(str_begin[0] < '0' || str_begin[0] > '9' || *pEnd != L'\0' ||
		   *begin <= 0 || *begin > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}

		while(pos2 < wcslen(cmd) && cmd[pos2] == L' ')
			++pos2;
		wcscpy(str_end, cmd + pos2);
		*end = wcstol(str_end, &pEnd, 10);
		if(str_end[0] < '0' || str_end[0] > '9' || *pEnd != L'\0' ||
		   *end < *begin || *end > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}
	}
	return true;
}

void delete_file(VisualScores *vs, wchar_t *cmd)
{
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	AVType type;
	int index;
	bool valid = delete_file_parse_input(vs, cmd, &type, &index);
	if(!valid)  return;
	
	switch(type)
	{
		case AVTYPE_IMAGE:
		{
			int pos_to_delete = vs -> image_pos[index - 1];
			if(index >= 2)
			{
				int pos_left = vs -> image_pos[index - 2];
				if(vs -> image_info[pos_to_delete] -> nb_repetition < 0 &&
				   vs -> image_info[pos_left] -> nb_repetition > 0)
				   vs -> image_info[pos_left] -> nb_repetition *= (-1);
			}
			
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

				if(vs -> audio_info[i] -> begin <= index && vs -> audio_info[i] -> end >= index)
					in_range_of_audio = i;
				if(vs -> audio_info[i] -> begin > index)
					--(vs -> audio_info[i] -> begin);
				if(vs -> audio_info[i] -> end  >= index)
					--(vs -> audio_info[i] -> end);

			}

			if(in_range_of_audio >= 0)
			{
				int i = in_range_of_audio;
				if(vs -> audio_info[i] -> partitioned)
				{
					bool muted_orig = muted;
					muted = true;
					wchar_t tag[10];
					swprintf(tag, 10, L"A%d", i + 1);
					discard_partition(vs, tag);
					muted = muted_orig;
					VS_print_log(PARTITION_DISCARDED, vs -> audio_info[i] -> filename);
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
			{
				for(int j = 0; j <= abs(vs -> image_info[vs -> image_pos[i]] -> nb_repetition); ++j)
					vs -> image_info[vs -> image_pos[i]] -> duration[j] = 3.0;
			}

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
	if(vs -> image_count != 0)
		settings(vs, L"");
	else  wprintf(L"\n");
}

bool delete_file_parse_input(VisualScores *vs, wchar_t *cmd, AVType *type, int *index)
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
			if(cmd[1] == L'G')
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
	
	wchar_t *pEnd;
	*index = wcstol(cmd + pos, &pEnd, 10);
	if(*pEnd != L'\0' || *index <= 0 || *index > max_index)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}
	
	return true;
}

void modify_file(VisualScores *vs, wchar_t *cmd)
{
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	AVType type;
	int index, arg1, arg2;
	bool valid = modify_file_parse_input(vs, cmd, &type, &index, &arg1, &arg2);
	if(!valid)  return;

	switch(type)
	{
		case AVTYPE_IMAGE:
		{
			int pos = ((index <= arg1) ? (arg1 - 1) : arg1);
			wchar_t tag[10];
			swprintf(tag, 10, L"I%d", index);
			wchar_t cmd[STRING_LIMIT];
			swprintf(cmd, STRING_LIMIT, L"%ls %d", vs -> image_info[vs -> image_pos[index - 1]] -> filename, pos);

			bool muted_orig = muted;
			muted = true;
			delete_file(vs, tag);
			load(vs, cmd);
			muted = muted_orig;

			break;
		}
		
		case AVTYPE_AUDIO:
		{
			int begin = arg1, end = arg2;
			for(int i = 0; i < vs -> audio_count; ++i)
			{
				if(i == index - 1)
					continue;
				if(begin <= vs -> audio_info[i] -> end &&
				   end   >= vs -> audio_info[i] -> begin)
				{
					VS_print_log(AUDIO_OVERLAP);
					return;
				}
			}

			int repetition_begin = -1, repetition_end = -1;
			for(int j = vs -> image_count - 1; j >= 0; --j)
			{
				if(repetition_end < 0 && vs -> image_info[ vs -> image_pos[j] ] -> nb_repetition < 0)
					repetition_end = j + 1;
				if(repetition_end < 0)
					continue;
				if(j == 0 || vs -> image_info[ vs -> image_pos[j - 1] ] -> nb_repetition <= 0)
					repetition_begin = j + 1;
				if(repetition_begin >= 0)
				{
					if(  begin <= repetition_end && end >= repetition_begin &&
					   !(begin <= repetition_begin && end >= repetition_end) )
					{
						VS_print_log(REPETITION_INTERSECT_AUDIO);
						return;
					}
					repetition_end = -1;
					repetition_begin = -1;
				}
			}
			
			for(int i = vs -> audio_info[index - 1] -> begin - 1; i < vs -> audio_info[index - 1] -> end; ++i)
			{
				for(int j = 0; j <= abs(vs -> image_info[vs -> image_pos[i]] -> nb_repetition); ++j)
					vs -> image_info[vs -> image_pos[i]] -> duration[j] = 3.0;
			}
			if(begin == end && vs -> image_info[vs -> image_pos[begin - 1]] -> nb_repetition == 0)
				vs -> image_info[vs -> image_pos[begin - 1]] -> duration[0] = vs -> audio_info[index - 1] -> duration[0];
			else
			{
				for(int i = begin - 1; i < end; ++i)
					vs -> image_info[vs -> image_pos[i]] -> duration[0] *= (-1);
			}

			vs -> audio_info[index - 1] -> partitioned = false;
			vs -> audio_info[index - 1] -> begin = begin;
			vs -> audio_info[index - 1] -> end = end;
			break;
		}
		
		case AVTYPE_BG_IMAGE:
		{
			int begin = arg1, end = arg2;
			vs -> bg_info[index - 1] -> begin = begin;
			vs -> bg_info[index - 1] -> end = end;
			break;
		}
	}
	
	VS_print_log(FILE_MODIFIED);
	settings(vs, L"");
}

bool modify_file_parse_input(VisualScores *vs, wchar_t *cmd, AVType *type, 
                             int *index, int *arg1, int *arg2)
{
	wchar_t tag[10], str_arg1[10], str_arg2[10];
	size_t pos1 = 0, pos2 = 0;
	
	while(pos1 < wcslen(cmd) && cmd[pos1] != L' ')
		++pos1;
	wcsncpy(tag, cmd, pos1);
	tag[pos1] = L'\0';
	
	while(pos1 < wcslen(cmd) && cmd[pos1] == L' ')
		++pos1;
	pos2 = pos1;
	while(pos2 < wcslen(cmd) && cmd[pos2] != L' ')
		++pos2;
	wcsncpy(str_arg1, cmd + pos1, pos2 - pos1);
	str_arg1[pos2 - pos1] = L'\0';

	while(pos2 < wcslen(cmd) && cmd[pos2] == L' ')
		++pos2;
	wcscpy(str_arg2, cmd + pos2);

	size_t pos;
	int max_index;
	switch(tag[0])
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
			if(tag[1] == L'G')
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
	
	wchar_t *pEnd;
	*index = wcstol(tag + pos, &pEnd, 10);
	if(*pEnd != L'\0' || tag[pos] < '0' || tag[pos] > '9' 
	   || *index <= 0 || *index > max_index)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}
	
	if(*type == AVTYPE_IMAGE)
	{
		*arg1 = wcstol(str_arg1, &pEnd, 10);
		if(*pEnd != L'\0' || str_arg1[0] < '0' || str_arg1[0] > '9' 
		   || *arg1 < 0 || *arg1 > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}
		
		*arg2 = -1;
		if(wcslen(str_arg2) != 0)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}
	}
	else
	{
		*arg1 = wcstol(str_arg1, &pEnd, 10);
		if(*pEnd != L'\0' || str_arg1[0] < '0' || str_arg1[0] > '9' 
		   || *arg1 <= 0 || *arg1 > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}
		
		*arg2 = wcstol(str_arg2, &pEnd, 10);
		if(*pEnd != L'\0' || str_arg2[0] < '0' || str_arg2[0] > '9' 
		   || *arg2 < *arg1 || *arg2 > vs -> image_count)
		{
			VS_print_log(INVALID_INPUT);
			return false;
		}
	}

	return true;
}

void set_repetition(VisualScores *vs, wchar_t *cmd)
{
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	int begin, end, times;
	bool valid = set_repetition_parse_input(vs, cmd, &begin, &end, &times);
	if(!valid)  return;
	
	bool overlap = false, coincide = true;
	for(int i = begin; i <= end; ++i)
	{
		if(vs -> image_info[vs -> image_pos[i - 1]] -> nb_repetition != 0)
		{
			overlap = true;
			break;
		}
	}
	
	if(vs -> image_info[vs -> image_pos[end - 1]] -> nb_repetition >= 0)
		coincide = false;
	for(int i = begin; i < end; ++i)
	{
		if(vs -> image_info[vs -> image_pos[i - 1]] -> nb_repetition <= 0)
		{
			coincide = false;
			break;
		}
	}
	
	if(overlap && (!coincide))
	{
		VS_print_log(REPETITION_OVERLAP);
		return;
	}
	
	for(int j = 0; j < vs -> audio_count; ++j)
	{
		if(  vs -> audio_info[j] -> begin <= end && vs -> audio_info[j] -> end >= begin &&
		   !(vs -> audio_info[j] -> begin >= begin && vs -> audio_info[j] -> end <= end) )
		{
			VS_print_log(REPETITION_INTERSECT_AUDIO);
			return;
		}
	}

	for(int i = begin; i <= end; ++i)
	{
		vs -> image_info[vs -> image_pos[i - 1]] -> nb_repetition = times * ( (i == end) ? (-1) : 1 );
		for(int j = 1; j <= times; ++j)
			vs -> image_info[vs -> image_pos[i - 1]] -> duration[j] = 
				vs -> image_info[vs -> image_pos[i - 1]] -> duration[0];
	}
	
	if(begin == end)
	{
		for(int i = 0; i < vs -> audio_count; ++i)
		{
			if(vs -> audio_info[i] -> begin == begin && vs -> audio_info[i] -> end == begin)
			for(int j = 0; j <= times; ++j)
				vs -> image_info[vs -> image_pos[begin - 1]] -> duration[j] *= (-1);
			break;
		}
	}
	
	VS_print_log(REPETITION_SET);
	settings(vs, L"");
}

bool set_repetition_parse_input(VisualScores *vs, wchar_t *cmd, int *begin, int *end, int *times)
{
	wchar_t str_begin[10], str_end[10], str_times[10];
	size_t pos1 = 0, pos2 = 0;
	while(pos1 < wcslen(cmd) && cmd[pos1] != L' ')
		++pos1;
	wcsncpy(str_begin, cmd, pos1);
	str_begin[pos1] = L'\0';
	
	wchar_t *pEnd;
	*begin = wcstol(str_begin, &pEnd, 10);
	if(str_begin[0] < '0' || str_begin[0] > '9' || *pEnd != L'\0' ||
	   *begin <= 0 || *begin > vs -> image_count)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}

	while(pos1 < wcslen(cmd) && cmd[pos1] == L' ')
		++pos1;
	pos2 = pos1;
	while(pos2 < wcslen(cmd) && cmd[pos2] != L' ')
		++pos2;
	wcsncpy(str_end, cmd + pos1, pos2 - pos1);
	str_end[pos2 - pos1] = L'\0';

	*end = wcstol(str_end, &pEnd, 10);
	if(str_end[0] < '0' || str_end[0] > '9' || *pEnd != L'\0' ||
	   *end < *begin || *end > vs -> image_count)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}

	while(pos2 < wcslen(cmd) && cmd[pos2] == L' ')
		++pos2;
	wcscpy(str_times, cmd + pos2);
	if(str_times[0] == L'\0')
	{
		*times = 1;
		return true;
	}
	
	*times = wcstol(str_times, &pEnd, 10);
	int times_max = (FILE_LIMIT - vs -> image_count) / (*end - *begin + 1) + 1;
	if(str_times[0] < '0' || str_times[0] > '9' || *pEnd != L'\0' || 
	   *times < 0 || *times > times_max)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}

	return true;
}

void set_duration(VisualScores *vs, wchar_t *cmd)
{
	if(vs -> image_count == 0)
	{
		VS_print_log(IMAGE_NOT_LOADED);
		return;
	}
	
	int index;
	double time;
	bool valid = set_duration_parse_input(vs, cmd, &index, &time);
	if(!valid)  return;
	
	int pos = vs -> image_pos[index - 1];
	for(int i = 0; i < vs -> audio_count; ++i)
	{
		if(vs -> audio_info[i] -> begin - 1 <= pos && vs -> audio_info[i] -> end - 1 >= pos)
		{
			VS_print_log(CAN_NOT_SET_DURATION);
			return;
		}
	}
	
	for(int i = 0; i <= abs(vs -> image_info[pos] -> nb_repetition); ++i)
		vs -> image_info[pos] -> duration[i] = time;
	VS_print_log(DURATION_SET);
	settings(vs, L"");
}

bool set_duration_parse_input(VisualScores *vs, wchar_t *cmd, int *index, double *time)
{
	if(cmd[0] != L'I')
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}
	
	wchar_t str_index[10], str_time[STRING_LIMIT];
	size_t pos = 1;
	
	while(pos < wcslen(cmd) && cmd[pos] != L' ')
		++pos;
	wcsncpy(str_index, cmd + 1, pos - 1);
	str_index[pos] = L'\0';

	while(pos < wcslen(cmd) && cmd[pos] == L' ')
		++pos;
	wcscpy(str_time, cmd + pos);
	
	wchar_t *pEnd;
	*index = wcstol(str_index, &pEnd, 10);
	if(*pEnd != L'\0' || str_index[0] < '0' || str_index[0] > '9' 
	   || *index <= 0 || *index > vs -> image_count)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}

	*time = wcstod(str_time, &pEnd);
	if(*pEnd != L'\0' || str_time[0] < '0' || str_time[0] > '9' || *time < 0.1 || *time > TIME_LIMIT)
	{
		VS_print_log(INVALID_INPUT);
		return false;
	}
	
	return true;
}
