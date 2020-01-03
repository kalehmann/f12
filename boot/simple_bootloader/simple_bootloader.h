#ifndef BOOT_SIMPLE_BOOTLOADER_H
#define BOOT_SIMPLE_BOOTLOADER_H

#define SIMPLE_BOOTLOADER_NAME_OFFSET 498

/**
 * The (binary) simple bootloader.
 *
 * It can boot any file from the root directory of the fat12 image by putting
 * the 8.3 name of the file at the offset 498 of the bootloader.
 */
extern unsigned char boot_simple_bootloader[];
/**
 * The size of the simple bootloader.
 *
 * This value MUST always be 512.
 */
extern unsigned int boot_simple_bootloader_len;

/**
 * @param name_8_3 is the 8.3 name of the file to boot. Note that this file must
 *        be in the root directory of the filesystem.
 */
void sibolo_set_8_3_name(const char *name_8_3);

#endif
