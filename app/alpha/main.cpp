//##################################################################################################
//
//  alpha - root task.
//
//##################################################################################################

#include "l4_api.h"
#include "l4_thrid.h"
#include "wrmos.h"
#include "list.h"
#include "elfloader.h"
#include "sys_ramfs.h"
#include "sys_utils.h"
#include "sys_eframe.h"
#include "wlibc_cb.h"
#include "wlibc_panic.h"
#include "krn-config.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

//--------------------------------------------------------------------------------------------------
// Cfg
//--------------------------------------------------------------------------------------------------
class Io_device_t
{
public:

	enum { Name_sz = Wrm_app_cfg_t::Dev_name_sz };

private:

	char     _name[Name_sz];
	paddr_t  _pa;  // may include iomem_addr (0xfffff000) and ioport_addr (0xfff)
	psize_t  _sz;  // may include iomem_sz   (0xfffffff0) and ioport_sz   (0xf)
	unsigned _irq;

public:


	static const Io_device_t* next(const Io_device_t* cur)
	{
		const Io_device_t* next = cur + 1;
		return next->_name[0] != 0  ?  next  :  0;
	}

	void name(const char* s, unsigned l) { strncpy(_name, s, l); }
	void pa(paddr_t v)       { _pa = v; }
	void sz(psize_t v)       { _sz = v; }
	void irq(unsigned v)     { _irq = v; }

	const char* name()     const { return _name; }
	paddr_t  pa()          const { return _pa; }
	psize_t  sz()          const { return _sz; }
	paddr_t  iomem_addr()  const { return round_down(_pa, 0x1000); }
	psize_t  iomem_sz()    const { return round_down(_sz, 0x10); }
	paddr_t  iomem_end()   const { return iomem_addr() + iomem_sz(); }
	unsigned ioport_addr() const { return get_offset(_pa, 0x1000); }
	unsigned ioport_sz()   const { return get_offset(_sz, 0x10); }
	unsigned ioport_end()  const { return ioport_addr() + ioport_sz(); }
	unsigned irq()         const { return _irq; }
};

class Board_cfg_t
{
public:
	enum { Devs_max = Wrm_app_cfg_t::Dev_list_sz };
	Io_device_t devices [Devs_max];

	const Io_device_t* devices_begin() const { return devices; }
};

class Memory_region_t
{
	enum { Name_sz = Wrm_app_cfg_t::Mem_name_sz };
public:
	char     name [Name_sz];
	size_t   sz;
	unsigned access;
	bool     cached;
	bool     contig;

	static const Memory_region_t* next(const Memory_region_t* cur)
	{
		const Memory_region_t* next = cur + 1;
		return next->name[0]!=0 ? next : 0;
	}
};

class Proj_cfg_t
{
public:
	enum
	{
		Apps_max = 8,
		Mems_max = 8
	};

	Board_cfg_t     brd;
	Memory_region_t mems [Mems_max];
	Wrm_app_cfg_t   apps [Apps_max];

	const Wrm_app_cfg_t*   apps_begin() const { return apps; }
	const Memory_region_t* mems_begin() const { return mems; }
};

static Proj_cfg_t parsed_proj_cfg;

const bool parse_dev_config(const char* str, Io_device_t* devs, unsigned list_sz)
{
	bool inside_section = false;
	const char* line = str;
	unsigned line_cnt = 1;
	unsigned cnt = 0;

	// parse config file line by line
	while (line)
	{
		if (line[0] != '#'  &&  strncmp(line, "\t#", 2)) // skip
		{
			if (!inside_section)
			{
				if (!strncmp(line, "DEVICES\n", 8))
					inside_section = true;
			}
			else // inside dev section
			{
				if (*line != '\t') // sections lines should start from tab
					return true;

				// parse line with device config

				enum { Words = 4 };  // words in line
				const char* word_ptr[Words];
				unsigned word_len[Words];
				const char* start = line + 1;
				for (unsigned i=0; i<Words; ++i)
				{
					const char* end = strpbrk(start, " \t\n");
					if (!end  ||  end == start)
					{
						wrm_loge("wrong dev cfg (%u):  too little params in line, expected %d.\n",
							line_cnt, Words);
						return false;
					}
					word_ptr[i] = start;
					word_len[i] = end - start;
					start = end + strspn(end, " \t"); // skip space
				}

				// put params to config

				if (cnt == list_sz)
				{
					wrm_loge("wrong dev cfg (%u):  too many devices, max=%u.\n", line_cnt, list_sz);
					return false;
				}

				if (word_len[0] >= Io_device_t::Name_sz)
				{
					wrm_loge("wrong dev cfg (%u):  too big device name, max=%u.\n",
						line_cnt, Io_device_t::Name_sz - 1);
					return false;
				}

				devs[cnt].name(word_ptr[0], word_len[0]);
				devs[cnt].pa( !strncmp(word_ptr[1], "krn", 3) ? Cfg_krn_uart_paddr : strtoul(word_ptr[1], 0, 16));
				devs[cnt].sz( !strncmp(word_ptr[2], "krn", 3) ? Cfg_krn_uart_sz    : strtoul(word_ptr[2], 0, 16));
				devs[cnt].irq(!strncmp(word_ptr[3], "krn", 3) ? Cfg_krn_uart_irq   : strtoul(word_ptr[3], 0, 10));
				cnt++;
			}
		}

		line = strchr(line, '\n');
		if (line)
			line++; // skip '\n'
		line_cnt++;
	}

	return true;
}

int acc_str_to_enum(const char* str, unsigned len)
{
	if (!strncmp(str, "r", len))
		return Acc_r;
	if (!strncmp(str, "w", len))
		return Acc_w;
	if (!strncmp(str, "rw", len))
		return Acc_rw;
	return -1;
}

const bool parse_mem_config(const char* str, Memory_region_t* mems, unsigned list_sz)
{
	bool inside_section = false;
	const char* line = str;
	unsigned line_cnt = 1;
	unsigned cnt = 0;

	// parse config file line by line
	while (line)
	{
		if (line[0] != '#'  &&  strncmp(line, "\t#", 2)) // skip
		{
			if (!inside_section)
			{
				if (!strncmp(line, "MEMORY\n", 7))
					inside_section = true;
			}
			else // inside mem section
			{
				if (*line != '\t') // sections lines should start from tab
					return true;

				// parse line with memory config

				enum { Words = 5 };  // words in line
				const char* word_ptr[Words];
				unsigned word_len[Words];
				const char* start = line + 1;
				for (unsigned i=0; i<Words; ++i)
				{
					const char* end = strpbrk(start, " \t\n");
					if (!end  ||  end == start)
					{
						wrm_loge("wrong mem cfg (%u):  too little params in line, expected %d.\n",
							line_cnt, Words);
						return false;
					}
					word_ptr[i] = start;
					word_len[i] = end - start;
					start = end + strspn(end, " \t"); // skip space
				}

				// put params to config

				if (cnt == list_sz)
				{
					wrm_loge("wrong mem cfg (%u):  too many memory regions, max=%u.\n",
						line_cnt, list_sz);
					return false;
				}

				if (word_len[0] >= sizeof(mems[cnt].name))
				{
					wrm_loge("wrong mem cfg (%u):  too big memory name, max=%zu.\n",
						line_cnt, sizeof(mems[cnt].name) - 1);
					return false;
				}

				strncpy(mems[cnt].name, word_ptr[0], word_len[0]);
				mems[cnt].sz     = strtoul(word_ptr[1], 0, 16);
				mems[cnt].access = acc_str_to_enum(word_ptr[2], word_len[2]);
				mems[cnt].cached = strtoul(word_ptr[3], 0, 10);
				mems[cnt].contig = strtoul(word_ptr[4], 0, 10);

				if (mems[cnt].access == (unsigned)-1)
				{
					wrm_loge("wrong mem cfg (%u):  bad access field='%.*s', allows r/w/rw.\n",
						line_cnt, word_len[2], word_ptr[2]);
					return false;
				}

				cnt++;
			}
		}

		line = strchr(line, '\n');
		if (line)
			line++; // skip '\n'
		line_cnt++;
	}

	return true;
}

