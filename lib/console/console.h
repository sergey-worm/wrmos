//##################################################################################################
//
//  console - console protocol implementation.
//
//##################################################################################################

#ifndef CONSOLE_H
#define CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

int w4console_init(void);
int w4console_open(void);
int w4console_close(void);
int w4console_read(void* buf, unsigned sz);
int w4console_write(const void* buf, unsigned sz);
void w4console_set_wlibc_cb(void);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_H
