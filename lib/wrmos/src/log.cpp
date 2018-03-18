//##################################################################################################
//
//  Log - WrmOS log subsystem.
//
//##################################################################################################

#include "wrm_log.h"
#include "l4_api.h"
#include <stdio.h>
#include <string.h>

static const char* loglevel2str(Wrm_loglevel_t level)
{
	switch (level)
	{
		case Wrm_log_err:  return "\x1b[1;31merr:\x1b[0m  ";
		case Wrm_log_wrn:  return "\x1b[1;33mwrn:\x1b[0m  ";
		case Wrm_log_inf:  return "\x1b[0;32minf:\x1b[0m  ";
		case Wrm_log_dbg:  return "\x1b[0;34mdbg:\x1b[0m  ";
	}
	return "__unknown_log_level__";
}

extern "C" void wrm_vlog(Wrm_loglevel_t level, const char* fmt, va_list args)
{
	L4_clock_t clock = l4_system_clock();
	word_t name = l4_utcb()->tls[0];
	char name_str[5];
	memcpy(name_str, &name, 4);
	name_str[4] = '\0';
	char str[256];
	int sz = snprintf(str, sizeof(str), "[%4s:%llu.%06u]  %s", name_str,
		(long long int)clock/1000000, // this cast need cause clock (uint64_t) may be 'long' or 'long long'
		(int)clock%1000000, loglevel2str(level));
	vsnprintf(str+sz, sizeof(str)-sz, fmt, args);
	printf("%s", str);
}

extern "C" void wrm_log(Wrm_loglevel_t level, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wrm_vlog(level, fmt, args);
	va_end(args);
}

extern "C" void wrm_loge(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wrm_vlog(Wrm_log_err, fmt, args);
	va_end(args);
}

extern "C" void wrm_logw(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wrm_vlog(Wrm_log_wrn, fmt, args);
	va_end(args);
}

extern "C" void wrm_logi(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wrm_vlog(Wrm_log_inf, fmt, args);
	va_end(args);
}

extern "C" void wrm_logd(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wrm_vlog(Wrm_log_dbg, fmt, args);
	va_end(args);
}
