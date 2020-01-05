$(top_builddir)/boot/default_bootcode.bin: default/bootcode.asm
	nasm -f bin -o default_bootcode.bin default/bootcode.asm

clean-local:
	rm -rf default_bootcode.bin
