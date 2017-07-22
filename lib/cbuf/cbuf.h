//##################################################################################################
//
//  Circular buffer
//
//##################################################################################################

#ifndef CBUF_H
#define CBUF_H

#include "sys-utils.h"
#include <stdint.h>
#include <string.h>

//--------------------------------------------------------------------------------------------------
class Cbuf_t
{
protected:

	char* _buf;
	size_t _sz;  // capacity = _sz - 1
	size_t _wp;
	size_t _rp;

public:

	Cbuf_t() : _buf(0), _sz(0), _wp(0), _rp(0) {}

	bool init(char* buf, size_t sz)
	{
		if (_buf)
			return false;

		_buf = buf;
		_sz = sz;
		_wp = 0;
		_rp = 0;
		return true;
	}

	// allowed overwriting old data
	size_t overwrite(const char* buf, size_t len)
	{
		if (!_buf || !_sz)
			return 0;                     // not inited

		if (len >= _sz - 1)               // write tail of buf
		{
			size_t offset = len - (_sz - 1);
			memcpy(_buf, buf + offset, _sz - 1);
			_rp = 0;
			_wp += _sz - 1;
		}
		else if (_rp == _wp)              // all buffer free
		{
			size_t l = min(len, _sz - 1);
			memcpy(_buf, buf, l);
			_rp = 0;
			_wp = l;
		}
		else
		{
			size_t l1 = min(len, _sz - _wp);
			memcpy(_buf + _wp, buf, l1);
			bool over = (_wp < _rp)  &&  ((_wp + l1) >= _rp);
			_wp = (_wp + l1) == _sz  ?  0  :  (_wp + l1);
			if (over)
				_rp = (_wp + 1) == _sz  ?  0  :  (_wp + 1);

			if (l1 != len)
			{
				assert(!_wp);
				size_t l2 = len - l1;
				memcpy(_buf, buf + l1, l2);
				over = (_wp + l2) >= _rp;
				_wp = l2;
				if (over)
					_rp = (_wp + 1) == _sz  ?  0  :  (_wp + 1);
			}
		}

		return len;
	}

	// write between _wp and _rp
	size_t write(const char* buf, size_t len)
	{
		if (!_buf || !_sz)
			return 0;                     // not inited

		size_t bytes = 0;

		if (_rp == _wp)                   // all buffer free
		{
			bytes = min(len, _sz - 1);
			memcpy(_buf, buf, bytes);
			_rp = 0;
			_wp = bytes;
		}
		else if (((_wp + 1) == _rp)  ||  (((_wp + 1) == _sz)  &&  !_rp))  // all buffer busy
		{
			bytes = 0;
		}
		else if (_rp > _wp)               // 0---wp___rp---sz
		{
			bytes = min(len, _rp - _wp - 1);
			memcpy(_buf + _wp, buf, bytes);
			_wp += bytes;
		}
		else if (_wp && !_rp)             // rp-------wp___sz
		{
			bytes = min(len, _sz - _wp - 1);
			memcpy(_buf + _wp, buf, bytes);
			_wp += bytes;
		}
		else                              // 0___rp---wp___sz
		{
			bytes = min(len, _sz - _wp);
			memcpy(_buf + _wp, buf, bytes);
			_wp = (_wp + bytes) == _sz  ?  0  :  (_wp + bytes);
			if (bytes != len)
			{
				assert(!_wp);
				size_t l = min(len - bytes, _rp - 1);
				memcpy(_buf, buf + bytes, l);
				_wp = l;
				bytes += l;
			}
		}
		return bytes;
	}

	size_t read(char* buf, size_t len)
	{
		size_t bytes = 0;

		if (_wp == _rp)
		{
			bytes = 0;
		}
		else if (_rp < _wp)              // 0___rp---wp___sz
		{
			bytes = min(len, _wp - _rp);
			memcpy(buf, _buf + _rp, bytes);
			_rp += bytes;
		}
		else                              // 0---wp___rp---sz
		{
			bytes = min(len, _sz - _rp);
			memcpy(buf, _buf + _rp, bytes);
			_rp = (_rp + bytes) == _sz  ?  0  :  (_rp + bytes);
			if (bytes != len)
			{
				assert(!_rp);
				size_t l = min(len - bytes, _wp);
				memcpy(buf + bytes, _buf, l);
				_rp = l;
				bytes += l;
			}
		}
		return bytes;
	}
};

#endif // CBUF_H
