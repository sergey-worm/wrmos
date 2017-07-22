//##################################################################################################
//
//  wrm_log.h - WrmOS log subsystem.
//
//##################################################################################################

#ifndef WRM_LOG_H
#define WRM_LOG_H

#include <stdarg.h>

typedef enum
{
	Wrm_log_err,
	Wrm_log_wrn,
	Wrm_log_inf,
	Wrm_log_dbg
} Wrm_loglevel_t;

#ifdef __cplusplus
extern "C" {
#endif

void wrm_vlog(Wrm_loglevel_t level, const char* fmt, va_list args);
void wrm_log(Wrm_loglevel_t level, const char* fmt, ...);
void wrm_loge(const char* fmt, ...);
void wrm_logw(const char* fmt, ...);
void wrm_logi(const char* fmt, ...);
void wrm_logd(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // WRM_LOG_H