const bool parse_app_config(const char* str, Wrm_app_cfg_t* apps, unsigned list_sz)
{
	bool inside_section = false;
	bool inside_app = false;
	const char* line = str;
	unsigned line_cnt = 1;
	unsigned app_cnt = 0;

	// parse config file line by line
	while (line)
	{
		if (line[0] != '#'  &&  strncmp(line, "\t#", 2)) // skip comments
		{
			if (!inside_section)
			{
				if (!strncmp(line, "APPLICATIONS\n", 13))
					inside_section = true;
			}
			else // inside app section
			{
				if (*line != '\t') // sections lines should start from tab
					return true;

				// parse app section line by line

				//	{
				//		name:             greth
				//		short_name:       eth
				//		file_path:        ramfs:/greth
				//		stack_size:       0x1000
				//		heap_size:        0x1000
				//		aspace_max:       1
				//		threads_max:      3
				//		prio_max:         150
				//		fpu:              1
				//		malloc_strategy:  on_startup | on_pagefault
				//		devices:          greth
				//		memory:           greth_mem
				//		args:             arg1 arg2
				//	}

				if (!inside_app)
				{
					// section begin
					if (!strncmp(line, "\t{\n", 3))
						inside_app = true;
				}
				else // inside app
				{
					// section end
					if (!strncmp(line, "\t}\n", 3))
					{
						inside_app = false;
						app_cnt++;
					}
					else
					{
						// find param value:
						// \t\tparam_name:  param_value\n
						const char* name_start = line + strspn(line, " \t"); // skip space
						const char* space = strpbrk(name_start, " \t\n");
						const char* val_start = space + strspn(space, " \t"); // skip space
						const char* val_end = strpbrk(val_start, "\n");
						unsigned val_len = val_end - val_start;

						if (!strncmp(line, "\t\tname:", 7))
						{
							if (!val_len  ||  val_len >= sizeof(apps[app_cnt].name))
							{
								wrm_loge("wrong app cfg (%u):  'name' absent or too big, max=%zu.\n",
									line_cnt, sizeof(apps[app_cnt].name) - 1);
								return false;
							}
							strncpy(apps[app_cnt].name, val_start, val_len);
						}
						else if (!strncmp(line, "\t\tshort_name:", 13))
						{
							if (!val_len  ||  val_len >= sizeof(apps[app_cnt].short_name))
							{
								wrm_loge("wrong app cfg (%u):  'short_name' absent or too big, max=%zu.\n",
									line_cnt, sizeof(apps[app_cnt].short_name) - 1);
								return false;
							}
							strncpy(apps[app_cnt].short_name, val_start, val_len);
						}
						else if (!strncmp(line, "\t\tfile_path:", 11))
						{
							if (!val_len  ||  val_len >= sizeof(apps[app_cnt].filename))
							{
								wrm_loge("wrong app cfg (%u):  'file_path' absent or too big, max=%zu.\n",
									line_cnt, sizeof(apps[app_cnt].filename) - 1);
								return false;
							}
							strncpy(apps[app_cnt].filename, val_start, val_len);
						}
						else if (!strncmp(line, "\t\tstack_size:", 13))
						{
							if (!val_len)
							{
								wrm_loge("wrong app cfg (%u):  'stack_size' absent.\n", line_cnt);
								return false;
							}
							apps[app_cnt].stack_sz = strtoul(val_start, 0, 16);
						}
						else if (!strncmp(line, "\t\theap_size:", 12))
						{
							if (!val_len)
							{
								wrm_loge("wrong app cfg (%u):  'heap_size' absent.\n", line_cnt);
								return false;
							}
							apps[app_cnt].heap_sz = strtoul(val_start, 0, 16);
						}
						else if (!strncmp(line, "\t\taspaces_max:", 14))
						{
							if (!val_len)
							{
								wrm_loge("wrong app cfg (%u):  'aspace_max' absent.\n", line_cnt);
								return false;
							}
							apps[app_cnt].max_aspaces = strtoul(val_start, 0, 10);
							if (!apps[app_cnt].max_aspaces || apps[app_cnt].max_aspaces > 16)
							{
								wrm_loge("wrong app cfg (%u):  'aspaces_max' 0 or too big, max=%d.\n",
									line_cnt, 16);
								return false;
							}
						}
						else if (!strncmp(line, "\t\tthreads_max:", 14))
						{
							if (!val_len)
							{
								wrm_loge("wrong app cfg (%u):  'threads_max' absent.\n", line_cnt);
								return false;
							}
							apps[app_cnt].max_threads = strtoul(val_start, 0, 10);
							if (!apps[app_cnt].max_threads || apps[app_cnt].max_threads > 32)
							{
								wrm_loge("wrong app cfg (%u):  'threads_max' 0 or too big, max=%d.\n",
									line_cnt, 32);
								return false;
							}
						}
						else if (!strncmp(line, "\t\tprio_max:", 11))
						{
							if (!val_len)
							{
								wrm_loge("wrong app cfg (%u):  'prio_max' absent.\n", line_cnt);
								return false;
							}
							apps[app_cnt].max_prio = strtoul(val_start, 0, 10);
							if (!apps[app_cnt].max_prio || apps[app_cnt].max_prio > 0xff)
							{
								wrm_loge("wrong app cfg (%u):  'prio_max' 0 or too big, max=%d.\n",
									line_cnt, 0xff);
								return false;
							}
						}
						else if (!strncmp(line, "\t\tfpu:", 6))
						{
							if (!val_len)
							{
								wrm_loge("wrong app cfg (%u):  'fpu' absent.\n", line_cnt);
								return false;
							}
							if (!strncmp(val_start, "on", val_len))
								apps[app_cnt].fpu = 1;
							else if (!strncmp(val_start, "off", val_len))
								apps[app_cnt].fpu = 0;
							else
							{
								wrm_loge("wrong app cfg (%u):  'fpu' must be 'on' or 'off'.\n", line_cnt);
								return false;
							}
						}
						else if (!strncmp(line, "\t\tmalloc_strategy:", 15))
						{
							if (!val_len)
							{
								wrm_loge("wrong app cfg (%u):  'malloc_strategy' absent.\n", line_cnt);
								return false;
							}
							if (!strncmp(val_start, "on_startup", val_len))
								apps[app_cnt].malloc_strategy = Wrm_app_cfg_t::Malloc_on_startup;
							else if (!strncmp(val_start, "on_pagefault", val_len))
								apps[app_cnt].malloc_strategy = Wrm_app_cfg_t::Malloc_on_pagefault;
							else
							{
								wrm_loge("wrong app cfg (%u):  'malloc_strategy' must be 'on_startup' "
									"or 'on_pagefault'.\n", line_cnt);
								return false;
							}
						}
						else if (!strncmp(line, "\t\tdevices:", 10))
						{
							// parse list word by word
							unsigned cnt = 0;
							const char* word_start = val_start;
							while (1)
							{
								const char* word_end = strpbrk(word_start, ", \t\n");
								unsigned word_len = word_end - word_start;
								if (!word_len)
									break;

								if (cnt > Wrm_app_cfg_t::Dev_list_sz)
								{
									wrm_loge("wrong app cfg (%u):  too many devices, max=%d.\n",
										line_cnt, Wrm_app_cfg_t::Dev_list_sz);
									return false;
								}

								if (word_len >= Wrm_app_cfg_t::Dev_name_sz)
								{
									wrm_loge("wrong app cfg (%u):  too big device name, max=%d.\n",
										line_cnt, Wrm_app_cfg_t::Dev_name_sz - 1);
									return false;
								}

								strncpy(apps[app_cnt].devs[cnt], word_start, word_len);

								if (*word_end == '\n')
									break;

								word_start = word_end + strspn(word_end, ", \t\n"); // skip space
								cnt++;
							}
						}
						else if (!strncmp(line, "\t\tmemory:", 9))
						{
							// parse list word by word
							unsigned cnt = 0;
							const char* word_start = val_start;
							while (1)
							{
								const char* word_end = strpbrk(word_start, ", \t\n");
								unsigned word_len = word_end - word_start;
								if (!word_len)
									break;

								if (cnt > Wrm_app_cfg_t::Mem_list_sz)
								{
									wrm_loge("wrong app cfg (%u):  too many memory regions, max=%d.\n",
										line_cnt, Wrm_app_cfg_t::Mem_list_sz);
									return false;
								}

								if (word_len >= Wrm_app_cfg_t::Mem_name_sz)
								{
									wrm_loge("wrong app cfg (%u):  too big memory name, max=%d.\n",
										line_cnt, Wrm_app_cfg_t::Mem_name_sz - 1);
									return false;
								}

								strncpy(apps[app_cnt].mems[cnt], word_start, word_len);

								if (*word_end == '\n')
									break;

								word_start = word_end + strspn(word_end, ", \t\n"); // skip space
								cnt++;
							}
						}
						else if (!strncmp(line, "\t\targs:", 7))
						{
							// parse list word by word
							unsigned cnt = 0;
							const char* word_start = val_start;
							while (1)
							{
								const char* word_end = strpbrk(word_start, ", \t\n");
								unsigned word_len = word_end - word_start;
								if (!word_len)
									break;

								if (cnt > Wrm_app_cfg_t::Arg_list_sz)
								{
									wrm_loge("wrong app cfg (%u):  too many args, max=%d.\n",
										line_cnt, Wrm_app_cfg_t::Arg_list_sz);
									return false;
								}

								if (word_len >= Wrm_app_cfg_t::Arg_sz)
								{
									wrm_loge("wrong app cfg (%u):  too big argument, max=%d.\n",
										line_cnt, Wrm_app_cfg_t::Arg_sz - 1);
									return false;
								}

								strncpy(apps[app_cnt].args[cnt], word_start, word_len);

								if (*word_end == '\n')
									break;

								word_start = word_end + strspn(word_end, ", \t\n"); // skip space
								cnt++;
							}
						}
						else
						{
							wrm_loge("wrong app cfg (%u):  unknown param name.\n", line_cnt);
							return false;
						}
					}
				}
			}
		}

		line = strchr(line, '\n');
		if (line)
			line++; // skip '\n'
		line_cnt++;
	}

	return true;
}

