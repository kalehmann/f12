#include <string.h>
#include <stdlib.h>

#include "libfat12.h"

/**
 * Get a free entry of a directory.
 *
 * @param dir a pointer to the lf12_directory_entry structure describing
 * the directory.
 * @return a pointer to the first free entry or
 *         NULL if there are no free entries or the entry describes a file
 */
static struct lf12_directory_entry *get_free_entry(struct lf12_directory_entry
						   *dir)
{
	if (!lf12_is_directory(dir)) {
		return NULL;
	}

	for (int i = 0; i < dir->child_count; i++) {
		if (lf12_entry_is_empty(&dir->children[i])) {
			return &dir->children[i];
		}
	}

	return NULL;
}

int lf12_is_directory(struct lf12_directory_entry *entry)
{
	if (entry->FileAttributes & LF12_ATTR_SUBDIRECTORY)
		return 1;
	return 0;
}

int lf12_is_dot_dir(struct lf12_directory_entry *entry)
{
	if (!lf12_is_directory(entry)) {
		return 0;
	}

	if (0 == memcmp(entry->ShortFileName, ".       ", 8) &&
	    0 == memcmp(entry->ShortFileExtension, "   ", 3)) {

		return 1;
	}
	if (0 == memcmp(entry->ShortFileName, "..      ", 8) &&
	    0 == memcmp(entry->ShortFileExtension, "   ", 3)) {

		return 1;
	}

	return 0;
}

int lf12_entry_is_empty(struct lf12_directory_entry *entry)
{
	if (entry->ShortFileName[0] == 0)
		return 1;
	return 0;
}

int lf12_get_child_count(struct lf12_directory_entry *entry)
{
	int child_count = 0;

	if (!lf12_is_directory(entry)) {
		return 0;
	}

	for (int i = 0; i < entry->child_count; i++) {
		if (!lf12_entry_is_empty(&entry->children[i])) {
			child_count++;
		}
	}

	return child_count;
}

int lf12_get_file_count(struct lf12_directory_entry *entry)
{
	struct lf12_directory_entry *child;
	int file_count = 0;

	if (!lf12_is_directory(entry)) {
		return 0;
	}

	for (int i = 0; i < entry->child_count; i++) {
		child = &entry->children[i];
		if (lf12_is_directory(child)) {
			if (!lf12_is_dot_dir(child)) {
				file_count += lf12_get_file_count(child);
			}
		} else if (!lf12_entry_is_empty(child)) {
			file_count++;
		}
	}

	return file_count;
}

int lf12_get_directory_count(struct lf12_directory_entry *entry)
{
	int dir_count = 0;

	if (!lf12_is_directory(entry)) {
		return 0;
	}

	for (int i = 0; i < entry->child_count; i++) {
		if (lf12_is_directory(&entry->children[i]) &&
		    !lf12_is_dot_dir(&entry->children[i])) {
			dir_count++;
			dir_count +=
				lf12_get_directory_count(&entry->children[i]);
		}
	}

	return dir_count;
}

void lf12_free_entry(struct lf12_directory_entry *entry)
{
	if (!lf12_is_directory(entry) || lf12_is_dot_dir(entry)) {
		return;
	}

	for (int i = 0; i < entry->child_count; i++) {
		lf12_free_entry(&entry->children[i]);
	}
	free(entry->children);
}

enum lf12_error lf12_move_entry(struct lf12_directory_entry *src,
				struct lf12_directory_entry *dest)
{
	int dest_free_entries;
	int entries_to_move;
	struct lf12_directory_entry *child, *free_entry;

	if (!lf12_is_directory(dest)) {
		return F12_NOT_A_DIR;
	}

	dest_free_entries = dest->child_count - lf12_get_child_count(dest);

	if (0 == memcmp(src->ShortFileName, ".       ", 8) &&
	    0 == memcmp(src->ShortFileExtension, "   ", 3) &&
	    lf12_is_directory(src)) {
		entries_to_move = lf12_get_child_count(src->parent) - 2;
	} else {
		entries_to_move = 1;
	}

	if (entries_to_move > dest_free_entries) {
		return F12_DIR_FULL;
	}

	if (0 == memcmp(src->ShortFileName, ".       ", 8) &&
	    0 == memcmp(src->ShortFileExtension, "   ", 3) &&
	    lf12_is_directory(src)) {
		for (int i = 0; i < src->parent->child_count; i++) {
			child = &src->parent->children[i];
			if (lf12_entry_is_empty(child)
			    || lf12_is_dot_dir(child)) {
				continue;
			}
			child->parent = dest;

			for (int j = 0; j < dest->child_count; j++) {
				if (!lf12_entry_is_empty(&dest->children[j])) {
					continue;
				}
				memmove(&dest->children[j], child,
					sizeof(struct lf12_directory_entry));
				memset(child, 0,
				       sizeof(struct lf12_directory_entry));
			}
		}

		return 0;
	}

	src->parent = dest;
	if (NULL == (free_entry = get_free_entry(dest))) {
		return F12_DIR_FULL;
	}

	for (int i = 0; i < src->child_count; i++) {
		if (lf12_entry_is_empty(&src->children[i])) {
			continue;
		}
		src->children[i].parent = free_entry;
	}

	memmove(free_entry, src, sizeof(struct lf12_directory_entry));
	memset(src, 0, sizeof(struct lf12_directory_entry));

	return F12_SUCCESS;
}
