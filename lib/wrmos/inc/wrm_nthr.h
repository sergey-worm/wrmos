//##################################################################################################
//
//  Named thread - allow to register threads by name.
//
//##################################################################################################

#ifndef WRM_NAMED_THREAD_H
#define WRM_NAMED_THREAD_H

#include "l4types.h"

#ifdef __cplusplus
extern "C" {
#endif

int wrm_nthread_register(const char* name, word_t* key0, word_t* key1);
int wrm_nthread_get_id(const char*name, L4_thrid_t* id, word_t* key0, word_t* key1);

#ifdef __cplusplus
}
#endif

#endif // WRM_NAMED_THREAD_H

