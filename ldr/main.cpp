//##################################################################################################
//
//  Bootloader - read RAMFS and load the kernel.
//
//##################################################################################################

#include "uart.h"
#include "ramfs.h"
#include "elfloader.h"
#include "ldr-config.h"
#include "list.h"
#include "sys-utils.h"
#include "panic.h"
#include "kip.h"
#include "libc_io.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define macro2str(s) str(s)
#define str(s) #s

inline unsigned b2kb(unsigned bytes) { return bytes / 1024; }
inline unsigned b2mb(unsigned bytes) { return bytes / 1024 / 1024; }

#ifdef Cfg_debug
#  define dprint printf
#else
#  define dprint(...)
#endif

// for panic()
extern "C" void break_execution(const char* str = 0)
{
	(void) str;
	while (1);
}

static void ldr_uart_putc(int c)
{
	uart_putc(Cfg_ldr_uart_paddr, c);
}

static void init_io()
{
	Libc_io_callbacks_t io;
	io.out_char    = ldr_uart_putc,
	io.out_string  = NULL;
	libc_init_io(&io); // init libc handlers
}

static unsigned dprint_elf(const char* fmt, ...)
{
	va_list args;
	char buf[256];
	int l = snprintf(buf, sizeof(buf), "[ldr]  elf:  ");
	va_start(args, fmt);
	vsnprintf(buf+l, sizeof(buf)-l, fmt, args);
	va_end(args);
	dprint("%s", buf);
	return 0;
}

/*
static unsigned dprint_list(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("[ldr]  lst:  ");
	vprintf(fmt, args);
	va_end(args);
	return 0;
}
*/

// memory region
class Region_t
{
public:
	addr_t start;
	size_t sz;
	const char* owner;
	explicit Region_t(addr_t a, size_t s, const char* o) : start(a), sz(s), owner(o) {}
	bool operator < (const Region_t& r) const { return start < r.start; }
	addr_t end() const { return start + sz; }
};

// all memory regions
typedef list_t <Region_t, 16> Regions_t;
Regions_t regions;

// elf section
class Section_t
{
public:
	addr_t addr;
	size_t sz;
	int type;
	explicit Section_t(addr_t a, size_t s, int t) : addr(a), sz(s), type(t) {}
};

// sections for sigma0 and roottask
typedef list_t <Section_t, 16> Sections_t;
Sections_t sections;

void get_ramfs(addr_t* addr, size_t* sz)
{
	// linker vars
	extern unsigned _ramfs_start;
	extern unsigned _ramfs_end;

	if (addr)
		*addr = (addr_t)&_ramfs_start;

	if (sz)
		*sz = (size_t)&_ramfs_end - (size_t)&_ramfs_start;
}

static void add_bootloader_to_regions()
{
	// linker vars
	extern int _bootloader_start;
	extern int _bootloader_end;

	// boundaries
	addr_t start = (addr_t)&_bootloader_start;
	size_t sz    = (size_t)&_bootloader_end - (size_t)&_bootloader_start;

	regions.insert_sort(Region_t(start, sz, "bootloader"));
}

static void add_free_mem_to_regions()
{
	if (Cfg_ram_start < regions.begin()->start)
		regions.insert_sort(Region_t(Cfg_ram_start, regions.begin()->start - Cfg_ram_start, "free"));

	const size_t ram_end = Cfg_ram_start + Cfg_ram_sz;
	if (regions.last()->end() < ram_end)
		regions.insert_sort(Region_t(regions.last()->end(), ram_end - regions.last()->end(), "free"));
}

// callback func for elf_foreach()
static void process_elf_shdr(size_t label, uintptr_t addr, size_t sz, unsigned access,
                             const char* name, int is_progbits)
{
	const char* app = (const char*)label;

	dprint("[ldr]    app=%s, addr=0x%08zx, sz=0x%08zx, acc=%u, progbits=%d, name=%s.\n",
		app, addr, sz, access, is_progbits, name);

	enum
	{
		Acc_r = 4,
		Acc_w = 2,
		Acc_x = 1,
		Acc_rw = Acc_r | Acc_w
	};

	if (is_progbits)
	{
		if (!is_pg_aligned(addr))
			panic("ldr:  region '%s' for app=%s has addr=0x%x not aligned to page size.\n", name, app, addr);

		if (!is_pg_aligned(sz) && strcmp(name, ".data"))
			panic("ldr:  region '%s' for app=%s has size=0x%x not aligned to page size.\n", name, app, sz);

		if (access & Acc_x)
		{
			if (!strcmp(app, "sigma0"))
				sections.push_back(Section_t(addr, sz, Mem_desc_t::Bldr_sigma0_text));
			else if (!strcmp(app, "roottask"))
				sections.push_back(Section_t(addr, sz, Mem_desc_t::Bldr_roottask_text));
		}
		else if (access == Acc_r)
		{
			if (!strcmp(app, "sigma0"))
				sections.push_back(Section_t(addr, sz, Mem_desc_t::Bldr_sigma0_rodata));
			else if (!strcmp(app, "roottask"))
				sections.push_back(Section_t(addr, sz, Mem_desc_t::Bldr_roottask_rodata));
		}
		else if (access == Acc_rw)
		{
			if (!strcmp(app, "sigma0"))
				sections.push_back(Section_t(addr, sz, Mem_desc_t::Bldr_sigma0_data));
			else if (!strcmp(app, "roottask"))
				sections.push_back(Section_t(addr, sz, Mem_desc_t::Bldr_roottask_data));
		}
	}
}