const Proj_cfg_t* parse_config(const char* cfg_file)
{
	bool ok = parse_dev_config(cfg_file, parsed_proj_cfg.brd.devices, Board_cfg_t::Devs_max);
	if (!ok)
		return 0;

	ok = parse_mem_config(cfg_file, parsed_proj_cfg.mems, Proj_cfg_t::Mems_max);
	if (!ok)
		return 0;

	ok = parse_app_config(cfg_file, parsed_proj_cfg.apps, Proj_cfg_t::Apps_max);
	if (!ok)
		return 0;

	return &parsed_proj_cfg;
}

const Proj_cfg_t* app_config()
{
	return &parsed_proj_cfg;
}

int get_dev_index(const Proj_cfg_t* proj_cfg, const char* name)
{
	int cnt = 0;
	const Io_device_t* dev = proj_cfg->brd.devices_begin();
	while (dev)
	{
		if (!strcmp(dev->name(), name))
			return cnt;
		dev = Io_device_t::next(dev);
		cnt++;
	}
	return -1;
}

int get_mem_index(const Proj_cfg_t* proj_cfg, const char* name)
{
	int cnt = 0;
	const Memory_region_t* mem = proj_cfg->mems_begin();
	while (mem)
	{
		if (!strcmp(mem->name, name))
			return cnt;
		mem = Memory_region_t::next(mem);
		cnt++;
	}
	return -1;
}

void print_proj_config(const Proj_cfg_t* proj_cfg)
{
	wrm_logi("Project config:\n");
	wrm_logi("  Board config:\n");
	wrm_logi("    ##  device    paddr        size        irq\n");
	const Io_device_t* dev = proj_cfg->brd.devices_begin();
	unsigned cnt = 0;
	while (dev && *dev->name())
	{
		wrm_logi("    %2u  %-8s  0x%08llx   0x%08llx  %3d\n",
			cnt, dev->name(), (long long)dev->pa(), (long long)dev->sz(), dev->irq());
		dev = Io_device_t::next(dev);
		cnt++;
	}
	wrm_logi("  Memory config:\n");
	wrm_logi("    ##  name       size        cached  contig\n");
	const Memory_region_t* mem = proj_cfg->mems_begin();
	cnt = 0;
	while (mem && *mem->name)
	{
		wrm_logi("    %2u  %8s  0x%08zx       %d       %d\n",
			cnt,  mem->name, mem->sz, mem->cached, mem->contig);
		mem = Memory_region_t::next(mem);
		cnt++;
	}

	#if 1 // new text format, not table

	wrm_logi("  Apps config:\n");

	char devs_str[32];
	char mems_str[32];
	char args_str[32];

	const Wrm_app_cfg_t* app = proj_cfg->apps_begin();
	cnt = 0;
	while (app && *app->name)
	{
		// make device list
		const Wrm_app_cfg_t::dev_name_t* dev = app->devs;
		devs_str[0] = '\0';
		while ((*dev)[0])
		{
			unsigned len = strlen(devs_str);
			snprintf(devs_str+len, sizeof(devs_str)-len, "%d", get_dev_index(proj_cfg, *dev));
			dev++;
			if ((*dev)[0])
			{
				unsigned len = strlen(devs_str);
				snprintf(devs_str+len, sizeof(devs_str)-len, ",");
			}
		}
		// make device list
		const Wrm_app_cfg_t::mem_name_t* mem = app->mems;
		mems_str[0] = '\0';
		while ((*mem)[0])
		{
			unsigned len = strlen(mems_str);
			snprintf(mems_str+len, sizeof(mems_str)-len, "%d", get_mem_index(proj_cfg, *mem));
			mem++;
			if ((*mem)[0])
			{
				unsigned len = strlen(mems_str);
				snprintf(mems_str+len, sizeof(mems_str)-len, ",");
			}
		}
		// make arg list
		const Wrm_app_cfg_t::arg_t* arg = app->args;
		args_str[0] = '\0';
		while ((*arg)[0])
		{
			unsigned len = strlen(args_str);
			snprintf(args_str+len, sizeof(args_str)-len, "%s", *arg);
			arg++;
			if ((*arg)[0])
			{
				unsigned len = strlen(args_str);
				snprintf(args_str+len, sizeof(args_str)-len, ", ");
			}
		}

		wrm_logi("    [%u]\n", cnt);
		wrm_logi("      name:             %s\n", app->name);
		wrm_logi("      short_name:       %s\n", app->short_name);
		wrm_logi("      file:             %s\n", app->filename);
		wrm_logi("      stack_sz:         0x%x\n", app->stack_sz);
		wrm_logi("      heap_sz:          0x%zx\n", app->heap_sz);
		wrm_logi("      max_aspaces:      %u\n", app->max_aspaces);
		wrm_logi("      max_threads:      %u\n", app->max_threads);
		wrm_logi("      max_prio:         %u\n", app->max_prio);
		wrm_logi("      fpu:              %u\n", app->fpu);
		wrm_logi("      malloc_strategy:  %s\n", app->malloc_strategy_str());
		wrm_logi("      devices:          %s\n", devs_str);
		wrm_logi("      memory:           %s\n", mems_str);
		wrm_logi("      args:             %s\n", args_str);

		cnt++;
		app = Wrm_app_cfg_t::next(app);
	}

	#else  // table format

	wrm_logi("  Apps config:\n");
	wrm_logi("    ##  name      sname  file          thrs  prio   devs              mems         args\n");

	char devs_str[32];
	char mems_str[32];
	char args_str[32];

	const Wrm_app_cfg_t* app = proj_cfg->apps_begin();
	cnt = 0;
	while (app && *app->name)
	{
		// make device list
		const Wrm_app_cfg_t::dev_name_t* dev = app->devs;
		devs_str[0] = '\0';
		while ((*dev)[0])
		{
			unsigned len = strlen(devs_str);
			snprintf(devs_str+len, sizeof(devs_str)-len, "%d", get_dev_index(proj_cfg, *dev));
			dev++;
			if ((*dev)[0])
			{
				unsigned len = strlen(devs_str);
				snprintf(devs_str+len, sizeof(devs_str)-len, ",");
			}
		}
		// make device list
		const Wrm_app_cfg_t::mem_name_t* mem = app->mems;
		mems_str[0] = '\0';
		while ((*mem)[0])
		{
			unsigned len = strlen(mems_str);
			snprintf(mems_str+len, sizeof(mems_str)-len, "%d", get_mem_index(proj_cfg, *mem));
			mem++;
			if ((*mem)[0])
			{
				unsigned len = strlen(mems_str);
				snprintf(mems_str+len, sizeof(mems_str)-len, ",");
			}
		}
		// make arg list
		const Wrm_app_cfg_t::arg_t* arg = app->args;
		args_str[0] = '\0';
		while ((*arg)[0])
		{
			unsigned len = strlen(args_str);
			snprintf(args_str+len, sizeof(args_str)-len, "%s", *arg);
			arg++;
			if ((*arg)[0])
			{
				unsigned len = strlen(args_str);
				snprintf(args_str+len, sizeof(args_str)-len, ",");
			}
		}

		wrm_logi("    %2u  %-8s  %-4s   %-12.12s   %3u   %3u   %-16s  %-8s     %-s\n",
			cnt, app->name, app->short_name, app->filename, app->max_threads, app->max_prio,
			devs_str, mems_str, args_str);

		cnt++;
		app = Wrm_app_cfg_t::next(app);
	}
	#endif // 0
}
//--------------------------------------------------------------------------------------------------
// ~ Cfg
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//  Named memory regions
//--------------------------------------------------------------------------------------------------
class Named_memory_regions_t
{
public:

	struct Named_region_t
	{
		typedef list_t <L4_fpage_t, 8> locations_t;

		Memory_region_t region;
		locations_t     locations;
	};

	typedef list_t <Named_region_t, 8> regions_t;
	regions_t _regions;

public:

	void dump() const
	{
		wrm_logw("Dump named memory regions (%zu):\n", _regions.size());
		for (regions_t::citer_t it=_regions.begin(); it!=_regions.end(); ++it)
		{
			wrm_logw("  %8s:  sz=0x%zx, cached=%u, contig=%u, locations(%zu):\n",
				it->region.name, it->region.sz, it->region.cached, it->region.contig, it->locations.size());

			Named_region_t::locations_t::citer_t it2 = it->locations.begin();
			for ( ; it2!=it->locations.end(); ++it2)
				wrm_logw("    addr=0x%lx, sz=0x%lx.\n", it2->addr(), it2->size());
		}
	}

	// TODO:  support segmented location
	bool add(const Memory_region_t* mreg, L4_fpage_t loc)
	{
		if (!location(mreg->name).is_nil())
			return false;  // already exist

		Named_region_t reg;
		reg.region = *mreg;
		reg.locations.push_back(loc);
		_regions.push_back(reg);
		return true;
	}

