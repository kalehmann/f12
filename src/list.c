#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "error.h"
#include "f12.h"
#include "list.h"

#define LIST_DATETIME_WIDTH 21
#define LIST_DATE_WIDTH 12

static const char *LIST_FORMAT =
	"%s%*s|-> %-*s" "%6$*7$s" "%8$*9$s" "%10$*11$s" "%12$*13$s\n";
static const char *LIST_DATETIME_FORMAT = "%Y-%m-%d %H:%M:%S";
static const char *LIST_DATE_FORMAT = "%Y-%m-%d";

#define STRLEN(s) ( sizeof(s)/sizeof(s[0]) - sizeof(s[0]) )

size_t _f12_digit_count(long number)
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

size_t _f12__f12_format_bytes_len(size_t bytes)
{
	if (bytes < 10000) {
		return _f12_digit_count(bytes) + STRLEN(" bytes");
	} else if (bytes < 10000000) {
		return _f12_digit_count(bytes / 1024) + STRLEN(" KiB  ");
	} else if (bytes < 10000000000) {
		return _f12_digit_count(bytes / (1024 * 1024)) +
			STRLEN(" MiB  ");
	}

	return _f12_digit_count(bytes / (1024 * 1024 * 1024)) +
		STRLEN(" GiB  ");
}

enum lf12_error _f12_list_entry(struct lf12_directory_entry *entry,
				char **output, struct f12_list_arguments *args)
{
	enum lf12_error err;
	size_t max_name_width, max_size_width;

	_f12_list_width(entry, 4, 2, &max_name_width, args->recursive);
	max_size_width = _f12_list_size_len(entry, args->recursive);

	if (!lf12_is_directory(entry)) {
		err = _f12_list_f12_entry(entry, output, args, 0,
					  max_name_width, max_size_width);
		if (F12_SUCCESS != err) {
			return err;
		}
	}

	for (int i = 0; i < entry->child_count; i++) {
		err = _f12_list_f12_entry(&entry->children[i], output, args, 0,
					  max_name_width, max_size_width);
		if (F12_SUCCESS != err) {
			return err;
		}
	}

	return F12_SUCCESS;
}

enum lf12_error _f12_list_f12_entry(struct lf12_directory_entry *entry,
				    char **output,
				    struct f12_list_arguments *args, int depth,
				    int name_width, int size_width)
{
	enum lf12_error err;
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

	if (lf12_entry_is_empty(entry) && NULL != entry->parent) {
		return F12_SUCCESS;
	}

	if (LF12_ATTR_LFN == entry->FileAttributes) {
		// Ignore VFAT Long FileName entries
		return F12_SUCCESS;
	}

	if (args->creation_date) {
		date = entry->CreateDate;
		time = entry->PasswordHashOrCreateTime;
		time_ms = entry->CreateTimeOrFirstCharacter;
		usecs = lf12_read_entry_timestamp(date, time, time_ms);
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
		usecs = lf12_read_entry_timestamp(date, time, time_ms);
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
		usecs = lf12_read_entry_timestamp(date, time, time_ms);
		timer = usecs / 1000000;
		timeinfo = localtime(&timer);
		strftime(acc_buf, LIST_DATE_WIDTH, LIST_DATE_FORMAT, timeinfo);

		acc_pad = strlen(acc_buf) + 2;
		name_padding = name_width - 4 - depth;
	}

	if (args->with_size) {
		size_str = _f12_format_bytes(entry->FileSize);
		size_pad = size_width + 1;
		name_padding = name_width - 4 - depth;
	}

	char *name = lf12_get_entry_file_name(entry);
	if (NULL == name) {
		return F12_ALLOCATION_ERROR;
	}

	esprintf(output, LIST_FORMAT, *output, depth, "",
		 name_padding, name,
		 creat_buf, creat_pad, mod_buf, mod_pad, acc_buf, acc_pad,
		 size_str, size_pad);
	free(name);
	if (args->with_size) {
		free(size_str);
	}

	if (args->recursive && lf12_is_directory(entry)
	    && !lf12_is_dot_dir(entry)) {
		for (int i = 0; i < entry->child_count; i++) {
			err = _f12_list_f12_entry(&entry->children[i], output,
						  args, depth + 2, name_width,
						  size_width);
			if (F12_SUCCESS != err) {
				return err;
			}
		}
	}

	return F12_SUCCESS;
}