// callback func for elf_foreach()
static void process_elf_phdr(size_t label, uintptr_t vaddr, uintptr_t paddr, size_t sz,
                             uintptr_t location, int is_load)
{
	const char* app = (const char*)label;
	dprint("[ldr]    app=%s, va=0x%08zx, pa=0x%08zx, sz=0x%08zx, load=%d.\n", app, vaddr, paddr, sz, is_load);
	if (is_load)
		regions.insert_sort(Region_t(paddr, sz, app));
}

// callback func for elf_foreach()
static void load_elf_phdr(size_t label, uintptr_t vaddr, uintptr_t paddr, size_t sz,
                          uintptr_t location, int is_load)
{
	dprint("[ldr]    load:  loc=0x%08zx, pa=0x%08zx, sz=0x%08zx, load=%d.\n", location, paddr, sz, is_load);
	if (is_load)
		memcpy((void*)paddr, (void*)location, sz);
}

static void init_kip(uintptr_t sigma0_entry, uintptr_t roottask_entry)
{
	// search KIP at the start of kernel region by magic L4_kip_magic

	L4_kip_t* kip = 0;
	for (Regions_t::citer_t it=regions.cbegin(); it!=regions.cend(); ++it)
		if (!strcmp(it->owner, "kernel"))
			if ((*(uint32_t*)it->start) == L4_kip_magic  &&  it->sz >= Cfg_page_sz)
				kip = (L4_kip_t*)it->start;

	if (!kip)
		panic("ldr:  Could not find KIP.\n");

	printf("[ldr]  KIP found at 0x%zx.\n", (addr_t)kip);

	// set apps entry points
	kip->sigma0_ip   = sigma0_entry;
	kip->roottask_ip = roottask_entry;

	// add memory descriptors
	Kip_mem_info_t* minfo = (Kip_mem_info_t*) &kip->memory_info;
	minfo->mem_desc_ptr = sizeof(L4_kip_t);  // offset from KIP base address
	minfo->number = 0;
	Mem_desc_t* mdesc = (Mem_desc_t*)((char*)kip + minfo->mem_desc_ptr);

	// add conventional memory desriptors - all RAM but regions of kernel, sigma0, roottask
	for (Regions_t::citer_t it=regions.cbegin(); it!=regions.cend(); ++it)
	{
		// note:  bootloader's memory mark as free
		if (!strcmp(it->owner, "bootloader") || !strcmp(it->owner, "free"))
		{
			mdesc->low   = it->start >> 10;
			mdesc->heigh = (it->end() - 1) >> 10;
			mdesc->virt  = 0;
			mdesc->type  = Mem_desc_t::Conventional;
			mdesc->type_exact = 0;
			mdesc++;
			minfo->number++;
		}
	}

	// add bootloader memory descriptors - sections of sigma0 and roottask
	for (Sections_t::citer_t it=sections.cbegin(); it!=sections.cend(); ++it)
	{
		// TODO:  check alignment addr and sz to pg_size

		mdesc->low   = it->addr >> 10;
		mdesc->heigh = (it->addr + it->sz - 1) >> 10;
		mdesc->virt  = 0;
		mdesc->type  = Mem_desc_t::Bootloader;
		mdesc->type_exact = it->type;
		mdesc++;
		minfo->number++;
	}
}

