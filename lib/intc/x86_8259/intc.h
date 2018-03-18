//##################################################################################################
//
//  Low-level driver for 8259A Programmable Interrupt Controller (PIC 8259).
//
//##################################################################################################

#ifndef INTC_H
#define INTC_H

#include <stdint.h>
#include "sys_proc.h"

#ifdef DEBUG
#  define print(...)  if (dprint) dprint(__VA_ARGS__)
#else
#  define print(...) (void)dprint
#endif

//--------------------------------------------------------------------------------------------------
//  INTC:  pic8259a
//--------------------------------------------------------------------------------------------------
enum
{
	PIC1            = 0x20,       // IO base address for master PIC
	PIC2            = 0xa0,       // IO base address for slave PIC
	PIC1_CMD        = PIC1 + 0,
	PIC1_DATA       = PIC1 + 1,
	PIC2_CMD        = PIC2 + 0,
	PIC2_DATA       = PIC2 + 1,

	PIC_EOI         = 0x20,       // End-of-interrupt command code

	ICW1_ICW4       = 0x01,       // ICW4 (not) needed
	ICW1_SINGLE     = 0x02,       // Single (cascade) mode
	ICW1_INTERVAL4  = 0x04,       // Call address interval 4 (8)
	ICW1_LEVEL      = 0x08,       // Level triggered (edge) mode
	ICW1_INIT       = 0x10,       // Initialization - required!

	ICW4_8086       = 0x01,       // 8086/88 (MCS-80/85) mode
	ICW4_AUTO       = 0x02,       // Auto (normal) EOI
	ICW4_BUF_SLAVE  = 0x08,       // Buffered mode/slave
	ICW4_BUF_MASTER = 0x0C,       // Buffered mode/master
	ICW4_SFNM       = 0x10,       // Special fully nested (not)

	PIC_READ_IRR    = 0x0a,       // OCW3 irq ready next CMD read
	PIC_READ_ISR    = 0x0b,       // OCW3 irq service next CMD read
};

inline uint16_t _intc_get_mask()
{
	return ((uint16_t)Proc::inb(PIC2_DATA) << 8) | Proc::inb(PIC1_DATA);
}

// helper func
inline uint16_t _intc_get_reg(int ocw3)
{
	// OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
	// represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain
	Proc::outb(PIC1_CMD, ocw3);
	Proc::outb(PIC2_CMD, ocw3);
	return (Proc::inb(PIC2_CMD) << 8) | Proc::inb(PIC1_CMD);
}

// return combined value of the cascaded PICs irq request register
inline uint16_t _intc_get_irr()
{
	return _intc_get_reg(PIC_READ_IRR);
}

// return combined value of the cascaded PICs in-service register
inline uint16_t _intc_get_isr()
{
	return _intc_get_reg(PIC_READ_ISR);
}

typedef void (*Intc_print_t)(const char* format, ...);

inline void intc_init(uintptr_t base_addr, Intc_print_t dprint)
{
	(void) base_addr;

	// mask all irqs
	Proc::outb(PIC1_DATA, 0xff);
	Proc::outb(PIC2_DATA, 0xff);

	// remap irqs
	// reinit PIC controllers with vector offsets 0x20 and 0x28 instead of default 0x8 and 0x70
	int offset1 = 0x20;  // vector offset for master PIC [0x20; 0x27]
	int offset2 = 0x28;  // same for slave PIC [0x28; 0x2f]

	uint8_t a1 = Proc::inb(PIC1_DATA);                // save masks
	uint8_t a2 = Proc::inb(PIC2_DATA);
	Proc::outb(PIC1_CMD, ICW1_INIT + ICW1_ICW4);      // starts the initialization sequence (in cascade mode)
	Proc::outb(PIC2_CMD, ICW1_INIT + ICW1_ICW4);
	Proc::outb(PIC1_DATA, offset1);                   // ICW2: Master PIC vector offset
	Proc::outb(PIC2_DATA, offset2);                   // ICW2: Slave PIC vector offset
	Proc::outb(PIC1_DATA, 4);                         // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	Proc::outb(PIC2_DATA, 2);                         // ICW3: tell Slave PIC its cascade identity (0000 0010)
	Proc::outb(PIC1_DATA, ICW4_8086);
	Proc::outb(PIC2_DATA, ICW4_8086);
	Proc::outb(PIC1_DATA, a1);                        // restore saved masks
	Proc::outb(PIC2_DATA, a2);
	print("irr=0x%x, isr=0x%x, mask=0x%x", _intc_get_irr(), _intc_get_isr(), _intc_get_mask());
}

inline int intc_unmask(uintptr_t base_addr, unsigned irq)
{
	(void) base_addr;
	if (irq > 0xf)
		return 1;

	uint16_t ioport = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	irq -= (irq < 8) ? 0 : 8;
	uint8_t val = Proc::inb(ioport) & ~(1 << irq);
	Proc::outb(ioport, val);
	return 0;
}

inline int intc_mask(uintptr_t base_addr, unsigned irq)
{
	(void) base_addr;
	if (irq > 0xf)
		return 1;

	uint16_t ioport = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	irq -= (irq < 8) ? 0 : 8;
	uint8_t val = Proc::inb(ioport) | (1 << irq);
	Proc::outb(ioport, val);
	return 0;
}

inline int intc_clear(uintptr_t base_addr, unsigned irq)
{
	(void) base_addr;
	if (irq > 0xf)
		return 1;

	// end od interrrupt
	if (irq >= 8)
		Proc::outb(PIC2_CMD, PIC_EOI);
	Proc::outb(PIC1_CMD, PIC_EOI);
	return 0;
}

inline bool intc_is_pending(uintptr_t base_addr, unsigned irq)
{
	//printf(" -- isr=0x%x, irr=0x%x.\n", _intc_get_isr(), _intc_get_irr());
	if (irq > 0xf)
		return 0;  // wront irq

	(void) base_addr;
	return _intc_get_isr() & (1 << irq)  ||  _intc_get_irr() & (1 << irq); // in-service or raised
}

inline unsigned intc_real_irq(uintptr_t base_addr, unsigned irq)
{
	(void) base_addr;
	(void) irq;
	return 0;
}

inline void intc_dump(uintptr_t base_addr, Intc_print_t dprint)
{
	print("PIC 8259:\n");
	print("  mask:       0x%x\n", _intc_get_mask());
	print("  isr:        0x%x\n", _intc_get_isr());
	print("  irr:        0x%x\n", _intc_get_irr());

	(void) base_addr;
	(void) dprint;
}

#undef print

#endif // INTC_T
