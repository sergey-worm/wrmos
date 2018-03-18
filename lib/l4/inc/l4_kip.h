//##################################################################################################
//
//  API to Kernel Interface Page area.
//
//##################################################################################################

#ifndef L4_KIP_H
#define L4_KIP_H

#include "sys_types.h"

// magic is "L4µK"
enum
{
	#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	L4_kip_magic = 0x4c34e64b
	#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	L4_kip_magic = 0x4be6344c
	#endif
};

struct L4_kip_thr_info_t
{
	union
	{
		word_t _raw;
		struct
		{
			unsigned user_base    : 12;
			unsigned system_base  : 12;
			unsigned bits_max     :  8;
		} _val;
	};

	word_t raw()         const { return _raw;                     }
	word_t user_base()   const { return _val.user_base;           }
	word_t system_base() const { return _val.system_base;         }
	word_t number_max()  const { return (1 << _val.bits_max) - 1; }

	void user_base(word_t v)   { _val.user_base   = v; }
	void system_base(word_t v) { _val.system_base = v; }
	void bits_max(word_t v)    { _val.bits_max    = v; }
};

static_assert(sizeof(L4_kip_thr_info_t) == sizeof(word_t));

typedef struct
{
	enum { Width = sizeof(word_t) * 8 / 2 };
	union
	{
		word_t _raw;
		struct
		{
			unsigned mem_desc_ptr : Width;
			unsigned number       : Width;
		};
	};
} Kip_mem_info_t;

static_assert(sizeof(Kip_mem_info_t) == sizeof(word_t));

typedef struct
{
	enum { Addr_width = sizeof(word_t) * 8 - 10 }; // 22/54 for 32/64-bit systems
	union
	{
		word_t _raw[2];
		struct
		{
			word_t   heigh      : Addr_width;
			unsigned reserve_1  : 10;
			word_t   low        : Addr_width;
			unsigned virt       :  1;
			unsigned reserve_2  :  1;
			unsigned type_exact :  4;
			unsigned type       :  4;
		}/* _val*/;
	};

	enum
	{
		// type
		Undefined     = 0x0,  // undefined
		Conventional  = 0x1,  // conventional memory
		Reserved      = 0x2,  // reserved memory (i.e., reserved by kernel)
		Dedicated     = 0x3,  // dedicated memory (i.e., memory not available to user)
		Shared        = 0x4,  // shared memory (i.e., available to all users)
		Bootloader    = 0xe,  // defined by boot loader
		Arch          = 0xf,  // architecture dependen

		// type_exact for type = Bootloader
		Bldr_undefined       = 0x0,  //
		Bldr_sigma0_text     = 0x1,  // to map sigmao by kernel
		Bldr_sigma0_rodata   = 0x2,  // -- // --
		Bldr_sigma0_data     = 0x3,  // -- // --
		Bldr_roottask_text   = 0x4,  // to process roottask pg faults by sigma0
		Bldr_roottask_rodata = 0x5,  // -- // --
		Bldr_roottask_data   = 0x6,  // -- // --
		Bldr_ramfs           = 0x7   // config to start apps
	};
} Mem_desc_t;

static_assert(sizeof(Mem_desc_t) == 2*sizeof(word_t));

// according to l4-x2-r6.pdf (sections 1.1 and 7.7)
typedef struct
{
	word_t magic;                  // 0x00 / 0x00  L4μK
	word_t api_version;            //
	word_t api_flags;              //
	word_t kern_desc_ptr;          //

	word_t kdebug_init;            // 0x10 / 0x20
	word_t kdebug_entry;           //
	word_t kdebug_low;             //
	word_t kdebug_hight;           //

	word_t sigma0_sp;              // 0x20 / 0x40
	word_t sigma0_ip;              //
	word_t sigma0_low;             //
	word_t sigma0_hight;           //

	word_t sigma1_sp;              // 0x30 / 0x60
	word_t sigma1_ip;              //
	word_t sigma1_low;             //
	word_t sigma1_hight;           //

	word_t roottask_sp;            // 0x40 / 0x80
	word_t roottask_ip;            //
	word_t roottask_low;           //
	word_t roottask_hight;         //

	#if 1 // wrm extention
	word_t tick_usec;              // 0x50 / 0xa0
	#else
	word_t padding_1 [1];          // 0x50 / 0xa0
	#endif
	word_t memory_info;            //
	word_t kdebug_config0;         //
	word_t kdebug_config1;         //

	word_t padding_2 [16];         // 0x60 / 0xc0

	#if 1 // wrm extention
	word_t uptime_usec_hi;         //
	word_t uptime_usec_lo;         //
	#else // original L4
	word_t padding_3 [2];          // 0xa0 / 0x140
	#endif
	word_t utcb_info;              //
	word_t kip_area_info;          //

	word_t padding_4 [2];          // 0xb0 / 0x160
	word_t boot_info;              //
	word_t proc_desc_ptr;          //

	word_t clock_info;             // 0xc0 / 0x180
	L4_kip_thr_info_t thread_info; //
	word_t page_info;              //
	word_t proc_info;              //

	word_t space_control_psc;      // 0xd0 / 0x1a0  // privileged syscall
	word_t thread_control_psc;     //               // privileged syscall
	word_t proc_control_psc;       //               // privileged syscall
	word_t mem_control_psc;        //               // privileged syscall

	word_t ipc_sc;                 // 0xe0 / 0x1c0  // normal syscall
	word_t lipc_sc;                //               // normal syscall
	word_t unmap_sc;               //               // normal syscall
	word_t exchange_registers_sc;  //               // normal syscall

	word_t system_clock_sc;        // 0xf0 / 0x1e0  // normal syscall
	word_t thread_switch_sc;       //               // normal syscall
	word_t schedule_sc;            //               // normal syscall
	word_t padding_5 [1];          //
} L4_kip_t;

static_assert(sizeof(L4_kip_t) == 64*sizeof(word_t));

inline void l4_kip_set_uptime_usec(L4_kip_t* kip, uint64_t uptime_usec)
{
	if (sizeof(word_t) == 4)
	{
		kip->uptime_usec_hi = uptime_usec >> 32;
		kip->uptime_usec_lo = uptime_usec;
	}
	else // 64-bit arch
	{
		kip->uptime_usec_lo = uptime_usec;
	}
}

inline uint64_t l4_kip_get_uptime_usec(L4_kip_t* kip)
{
	if (sizeof(word_t) == 4)
	{
		return ((uint64_t)kip->uptime_usec_hi << 32) | kip->uptime_usec_lo;
	}
	else // 64-bit arch
	{
		return kip->uptime_usec_lo;
	}
}

#endif // L4_KIP_H
