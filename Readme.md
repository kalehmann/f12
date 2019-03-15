## f12

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

### Build it from source

Run the following commands:

```
autoreconf --install
./configure
make
```
