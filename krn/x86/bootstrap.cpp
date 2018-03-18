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

// linker vars
extern int _diff_va_pa;
extern int _kern_vaddr;

// temporary page tables for rude initial mapping
static word_t l1tb [L1_sz] __attribute__((aligned(sizeof(word_t) * L1_sz)));

// now MMU is disabled and CPU works on phys address space
// turn on MMU and go to virtual address space
extern "C" void bootstrap()
{
	// initialize page tables

	addr_t kern_va = (addr_t)&_kern_vaddr;
	addr_t kern_pa = (addr_t)&_kern_vaddr - (addr_t)&_diff_va_pa;
	unsigned va_index_base = kern_va / L1_pgsz;
	unsigned pa_index_base = kern_pa / L1_pgsz;

	mmu_zero(l1tb, sizeof(l1tb));

	// rude mapping:  kern_pa -> kern_pa (1:1), kern_pv -> kern_pa
	for (unsigned i=0; i<Karea_sz/L1_pgsz; ++i)
	{
		mmu_set_map(1, l1tb + pa_index_base + i, kern_pa + i * L1_pgsz, Mmu_acc_krwx_uno, Cachable);
		mmu_set_map(1, l1tb + va_index_base + i, kern_pa + i * L1_pgsz, Mmu_acc_krwx_uno, Cachable);
	}
	mmu_set_map(1, l1tb + Cfg_krn_uart_vaddr / L1_pgsz, Cfg_krn_uart_paddr, Mmu_acc_krwx_uno, NotCachable);

	mmu_root_table((word_t)l1tb);
	mmu_enable_pse();    // enable Page Size Extention to allow use 4 MB pages
	mmu_enable_paging();
	mmu_tlb_flush();

	// set startup stack and go to virtual aspace
	Proc::set_new_stack_area((word_t)(startup_stack), Startup_stack_sz);
	Proc::jump((long)startup);  // not 'call' to avoid 'pop' operation with new stack
}

// NOTE:  this function link in virtual address space.
// mapping phys:phys was need to jump from physical address
// space to virtual, it is no longer needed
void __attribute__((section(".virt.text"))) bootstrap_remove_phys_mapping()
{
	addr_t kern_pa = (addr_t)&_kern_vaddr - (addr_t)&_diff_va_pa;
	unsigned pa_index_base = kern_pa / L1_pgsz;
	for (unsigned i=0; i<Karea_sz/L1_pgsz; ++i)
	{
		mmu_set_inv(1, l1tb + pa_index_base + i);
	}
	mmu_tlb_flush();
}
