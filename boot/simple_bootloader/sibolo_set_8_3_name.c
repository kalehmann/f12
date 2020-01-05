#include <string.h>

#include "simple_bootloader.h"

void sibolo_set_8_3_name(const char *name_8_3)
{
	memcpy(boot_simple_bootloader + SIMPLE_BOOTLOADER_NAME_OFFSET, name_8_3,
	       11);
}
