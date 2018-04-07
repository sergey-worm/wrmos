//##################################################################################################
//
//  Low-level driver for ARM Generic Interrupt Controller v1 (GIC).
//
//##################################################################################################

#ifndef INTC_H
#define INTC_H

#include <stdint.h>

#ifdef DEBUG
#  define print(...)  if (dprint) dprint(__VA_ARGS__)
#else
#  define print(...) (void)dprint
#endif

#define SPACE_SIZE(addr1, addr2) (((addr2) - (addr1)) / sizeof(uint32_t))

typedef struct
{
	// CPU interface
	uint32_t control;    // ICCICR   rw  0x00000000  CPU Interface Control Register
	uint32_t prio_mask;  // ICCPMR   rw  0x00000000  Interrupt Priority Mask Register
	uint32_t bin_point;  // ICCBPR   rw  0x0-0x3     Binary Point Register
	uint32_t ack;        // ICCIAR   ro  0x000003FF  Interrupt Acknowledge Register
	uint32_t eoi;        // ICCEOIR  wo              End of Interrupt Register
	uint32_t run_prio;   // ICCRPR   ro  0x000000FF  Running Priority Register
	uint32_t hi_pend;    // ICCHPIR  ro  0x000003FF  Highest Pending Interrupt Register
	uint32_t abin_point; // ICCABPR  rw  0x00000000  Aliased Binary Point Register
	uint32_t _reserved1[SPACE_SIZE(0x20,0x40)];   // reserverd
	uint32_t impl[SPACE_SIZE(0x40,0xd0)];         // implementation defined registers
	uint32_t _reserved2[SPACE_SIZE(0xd0,0xfc)];   // reserved
	uint32_t ident;      // ICCIIDR  ro  -           CPU Interface Identification Register
} Intc_gicc_regs_t;

typedef struct
{
	// distributor interface
	uint32_t control;                             // ICDDCR     rw  0x00000000  Distributor Control Register
	uint32_t type;                                // ICDICTR    ro           -  Interrupt Controller Type Register
	uint32_t imp_id;                              // ICDIIDR    ro           -  Distributor Implementer Identification Register
	uint32_t _reserved1[SPACE_SIZE(0x0c,0x80)];   // reserved
	uint32_t security;                            // ICDISR     rw           -  Interrupt Security Registers
	uint32_t _reserved2[SPACE_SIZE(0x84,0x100)];  // reserved       0x00000000
	uint32_t set_en[SPACE_SIZE(0x100,0x180)];     // ICDISER    rw           -  Interrupt Set-Enable Registers
	uint32_t clr_en[SPACE_SIZE(0x180,0x200)];     // ICDICER    rw           -  Interrupt Clear-Enable Registers
	uint32_t set_pend[SPACE_SIZE(0x200,0x280)];   // ICDISPR    rw  0x00000000  Interrupt Set-Pending Registers
	uint32_t clr_pend[SPACE_SIZE(0x280,0x300)];   // ICDICPR    rw  0x00000000  Interrupt Clear-Pending Registers
	uint32_t act_bit[SPACE_SIZE(0x300,0x380)];    // ICDABR     ro  0x00000000  Active Bit Registers
	uint32_t _reserved3[SPACE_SIZE(0x380,0x400)]; // reserved
	uint32_t prio[SPACE_SIZE(0x400,0x7fc)];       // ICDIPR     rw  0x00000000  Interrupt Priority Registers
	uint32_t _reserved4;                          // reserved
	uint32_t proc_target[SPACE_SIZE(0x800,0x820)];// ICDIPTR    ro           -  Interrupt Processor Targets Registers
	uint32_t _reserved5[SPACE_SIZE(0x820,0xbfc)]; // reserved   rw  0x00000000
	uint32_t _reserved6;                          // reserved
	uint32_t config[SPACE_SIZE(0xc00,0xd00)];     // ICDICFR    rw           -  Interrupt Configuration Registers
	uint32_t _reserved7[SPACE_SIZE(0xd00,0xe00)]; //                            implementation defined registers
	uint32_t _reserved8[SPACE_SIZE(0xe00,0xf00)]; // reserved
	uint32_t sgi;                                 // ICDSGIR    wo           -  Software Generated Interrupt Register
	uint32_t _reserved9[SPACE_SIZE(0xf04,0xfd0)]; // reserved
	uint32_t ident[SPACE_SIZE(0xfd0,0x1000)];     //            ro           -  Identification registers
} Intc_gicd_regs_t;

typedef struct
{
	uint32_t _space1[SPACE_SIZE(0,0x100)];        // 0x100
	Intc_gicc_regs_t gicc;                        // 0x100
	uint32_t _space2[SPACE_SIZE(0x200,0x1000)];   // 0xd00
	Intc_gicd_regs_t gicd;                        // 0x1000
} Intc_regs_t;

