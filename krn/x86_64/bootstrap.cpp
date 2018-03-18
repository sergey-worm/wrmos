//##################################################################################################
//
//  Create new 1:1 and phys:virt mapping and go to kernel virtual address space.
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
static word_t l1tb[L1_sz]    __attribute__((aligned(sizeof(word_t) * L1_sz)));
static word_t l2tb_pa[L2_sz] __attribute__((aligned(sizeof(word_t) * L2_sz)));
static word_t l2tb_va[L2_sz] __attribute__((aligned(sizeof(word_t) * L2_sz)));
static word_t l3tb_pa[L3_sz] __attribute__((aligned(sizeof(word_t) * L3_sz)));
static word_t l3tb_va[L3_sz] __attribute__((aligned(sizeof(word_t) * L3_sz)));

// now CPU in 64-bit mode, MMU is enabled and first 16 MB are mapped 1:1
// need to build new page tables with 1:1 mapping and phys:virt mapping
extern "C" void bootstrap()
{
	// initialize page tables

	addr_t kern_va = (addr_t)&_kern_vaddr;
	addr_t kern_pa = (addr_t)&_kern_vaddr - (addr_t)&_diff_va_pa;

	mmu_zero(l1tb, sizeof(l1tb));
	mmu_zero(l2tb_pa, sizeof(l2tb_pa));
	mmu_zero(l2tb_va, sizeof(l2tb_va));
	mmu_zero(l3tb_pa, sizeof(l3tb_pa));
	mmu_zero(l3tb_va, sizeof(l3tb_va));

	unsigned pa_index_l1 = (kern_pa / L1_pgsz) & (L1_sz - 1);
	unsigned va_index_l1 = (kern_va / L1_pgsz) & (L1_sz - 1);
	mmu_set_dir(1, l1tb + pa_index_l1, (uintptr_t) l2tb_pa);
	mmu_set_dir(1, l1tb + va_index_l1, (uintptr_t) l2tb_va);

	unsigned pa_index_l2 = (kern_pa / L2_pgsz) & (L2_sz - 1);
	unsigned va_index_l2 = (kern_va / L2_pgsz) & (L2_sz - 1);
	mmu_set_dir(2, l2tb_pa + pa_index_l2, (uintptr_t) l3tb_pa);
	mmu_set_dir(2, l2tb_va + va_index_l2, (uintptr_t) l3tb_va);

	// rude mapping:  kern_pa -> kern_pa (1:1), kern_pv -> kern_pa
	unsigned pa_index_l3 = (kern_pa / L3_pgsz) & (L3_sz - 1);
	unsigned va_index_l3 = (kern_va / L3_pgsz) & (L3_sz - 1);
	for (unsigned i=0; i<Karea_sz/L3_pgsz; ++i)
	{
		mmu_set_map(3, l3tb_pa + pa_index_l3 + i, kern_pa + i * L3_pgsz, Mmu_acc_krwx_uno, Cachable);
		mmu_set_map(3, l3tb_va + va_index_l3 + i, kern_pa + i * L3_pgsz, Mmu_acc_krwx_uno, Cachable);
	}
	unsigned index = (Cfg_krn_uart_vaddr / L3_pgsz) & (L3_sz - 1);
	mmu_set_map(3, l3tb_va + index, Cfg_krn_uart_paddr, Mmu_acc_krwx_uno, NotCachable);

	mmu_root_table((word_t)l1tb);

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
	unsigned pa_index_l1 = (kern_pa / L1_pgsz) & (L1_sz - 1);
	mmu_set_inv(1, l1tb + pa_index_l1);
	mmu_tlb_flush();
}
