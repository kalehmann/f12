#include <stdlib.h>
#include <string.h>
#include "libfat12.h"

/*
 * Function: build_path
 * --------------------
 * Populates a f12_path structure from an array of directory and file names.
 *
 * input_parts: a pointer to an array of file and directory names
 * part_count: the number of file and directory names in the array
 * path: a pointer to the f12_path structure to populate
 *
 * returns: -1 on failure
 *           0 on success
 */
static int build_path(char **input_parts, int part_count, struct f12_path *path)
{
  path->name = f12_convert_name(input_parts[0]);
  path->short_file_name = path->name;
  path->short_file_extension = path->name + 8;
  path->descendant = malloc(sizeof(struct f12_path));

  if (NULL == path->descendant) {
    return -1;
  }

  if (1 == part_count) {
    path->descendant = NULL;
    return 0;
  }
  
  int res = build_path(&input_parts[1], part_count - 1,
		       path->descendant);
  if (-1 == res) {
    return -1;
  }
  path->descendant->ancestor = path;

  return 0;
}

/*
 * Function: split_input
 * ---------------------
 * Splits a filepath into its components.
 * 
 * input: a pointer to the filepath
 * input_parts: a pointer a array of string pointers. That array will be filled with
 *              the components of the input. After use free must be called on the
 *              first element of the array and the array itself.
 *
 * returns: the number of directories and files in the path and therefore the length
 *          of the input_parts array
 */
static int split_input(const char *input, char ***input_parts)
{
  if (input[0] == '/') {
    /* omit leading slash */
    input++;
  }
  if (input[0] == '\0') {
    return EMPTY_PATH;
  }
  
  int input_len = strlen(input) + 1, input_part_count = 1;
  char *rawpath = malloc(input_len);
  
  if (NULL == rawpath) {
    return -1;
  }

  memcpy(rawpath, input, input_len);

  if (rawpath[input_len - 2] == '/') {
    /* omit slash at the end of the input */
    rawpath[input_len -2] = '\0';
    input_len--;
  }  

  /* 
   * Replace all slashes with string terminators.
   * After that rawpath points to all the elements of the path as seperate strings.
   */
  for (int i=0; i<input_len; i++) {
    if (rawpath[i] == '/') {
      input_part_count++;
      rawpath[i] = '\0';
    }
  }

  *input_parts = malloc(sizeof(char *) * input_part_count);
  if (NULL == *input_parts) {
    free(rawpath);
    return -1;
  }

  (*input_parts)[0] = rawpath;

  int part = 0, i = 0;
  while (i < (input_len - 2)) {
    if (rawpath[i++] == '\0') {
      (*input_parts)[++part] = &rawpath[i];
    }
  }

  return input_part_count;
}

/*
 * Function: f12_entry_from_path
 * -----------------------------
 * Find a file or subdirectory in a directory from a path.
 *
 * entry: a pointer to a f12_directory_entry structure, that describes the 
 *        directory that should be searched for files
 * path: a pointer to a f12_path structure
 *
 * returns: a pointer to a f12_directory_entry structure describing the file 
 *          or subdirectory or FILE_NOT_FOUND if the path matches no file in
 *          the given directory  
 */
struct f12_directory_entry *f12_entry_from_path(struct f12_directory_entry *entry,
						struct f12_path *path)
{ 
  for (int i=0; i<entry->child_count; i++) {
    if (0 == memcmp(entry->children[i].ShortFileName, path->short_file_name, 8) &&
	0 == memcmp(entry->children[i].ShortFileExtension,
	       path->short_file_extension, 3)) {
      if (NULL == path->descendant) {
	return &entry->children[i];
      }
      return f12_entry_from_path(&entry->children[i], path->descendant);
    }
  }

  return FILE_NOT_FOUND;
}

/*
 * Function: f12_parse_path
 * ------------------------
 * Creates a f12_path structure from a string with a filepath.
 * 
 * input: a pointer to a string with the filepath to parse
 * path: a pointer to a pointer to a f12_path structure with the parsed path
 *       The structure must be freed!
 *
 * returns: EMPTY_PATH if input does not contain a file or directory name
 *          -1 on allocation failure
 *          0 on success
 */
int f12_parse_path(const char *input, struct f12_path **path)
{
  char **input_parts;
  int input_part_count = split_input(input, &input_parts);

  if (input_part_count == EMPTY_PATH) {
    return EMPTY_PATH;
  }
  
  *path = malloc(sizeof(struct f12_path));

  if (*path == NULL) {
    free(input_parts[0]);
    free(input_parts);
    return -1;
  }
  
  build_path(input_parts, input_part_count, *path);

  free(input_parts[0]);
  free(input_parts);
  
  return 0;
}

/*
 * Function: f12_path_get_parent
 * ------------------------
 * Determines if one of two given f12_path structure describes a parent directory
 * of the file or directory described by the other path.
 *
 * path_a: a pointer to a f12_path structure
 * path_b: a pointer to a f12_path structure
 *
 * returns: PATHS_EQUAL if both structures describe the same file path
 *          PATHS_SECOND if the second path describes a parent directory of the
 *          first path
 *          PATHS_FIRST if the first path describes a parent directory of the
 *          second path
 */
int f12_path_get_parent(struct f12_path *path_a, struct f12_path *path_b)
{ 
  while (0 == memcmp(path_a->name, path_b->name, 11)) {
    if (path_b->descendant == NULL) {
      if (path_a->descendant == NULL) {
	// Both are equal
	return PATHS_EQUAL;
      }
      
      return PATHS_SECOND;
    }

    if (path_a->descendant == NULL) {
      return PATHS_FIRST;
    }

    path_a = path_a->descendant;
    path_b = path_b->descendant; 
  }

  return PATHS_UNRELATED; 
}

/*
 * Function: f12_free_path
 * -----------------------
 * Frees a f12_path structure
 *
 * path: a pointer to the f12_path structure
 *
 * returns: 0 on success
 */
int f12_free_path(struct f12_path *path)
{
  if (path->descendant != NULL) {
    f12_free_path(path->descendant);
  }
  free(path->name);
  free(path);
  
  return 0;
}
