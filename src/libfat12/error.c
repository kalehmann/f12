#include <errno.h>
#include <string.h>

#include "libfat12.h"

static char *ERR_NOT_A_DIR = "Not a directory";
static char *ERR_DIR_FULL = "Directory full";
static char *ERR_ALLOCATION_ERROR = "Allocation failure";
static char *ERR_IO = "Input Output failure";
static char *ERR_LOGIC = "An internal logic error occurred";
static char *ERR_IMAGE_FULL = "The image is full";
static char *ERR_FILE_NOT_FOUND = "The file was not found on the image";
static char *ERR_EMPTY_PATH = "Operation not permitted on the root directory";
static char *ERR_DIR_NOT_EMPTY = "Directory is not empty";
static char *ERR_SUCCESS = "Success";
static char *ERR_UNKNOWN = "Error unknown";
static char *ERR_DIR = "Target is a directory. Maybe use the recursive flag";

static int saved_errno = 0;
static int has_saved = 0;

void f12_save_errno(void)
{
	if (has_saved) {
		return;
	}

	has_saved = 1;
	saved_errno = errno;
}

char *f12_strerror(enum f12_error err)
{
	switch (err) {
	case F12_NOT_A_DIR:
		return ERR_NOT_A_DIR;
	case F12_DIR_FULL:
		return ERR_DIR_FULL;
	case F12_ALLOCATION_ERROR:
		return ERR_ALLOCATION_ERROR;
	case F12_IO_ERROR:
		if (has_saved) {
			return strerror(saved_errno);
		}
		return ERR_IO;
	case F12_LOGIC_ERROR:
		return ERR_LOGIC;
	case F12_IMAGE_FULL:
		return ERR_IMAGE_FULL;
	case F12_FILE_NOT_FOUND:
		return ERR_FILE_NOT_FOUND;
	case F12_EMPTY_PATH:
		return ERR_EMPTY_PATH;
	case F12_DIR_NOT_EMPTY:
		return ERR_DIR_NOT_EMPTY;
	case F12_SUCCESS:
		return ERR_SUCCESS;
	case F12_IS_DIR:
		return ERR_DIR;
	default:
		break;
	}

	if (has_saved) {
		return strerror(saved_errno);
	}
	return ERR_UNKNOWN;
}
