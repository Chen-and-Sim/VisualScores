/** 
 * VisualScores header file: visualscores.h
 * Defines struct VisualScores, which contains three tracks of the video file.
 * Declares function directly called by user input.
 */

#ifndef VISUALSCORES_H
#define VISUALSCORES_H

#include <io.h>
#include <stdbool.h>
#include <windows.h>

#include "avinfo.h"

#define FILE_LIMIT 300  /* maximum number of files in each track */
#define TIME_LIMIT 10000  /* maximum length of the video file */

/**
 * ALWAYS NOTICE THAT THE INDEX OF USER INPUT AND TAG STARTS FROM 1, BUT THE
 * INDEX OF ALL VARIABLES IN A VISUALSCORES OBJECT STARTS FROM 0.
 */
typedef struct VisualScores
{
	AVInfo **image_info;  /* image track */
	AVInfo **audio_info;  /* audio track */
	AVInfo **bg_info;     /* background image track */
	AVInfo  *video_info;  /* video file */

	int image_count;
	int audio_count;
	int bg_count;
	
	/**
	 * This variable gives the position of image files in "image_info" in the 
	 * order of the image track.
	 * For example, we have n image files "a_1.png", "a_2.png", ... , "a_n.png"
	 * loaded in "image_info", and the i-th image in the image track (or the 
	 * image tagged Ii) is "a_j.png". In this case, image_pos[i - 1] = j.
	 */
	int *image_pos;

} VisualScores;

/* name of commands and corrsponding functions */
#define COMMAND_COUNT 15
extern const char short_command[COMMAND_COUNT][5];
extern const char long_command[COMMAND_COUNT][10];
extern void (*functions[COMMAND_COUNT]) (VisualScores *, char *);

/* You should always call this function when initializing an VisualScore object. */
extern VisualScores *VS_init();

/* You should always call this function when destroying an VisualScore object. */
extern void VS_free(VisualScores *vs);

/* basic operations */
extern void about(VisualScores *vs, char *cmd);
extern void help(VisualScores *vs, char *cmd);
extern void switch_language(VisualScores *vs, char *cmd);
extern void quit(VisualScores *vs, char *cmd);
extern void settings(VisualScores *vs, char *cmd);

/**
 * Used in partition ond video export. Records the correct indexes of image files after 
 * repeating to 'rec_index' and returns the size of 'rec_index'.
 */
extern int  fill_index(VisualScores *vs, int begin, int end, int *rec_index);

/**
 * Return true if the extension of the filename matches one of the extensions we support; 
 * otherwise return false.
 */
extern bool has_image_ext(char *filename);
extern bool has_audio_ext(char *filename);
extern bool has_video_ext(char *filename);

/* Load an image to the image track. */
extern void load(VisualScores *vs, char *cmd);
extern bool load_check_input(VisualScores *vs, char *cmd, char *filename, int *offset);

/* Load all images in the folder to the image track. */
extern void load_all(VisualScores *vs, char *cmd);

/* Load an image to the background track or load an audio file to the audio track. */
extern void load_other(VisualScores *vs, char *cmd);
extern bool load_other_check_input(VisualScores *vs, char *cmd, char *filename, int *begin, int *end);

/* Delete a file. */
extern void delete_file(VisualScores *vs, char *cmd);
extern bool delete_file_check_input(VisualScores *vs, char *cmd, AVType *type, int *index);

/* Modify a file. If it is an image file, its position is modified; otherwise its range is modified. */
extern void modify_file(VisualScores *vs, char *cmd);
extern bool modify_file_check_input(VisualScores *vs, char *cmd, AVType *type, 
                                    int *index, int *arg1, int *arg2);

/* Set the number of repetition of images. */
extern void set_repetition(VisualScores *vs, char *cmd);
extern bool set_repetition_check_input(VisualScores *vs, char *cmd, int *begin, int *end, int *times);

/* Set the duration of an image file not in the range of any audio file. */
extern void set_duration(VisualScores *vs, char *cmd);
extern bool set_duration_check_input(VisualScores *vs, char *cmd, int *index, double *time);

#define ID_ENTER 1
#define ID_ESCAPE 2

/* Partition an audio file and determine the duration of images in the range of the audio file. */
extern void partition_audio(VisualScores *vs, char *cmd);
extern bool partition_audio_check_input(VisualScores *vs, char *cmd, int *index);

/* Sets the duration of each image file from rec_duration. */
extern void register_duration(VisualScores *vs, int total_partition, int *rec_index, double *rec_duration);

/* event processing functions */
extern void do_painting(HWND hWnd, HBITMAP *hBitmap);
/* This function returns true if we want to exit the message loop. */
extern bool enter_pressed(HWND hWnd, HBITMAP *hBitmap, VisualScores *vs, AVInfo *audio_info, 
                          int *partition_count, int total_partition, int *rec_index, 
								  clock_t *begin_time, clock_t *prev_time, double *rec_duration);
extern void escape_pressed(HWND hWnd, HBITMAP *hBitmap);

/* Discard the partition done to an audio file. */
extern void discard_partition(VisualScores *vs, char *cmd);

/* Determine the filename of the video file from the argument [Path] specified by user input. */
extern void get_video_filename(char *dest, char *src);

/* Export the video file. */
extern void export_video(VisualScores *vs, char *cmd);
extern bool write_image_track(VisualScores *vs);
extern bool write_audio_track(VisualScores *vs);

#endif /* VISUALSCORES_H */
