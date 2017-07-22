//##################################################################################################
//
//  App startup code.
//
//##################################################################################################

.global app_entry
app_entry:

	// set stack pointer
	sethi %hi(app_stack), %sp
	or %sp, %lo(app_stack), %sp

	// reserv stack frame
	sub %sp, 96, %sp

	// go to  C-code
	call main
	 nop

//.section .bss
.section .stack, "aw"  // FIXME
.align 8
.space 0x8000
app_stack:
