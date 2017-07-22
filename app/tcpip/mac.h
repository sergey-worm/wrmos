//##################################################################################################
//
//  Mac - ethernet address.
//
//##################################################################################################

#ifndef MAC_H
#define MAC_H

#include <string.h>
#include <stdint.h>

class mac_t
{
	uint8_t _addr[6];

public:

	mac_t() { memset(_addr, 0, 6); }
	mac_t(const uint8_t* a) { memcpy(_addr, a, 6); }

	bool operator == (const mac_t& m) const { return !memcmp(_addr, m.buf(), 6); }
	bool operator != (const mac_t& m) const { return  memcmp(_addr, m.buf(), 6); }

	const uint8_t* buf() const { return (uint8_t*)&_addr; }
	uint8_t*       buf()       { return (uint8_t*)&_addr; }

	bool is_nil() const
	{
		const uint8_t* a = _addr;
		return a[0]==0 && a[1]==0 && a[2]==0 && a[3]==0 && a[4]==0 && a[5]==0;
	}

	bool is_bcast() const
	{
		const uint8_t* a = _addr;
		return a[0]==0xff && a[1]==0xff && a[2]==0xff && a[3]==0xff && a[4]==0xff && a[5]==0xff;
	}

	void set(uint8_t* a)
	{
		memcpy(_addr, a, 6);
	}

	void set_nil()
	{
		uint8_t* a = _addr;
		a[0] = a[1] = a[2] = a[3] = a[4] = a[5] = 0;
	}

	void set_bcast()
	{
		uint8_t* a = _addr;
		a[0] = a[1] = a[2] = a[3] = a[4] = a[5] = 0xff;
	}

	// thread unsafe
	const char* str() const
	{
		enum { Str_num = 3 };
		typedef char string_t [32];
		static string_t strings[Str_num];
		static unsigned id = 0;
		char* s = strings[id];
		id = (id+1)==Str_num ? 0 : (id+1);

		const uint8_t* a = _addr;
		snprintf(s, sizeof(string_t), "%02x:%02x:%02x:%02x:%02x:%02x", a[0], a[1], a[2], a[3], a[4], a[5]);
		s[sizeof(string_t)-1] = '\0';
		return s;
	}
} __attribute__((packed));

#endif // MAC_H
