#include <string.h>
#include <stdlib.h>

#include "libfat12.h"

static size_t get_path_length(struct f12_directory_entry *entry)
{
        size_t path_length = 1;
        struct f12_directory_entry *tmp_entry = entry;

        do {
                for (int i=0; i<8; i++) {
                        if (tmp_entry->ShortFileName[i] != ' ') {
                                path_length++;
                        }
                }
                if (*tmp_entry->ShortFileExtension != ' ') {
                        // Add one byte for the dot
                        path_length++;
                }
                for (int i=0; i<3; i++) {
                        if (tmp_entry->ShortFileExtension[i] != ' ') {
                                path_length++;
                        }
                }
                tmp_entry = tmp_entry->parent;
                if (tmp_entry) {
                        // Add one byte for the slash
                        path_length++;
                }
        } while (tmp_entry && tmp_entry->parent);

        return path_length;
}

char *f12_get_file_name(struct f12_directory_entry *entry) {
    char name[13], *result;
    int length = 0;
    int i = 0;

    while (i < 8 &&
           entry->ShortFileName[i] != 0 &&
           entry->ShortFileName[i] != ' ') {
        name[length++] = entry->ShortFileName[i++];
    }
    if (entry->ShortFileExtension[0] != ' ') {
        name[length++] = '.';
        i = 0;

        while (i < 3 &&
               entry->ShortFileExtension[i] != 0 &&
               entry->ShortFileExtension[i] != ' ') {
            name[length++] = entry->ShortFileExtension[i++];
        }
    }
    name[length++] = 0;

    result = malloc(length);

    if (NULL == result) {
        return NULL;
    }

    memcpy(result, name, length);

    return result;
}

char *f12_convert_name(char *name) {
    int i = 0;
    char *converted_name = malloc(11), c;

    if (NULL == converted_name) {
        return NULL;
    }

    for (int i = 0; i < 11; i++) {
        converted_name[i] = ' ';
    }

    while ((c = *(name++)) != '\0') {
        if (c == '.' && *name != '\0' && *name != '.') {
            i = 8;
            continue;
        }
        converted_name[i++] = c;
    }

    return converted_name;
}

enum f12_error f12_get_entry_path(struct f12_directory_entry *entry,
                                  char **path)
{
    size_t path_length = get_path_length(entry), name_length = 0;
    char *entry_name = NULL;
    struct f12_directory_entry *tmp_entry = entry;

    *path = malloc(path_length);

    if (NULL == *path) {
        return F12_ALLOCATION_ERROR;
    }

    (*path)[path_length - 1] = 0;
    (*path) += path_length - 1;

    do {
        entry_name = f12_get_file_name(tmp_entry);
        if (NULL == entry_name) {
                free(*path);

                return F12_ALLOCATION_ERROR;
        }
        name_length = strlen(entry_name);
        *path -= name_length;
        memcpy(*path, entry_name, name_length);
        free(entry_name);
        tmp_entry = tmp_entry->parent;
        if (NULL != tmp_entry) {
                (*path)--;
                *(*path) = '/';
        }
    } while (tmp_entry && tmp_entry->parent);


    return F12_SUCCESS;
}
