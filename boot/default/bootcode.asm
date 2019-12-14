;;; This file contains a simple bootcode, that just prints a message to inform
;;; the user, that this is not a bootable disk.

	;; The first 3 bytes are used to jump after the bios parameter block
	;; to our bootcode.
	jmp short __code
	nop

%include "dos_4_0_ebpb.inc"

__code:
	;; Disable all interupts as they usually push data on the stack and we
	;; change the stack location here
	cli
	mov 	ax, 0x7e0
	mov	ss, ax
	mov	sp, 4096
	;; Enable the interupts again
	sti

	;; First tweak the video mode to output colors.
	;; INT 0x10,0x0 with 0x3 in the AL register is 80x25 characters with
	;; 16 colors.
	xor	ah, ah
	mov	al, 0x3
	int	0x10

	mov 	ax, 0x7C0
	mov 	ds, ax
	mov 	si, message

	mov	bl, 0xf
	mov	bh, 0
	mov	di, 0b00000000_00011111
	call	print
end:
	jmp end

;;; This function moves the cursor to the begin of the next line and fills the
;;; rest of the line with the color attributes in the lower DI register.
linebreak:
	;; INT 0x10,0x3 reads the current position of the cursor.
	;; It takes the following arguments:
	;;  AH  : 0x3
	;;  BH  : the page to get the cursor position for
	;; The return values are:
	;;  DH  : row
	;;  DL  : column
	mov	ah, 0x3
	xor	bh, bh
	int	0x10

	xor	cx, cx
	mov 	cl, 80
	sub	cl, dl

	mov	ah, 0x9
	mov	al, 0x20
	mov	bx, di
	xor	bh, bh
	int	0x10

	;; INT 0x10,0x2 sets the position of the cursor
	;; It takes the following arguments:
	;;  AH  : 0x2
	;;  BH  : the number of the page to set the cursor position for
	;;        (in graphics mode this is always 0)
	;;  DH  : the new row of the cursor
	;;  DL  : the new column of the cursor
	mov	ah, 0x2
	xor	bh, bh
	xor	dl, dl
	inc	dh
	int 	0x10

	ret

;;; This function prints the string from [DS:SI] with the color attributes in
;;; the lower DI register to the screen.
print:
.loop:
	mov	al, [ds:si]
	test	al, al
	jz	.done
	cmp	al, 0xa
	jne	.no_linebreak
.linebreak:
	call	linebreak
	jmp	.loop_end
.no_linebreak:
	mov	ah, 0x9
	mov	bx, di
	xor	bh, bh
	mov	cx, 1
	int	0x10
	;; INT 0x10,0x3 reads the current position of the cursor.
	;; It takes the following arguments:
	;;  AH  : 0x3
	;;  BH  : the page to get the cursor position for
	;; The return values are:
	;;  DH  : row
	;;  DL  : column
	mov	ah, 0x3
	xor	bh, bh
	int	0x10
	;; INT 0x10,0x2 sets the position of the cursor
	;; It takes the following arguments:
	;;  AH  : 0x2
	;;  BH  : the number of the page to set the cursor position for
	;;        (in graphics mode this is always 0)
	;;  DH  : the new row of the cursor
	;;  DL  : the new column of the cursor
	mov	ah, 0x2
	xor	bh, bh
	inc	dl
	int 	0x10

.loop_end:
	inc	si
	jmp	.loop
.done:
	ret

message:
	db 0xa
	db '      _____   _____    ______ ', 0xa
	db '     /####/  /#####]  /######}', 0xa
	db '    /#/__   [#/ /#/  (#/ _/#/ ', 0xa
	db '   /####/    _ /#/_    _/#/__ ', 0xa
	db '  /#/      /######/   /######/', 0xa, 0xa
	db '~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~', 0xa, 0xa
	db ' This medium is not bootable. Please insert a bootable medium', 0xa
	db ' and reboot your computer.', 0xa, 0xa, 0xa, 0xa, 0xa,  0xa, 0xa, 0xa
	db 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0
	TIMES	510 - ($ - $$) db 0
	DW 0xaa55
