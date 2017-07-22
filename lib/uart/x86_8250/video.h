//##################################################################################################
//
//  video.h - allows output to video display, may be use as virtual uart.
//
//##################################################################################################

// this data should be exist somewhere
extern unsigned video_x;
extern unsigned video_y;
extern uintptr_t video_mem;

enum
{
	Video_x_min =  0,       //
	Video_x_max = 80,       //
	Video_y_min =  6,       // reserve space for banner
	Video_y_max = 25,       //

	Cursor = 22,            //
	Space  = ' '            //
};

inline void video_cell(unsigned x, unsigned y, char ch, uint8_t attr = 0x07)
{
	if (x >= Video_x_max  ||  y >= Video_y_max)
		return;

	uint16_t* vmem = (uint16_t*) video_mem;
	*(vmem + Video_x_max*y + x) = (attr << 8) | ch;
}

inline char video_cell(unsigned x, unsigned y)
{
	if (x >= Video_x_max  ||  y >= Video_y_max)
		return '@';

	uint16_t* vmem = (uint16_t*) video_mem;
	return *(vmem + Video_x_max*y + x) & 0xff;
}

static void video_clear_screen()
{
	for (unsigned y=0; y<Video_y_max; ++y)
		for (unsigned x=0; x<Video_x_max; ++x)
			video_cell(x, y, ' ');
}

static void video_shift_line()
{
	// shift all buffer up
	for (unsigned y=Video_y_min; y<Video_y_max-1; ++y)
		for (unsigned x=Video_x_min; x<Video_x_max; ++x)
			video_cell(x, y, video_cell(x, y + 1));

	// clear bottom line
	for (unsigned x=0; x<Video_x_max; ++x)
		video_cell(x, Video_y_max - 1, ' ');
}

static void video_putc(char ch)
{
	static bool inside_escape = 0;
	if (ch == '\x1b')
		inside_escape = 1;

	if (inside_escape)
	{
		if (ch == 'm')
			inside_escape = 0;
		return;
	}

	if (ch == '\r')
	{
		if (video_cell(video_x, video_y) == Cursor) // remove cursor
			video_cell(video_x, video_y, Space);
		video_x = Video_x_min;
		if (video_cell(video_x, video_y) == Space) // set cursor
			video_cell(video_x, video_y, Cursor);
	}
	else if (ch == '\n')
	{
		if (video_cell(video_x, video_y) == Cursor) // remove cursor
			video_cell(video_x, video_y, Space);
		video_y++;
		if (video_y == Video_y_max)
		{
			video_y--;
			video_shift_line();
		}
		if (video_cell(video_x, video_y) == Space) // set cursor
			video_cell(video_x, video_y, Cursor);
	}
	else  // printable char
	{
		video_cell(video_x, video_y, ch);
		video_x++;
		if (video_x == Video_x_max)
		{
			video_x = Video_x_min;
			video_y++;
		}
		if (video_y == Video_y_max)
		{
			video_y--;
			video_shift_line();
		}
		video_cell(video_x, video_y, Cursor);
	}
}

static void video_puts(const char* str)
{
	while (*str)
	{
		video_putc(*str);
		str++;
	}
}

inline const char* banner()
{
	static const char* b =
		"   _    _ ___ __  __   ___  ___ \r\n"
		"  | |  | | _ \\  \\/  | / _ \\/ __|\r\n"
		"  | |/\\| |   / |\\/| || (_) \\__ \\\r\n"
		"  |__/\\__|_|_\\_|  |_(_)___/|___/\r\n"
		"          From Russia with love!\r\n";
	return b;
}

static void video_print_banner()
{
	// clear banner space
	for (unsigned y=0; y<Video_y_min; ++y)
		for (unsigned x=0; x<Video_x_max; ++x)
			video_cell(x, y, ' ');

	unsigned save_x = video_x;
	unsigned save_y = video_y;
	video_x = Video_x_min;
	video_y = 0;
	video_puts(banner());
	video_cell(video_x, video_y, ' ');  // clear cursor
	video_x = save_x;
	video_y = save_y;
}

static void video_init(uintptr_t video_mem_addr)
{
	video_x = Video_x_min;
	video_y = Video_y_min;
	video_mem = video_mem_addr;

	video_clear_screen();
	video_print_banner();
}
