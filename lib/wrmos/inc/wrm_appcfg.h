//##################################################################################################
//
//  App config - to storage config of application.
//
//##################################################################################################

#ifndef WRM_APP_CFG_H
#define WRM_APP_CFG_H

#include "l4_types.h"

//#ifdef __cplusplus
//extern "C" {
//#endif

class Wrm_app_cfg_t
{
public:

	enum
	{
		App_name_sz          = 16,
		App_short_name_sz    =  5,
		File_name_sz         = 32,
		Arg_sz               = 16,
		Arg_list_sz          =  4,
		Dev_name_sz          = 12,
		Dev_list_sz          = 12,
		Mem_name_sz          = 12,
		Mem_list_sz          =  4,

		Malloc_on_pagefault  =  0,  // default
		Malloc_on_startup    =  1,
	};

public:

	typedef char mem_name_t [Mem_name_sz];
	typedef char dev_name_t [Dev_name_sz];
	typedef char arg_t [Arg_sz];

	char           name [App_name_sz];             //
	char           short_name [App_short_name_sz]; //
	char           filename [File_name_sz];        //
	unsigned       stack_sz;                       // should be alligned to page size
	mutable size_t heap_sz;                        // should be alligned to page size
	uint8_t        max_aspaces;                    //
	uint8_t        max_threads;                    //
	uint8_t        max_prio;                       //
	uint8_t        fpu;                            //
	int            malloc_strategy;                // on_startup or on_pagefault
	dev_name_t     devs [Dev_list_sz];             // available devices
	mem_name_t     mems [Mem_list_sz];             // available memory regions
	arg_t          args [Arg_list_sz];             //

	static const Wrm_app_cfg_t* next(const Wrm_app_cfg_t* cur)
	{
		const Wrm_app_cfg_t* next = cur + 1;
		return next->name[0]!=0 ? next : 0;
	}

	const char* malloc_strategy_str() const
	{
		switch (malloc_strategy)
		{
			case Malloc_on_startup:    return "on_startup";
			case Malloc_on_pagefault:  return "on_pagefault";
		}
		return "__unknown_map_strategy__";
	}
};

//#ifdef __cplusplus
//}
//#endif

#endif // WRM_APP_CFG_H

