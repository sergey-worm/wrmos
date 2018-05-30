//##################################################################################################
//
//  Arch_data - arch specifics kernel data.
//
//##################################################################################################

#ifndef ARCH_DATA_H
#define ARCH_DATA_H

#include "wlibc_assert.h"
#include <string.h>

class Io_bitmap_t
{
	enum
	{
		Io_bitmap_bits  = 0x10000,
		Io_bitmap_bytes = Io_bitmap_bits / 8,
		Io_bitmap_longs = Io_bitmap_bytes / sizeof(long),
		Bits_per_long   = sizeof(long) * 8
	};

	long _bitmap[Io_bitmap_longs + 1]; // last extra byte must be 0xff
	unsigned _begin;                   // begin of range with enabled ports
	unsigned _end;                     // end   of range with enabled ports

public:

	Io_bitmap_t()
	{
		init();
	}

	void init()
	{
		// by default all ports are disabled
		memset(_bitmap, 0xff, sizeof(_bitmap));
		_begin = 0;
		_end = 0;
	}

	void dump(const char* mark, unsigned longs = 0)
	{
		force_printk_uart("Dump (%s):\n", mark);
		if (!longs)
			longs = Io_bitmap_longs;
		for (unsigned l=0; l<longs; l+=4)
		{
			force_printk_uart("%4u:  0x%016lx 0x%016lx 0x%016lx 0x%016lx.\n",
				l, _bitmap[l+0], _bitmap[l+1], _bitmap[l+2], _bitmap[l+3]);
		}
	}

	#define get_max(a, b) ((a)>(b)?(a):(b))
	#define get_min(a, b) ((a)<(b)?(a):(b))

	void set_ioperm_all(int enable)
	{
		memset(_bitmap, enable ? 0x00 : 0xff, Io_bitmap_bytes);
		_begin = 0;
		_end = Io_bitmap_bits;
	}

	void set_ioperm(unsigned port, unsigned sz, int enable)
	{
		wassert(port < Io_bitmap_bits  &&  (port+sz) <= Io_bitmap_bits);
		wassert(sz);
		if (enable)
		{
			// clear bits
			for (unsigned p=port; p<port+sz; ++p)
				_bitmap[p/Bits_per_long] &= ~(1ul << (p%Bits_per_long));

			// update min and max
			_begin = get_min(_begin, port);
			_end = get_max(_end, port+sz);
		}
		else
		{
			// set bits
			for (unsigned p=port; p<port+sz; ++p)
				_bitmap[p/Bits_per_long] |= 1ul << (p%Bits_per_long);

			// update min and max
			if (_end)
			{
				// update _begin - find first clear bit
				for (unsigned p=_begin; p<_end; ++p)
				{
					if (!(_bitmap[p/Bits_per_long] & (1ul << (p%Bits_per_long))))
					{
						_begin = p;
						break;
					}
				}

				// update _end - find last clear bit
				for (unsigned p=_end-1; p>=_begin; --p)
				{
					if (!(_bitmap[p/Bits_per_long] & (1ul << (p%Bits_per_long))))
					{
						_end = p;
						break;
					}
				}

				if (_begin == _end)
					_begin = _end = 0;
			}
		}
	}

	int is_ioperm(unsigned port, unsigned sz, int enable)
	{
		wassert(port < Io_bitmap_bits  &&  (port+sz) < Io_bitmap_bits);
		wassert(sz);
		if (enable)
		{
			if (port >= _end  ||  (port+sz) < _begin)
				return false; // outside range always disabled
			for (unsigned p=port; p<port+sz; ++p)
				if (_bitmap[p/Bits_per_long] & (1ul << (p%Bits_per_long)))
					return false;
		}
		else
		{
			if (port >= _end  ||  (port+sz) < _begin)
				return true; // outsude range always disabled
			for (unsigned p=port; p<port+sz; ++p)
				if (!(_bitmap[p/Bits_per_long] & (1ul << (p%Bits_per_long))))
					return false;
		}
		return true;
	}

	void set_from(const Io_bitmap_t* src)
	{
		bool need_clear = false;
		bool need_copy = false;
		if (!_end && !src->_end) // dst and dst are empty - nothing to do
		{
			return;
		}
		else if (!_end)          // dst is empty - copy src to dst
		{
			need_copy = true;
		}
		else if (!src->_end)     // src is empty - clear dst
		{
			need_clear = true;
		}
		else                     // src and dst aren't empty - clear and copy
		{
			need_clear = need_copy = true;
		}

		if (need_clear)
		{
			unsigned begin_long = _begin / Bits_per_long;
			unsigned end_long = (_end - 1) / Bits_per_long + 1;
			for (unsigned l=begin_long; l<end_long; ++l)
				_bitmap[l] = ~0ul;
			_begin = _end = 0;
		}

		if (need_copy)
		{
			_begin = src->_begin;
			_end = src->_end;
			unsigned begin_long = _begin / Bits_per_long;
			unsigned end_long = (_end - 1) / Bits_per_long + 1;
			for (unsigned l=begin_long; l<end_long; ++l)
				_bitmap[l] = src->_bitmap[l];
		}
	}
};

struct Arch_task_t
{
	Io_bitmap_t iomap;

	inline void set_ioperm_all(int enable)
	{
		iomap.set_ioperm_all(enable);
	}

	inline void set_ioperm(unsigned port, unsigned sz, int enable)
	{
		iomap.set_ioperm(port, sz, enable);
	}

	inline int is_ioperm(unsigned port, unsigned sz, int enable)
	{
		return iomap.is_ioperm(port, sz, enable);
	}

	inline void set_current()
	{
		void set_cur_ioperm(const Io_bitmap_t* src);
		set_cur_ioperm(&iomap);
	}
};

#endif // ARCH_DATA_H
