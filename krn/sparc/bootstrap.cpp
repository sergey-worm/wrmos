//##################################################################################################
//
//  Create 1:1 and phys:virt mapping and go to kernel virtual address space.
//
//##################################################################################################

#include "mmu.h"
#include "krn-config.h"
#include "sys-utils.h"
#include "processor.h"
#include "bootstrap.h"
#include "startup.h"

// linker vars
extern int _diff_va_pa;
extern int _kern_vaddr;

// temporary page tables for rude initial mapping
static word_t ctxtb [Ctx_sz] __attribute__((aligned(sizeof(word_t) * Ctx_sz)));
static word_t l1tb  [L1_sz]  __attribute__((aligned(sizeof(word_t) * L1_sz)));

//  NOTE:      now we are using phys address space;  SP and TBR point to virt address;
//  DANGEROS:  no stack vars here;  no traps/exceptions here;  no calling of functions
//  DO:        turn on MMU and go to virtual address space
extern "C" void bootstrap()
{
	// initialize page tables

	addr_t kern_va = (addr_t)&_kern_vaddr;
	addr_t kern_pa = (addr_t)&_kern_vaddr - (addr_t)&_diff_va_pa;
	unsigned va_index_base = kern_va / L1_pgsz;
	unsigned pa_index_base = kern_pa / L1_pgsz;

	mmu_zero(ctxtb, sizeof(ctxtb));
	mmu_zero(l1tb, sizeof(l1tb));

	// rude mapping:  kern_va -> kern_pa, and kern_pa -> kern_pa (1:1)
	unsigned ctx = 0;                               // use context 0
	mmu_reg_ctx(ctx);                               // set current context
	mmu_reg_ctxtb((paddr_t)ctxtb);                  // set context page table
	mmu_set_dir(0, ctxtb + ctx, (paddr_t)l1tb);
	for (unsigned i=0; i<Karea_sz/L1_pgsz; ++i)
	{
		mmu_set_map(1, l1tb + pa_index_base + i, kern_pa + i * L1_pgsz, Mmu_acc_krwx_uno, Cachable);
		mmu_set_map(1, l1tb + va_index_base + i, kern_pa + i * L1_pgsz, Mmu_acc_krwx_uno, Cachable);
	}
	mmu_set_map(1, l1tb + Cfg_krn_uart_vaddr / L1_pgsz, Cfg_krn_uart_paddr, Mmu_acc_krwx_uno, NotCachable);

	word_t ctrl = mmu_reg_ctrl();                   //
	ctrl |= 1;                                      //
	mmu_reg_ctrl(ctrl);                             // enable MMU
	mmu_tlb_flush();                                // flush TLB, I-cache, D-cache

	// set startup stack and go to virtual aspace
	Proc::set_new_stack_area((word_t)(startup_stack), Startup_stack_sz);
	startup();
}

// NOTE:  this function link in virtual address space.
// mapping phys:phys was need to switch from physical address
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
