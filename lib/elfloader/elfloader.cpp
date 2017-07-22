//##################################################################################################
//
//  Module for read headers and load ELF files.
//
//##################################################################################################

#include <string.h>
#include <stdint.h>
#include "elfloader.h"

#ifdef Cfg_debug
#  define print(...)  if (dprint) dprint(__VA_ARGS__)
#else
#  define print(...) (void)dprint
#endif

enum { Little_endian, Big_endian };

inline uint16_t to_host(uint16_t val, int val_endian)
{
	#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	if (val_endian != Big_endian)
	{
		uint16_t t = val;
		val = ((t >> 8) & 0xff) | ((t & 0xff) << 8);
	}
	#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	if (val_endian != Little_endian)
	{
		uint16_t t = val;
		val = ((t >> 8) & 0xff) | ((t & 0xff) << 8);
	}
	#endif
	return val;
}

inline uint32_t to_host(uint32_t val, int val_endian)
{
	#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	if (val_endian != Big_endian)
	{
		uint32_t t = val;
		val = ((t >> 24) & 0xff) | ((t >> 8) & 0xff00) | ((t & 0xff00) << 8) | ((t & 0xff) << 24);
	}
	#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	if (val_endian != Little_endian)
	{
		uint32_t t = val;
		val = ((t >> 24) & 0xff) | ((t >> 8) & 0xff00) | ((t & 0xff00) << 8) | ((t & 0xff) << 24);
	}
	#endif
	return val;
}

inline uint64_t to_host(uint64_t val, int val_endian)
{

	#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	if (val_endian != Big_endian)
	{
		uint64_t t = val;
		val = ((t >> 56) & 0xff) | ((t >> 40) & 0xff00) | ((t >> 24) & 0xff0000) | ((t >> 8) & 0xff000000) |
		      ((t & 0xff000000) << 8) | ((t & 0xff0000) << 24) | ((t & 0xff00) << 40) | ((t & 0xff) << 56);
	}
	#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	if (val_endian != Little_endian)
	{
		uint64_t t = val;
		val = ((t >> 56) & 0xff) | ((t >> 40) & 0xff00) | ((t >> 24) & 0xff0000) | ((t >> 8) & 0xff000000) |
		      ((t & 0xff000000) << 8) | ((t & 0xff0000) << 24) | ((t & 0xff00) << 40) | ((t & 0xff) << 56);
	}
	#endif
	return val;
}

void test_endianes(Elf_dprint_t dprint)
{
	uint16_t le16 = 0x1234;
	uint16_t be16 = 0x1234;
	dprint("[endian]  le16=0x%x --> host=0x%x,  be16=0x%x --> host=0x%x.\n",
		le16, to_host(le16, Little_endian), be16, to_host(be16, Big_endian));

	uint32_t le32 = 0x12345678;
	uint32_t be32 = 0x12345678;
	dprint("[endian]  le32=0x%x --> host=0x%x,  be32=0x%x --> host=0x%x.\n",
		le32, to_host(le32, Little_endian), be32, to_host(be32, Big_endian));

	uint64_t le64 = 0x123456789abcdef0;
	uint64_t be64 = 0x123456789abcdef0;
	dprint("[endian]  le64=0x%X.\n", le64);
	dprint("[endian]  le64=0x%X --> host=0x%X,  be64=0x%X --> host=0x%X.\n",
		le64, to_host(le64, Little_endian), be64, to_host(be64, Big_endian));
}

enum
{
	Elf_magic         = 0x7F454C46, // 0x7f, E, L, F

	// elf header format
	Fmt_32bit         = 1,
	Fmt_64bit         = 2,

	// elf header endian
	Endian_little     = 1,
	Endian_big        = 2,

	// elf header type
	Etype_reloc        = 1,
	Etype_exec         = 2,
	Etype_shared       = 3,
	Etype_core         = 4,

	// elf header machine
	Mach_sparc        = 0x02,
	Mach_x86          = 0x03,
	Mach_mips         = 0x08,
	Mach_ppc          = 0x14,
	Mach_aarm         = 0x28,
	Mach_superh       = 0x2A,
	Mach_ia64         = 0x32,
	Mach_x8664        = 0x3E,
	Mach_aarch64      = 0xB7,

