//##################################################################################################
//
//  Convert virtual memory access to mmu-access-permission.
//
//##################################################################################################

#ifndef ACC_2_MMUACC
#define ACC_2_MMUACC

#include "mmu.h"
#include "access.h"

inline unsigned acc2mmuacc(kacc_t acc)
{
	// arm MMU accesses:
	//   Mmu_acc_kno_uno   = 0,
	//   Mmu_acc_krwx_uno  = 1,
	//   Mmu_acc_krwx_urx  = 2,
	//   Mmu_acc_krwx_urwx = 3
	switch (acc)
	{
		case Acc_kr:      return Mmu_acc_krwx_uno;
		case Acc_kw:      return Mmu_acc_krwx_uno;
		case Acc_kx:      return Mmu_acc_krwx_uno;
		case Acc_ur:      return Mmu_acc_krwx_urx;
		case Acc_uw:      return Mmu_acc_krwx_urwx;
		case Acc_ux:      return Mmu_acc_krwx_urx;
		case Acc_krx:     return Mmu_acc_krwx_uno;
		case Acc_krw:     return Mmu_acc_krwx_uno;
		case Acc_krwx:    return Mmu_acc_krwx_uno;
		case Acc_urx:     return Mmu_acc_krwx_urx;
		case Acc_urw:     return Mmu_acc_krwx_urwx;
		case Acc_urwx:    return Mmu_acc_krwx_urwx;
		case Acc_krw_urw: return Mmu_acc_krwx_urwx;
		case Acc_kx_ux:   return Mmu_acc_krwx_urx;
		case Acc_kw_ur:   return Mmu_acc_krwx_urx;
	}
	printk("%s:  acc=%d.\n", __func__, acc);
	assert(false);
	return -1;
}

#endif // ACC_2_MMUACC
