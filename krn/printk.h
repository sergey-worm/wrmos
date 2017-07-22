#ifndef PRINTK_H
#define PRINTK_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef Cfg_klog
void printk(const char* fmt, ...);
#else
#define printk(...)
#endif

void force_printk(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // PRINTK_H