	// section header type
	Stype_null          =  0,
	Stype_progbits      =  1,
	Stype_symtab        =  2,
	Stype_strtab        =  3,
	Stype_rela          =  4,
	Stype_hash          =  5,
	Stype_dynamic       =  6,
	Stype_note          =  7,
	Stype_nobits        =  8,
	Stype_rel           =  9,
	Stype_shlib         = 10,
	Stype_dynsym        = 11,
	Stype_loproc        = 0x70000000,
	Stype_hiproc        = 0x7fffffff,
	Stype_louser        = 0x80000000,
	Stype_hiuser        = 0xffffffff,

	// program header type
	Ptype_null          = 0,
	Ptype_load          = 1,
	Ptype_dynamic       = 2,
	Ptype_interp        = 3,
	Ptype_note          = 4,
	Ptype_shlib         = 5,
	Ptype_phdr          = 6,
	Ptype_loproc        = 0x70000000,
	Ptype_hiproc        = 0x7fffffff,
	Ptype_gnustack      = 0x6474e551,

	// program flags
	Pflags_x             = 0x1,
	Pflags_w             = 0x2,
	Pflags_r             = 0x4,
};

#if Cfg_debug

static const char* format_str(int format)
{
	switch (format)
	{
		case Fmt_32bit:   return "32-bit";
		case Fmt_64bit:   return "64-bit";
	}
	return "__unknown_format__";
}

static const char* endian_str(int endian)
{
	switch (endian)
	{
		case Endian_little:   return "little";
		case Endian_big:      return "big";
	}
	return "__unknown_endian__";
}

static const char* type_str(int etype)
{
	switch (etype)
	{
		case Etype_reloc:   return "relocatable";
		case Etype_exec:    return "executable";
		case Etype_shared:  return "shared";
		case Etype_core:    return "core";
	}
	return "__unknown_elf_type__";
}

static const char* machine_str(int machine)
{
	switch (machine)
	{
		case Mach_sparc:   return "sparc";
		case Mach_x86:     return "x86";
		case Mach_mips:    return "mips";
		case Mach_ppc:     return "power-pc";
		case Mach_aarm:    return "arm";
		case Mach_superh:  return "super-h";
		case Mach_ia64:    return "ia-64";
		case Mach_x8664:   return "x86-64";
		case Mach_aarch64: return "aarch64";
	}
	return "__unknown_machine__";
}

static const char* stype_str(int stype)
{
	switch (stype)
	{
		case Stype_null:     return "null";
		case Stype_progbits: return "progbits";
		case Stype_symtab:   return "symtab";
		case Stype_strtab:   return "strtab";
		case Stype_rela:     return "rela";
		case Stype_hash:     return "hash";
		case Stype_dynamic:  return "dynamic";
		case Stype_note:     return "note";
		case Stype_nobits:   return "nobits";
		case Stype_rel:      return "rel";
		case Stype_shlib:    return "shlib";
		case Stype_dynsym:   return "dynsym";
		case Stype_loproc:   return "loproc";
		case Stype_hiproc:   return "hiproc";
		case Stype_louser:   return "louser";
		case Stype_hiuser:   return "hiuser";
		case 0x6ffffff5:     return "0x6ffffff5";
	}
	return "__unknown_sect_type__";
}

static const char* ptype_str(int ptype)
{
	switch (ptype)
	{
		case Ptype_null:     return "null";
		case Ptype_load:     return "load";
		case Ptype_dynamic:  return "dynamic";
		case Ptype_interp:   return "interp";
		case Ptype_note:     return "note";
		case Ptype_shlib:    return "shlib";
		case Ptype_phdr:     return "phdr";
		case Ptype_loproc:   return "loproc";
		case Ptype_hiproc:   return "hiproc";
		case Ptype_gnustack: return "gnustack";
	}
	return "__unknown_progr_type__";
}