	// TODO:  support segmented location
	L4_fpage_t location(const char* name, unsigned* cached = 0, unsigned* contig = 0) const
	{
		for (regions_t::citer_t it=_regions.begin(); it!=_regions.end(); ++it)
		{
			if (!strcmp(name, it->region.name))
			{
				if (cached)
					*cached = it->region.cached;
				if (contig)
					*contig = it->region.contig;
				return it->locations.size() ? it->locations.front() : L4_fpage_t::create_nil();
			}
		}
		return L4_fpage_t::create_nil();
	}
};
static Named_memory_regions_t named_memory;
//--------------------------------------------------------------------------------------------------
//  ~ Named memory regions
//--------------------------------------------------------------------------------------------------

static int next_thread_no = l4_kip()->thread_info.user_base() + 2; // +0 sigma0, +1 alpha

// Sigma0 protocol
void get_memory_from_sigma0()
{
	wrm_logi("get memory from sigma0.\n");

	L4_utcb_t* utcb = l4_utcb();
	const L4_thrid_t sgm0  = l4_thrid_sigma0();

	size_t req_size = 0x80000000; // 2GB

	while (1)
	{
		utcb->acceptor(L4_acceptor_t(L4_fpage_t::create_complete(), false));

		L4_fpage_t req_fpage = L4_fpage_t::create(0, req_size, L4_fpage_t::Acc_rwx);
		assert(!req_fpage.is_nil());
		req_fpage.base(-1);
		word_t req_attr = 0; // arch dependent attribs, 0 - default

		L4_msgtag_t tag;
		tag.set_proto(L4_msgtag_t::Sigma0, 2, 0);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = req_fpage.raw();
		utcb->mr[2] = req_attr;

		L4_thrid_t from = L4_thrid_t::Nil;
		L4_time_t never(L4_time_t::Never);
		int rc = l4_ipc(sgm0, sgm0, L4_timeouts_t(never, never), &from);
		if (rc)
		{
			wrm_logi("l4_ipc(sgm0) - rc=%u.\n", rc);
			l4_kdb("Sending grant/map item is failed");
		}

		// copy msg to use printf below
		tag = utcb->msgtag();
		word_t mr[L4_utcb_t::Mr_words];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));

		//wrm_logd("rx:  from=%u, tag=0x%lx, u=%u, t=%u, mr[1]=0x%lx, mr[2]=0x%lx.\n",
		//	from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1], mr[2]);

		assert(tag.untyped() == 0);
		assert(tag.typed() == 2);

		L4_typed_item_t* item = (L4_typed_item_t*) &mr[1];
		assert(item->is_map_item());

		L4_map_item_t* mitem = (L4_map_item_t*) item;
		L4_fpage_t fpage = mitem->fpage();

		assert(!fpage.is_complete());
                                                                                                    
		if (!mitem->snd_base()  &&  fpage.is_nil())
		{
			// map reject
			//wrm_logd("map reject.\n");
			if (req_size == Cfg_page_sz)
				break;
			req_size >>= 1;
			continue;
		}

		//wrm_logd("rcv memory:  addr=%#x, sz=%#x.\n", fpage.addr(), fpage.size());

		memset((void*)fpage.addr(), 0, fpage.size());  // to check unmapped addresses
		wrm_mpool_add(fpage);
		//wrm_mpool_dump();
	}

	wrm_logi("got memory:  %#zx bytes.\n", wrm_mpool_size());
}

// Use MemoryControl privilaged system call
int make_not_cached(L4_fpage_t* fpages, size_t num)
{
	L4_utcb_t* utcb = l4_utcb();

	word_t attr0 = 1;  // NotCached

	// put fpages to MRs
	for (unsigned i=0; i<num; ++i)
		utcb->mr[0] = (fpages[i].raw() & ~0xf) + 0;  // set attr0 for fpage[i]

	return l4_memory_control(num-1, attr0);
}

// Sigma0 protocol
int do_iospace_request(L4_fpage_t iospace)
{
	L4_utcb_t* utcb = l4_utcb();
	const L4_thrid_t sgm0  = l4_thrid_sigma0();

	L4_msgtag_t tag;
	tag.set_proto(L4_msgtag_t::Sigma0, 2, 0);
	utcb->acceptor().set(L4_fpage_t::create_complete(), false);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = iospace.raw();
	utcb->mr[2] = 0; // req_attribs - arch dependent, 0 - default
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_time_t never(L4_time_t::Never);
	int rc = l4_ipc(sgm0, sgm0, L4_timeouts_t(never, never), &from);
	if (rc)
	{
		wrm_loge("l4_ipc(sgm0) - rc=%u.\n", rc);
		return -1;
	}

	// copy msg to use printf below
	tag = utcb->msgtag();
	word_t mr[L4_utcb_t::Mr_words];
	memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));

	//wrm_logd("rx:  from=%u, tag=0x%lx, u=%u, t=%u, mr[1]=0x%lx, mr[2]=0x%lx.\n",
	//	from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1], mr[2]);

	assert(tag.untyped() == 0);
	assert(tag.typed() == 2);

	L4_typed_item_t* item = (L4_typed_item_t*) &mr[1];
	assert(item->is_map_item());

	L4_map_item_t* mitem = (L4_map_item_t*) item;
	L4_fpage_t fpage = mitem->fpage();
	assert(!fpage.is_complete());

	if (!mitem->snd_base()  &&  fpage.is_nil())
	{
		//wrm_logi("map reject.\n");
		return -2;
	}

	//wrm_logd("rcv iospace:  addr=%#lx, sz=%#lx.\n", fpage.addr(), fpage.size());

	if (!fpage.is_io())
	{
		rc = make_not_cached(&fpage, 1);
		if (rc)
		{
			wrm_loge("make_not_cached() - rc=%d.\n", rc);
			return -3;
		}
	}

	return 0;
}

void get_iospace_from_sigma0(const Proj_cfg_t* proj_cfg)
{
	wrm_logi("get iospace from sigma0.\n");

	const Io_device_t* dev = app_config()->brd.devices_begin();
	while (dev && *dev->name())
	{
		// map io-ports
		if (dev->ioport_sz())
		{
			unsigned port_base = dev->ioport_addr();
			unsigned port_sz = dev->ioport_sz();
			//wrm_logd("Map IO-ports:  %s:  port=0x%x, sz=%u.\n", dev->name(), port_base, port_sz);
			L4_fpage_t io = L4_fpage_t::create_io(port_base, port_sz);
			assert(!io.is_nil());
			int rc = do_iospace_request(io);
			if (rc)
			{
				wrm_loge("Could not get io-port from Sigma0 '%s'.\n", dev->name());
				panic("do_iospace_request() - rc=%d.", rc);
			}
		}

		// map io-mem page by page
		if (dev->iomem_sz())
		{
			paddr_t pa_first = round_pg_down(dev->iomem_addr());
			paddr_t pa_final = round_pg_down(dev->iomem_addr() + dev->iomem_sz() - 1);
			//wrm_logd("Map IO-mem:    %s:  (0x%llx - 0x%llx].\n", dev->name(),
			//	(long long)pa_first, (long long)pa_final);
			for (paddr_t pa=pa_first; pa<=pa_final; pa+=Cfg_page_sz)
			{
				//wrm_logd("Map IO-mem page:  %s:  0x%llx.\n", dev->name(), pa);
				L4_fpage_t io = L4_fpage_t::create(pa, Cfg_page_sz, Acc_rw);
				assert(!io.is_nil());
				int rc = do_iospace_request(io);
				if (rc)
				{
					wrm_loge("Could not get io-mem from Sigma0 '%s'.\n", dev->name());
					panic("do_iospace_request() - rc=%d.", rc);
				}
			}
		}
		dev = Io_device_t::next(dev);
	}
	wrm_logi("got iospace from sigma0.\n");
}

void prepare_named_memory_regions(const Proj_cfg_t* proj_cfg)
{
	wrm_logi("prepare named memory regions for apps.\n");

	const Memory_region_t* mem = proj_cfg->mems_begin();
	while (mem && *mem->name)
	{
		//wrm_logi("  mem:  %8s:  sz=0x%08x, cached=%d, contig=%d\n",
		//	mem->name, mem->sz, mem->cached, mem->contig);

		// TODO:  support segmented NotContig memory

		if (!is_aligned(mem->sz, Cfg_page_sz))
		{
			wrm_loge("Wrong size=0x%zx, for named memory '%s'.\n", mem->sz, mem->name);
			wrm_loge("Size should be aligned to page size.\n");
			panic("Wrong config for named memory.");
		}

		L4_fpage_t location = wrm_mpool_alloc(mem->sz);
		if (location.is_nil())
		{
			wrm_loge("Could not allocate 0x%zx bytes for named memory '%s'.\n", mem->sz, mem->name);
			wrm_mpool_dump();
			panic("Could not allocate memory.");
		}

		if ((location.access() & mem->access) != mem->access)
		{
			wrm_loge("Wrong fpage's access permissions.\n");
			panic("Wrong fpage's access permissions.");
		}
		location.access(mem->access);

		int rc = make_not_cached(&location, 1);
		if (rc)
		{
			wrm_loge("make_not_cached() - rc=%d.\n", rc);
			panic("Could not set memory attribs.");
		}

		bool added = named_memory.add(mem, location);
		if (!added)
			panic("Could not add location to named memory.");

		//named_memory.dump();

		mem = Memory_region_t::next(mem);
	}

	wrm_logi("prepared named memory regions for apps.\n");
}

