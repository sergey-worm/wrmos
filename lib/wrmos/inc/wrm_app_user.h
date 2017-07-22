//##################################################################################################
//
//  App - user API for applications.
//
//##################################################################################################

#ifndef WRM_APP_USER_H
#define WRM_APP_USER_H

#include "l4types.h"

#ifdef __cplusplus
extern "C" {
#endif

int wrm_app_threads(L4_thrid_t app, unsigned* thrno_begin, unsigned* thrno_end);

#ifdef __cplusplus
}
#endif

#endif // WRM_APP_USER_H

