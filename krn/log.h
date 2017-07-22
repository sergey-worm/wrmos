//##################################################################################################
//
//  Kernel log subsystem.
//
//##################################################################################################

#ifndef LOG_H
#define LOG_H

#include "cbuf.h"
#include "kuart.h"

//--------------------------------------------------------------------------------------------------
class Log_buf_t : public Cbuf_t
{
public:

	// get number of strings
	inline int strings()
	{
		if (_rp == _wp)
			return 0;

		size_t rbegin = _wp ? (_wp - 1) : (_sz - 1);
		size_t rend   = _rp ? (_rp - 1) : (_sz - 1);
		size_t cnt    = 0;

		for (unsigned i = rbegin; i != rend; i = i ? (i-1) : (_sz-1))
			if (_buf[i] == '\n')
				cnt++;

		return cnt;
	}

	// get string with number 'num' from the tail of buffer
	inline int get_string(unsigned num, char* str, size_t len)
	{
		if (_rp == _wp)
			return -1;

		// find string

		size_t rbegin = _wp ? (_wp - 1) : (_sz - 1);
		size_t rend   = _rp ? (_rp - 1) : (_sz - 1);
		size_t str_s  = rbegin;
		size_t str_e  = rbegin;
		size_t cnt    = 0;
		bool   found  = false;
		unsigned i;
		unsigned next_i;
		for (i = rbegin; i != rend; i = next_i)
		{
			next_i = i ? (i-1) : (_sz-1);
			if (_buf[i] == '\n'  ||  next_i == rend)
			{
				if (cnt++ == num)
				{
					str_s = i;
					found = true;
					break;
				}
				str_e = i;
			}
		}

		if (!found)
			return -1;

		if (str_s == rbegin)
			return 0;  // num==0 and rbegin=='\n'

		// correct str_s
		if (_buf[str_s] == '\n')
			str_s = (str_s + 1) == _sz  ?  0  :  (str_s + 1);    // skip '\n'
		if (_buf[str_s] == '\r')
			str_s = (str_s + 1) == _sz  ?  0  :  (str_s + 1);    // skip '\r'

		// copy from [str_s, str_e) to buf
		size_t bytes = 0;
		if (str_s == str_e)
		{
			bytes = 0;
		}
		else if (str_s < str_e)
		{
			bytes = min(len, str_e - str_s);
			memcpy(str, _buf + str_s, bytes);
		}
		else
		{
			bytes = min(len, _sz - str_s);
			memcpy(str, _buf + str_s, bytes);
			if (bytes != len)
			{
				size_t l = min(len - bytes, str_e);
				memcpy(str + bytes, _buf, l);
				bytes += l;
			}
		}

		return bytes;
	}
};

//--------------------------------------------------------------------------------------------------
class Log
{
	static Log_buf_t _cbuf;

public:

	static void init(char* buf, size_t sz)
	{
		_cbuf.init(buf, sz);
	}

	static void write(const char* buf, size_t sz)
	{
		_cbuf.overwrite(buf, sz);
	}

	// print line by line via uart
	static void dump(const char* prompt, int lines = -1)
	{
		if (lines == -1)
			lines = _cbuf.strings();

		char str[128];
		for (int i=lines; i>0; --i)
		{
			int len = _cbuf.get_string(i, str, sizeof(str)-1);
			if (len < 0)
				continue;   // line not found

			if (i == 1  &&  !len)
				continue;   // dont print last empty string

			Uart::puts(prompt);
			Uart::putsn(str, len);
			Uart::puts("\r\n");
			Uart::puts("\x1b[0m"); // reset color
		}
	}
};

#endif // LOG_H
