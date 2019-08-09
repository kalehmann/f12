## f12

[![Build Status](https://drone.kalehmann.de/api/badges/karsten/f12/status.svg)](https://drone.kalehmann.de/karsten/f12)

f12 is an experimental tool to access fat12 images without requiring superuser
privileges for mounting them.

It has the ability to
- create fat12 images
- delete files or directories on fat12 images
- get files or directories from them
- get information about them
- list their contents
- move around files and directories on fat12 images
- put files or directories on fat12 images

### Do not actually use this!

While this tool may work, it is still experimental. It may work or not. If you
really need something like this, you should use
[GNU mtools](https://www.gnu.org/software/mtools/).

### Building f12 from source

Run the following commands:

```
autoreconf --install
./configure
make
```

### Testing f12

This program features different types of automated testing.

#### Unit tests

f12 has a small number of unit tests. These can be run with

```
make check
```

#### Highlevel tests

There exists also a bunch of high level tests which cover most of the
applications functions. These are implemented using
[bats](https://github.com/bats-core/bats-core).

Run them with

```
bats tests
```

Additional options for these tests can be configured through environment variables:

| Variable       | Description                                             |
|----------------|---------------------------------------------------------|
| `VALGRIND`     | Set this variable to leak check each test with valgrind |
| `VALGRIND_TAP` | If set, a tap compliant output containing valgrinds output as comment is written to the file specified in this variable |
| `TMP_DIR`      | Directory for temporary files during the test. This variable must be set to different locations for parallel runs |

#### Code coverage

f12 can be configured with code coverage enabled.

Configure f12 with code coverage enabled

```
./configure --enable-coverage
```

and rebuild the project with a empty `CFLAGS` variable (or any custom option except
optimizations).

```
make clean
make CFLAGS=''
```