//##################################################################################################
//
//  16-bit helpers, common for x86 and x86_64.
//  This code is part of MBR.
//
//##################################################################################################

.code16

//--------------------------------------------------------------------------------------------------
bad_disk:
	mov  $msg_bad_disk, %ax
	call print
	call halt

//--------------------------------------------------------------------------------------------------
halt:
	hlt
1:
	jmp 1b

//--------------------------------------------------------------------------------------------------
.global boot_device_id
boot_device_id:
	.byte 0

//--------------------------------------------------------------------------------------------------
// print string from %ax
print:
	mov %ax, %si
	call serial_print
	mov %ax, %si
	call video_print
	ret

//--------------------------------------------------------------------------------------------------
serial_init:
	push %ax
	push %dx
	mov  $0x00, %dx      // DX:  port number (0-1)
	mov  $0x00, %ah      // AH:  BIOS serial service code "init"
	mov  $0xe3, %al      // boud=9600, parity=0, stopbit=1, 8n1
	int  $0x14           // call BIOS
	pop  %dx
	pop  %ax
	ret

//--------------------------------------------------------------------------------------------------
serial_wait_tx:
	push %ax
	push %dx
	mov  $0x00, %dx      // DX:  port number (0-1)
1:
	mov  $0x03, %ah      // AH:  BIOS serial service code "status"
	int  $0x14           // call BIOS
	test $0x40, %ah      // check flag "tx shift reg empty"
	je 1b                //
2:
	pop %dx
	pop %ax
	ret

//--------------------------------------------------------------------------------------------------
// print string from %si via serial port
serial_print:
	push %ax
	push %dx
	mov  $0x00, %dx      // DX:  port number (0-1)
1:
	mov  $0x01, %ah      // AH:  BIOS serial service code "send"
	lodsb                // Load next byte from %si buffer into %al
	test %al, %al        // %al == 0 ?
	jz   2f              //
	int  $0x14           // call BIOS
	call serial_wait_tx
	jmp  1b
2:
	pop %dx
	pop %ax
	ret

//--------------------------------------------------------------------------------------------------
// print string from %si via video
video_print:
	push %ax
	push %bx
	mov  $0x7, %bx       // BH: page 0, BL: attribute 7 (normal white)
	mov  $0x0e, %ah      // BIOS function "TELETYPE OUTPUT"
1:
	lodsb                // Load next byte from %si buffer into %al
	test %al, %al        // %al == 0 ?
	jz   2f              //
	int  $0x10           // call BIOS
	jmp  1b
2:
	pop %bx
	pop %ax
	ret

//--------------------------------------------------------------------------------------------------
// load boot1 code and data
load_boot1:
	// recalibrate disk controller
	mov $0, %ah                  // cmd:  reset
	mov ( boot_device_id ), %dl  // disk
	int $0x13
	jc  bad_disk

	// load image sector by sector
	mov  $0x07e0, %bx            // loader starts from 0x7e00 (es = 0x7e00 << 4)
	mov  %bx, %es                // buffer is %es:%bx
	xor  %bx, %bx                //
	mov  $0, %ch                 // start track = 0
	mov  $2, %cl                 // start sector = 2
	mov  $2, %ah                 // cmd:  read sectors
	mov  $16, %al                 // read sectors 16 -> 0x2000 bytes
	mov  ( boot_device_id ), %dl // disk
	mov  $0, %dh                 // start head
	int  $0x13                   // call BIOS
	jc   bad_disk
	ret

msg_hello:
	.asciz "[boot0]  hello.\n\r"

msg_load_boot1:
	.asciz "[boot0]  load boot1 ..."

msg_bad_disk:
	.asciz "\r\n[boot0]  disk error.\n\r"

msg_switch_to_pm:
	.asciz "[boot0]  switch to protected mode.\r\n"

msg_done:
	.asciz " done.\n\r"

// vi: set ft=asm:
