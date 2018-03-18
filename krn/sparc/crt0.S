//##################################################################################################
//
//  Kernel low-level asm code - entry point, trap table, etc.
//
//##################################################################################################

// trap time registers aliases

#define t_psr      l0    /*  Trap time PSR.                                       */
#define t_pc       l1    /*  PC to return from trap. Filled by the hardware.      */
#define t_npc      l2    /*  nPC to return from trap. Filled by the hardware.     */
#define t_wim      l3    /*  Current WIM. Initialized either by trap table entry. */
#define t_tmp1     l4
#define t_tmp2     l5
#define t_tmp3     l6
#define t_irq      l7    /*  Interrupt level                                      */
#define t_ttype    l7    /*  Trap Type                                            */

//-------------------------------------------------------------------------------------------------
// MACROS - some low-level operations.
//-------------------------------------------------------------------------------------------------

// set new PSR, but PSR.CWP = OLD_PSR.CWP
.macro get_psr_for_return  psrval scratch
	srl  %\psrval, 5, %\psrval
	sll  %\psrval, 5, %\psrval
	mov %psr, %\scratch
	and %\scratch, 0x1f, %\scratch
	or  %\scratch, %\psrval, %\psrval
.endm

.macro store_frame  base
	std %l0, [%\base +  0]
	std %l2, [%\base +  8]
	std %l4, [%\base + 16]
	std %l6, [%\base + 24]
	std %i0, [%\base + 32]
	std %i2, [%\base + 40]
	std %i4, [%\base + 48]
	std %i6, [%\base + 56]
.endm

.macro store_frame_and_update_wim  base
	std %l0, [%\base +  0]
	// update wim, use l0 and l1 for it
	rd  %psr, %l0
	mov  1, %l1
	sll  %l1, %l0, %l1
	wr  %l1, %wim
	// nop; nop; nop   // 3 delay slots, but don't need here
	// store remaining regs
	std %l2, [%\base +  8]
	std %l4, [%\base + 16]
	std %l6, [%\base + 24]
	std %i0, [%\base + 32]
	std %i2, [%\base + 40]
	std %i4, [%\base + 48]
	std %i6, [%\base + 56]
.endm

.macro restore_frame  base
	ldd   [%\base +  0], %l0
	ldd   [%\base +  8], %l2
	ldd   [%\base + 16], %l4
	ldd   [%\base + 24], %l6
	ldd   [%\base + 32], %i0
	ldd   [%\base + 40], %i2
	ldd   [%\base + 48], %i4
	ldd   [%\base + 56], %i6
.endm

.macro store_globals  base scratch
	rd  %y, %\scratch
	st  %\scratch, [%\base +  0]
	st  %g1,       [%\base +  4]
	std %g2,       [%\base +  8]
	std %g4,       [%\base + 16]
	std %g6,       [%\base + 24]
.endm

.macro restore_globals  base scratch
	ld  [%\base +  0],  %\scratch
	ld  [%\base +  4], %g1
	ldd [%\base +  8], %g2
	ldd [%\base + 16], %g4
	ldd [%\base + 24], %g6
	wr  %\scratch, %y
.endm

.macro store_syscall_frame  base
	std %i0, [%\base +  0]
	std %i2, [%\base +  8]
	std %i4, [%\base + 16]
	std %i6, [%\base + 24]
.endm

.macro restore_syscall_frame  base
	ldd [%\base +   0], %i0
	ldd [%\base +   8], %i2
	ldd [%\base +  16], %i4
	ldd [%\base +  24], %i6
.endm

// valid for NWIN = 8
.macro rotate_right  val scratch
	srl %\val, 1, %\scratch
	sll %\val, 7, %\val
	or %\val, %\scratch, %\val
	and %\val, 0xff, %\val
.endm

// valid for NWIN = 8
.macro rotate_left  val scratch
	sll %\val, 1, %\scratch
	srl %\val, 7, %\val
	or %\val, %\scratch, %\val
	and %\val, 0xff, %\val
.endm

.macro set_zero_if_cwp_valid  psrval wimval scratch
	mov 1, %\scratch
	sll %\scratch, %\psrval, %\scratch
	andcc %\scratch, %\wimval, %g0  // set PSR.ICC.zero
.endm

// valid for NWIN = 8
.macro get_wim_for_cwp_plus_one  psrval scratch wimres
	mov 2, %\scratch                      // scratch = 1 << +1
	sll %\scratch, %\psrval, %\scratch    // scratch = 1 << (cwp + 1)
	srl %\scratch, 8, %\wimres            // wimres = 1 >> (cwp - 7)
	or  %\scratch, %\wimres, %\wimres     // wimres = two bits
	and %\wimres, 0xff, %\wimres          // wimres = result wim
.endm

// valid for NWIN = 8
.macro get_wim_for_cwp  psrval scratch wimres
	and %\psrval, 0x1f, %\scratch         // scratch = psrval.cwp
	mov 1, %\wimres                       // wimres = 1
	sll %\wimres, %\scratch, %\wimres     // wimres =<< scratch
.endm

// valid for NWIN = 8
.macro decrement_cwp  cwp
	cmp %\cwp, %g0
	bne 1f
	 nop
	add %\cwp, 8, %\cwp
1:
	sub %\cwp, 1, %\cwp
.endm

// assert( wim[CWP+1] == 1 )
.macro assert_prev_win_is_invalid  psrval scratch1 scratch2
#ifdef DEBUG
	get_wim_for_cwp_plus_one  \psrval \scratch1 \scratch2  // scratch2 = expected wim
	rd  %wim, %\scratch1                                   // scratch1 = real_wim
	andcc %\scratch1, %\scratch2, %g0
	bne 1f
	 nop
	ta 0
	 nop
1:
#endif
.endm

#define Stack_frame_sz         96 /* use for external funcs, compiler may use bytes [65..96] */
#define Regwin_frame_sz        64 /* use for load/store register window */
#define Syscall_frame_sz       32
#define Global_frame_sz        32
#define Proc_status_frame_sz   16 /* content trap time regs:  t_psr, t_wim, t_pc, t_npc */

// summary entry frame size = 7*64 + 32 + 32 + 16 = 524 = 0x210

//--------------------------------------------------------------------------------------------------
// Used for inst/data-exception, syscall, irq.
.macro store_timer_value  val_addr scratch1 scratch2
	set timer_va, %\scratch1                      // scratch1 = &timer_va
	ld [%\scratch1], %\scratch1                   // scratch1 = timer_va
	ld [%\scratch1 + 0x10], %\scratch2            // scratch2 = timer_va.value
	sethi %hi(\val_addr), %\scratch1              // scratch1 = &val_addr
	st %\scratch2, [%\scratch1 + %lo(\val_addr)]  // *scratch1 = scratch2
.endm

#define USE_FIXED_REGWINS_AREA 1

//--------------------------------------------------------------------------------------------------
// Using for inst/data-exception, syscall, irq.
.macro exception_entry  scratch scratch_sp force_eentry_user=0

	// USE:  only LOCALS (t_xxx and scratch)

	// storage timer value for time accounting
	store_timer_value  cur_kentry_start \scratch \scratch_sp

	.if \force_eentry_user
	// did exceptions occur in user ?
	btst 0x40, %t_psr
	bne 5f
	 nop
	.endif

1:	// eentry_user

	// USE:  only LOCALS (t_xxx and scratch)

	// STEP 1:  get kernel stack pointer for current thread
	sethi %hi(cur_ksp), %\scratch
	ld [%\scratch + %lo(cur_ksp)], %\scratch_sp

	// STEP 2:  store t_psr (to allow modify it in kernel) and t_pc
	sub %\scratch_sp, Proc_status_frame_sz, %\scratch_sp
	st %t_psr, [%\scratch_sp +  0]
	st %t_wim, [%\scratch_sp +  4]
	st %t_pc,  [%\scratch_sp +  8]
	st %t_npc, [%\scratch_sp + 12]

	// STEP 3:  store Syscall_frame
	sub %\scratch_sp, Syscall_frame_sz, %\scratch_sp
	store_syscall_frame \scratch_sp

	// STEP 4:  store Globals
	sub %\scratch_sp, Global_frame_sz, %\scratch_sp
	store_globals  \scratch_sp, \scratch

	// USE:  LOCALS and GLOBALS

2:	// store_user_regwins

	// STEP 5:  store user regwins

	// DEFINE GLOBAL ALIASES (g1 and g2 use as scratch)
	#define g_nwim     g4
	#define g_wim      g5
	#define g_psr      g6
	#define g_sp       g7

	mov %t_psr, %g_psr                       // save CWP in globals
	mov %t_wim, %g_wim                       // save WIM in globals
	mov %\scratch_sp, %g_sp                  // save SP in globals

	#if USE_FIXED_REGWINS_AREA

	get_wim_for_cwp  t_psr g2 g1             // g1_cur = 1 << cwp

30:	// loop1_start - skip space for clean windows

	cmp %g1, %g_wim                          // while ( g1_cur != wim )  // while cur win is valid
	be 40f                                   // {
	 nop                                     //
                                             //
	sub %g_sp, Regwin_frame_sz, %g_sp        //     preserve space for user Regwin_frame
	rotate_right g1 g2                       //     g1_cur =>> 1
                                             //
	b 30b                                    //
	 nop                                     // }

40:	// loop1_end

	#endif // USE_FIXED_REGWINS_AREA

	get_wim_for_cwp_plus_one  t_psr g2 g1    // g1_prev = 1 << (cwp + 1)
	mov %g1, %g_nwim                         // save new wim to set after flushing user regwins

	// USE:  only GLOBALS

3:	// loop2_start - store dirty windows

	cmp %g1, %g_wim                          // while ( g1_prev != wim )  // while prev win is valid
	be 4f                                    // {
	 nop                                     //
                                             //
	restore                                  //     cwp++, go into prev window
	sub %g_sp, Regwin_frame_sz, %g_sp        //     preserve space for user Regwin_frame
	store_frame g_sp                         //     store regwin
	rotate_left g1 g2                        //     g1_prev =<< 1
                                             //
	b 3b                                     //
	 nop                                     // }

4:	// loop2_end

	mov %g_nwim, %wim                        // set new wim defined above
	mov %g_psr, %psr                         // return to trap window by restoring the psr
	 nop; nop; nop                           // important 3 nops to use %sp below

	mov %g_sp, %sp                           // now SP point to preserved space for WOF
	sub %sp, Stack_frame_sz, %sp             // preserve space for Stack_frame

	b 7f                                     // set SP and go to end of macro
	 nop

	// UNDEF GLOBAL ALIASES
	#undef g_nwim
	#undef g_wim
	#undef g_psr
	#undef g_sp

5:	// eentry_kernel

	// USE:  only LOCALS (t_xxx and scratch)

	// STEP 1:  flush next regwin if current is invalid (like WOF)
	set_zero_if_cwp_valid  t_psr t_wim \scratch
	bz 6f
	 nop

	rotate_right  t_wim \scratch             // new WIM value
	mov %t_wim, %wim                         // set new WIM
	// nop; nop; nop                         // 3 delay slots, but don't need here

	save                                     // go to dirty window
	store_frame  sp                          // store window to stack
	restore                                  // go back to trap window

6:	// skip_flushing_regwin

	// USE:  LOCALS and OUTPUTS

	// STEP 2:  alloc space for globals and stack_frame
	sub %fp, Global_frame_sz + Stack_frame_sz, %sp

	// STEP 3:  store globals
	store_globals  sp + Stack_frame_sz, \scratch

7:	// eentry_end
.endm

//--------------------------------------------------------------------------------------------------
// Used for inst/data-exception, syscall, irq.
.macro exception_exit  scratch scratch_sp force_eexit_user=0

	.if \force_eexit_user
	// will the rett land us in user mode ?
	btst 0x40, %t_psr
	bne 7f
	 nop
	.endif

1:	// eexit_user

	// USE:  GLOBALS and LOCALS (t_xxx and scratch)

	// STEP 1:  restore t_wim/t_pc/t_npc/t_psr.(ef|cwp), it may be changed by kernel

	// psr mask:  icc, ef, cwp --> bits [23..20,12,4..0] --> 0xf0101f
	#define Update_psr_mask 0xf0101f
	#define offset (Stack_frame_sz + 7*Regwin_frame_sz + Global_frame_sz + Syscall_frame_sz)

	ld [%sp + offset +  0], %t_tmp3     // new_psr
	set Update_psr_mask, %\scratch      // prepare update mask
	and %t_tmp3, %\scratch, %t_tmp3     // cut update bits from new_psr
	andn %t_psr, %\scratch, %t_psr      // clear update bits in old_psr
	or %t_psr, %t_tmp3, %t_psr          // old_psr |= new_psr

	ld [%sp + offset +  4], %t_wim
	ld [%sp + offset +  8], %t_pc
	ld [%sp + offset + 12], %t_npc

	// STEP 2:  restore user regwins

	// DEFINE GLOBAL ALIASES (g1 and g2 use as scratch)
	#define g_tcwp     g3
	#define g_tcwp2    g4
	#define g_twim     g5
	#define g_psr      g6
	#define g_sp       g7

	// Step 2.1:  save state in globals

	mov %psr, %g_psr                         // current CWP, for assertion below
	and %t_psr, 0x1f, %g_tcwp                // trap-entry CWP
	mov %g_tcwp, %g_tcwp2                    // trap-entry CWP backup
	mov %t_wim, %g_twim                      // trap-entry WIM
	add %sp, Stack_frame_sz, %g_sp           // g_sp to restore user regwins

	// some assertions
	assert_prev_win_is_invalid  g_psr g1 g2

	#if USE_FIXED_REGWINS_AREA
	set tmp_end_of_rgwins, %g1
	add %g_sp, 7*Regwin_frame_sz, %g2        // store pointer of regwins area
	st %g2, [%g1]                            // tmp_end_of_rgwins = sp + 7 * regwins_sz
	#endif

	// Step 2.2:  disable WIM traps
	mov %g0, %wim                            // don't wait 3 delay slots here

	// USE:  only GLOBALS

	// Step 2.3:  set CWP to initial position for restoring of windows, and set WIM

	get_wim_for_cwp_plus_one  g_tcwp g2 g1   // g1 = 1 << (tpsr.cwp + 1)

2:	// loop1_start

	cmp %g1, %g_twim                         // while ( twim != (1 << (tcwp + 1)) )
	be 3f                                    // {
	 nop                                     //
                                             //
	decrement_cwp  g_tcwp                    //     tcwp--
	rotate_right  g1 g2                      //     twim >> 1
	b 2b                                     //
	 save                                    //     cwp--
	                                         // }

3:	// loop1_end

	mov %psr, %g_psr                         // save CWP in globals to set WIM below

	// Step 2.4:  restore user windows

4:	// loop2_start

	cmp %g_tcwp, %g_tcwp2                    // while ( tcwp != tcwp2 )
	be 5f                                    // {
	 nop                                     //
                                             //
	restore_frame  g_sp                      //     restore frame
	save                                     //     cwp--
	decrement_cwp  g_tcwp                    //     tcwp--
	b 4b                                     //
	 add %g_sp, Regwin_frame_sz, %g_sp       //     set next regwin frame
                                             // }
5:	// loop2_end

	// now we got back into trap window, and may set new WIM

	// Step 2.5:  set new WIM
	get_wim_for_cwp_plus_one  g_psr g2 t_wim    // t_wim = 1 << (cwp + 1)  // note:  use t_wim !
	mov %t_wim, %wim

	#if USE_FIXED_REGWINS_AREA
	set tmp_end_of_rgwins, %g1
	ld [%g1], %\scratch_sp                   // set pointer to end of regwins area
	#else
	mov %g_sp, %\scratch_sp
	#endif

	// UNDEF GLOBAL ALIASES
	#undef g_tcwp
	#undef g_tcwp2
	#undef g_twim
	#undef g_psr
	#undef g_sp

	// USE:  GLOBALS and LOCALS

	// STEP 3:  restore globals
	restore_globals \scratch_sp \scratch
	add %\scratch_sp, Global_frame_sz, %\scratch_sp

	// USE:  only LOCALS

	// STEP 4:  restore Syscal_frame or skip it
	restore_syscall_frame \scratch_sp
	add %\scratch_sp, Syscall_frame_sz, %\scratch_sp

	// STEP 5:  proc status frame already resored, skip it
	add %\scratch_sp, Proc_status_frame_sz, %\scratch_sp

	// STEP 6:  set kernel SP for current thread and go to end of macro
	sethi %hi(cur_ksp), %\scratch
	b 9f
	 st %\scratch_sp,  [ %\scratch + %lo(cur_ksp) ]

7:	// eexit_kernel

	// USE:  GLOBALS and LOCALS (t_xxx and scratch)

	// STEP 1:  if rett will land us in the invalid window - restore prev window (like WUF)

	mov %psr, %g1
	mov %wim, %g2
	rotate_right  g2 \scratch                // the same as psr.cwp++
	set_zero_if_cwp_valid  g1 g2 \scratch
	bz 8f
	 nop

	mov %wim, %t_wim
	rotate_left  t_wim, \scratch             // new WIM value
	mov %g0, %wim                            // set new WIM
	 nop; nop; nop                           // 3 delay slots, all need here

	restore                                  // go to window to restore
	restore_frame  sp                        // restore window from kstack
	save                                     // go back to trap window

	mov %t_wim, %wim                         // set new WIM
	 nop; nop; nop                           // 3 delay slots, all need here

8:	// skip_restoring_regwin

	// STEP 2:  restore globals
	restore_globals  sp + Stack_frame_sz, \scratch

9:	// eexit_end

	// storage timer value for time accounting
	store_timer_value  cur_kexit_end \scratch \scratch_sp

.endm // exception_exit

.macro TRAP  handler
	mov %psr, %t_psr
	sethi %hi(\handler), %t_tmp1
	jmp %t_tmp1 + %lo(\handler)
	 mov %wim, %t_wim
.endm

.macro TRAP_INTERRUPT  int_level
	mov %psr, %t_psr
	mov \int_level, %t_irq
	b irq_trap
	 mov %wim, %t_wim
.endm

.macro BAD_TRAP  ttype
	#if 1
	mov %psr, %t_psr
	mov \ttype, %t_ttype
	b bad_trap
	 mov %wim, %t_wim
	#else
	ta 0; nop; nop; nop
	#endif
.endm

.macro BAD_TRAP_4  ttype_base
	BAD_TRAP  \ttype_base + 0
	BAD_TRAP  \ttype_base + 1
	BAD_TRAP  \ttype_base + 2
	BAD_TRAP  \ttype_base + 3
.endm

.macro BAD_TRAP_16  ttype_base
	BAD_TRAP_4  \ttype_base + 0
	BAD_TRAP_4  \ttype_base + 4
	BAD_TRAP_4  \ttype_base + 8
	BAD_TRAP_4  \ttype_base + 12
.endm


.section .text.init, "ax"

.align 0x1000  // trap handler table, must be aligned to page size
.global leon_traphandlers
leon_traphandlers:                           // description                prio    tt
	BAD_TRAP  0                              // reset                         1  0x00
	TRAP inst_access_exception               // instruction_access_exception  5  0x01
	BAD_TRAP  2                              // illegal_instruction           7  0x02
	BAD_TRAP  3                              // privileged_instruction        6  0x03
	BAD_TRAP  4                              // fp_disabled                   8  0x04
	TRAP window_overflow                     // window_overflow               9  0x05
	TRAP window_underflow                    // window_underflow              9  0x06
	BAD_TRAP  7                              // mem_address_not_aligned      10  0x07
	BAD_TRAP  8                              // fp_exception                 11  0x08
	TRAP data_access_exception               // data_access_exception        13  0x09
	BAD_TRAP  10                             // tag_overflow                 14  0x0a
	BAD_TRAP  11                             // watchpoint_detected           8  0x0b
	BAD_TRAP  12                             // ?                                0x0c
	BAD_TRAP  13                             // ?                                0x0d
	BAD_TRAP  14                             // ?                                0x0e
	BAD_TRAP  15                             // ?                                0x0f
	BAD_TRAP  16                             // ?                                0x10

	TRAP_INTERRUPT  1                        // interrupt_level_1            31  0x11
	TRAP_INTERRUPT  2                        // interrupt_level_2            30  0x12
	TRAP_INTERRUPT  3                        // interrupt_level_3            29  0x13
	TRAP_INTERRUPT  4                        // interrupt_level_4            28  0x14
	TRAP_INTERRUPT  5                        // interrupt_level_5            27  0x15
	TRAP_INTERRUPT  6                        // interrupt_level_6            26  0x16
	TRAP_INTERRUPT  7                        // interrupt_level_7            25  0x17
	TRAP_INTERRUPT  8                        // interrupt_level_8            24  0x18
	TRAP_INTERRUPT  9                        // interrupt_level_9            23  0x19
	TRAP_INTERRUPT 10                        // interrupt_level_10           22  0x1a
	TRAP_INTERRUPT 11                        // interrupt_level_11           21  0x1b
	TRAP_INTERRUPT 12                        // interrupt_level_12           20  0x1c
	TRAP_INTERRUPT 13                        // interrupt_level_13           19  0x1d
	TRAP_INTERRUPT 14                        // interrupt_level_14           18  0x1e
	TRAP_INTERRUPT 15                        // interrupt_level_15           17  0x1f

	BAD_TRAP_16    0x20                                                      //  0x20
	BAD_TRAP_16    0x30                                                      //  0x30
	BAD_TRAP_16    0x40                                                      //  0x40
	BAD_TRAP_16    0x50                                                      //  0x50
	BAD_TRAP_16    0x60                                                      //  0x60
	BAD_TRAP_16    0x70                                                      //  0x70
	BAD_TRAP_16    0x80                                                      //  0x80
	BAD_TRAP       0x90                                                      //  0x90
	TRAP syscall_trap                                                        //  0x91
	BAD_TRAP       0x92                                                      //  0x92
	BAD_TRAP       0x93                                                      //  0x93
	BAD_TRAP_4     0x94                                                      //  0x94
	BAD_TRAP_4     0x98                                                      //  0x98
	BAD_TRAP_4     0x9c                                                      //  0x9c
	BAD_TRAP_16    0xa0                                                      //  0xa0
	BAD_TRAP_16    0xb0                                                      //  0xb0
	BAD_TRAP_16    0xc0                                                      //  0xc0
	BAD_TRAP_16    0xd0                                                      //  0xd0
	BAD_TRAP_16    0xe0                                                      //  0xe0
	BAD_TRAP_16    0xf0                                                      //  0xf0


//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim, t_ttype
bad_trap:

	exception_entry  t_tmp1 t_tmp2

	// prepare arguments
	mov %t_ttype, %o0
	mov %t_psr,   %o1
	mov %t_pc,    %o2
	mov %t_npc,   %o3

	// re-enable traps and disable irqs
	or %t_psr, 0xf20, %t_tmp1
	wr %t_tmp1, %psr

	// do high-level stuff
	call sparc_entry_bad_trap
	 nop

	exception_exit  t_tmp1 t_tmp2

	// return
	get_psr_for_return  t_psr t_tmp1
	mov %t_psr, %psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0
	 rett %t_npc

//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim
window_overflow:

#if 0 // very OLD

	save                                     // go to dirty window
	store_frame_and_update_wim  sp           // store window to stack and wim--
	restore                                  // go back to trap window

	jmp %t_pc                                // go back to trapped instruction SAVE
	 rett %t_npc

#endif

#if 0 // OLD

	// store original %g1 and %g2 to use it
	mov  %g1, %t_tmp1
	mov  %g2, %t_tmp2

	// calc new wim and store it in %g1
	rd   %wim, %g1
	rotate_right  g1 t_tmp3

	// goto window to store
	save

	// re-enable traps and disable irq to allow page faults
	rd  %psr, %g2
	or  %g2, 0xf20, %g2
	wr  %g2, %psr

	// store window
	store_frame  sp

	// disable traps
	and %g2, ~0x20, %g2
	wr  %g2, %psr

	// set new wim
	wr  %g1, %wim
	 nop; nop; nop

	// go back to tap window and resore %g1 and %g2
	restore
	mov  %t_tmp1, %g1
	mov  %t_tmp2, %g2

	mov %t_psr, %psr
	 nop; nop; //nop                         // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0                          // go back to trapped instruction SAVE
	 rett %t_npc

#else // NEW

	btst 0x40, %t_psr                        // did trap in user mode ?
	be 2f                                    // go to usr
	 nop

1:  // krn wof

	// store original %g1 and %g2 to use it
	mov  %g1, %t_tmp1
	mov  %g2, %t_tmp2

	// calc new wim and store it in %g1
	rd   %wim, %g1
	rotate_right  g1 t_tmp3

	// go to window to store
	save

	// re-enable traps and disable irq to allow page faults
	rd  %psr, %g2
	or  %g2, 0xf20, %g2
	wr  %g2, %psr

	// store window
	store_frame  sp

	// disable traps
	and %g2, ~0x20, %g2
	wr  %g2, %psr

	// set new wim
	wr  %g1, %wim
	 nop; nop; nop

	// go back to tap window and resore %g1 and %g2
	restore
	mov  %t_tmp1, %g1
	mov  %t_tmp2, %g2

	mov %t_psr, %psr
	 nop; nop; //nop                         // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0                          // go back to trapped instruction SAVE
	 rett %t_npc

2:  // usr wof

	mov %g0, %wim                            // disable window traps
	 nop; nop; nop                           // 3 delay slots, but here enough 2 nop's

	set 0x300, %t_tmp1                       // address of MMU.fsr
	lda [ %t_tmp1 ] 0x19, %g0                // clear MMU.fsr

	lda [ %g0 ] 0x19, %t_tmp1                // read MMU.ctrl
	or %t_tmp1, 0x2, %t_tmp1                 // turn on no-fault bit
	sta %t_tmp1, [ %g0 ] 0x19                // set new MMU.ctrl

	save                                     // go to window to save
	store_frame  sp                          // store window to stack
	restore                                  // go back to trap window

	lda [ %g0 ] 0x19, %t_tmp1                // read MMU.ctrl
	andn %t_tmp1, 0x2, %t_tmp1               // turn off no-fault bit
	sta %t_tmp1, [ %g0 ] 0x19                // set new MMU.ctrl

	set 0x300, %t_tmp1                       // address of MMU.fsr
	lda [ %t_tmp1 ] 0x19, %t_tmp1            // read MMU.fsr

	andcc %t_tmp1, 0x2, %g0                  // did fault occur ?
	bne 3f                                   // yes -> go to pfault processing
	 nop

	// no pfault, exit

	rotate_right  t_wim, t_tmp1              // new WIM value
	mov %t_wim, %wim                         // set new WIM
	 nop; nop; nop                           // 3 delay slots

	mov %t_psr, %psr                         // restore psr
	 nop; nop; //nop                         // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0                          // go back to trapped instruction RESTORE
	 rett %t_npc

3:	// pfault in usr wof

	exception_entry  t_tmp1 t_tmp2 1         // do exception entry as for user entry

	// re-enable traps and disable irqs
	or %t_psr, 0xf20, %t_tmp1
	wr %t_tmp1, %psr

// TEST ME
set 0x400, %o0                           // address of MMU.fsr
lda [ %o0 ] 0x19, %o0                    // load MMU.fsr into 1st argument
set 0x300, %o1                           // address of MMU.far
lda [ %o1 ] 0x19, %o1                    // load MMU.far into 2nd argument
mov %t_pc, %o2                           // load faulting instruction into 3rd argument
call sparc_test_me;  nop

	set 0x400, %o0                           // address of MMU.fsr
	lda [ %o0 ] 0x19, %o0                    // load MMU.fsr into 1st argument

	set 0x300, %o1                           // address of MMU.far
	lda [ %o1 ] 0x19, %o1                    // load MMU.far into 2nd argument

	mov %t_pc, %o2                           // load faulting instruction into 3rd argument
	set 1, %o3                               // pfault place WOF

	// do high-level stuff
	call sparc_entry_pagefault
	 nop

	exception_exit  t_tmp1 t_tmp2 1          // do exception exit as for user entry

	// re-store window
	save                                     // go to window to save
	store_frame  sp                          // store window to stack
	restore                                  // go back to trap window

	rotate_right  t_wim, t_tmp1              // new WIM value
	mov %t_wim, %wim                         // set new WIM
	// nop; nop; nop                         // 3 delay slots, but here enough 0 nop's

	// return
	get_psr_for_return  t_psr t_tmp1
	mov %t_psr, %psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0
	 rett %t_npc

#endif

//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim
window_underflow:

#if 0 // very OLD

	mov %g0, %wim                            // disable window traps
	 nop; nop; nop                           // 3 delay slots, but here enough 2 nop's

	restore                                  // go to window to restore
	restore                                  //
	restore_frame  sp                        // restore window from stack
	save                                     //
	save                                     // go back to trap window

	rotate_left  t_wim, t_tmp1               // new WIM value
	mov %t_wim, %wim                         // set new WIM
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmp %t_pc                                // go back to trapped instruction RESTORE
	 rett %t_npc

