//##################################################################################################
//
//  ramfs.h - structure of file header and binary data,
//            ramfs.generate.c will be generated by 'make'
//            and will content file's headers and bodies.
//
//##################################################################################################

#ifndef RAMFS_H
#define RAMFS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	char* name;                    // file name, 0-terminated C-string
	unsigned size;                 // data size in bytes
	const unsigned char* data;     // data body pointer
} Ramfs_file_header_t;

inline Ramfs_file_header_t* ramfs_next(Ramfs_file_header_t* hdr)
{
	Ramfs_file_header_t* next = hdr + 1;
	return next->name ? next : 0;  // no name --> list terminator
}

#ifdef __cplusplus
}
#endif

#endif // RAMFS_H