const Proj_cfg_t* parse_project_config()
{
	// find file in RAMFS
	addr_t cfg_addr = 0;
	size_t cfg_sz = 0;
	int rc = wrm_ramfs_get_file("config.alph", &cfg_addr, &cfg_sz);
	if (rc)
		panic("config file not found:  wrm_ramfs_get_file() - rc=%d.\n", rc);

	/*
	// debug output
	wrm_logi("config:\n\n");
	char* cfg_file = (char*)cfg_addr;
	for (unsigned i=0; i<cfg_sz; ++i)
		printf("%c", cfg_file[i]);
	printf("\n\n");
	*/

	// parse project config
	const Proj_cfg_t* proj_cfg = parse_config((char*)cfg_addr);
	if (!proj_cfg)
		exit(1);

	print_proj_config(proj_cfg);

	return proj_cfg;
}

static void break_execution(const char* str = 0)
{
	l4_kdb(str);
}

static void init_io()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->out_char   = NULL;
	cb->out_string = l4_kdb_putsn;
	cb->break_exec = break_execution;
}

//--------------------------------------------------------------------------------------------------
//  Named threads
//--------------------------------------------------------------------------------------------------
class Named_thread_t
{
public:
	enum
	{
		Name_words_max = 8,
		Name_len_max = Name_words_max * sizeof(word_t)
	};
	char       name [Name_len_max];
	L4_thrid_t id;
	word_t     key0;
	word_t     key1;

	Named_thread_t(const char* n, L4_thrid_t i, word_t k0, word_t k1) : id(i), key0(k0), key1(k1)
	{
		strncpy(name, n, Name_len_max);
		name[Name_len_max-1] = '\0';
	}
};

class Named_threads_t
{
	typedef list_t <Named_thread_t, 4> nthreads_t;
	static nthreads_t _nthreads;
public:

	static int record(const char* name, L4_thrid_t id, word_t* key0, word_t* key1)
	{
		if (strlen(name) + 1 > Named_thread_t::Name_len_max)
			return 1;  // too big name

		for (nthreads_t::citer_t it=_nthreads.cbegin(); it!=_nthreads.cend(); ++it)
			if (!strcmp(name, it->name))
				return 2;  // already exist

		// generate unique key
		*key0 = l4_system_clock();
		*key1 = l4_system_clock();

		_nthreads.push_back(Named_thread_t(name, id, *key0, *key1));
		return 0;
	}

	static int get_id(const char* name, L4_thrid_t* id, word_t* key0, word_t* key1)
	{
		for (nthreads_t::citer_t it=_nthreads.cbegin(); it!=_nthreads.cend(); ++it)
		{
			if (!strcmp(name, it->name))
			{
				*id   = it->id;
				*key0 = it->key0;
				*key1 = it->key1;
				return 0;
			}
		}
		return 1;
	}
};
Named_threads_t::nthreads_t Named_threads_t::_nthreads;
//--------------------------------------------------------------------------------------------------
//  ~Named threads
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//  Requests handlers
//--------------------------------------------------------------------------------------------------
static void process_pfault(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	// get pfault params
	assert(tag.untyped() == 2);
	assert(tag.typed() == 0);
	acc_t acc  = tag.pfault_access();
	word_t addr = mr[1];
	word_t inst = mr[2];
	//wrm_logi("pfault from 0x%x/%u:  addr=%x, acc=%s, inst=%x.\n", from.raw(), from.number(), addr, acc2str(acc), inst);

	// find local address for fault address
	L4_fpage_t local_fpage;
	int rc = wrm_app_get_location(from, addr, sizeof(word_t), acc, &local_fpage);
	if (rc == -2) // location not found
	{
		//wrm_logd("pager could not resolve pfault:  thr=%u, va=0x%lx, acc=%d, inst=0x%lx.\n",
		//	from.number(), addr, acc, inst);
		local_fpage = L4_fpage_t::create_complete();  // to raise exception for fault thread
	}
	else if (rc)
	{
		wrm_loge("wrm_app_get_location(addr=0x%lx, acc=%d) - rc=%d, thr=%u, inst=0x%lx.\n",
			addr, acc, rc, from.number(), inst);
		l4_kdb("unexpected error, roottask internal error");
	}

	// grant/map
	L4_map_item_t item = L4_map_item_t::create(local_fpage);
	tag.ipc_label(0);
	tag.propagated(false);
	tag.untyped(0);
	tag.typed(2);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = item.word0();
	utcb->mr[2] = item.word1();
	rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send(map) - rc=%u.\n", rc);
		l4_kdb("Sending grant/map item is failed");
	}
}

static void process_io_pfault(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	// get io-pfault params
	assert(tag.untyped() == 2);
	assert(tag.typed() == 0);
	L4_fpage_t fpage = L4_fpage_t::create(mr[1]);
	assert(fpage.is_io());
	word_t inst = mr[2];
	wrm_logw("io-pfault from %u:  port=0x%lx, sz=0x%lx, inst=%lx.\n", from.number(),
		fpage.io_port(), fpage.io_size(), inst);

	assert(fpage.io_size() == 1);

	// prepare result IO-fpage
	L4_fpage_t res_fpage = fpage;

	// check permission
	bool perm_ok = false;
	do
	{
		// find app cfg
		const Wrm_app_cfg_t* cfg = app_config()->apps_begin();
		unsigned from_num = from.number();
		unsigned app_first_thread_num = l4_kip()->thread_info.user_base() + 2;
		while (cfg)
		{
			if (from_num >= app_first_thread_num  &&
			    from_num < (app_first_thread_num + cfg->max_threads))
				break;
			app_first_thread_num += cfg->max_threads;
			cfg = Wrm_app_cfg_t::next(cfg);
		}
		if (!cfg)
		{
			wrm_loge("io-pfault:  no app for id=%u.\n", from.number());
			break;
		}

		// check all devs from dev cfg
		const Io_device_t* dev = app_config()->brd.devices_begin();
		while (dev && !perm_ok)
		{
			const Wrm_app_cfg_t::dev_name_t* dname = cfg->devs;
			while ((*dname)[0])
			{
				if (!strcmp(*dname, dev->name()))
				{
					if (fpage.io_port() >= dev->ioport_addr()  &&
						(fpage.io_port() + fpage.io_size()) < dev->ioport_end())
					{
						//wrm_logd("io-pfault:  found device:  name=%s, ioport=0x%x, sz=%u.\n",
						//	dev->name(), dev->ioport_addr(), dev->ioport_sz());
						perm_ok = true;
						break;
					}
				}
				dname++;
			}
			dev = Io_device_t::next(dev);
		}
	} while (0);

	if (!perm_ok)
	{
		wrm_loge("No permissions for ioport=0x%lx.\n", fpage.io_port());
		res_fpage = L4_fpage_t::create_complete();  // to raise exception for fault thread
	}

	// grant/map
	L4_map_item_t item = L4_map_item_t::create(res_fpage);
	tag.set_ipc(0, 0, 2);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = item.word0();
	utcb->mr[2] = item.word1();
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send(map) - rc=%u.\n", rc);
		l4_kdb("Sending grant/map item is failed");
	}
}

static void process_exception(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	printf("\n");
	wrm_loge("catch unhandled exception (%ld) from user thread id=%u:\n", tag.proto_label(), from.number());

	if (tag.proto_label() == L4_msgtag_t::Mmu_exception)
	{
		wrm_loge("MMU exception:  addr=0x%lx, acc=%ld.\n\n", mr[1] & ~0x7, mr[1] & 0x7);
	}

	Entry_frame_t* eframe = (Entry_frame_t*) &mr[2];
	eframe->dump(printf, false);

	printf("\n");
	wrm_loge("TODO:  terminate thread/task.\n");
	panic("xxx");
}