#endif

#if 0 // OLD

	mov %g0, %wim                            // disable window traps
	// nop; nop; nop                         // 3 delay slots, but here not need

	// re-enable traps and disable irq to allow page faults
	rd  %psr, %t_tmp1
	or  %t_tmp1, 0xf20, %t_tmp1
	wr  %t_tmp1, %psr
	 nop; // nop; nop;                       // 3 delay slots, but here enough 1 nop

	restore                                  // go to window to restore
	restore                                  //
	restore_frame  sp                        // restore window from stack
	save                                     //
	save                                     // go back to trap window

	rotate_left  t_wim, t_tmp1               // new WIM value
	mov %t_wim, %wim                         // set new WIM
	// nop; nop; nop                         // 3 delay slots, but here not need

	mov %t_psr, %psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0                          // go back to trapped instruction RESTORE
	 rett %t_npc

#else // NEW

	btst 0x40, %t_psr                        // did trap in user mode ?
	be 2f                                    // go to usr
	 nop

1:  // krn wuf

	mov %g0, %wim                            // disable window traps
	// nop; nop; nop                         // 3 delay slots, but here enough 0 nop's

	// re-enable traps and disable irq to allow page faults
	rd  %psr, %t_tmp1
	or  %t_tmp1, 0xf20, %t_tmp1
	wr  %t_tmp1, %psr
	 nop; // nop; nop                        // 3 delay slots, but here enough 1 nop

	restore                                  // go to window to restore
	restore                                  //
	restore_frame  sp                        // restore window from stack
	save                                     //
	save                                     // go back to trap window

	rotate_left  t_wim, t_tmp1               // new WIM value
	mov %t_wim, %wim                         // set new WIM
	// nop; nop; nop                         // 3 delay slots, but here not need

	mov %t_psr, %psr                         // restore psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0                          // go back to trapped instruction RESTORE
	 rett %t_npc

2:  // usr wuf

	mov %g0, %wim                            // disable window traps
	// nop; nop; nop                         // 3 delay slots, but here not need

	set 0x300, %t_tmp1                       // address of MMU.fsr
	lda [ %t_tmp1 ] 0x19, %g0                // clear MMU.fsr

	lda [ %g0 ] 0x19, %t_tmp1                // read MMU.ctrl
	or %t_tmp1, 0x2, %t_tmp1                 // turn on no-fault bit
	sta %t_tmp1, [ %g0 ] 0x19                // set new MMU.ctrl

	restore                                  // go to window to restore
	restore                                  //
	restore_frame  sp                        // restore window from stack
	save                                     //
	save                                     // go back to trap window

	lda [ %g0 ] 0x19, %t_tmp1                // read MMU.ctrl
	andn %t_tmp1, 0x2, %t_tmp1               // turn off no-fault bit
	sta %t_tmp1, [ %g0 ] 0x19                // set new MMU.ctrl

	set 0x300, %t_tmp1                       // address of MMU.fsr
	lda [ %t_tmp1 ] 0x19, %t_tmp1            // read MMU.fsr

	andcc %t_tmp1, 0x2, %g0                  // did fault occur ?
	bne 3f                                   // yes -> go to pfault processing
	 nop

	// no pfault, exit

	rotate_left  t_wim, t_tmp1               // new WIM value
	mov %t_wim, %wim                         // set new WIM
	// nop; nop; nop                         // 3 delay slots, but here not need

	mov %t_psr, %psr                         // restore psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0                          // go back to trapped instruction RESTORE
	 rett %t_npc

3:	// pfault in usr wuf

	exception_entry  t_tmp1 t_tmp2 1         // do exception entry as for user entry

	set 0x400, %o0                           // address of MMU.fsr
	lda [ %o0 ] 0x19, %o0                    // load MMU.fsr into 1st argument

	set 0x300, %o1                           // address of MMU.far
	lda [ %o1 ] 0x19, %o1                    // load MMU.far into 2nd argument

	mov %t_pc, %o2                           // load faulting instruction into 3rd argument
	set 2, %o3                               // pfault place WUF

	// re-enable traps and disable irqs
	or %t_psr, 0xf20, %t_tmp1
	wr %t_tmp1, %psr

	// do high-level stuff
	call sparc_entry_pagefault
	 nop

	exception_exit  t_tmp1 t_tmp2 1          // do exception exit as for user entry

	// set new wim to restore window         // note:  t_wim may be changed in exception_exit
	rotate_left  t_wim, t_tmp1               // new WIM value
	mov %t_wim, %wim                         // set new WIM
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	// re-load window
	restore                                  // go to window to restore
	restore                                  //
	restore_frame  sp                        // restore window from stack
	save                                     //
	save                                     // go back to trap window

	// return
	get_psr_for_return  t_psr t_tmp1
	mov %t_psr, %psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0
	 rett %t_npc

#endif