enum lf12_error _f12_list_width(struct lf12_directory_entry *root_entry,
				size_t prefix_len, size_t indent_len,
				size_t *width, int recursive)
{
	size_t line_width = prefix_len, max_width = 0, max_child_width = 0;
	struct lf12_directory_entry *entry = NULL;
	char *entry_name = NULL;

	if (!lf12_is_directory(root_entry)) {
		entry_name = lf12_get_entry_file_name(root_entry);
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
		if (lf12_entry_is_empty(entry)) {
			continue;
		}

		entry_name = lf12_get_entry_file_name(entry);
		if (NULL == entry_name) {
			return F12_ALLOCATION_ERROR;
		}
		line_width += strlen(entry_name);
		free(entry_name);

		if (lf12_is_directory(entry) && !lf12_is_dot_dir(entry)
		    && recursive && entry->child_count > 0) {
			_f12_list_width(entry, prefix_len + indent_len,
					indent_len, &max_child_width,
					recursive);
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

size_t _f12_list_size_len(struct lf12_directory_entry *root_entry,
			  int recursive)
{
	size_t cur_len = 0, max_len = 0;
	struct lf12_directory_entry *entry = NULL;

	if (!lf12_is_directory(root_entry)) {
		return _f12__f12_format_bytes_len(root_entry->FileSize);
	}

	if (0 == root_entry->child_count) {
		return 0;
	}

	for (int i = 0; i < root_entry->child_count; i++) {
		entry = &(root_entry->children[i]);
		if (lf12_entry_is_empty(entry) || lf12_is_dot_dir(entry)) {
			continue;
		}

		if (lf12_is_directory(entry) && recursive) {
			cur_len = _f12_list_size_len(entry, recursive);
		} else {
			cur_len = _f12__f12_format_bytes_len(entry->FileSize);
		}

		if (cur_len > max_len) {
			max_len = cur_len;
		}
	}

	return max_len;
}

int f12_list(struct f12_list_arguments *args, char **output)
{
	struct lf12_directory_entry *entry;
	enum lf12_error err;
	struct lf12_metadata *f12_meta = NULL;
	FILE *fp = NULL;
	struct lf12_path *path;
	int res;

	fp = fopen(args->device_path, "r+");
	if (EXIT_SUCCESS != (res = open_image(fp, &f12_meta, output))) {
		return res;
	}
	fclose(fp);
	fp = NULL;

	if (args->path == NULL || args->path[0] == '\0') {
		err = _f12_list_entry(f12_meta->root_dir, output, args);
		lf12_free_metadata(f12_meta);
		f12_meta = NULL;
		if (F12_SUCCESS != err) {
			return print_error(fp, f12_meta, output, "%s\n",
					   strerror(err));
		}

		return EXIT_SUCCESS;
	}

	err = lf12_parse_path(args->path, &path);
	if (F12_EMPTY_PATH == err) {
		err = _f12_list_entry(f12_meta->root_dir, output, args);
		lf12_free_metadata(f12_meta);
		f12_meta = NULL;
		if (F12_SUCCESS != err) {
			return print_error(fp, f12_meta, output, "%s\n",
					   strerror(err));
		}

		return EXIT_SUCCESS;
	}
	if (F12_SUCCESS != err) {
		return print_error(fp, f12_meta, output, "%s\n",
				   lf12_strerror(err));
	}

	entry = lf12_entry_from_path(f12_meta->root_dir, path);
	lf12_free_path(path);
	if (NULL == entry) {
		return print_error(fp, f12_meta, output, _("File not found\n"));
	}

	err = _f12_list_entry(entry, output, args);
	lf12_free_metadata(f12_meta);
	f12_meta = NULL;
	if (F12_SUCCESS == err) {
		return EXIT_SUCCESS;
	}

	return print_error(fp, f12_meta, output, "%s\n", lf12_strerror(err));
}
