#ifndef LF12_NAME_P_H
#define LF12_NAME_P_H

/**
 * Get the length of the path for a file or directory inside an fat12 formated
 * image, including a leading slash and the null termination character.
 *
 * @param entry the directory entry to get the path length for
 * @return the length of the entries path
 */
size_t _lf12_get_path_length(struct lf12_directory_entry *entry);

/**
 * Sanitizes a character from a file name for the storage in a fat12 directory
 * entry.
 *
 * Valid characters, that are returned as they are include:
 *  - A-Z
 *  - 0-9
 *  - !           (exclamation mark)
 *  - #           (number sign)
 *  - $           (dollar sign)
 *  - %           (percent sign)
 *  - &           (ampersand)
 *  - '           (apostrophe)
 *  - "(" and ")" (left and right parenthesis)
 *  - @           (at sign)
 *  - ^           (caret)
 *  - "`"         (grave accent)
 *  - _           (underscore)
 *  - "-"         (hyphen minus)
 *  - "{" and "}" (left and right curly bracket)
 *  - ~           (tilde)
 *  - " "         (space)
 *
 * Lower case letters (a-z) are replaced by upper case letters and every other
 * invalid character by an underscore.
 *
 * @param character the character to be sanitized
 * @return the sanitized character.
 */
char _lf12_sanitize_file_name_char(char character);

#endif
