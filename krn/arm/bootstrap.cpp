//##################################################################################################
//
//  Create 1:1 and phys:virt mapping and go to kernel virtual address space.
//
//##################################################################################################

#include "mmu.h"
#include "krn-config.h"
#include "sys_proc.h"
#include "bootstrap.h"
#include "startup.h"

// use L1 rude mapping or L2+L1
//#define USE_L2_TB

// linker vars
extern int _diff_va_pa;
extern int _kern_vaddr;

// temporary page tables for rude initial mapping
static word_t l1tab [Cfg_max_cpus][L1_sz] __attribute__((aligned(sizeof(word_t) * L1_sz)));
#ifdef USE_L2_TB
static word_t l2tab [Cfg_max_cpus][Karea_sz/L1_pgsz][L2_sz] __attribute__((aligned(sizeof(word_t) * L2_sz)));
#endif

// now MMU is disabled and CPU works on phys address space
// turn on MMU and go to virtual address space
extern "C" void bootstrap()
{
	// initialize page tables
	addr_t kern_va = (addr_t)&_kern_vaddr;
	addr_t kern_pa = (addr_t)&_kern_vaddr - (addr_t)&_diff_va_pa;

	#ifndef USE_L2_TB // use L1 table

		word_t* l1tb = l1tab[Proc::cpuid()];

		unsigned va_index_base = kern_va / L1_pgsz;
		unsigned pa_index_base = kern_pa / L1_pgsz;

		mmu_zero(l1tb, sizeof(l1tb));

		// rude mapping:  kern_va -> kern_pa, and kern_pa -> kern_pa (1:1)
		for (unsigned i=0; i<Karea_sz/L1_pgsz; ++i)
		{
			mmu_set_map(1, l1tb + pa_index_base + i, kern_pa + i * L1_pgsz, Mmu_acc_krwx_uno, Cachable);
			mmu_set_map(1, l1tb + va_index_base + i, kern_pa + i * L1_pgsz, Mmu_acc_krwx_uno, Cachable);
		}
		mmu_set_map(1, l1tb + Cfg_krn_uart_vaddr / L1_pgsz, Cfg_krn_uart_paddr, Mmu_acc_krwx_uno, NotCachable);
		mmu_set_map(1, l1tb + Cfg_krn_intc_vaddr / L1_pgsz, Cfg_krn_intc_paddr, Mmu_acc_krwx_uno, NotCachable);

		/*
		// TODO:  map device io region
		#define Cfg_krn_devs_vaddr 0xfc000000
		#define Cfg_krn_devs_paddr 0xf8f00000
		mmu_set_map(1, l1tb + Cfg_krn_devs_vaddr / L1_pgsz, Cfg_krn_devs_paddr, Mmu_acc_krwx_uno, NotCachable);
		*/

	#else // use L2 tables

		word_t* l1tb = l1tab[Proc::cpuid()];
		word_t** l2tb = l2tab[Proc::cpuid()];

		unsigned l1_va_index_base = kern_va / L1_pgsz;
		unsigned l1_pa_index_base = kern_pa / L1_pgsz;

		mmu_zero(l1tb, sizeof(l1tb));
		mmu_zero((word_t*)l2tb, sizeof(l2tb));

		// rude mapping:  kern_va -> kern_pa, and kern_pa -> kern_pa (1:1)
		for (unsigned i=0; i<Karea_sz/L1_pgsz; ++i)
		{
			unsigned l2_va_index_base = 0;
			unsigned l2_pa_index_base = 0;

			for (unsigned j=0; j<L2_sz; ++j)
			{
				mmu_set_map(2, l2tb[i] + l2_pa_index_base + j, kern_pa + i*L1_pgsz + j*L2_pgsz, Mmu_acc_krwx_uno, Cachable);
				mmu_set_map(2, l2tb[i] + l2_va_index_base + j, kern_pa + i*L1_pgsz + j*L2_pgsz, Mmu_acc_krwx_uno, Cachable);
			}
			mmu_set_dir(1, l1tb + l1_pa_index_base + i, (paddr_t)l2tb[i]);
			mmu_set_dir(1, l1tb + l1_va_index_base + i, (paddr_t)l2tb[i]);
		}
		mmu_set_map(1, l1tb + Cfg_krn_uart_vaddr / L1_pgsz, Cfg_krn_uart_paddr, Mmu_acc_krwx_uno, NotCachable);
		mmu_set_map(1, l1tb + Cfg_krn_intc_vaddr / L1_pgsz, Cfg_krn_intc_paddr, Mmu_acc_krwx_uno, NotCachable);

	#endif

//*(volatile unsigned long*)0xf8f0000c = 0xffff;
//*(volatile unsigned long*)0xf8f00000 = 2;

	mmu_reg_ttbr((word_t)l1tb);
	mmu_reg_domain(3);
	mmu_reg_asid(0);
	mmu_reg_ctrl(Mmu_reg_ctrl_icache | Mmu_reg_ctrl_wrbuf | Mmu_reg_ctrl_dcache | Mmu_reg_ctrl_mmu);
	mmu_tlb_flush();

// unsigned long* scu = (unsigned long*)0xfc000000;
// (void)scu;
// *scu = 3;
 //Proc::l2cache_enable();




	// set startup stack and go to virtual aspace
	Proc::set_new_stack_area((word_t)(startup_stack[Proc::cpuid()]), Startup_stack_sz);
	Proc::jump((long)startup);  // not 'call' to avoid 'pop' operation with new stack
}

// NOTE:  this function link in virtual address space.
// mapping phys:phys was need to jump from physical address
// space to virtual, it is no longer needed
void __attribute__((section(".virt.text"))) bootstrap_remove_phys_mapping()
{
	word_t* l1tb = l1tab[Proc::cpuid()];
	addr_t kern_pa = (addr_t)&_kern_vaddr - (addr_t)&_diff_va_pa;
	unsigned pa_index_base = kern_pa / L1_pgsz;
	for (unsigned i=0; i<Karea_sz/L1_pgsz; ++i)
	{
		mmu_set_inv(1, l1tb + pa_index_base + i);
	}
	mmu_tlb_flush();
}
