//##################################################################################################
//
//  API for low-level GRETH driver.
//
//##################################################################################################

#ifndef GRETH_H
#define GRETH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// forward declaration, definition in greth.c
struct Greth_regs_t;

typedef void (*Greth_dprint_t)(const char* format, ...);
typedef void (*Greth_dcache_inval_t)(unsigned addr, unsigned sz);
typedef void (*Greth_dcache_flush_t)(unsigned addr, unsigned sz);
typedef uint64_t Greth_descr_t;

// NOTE:  examples of cache controls for kernel mode:
//        inval:  asm volatile ("lda [%[addr]] 0x01, %%g0" : : [addr] "r" (ptr));
//        flush:  asm volatile ("lda [%[addr]] 0x10, %%g0" : : [addr] "r" (ptr));

// greth device data
typedef struct Greth_dev
{
	// user set up data
	struct Greth_regs_t* regs;            // valid memory mapped device address
	Greth_descr_t* tx_dtb;                // buf for TX descriptor table, virt addr, aligned to 1kB
	Greth_descr_t* rx_dtb;                // buf for RX descriptor table, virt addr, aligned to 1kB
	Greth_descr_t* tx_dtb_phys;           // phys addresses of TX descriptor table
	Greth_descr_t* rx_dtb_phys;           // phys addresses of RX descriptor table
	unsigned tx_dtb_sz;                   // TX descr table size in 8 byte elements, >=1, <=128
	unsigned rx_dtb_sz;                   // RX descr table size in 8 byte elements, >=1, <=128
	uint64_t mac;                         // 6 bytes of MAC address
	Greth_dcache_inval_t dcache_inval;    // to invalidate data cache
	Greth_dcache_flush_t dcache_flush;    // to write dcache to memory
	Greth_dprint_t dprint;                // set print callback if you want

	// auxiliary tx fields, don't touch
	Greth_descr_t* tx_dtb_start;          // start of TX descriptor table
	Greth_descr_t* tx_dtb_end;            // end of TX descriptor table
	Greth_descr_t* tx_dptr_wr;            // ptr to set TX descriptor
	Greth_descr_t* tx_dptr_rd;            // ptr to read TX descriptor

	// auxiliary rx fields, don't touch
	Greth_descr_t* rx_dtb_start;          // start of RX descriptor table
	Greth_descr_t* rx_dtb_end;            // end of RX descriptor table
	Greth_descr_t* rx_dptr_wr;            // ptr to set RX descriptor
	Greth_descr_t* rx_dptr_rd;            // ptr to read RX descriptor
} Greth_dev_t;

typedef enum
{
	Greth_ok               = 0,
	Greth_err_wrong_param  = 1,
	Greth_err_no_buf       = 2,
	Greth_err_not_aligned  = 3,
	Greth_err_wrong_size   = 4,
	Greth_err_too_small    = 5,
	Greth_err_too_big      = 6,
	Greth_err_busy         = 7,
	Greth_err_failed       = 8,
	Greth_err_empty        = 9,
} Greth_err_t;

enum
{
	// GRETH rx/tx buffers contains the next parts of ETH frame:
	//     dst_addr  - 6 bytes,
	//     src_addr  - 6 bytes,
	//     tag (opt) - 4 bytes,
	//     ethertype - 2 bytes
	//     payload   - 46 (42) - 1500 bytes
	Greth_buf_min = 60,
	Greth_buf_max = 1518
};

typedef enum
{
	Greth_tx_status_idle  = 1, // not used
	Greth_tx_status_txing = 2, // transmitting
	Greth_tx_status_ok    = 3, // transmited ok
	Greth_tx_status_err   = 4, // transmited failed
} Greth_tx_status_t;

typedef enum
{
	Greth_rx_status_idle  = 5, // not used
	Greth_rx_status_rxing = 6, // receiving
	Greth_rx_status_ok    = 7, // received ok
	Greth_rx_status_err   = 8, // received failed
} Greth_rx_status_t;

typedef enum
{
	Greth_irq_status_nil  = 0,      // no irq happened
	Greth_irq_status_tx   = 1 << 0, // tx irq happened
	Greth_irq_status_rx   = 1 << 1, // rx irq happened
} Greth_irq_status_t;

// Common notes:
//   return   - 0 if success, else error code;
//   buf_phys - aligne to 4 bytes

void greth_dump(const Greth_dev_t* dev);      // dump device state via dprint

Greth_err_t greth_init(Greth_dev_t* dev);   // init device

Greth_err_t greth_transmit(Greth_dev_t* dev, const void* buf_phys, unsigned size);
Greth_err_t greth_txstatus(Greth_dev_t* dev, void** buf_phys, Greth_tx_status_t* tx_status);
Greth_err_t greth_txfree(Greth_dev_t* dev);

Greth_err_t greth_receive(Greth_dev_t* dev, void* buf_phys, unsigned size);
Greth_err_t greth_rxstatus(Greth_dev_t* dev, void** buf_phys, unsigned* size, Greth_rx_status_t* rx_status);
Greth_err_t greth_rxfree(Greth_dev_t* dev);

Greth_err_t greth_irq_status(Greth_dev_t* dev, Greth_irq_status_t* irq_status);

#ifdef __cplusplus
}
#endif

#endif // GRETH_H

