#include <stdlib.h>
#include <string.h>
#include "libfat12.h"

enum lf12_error _lf12_build_path(char **input_parts,
				 int part_count, struct lf12_path *path)
{
	enum lf12_error err;

	path->ancestor = NULL;

	char *name = lf12_convert_name(input_parts[0]);
	if (NULL == name) {
		return F12_ALLOCATION_ERROR;
	}

	path->name = malloc(12);
	if (NULL == path->name) {
		free(name);

		return F12_ALLOCATION_ERROR;
	}
	memcpy(path->name, name, 11);
	path->name[11] = '\0';

	path->short_file_name = malloc(9);
	if (NULL == path->short_file_name) {
		free(name);
		free(path->name);

		return F12_ALLOCATION_ERROR;
	}
	memcpy(path->short_file_name, name, 8);
	path->short_file_name[8] = '\0';

	path->short_file_extension = malloc(4);
	if (NULL == path->short_file_extension) {
		free(name);
		free(path->name);
		free(path->short_file_name);

		return F12_ALLOCATION_ERROR;
	}
	memcpy(path->short_file_extension, name + 8, 3);
	path->short_file_extension[3] = '\0';
	free(name);

	if (1 == part_count) {
		path->descendant = NULL;
		return F12_SUCCESS;
	}

	path->descendant = malloc(sizeof(struct lf12_path));
	if (NULL == path->descendant) {
		free(path->name);
		free(path->short_file_name);
		free(path->short_file_extension);

		return F12_ALLOCATION_ERROR;
	}

	err = _lf12_build_path(&input_parts[1], part_count - 1,
			       path->descendant);
	if (F12_SUCCESS != err) {
		free(path->name);
		free(path->short_file_name);
		free(path->short_file_extension);
		free(path->descendant);

		return err;
	}
	path->descendant->ancestor = path;

	return F12_SUCCESS;
}

enum lf12_error _lf12_split_input(const char *input,
				  char ***input_parts, int *part_count)
{
	if (input[0] == '/') {
		/* omit leading slash */
		input++;
	}
	if (input[0] == '\0') {
		return F12_EMPTY_PATH;
	}

	int input_len = strlen(input) + 1;
	*part_count = 1;
	char *rawpath = malloc(input_len);
	if (NULL == rawpath) {
		return F12_ALLOCATION_ERROR;
	}

	memcpy(rawpath, input, input_len);

	if (rawpath[input_len - 2] == '/') {
		/* omit slash at the end of the input */
		rawpath[input_len - 2] = '\0';
		input_len--;
	}

	/*
	 * Replace all slashes with string terminators.
	 * After that rawpath points to all the elements of the path as seperate strings.
	 */
	for (int i = 0; i < input_len; i++) {
		if (rawpath[i] == '/') {
			(*part_count)++;
			rawpath[i] = '\0';
		}
	}

	*input_parts = malloc(sizeof(char *) * (*part_count));
	if (NULL == *input_parts) {
		free(rawpath);
		return F12_ALLOCATION_ERROR;
	}

	(*input_parts)[0] = rawpath;

	int part = 0, i = 0;
	while (i < (input_len - 2)) {
		if (rawpath[i++] == '\0') {
			(*input_parts)[++part] = &rawpath[i];
		}
	}

	return F12_SUCCESS;
}

struct lf12_directory_entry *lf12_entry_from_path(struct lf12_directory_entry
						  *entry,
						  struct lf12_path *path)
{
	if (NULL == path) {
		return entry;
	}

	for (int i = 0; i < entry->child_count; i++) {
		if (0 == memcmp(entry->children[i].ShortFileName,
				path->short_file_name, 8) &&
		    0 == memcmp(entry->children[i].ShortFileExtension,
				path->short_file_extension, 3)) {
			if (NULL == path->descendant) {
				return &entry->children[i];
			}
			return lf12_entry_from_path(&entry->children[i],
						    path->descendant);
		}
	}

	return NULL;
}

enum lf12_error lf12_parse_path(const char *input, struct lf12_path **path)
{
	enum lf12_error err;

	char **input_parts = NULL;
	int input_part_count = 0;

	*path = NULL;
	err = _lf12_split_input(input, &input_parts, &input_part_count);

	if (F12_SUCCESS != err) {
		return err;
	}

	*path = malloc(sizeof(struct lf12_path));

	if (*path == NULL) {
		free(input_parts[0]);
		free(input_parts);

		return F12_ALLOCATION_ERROR;
	}

	err = _lf12_build_path(input_parts, input_part_count, *path);
	free(input_parts[0]);
	free(input_parts);

	if (F12_SUCCESS != err) {
		free(path);

		return err;
	}

	return F12_SUCCESS;
}

void lf12_free_path(struct lf12_path *path)
{
	if (NULL == path) {
		return;
	}

	if (NULL != path->descendant) {
		lf12_free_path(path->descendant);
	}

	free(path->name);
	free(path->short_file_name);
	free(path->short_file_extension);
	free(path);
}

enum lf12_path_relations lf12_path_get_parent(struct lf12_path *path_a,
					      struct lf12_path *path_b)
{
	if (NULL == path_a) {
		if (NULL == path_b) {
			return F12_PATHS_EQUAL;
		}

		return F12_PATHS_FIRST;
	}
	if (NULL == path_b) {
		return F12_PATHS_SECOND;
	}

	while (0 == memcmp(path_a->name, path_b->name, 11)) {
		if (path_b->descendant == NULL) {
			if (path_a->descendant == NULL) {
				// Both are equal
				return F12_PATHS_EQUAL;
			}

			return F12_PATHS_SECOND;
		}

		if (path_a->descendant == NULL) {
			return F12_PATHS_FIRST;
		}

		path_a = path_a->descendant;
		path_b = path_b->descendant;
	}

	return F12_PATHS_UNRELATED;
}

enum lf12_error lf12_path_create_directories(struct lf12_metadata *f12_meta,
					     struct lf12_directory_entry *entry,
					     struct lf12_path *path)
{
	enum lf12_error err;

	for (int i = 0; i < entry->child_count; i++) {
		if (0 == memcmp(entry->children[i].ShortFileName,
				path->short_file_name, 8) &&
		    0 == memcmp(entry->children[i].ShortFileExtension,
				path->short_file_extension, 3)) {
			if (!lf12_is_directory(&entry->children[i])) {
				return F12_NOT_A_DIR;
			}

			if (NULL == path->descendant) {
				return F12_SUCCESS;
			}

			return lf12_path_create_directories(f12_meta,
							    &entry->children[i],
							    path->descendant);
		}
	}

	// This directory does not exist, lets create it
	for (int i = 0; i < entry->child_count; i++) {
		if (entry->children[i].ShortFileName[0] == 0) {
			memcpy(&(entry->children[i].ShortFileName),
			       path->short_file_name, 8);
			memcpy(&(entry->children[i].ShortFileExtension),
			       path->short_file_extension, 3);
			entry->children[i].parent = entry;
			err = lf12_create_directory_table(f12_meta,
							  &entry->children[i]);
			if (err != F12_SUCCESS) {
				return err;
			}

			if (path->descendant == NULL) {
				return F12_SUCCESS;
			}

			return lf12_path_create_directories(f12_meta,
							    &entry->children[i],
							    path->descendant);
		}
	}

	return F12_DIR_FULL;
}
