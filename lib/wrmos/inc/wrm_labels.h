//##################################################################################################
//
//  wrm_labels.h - labels for IPC messages.
//
//##################################################################################################

#ifndef WRM_LABELS
#define WRM_LABELS

enum
{
	// Alpha proto
	Wrm_ipc_map_io           = 1,
	Wrm_ipc_attach_int       = 2,
	Wrm_ipc_detach_int       = 3,
	Wrm_ipc_get_usual_mem    = 4,
	Wrm_ipc_get_named_mem    = 5,
	Wrm_ipc_create_thread    = 6,
	Wrm_ipc_register_thread  = 7,
	Wrm_ipc_get_thread_id    = 8,
	Wrm_ipc_app_threads      = 9
};

#endif // WRM_LABELS
