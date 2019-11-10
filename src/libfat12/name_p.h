#ifndef LF12_NAME_P_H
#define LF12_NAME_P_H

/**
 * Get the length of the path for a file or directory inside an fat12 formated
 * image, including a leading slash and the null termination character.
 *
 * @param entry the directory entry to get the path length for
 * @return the length of the entries path
 */
size_t _lf12_get_path_length(struct f12_directory_entry *entry);

#endif
