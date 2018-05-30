//##################################################################################################
//
//  boot1 - need for load whole image from disk to RAM and run main bootloader.
//
//  NOTE:  no constructors, no bss initializations.
//
//##################################################################################################

#include <stdint.h>
#include <stddef.h>
#include "bootloader-load-addr.h"
#include "ldr-config.h"

#define UART_WITH_VIDEO
#include "uart.h"

#define INL __attribute__((always_inline)) inline

static INL void outb(uint16_t port, uint8_t v)
{
	asm volatile ("outb %0, %1" :: "a"(v), "dN"(port));
}

static INL uint8_t inb(uint16_t port)
{
	uint8_t v;
	asm volatile ("inb %1, %0" : "=a"(v) : "dN"(port));
	return v;
}

static INL void stack_pushw(uint16_t val)
{
	asm volatile ("pushw %0" :: "r"(val));
}

static INL uint16_t stack_popw()
{
	uint16_t val;
	asm volatile ("popw %0" : "=r"(val));
	return val;
}

// don't use library implementation cause in boot1 time no libraries in memory
static void memcpy(void* dst, const void* src, size_t sz)
{
	for (size_t i=0; i<sz; ++i)
		((char*)dst)[i] = ((char*)src)[i];
}

// don't use library implementation cause in boot1 time no libraries in memory
static void memset(void* dst, int val, size_t sz)
{
	for (size_t i=0; i<sz; ++i)
		((char*)dst)[i] = val;
}

//--------------------------------------------------------------------------------------------------
// PRINT
//--------------------------------------------------------------------------------------------------

static void print_string(unsigned port, const char* str)
{
	while (*str)
	{
		uart_putc(port, *str);
		str++;
	}
}

static void puts(const char* str1, const char* str2 = "", const char* str3 = "")
{
	print_string(Cfg_ldr_uart_paddr, str1);
	print_string(Cfg_ldr_uart_paddr, str2);
	print_string(Cfg_ldr_uart_paddr, str3);
}

static void panic(const char* str1, const char* str2 = "")
{
	puts("Panic:  ", str1, str2);
	while (1);
}

inline unsigned min(unsigned a, unsigned b) { return a<b ? a : b; }
inline unsigned max(unsigned a, unsigned b) { return a>b ? a : b; }

static void print_num(unsigned port, unsigned num, int base, unsigned width = 0)
{
	char all_digits[] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
	char digits[22];
	memset(digits, '0', sizeof(digits));
	unsigned most_significant_digit = 0;
	unsigned pos = 0;
	while (num)
	{
		unsigned d = num % base;
		num /= base;
		digits[pos] = all_digits[d];
		if (d)
			most_significant_digit = pos;
		pos++;
	}

	if (width)
		width -= 1; // cause prints width+1 symbols
	width = max(width, most_significant_digit);
	width = min(width, sizeof(digits) - 1);

	for (int i=width; i>=0; --i)
		uart_putc(port, digits[i]);
}

static void print_dec(const char* pref, unsigned num, const char* postf = "")
{
	puts(pref);
	print_num(Cfg_ldr_uart_paddr, num, 10);
	puts(postf);
}

static void print_hex(const char* pref, unsigned num, const char* postf = "")
{
	puts(pref);
	print_num(Cfg_ldr_uart_paddr, num, 16);
	puts(postf);
}

//--------------------------------------------------------------------------------------------------
// BIOS FUNCS
//--------------------------------------------------------------------------------------------------

struct Bioscall_frame_t
{
	union
	{
		struct
		{
			uint16_t ax;
			uint16_t cx;
			uint16_t dx;
			uint16_t bx;
			uint16_t intno;
		};
		struct
		{
			uint8_t al;
			uint8_t ah;
			uint8_t cl;
			uint8_t ch;
			uint8_t dl;
			uint8_t dh;
			uint8_t bl;
			uint8_t bh;
			uint16_t flags;
		};
	};

	Bioscall_frame_t() : ax(0), cx(0), dx(0), bx(0), intno(0) {}

	INL void push()
	{
		stack_pushw(ax);
		stack_pushw(cx);
		stack_pushw(dx);
		stack_pushw(bx);
		stack_pushw(intno);
	}

	INL void pop()
	{
		flags = stack_popw();
		bx = stack_popw();
		dx = stack_popw();
		cx = stack_popw();
		ax = stack_popw();
	}

	void dump()
	{
		puts("  bioscall_frame:\r\n");
		print_hex("  | ax=0x", ax, "\r\n");
		print_hex("  | cx=0x", cx, "\r\n");
		print_hex("  | dx=0x", dx, "\r\n");
		print_hex("  | bx=0x", bx, "\r\n");
		print_hex("  | int/flg=0x", intno, "\r\n");
	}

} __attribute__((packed));

