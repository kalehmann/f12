/* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "format.h"

#define STRLEN(s) ( sizeof(s)/sizeof(s[0]) - sizeof(s[0]) )

char *format_bytes(size_t bytes)
{
	char *out;

	if (bytes < 10000) {
		asprintf(&out, "%ld bytes", bytes);
	} else if (bytes < 10000000) {
		asprintf(&out, "%ld KiB  ", bytes / 1024);
	} else if (bytes < 10000000000) {
		asprintf(&out, "%ld MiB  ", bytes / (1024 * 1024));
	} else {
		asprintf(&out, "%ld GiB  ", bytes / (1024 * 1024 * 1024));
	}

	return out;
}

size_t digit_count(long number)
{
	long n = 10;

	for (int i = 1; i < 20; i++) {
		if (number < n) {
			return i;
		}
		n *= 10;
	}

	return 20;
}

size_t format_bytes_len(size_t bytes)
{
	if (bytes < 10000) {
		return digit_count(bytes) + STRLEN(" bytes");
	} else if (bytes < 10000000) {
		return digit_count(bytes / 1024) + STRLEN(" KiB  ");
	} else if (bytes < 10000000000) {
		return digit_count(bytes / (1024 * 1024)) + STRLEN(" MiB  ");
	}

	return digit_count(bytes / (1024 * 1024 * 1024)) + STRLEN(" GiB  ");
}

enum lf12_error dump_move(struct lf12_directory_entry *src,
			  struct lf12_directory_entry *dest, char **output)
{
	enum lf12_error err;
	char *tmp = NULL, *file_name = NULL, *dest_path = NULL, *src_path =
		NULL;
	struct lf12_directory_entry *tmp_entry = src, *child = NULL;
	int i = 0;
	size_t src_offset = 0;

	err = lf12_get_entry_path(dest, &dest_path);
	if (F12_SUCCESS != err) {
		return err;
	}

	if (!lf12_is_directory(tmp_entry)) {
		err = lf12_get_entry_path(src, &src_path);
		if (err != F12_SUCCESS) {
			free(dest_path);

			return err;
		}
		file_name = lf12_get_file_name(src);
		if (*output) {
			tmp = *output;
			asprintf(output, "%s%s -> %s/%s\n", *output,
				 src_path, dest_path, file_name);
			free(tmp);
		} else {
			asprintf(output, "%s -> %s/%s\n", src_path, dest_path,
				 file_name);
		}
		free(file_name);
		free(src_path);
		free(dest_path);

		return F12_SUCCESS;
	}

	err = lf12_get_entry_path(src, &tmp);
	if (err != F12_SUCCESS) {
		free(dest_path);

		return err;
	}
	src_offset = strlen(tmp);
	free(tmp);

	do {
		if (i >= tmp_entry->child_count) {
			i = 0;
			while (&tmp_entry->parent->children[i] != tmp_entry) {
				i++;
			}
			i++;
			tmp_entry = tmp_entry->parent;
		}

		child = &tmp_entry->children[i];

		if (lf12_is_dot_dir(child) || lf12_entry_is_empty(child)) {
			i++;
			continue;
		}
		err = lf12_get_entry_path(child, &src_path);
		if (err != F12_SUCCESS) {
			free(dest_path);

			return err;
		}
		tmp = *output;
		if (tmp) {
			asprintf(output, "%s%s", *output, src_path);
			free(tmp);
		} else {
			asprintf(output, "%s", src_path);
		}

		tmp = *output;
		file_name = lf12_get_file_name(src);
		asprintf(output, "%s -> %s/%s%s\n", *output,
			 dest_path, file_name, src_path + src_offset);
		free(file_name);
		free(tmp);
		free(src_path);

		if (lf12_is_directory(child)
		    && lf12_get_child_count(child) > 2
		    && !lf12_is_dot_dir(child)) {
			tmp_entry = child;
			i = 0;

			continue;
		}

		i++;
	}
	while (i < tmp_entry->child_count || tmp_entry != src);

	free(dest_path);

	return F12_SUCCESS;
}
