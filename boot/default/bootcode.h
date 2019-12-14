#ifndef BOOT_DEFAULT_BOOTCODE_H
#define BOOT_DEFAULT_BOOTCODE_H

/**
 * The (binary) default bootloader.
 *
 * It just prints a message that the medium is not bootable.
 */
extern unsigned char boot_default_bootcode_bin[];
/**
 * The size of the default bootloader.
 *
 * This value MUST always be 512.
 */
extern unsigned int boot_default_bootcode_bin_len;

#endif