//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim, t_irq
irq_trap:

	exception_entry  t_tmp1 t_tmp2

	// re-enable traps and disable irqs
	or %t_psr, 0xf20, %t_tmp1
	wr %t_tmp1, %psr

	// do high-level stuff
	call sparc_entry_irq
	 mov %t_irq, %o0

	exception_exit  t_tmp1 t_tmp2

	// return
	get_psr_for_return  t_psr t_tmp1
	mov %t_psr, %psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0
	 rett %t_npc

//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim
inst_access_exception:

	exception_entry  t_tmp1 t_tmp2

	mov %t_pc, %o0                           // fault address == instruction address

	ba mmu_exception_continue
	 nop

//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim
data_access_exception:
/**/
	// did exceptions occur in user ?
	btst 0x40, %t_psr
	be 1f
	 nop
	b data_pfault_in_kernel_mode             // go to bug processing
1:
/*~*/
	exception_entry  t_tmp1 t_tmp2

	set 0x400, %t_tmp1                       // address of MMU fault address register
	lda [ %t_tmp1 ] 0x19, %o0                // load fault address into 1st argument


mmu_exception_continue:

	set 0x300, %t_tmp1                       // address of MMU fault status register
	lda [ %t_tmp1 ] 0x19, %o1                // load status register into 2nd argument

	mov %t_pc, %o2                           // load faulting instruction into 3rd argument
	set 3, %o3                               // pfault place PF

	// re-enable traps and disable irqs
	or %t_psr, 0xf20, %t_tmp1
	wr %t_tmp1, %psr

	// do high-level stuff
	call sparc_entry_pagefault
	 nop

	exception_exit  t_tmp1 t_tmp2

	// return
	get_psr_for_return  t_psr t_tmp1
	mov %t_psr, %psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0
	 rett %t_npc


// pfault in kernel mode is сonsequence of bugs,
// one of bugs may kstack overflow,
// to allow execute C-code and dump this error we
// set new kpfault stack and invalidate of all regwindows
data_pfault_in_kernel_mode:

	// invalidate all windows
	get_wim_for_cwp_plus_one t_psr t_tmp1 t_tmp2 // t_tmp2 = 1 << (cwp + 1)
	mov %t_tmp2, %wim                        // wim = %wim, 3 delay slots

	// set stack pointer and reserve stack frame
	set kpfault_stack, %sp
	sub %sp, Stack_frame_sz, %sp

	set 0x400, %t_tmp1                       // address of MMU fault address register
	lda [ %t_tmp1 ] 0x19, %o0                // load fault address into 1st argument

	set 0x300, %t_tmp1                       // address of MMU fault status register
	lda [ %t_tmp1 ] 0x19, %o1                // load status register into 2nd argument

	mov %t_pc, %o2                           // load faulting instruction into 3rd argument
	set 4, %o3                               // pfault place KPF

	// re-enable traps and disable irqs
	or %t_psr, 0xf20, %t_tmp1
	wr %t_tmp1, %psr

	// do high-level stuff to call panic()
	call sparc_entry_pagefault
	 nop


//--------------------------------------------------------------------------------------------------
// valid regs:  t_psr, t_pc, t_npc, t_wim
// incomings: %i0 - syscall_id
syscall_trap:

	exception_entry  t_tmp1 t_tmp2

	// re-enable traps and disable irqs
	or %t_psr, 0xf20, %t_tmp1
	wr %t_tmp1, %psr

	// do high-level stuff
	call sparc_entry_syscall
	 mov %t_pc, %o0

	exception_exit  t_tmp1 t_tmp2

	// return
	get_psr_for_return  t_psr t_tmp1
	mov %t_psr, %psr
	 nop; nop; // nop                        // 3 delay slots, but here enough 2 nop's

	jmpl %t_pc, %g0
	 rett %t_npc


.section .data
.global cur_ksp
.global timer_va
.global cur_kentry_start
.global cur_kexit_end
cur_ksp:           .long 0  // kernel stack pointer for current thread
timer_va:          .long 0  // timer virtual address, it needs for time accounting
cur_kentry_start:  .long -1 // kernel entry timer value for current thread
cur_kexit_end:     .long -1 // kernel exit  timer value for current thread

tmp_end_of_rgwins: .long 0


.align 8
.space 0x1000
kpfault_stack:

//--------------------------------------------------------------------------------------------------
// low-level entry point
.section .bootstrap  // work at phys addresses
.global _start
_start:

	// setup trap handler table
	sethi %hi(leon_traphandlers), %g2
	wr %g2, %tbr

	// set initial PSR and WIM
	mov 0xfe0, %psr        // irq_off, supervisor, trap_on, CWP=0
	// nop; nop; nop       // 3 delay slots, but here not need
	mov 2, %wim            // set invalid windows #1
	 nop; nop // nop       // 3 delay slots, but here enough 2 nop's

	// set stack pointer and reserve stack frame
	set bootstrap_stack, %sp
	sub %sp, Stack_frame_sz, %sp

	call bootstrap
	 nop

.align 8
.space 0x400
bootstrap_stack:

