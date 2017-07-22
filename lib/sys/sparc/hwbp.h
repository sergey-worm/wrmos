//##################################################################################################
//
//  HWBP - HardWare Break Point - API to use hardware breakpoint on Leon3 processor.
//
//##################################################################################################

#include <stdint.h>

enum Hwbp_flags_t
{
    Hwbp_off = 0,
    Hwbp_r   = 1 << 2,
    Hwbp_w   = 1 << 1,
    Hwbp_x   = 1 << 0,
    Hwbp_rw  = Hwbp_r | Hwbp_w,
    Hwbp_rwx = Hwbp_r | Hwbp_w | Hwbp_x
};

// set hardware breakpoint
int hwbp_set(int bpid, uint32_t addr, uint32_t mask = 0xfffffffc, int flags = Hwbp_rwx);

// read hardware breakpoint
bool hwbp_isset(int bpid, uint32_t* addr = 0, uint32_t* mask = 0, int* flags = 0);
