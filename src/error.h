#ifndef F12_ERROR_H
#define F12_ERROR_H

/**
 * Enhanced sprintf. This is essentially the same as asprintf, except if strp
 * already points to an existing string pointer, it is freed.
 * 
 * Check the manual of asprintf for information about the parameters.
 */
int esprintf(char **strp, const char *fmt, ...);

#endif
