//##################################################################################################
//
//  Some helpers functions.
//
//##################################################################################################

#ifndef HELPERS_H
#define HELPERS_H

/*
// thread unsafe
static const char* mac2str(const uint8_t* m)
{
	enum { Str_num = 2 };
	typedef char string_t [32];
	static string_t strings[Str_num];
	static unsigned id = 0;
	char* str = strings[id];
	id = (id+1)==Str_num ? 0 : (id+1);

	snprintf(str, sizeof(string_t), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
	str[sizeof(string_t)-1] = '\0';
	return str;
};
*/

// thread unsafe
const char* iaddr2str(const uint8_t* ip)
{
	enum { Str_num = 2 };
	typedef char string_t [32];
	static string_t strings[Str_num];
	static unsigned id = 0;
	char* str = strings[id];
	id = (id+1)==Str_num ? 0 : (id+1);

	snprintf(str, sizeof(string_t), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
	str[sizeof(string_t)-1] = '\0';
	return str;
};

inline const char* iaddr2str(uint32_t ip)
{
	return iaddr2str((uint8_t*)&ip);
}

const char* saddr2str(const sockaddr* saddr)
{
	if (saddr->sa_family != AF_INET)
		return "__unsupported_socket_family__";

	enum { Str_num = 2 };
	typedef char string_t [32];
	static string_t strings[Str_num];
	static unsigned id = 0;
	char* str = strings[id];
	id = (id+1)==Str_num ? 0 : (id+1);

	const sockaddr_in* a = (sockaddr_in*) saddr;
	snprintf(str, sizeof(str)-1, "inet:%s:%d", iaddr2str(a->sin_addr.s_addr), ntohs(a->sin_port));
	str[sizeof(str)-1] = '\0';
	return str;
}

const char* saddr2str(const sockaddr_in* saddr)
{
	return saddr2str((const sockaddr*) saddr);
}

void memswap(void* ptr1, void* ptr2, size_t sz)
{
	uint8_t* p1 = (uint8_t*) ptr1;
	uint8_t* p2 = (uint8_t*) ptr2;
	while (sz--)
	{
		uint8_t b = *p1;
		*p1 = *p2;
		*p2 = b;
		p1++;
		p2++;
	}
}

// compute Internet Checksum (RFC 1071)
// if calc for data+checksum -- result will be 0
uint16_t inet_checksum(uint8_t* data1, size_t len1, uint8_t* data2 = 0, size_t len2 = 0)
{
	register uint32_t sum = 0;

	// data1:  calculate the sum of each 16 bit value
	for (unsigned i=0; i<len1-1; i+=2)
		sum += (data1[i] << 8) + data1[i+1];

	// data1:  add last 8 bit value, if any
	if (len1 & 1)
		sum += data1[len1-1] << 8;

	if (data2 && len2)
	{
		// data2:  calculate the sum of each 16 bit value
		for (unsigned i=0; i<len2-1; i+=2)
			sum += (data2[i] << 8) + data2[i+1];

		// data2:  add last 8 bit value, if any
		if (len2 & 1)
			sum += data2[len2-1] << 8;
	}

	// fold 32-bit sum to 16 bits
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	// flip every bit and return
	return ~sum;
}

#endif // HELPERS_H
