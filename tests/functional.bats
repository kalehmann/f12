#! /usr/bin/env bats

setup() {
    if [[ ! -f "tests/fixtures/test.img.bak" ]]; then
        echo "Fixture tests/fixtures/test.img.bak not found"
        exit 1
    fi

    cp tests/fixtures/test.img.bak tests/fixtures/test.img
    mkdir tests/tmp
}

teardown() {
    rm -f tests/fixtures/test.img
    rm -rf tests/tmp
}

@test "I can get the help" {
    run ./src/f12 --help
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"The following options are available"* ]]
    [[ "$output" == *"put DEVICE"* ]]
    [[ "$output" == *"move DEVICE"* ]]
    [[ "$output" == *"list DEVICE"* ]]
    [[ "$output" == *"info DEVICE"* ]]
    [[ "$output" == *"get DEVICE"* ]]
    [[ "$output" == *"del DEVICE"* ]]
}

@test "I can get info about a fat12 image" {
    run ./src/f12 info tests/fixtures/test.img
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"Partition size"* ]]
    [[ "$output" == *"Used bytes"* ]]
    [[ "$output" == *"Files"* ]]
    [[ "$output" == *"Directories"* ]]
}

@test "I can dump the bios parameter block of a fat12 image" {
    run ./src/f12 info --dump-bpb tests/fixtures/test.img
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"Bios Parameter Block"* ]]
    [[ "$output" == *"OEMLabel"* ]]
    [[ "$output" == *"Sector size"* ]]
    [[ "$output" == *"Sectors per cluster"* ]]
    [[ "$output" == *"VolumeID"* ]]
    [[ "$output" == *"Volume label"* ]]
    [[ "$output" == *"File system"* ]]
}

