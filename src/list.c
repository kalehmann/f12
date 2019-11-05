/* Use the not posix conform function asprintf */
#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "f12.h"
#include "format.h"
#include "list.h"

#define LIST_DATETIME_WIDTH 21
#define LIST_DATE_WIDTH 12

static const char *LIST_FORMAT =
	"%s%*s|-> %-*s" "%6$*7$s" "%8$*9$s" "%10$*11$s" "%12$*13$s\n";
static const char *LIST_DATETIME_FORMAT = "%Y-%m-%d %H:%M:%S";
static const char *LIST_DATE_FORMAT = "%Y-%m-%d";

enum f12_error list_entry(struct f12_directory_entry *entry, char **output,
			  struct f12_list_arguments *args)
{
	enum f12_error err;
	size_t max_name_width, max_size_width;

	list_width(entry, 4, 2, &max_name_width, args->recursive);
	max_size_width = list_size_len(entry, args->recursive);

	if (!f12_is_directory(entry)) {
		err = list_f12_entry(entry, output, args, 0, max_name_width,
				     max_size_width);
		if (F12_SUCCESS != err) {
			return err;
		}
	}

	for (int i = 0; i < entry->child_count; i++) {
		err = list_f12_entry(&entry->children[i], output, args, 0,
				     max_name_width, max_size_width);
		if (F12_SUCCESS != err) {
			return err;
		}
	}

	return F12_SUCCESS;
}

enum f12_error list_f12_entry(struct f12_directory_entry *entry, char **output,
			      struct f12_list_arguments *args, int depth,
			      int name_width, int size_width)
{
	enum f12_error err;
	int name_padding = 0;
	char creat_buf[LIST_DATETIME_WIDTH] = "";
	char mod_buf[LIST_DATETIME_WIDTH] = "";
	char acc_buf[LIST_DATE_WIDTH] = "";
	char *size_str = "";
	int creat_pad = 0, mod_pad = 0, acc_pad = 0, size_pad = 0;
	long usecs;
	time_t timer;
	struct tm *timeinfo;
	uint16_t date, time;
	uint8_t time_ms;

	if (f12_entry_is_empty(entry) && NULL != entry->parent) {
		return F12_SUCCESS;
	}

	if (args->creation_date) {
		date = entry->CreateDate;
		time = entry->PasswordHashOrCreateTime;
		time_ms = entry->CreateTimeOrFirstCharacter;
		usecs = f12_read_entry_timestamp(date, time, time_ms);
		timer = usecs / 1000000;
		timeinfo = localtime(&timer);
		strftime(creat_buf, LIST_DATETIME_WIDTH, LIST_DATETIME_FORMAT,
			 timeinfo);

		creat_pad = strlen(creat_buf) + 2;
		name_padding = name_width - 4 - depth;
	}
	if (args->modification_date) {
		date = entry->LastModifiedDate;
		time = entry->LastModifiedTime;
		time_ms = 0;
		usecs = f12_read_entry_timestamp(date, time, time_ms);
		timer = usecs / 1000000;
		timeinfo = localtime(&timer);
		strftime(mod_buf, LIST_DATETIME_WIDTH, LIST_DATETIME_FORMAT,
			 timeinfo);

		mod_pad = strlen(mod_buf) + 2;
		name_padding = name_width - 4 - depth;
	}
	if (args->access_date) {
		date = entry->OwnerIdOrLastAccessDate;
		time = 0;
		time_ms = 0;
		usecs = f12_read_entry_timestamp(date, time, time_ms);
		timer = usecs / 1000000;
		timeinfo = localtime(&timer);
		strftime(acc_buf, LIST_DATE_WIDTH, LIST_DATE_FORMAT, timeinfo);

		acc_pad = strlen(acc_buf) + 2;
		name_padding = name_width - 4 - depth;
	}

	if (args->with_size) {
		size_str = format_bytes(entry->FileSize);
		size_pad = size_width + 1;
		name_padding = name_width - 4 - depth;
	}

	char *name = f12_get_file_name(entry);
	if (NULL == name) {
		return F12_ALLOCATION_ERROR;
	}

	char *temp = *output;
	asprintf(output, LIST_FORMAT, temp, depth, "",
		 name_padding, name,
		 creat_buf, creat_pad, mod_buf, mod_pad, acc_buf, acc_pad,
		 size_str, size_pad);
	free(name);
	free(temp);
	if (args->with_size) {
		free(size_str);
	}

	if (args->recursive && f12_is_directory(entry)
	    && !f12_is_dot_dir(entry)) {
		for (int i = 0; i < entry->child_count; i++) {
			err = list_f12_entry(&entry->children[i], output, args,
					     depth + 2, name_width, size_width);
			if (F12_SUCCESS != err) {
				return err;
			}
		}
	}

	return F12_SUCCESS;
}

enum f12_error list_width(struct f12_directory_entry *root_entry,
			  size_t prefix_len, size_t indent_len, size_t * width,
			  int recursive)
{
	size_t line_width = prefix_len, max_width = 0, max_child_width = 0;
	struct f12_directory_entry *entry = NULL;
	char *entry_name = NULL;

	if (!f12_is_directory(root_entry)) {
		entry_name = f12_get_file_name(root_entry);
		if (NULL == entry_name) {
			return F12_ALLOCATION_ERROR;
		}
		*width = strlen(entry_name) + prefix_len;
		free(entry_name);

		return F12_SUCCESS;
	}

	if (0 == root_entry->child_count) {
		return F12_SUCCESS;
	}

	for (int i = 0; i < root_entry->child_count; i++) {
		entry = &(root_entry->children[i]);
		if (f12_entry_is_empty(entry)) {
			continue;
		}

		entry_name = f12_get_file_name(entry);
		if (NULL == entry_name) {
			return F12_ALLOCATION_ERROR;
		}
		line_width += strlen(entry_name);
		free(entry_name);

		if (f12_is_directory(entry) && !f12_is_dot_dir(entry)
		    && recursive && entry->child_count > 0) {
			list_width(entry, prefix_len + indent_len, indent_len,
				   &max_child_width, recursive);
			if (max_child_width > line_width) {
				line_width = max_child_width;
			}
		}

		if (line_width > max_width) {
			max_width = line_width;
		}

		line_width = prefix_len;
	}

	*width = max_width;

	return F12_SUCCESS;
}

size_t list_size_len(struct f12_directory_entry * root_entry, int recursive)
{
	size_t cur_len = 0, max_len = 0;
	struct f12_directory_entry *entry = NULL;

	if (!f12_is_directory(root_entry)) {
		return format_bytes_len(root_entry->FileSize);
	}

	if (0 == root_entry->child_count) {
		return 0;
	}

	for (int i = 0; i < root_entry->child_count; i++) {
		entry = &(root_entry->children[i]);
		if (f12_entry_is_empty(entry) || f12_is_dot_dir(entry)) {
			continue;
		}

		if (f12_is_directory(entry) && recursive) {
			cur_len = list_size_len(entry, recursive);
		} else {
			cur_len = format_bytes_len(entry->FileSize);
		}

		if (cur_len > max_len) {
			max_len = cur_len;
		}
	}

	return max_len;
}
