//##################################################################################################
//
//  HWBP - HardWare Break Point - API to use hardware breakpoint on Leon3 processor.
//
//##################################################################################################

/*
enum Hwbp_flags_t
{
    Hwbp_off = 0,
    Hwbp_r   = 1 << 2,
    Hwbp_w   = 1 << 1,
    Hwbp_x   = 1 << 0,
    Hwbp_rw  = Hwbp_r | Hwbp_w,
    Hwbp_rwx = Hwbp_r | Hwbp_w | Hwbp_x
};
*/
#include "sparc/sys_hwbp.h"

// set hardware breakpoint
int hwbp_set(int bpid, uint32_t addr, uint32_t mask, int flags)
{
	uint32_t addr_reg = addr & ~3;  // use bits [31-2]
	uint32_t mask_reg = mask & ~3;  // use bits [31-2]

	enum
	{
		IF = 1,                     // Instruction Fetch ADDR bit
		DL = 2,                     // Data Load MASK bit
		DS = 1                      // Data Store MASK bit
	};

	if (flags & Hwbp_r)    mask_reg |= DL;  // set Data Load bit
	if (flags & Hwbp_w)    mask_reg |= DS;  // set Data Store bit
	if (flags & Hwbp_x)    addr_reg |= IF;  // set Instruction Fetch bit

	const uint32_t safe_mask = 0xfffffffc;

	switch (bpid)
	{
		case 0:
			asm volatile("wr %0, %%asr25" : : "r"(safe_mask));
			asm volatile("wr %0, %%asr24" : : "r"(addr_reg));
			asm volatile("wr %0, %%asr25" : : "r"(mask_reg));
			break;
		case 1:
			asm volatile("wr %0, %%asr27" : : "r"(safe_mask));
			asm volatile("wr %0, %%asr26" : : "r"(addr_reg));
			asm volatile("wr %0, %%asr27" : : "r"(mask_reg));
			break;
		case 2:
			asm volatile("wr %0, %%asr29" : : "r"(safe_mask));
			asm volatile("wr %0, %%asr28" : : "r"(addr_reg));
			asm volatile("wr %0, %%asr29" : : "r"(mask_reg));
			break;
		case 3:
			asm volatile("wr %0, %%asr31" : : "r"(safe_mask));
			asm volatile("wr %0, %%asr30" : : "r"(addr_reg));
			asm volatile("wr %0, %%asr31" : : "r"(mask_reg));
			break;
		default:
			return -1;
	}
	return 0;
}

// read hardware breakpoint
bool hwbp_isset(int bpid, uint32_t* addr, uint32_t* mask, int* flags)
{
	uint32_t addr_reg;
	uint32_t mask_reg;

	enum
	{
		IF = 1,        // Instruction Fetch ADDR bit
		DL = 2,        // Data Load MASK bit
		DS = 1         // Data Store MASK bit
	};

	switch (bpid)
	{
		case 0:
			asm volatile("rd %%asr24, %0" : "=r"(addr_reg));
			asm volatile("rd %%asr25, %0" : "=r"(mask_reg));
			break;
		case 1:
			asm volatile("rd %%asr26, %0" : "=r"(addr_reg));
			asm volatile("rd %%asr27, %0" : "=r"(mask_reg));
			break;
		case 2:
			asm volatile("rd %%asr28, %0" : "=r"(addr_reg));
			asm volatile("rd %%asr29, %0" : "=r"(mask_reg));
			break;
		case 3:
			asm volatile("rd %%asr30, %0" : "=r"(addr_reg));
			asm volatile("rd %%asr31, %0" : "=r"(mask_reg));
			break;
		default:
			return -1;
	}

	int flags_val = 0;

	if (mask_reg & DL)    flags_val |= Hwbp_r;  // set Read bit
	if (mask_reg & DS)    flags_val |= Hwbp_w;  // set Write bit
	if (addr_reg & IF)    flags_val |= Hwbp_x;  // set Execute bit

	if (addr)    *addr = addr_reg & ~3;
	if (mask)    *mask = mask_reg & ~3;
	if (flags)   *flags = flags_val;

	return flags_val;
}
