//##################################################################################################
//
//  Ram FS - content file's headers and bodies.
//
//##################################################################################################

#include "wrm_ramfs.h"
#include "sys_ramfs.h"
#include "l4_kip.h"
#include "l4_api.h"
#include <string.h>

struct Ramfs_t
{
	addr_t addr;
	size_t sz;
};
static Ramfs_t _ramfs;

static void find_ramfs()
{
	addr_t adr = 0;
	size_t sz = 0;
	L4_kip_t* kip = l4_kip();
	const Kip_mem_info_t* minfo = (Kip_mem_info_t*) &kip->memory_info;
	const Mem_desc_t* mdesc = (Mem_desc_t*)(void*)((char*)kip + minfo->mem_desc_ptr);
	for (unsigned i=0; i<minfo->number; ++i)
	{
		if (mdesc[i].type == Mem_desc_t::Bootloader  &&
			mdesc[i].type_exact == Mem_desc_t::Bldr_ramfs)
		{
			adr = mdesc[i].low << 10;
			sz = ((mdesc[i].heigh + 1) << 10) - adr;
			break;
		}
	}
	_ramfs.addr = adr;
	_ramfs.sz = sz;
}

extern "C" int wrm_ramfs_get_file(const char* filename, addr_t* addr, size_t* size)
{
	if (!_ramfs.sz)
		find_ramfs();

	if (!_ramfs.sz)
		return -1;  // no ramfs found

	Ramfs_file_header_t* file = (Ramfs_file_header_t*) _ramfs.addr;
	do
	{
		if (!strcmp(file->name, filename))
		{
			if (addr)
				*addr = (addr_t) file->data;
			if (size)
				*size = file->size;
			return 0;
		}
	} while ((file = ramfs_next(file)));
	return -2;  // file not found
}
