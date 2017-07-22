//##################################################################################################
//
//  Kernel highest-level entry points.
//
//##################################################################################################

#include "types.h"

void kentry_irq(unsigned irq);
void kentry_syscall();
void kentry_pagefault(word_t fault_addr, word_t fault_access, word_t fault_inst);