// C entry point
extern "C" int main()
{
	init_bss();
	uart_init(Cfg_ldr_uart_paddr, Cfg_ldr_uart_bitrate, Cfg_sys_clock_hz);
	init_io();
	call_ctors();

	// print banner
	printf("\n\r");
	printf("[ldr]   _    _ ___ __  __   ___  ___ \n");
	printf("[ldr]  | |  | | _ \\  \\/  | / _ \\/ __|\n");
	printf("[ldr]  | |/\\| |   / |\\/| || (_) \\__ \\\n");
	printf("[ldr]  |__/\\__|_|_\\_|  |_(_)___/|___/\n");
	printf("[ldr]          From Russia with love!\n");
	printf("[ldr]\n");

	// print system info
	printf("[ldr]  hello:  %s  %s.\n", __TIME__, __DATE__);
	printf("[ldr]  hware:  %s, %s, %s, %s.\n", macro2str(Cfg_arch), macro2str(Cfg_cpu), macro2str(Cfg_plat), macro2str(Cfg_brd));
	printf("[ldr]  ram:    [0x%08x - 0x%08x)  %6u MB.\n", Cfg_ram_start, Cfg_ram_start + Cfg_ram_sz, b2mb(Cfg_ram_sz));

	addr_t ramfs_addr;
	size_t ramfs_sz;
	get_ramfs(&ramfs_addr, &ramfs_sz);
	printf("[ldr]  ramfs:  [0x%08zx - 0x%08zx)  %6u KB.\n", ramfs_addr, ramfs_addr + ramfs_sz, b2kb(ramfs_sz));
	sections.push_back(Section_t((addr_t)ramfs_addr, ramfs_sz, Mem_desc_t::Bldr_ramfs));

	// search files - kernel, sigma0, roottask

	const Ramfs_file_header_t* kernel_file = 0;
	const Ramfs_file_header_t* sigma0_file = 0;
	const Ramfs_file_header_t* roottask_file = 0;
	Ramfs_file_header_t* file = (Ramfs_file_header_t*) ramfs_addr;
	unsigned cnt = 1;
	printf("[ldr]  ##  name                    data      size  content\n");
	do
	{
		if (!strcmp(file->name, "kernel.elf"))
			kernel_file = file;
		else
		if (!strcmp(file->name, "sigma0.elf"))
			sigma0_file = file;
		else
		if (!strcmp(file->name, "roottask.elf"))
			roottask_file = file;

		printf("[ldr]  %2u  %-16s  0x%08zx  %8u  '%.8s ...'\n",
			cnt, file->name, (uintptr_t)file->data, file->size, file->data);

		assert((uintptr_t)file->data >= ramfs_addr  &&
			((uintptr_t)file->data + file->size) < (ramfs_addr + ramfs_sz));

		cnt++;
	} while ((file = ramfs_next(file)));
	printf("[ldr]\n");

	// check search result

	if (!kernel_file || !sigma0_file || !roottask_file)
	{
		const char* exist = "exists";
		const char* absent = "is absent (!)";
		printf("[ldr]  ERROR:  could not find loadable files:  kernel %s, sigma0 %s, roottask %s.\n",
			kernel_file ? exist : absent, sigma0_file ? exist : absent, roottask_file ? exist : absent);
		while(1);
	}

	// debug dump
	/*
	elf_dump(kernel_file->data, kernel_file->size, dprint_elf);
	printf("\n");
	elf_dump(sigma0_file->data, sigma0_file->size, dprint_elf);
	printf("\n");
	elf_dump(roottask_file->data, roottask_file->size, dprint_elf);
	printf("\n");
	*/

	// get section information for sigma0 and roottask (section headers)
	// accumulate all memory regions to 'regions' (program headers)

	int rc = elf_foreach(kernel_file->data, kernel_file->size, Elf_no_shdr_func,
	                     process_elf_phdr, (addr_t)"kernel", 0, dprint_elf);
	if (rc)
		panic("[ldr]  wrong kernel.elf, rc=%d.\n", rc);

	rc = elf_foreach(sigma0_file->data, sigma0_file->size, process_elf_shdr,
	                 process_elf_phdr, (addr_t)"sigma0", 0, dprint_elf);
	if (rc)
		panic("[ldr]  wrong sigma0.elf, rc=%d\n", rc);

	rc = elf_foreach(roottask_file->data, roottask_file->size, process_elf_shdr,
	                 process_elf_phdr, (addr_t)"roottask", 0, dprint_elf);
	if (rc)
		panic("[ldr]  wrong roottask.elf, rc=%d.\n", rc);

	add_bootloader_to_regions();
	add_free_mem_to_regions();

	// check overlaps

	dprint("[ldr]  memory regions:\n");
	for (Regions_t::citer_t it=regions.cbegin(); it!=regions.cend(); ++it)
	{
		dprint("[ldr]    [%zx - %zx)  sz=0x%08zx,  %s.\n", it->start, it->start + it->sz, it->sz, it->owner);
	}

	for (Regions_t::citer_t it=regions.cbegin(); (it+1)!=regions.cend(); ++it)
	{
		if (it->end() > (it+1)->start)
		{
			panic("ldr:  %s [%x-%x) overlaps with %s [%x - %x).\n",
				it->owner, it->start,  it->start + it->sz,
				(it+1)->owner, (it+1)->start,  (it+1)->start + (it+1)->sz);
		}
	}

	// load ELFs to memory

	uintptr_t kernel_entry = 0;
	uintptr_t sigma0_entry = 0;
	uintptr_t roottask_entry = 0;

	rc = elf_foreach(kernel_file->data, kernel_file->size, 0, load_elf_phdr, 0, &kernel_entry, dprint_elf);
	if (rc)
		panic("[ldr]  elf_load(kernel.elf) - rc=%d.\n", rc);

	rc = elf_foreach(sigma0_file->data, kernel_file->size, 0, load_elf_phdr, 0, &sigma0_entry, dprint_elf);
	if (rc)
		printf("[ldr]  elf_load(sigma0.elf) - rc=%d.\n", rc);

	rc = elf_foreach(roottask_file->data, kernel_file->size, 0, load_elf_phdr, 0, &roottask_entry, dprint_elf);
	if (rc)
		printf("[ldr]  elf_load(roottask.elf) - rc=%d.\n", rc);

	init_kip(sigma0_entry, roottask_entry);

	printf("[ldr]  Go to kernel.\n\n");

	typedef void(*func_t)(void);
	((func_t)kernel_entry)();

	return 0;
}