static const char* pflags_str(int pflags)
{
	switch (pflags)
	{
		case 0:                               return "access_denied";
		case Pflags_r:                        return "r";
		case Pflags_w:                        return "w";
		case Pflags_x:                        return "x";
		case Pflags_r | Pflags_w:             return "rw";
		case Pflags_r | Pflags_x:             return "rx";
		case Pflags_w | Pflags_x:             return "wx";
		case Pflags_r | Pflags_w | Pflags_x:  return "rwx";
	}
	return "__unknown_pflags_type__";
}

#endif // Cfg_debug

static unsigned sflags2acc(unsigned flags)
{
	enum { Flag_write = 1, Flag_alloc = 2, Flag_exec = 4 };
	enum { Acc_r = 4, Acc_w = 2, Acc_x = 1 };
	unsigned res = 0;
	if (flags & Flag_write)  res |= Acc_w;
	if (flags & Flag_alloc)  res |= Acc_r;
	if (flags & Flag_exec)   res |= Acc_x;
	return res;
};

struct Elf_ident_t
{
	uint8_t magic[4]; // magic:  0x7F, 'E', 'L', 'F'
	uint8_t format;   // 1 - 32-bit, 1 - 64-bit
	uint8_t endian;   // 1 - little, 2 - big
	uint8_t ver;      // always 1
	uint8_t osabi;    // target OS ABI
	uint8_t abiver;   // ABI version
	uint8_t pad[7];   // padding, unused
};

struct Elf_hdr32_t
{
	Elf_ident_t ident;    //
	uint16_t type;        // 1 - relocatable, 2 - executable, 3 - shared, 4 - core
	uint16_t machine;     // 0x02 - SPARC, 0x03 - x86, 0x28 - ARM
	uint32_t ver;         // always 1
	uint32_t entry;       // entry point,                 (uint64_t) size depends on ident.format
	uint32_t phoff;       // program header table offset, (uint64_t) size depends on ident.format
	uint32_t shoff;       // section header table offset, (uint64_t) size depends on ident.format
	uint32_t flags;       // depends on the target architecture
	uint16_t ehsize;      // elf header size, 52 bytes for 32-bit, 64 for 64-bit
	uint16_t phentsize;   // program header table entry size
	uint16_t phnum;       // program header table entries number
	uint16_t shentsize;   // section header table entry size
	uint16_t shnum;       // section header table entries number
	uint16_t shstrndx;    // index of the section header table entry that contains the section names
};

struct Elf_hdr64_t
{
	Elf_ident_t ident;    //
	uint16_t type;        // 1 - relocatable, 2 - executable, 3 - shared, 4 - core
	uint16_t machine;     // 0x02 - SPARC, 0x03 - x86, 0x28 - ARM
	uint32_t ver;         // always 1
	uint64_t entry;       // entry point,                 (uint64_t) size depends on ident.format
	uint64_t phoff;       // program header table offset, (uint64_t) size depends on ident.format
	uint64_t shoff;       // section header table offset, (uint64_t) size depends on ident.format
	uint32_t flags;       // depends on the target architecture
	uint16_t ehsize;      // elf header size, 52 bytes for 32-bit, 64 for 64-bit
	uint16_t phentsize;   // program header table entry size
	uint16_t phnum;       // program header table entries number
	uint16_t shentsize;   // section header table entry size
	uint16_t shnum;       // section header table entries number
	uint16_t shstrndx;    // index of the section header table entry that contains the section names
};

struct Elf_shdr32_t
{
	uint32_t name;
	uint32_t type;
	uint32_t flags;
	uint32_t addr;
	uint32_t offset;
	uint32_t size;
	uint32_t link;
	uint32_t info;
	uint32_t addralign;
	uint32_t entsize;
};

struct Elf_shdr64_t
{
	uint32_t name;       // section name, index in string tbl
	uint32_t type;       // section type
	uint64_t flags;      // section misc attributes
	uint64_t addr;       // section virt addr
	uint64_t offset;     // section file offset
	uint64_t size;       // section size in bytes
	uint32_t link;       // index of another section
	uint32_t info;       // additional section info
	uint64_t addralign;  // section alignment
	uint64_t entsize;    // entry size if section holds table
};

struct Elf_phdr32_t
{
	uint32_t type;
	uint32_t offset;
	uint32_t vaddr;
	uint32_t paddr;
	uint32_t filesz;
	uint32_t memsz;
	uint32_t flags;
	uint32_t align;
};

