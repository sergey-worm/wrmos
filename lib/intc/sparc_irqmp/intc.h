//##################################################################################################
//
//  Low-level driver for Aeroflex Gaisler Multi-processor Interrupt Ctrl (IRQMP).
//
//##################################################################################################

#ifndef INTC_H
#define INTC_H

#include <stdint.h>
#include <assert.h>

#ifdef DEBUG
#  define print(...)  if (dprint) dprint(__VA_ARGS__)
#else
#  define print(...) (void)dprint
#endif

enum { Proc_max = 0x10 }; // Max processors number

#define SPACE_SIZE(addr1, addr2) (((addr2) - (addr1)) / sizeof(uint32_t))

typedef struct
{
	uint32_t level;    // 0x00 interrupt level
	uint32_t pending;  // 0x04 interrupt pending (extended)
	uint32_t force;    // 0x08 interrupt force (NCPU = 0)
	uint32_t clear;    // 0x0C interrupt clear (extended)
	uint32_t status;   // 0x10 multiprocessor status
	uint32_t bcast;    // 0x14 broadcast
	uint32_t space1 [SPACE_SIZE(0x18, 0x40)];
	uint32_t proc_mask  [Proc_max]; // 0x40 proc N interrupt mask (extended)
	uint32_t proc_force [Proc_max]; // 0x80 proc N interrupt force
	uint32_t proc_eack  [Proc_max]; // 0xc0 proc N extended interrupt acknowledge
	uint32_t space2 [SPACE_SIZE(0x100, 0x300)];
	// TODO:  // 0x300 + 0x4 * n Interrupt map register N
} Intc_regs_t;

enum
{
	Proc_num =  2,
	Irq_min  =  1,
	Irq_max  = 15,  // regular irq max
	Irq_emax = 31,  // irq max if extended irq is supported

	Status_ncpu_shift  = 28,     // 28-31, number of CPUs minus 1
	Status_ncpu_mask   = 0xf,    // 
	Status_ba          = 27,     // 27,    broadcast available, for NSPU > 0
	Status_eirq_shift  = 16,     // 16-19, number of CPUs minus 1
	Status_eirq_mask   = 0xf,    //
	Status_status_mask = 0xffff, // 0-15,  power-down status of CPU[n], 1 power-down, 0 running n.

	CPU                = 0,      // FIXME: currently supported one CPU only
};

typedef void (*Intc_print_t)(const char* format, ...);

inline unsigned ncpu(volatile Intc_regs_t* regs)
{
	return 1 + ((regs->status >> Status_ncpu_shift) & Status_ncpu_mask);
}

inline void intc_init(uintptr_t base_addr, Intc_print_t dprint)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	regs->pending = 0;  // clear all

	print("Sparc IRQMP:    %d CPU%s.\n", ncpu(regs), ncpu(regs)>1?"s":"");
}

static inline unsigned irqmax(volatile Intc_regs_t* regs)
{
	unsigned eirq = (regs->status >> Status_eirq_shift) & Status_eirq_mask;
	return eirq ? Irq_emax : Irq_max;
}

inline int intc_unmask(uintptr_t base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	if (irq < Irq_min  ||  irq > irqmax(regs))
		return -1; // Intc_err_inval_irq;

	unsigned eirq = (regs->status >> Status_eirq_shift) & Status_eirq_mask;
	if (irq == eirq)
		return -2; // Intc_err_inval_irq;

	regs->proc_mask[CPU] |= 1 << irq;
	return 0;
}

inline int intc_mask(uintptr_t base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	if (irq < Irq_min  ||  irq > irqmax(regs))
		return -1; // Intc_err_inval_irq;

	unsigned eirq = (regs->status >> Status_eirq_shift) & Status_eirq_mask;
	if (irq == eirq)
		return -2; // Intc_err_inval_irq;

	regs->proc_mask[CPU] &= ~(1 << irq);

	return 0; // Intc_ok;
}

inline void intc_dump(uintptr_t base_addr, Intc_print_t dprint);

inline int intc_clear(uintptr_t base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	unsigned eirq = (regs->status >> Status_eirq_shift) & Status_eirq_mask;
	if (irq < Irq_min  ||  irq > irqmax(regs)  ||  irq == eirq)
		return -2; // incorrect IRQ number

	if (irq <= Irq_max)
	{
		// regular irq
		regs->clear = 1 << irq;
	}
	else
	{
		// extended irq - clear pending bit (eirq pend bit was automatically cleared)
		regs->pending &= ~(1 << irq);
	}
	return 0;
}

inline bool intc_is_pending(uintptr_t base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;
	return regs->pending & (1 << irq);
}

inline unsigned intc_real_irq(uintptr_t base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	if (irq < Irq_min  ||  irq > Irq_max)
		return -2; // incorrect IRQ number

	unsigned eirq = (regs->status >> Status_eirq_shift) & Status_eirq_mask;
	if (irq == eirq)
		irq = regs->proc_eack[CPU];  // extended irq

	assert(irq && "irq == 0");

	return irq;
}

inline void intc_dump(uintptr_t base_addr, Intc_print_t dprint)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	unsigned ncpu = 1 + ((regs->status >> Status_ncpu_shift) & Status_ncpu_mask);

	dprint("Sparc IRQMP:  ncpu=%d\n", ncpu);
	dprint("  level:          0x%x\n", regs->level);
	dprint("  pending:        0x%x\n", regs->pending);
	dprint("  force:          0x%x\n", regs->force);
	dprint("  clear:          0x%x\n", regs->clear);
	dprint("  status:         0x%x\n", regs->status);

	for (int i=0; i<ncpu; ++i)
		dprint("  proc_mask[%d]:   0x%x\n", i, regs->proc_mask[i]);
		
	for (int i=0; i<ncpu; ++i)
		dprint("  proc_force[%d]:  0x%x\n", i, regs->proc_force[i]);
	
	for (int i=0; i<ncpu; ++i)
		dprint("  proc_eack[%d]:   0x%x\n", i, regs->proc_eack[i]);
}

#undef print

#endif // INTC_H
