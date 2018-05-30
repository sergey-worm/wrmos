#ifndef PRINTK_H
#define PRINTK_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef Cfg_klog
void printk(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void printk_tmr_pend(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
#else
#define printk(...) do {} while (0)
#define printk_tmr_pend(...) do {} while (0)
#endif

void force_printk(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void force_printk_uart(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void printf_uart(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif // PRINTK_H