void process_map_io(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	enum
	{
		Dev_name_words_max = 8,
		Dev_name_len_max = Dev_name_words_max * sizeof(word_t)
	};
	assert(tag.untyped() <= Dev_name_words_max);
	assert(tag.typed() == 0);
	const char* dev_name = (const char*)&mr[1];
	//wrm_logd("map_io:  dev=%.*s.\n", Dev_name_len_max, dev_name);

	int ecode = 0;
	size_t iomem_offset = 0;
	size_t iomem_size = 0;
	L4_fpage_t iomem = L4_fpage_t::create_nil();
	L4_fpage_t ioport = L4_fpage_t::create_nil();
	do
	{
		// find app cfg
		const Wrm_app_cfg_t* cfg = app_config()->apps_begin();
		unsigned from_num = from.number();
		unsigned app_first_thread_num = l4_kip()->thread_info.user_base() + 2;
		while (cfg)
		{
			if (from_num >= app_first_thread_num  &&
			    from_num < (app_first_thread_num + cfg->max_threads))
				break;
			app_first_thread_num += cfg->max_threads;
			cfg = Wrm_app_cfg_t::next(cfg);
		}
		if (!cfg)
		{
			wrm_loge("No app for id=0x%lx/%u.\n", from.raw(), from.number());
			ecode = 1;  // no app
			break;
		}

		// find dev cfg
		bool ok = false;
		const Io_device_t* dev = app_config()->brd.devices_begin();
		while (dev)
		{
			if (!strncmp(dev->name(), dev_name, Dev_name_len_max))
			{
				ok = true;
				break;
			}
			dev = Io_device_t::next(dev);
		}
		if (!ok)
		{
			wrm_loge("No such device '%s'.\n", dev_name);
			ecode = 2;  // no device
			break;
		}

		// find permissions
		ok = false;
		const Wrm_app_cfg_t::dev_name_t* dname = cfg->devs;
		while ((*dname)[0])
		{
			if (!strncmp(*dname, dev_name, Dev_name_len_max))
			{
				ok = true;
				break;
			}
			dname++;
		}
		if (!ok)
		{
			wrm_loge("No permission for device '%s'.\n", dev_name);
			ecode = 3;  // no perm
			break;
		}

		// map IO-mem if need
		if (dev->iomem_sz())
		{
			size_t map_sz = round_pg_down(dev->iomem_end() - 1) - round_pg_down(dev->iomem_addr()) + Cfg_page_sz;
			iomem = L4_fpage_t::create(round_pg_down(dev->iomem_addr()), map_sz, Acc_rw);
			assert(!iomem.is_nil());
			iomem_offset = get_pg_offset(dev->pa());
			iomem_size = dev->iomem_sz();
		}

		// map IO-ports if need
		if (dev->ioport_sz())
		{
			ioport = L4_fpage_t::create_io(dev->ioport_addr(), dev->ioport_sz());
			assert(!ioport.is_nil());
		}
	} while (0);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	if (ecode)
	{
		tag.set_ipc(Wrm_ipc_map_io, 1, 0);
		utcb->mr[0] = tag.raw();;
		utcb->mr[1] = ecode;
	}
	else
	{
		tag.set_ipc(Wrm_ipc_map_io, 2, 4);
		L4_map_item_t mitem_mem = L4_map_item_t::create(iomem);
		L4_map_item_t mitem_port = L4_map_item_t::create(ioport);
		utcb->mr[0] = tag.raw();;
		utcb->mr[1] = iomem_offset;
		utcb->mr[2] = iomem_size;
		utcb->mr[3] = mitem_mem.word0();
		utcb->mr[4] = mitem_mem.word1();
		utcb->mr[5] = mitem_port.word0();
		utcb->mr[6] = mitem_port.word1();
	}
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send(map) - rc=%u.\n", rc);
		l4_kdb("Sending grant/map item is failed");
	}
}

void process_attach_detach_int(L4_msgtag_t tag, word_t* mr, L4_thrid_t from, unsigned action)
{
	enum
	{
		Dev_name_words_max = 8,
		Dev_name_len_max = Dev_name_words_max * sizeof(word_t)
	};
	assert(tag.untyped() <= Dev_name_words_max);
	assert(tag.typed() == 0);
	const char* dev_name = (const char*)&mr[1];
	//wrm_logi("%atach_int:  dev=%.*s.\n", action==Wrm_ipc_attach_int?"at":"de", Dev_name_len_max, dev_name);

	int ecode = 0;
	size_t intno = -1;
	do
	{
		// find app cfg
		const Wrm_app_cfg_t* cfg = app_config()->apps_begin();
		unsigned from_num = from.number();
		unsigned app_first_thread_num = l4_kip()->thread_info.user_base() + 2;
		while (cfg)
		{
			if (from_num >= app_first_thread_num  &&
			    from_num < (app_first_thread_num + cfg->max_threads))
				break;
			app_first_thread_num += cfg->max_threads;
			cfg = Wrm_app_cfg_t::next(cfg);
		}
		if (!cfg)
		{
			wrm_loge("No app for id=0x%lx/%u.\n", from.raw(), from.number());
			ecode = 1;  // no app
			break;
		}

		// find dev cfg
		bool ok = false;
		const Io_device_t* dev = app_config()->brd.devices_begin();
		while (dev)
		{
			if (!strncmp(dev->name(), dev_name, Dev_name_len_max))
			{
				ok = true;
				break;
			}
			dev = Io_device_t::next(dev);
		}
		if (!ok)
		{
			wrm_loge("No such device '%s'.\n", dev_name);
			ecode = 2;  // no device
			break;
		}

		// find permissions
		ok = false;
		const Wrm_app_cfg_t::dev_name_t* dname = cfg->devs;
		while ((*dname)[0])
		{
			if (!strncmp(*dname, dev_name, Dev_name_len_max))
			{
				ok = true;
				break;
			}
			dname++;
		}
		if (!ok)
		{
			wrm_loge("No permission for device '%s'.\n", dev_name);
			ecode = 3;  // no perm
			break;
		}

		// attach/detach to interrupt thread via ThreadControl
		L4_thrid_t space = L4_thrid_t::Nil;
		L4_thrid_t sched = L4_thrid_t::Nil;
		L4_thrid_t pager = action == Wrm_ipc_attach_int  ?  from  :  L4_thrid_t::Nil;
		int rc = l4_thread_control(L4_thrid_t::create_irq(dev->irq()), space, sched, pager, 0);
		if (rc)
		{
			wrm_loge("%s:  l4_thread_control() - rc=%u.\n", __func__, rc);
			ecode = 4;  // internal error
			break;
		}
		intno = dev->irq();
	} while (0);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	tag.ipc_label(action);
	tag.propagated(false);
	tag.untyped(1);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = ecode ? -ecode : intno;
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send() - rc=%u.\n", rc);
		l4_kdb("Sending attach/detach response is failed");
	}
}

void process_named_memory_request(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	enum
	{
		Mem_name_words_max = 8,
		Mem_name_len_max = Mem_name_words_max * sizeof(word_t)
	};
	assert(tag.untyped() <= Mem_name_words_max);
	assert(tag.typed() == 0);
	const char* mem_name = (const char*)&mr[1];
	//wrm_logd("nmem_req:  mem=%.*s.\n", Mem_name_len_max, mem_name);

	int ecode = 0;
	unsigned cached = -1;
	unsigned contig = -1;
	L4_map_item_t mitem = L4_map_item_t::create(L4_fpage_t::create_nil());
	do
	{
		// find app cfg
		const Wrm_app_cfg_t* cfg = app_config()->apps_begin();
		unsigned from_num = from.number();
		unsigned app_first_thread_num = l4_kip()->thread_info.user_base() + 2;
		while (cfg)
		{
			if (from_num >= app_first_thread_num  &&
			    from_num < (app_first_thread_num + cfg->max_threads))
				break;
			app_first_thread_num += cfg->max_threads;
			cfg = Wrm_app_cfg_t::next(cfg);
		}
		if (!cfg)
		{
			wrm_loge("No app for id=0x%lx/%u.\n", from.raw(), from.number());
			ecode = 1;  // no app
			break;
		}

		// check permissions
		bool ok = false;
		const Wrm_app_cfg_t::mem_name_t* mname = cfg->mems;
		while ((*mname)[0])
		{
			if (!strncmp(*mname, mem_name, Mem_name_len_max))
			{
				ok = true;
				break;
			}
			mname++;
		}
		if (!ok)
		{
			wrm_loge("No permission for named memory '%s'.\n", mem_name);
			ecode = 3;  // no perm
			break;
		}

		// find memory
		L4_fpage_t location = named_memory.location(mem_name, &cached, &contig);
		if (location.is_nil())
		{
			wrm_loge("No such memory '%s'.\n", mem_name);
			ecode = 2;  // no device
			break;
		}

		mitem.set(location);

	} while (0);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	if (ecode)
	{
		tag.set_ipc(Wrm_ipc_get_named_mem, 1, 0);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = ecode;
	}
	else
	{
		tag.set_ipc(Wrm_ipc_get_named_mem, 4, 2);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = 0;                     // MSW of paddr
		utcb->mr[2] = mitem.fpage().addr();  // LSW of paddr
		utcb->mr[3] = cached;
		utcb->mr[4] = contig;
		utcb->mr[5] = mitem.word0();
		utcb->mr[6] = mitem.word1();
	}
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send(map) - rc=%u.\n", rc);
		l4_kdb("Sending grant/map item is failed");
	}
}