struct Elf_phdr64_t
{
	uint32_t type;    //
	uint32_t flags;   //
	uint64_t offset;  // segment file offset
	uint64_t vaddr;   // segment virt address
	uint64_t paddr;   // segment phys address
	uint64_t filesz;  // segment size in file
	uint64_t memsz;   // segment size in memory
	uint64_t align;   // segment alignment, file & memory
};

template <typename HDR_t>
static void to_host_hdr(HDR_t* in, HDR_t* out, int endian)
{
	out->type      = to_host(in->type, endian);
	out->machine   = to_host(in->machine, endian);
	out->ver       = to_host(in->ver, endian);
	out->entry     = to_host(in->entry, endian);
	out->phoff     = to_host(in->phoff, endian);
	out->shoff     = to_host(in->shoff, endian);
	out->flags     = to_host(in->flags, endian);
	out->ehsize    = to_host(in->ehsize, endian);
	out->phentsize = to_host(in->phentsize, endian);
	out->phnum     = to_host(in->phnum, endian);
	out->shentsize = to_host(in->shentsize, endian);
	out->shnum     = to_host(in->shnum, endian);
	out->shstrndx  = to_host(in->shstrndx, endian);
}

template <typename SHDR_t>
static void to_host_shdr(SHDR_t* in, SHDR_t* out, int endian)
{
	out->name      = to_host(in->name, endian);
	out->type      = to_host(in->type, endian);
	out->flags     = to_host(in->flags, endian);
	out->addr      = to_host(in->addr, endian);
	out->offset    = to_host(in->offset, endian);
	out->size      = to_host(in->size, endian);
	out->link      = to_host(in->link, endian);
	out->info      = to_host(in->info, endian);
	out->addralign = to_host(in->addralign, endian);
	out->entsize   = to_host(in->entsize, endian);
}

template <typename PHDR_t>
static void to_host_phdr(PHDR_t* in, PHDR_t* out, int endian)
{
	out->type   = to_host(in->type, endian);
	out->offset = to_host(in->offset, endian);
	out->vaddr  = to_host(in->vaddr, endian);
	out->paddr  = to_host(in->paddr, endian);
	out->filesz = to_host(in->filesz, endian);
	out->memsz  = to_host(in->memsz, endian);
	out->flags  = to_host(in->flags, endian);
	out->align  = to_host(in->align, endian);
}

