#ifndef LF12_PATH_H
#define LF12_PATH_H

/**
 * Populates a f12_path structure from an array of directory and file names.
 *
 * @param input_parts a pointer to an array of file and directory names
 * @param part_count the number of file and directory names in the array
 * @param path a pointer to the f12_path structure to populate
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error _lf12_build_path(char **input_parts,
				int part_count, struct f12_path *path);

/**
 * Splits a filepath into its components.
 *
 * @param input a pointer to the filepath
 * @param input_parts a pointer a array of string pointers. That array will be
 * filled with the components of the input. After use free must be called on
 * the first element of the array and the array itself.
 * @param part_count a pointer to the variable where the number of parts gets
 * written into.
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error _lf12_split_input(const char *input,
				 char ***input_parts, int *part_count);

#endif
