#ifndef F12_LIST_H
#define F12_LIST_H

#include "libfat12/libfat12.h"

/**
 * Lists a directory or single file on a fat 12 image. If it is a directory, it
 * lists the directory itself and all its childs.
 *
 * @param entry a pointer to the f12_directory_entry structure describing the
 *              entry to list
 * @param output a pointer to the string with the list output. The destination
 *               of the pointer must be freed.
 * @param args a pointer to the structure with the list arguments
 * @return any error that occurred or F12_SUCCESS
 */
enum f12_error list_entry(struct f12_directory_entry *entry, char **output,
			  struct f12_list_arguments *args);

/**
 * Lists a entry from a directory table of a fat 12 file system. If the entry
 * is a directory, it lists only its childs. If the entry is a file it only lists 
 * the entry itself
 * 
 * @param entry a pointer to the f12_directory_entry structure describing the
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
enum f12_error list_f12_entry(struct f12_directory_entry *entry, char **output,
			      struct f12_list_arguments *args, int depth,
			      int name_width, int size_width);

/**
 * Gets the width of the longest filename of all children of the root entry.
 *
 * @param root_entry a pointer to the f12_directory_entry structure of the entry
 *                   to find the longest child for
 * @param prefix_len the length of the prefix of each line
 * @param indent_len the additional length per indentation level
 * @param width a pointer where the width of the longest filename will get
 *              written into
 * @param recursive whether to search recursive or not
 * @return any error that occurred or F12_SUCCESS
 */
enum f12_error list_width(struct f12_directory_entry *root_entry,
			  size_t prefix_len, size_t indent_len, size_t *width,
			  int recursive);

/**
 * Gets the width of the longest formatted file size of all children of the root
 * entry.
 *
 * @param root_entry a pointer to the f12_directory_entry structure of the entry
 *                   to find the longest formatted file size for
 * @param recursive whether to search recursive or not
 * @return the size of the longest formatted file size
 */
size_t list_size_len(struct f12_directory_entry *root_entry, int recursive);

#endif