@test "I can delete a file from a fat12 image" {
    run ./src/f12 del tests/fixtures/test.img FOLDER1/DATA.DAT
    [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img FOLDER1/DATA.DAT
    [[ "$output" == "File not found" ]]
}

@test "I can delete a directory from a fat12 image" {
    run ./src/f12 del --recursive tests/fixtures/test.img FOLDER1
    [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img
    [[ "$output" != *"FOLDER1"* ]]
}

@test "I can get a file from a fat12 image" {
    run ./src/f12 get tests/fixtures/test.img FOLDER1/SUBDIR/SECRET.TXT tests/tmp/secret.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/secret.txt
    [[ "$output" == "12345678" ]]
}

@test "I can get a directory from a fat12 image" {
    run ./src/f12 get tests/fixtures/test.img FOLDER1/SUBDIR tests/tmp/subdir
    [[ "$status" -eq 0 ]]
    [[ -d tests/tmp/subdir ]]
    [[ -f tests/tmp/subdir/SECRET.TXT ]]
    run cat tests/tmp/subdir/SECRET.TXT
    [[ "$output" == "12345678" ]]
}

@test "I can list the content of a fat12 image" {
    run ./src/f12 list tests/fixtures/test.img
    [[ "$status" -eq 0 ]]
    [[ "${lines[0]}" == "|-> FOLDER1" ]]
    [[ "${lines[1]}" == "|-> FOLDER2" ]]
    [[ "${lines[2]}" == "|-> FILE.BIN" ]]
}

@test "I can list the content of a fat12 image recursively" {
    run ./src/f12 list --recursive tests/fixtures/test.img
    [[ "$status" -eq 0 ]]
    [[ "$output" == *"FOLDER1"* ]]
    [[ "$output" == *"SUBDIR"* ]]
    [[ "$output" == *"SECRET.TXT"* ]]
    [[ "$output" == *"DATA.DAT"* ]]
    [[ "$output" == *"FOLDER2"* ]]
    [[ "$output" == *"FILE.BIN"* ]]
}

@test "I can list the content of a specific directory on a fat12 image" {
    run ./src/f12 list tests/fixtures/test.img FOLDER2
    [[ "$status" -eq 0 ]]
    [[ "${lines[0]}" == "|-> ." ]]
    [[ "${lines[1]}" == "|-> .." ]]
    [[ "${lines[2]}" == "|-> TEXT.TXT" ]]
}

@test "I can list the content of a specific directory on a fat12 image recursively" {
    run ./src/f12 list --recursive tests/fixtures/test.img FOLDER1
    [[ "$status" -eq 0 ]]
    [[ "${lines[0]}" == "|-> ." ]]
    [[ "${lines[1]}" == "|-> .." ]]
    [[ "${lines[2]}" == "|-> SUBDIR" ]]
    [[ "${lines[3]}" == "  |-> ." ]]
    [[ "${lines[4]}" == "  |-> .." ]]
    [[ "${lines[5]}" == "  |-> SECRET.TXT" ]]
    [[ "${lines[6]}" == "|-> DATA.DAT" ]]
}

@test "I can move a file on a fat12 image" {
    run ./src/f12 move tests/fixtures/test.img FILE.BIN FOLDER2
    [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img FILE.BIN
    [[ "$output" == "File not found" ]]
    run ./src/f12 get tests/fixtures/test.img FOLDER2/FILE.BIN tests/tmp/file.bin
    run cat tests/tmp/file.bin
    [[ "$output" == "Binary COntent" ]]
}

@test "I can move a directory on a fat12 image" {
    run ./src/f12 move --recursive tests/fixtures/test.img FOLDER1 FOLDER2
    [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img FOLDER1
    [[ "$output" == "File not found" ]]
    run ./src/f12 get tests/fixtures/test.img FOLDER2/FOLDER1/SUBDIR/SECRET.TXT tests/tmp/secret.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/secret.txt
    [[ "$output" == "12345678" ]]
}

@test "I can put a single file in the root directory of a fat12 image" {
    run ./src/f12 put tests/fixtures/test.img tests/fixtures/test.txt TEST.TXT
    [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img
    [[ "$output" == *"TEST.TXT"* ]]
    run ./src/f12 get tests/fixtures/test.img TEST.TXT tests/tmp/test.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/test.txt
    [[ "$output" == "This is test content" ]]
}

@test "I can put a single file in a non existing subdirectory of a fat12 image" {
    run ./src/f12 put tests/fixtures/test.img tests/fixtures/test.txt NEW_DIR/TEST.TXT
    [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img NEW_DIR
    [[ "$output" == *"TEST.TXT"* ]]
    run ./src/f12 get tests/fixtures/test.img NEW_DIR/TEST.TXT tests/tmp/test.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/test.txt
    [[ "$output" == "This is test content" ]]
}

@test "I can put a single file in an existent subdirectory of a fat12 image" {
    run ./src/f12 put tests/fixtures/test.img tests/fixtures/test.txt FOLDER1/TEST.TXT
    [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img FOLDER1
    [[ "$output" == *"TEST.TXT"* ]]
    run ./src/f12 get tests/fixtures/test.img FOLDER1/TEST.TXT tests/tmp/test.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/test.txt
    [[ "$output" == "This is test content" ]]
}

@test "I can overwrite an existing file on a fat12 image by putting another file in its place" {
    run ./src/f12 put tests/fixtures/test.img tests/fixtures/test.txt NEW/TEST.TXT
    [[ "$status" -eq 0 ]]
    run ./src/f12 put tests/fixtures/test.img tests/fixtures/data.txt NEW/TEST.TXT
        [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img NEW
    [[ "${#lines[@]}" == "3" ]]
    run ./src/f12 get tests/fixtures/test.img NEW/TEST.TXT tests/tmp/test.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/test.txt
    [[ "$output" == "This is data" ]]
}

@test "I can put a directory on a fat12 image" {
    run ./src/f12 put tests/fixtures/test.img tests/fixtures/TEST NEW/TESTDIR
    [[ "$status" -eq 0 ]]
    run ./src/f12 list tests/fixtures/test.img NEW/TESTDIR
    #[[ "${#lines[@]}" == "6" ]]
    run ./src/f12 list tests/fixtures/test.img NEW/TESTDIR/SUBDIR1
    [[ "${#lines[@]}" == "3" ]]
    [[ "${lines[2]}" == "|-> FILE1.TXT" ]]
    run ./src/f12 list tests/fixtures/test.img NEW/TESTDIR/SUBDIR2
    [[ "${#lines[@]}" == "3" ]]
    [[ "${lines[2]}" == "|-> FILE2.TXT" ]]
    run ./src/f12 get tests/fixtures/test.img NEW/TESTDIR/TEST.DAT tests/tmp/test.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/test.txt
    [[ "$output" == "1234" ]]
    run ./src/f12 get tests/fixtures/test.img NEW/TESTDIR/DATA.BIN tests/tmp/test.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/test.txt
    [[ "$output" == "5678" ]]
    run ./src/f12 get tests/fixtures/test.img NEW/TESTDIR/SUBDIR1/FILE1.TXT tests/tmp/test.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/test.txt
    [[ "$output" == "test" ]]
    run ./src/f12 get tests/fixtures/test.img NEW/TESTDIR/SUBDIR2/FILE2.TXT tests/tmp/test.txt
    [[ "$status" -eq 0 ]]
    run cat tests/tmp/test.txt
    [[ "$output" == "foo" ]]
}