extern "C" int elf_check(const void* elf, unsigned sz, Elf_dprint_t dprint)
{
	if (sz < sizeof(Elf_hdr32_t))
	{
		print("ERROR:  ELF size too small (%u)\n", sz);
		return 1;
	}

	if ((size_t)elf & 3)
	{
		print("ERROR:  ELF does not align to 4 byte.\n");
		return 2;
	}

	// check ident

	Elf_ident_t* ident = (Elf_ident_t*) elf;

	if (ident->magic[0] != 0x7f  ||
	    ident->magic[1] != 'E'   ||
	    ident->magic[2] != 'L'   ||
	    ident->magic[3] != 'F')
	{
		print("ERROR:  wrong magic=%.4s.\n", ident->magic);
		return 3;
	}

	if (ident->format != Fmt_32bit  &&  ident->format != Fmt_64bit)
	{
		print("ERROR:  wrong format.\n");
		return 4;
	}

	if (ident->endian != Endian_little  &&  ident->endian != Endian_big)
	{
		print("ERROR:  wrong endian.\n");
		return 5;
	}

	int endian = ident->endian == Endian_big ? Big_endian : Little_endian;

	// check elf header

	if (ident->format == Fmt_32bit)
	{
		// convert to host endianness
		Elf_hdr32_t* eh32 = (Elf_hdr32_t*)(void*) elf;
		Elf_hdr32_t ehdr32;
		to_host_hdr(eh32, &ehdr32, endian);

		if (ehdr32.ehsize != sizeof(Elf_hdr32_t))
		{
			print("ERROR:  wrong elf header size (%u), expected %u.\n", ehdr32.ehsize, sizeof(Elf_hdr32_t));
			return 6;
		}

		// check sections headers

		unsigned shend = ehdr32.shoff + ehdr32.shnum * ehdr32.shentsize;
		if (sz < shend)
		{
			print("ERROR:  ELF is size too small (%u), expect >= %u.\n", sz, shend);
			return 7;
		}

		if (ehdr32.shentsize != sizeof(Elf_shdr32_t))
		{
			print("ERROR:  unexpected section header entry size (%u), expect %u.\n",
				ehdr32.shentsize, sizeof(Elf_shdr32_t));
			return 8;
		}

		if (ehdr32.shstrndx >= ehdr32.shnum)
		{
			print("ERROR:  wrong index of section with names.\n");
			return 9;
		}

		// check program headers

		if (ehdr32.phentsize != sizeof(Elf_phdr32_t))
		{
			print("ERROR:  unexpected program header entry size (%u), expect %u.\n",
				ehdr32.phentsize, sizeof(Elf_phdr32_t));
			return 10;
		}
	}
	else // 64-bit
	{
		if (sz < sizeof(Elf_hdr64_t))
		{
			print("ERROR:  ELF size too small (%u)", sz);
			return 11;
		}

		// convert to host endianness
		Elf_hdr64_t* eh64 = (Elf_hdr64_t*)(void*) elf;
		Elf_hdr64_t ehdr64;
		to_host_hdr(eh64, &ehdr64, endian);

		if (ehdr64.ehsize != sizeof(Elf_hdr64_t))
		{
			print("ERROR:  elf header size (%u), expected %u.\n", ehdr64.ehsize, sizeof(Elf_hdr64_t));
			return 12;
		}

		// check sections headers

		unsigned shend = ehdr64.shoff + ehdr64.shnum * ehdr64.shentsize;
		if (sz < shend)
		{
			print("ERROR:  ELF is size too small (%u), expect >= %u.\n", sz, shend);
			return 13;
		}

		if (ehdr64.shentsize != sizeof(Elf_shdr64_t))
		{
			print("ERROR:  unexpected section header entry size (%u), expect %u.\n",
				ehdr64.shentsize, sizeof(Elf_shdr64_t));
			return 14;
		}

		if (ehdr64.shstrndx >= ehdr64.shnum)
		{
			print("ERROR:  wrong index of section with names.\n");
			return 15;
		}

		// check program headers

		if (ehdr64.phentsize != sizeof(Elf_phdr64_t))
		{
			print("ERROR:  unexpected program header entry size (%u), expect %u.\n",
				ehdr64.phentsize, sizeof(Elf_phdr64_t));
			return 16;
		}
	}

	return 0;
}

extern "C" int elf_entry(const void* elf, unsigned sz, unsigned* entry_point, Elf_dprint_t dprint)
{
	print("dump:  elf=0x%x, sz=0x%x.\n", elf, sz);

	*entry_point = 0;

	int rc = elf_check(elf, sz, dprint);
	if (rc)
	{
		print("ERROR:  elf is not valid, rc=%d.\n", rc);
		return 1;
	}

	Elf_ident_t* ident = (Elf_ident_t*) elf;
	int endian = ident->endian == Endian_big ? Big_endian : Little_endian;

	if (ident->format == Fmt_32bit)
	{
		// convert to host endianness
		Elf_hdr32_t* eh32 = (Elf_hdr32_t*)(void*) elf;
		Elf_hdr32_t ehdr32;
		to_host_hdr(eh32, &ehdr32, endian);
		*entry_point = ehdr32.entry;
	}
	else // 64-bit
	{
		// convert to host endianness
		Elf_hdr64_t* eh64 = (Elf_hdr64_t*)(void*) elf;
		Elf_hdr64_t ehdr64;
		to_host_hdr(eh64, &ehdr64, endian);
		*entry_point = ehdr64.entry;

		print("dump:  ERROR:  sorry ELF64 not supported yet.\n");
		return 2;
	}

	return 0;
}

