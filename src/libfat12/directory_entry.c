#include <string.h>
#include <stdlib.h>

#include "libfat12.h"


/**
 * Get a free entry of a directory.
 *
 * @param dir a pointer to the f12_directory_entry structure describing the directory.
 * @return a pointer to the first free entry or
 *         NULL if there are no free entries or dir is a file
 */
static struct f12_directory_entry *
get_free_entry(struct f12_directory_entry *dir)
{
        if (!f12_is_directory(dir)) {
                return NULL;
        }

        for (int i = 0; i < dir->child_count; i++) {
                if (f12_entry_is_empty(&dir->children[i])) {
                        return &dir->children[i];
                }
        }

        return NULL;
}

/**
 * Check whether a f12_directory_entry structure describes a file or a directory
 *
 * @param entry a pointer to the f12_directory_entry structure
 * @return 1 if the structure describes a directory else 0
 */
int f12_is_directory(struct f12_directory_entry *entry)
{
        if (entry->FileAttributes & F12_ATTR_SUBDIRECTORY)
                return 1;
        return 0;
}

/**
 * Check if a f12_directory_entry structure describes a dot dir.
 * The dot directories . and .. are links to the current and the parent directory.
 *
 * @param entry a pointer to the f12_directory_entry structure.
 * @return 1 if the structure describes a dot directory else 0
 */
int f12_is_dot_dir(struct f12_directory_entry *entry)
{
        if (!f12_is_directory(entry)) {
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

/**
 * Check whether a f12_directory_entry structure describes a file/directory or is
 * empty.
 *
 * @param entry a pointer to the f12_direcotry_entry structure
 * @return 1 if the entry describes a directory else 0
 */
int f12_entry_is_empty(struct f12_directory_entry *entry)
{
        if (entry->ShortFileName[0] == 0)
                return 1;
        return 0;
}

/**
 * Get the number of used entries of a directory (not its subdirectories).
 *
 * @param entry a pointer to the f12_directory_entry structure of the directory
 * @return a pointer to the f12_directory_entry structure of the directory
 */
int f12_get_child_count(struct f12_directory_entry *entry)
{
        int child_count = 0;

        if (!f12_is_directory(entry)) {
                return F12_NOT_A_DIR;
        }

        for (int i = 0; i < entry->child_count; i++) {
                if (!f12_entry_is_empty(&entry->children[i])) {
                        child_count++;
                }
        }

        return child_count;
}

/**
 * Get the number of files in a directory and all its subdirectories.
 *
 * @param entry a pointer to the f12_directory_entry structure of the directory
 * @return the number of files in the directory and all its subdirectories or
 *         F12_NOT_A_DIR if entry is not a directory
 */
int f12_get_file_count(struct f12_directory_entry *entry)
{
        struct f12_directory_entry *child;
        int file_count = 0;

        if (!f12_is_directory(entry)) {
                return F12_NOT_A_DIR;
        }

        for (int i = 0; i < entry->child_count; i++) {
                child = &entry->children[i];
                if (f12_is_directory(child)) {
                        if (!f12_is_dot_dir(child)) {
                                file_count += f12_get_file_count(child);
                        }
                } else if (!f12_entry_is_empty(child)) {
                        file_count++;
                }
        }

        return file_count;
}

/**
 * Get the number of subdirectories (and their subdirectories) in a directory.
 *
 * @param entry a pointer to the f12_directory_entry structure of the directory
 * @return the number of subdirectories and their subdirectories in the
 *         directory or F12_NOT_A_DIR if entry is not a directory
 */
int f12_get_directory_count(struct f12_directory_entry *entry)
{
        int dir_count = 0;

        if (!f12_is_directory(entry)) {
                return F12_NOT_A_DIR;
        }

        for (int i = 0; i < entry->child_count; i++) {
                if (f12_is_directory(&entry->children[i]) &&
                    !f12_is_dot_dir(&entry->children[i])) {
                        dir_count++;
                        dir_count += f12_get_directory_count(
                                &entry->children[i]);
                }
        }

        return dir_count;
}

/**
 * Free a f12_directory_entry structure and all subsequent entries.
 * @param entry a pointer to the f12_directory_entry structure to free
 * @return 0
 */
void f12_free_entry(struct f12_directory_entry *entry)
{
        if (!f12_is_directory(entry) || f12_is_dot_dir(entry)) {
                return;
        }

        for (int i = 0; i < entry->child_count; i++) {
                f12_free_entry(&entry->children[i]);
        }
        free(entry->children);
}

/**
 * Remove the entry src from the parent and make it a child of the entry dest.
 * If src points to the "." entry, all siblings of the entry (except "..") will
 * be moved.
 *
 * @param src a pointer to the f12_directory_entry structure that should be moved
 * @param dest a pointer to the f12_directory_entry structure that should be the new
 *        parent of src
 * @return 0 on success
 *         F12_NOT_A_DIR if dest is a files and not a directory
 *         F12_DIR_FULL if all entries in dest are used
 */
int f12_move_entry(struct f12_directory_entry *src,
                   struct f12_directory_entry *dest)
{
        int dest_free_entries;
        int entries_to_move;
        struct f12_directory_entry *child, *free_entry;

        if (!f12_is_directory(dest)) {
                return F12_NOT_A_DIR;
        }

        dest_free_entries = dest->child_count - f12_get_child_count(dest);

        if (0 == memcmp(src->ShortFileName, ".       ", 8) &&
            0 == memcmp(src->ShortFileExtension, "   ", 3) &&
            f12_is_directory(src)) {
                entries_to_move = f12_get_child_count(src->parent) - 2;
        } else {
                entries_to_move = 1;
        }

        if (entries_to_move > dest_free_entries) {
                return F12_DIR_FULL;
        }

        if (0 == memcmp(src->ShortFileName, ".       ", 8) &&
            0 == memcmp(src->ShortFileExtension, "   ", 3) &&
            f12_is_directory(src)) {
                for (int i = 0; i < src->parent->child_count; i++) {
                        child = &src->parent->children[i];
                        if (f12_entry_is_empty(child) ||
                            f12_is_dot_dir(child)) {
                                continue;
                        }
                        child->parent = dest;

                        for (int j = 0; j < dest->child_count; j++) {
                                if (!f12_entry_is_empty(&dest->children[j])) {
                                        continue;
                                }
                                memmove(&dest->children[j], child,
                                        sizeof(struct f12_directory_entry));
                                memset(child, 0,
                                       sizeof(struct f12_directory_entry));
                        }
                }

                return 0;
        }

        src->parent = dest;
        if (NULL == (free_entry = get_free_entry(dest))) {
                return F12_DIR_FULL;
        }

        for (int i = 0; i < src->child_count; i++) {
                if (f12_entry_is_empty(&src->children[i])) {
                        continue;
                }
                src->children[i].parent = free_entry;
        }

        memmove(free_entry, src, sizeof(struct f12_directory_entry));
        memset(src, 0, sizeof(struct f12_directory_entry));

        return 0;
}
