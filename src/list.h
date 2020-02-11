#ifndef F12_LIST_H
#define F12_LIST_H

#include "f12.h"
#include "libfat12/libfat12.h"

/**
 * @return the number of digits the given number has in the decimal system 
 */
size_t _f12_digit_count(long number);

/**
 * Returns the length of the string returned by the function _f12_format_bytes
 * for the number passed in the bytes parameter.
 *
 * @param bytes the argument passed to the _f12_format_bytes function to return
 *              the string length for
 * @return the length of the string returned by the function _f12_format_bytes
 * for the number passed in the bytes parameter. 
 */
size_t _f12__f12_format_bytes_len(size_t bytes);

/**
 * Lists a directory or single file on a fat 12 image. If it is a directory, it
 * lists the directory itself and all its childs.
 *
 * @param entry a pointer to the lf12_directory_entry structure describing the
 *              entry to list
 * @param output a pointer to the string with the list output. The destination
 *               of the pointer must be freed.
 * @param args a pointer to the structure with the list arguments
 * @return any error that occurred or F12_SUCCESS
 */
enum lf12_error _f12_list_entry(struct lf12_directory_entry *entry,
				char **output, struct f12_list_arguments *args);

/**
 * Lists a entry from a directory table of a fat 12 file system. If the entry
 * is a directory, it lists only its childs. If the entry is a file it only lists 
 * the entry itself
 * 
 * @param entry a pointer to the lf12_directory_entry structure describing the
 *              entry to list
 * @param output a pointer to the string with the list output. The destination
 *               of the pointer must be freed.
 * @param args a pointer to the structure with the list arguments
 * @param depth the depth of indentation of the current given entry in the list
 * @param name_width the width of the longest childs filename including the
 *                   spaces for indentation
 * @param size_width the width of the longest formatted file size
 * @return any error that occurred or F12_SUCCESS
 */
enum lf12_error _f12_list_f12_entry(struct lf12_directory_entry *entry,
				    char **output,
				    struct f12_list_arguments *args, int depth,
				    int name_width, int size_width);

/**
 * Gets the width of the longest filename of all children of the root entry.
 *
 * @param root_entry a pointer to the lf12_directory_entry structure of the entry
 *                   to find the longest child for
 * @param prefix_len the length of the prefix of each line
 * @param indent_len the additional length per indentation level
 * @param width a pointer where the width of the longest filename will get
 *              written into
 * @param recursive whether to search recursive or not
 * @return any error that occurred or F12_SUCCESS
 */
enum lf12_error _f12_list_width(struct lf12_directory_entry *root_entry,
				size_t prefix_len, size_t indent_len,
				size_t *width, int recursive);

/**
 * Gets the width of the longest formatted file size of all children of the root
 * entry.
 *
 * @param root_entry a pointer to the lf12_directory_entry structure of the entry
 *                   to find the longest formatted file size for
 * @param recursive whether to search recursive or not
 * @return the size of the longest formatted file size
 */
size_t _f12_list_size_len(struct lf12_directory_entry *root_entry,
			  int recursive);

#endif
