#! /usr/bin/env bash

set -e

if [[ ${EUID} -ne 0 ]]; then
    echo "Must run as root"
    exit 1
fi

if [[ ${#} -ne 1 ]]; then
    echo "\
Invalid number of arguments.

Usage: $0 [ImageName]
"
fi

rm -rf ${1}.img
mkfs.msdos -C ${1}.img 1440 > /dev/null

LOOP_DEV=$(losetup -f)
losetup ${LOOP_DEV} ${1}.img
mkdir -p ${1}
mount -o loop ${LOOP_DEV} ${1}

mkdir -p ${1}/FOLDER1/SUBDIR
mkdir ${1}/FOLDER2

echo "12345678" > ${1}/FOLDER1/SUBDIR/SECRET.TXT
echo "MyData" > ${1}/FOLDER1/DATA.DAT
echo "a text" > ${1}/FOLDER2/TEXT.TXT
echo "Binary COntent" > ${1}/FILE.BIN

sync
umount ${1}
rm -rf ${1}
losetup -d ${LOOP_DEV}