extern "C" int elf_dump(const void* elf, unsigned sz, Elf_dprint_t dprint)
{
	print("dump:  elf=0x%x, sz=0x%x.\n", elf, sz);

	int rc = elf_check(elf, sz, dprint);
	if (rc)
	{
		print("ERROR:  elf is not valid, rc=%d.\n", rc);
		return 1;
	}

	// dump ident

	Elf_ident_t* ident = (Elf_ident_t*) elf;

	uint32_t* magic = (uint32_t*)(void*) ident->magic;
	(void) magic;
	print("dump:  magic:       0x%x\n", *magic);
	print("dump:  format:      %s\n",   format_str(ident->format));
	print("dump:  endian:      %s\n",   endian_str(ident->endian));
	print("dump:  ident ver:   0x%x\n", ident->ver);
	print("dump:  OS ABI:      0x%x\n", ident->osabi);
	print("dump:  ABI ver:     0x%x\n", ident->abiver);

	int endian = ident->endian == Endian_big ? Big_endian : Little_endian;

	// dump elf header

	if (ident->format == Fmt_32bit)
	{
		// convert to host endianness
		Elf_hdr32_t* eh32 = (Elf_hdr32_t*)(void*) elf;
		Elf_hdr32_t ehdr32;
		to_host_hdr(eh32, &ehdr32, endian);

		print("dump:  type:        %s\n",   type_str(ehdr32.type));
		print("dump:  machine:     %s\n",   machine_str(ehdr32.machine));
		print("dump:  version:     0x%x\n", ehdr32.ver);
		print("dump:  entry:       0x%x\n", ehdr32.entry);
		print("dump:  phof:        0x%x\n", ehdr32.phoff);
		print("dump:  shoff:       0x%x\n", ehdr32.shoff);
		print("dump:  flags:       0x%x\n", ehdr32.flags);
		print("dump:  hdr sz:      0x%x\n", ehdr32.ehsize);
		print("dump:  phentsize:   0x%x\n", ehdr32.phentsize);
		print("dump:  phnum:       0x%x\n", ehdr32.phnum);
		print("dump:  shentsize:   0x%x\n", ehdr32.shentsize);
		print("dump:  shnum:       0x%x\n", ehdr32.shnum);
		print("dump:  shstrndx:    0x%x\n", ehdr32.shstrndx);

		// dump sections headers

		Elf_shdr32_t* sh_str = (Elf_shdr32_t*)((size_t)elf + ehdr32.shoff + ehdr32.shstrndx * ehdr32.shentsize);
		const char* str_tb = (char*)((size_t)elf + to_host(sh_str->offset, endian));
		(void) str_tb;

		print("dump:  section headers(%u):\n", ehdr32.shnum);
		print("dump:  ##  type        addr        offset      size        flags        name\n");

		for (unsigned i=0; i<ehdr32.shnum; ++i)
		{
			Elf_shdr32_t* sh = (Elf_shdr32_t*)((size_t)elf + ehdr32.shoff + i * ehdr32.shentsize);
			Elf_shdr32_t shdr;
			to_host_shdr(sh, &shdr, endian);

			print("dump:  %2u  %-10s  0x%08x  0x%08x  0x%08x  0x%08x  %s\n",
				i,
				stype_str(shdr.type),
				shdr.addr,
				shdr.offset,
				shdr.size,
				shdr.flags,
				str_tb + shdr.name );
		}

		// dump program headers

		print("dump:  program headers(%u):\n", ehdr32.phnum);
		print("dump:  ##  type        offset      vaddr       paddr       filesz      memsz       flags  align\n");
		for (unsigned i=0; i<ehdr32.phnum; ++i)
		{
			Elf_phdr32_t* ph = (Elf_phdr32_t*)((size_t)elf + ehdr32.phoff + i * ehdr32.phentsize);
			Elf_phdr32_t phdr;
			to_host_phdr(ph, &phdr, endian);

			print("dump:  %2u  %-10s  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  %3s    0x%x\n",
				i,
				ptype_str(phdr.type), phdr.offset, phdr.vaddr, phdr.paddr,
				phdr.filesz, phdr.memsz, pflags_str(phdr.flags), phdr.align);
		}
	}
	else // 64-bit
	{
		// convert to host endianness
		Elf_hdr64_t* eh64 = (Elf_hdr64_t*)(void*) elf;
		Elf_hdr64_t ehdr64;
		to_host_hdr(eh64, &ehdr64, endian);

		print("dump:  type:        %s\n",   type_str(ehdr64.type));
		print("dump:  machine:     %s\n",   machine_str(ehdr64.machine));
		print("dump:  version:     0x%x\n", ehdr64.ver);
		print("dump:  entry:       0x%X\n", ehdr64.entry);
		print("dump:  phof:        0x%X\n", ehdr64.phoff);
		print("dump:  shoff:       0x%X\n", ehdr64.shoff);
		print("dump:  flags:       0x%x\n", ehdr64.flags);
		print("dump:  hdr sz:      0x%x\n", ehdr64.ehsize);
		print("dump:  phentsize:   0x%x\n", ehdr64.phentsize);
		print("dump:  phnum:       0x%x\n", ehdr64.phnum);
		print("dump:  shentsize:   0x%x\n", ehdr64.shentsize);
		print("dump:  shnum:       0x%x\n", ehdr64.shnum);
		print("dump:  shstrndx:    0x%x\n", ehdr64.shstrndx);

		print("dump:  ERROR:  sorry ELF64 not supported yet.\n");
		return 2;
	}

	return 0;
}