enum
{
	Gicd_type_ncpu_offs = 5,
	Gicd_type_ncpu_mask = 0x7,
	Gicd_type_lines_offs = 0,
	Gicd_type_lines_mask = 0x1f,
};

typedef void (*Intc_print_t)(const char* format, ...);

inline unsigned getval(unsigned val, unsigned offset, unsigned mask)
{
	return (val >> offset) & mask;
}

inline unsigned ncpu(volatile Intc_regs_t* regs)
{
	return getval(regs->gicd.type, Gicd_type_ncpu_offs, Gicd_type_ncpu_mask) + 1;
}

static inline unsigned irqmax(volatile Intc_regs_t* regs)
{
	return 32 * (getval(regs->gicd.type, Gicd_type_lines_offs, Gicd_type_lines_mask) + 1);
}

// get current pending irq
inline unsigned intc_irq(unsigned base_addr)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;
	return regs->gicc.ack;
}

inline void intc_init(unsigned base_addr, Intc_print_t dprint)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;
	print("ARM GIC v1:    ncpu=%u, nirq=%u.\n", ncpu(regs), irqmax(regs));

	// GICD init
	regs->gicd.control = 1;          // enable dist iface

	// GICC init
	regs->gicc.prio_mask = 0xff;     // set lowest prio, allow all irq prios
	regs->gicc.bin_point = 0x7;      // ?
	regs->gicc.control = 1;          // enable cpu iface
}

inline int intc_unmask(unsigned base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	if (irq >= irqmax(regs))
		return 1;

	((uint8_t*)regs->gicd.proc_target)[irq] = 0xff;  // target all processors
	regs->gicd.set_en[irq/32] |= 1 << (irq%32);      // enable IRQ
	return 0;
}

inline int intc_mask(unsigned base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	if (irq >= irqmax(regs))
		return 1;

	regs->gicd.set_en[irq/32] &= ~(1 << (irq%32));   // disable IRQ
	return 0;
}

inline int intc_clear(unsigned base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	if (irq >= irqmax(regs))
		return 1;

	// nothing for PL011
	return 0;
}

inline int intc_eoi(unsigned base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	if (irq >= irqmax(regs))
		return 1;

	regs->gicc.eoi = irq;
	return 0;
}

inline bool intc_is_pending(unsigned base_addr, unsigned irq)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	if (irq >= irqmax(regs))
		return 1;

	return regs->gicd.set_pend[irq/32] & (1 << (irq%32));
}

inline void intc_dump(unsigned base_addr, Intc_print_t dprint)
{
	volatile Intc_regs_t* regs = (Intc_regs_t*) base_addr;

	volatile uint32_t* set_en   = regs->gicd.set_en;
	volatile uint32_t* clr_en   = regs->gicd.clr_en;
	volatile uint32_t* set_pend = regs->gicd.set_pend;
	volatile uint32_t* clr_pend = regs->gicd.clr_pend;

	print("ARM GIC v1:\n");
	print("  GICC:\n");
	print("    control:    0x%x\n", regs->gicc.control);
	print("    prio_mask:  0x%x\n", regs->gicc.prio_mask);
	print("    bin_point:  0x%x\n", regs->gicc.bin_point);
	print("    ack:        %u\n",   regs->gicc.ack);
	print("    eoi:        0x%x\n", regs->gicc.eoi);
	print("    run_prio:   0x%x\n", regs->gicc.run_prio);
	print("    hi_pend:    %u\n",   regs->gicc.hi_pend);
	print("    abin_point: 0x%x\n", regs->gicc.abin_point);
	print("    ident:      0x%x\n", regs->gicc.ident);
	print("  GICD:\n");
	print("    control:    0x%x\n", regs->gicd.control);
	print("    type:       0x%x:  cpus=%u, irqs=%u\n", regs->gicd.type, ncpu(regs), irqmax(regs));
	print("    imp_id:     0x%x\n", regs->gicd.imp_id);
	print("    security:   0x%x\n", regs->gicd.security);
	print("    set_en:     0x%x 0x%x 0x%x 0x%x\n", set_en[0], set_en[1], set_en[2], set_en[3]);
	print("    clr_en:     0x%x 0x%x 0x%x 0x%x\n", clr_en[0], clr_en[1], clr_en[2], clr_en[3]);
	print("    set_pend:   0x%x 0x%x 0x%x 0x%x\n", set_pend[0], set_pend[1], set_pend[2], set_pend[3]);
	print("    clr_pend:   0x%x 0x%x 0x%x 0x%x\n", clr_pend[0], clr_pend[1], clr_pend[2], clr_pend[3]);
}

#undef print

#endif // INTC_H