void process_usual_memory_request(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	assert(tag.untyped() == 2);
	assert(tag.typed() == 0);
	//wrm_logd("mem_req:  sz=0x%lx.\n", mr[1]);

	addr_t rem_va = mr[1];
	size_t req_sz = mr[2];
	int ecode = 0;
	L4_fpage_t location = L4_fpage_t::create_nil();
	do
	{
		// find app cfg
		const Wrm_app_cfg_t* cfg = app_config()->apps_begin();
		unsigned from_num = from.number();
		unsigned app_first_thread_num = l4_kip()->thread_info.user_base() + 2;
		while (cfg)
		{
			if (from_num >= app_first_thread_num  &&
			    from_num < (app_first_thread_num + cfg->max_threads))
				break;
			app_first_thread_num += cfg->max_threads;
			cfg = Wrm_app_cfg_t::next(cfg);
		}
		if (!cfg)
		{
			wrm_loge("mem_req:  no app for thr=%u.\n", from.number());
			ecode = 1;  // no app
			break;
		}

		// check heap limit
		if (req_sz > cfg->heap_sz)
		{
			//wrm_logd("mem_req:  heap not enough, req_sz=0x%zx, heap_sz=0x%zx.\n", req_sz, cfg->heap_sz);
			ecode = 2;  // heap not enough
			break;
		}

		// allocate memory
		location = wrm_mpool_alloc(req_sz);
		if (location.is_nil())
		{
			wrm_loge("mem_req:  wrm_mpool_alloc(0x%zx) - failed, mpool_sz=0x%zx.\n", req_sz, wrm_mpool_size());
			ecode = 3;  // memory not enough
			break;
		}

		// set location in alpha regions
		int rc = wrm_app_add_location(from, rem_va, location);
		if (rc)
		{
			wrm_loge("mem_req:  wrm_app_set_location() - rc=%d.\n", rc);
			ecode = 4;  // may be vspace crossing
			wrm_mpool_add(location);  // put back to mpool
			break;
		}

		cfg->heap_sz -= req_sz;

	} while (0);

	L4_map_item_t mitem = L4_map_item_t::create(location);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	if (ecode)
	{
		tag.set_ipc(Wrm_ipc_get_usual_mem, 1, 0);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = ecode;
	}
	else
	{
		tag.set_ipc(Wrm_ipc_get_usual_mem, 0, 2);
		utcb->mr[0] = tag.raw();
		utcb->mr[1] = mitem.word0();
		utcb->mr[2] = mitem.word1();
	}
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send(map) - rc=%u.\n", rc);
		l4_kdb("Sending grant/map item is failed");
	}
}

void process_create_thread_request(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	assert(tag.untyped() == 5);
	assert(tag.typed() == 0);
	//wrm_logi("request:  create thread.\n");

	int ecode = 0;
	L4_thrid_t newid = L4_thrid_t::Nil;
	do
	{
		// find app cfg
		const Wrm_app_cfg_t* cfg = app_config()->apps_begin();
		unsigned from_num = from.number();
		unsigned app_first_thread_num = l4_kip()->thread_info.user_base() + 2;
		while (cfg)
		{
			if (from_num >= app_first_thread_num  &&
			    from_num < (app_first_thread_num + cfg->max_threads))
				break;
			app_first_thread_num += cfg->max_threads;
			cfg = Wrm_app_cfg_t::next(cfg);
		}
		if (!cfg)
		{
			wrm_loge("%s:  no app for id=0x%lx/%u.\n", __func__, from.raw(), from.number());
			ecode = 1;  // no app
			break;
		}

		// alloc thread number
		unsigned newno = 0;
		int rc = wrm_app_alloc_thrno(from, &newno);
		if (rc)
		{
			wrm_loge("%s:  wrm_app_alloc_thrnum() - rc=%d.\n", __func__, rc);
			ecode = 2;  // no free threads
			break;
		}

		// get max prio for this app
		unsigned max_prio = 0;
		rc = wrm_app_max_prio(from, &max_prio);
		if (rc)
		{
			wrm_loge("%s:  wrm_app_max_prio() - rc=%d.\n", __func__, rc);
			ecode = 3;  // some error
			break;
		}

		// create thread
		newid = L4_thrid_t::create_global(newno);
		L4_fpage_t  rem_utcb = L4_fpage_t::create(mr[1]);
		unsigned prio = mr[2];
		L4_thrid_t space(mr[3]);
		L4_thrid_t sched(mr[4]);
		L4_thrid_t pager(mr[5]);

		// check prio
		if (prio > max_prio)
		{
			if (prio != 255)
				wrm_logw("require prio=%u > max_prio=%u, decrease prio to max=%u.\n", prio, max_prio, max_prio);
			prio = max_prio;
		}

		// find local address for rem_utcb area
		L4_fpage_t loc_utcb;
		rc = wrm_app_get_location(from, rem_utcb.addr(), rem_utcb.size(), Acc_rw, &loc_utcb);
		if (rc)
		{
			wrm_loge("%s:  wrm_app_get_location(utcb=0x%lx, sz=0x%lx, acc=%d) - rc=%d.\n",
				__func__, rem_utcb.addr(), rem_utcb.size(), Acc_rw, rc);
			ecode = 4;  // bad utcb
			break;
		}

		// set default params if incomming is Any
		L4_thrid_t alpha = l4_utcb()->global_id();
		space = !space.is_any() ? space : from;   // default:  use 'from' aspace
		sched = !sched.is_any() ? sched : alpha;  // default:  alpha to can set prio below via l4_schedule
		pager = !pager.is_any() ? pager : alpha;  // default:  default pager

		// create thread inside 'from' aspace via ThreadControl
		rc = l4_thread_control(newid, space, sched, pager, loc_utcb.addr());
		if (rc)
		{
			wrm_loge("%s:  l4_thread_control() - rc=%u.\n", __func__, rc);
			ecode = 5;  // some error
			break;
		}

		// set thread params via Schedule syscall, it should be done by thread scheduler
		// XXX:  it may be outside Alpha, or not
		rc = l4_schedule(newid, -1, -1, prio, -1);
		if (rc)
		{
			wrm_loge("%s:  l4_schedule() - rc=%d.\n", __func__, rc);
			ecode = 6;  // some error
			break;
		}
	} while (0);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	tag.ipc_label(Wrm_ipc_create_thread);
	tag.propagated(false);
	tag.untyped(2);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = ecode;
	utcb->mr[2] = newid.raw();
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("%s:  l4_send(map) - rc=%u.\n", __func__, rc);
		l4_kdb("Sending grant/map item is failed");
	}
}

void process_create_task_request(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	assert(tag.untyped() == 4);
	assert(tag.typed() == 0);
	//wrm_logi("request:  create thread with new aspace.\n");

	int ecode = 0;
	L4_thrid_t newid = L4_thrid_t::Nil;
	do
	{
		// find app cfg
		const Wrm_app_cfg_t* cfg = app_config()->apps_begin();
		unsigned from_num = from.number();
		unsigned app_first_thread_num = l4_kip()->thread_info.user_base() + 2;
		while (cfg)
		{
			if (from_num >= app_first_thread_num  &&
			    from_num < (app_first_thread_num + cfg->max_threads))
				break;
			app_first_thread_num += cfg->max_threads;
			cfg = Wrm_app_cfg_t::next(cfg);
		}
		if (!cfg)
		{
			wrm_loge("%s:  no app for id=0x%lx/%u.\n", __func__, from.raw(), from.number());
			ecode = 1;  // no app
			break;
		}

		// alloc new aspace
		int rc = wrm_app_alloc_aspace(from);
		if (rc)
		{
			wrm_loge("%s:  wrm_app_alloc_aspace() - rc=%d.\n", __func__, rc);
			ecode = 2;  // no free threads
			break;
		}

		// alloc thread number
		unsigned newno = 0;
		rc = wrm_app_alloc_thrno(from, &newno);
		if (rc)
		{
			wrm_loge("%s:  wrm_app_alloc_thrnum() - rc=%d.\n", __func__, rc);
			ecode = 3;  // no free threads
			break;
		}

		// get max prio for this app
		unsigned max_prio = 0;
		rc = wrm_app_max_prio(from, &max_prio);
		if (rc)
		{
			wrm_loge("%s:  wrm_app_max_prio() - rc=%d.\n", __func__, rc);
			ecode = 4;  // some error
			break;
		}

		// create task
		newid = L4_thrid_t::create_global(newno);
		L4_fpage_t rem_utcbs = L4_fpage_t::create(mr[1]);
		unsigned prio = mr[2];
		L4_thrid_t inc_pager(mr[3]);
		L4_fpage_t kip_area = L4_fpage_t::create(mr[4]);

		// check prio
		if (prio > max_prio)
		{
			wrm_logw("require prio=%u > max_prio=%u, decrease prio to max=%u.\n", prio, max_prio, max_prio);
			prio = max_prio;
		}

		// find local address for rem_utcbs
		L4_fpage_t loc_utcbs;
		rc = wrm_app_get_location(from, rem_utcbs.addr(), rem_utcbs.size(), Acc_rw, &loc_utcbs);
		assert(rem_utcbs.size() == loc_utcbs.size());
		if (rc)
		{
			wrm_loge("%s:  wrm_app_get_location(utcbs=0x%lx, sz=0x%lx, acc=%d) - rc=%d.\n",
				__func__, rem_utcbs.addr(), rem_utcbs.size(), Acc_rw, rc);
			ecode = 5;  // bad utcbs
			break;
		}

		// create application's thread/aspace via ThreadControl
		L4_thrid_t alpha = l4_utcb()->global_id();
		L4_thrid_t space = newid;  // =dest for aspace creation
		L4_thrid_t sched = alpha;  // alpha to can set prio below via l4_schedule
		L4_thrid_t pager = L4_thrid_t::Nil;
		rc = l4_thread_control(newid, space, sched, pager, loc_utcbs.addr());
		if (rc)
		{
			wrm_loge("%s:  l4_thread_control() - rc=%d.\n", __func__, rc);
			ecode = 6;
			break;
		}

		// set thread params via Schedule
		rc = l4_schedule(newid, -1, -1, prio, -1);
		if (rc)
		{
			wrm_loge("%s:  l4_schedule() - rc=%d.\n", __func__, rc);
			ecode = 7;
			break;
		}

		// configure application's aspace via SpaceControl
		word_t control = 0;
		L4_thrid_t redirector = L4_thrid_t::Nil;
		rc = l4_space_control(newid, control, kip_area, loc_utcbs, redirector);
		if (rc)
		{
			wrm_loge("%s:  l4_space_control() - rc=%d.\n", __func__, rc);
			ecode = 8;
			break;
		}

		// activate aplication's thread via ThreadControl
		pager = inc_pager;
		rc = l4_thread_control(newid, space, sched, pager, -1);
		if (rc)
		{
			wrm_loge("%s:  l4_thread_control() - rc=%d.\n", __func__, rc);
			ecode = 9;  // bad utcb
			break;
		}
	} while (0);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	tag.ipc_label(Wrm_ipc_create_task);
	tag.propagated(false);
	tag.untyped(2);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = ecode;
	utcb->mr[2] = newid.raw();
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("%s:  l4_send(map) - rc=%u.\n", __func__, rc);
		l4_kdb("Sending grant/map item is failed");
	}
}

// register thread by name
void process_register_thread_request(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	assert(tag.untyped() <= Named_thread_t::Name_words_max);
	assert(tag.typed() == 0);
	const char* name = (const char*)&mr[1];  // FIXME:  do not access to MR via pointer
	//wrm_logi("regthr:  %.*s.\n", Named_thread_t::Name_len_max, name);

	int ecode = 0;
	word_t key0 = 1;
	word_t key1 = 2;
	do
	{
		int rc = Named_threads_t::record(name, from, &key0, &key1);
		if (rc)
		{
			wrm_loge("Failed to register thread, rc=%d.\n", rc);
			ecode = 1;  // no app
			break;
		}

	} while (0);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	tag.ipc_label(Wrm_ipc_register_thread);
	tag.propagated(false);
	tag.typed(0);
	tag.untyped(3);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = ecode;
	utcb->mr[2] = key0;
	utcb->mr[3] = key1;
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send(rep) - rc=%u.\n", rc);
		l4_kdb("Sending register-thread response is failed");
	}
}

// get thread ID by name
void process_get_thread_id_request(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	assert(tag.untyped() <= Named_thread_t::Name_words_max);
	assert(tag.typed() == 0);
	const char* name = (const char*)&mr[1];  // FIXME:  do not access to MR via pointer
	//wrm_logi("regthr:  %.*s.\n", Named_thread_t::Name_len_max, name);

	int ecode = 0;
	word_t key0 = 1;
	word_t key1 = 2;
	L4_thrid_t id;
	do
	{
		int rc = Named_threads_t::get_id(name, &id, &key0, &key1);
		if (rc)
		{
			wrm_loge("%s:  failed to get thread id, name=%.*s, rc=%d.\n",
				__func__, Named_thread_t::Name_len_max, name, rc);
			ecode = 1;  // no app
			break;
		}

	} while (0);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	tag.ipc_label(Wrm_ipc_get_thread_id);
	tag.propagated(false);
	tag.typed(0);
	tag.untyped(4);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = ecode;
	utcb->mr[2] = id.raw();
	utcb->mr[3] = key0;
	utcb->mr[4] = key1;
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send(rep) - rc=%u.\n", rc);
		l4_kdb("Sending get_thr-id response is failed.");
	}
}

// get range of thread IDs for app specefied by thread ID
void process_app_threads_request(L4_msgtag_t tag, word_t* mr, L4_thrid_t from)
{
	assert(tag.untyped() == 1);
	assert(tag.typed() == 0);
	//wrm_logi("thrnums:  from=0x%lx/%u.\n", from.raw(), from.number());

	L4_thrid_t app_id = mr[1];
	unsigned thrno_begin = 0;
	unsigned thrno_end = 0;

	int ecode = wrm_app_get_thr_numbers(app_id, &thrno_begin, &thrno_end);

	// send reply
	L4_utcb_t* utcb = l4_utcb();
	tag.ipc_label(Wrm_ipc_app_threads);
	tag.propagated(false);
	tag.untyped(3);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = ecode;
	utcb->mr[2] = thrno_begin;
	utcb->mr[3] = thrno_end;
	int rc = l4_send(from, L4_time_t::Never);
	if (rc)
	{
		wrm_loge("l4_send(map) - rc=%u.\n", rc);
		l4_kdb("Sending grant/map item is failed");
	}
}
//--------------------------------------------------------------------------------------------------
//  ~Requests handlers
//--------------------------------------------------------------------------------------------------

int main()
{
	init_bss();                        // clear .bss section
	init_io();                         // initialize IO
	call_ctors();                      // call user ctors

	wrm_logi("hello.\n");

	// get memory from sigma0 and put it to memory pool
	get_memory_from_sigma0();

	// parse project config
	const Proj_cfg_t* proj_cfg = parse_project_config();

	get_iospace_from_sigma0(proj_cfg);
	prepare_named_memory_regions(proj_cfg);

	// run all project apps
	const Wrm_app_cfg_t* app = proj_cfg->apps_begin();
	while (app)
	{
		wrm_logi("create app=%s.\n", app->name);
		L4_thrid_t id = L4_thrid_t::create_global(next_thread_no);
		int rc = wrm_app_create(id, app);
		if (rc)
		{
			wrm_loge("wrm_app_create() - rc=%d.\n", rc);
			l4_kdb("failed to create app");
		}
		next_thread_no += app->max_threads;
		app = Wrm_app_cfg_t::next(app);
	}

	// ipc loop - wait pfaults msgs from apps
	while (1)
	{
		L4_thrid_t from = L4_thrid_t::Nil;
		//wrm_logi("wait ipc.\n");
		int rc = l4_receive(L4_thrid_t::Any, L4_time_t::Never, &from);
		L4_utcb_t* utcb = l4_utcb();
		L4_msgtag_t tag = utcb->msgtag();
		word_t mr[L4_utcb_t::Mr_words];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));

		//wrm_logi("rx:  rc=%d.\n", rc);
		if (rc)
		{
			wrm_loge("rx:  rc=%d.\n", rc);
			continue;
		}

		//wrm_logi("rx:  from=0x%x/%u, tag=0x%x.\n", from.raw(), from.number(), tag.raw());

		if (tag.proto_label() == L4_msgtag_t::Pagefault)
		{
			process_pfault(tag, mr, from);
		}
		else if (tag.proto_label() == L4_msgtag_t::Io_pagefault)
		{
			process_io_pfault(tag, mr, from);
		}
		else if (tag.is_exc_request())
		{
			process_exception(tag, mr, from);
		}
		else
		{
			switch (tag.ipc_label())
			{
				case Wrm_ipc_map_io:           process_map_io(tag, mr, from);                  break;
				case Wrm_ipc_attach_int:       process_attach_detach_int(tag, mr, from, Wrm_ipc_attach_int);  break;
				case Wrm_ipc_detach_int:       process_attach_detach_int(tag, mr, from, Wrm_ipc_detach_int);  break;
				case Wrm_ipc_get_usual_mem:    process_usual_memory_request(tag, mr, from);    break;
				case Wrm_ipc_get_named_mem:    process_named_memory_request(tag, mr, from);    break;
				case Wrm_ipc_create_thread:    process_create_thread_request(tag, mr, from);   break;
				case Wrm_ipc_create_task:      process_create_task_request(tag, mr, from);     break;
				case Wrm_ipc_register_thread:  process_register_thread_request(tag, mr, from); break;
				case Wrm_ipc_get_thread_id:    process_get_thread_id_request(tag, mr, from);   break;
				case Wrm_ipc_app_threads:      process_app_threads_request(tag, mr, from);     break;
				default:
					wrm_loge("rx:  from=%u, label=%ld, u=%u, t=%u.\n", from.number(), tag.ipc_label(), tag.untyped(), tag.typed());
					l4_kdb("Alpha received unexpected msg - IMPLEMENT ME");
			}
		}
	}

	return 0;
}