extern "C" void do_bioscall();

void __attribute__((noinline)) bioscall(Bioscall_frame_t* frame)
{
	frame->push();
	do_bioscall();
	frame->pop();
}

void bios_read_disk_params(uint16_t devid, uint16_t* max_track, uint16_t* max_head, uint16_t* max_sector)
{
	Bioscall_frame_t frame;
	frame.ah = 0x08;                // cmd:  'get disk params'
	frame.dl = devid;               // use initial boot device
	frame.intno = 0x13;             // disk IO

	bioscall(&frame);

	if (frame.flags & 1)            // carry flag set
		frame.dump();

	*max_track = ((uint16_t)(frame.cl & 0xc0) << 2) | frame.ch;
	*max_head = frame.dh;
	*max_sector = (uint16_t)frame.cl & 0x3f;
}

// read one dosk sector to space [0x7a00, 0x7c00)
void bios_read_disk_sector(uint16_t devid, uint16_t track, uint16_t head, uint16_t sector)
{
	Bioscall_frame_t frame;
	frame.ah = 0x02;                // cmd:  'read sector'
	frame.al = 1;                   // one sector
	frame.bx = 0x7a00;              // buf is %es:%bx (assume %es=0)
	frame.ch = track;               // track.lo8
	frame.cl = (track>>8) | sector; // track.hi2 | sector
	frame.dh = head;                // head
	frame.dl = devid;               // use initial boot device
	frame.intno = 0x13;             // disk IO

	bioscall(&frame);

	if (frame.flags & 1)            // carry flag set
		panic(__func__, ":  failed. \r\n");
}

// read one dosk sector to space [0x7a00, 0x7c00)
void bios_disable_video_cursor()
{
	Bioscall_frame_t frame;
	frame.ah = 0x01;                // cmd:  'set cursor'
	frame.ch = 0x20;                // off
	frame.intno = 0x10;             // video IO

	bioscall(&frame);

	if (frame.flags & 1)            // carry flag set
		panic(__func__, ":  failed. \r\n");
}

//--------------------------------------------------------------------------------------------------
// LOADER_1
//--------------------------------------------------------------------------------------------------

// load image from boot device to RAM
void load_image()
{
	extern uint8_t boot_device_id;
	uint16_t max_head = 0;
	uint16_t max_track = 0;
	uint16_t max_sector = 0;

	bios_read_disk_params(boot_device_id, &max_track, &max_head, &max_sector);

	print_dec("[boot1]  max_track:   ", max_track,  "\r\n");
	print_dec("[boot1]  max_head:    ", max_head,   "\r\n");
	print_dec("[boot1]  max_sector:  ", max_sector, "\r\n");

	puts("[boot1]  ");

	// now we have read from device:
	//   *  1 sector  for boot0
	//   * 16 sectors for boot1
	// and will read sector by sector at 0x7a00 while does not found
	// marker 0xdeadbabe at the start of sector
	uint16_t  track  = 0;
	uint16_t  head   = 0;
	uint16_t  sector = 18;
	uint32_t* marker = (uint32_t*)0x7a00;
	unsigned  count  = 0;

	while (1)
	{
		if (!(count % 0x20))  // print dot every 0x4000 bytes
			puts(!(count % 0x800) ? "\r\n[boot1]  " : ".");

		//print_dec("[boot1]  track=", track, ", ");
		//print_dec("head=", head, ", ");
		//print_dec("sector=", sector, ".\r\n");

		bios_read_disk_sector(boot_device_id, track, head, sector);

		if (*marker == 0xdeadbabe)
			break;

		// relocate loaded sector to target memory
		size_t dst = Cfg_load_addr + count * 0x200;
		memcpy((void*)dst, (void*)0x7a00, 0x200);
		//print_hex("cpy to src=0x", 0x7c00 + 1*0x200 + 16*0x200 + count * 0x200);
		//print_hex(", dst=0x", dst, "\r\n");
		count++;

		// inc sector
		if (sector != max_sector)
		{
			sector++;
		}
		else // inc head
		{
			sector = 1;
			if (head != max_head)
			{
				head++;
			}
			else // inc track
			{
				head = 0;
				if (track != max_track)
					track++;
				else
					panic("\r\n[boot1]  error:  no end of image.\r\n");
			}
		}
	}
	puts("\r\n");
}

//char stack_area[0x1000];

extern "C" int main();

extern "C" void boot1_main()
{
	bios_disable_video_cursor();
	uart_init(Cfg_ldr_uart_paddr, Cfg_ldr_uart_bitrate, Cfg_sys_clock_hz);

	puts("[boot1]  hello.\n\r");

	puts("[boot1]  load image ...\n\r");
	load_image();
	puts("[boot1]  image loded.\n\r");

	puts("[boot1]  go to main bootloader.\n\r");
	main();
}
