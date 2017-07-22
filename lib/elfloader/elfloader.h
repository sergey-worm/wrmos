//##################################################################################################
//
//  Module for read headers and load ELF files.
//
//##################################################################################################

#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned (*Elf_dprint_t)(const char* format, ...);
#define Elf_no_dprint ((Elf_dprint_t)0)

// check correctness of elf file
// return:  0 if success or error code
int elf_check(const void* elf, unsigned sz, Elf_dprint_t dprint = Elf_no_dprint);

// get entry address
// out:     entry_point - entry point of elf
// return:  0 if success or error code
int elf_entry(const void* elf, unsigned sz, unsigned* entry_point, Elf_dprint_t dprint = Elf_no_dprint);

// print header info of elf file
// return:  0 if success or error code
int elf_dump(const void* elf, unsigned sz, Elf_dprint_t dprint = Elf_no_dprint);

typedef void (*Elf_shdr_func_t)(size_t label, uintptr_t addr, size_t sz, unsigned access,
                                const char* name, int is_progbits);
typedef void (*Elf_phdr_func_t)(size_t label, uintptr_t vaddr, uintptr_t paddr, size_t sz,
                                uintptr_t location, int is_load);
#define Elf_no_shdr_func ((Elf_shdr_func_t)0)
#define Elf_no_phdr_func ((Elf_phdr_func_t)0)

// iterate for each section-header and program-header
// return:  0 if success or error code
int elf_foreach(const void* elf, size_t sz, Elf_shdr_func_t sh_func,
                Elf_phdr_func_t ph_func, size_t label, uintptr_t* entry, Elf_dprint_t dprint = Elf_no_dprint);


#ifdef __cplusplus
}
#endif

#endif // ELFLOADER_H
