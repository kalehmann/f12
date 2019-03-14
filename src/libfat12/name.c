#include <string.h>
#include <stdlib.h>

#include "libfat12.h"

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
