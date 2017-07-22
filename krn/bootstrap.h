//##################################################################################################
//
//  bootstrap - create rude mapping and go to kernel startup.
//
//##################################################################################################▫

#ifndef BOOTSTRAP_H
#define BOOTSTRUP_H

enum { Karea_sz = 16 * 0x100000 }; // 16 MB, kernel area size for bootstrap rude mapping
void bootstrap_remove_phys_mapping();

#endif // BOOTSTRAP_H