template <typename HDR_t, typename SHDR_t, typename PHDR_t>
static void foreach(const void* elf, Elf_shdr_func_t sh_func, Elf_phdr_func_t ph_func, size_t label,
                    uintptr_t* entry, int endian, Elf_dprint_t dprint)
{
		// convert to host endianness
		HDR_t* eh32 = (HDR_t*) elf;
		HDR_t ehdr32;
		to_host_hdr(eh32, &ehdr32, endian);

		if (entry)
			*entry = ehdr32.entry;

		// iterate sections headers
		if (sh_func)
		{
			SHDR_t* sh_str = (SHDR_t*)((size_t)elf + ehdr32.shoff + ehdr32.shstrndx * ehdr32.shentsize);
			const char* str_tb = (char*)((size_t)elf + to_host(sh_str->offset, endian));

			for (unsigned i=0; i<ehdr32.shnum; ++i)
			{
				SHDR_t* sh = (SHDR_t*)((size_t)elf + ehdr32.shoff + i * ehdr32.shentsize);
				SHDR_t shdr;
				to_host_shdr(sh, &shdr, endian);

				unsigned acc = sflags2acc(shdr.flags);

				sh_func(label, shdr.addr, shdr.size, acc, str_tb + shdr.name, shdr.type == Stype_progbits);
			}
		}

		// iterate program headers
		if (ph_func)
		{
			for (unsigned i=0; i<ehdr32.phnum; ++i)
			{
				PHDR_t* ph = (PHDR_t*)((size_t)elf + ehdr32.phoff + i * ehdr32.phentsize);
				PHDR_t phdr;
				to_host_phdr(ph, &phdr, endian);

				uintptr_t location = (size_t)elf + phdr.offset;

				ph_func(label, phdr.vaddr, phdr.paddr, phdr.memsz, location, phdr.type == Ptype_load);
			}
		}

}

extern "C" int elf_foreach(const void* elf, size_t sz, Elf_shdr_func_t sh_func,
                Elf_phdr_func_t ph_func, size_t label, uintptr_t* entry, Elf_dprint_t dprint)
{
	print("foreach:  elf=0x%x, sz=0x%x.\n", elf, sz);

	if (entry)
		*entry = 0;

	int rc = elf_check(elf, sz, dprint);
	if (rc)
	{
		print("foreach:  ERROR:  elf is not valid, rc=%d.\n", rc);
		return 1;
	}

	// get ident

	Elf_ident_t* ident = (Elf_ident_t*) elf;

	int endian = ident->endian == Endian_big ? Big_endian : Little_endian;

	// dump elf header

	if (ident->format == Fmt_32bit)
	{
		foreach<Elf_hdr32_t, Elf_shdr32_t, Elf_phdr32_t>(elf, sh_func, ph_func, label, entry, endian, dprint);
	}
	else // 64-bit
	{
		foreach<Elf_hdr64_t, Elf_shdr64_t, Elf_phdr64_t>(elf, sh_func, ph_func, label, entry, endian, dprint);
	}

	return 0;
}

