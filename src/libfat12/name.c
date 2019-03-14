#include <string.h>
#include <stdlib.h>

#include "libfat12.h"

/**
 * Generates a human readable filename in the format name.extension from a fat12
 * directory entry.
 *
 * @param entry the directory entry to generate the file name from
 * @return a pointer to the file name that must be freed
 */
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

/**
 * Converts a human readable filename to the 8.3 format
 *
 * @param name a pointer to the human readable filename
 * @return a pointer to an array of 11 chars with the filename in the 8.3 format,
 *         that must be freed after use.
 */
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
