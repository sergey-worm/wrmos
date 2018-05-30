//##################################################################################################
//
//  video.h - allows output to video display, may be use as virtual uart.
//
//##################################################################################################

enum
{
	Video_x_min =  0,       //
	Video_x_max = 80,       //
	Video_y_min =  6,       // reserve space for banner
	Video_y_max = 25,       //

	// last 2 cell is used for storage current x and y
	Video_cur_x_storage_cell = Video_x_max * 1 - 2,
	Video_cur_y_storage_cell = Video_x_max * 1 - 1,

	Cursor = 22,            //
	Space  = ' '            //
};

inline void _video_store_x(unsigned long base_addr, uint16_t x)
{
	uint16_t* vmem = (uint16_t*) base_addr;
	*(vmem + Video_cur_x_storage_cell) = x;
}

inline void _video_store_y(unsigned long base_addr, uint16_t y)
{
	uint16_t* vmem = (uint16_t*) base_addr;
	*(vmem + Video_cur_y_storage_cell) = y;
}

inline uint16_t _video_get_x(unsigned long base_addr)
{
	uint16_t* vmem = (uint16_t*) base_addr;
	return *(vmem + Video_cur_x_storage_cell);
}

inline uint16_t _video_get_y(unsigned long base_addr)
{
	uint16_t* vmem = (uint16_t*) base_addr;
	return *(vmem + Video_cur_y_storage_cell);
}

inline void _video_add_x(unsigned long base_addr, int add)
{
	_video_store_x(base_addr, _video_get_x(base_addr) + add);
}

inline void _video_add_y(unsigned long base_addr, int add)
{
	_video_store_y(base_addr, _video_get_y(base_addr) + add);
}

inline void _video_set_cell(unsigned long base_addr, unsigned x, unsigned y, char ch, uint8_t attr = 0x07)
{
	if (x >= Video_x_max  ||  y >= Video_y_max)
		return;

	int cell = Video_x_max*y + x;
	if (cell == Video_cur_x_storage_cell  ||  cell == Video_cur_y_storage_cell)
		return;

	uint16_t* vmem = (uint16_t*) base_addr;
	*(vmem + cell) = (attr << 8) | ch;
}

inline char _video_get_cell(unsigned long base_addr, unsigned x, unsigned y)
{
	if (x >= Video_x_max  ||  y >= Video_y_max)
		return '@';

	int cell = Video_x_max*y + x;
	if (cell == Video_cur_x_storage_cell  ||  cell == Video_cur_y_storage_cell)
		return 0;

	uint16_t* vmem = (uint16_t*) base_addr;
	return *(vmem + cell) & 0xff;
}

inline void _video_set_cur_cell(unsigned long base_addr, char ch, uint8_t attr = 0x07)
{
	_video_set_cell(base_addr, _video_get_x(base_addr), _video_get_y(base_addr), ch, attr);
}

inline char _video_get_cur_cell(unsigned long base_addr)
{
	return _video_get_cell(base_addr, _video_get_x(base_addr), _video_get_y(base_addr));
}

static void _video_clear_screen(unsigned long base_addr)
{
	for (unsigned y=0; y<Video_y_max; ++y)
		for (unsigned x=0; x<Video_x_max; ++x)
			_video_set_cell(base_addr, x, y, ' ');
}

static void _video_shift_line(unsigned long base_addr)
{
	// shift all buffer up
	for (unsigned y=Video_y_min; y<Video_y_max-1; ++y)
		for (unsigned x=Video_x_min; x<Video_x_max; ++x)
			_video_set_cell(base_addr, x, y, _video_get_cell(base_addr, x, y + 1));

	// clear bottom line
	for (unsigned x=0; x<Video_x_max; ++x)
		_video_set_cell(base_addr, x, Video_y_max - 1, ' ');
}

static inline void video_putc(unsigned long base_addr, char ch)
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
		if (_video_get_cur_cell(base_addr) == Cursor) // remove cursor
			_video_set_cur_cell(base_addr, Space);
		_video_store_x(base_addr, Video_x_min);
		if (_video_get_cur_cell(base_addr) == Space) // set cursor
			_video_set_cur_cell(base_addr, Cursor);
	}
	else if (ch == '\n')
	{
		if (_video_get_cur_cell(base_addr) == Cursor) // remove cursor
			_video_set_cur_cell(base_addr, Space);
		_video_add_y(base_addr, +1);
		if (_video_get_y(base_addr) == Video_y_max)
		{
			_video_add_y(base_addr, -1);
			_video_shift_line(base_addr);
		}
		if (_video_get_cur_cell(base_addr) == Space) // set cursor
			_video_set_cur_cell(base_addr, Cursor);
	}
	else  // printable char
	{
		_video_set_cur_cell(base_addr, ch);
		_video_add_x(base_addr, +1);
		if (_video_get_x(base_addr) == Video_x_max)
		{
			_video_store_x(base_addr, Video_x_min);
			_video_add_y(base_addr, +1);
		}
		if (_video_get_y(base_addr) == Video_y_max)
		{
			_video_add_y(base_addr, -1);
			_video_shift_line(base_addr);
		}
		_video_set_cur_cell(base_addr, Cursor);
	}
}

inline void _video_puts(unsigned long base_addr, const char* str)
{
	while (*str)
	{
		video_putc(base_addr, *str);
		str++;
	}
}

inline const char* _banner()
{
	static const char* b =
		"   _    _ ___ __  __   ___  ___ \r\n"
		"  | |  | | _ \\  \\/  | / _ \\/ __|\r\n"
		"  | |/\\| |   / |\\/| || (_) \\__ \\\r\n"
		"  |__/\\__|_|_\\_|  |_(_)___/|___/\r\n"
		"          From Russia with love!\r\n";
	return b;
}

static inline void _video_print_banner(unsigned long base_addr)
{
	// clear banner space
	for (unsigned y=0; y<Video_y_min; ++y)
		for (unsigned x=0; x<Video_x_max; ++x)
			_video_set_cell(base_addr, x, y, ' ');

	unsigned save_x = _video_get_x(base_addr);
	unsigned save_y = _video_get_y(base_addr);
	_video_store_x(base_addr, Video_x_min);
	_video_store_y(base_addr, 0);
	_video_puts(base_addr, _banner());
	_video_set_cell(base_addr, Video_x_min, 0, ' ');  // clear cursor
	_video_store_x(base_addr, save_x);
	_video_store_y(base_addr, save_y);
}

static inline void video_init(unsigned long base_addr)
{
	_video_store_x(base_addr, Video_x_min);
	_video_store_y(base_addr, Video_y_min);
	_video_clear_screen(base_addr);
	_video_print_banner(base_addr);
}
