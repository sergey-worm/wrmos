//##################################################################################################
//
//  Packets - network packets for Eth, Icmp, Ip, Tcp, Udp, ...
//
//##################################################################################################

#ifndef PACKETS_H
#define PACKETS_H

#include "helpers.h"
#include "mac.h"

// helper
static const char* deep2str(size_t deep)
{
	switch (deep)
	{
		case 0:   return "|";
		case 1:   return "|  ";
		case 2:   return "|    ";
		case 3:   return "|      ";
		case 4:   return "|        ";
		case 5:   return "|          ";
		case 6:   return "|            ";
		case 7:   return "|              ";
		case 8:   return "|                ";
		case 9:   return "|                  ";
		case 10:  return "|                    ";
	}
	return "__unsupported_deep__";
}

//--------------------------------------------------------------------------------------------------
struct Tcp_packet_t
{
	uint16_t src;               // src port
	uint16_t dst;               // dst port
	uint32_t seq;               // seq number
	uint32_t ack;               // ack number
	struct
	{
		unsigned hlen     : 4;  //
		unsigned reserved : 3;  //
		unsigned flags    : 9;  //
	} __attribute__((packed));
	uint16_t win_sz;            //
	uint16_t checksum;          // header+payload checksum
	uint16_t urgent_ptr;        //

private:

	const char* flags2str() const
	{
		static char str[40];
		int res = snprintf(str, sizeof(str)-1, "%s%s%s%s%s%s%s%s%s",
			flags & Ns  ? "ns "  : "",  flags & Cwr ? "cwr " : "",  flags & Ece ? "ece " : "",
			flags & Urg ? "urg " : "",  flags & Ack ? "ack " : "",  flags & Psh ? "psh " : "",
			flags & Rst ? "rst " : "",  flags & Syn ? "syn " : "",  flags & Fin ? "fin " : "");
		if (res < 0)
			return "__flags2str_error__";
		if ((unsigned)res > sizeof(str)-1)
			str[sizeof(str)-1] = '\0';  // overflow, add terminator
		else if (res)
			str[strlen(str)-1] = '\0';  // remove last space
		return str;
	}

public:

	enum
	{
		Length_min         = 20,   // header without options and payload
		Typically_opts_len = 20,   // typically options length
		Typically_hdr_len  = Length_min + Typically_opts_len
	};

	enum flag_t
	{
		Ns  = 1 << 8,  // Ecn-nonce concealment protection (experimental: see RFC 3540)
		Cwr = 1 << 7,  // congestion Window Reduced
		Ece = 1 << 6,  // Ecn-echo has a dual role, depending on SYN flag
		Urg = 1 << 5,  // indicates that the Urgent pointer field is significant
		Ack = 1 << 4,  // indicates that the Acknowledgment field is significant
		Psh = 1 << 3,  // push function
		Rst = 1 << 2,  // reset the connection
		Syn = 1 << 1,  // synchronize sequence numbers
		Fin = 1 << 0,  // no more data from sender
	};

	enum opt_t
	{
		Opt_end        = 0,
		Opt_nop        = 1,
		Opt_mss        = 2,
		Opt_win_scale  = 3,
		Opt_sack_perm  = 4,
		Opt_sack       = 5,
		Opt_timestamps = 8
	};

	const uint8_t* optptr(size_t offs) const
	{
		assert(offs < (hdrlen() - Length_min));
		return (uint8_t*)this + Length_min + offs;
	}

	uint8_t* optptr(size_t offs)
	{
		assert(offs < (hdrlen() - Length_min));
		return (uint8_t*)this + Length_min + offs;
	}

	uint8_t optbyte(size_t offs) const
	{
		return *optptr(offs);
	}

public:

	inline size_t  hdrlen()      const { return hlen * sizeof(uint32_t); }
	const uint8_t* payload()     const { return (uint8_t*)this + hdrlen(); }
	uint8_t*       payload()           { return (uint8_t*)this + hdrlen(); }
	void payload(const void* d, size_t l) { memcpy(payload(), d, l); }

	uint16_t calc_checksum(uint16_t tcplen, uint32_t ipsrc, uint32_t ipdst, uint8_t proto) const
	{
		uint32_t pseudo_hdr[3] = { ipsrc, ipdst, ((uint32_t)proto << 16) + tcplen };
		return inet_checksum((uint8_t*)this, tcplen, (uint8_t*)&pseudo_hdr, sizeof(pseudo_hdr));
	}

	void dump(size_t deep, size_t len, uint32_t ipsrc, uint32_t ipdst, uint8_t proto) const
	{
		const char* space = deep2str(deep);
		wrm_logd("%s TCP packet (%u):\n", space, len);
		if (len < hlen)
		{
			wrm_loge("%s wrong length:  ip_pload=%u, tcp_hlen=%u.\n", space, len, hlen);
			return;
		}
		const char* chsum_res = calc_checksum(len, ipsrc, ipdst, proto) ?
		                        "\x1b[0;33mincorrect\x1b[0m" : "\x1b[0;32mcorrect\x1b[0m";
		wrm_logd("%s src_port:    %u\n",          space, src);
		wrm_logd("%s dst_port:    %u\n",          space, dst);
		wrm_logd("%s seq:         %u %s\n",       space, seq, flags&Syn?"/ relative 0":"");
		wrm_logd("%s ack:         %u %s\n",       space, ack, flags&Ack?"/ relative 0":"");
		wrm_logd("%s hlen:        %u\n",          space, hdrlen());
		wrm_logd("%s flags:       0x%x (%s)\n",   space, flags, flags2str());
		wrm_logd("%s win_sz:      %u\n",          space, win_sz);
		wrm_logd("%s checksum:    0x%04x / %s\n", space, checksum, chsum_res);
		wrm_logd("%s urgent_ptr:  %u\n",          space, urgent_ptr);
		wrm_logd("%s options (%u bytes):\n",      space, hdrlen() - Length_min);
		// parse options
		size_t rest = hdrlen() - Length_min;
		size_t offs = 0;
		bool   end  = false;
		while (rest > 0  &&  !end)
		{
			int kind  = optbyte(offs); 
			int oplen = optbyte(offs + 1);
			switch (kind)
			{
				case Opt_end:
				{
					wrm_logd("%s : opt_end\n", space);
					oplen = 1;
					end = true;
					break;
				}
				case Opt_nop:
				{
					wrm_logd("%s : opt_nop\n", space);
					oplen = 1;
					break;
				}
				case Opt_mss:
				{
					int mss = (optbyte(offs + 2) << 8) + optbyte(offs + 3);
					wrm_logd("%s : opt_mss:         %u\n", space, mss);
					assert(oplen == 4);
					break;
				}
				case Opt_win_scale:
				{
					int scale = optbyte(offs + 2);
					wrm_logd("%s : opt_win_scale:   %u\n", space, scale);
					assert(oplen == 3);
					break;
				}
				case Opt_sack_perm:
				{
					wrm_logd("%s : opt_sack_perm\n", space);
					assert(oplen == 2);
					break;
				}
				case Opt_timestamps:
				{
					const uint8_t* b = optptr(offs + 2);
					int tsval = (b[0]<<24) + (b[1]<<16) + (b[2]<<8) + b[3];
					int tsecr = (b[4]<<24) + (b[5]<<16) + (b[6]<<8) + b[7];
					wrm_logd("%s : opt_timestamps:  tsval=%u, tsecr=%u\n", space, tsval, tsecr);
					assert(oplen == 10);
					break;
				}
				default:
					wrm_logw("%s : unknown option:  kind=%d, len=%d\n", space, kind, oplen);
			}
			rest -= oplen;
			offs += oplen;
			if (rest < 0)
				wrm_loge("%s   options are corrupted.\n", space);
		}
		wrm_logd("%s payload (%u):\n",     space, len - hdrlen());
	}

	// find option by kind
	uint8_t* opt(int kind)
	{
		// TODO:  check options length !!!
		size_t rest = hdrlen() - Length_min;
		size_t offs = 0;
		while (rest > 0)
		{
			uint8_t* opt = optptr(offs);
			int knd = opt[0];

			if (knd == kind)
				return opt;

			if (knd == Opt_end)
				break;

			int oplen  = knd == Opt_nop ? 1 : opt[1];
			rest -= oplen;
			offs += oplen;

			if (rest < 0)
				wrm_loge("find op:  options are corrupted.\n");
		}
		return 0;
	}
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct Udp_packet_t
{
	uint16_t src;        //
	uint16_t dst;        //
	uint16_t length;     // payload length
	uint16_t checksum;   // header+payload checksum

	enum
	{
		Length_min = 8
	};

public:

	inline static size_t hdrlen()      { return 8; }
	size_t         payload_len() const { return length - hdrlen(); }
	const uint8_t* payload()     const { return (uint8_t*)this + hdrlen(); }
	uint8_t*       payload()           { return (uint8_t*)this + hdrlen(); }
	void payload(const void* d, size_t l) { memcpy(payload(), d, l); }

	// calculate packet checksum
	uint16_t calc_checksum(uint32_t ipsrc, uint32_t ipdst, uint8_t proto) const
	{
		uint32_t pseudo_hdr[3] = { ipsrc, ipdst, ((uint32_t)proto << 16) + length };
		return inet_checksum((uint8_t*)this, length, (uint8_t*)&pseudo_hdr, sizeof(pseudo_hdr));
	}
	//uint16_t calc_checksum() const
	//{
	//	return inet_checksum((uint8_t*)this, length);
	//}

	void dump(size_t deep, size_t len, uint32_t ipsrc, uint32_t ipdst, uint8_t proto) const
	{
		const char* space = deep2str(deep);
		wrm_logd("%s UDP packet (%u):\n", space, len);
		if (len < length)
		{
			wrm_loge("%s wrong length:  ip_pload=%u, udp_len=%u.\n", space, len, length);
			return;
		}
		const char* chsum_res = calc_checksum(ipsrc, ipdst, proto) ?
		                        "\x1b[0;33mincorrect\x1b[0m" : "\x1b[0;32mcorrect\x1b[0m";
		wrm_logd("%s src_port:    %u\n", space, src);
		wrm_logd("%s dst_port:    %u\n", space, dst);
		wrm_logd("%s length:      %u\n", space, length);
		wrm_logd("%s checksum:    0x%04x / %s\n", space, checksum, chsum_res);
		wrm_logd("%s payload (%u):\n",   space, payload_len());
	}
} __attribute__((packed));

struct Ip_packet_t;
void ip_packet_dump(const Ip_packet_t* ip, size_t deep, size_t len);

//--------------------------------------------------------------------------------------------------
// Destination unreachable message
struct Icmp_dest_unreach_t
{
	uint32_t unused;       //

	enum
	{
		Length_min = 4 + /*Ip_packet_t::Length_min*/ 20 // FIXME
	};

public:

	static inline size_t hdrlen()  { return 4; }
	const uint8_t* payload() const { return (uint8_t*)this + hdrlen(); }
	uint8_t*       payload()       { return (uint8_t*)this + hdrlen(); }
	void payload(const void* d, size_t l) { memcpy(payload(), d, l); }

	void dump(size_t deep, size_t len) const
	{
		const char* space = deep2str(deep);
		wrm_logd("%s Dest unreach packet (%u):\n", space, len);
		if (len < Length_min)
		{
			wrm_loge("%s too small dest-unreach packet:  %u bytes.\n", space, len);
			return;
		}
		size_t pload_len = len - hdrlen();
		wrm_logd("%s payload (%u):\n", space, pload_len);
		const Ip_packet_t* ip = (Ip_packet_t*) payload();
		ip_packet_dump(ip, deep + 1, pload_len);  //ip->dump(deep + 1, pload_len);
	}
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
// echo-request oe echo-reply
struct Icmp_echo_t
{
	uint16_t identifier;       //
	uint16_t sequence_number;  //
	uint64_t timestamp;        // is mandatory ?

	enum
	{
		Length_min = 4
	};

public:

	static inline size_t hdrlen()  { return 4+8; }
	const uint8_t* payload() const { return (uint8_t*)this + hdrlen(); }
	uint8_t*       payload()       { return (uint8_t*)this + hdrlen(); }
	void payload(const void* d, size_t l) { memcpy(payload(), d, l); }

	void dump(size_t deep, size_t len) const
	{
		const char* space = deep2str(deep);
		wrm_logd("%s Echo packet (%u):\n", space, len);
		if (len < Length_min)
		{
			wrm_loge("%s too small echo packet:  %u bytes.\n", space, len);
			return;
		}
		size_t pload_len = len - hdrlen();
		wrm_logd("%s ident:       %u\n",   space, identifier);
		wrm_logd("%s seq_num:     %u\n",   space, sequence_number);
		wrm_logd("%s timestamp:   %llu\n", space, timestamp);
		wrm_logd("%s payload (%u):\n",     space, pload_len);
	}
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct Icmp_packet_t
{
	uint8_t  type;      //
	uint8_t  code;      //
	uint16_t checksum;  // header+payload checksum

	enum
	{
		Type_echo_reply   = 0,
		Type_dest_unreach = 3,
		Type_echo_request = 8,
	};

	enum
	{
		Code_dst_net_unreach       =  0,  // Destination network unreachable
		Code_dst_host_unreach      =  1,  // Destination host unreachable
		Code_dst_proto_unreach     =  2,  // Destination protocol unreachable
		Code_dst_port_inreach      =  3,  // Destination port unreachable
		Code_fragm_required_and_df =  4,  // Fragmentation required, and DF flag set
		Code_src_route_failed      =  5,  // Source route failed
		Code_dst_net_unknown       =  6,  // Destination network unknown
		Code_dst_host_unknown      =  7,  // Destination host unknown
		Code_src_host_isolate      =  8,  // Source host isolated
		Code_net_adm_prohibited    =  9,  // Network administratively prohibited
		Code_host_adm_prohibited   = 10,  // Host administratively prohibited
		Code_net_unreach_for_tos   = 11,  // Network unreachable for ToS
		Code_host_unreach_for_tos  = 12,  // Host unreachable for ToS
		Code_comm_adm_prohibited   = 13,  // Communication administratively prohibited
		Code_host_prec_violation   = 14,  // Host Precedence Violation
		Code_prec_cutof_in_effect  = 15,  // Precedence cutoff in effect
	};

	enum
	{
		Length_min = 4
	};

private:

	static const char* type2str(int type)
	{
		switch (type)
		{
			case Type_echo_reply:    return "echo_reply";
			case Type_dest_unreach:  return "dest_unreach";
			case Type_echo_request:  return "echo_request";
		}
		return "__unknown_icmp_type__";
	}
	static const char* code2str(int type, int code)
	{
		switch (type)
		{
			case Type_echo_reply:    return "echo_reply";
			case Type_dest_unreach:
			{
				switch (type)
				{
					case Code_dst_net_unreach:        return "dst_net_unreach";
					case Code_dst_host_unreach:       return "dst_host_unreach";
					case Code_dst_proto_unreach:      return "dst_proto_unreach";
					case Code_dst_port_inreach:       return "dst_port_inreach";
					case Code_fragm_required_and_df:  return "fragm_required_and_df";
					case Code_src_route_failed:       return "src_route_failed";
					case Code_dst_net_unknown:        return "dst_net_unknown";
					case Code_dst_host_unknown:       return "dst_host_unknown";
					case Code_src_host_isolate:       return "src_host_isolate";
					case Code_net_adm_prohibited:     return "net_adm_prohibited";
					case Code_host_adm_prohibited:    return "host_adm_prohibited";
					case Code_net_unreach_for_tos:    return "net_unreach_for_tos";
					case Code_host_unreach_for_tos:   return "host_unreach_for_tos";
					case Code_comm_adm_prohibited:    return "comm_adm_prohibited";
					case Code_host_prec_violation:    return "host_prec_violation";
					case Code_prec_cutof_in_effect:   return "prec_cutof_in_effect";
				}
			}
			case Type_echo_request:  return "echo_request";
		}
		return "__unknown_icmp_code__";
	}

public:

	static inline size_t hdrlen()  { return 4; }
	const uint8_t* payload() const { return (uint8_t*)this + hdrlen(); }

	// calculate packet checksum
	uint16_t calc_checksum(size_t pktlen) const
	{
		return inet_checksum((uint8_t*)this, pktlen);
	}

	void dump(size_t deep, size_t len) const
	{
		const char* space = deep2str(deep);
		wrm_logd("%s ICMP packet (%u):\n", space, len);
		if (len < Length_min)
		{
			wrm_loge("%stoo small icmp packet:  %u bytes.\n", space, len);
			return;
		}
		const char* chsum_res = calc_checksum(len) ? "\x1b[0;33mincorrect\x1b[0m" : "\x1b[0;32mcorrect\x1b[0m"; 
		size_t pload_len = len - hdrlen();
		wrm_logd("%s type:      %u / %s\n",            space, type, type2str(type));
		wrm_logd("%s code:      %u / %s\n",            space, code, code2str(type, code));
		wrm_logd("%s checksum:  0x%04x / %s\n", space, checksum, chsum_res);
		wrm_logd("%s payload (%u):\n", space, pload_len);
		switch (type)
		{
			case Type_echo_request:
			case Type_echo_reply:
			{
				const Icmp_echo_t* echo = (Icmp_echo_t*) payload();
				echo->dump(deep + 1, pload_len);
				break;
			}
			case Type_dest_unreach:
			{
				const Icmp_dest_unreach_t* dest_unreach = (Icmp_dest_unreach_t*) payload();
				dest_unreach->dump(deep + 1, pload_len);
				break;
			}
			default:
				wrm_loge("%s unsupported icmp type %u.\n", space, type);
		}
	}
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct Ip_packet_t
{
	struct
	{
		unsigned version         :  4;  // IPv4 or IPv6
		unsigned header_length   :  4;  // internet header length
		unsigned dscp            :  6;  // differentiated Services Code Point
		unsigned ecn             :  2;  // explicit congestion notification
		unsigned total_length    : 16;  // header + data, in bytes, >= 20
		unsigned identification  : 16;  //
		unsigned flag0_reserved  :  1;  //
		unsigned flag1_df        :  1;  // don't fragment
		unsigned flag2_mf        :  1;  // more fragments
		unsigned fragment_offset : 13;  //
	};
	uint8_t  time_to_live;              //
	uint8_t  protocol;                  //
	uint16_t header_checksum;           //
	uint32_t src;                       //
	uint32_t dst;                       //

	enum
	{
		Proto_icmp =  1,
		Proto_tcp  =  6,
		Proto_udp  = 17,
	};

	enum
	{
		Typically_hdr_len = 20,
		Length_min = 20
	};

private:

	static const char* proto2str(int proto)
	{
		switch (proto)
		{
			case Proto_icmp:    return "icmp";
			case Proto_tcp:     return "tcp";
			case Proto_udp:     return "udp";
		}
		return "__unknown_ip_proto__";
	}

public:

	inline size_t  hdrlen()  const { return header_length * 4; }
	const uint8_t* payload() const { return (uint8_t*)this + hdrlen(); }

	// calculate header checksum
	uint16_t calc_checksum() const
	{
		return inet_checksum((uint8_t*)this, hdrlen());
	}

	void dump(size_t deep, size_t len) const
	{
		const char* space = deep2str(deep);
		wrm_logd("%s IP packet (%u):\n", space, len);
		if (len < Length_min  ||  len < total_length)
		{
			wrm_loge("%s too small IP packet:  %u bytes (tlen=%u).\n", space, len, total_length);
			return;
		}
		const char* chsum_res = calc_checksum() ? "\x1b[0;33mincorrect\x1b[0m" : "\x1b[0;32mcorrect\x1b[0m";
		size_t pload_len = total_length - hdrlen();
		wrm_logd("%s version:  %u\n",                 space, version);
		wrm_logd("%s hlen:     %u\n",                 space, hdrlen());
		wrm_logd("%s dscp:     0x%02x\n",             space, dscp);
		wrm_logd("%s ecn:      %u\n",                 space, ecn);
		wrm_logd("%s tlen:     %u\n",                 space, total_length);
		wrm_logd("%s ident:    0x%04x/%u\n",          space, identification, identification);
		wrm_logd("%s flags:    don't_fragm=%d, more_fragms=%d\n", space, flag1_df, flag2_mf);
		wrm_logd("%s foffset:  %u\n",                 space, fragment_offset);
		wrm_logd("%s ttl:      %u\n",                 space, time_to_live);
		wrm_logd("%s proto:    %u/%s\n",              space, protocol, proto2str(protocol));
		wrm_logd("%s hchsum:   0x%04x / %s\n",        space, header_checksum, chsum_res);
		wrm_logd("%s src:      %s\n",                 space, iaddr2str(src));
		wrm_logd("%s dst:      %s\n",                 space, iaddr2str(dst));
		wrm_logd("%s payload (%u):\n",                space, pload_len);
		switch (protocol)
		{
			case Ip_packet_t::Proto_icmp:
			{
				const Icmp_packet_t* icmp = (Icmp_packet_t*) payload();
				icmp->dump(deep + 1, pload_len);
				break;
			}
			case Ip_packet_t::Proto_tcp:
			{
				const Tcp_packet_t* tcp = (Tcp_packet_t*) payload();
				tcp->dump(deep + 1, pload_len, src, dst, protocol);
				break;
			}
			case Ip_packet_t::Proto_udp:
			{
				const Udp_packet_t* udp = (Udp_packet_t*) payload();
				udp->dump(deep + 1, pload_len, src, dst, protocol);
				break;
			}
			default:
				wrm_loge("%s unsupported IP protocol  %u.\n", space, protocol);
		}
	}
} __attribute__((packed));

void ip_packet_dump(const Ip_packet_t* ip, size_t deep, size_t len)
{
	ip->dump(deep, len);
}

//--------------------------------------------------------------------------------------------------
struct Arp_packet_t
{
	uint16_t htype;  // hardware type
	uint16_t ptype;  // protocol type
	uint8_t  hlen;   // hardware address length
	uint8_t  plen;   // protocol address length
	uint16_t oper;   // operation
	mac_t    sha;    // sender hardware address
	uint32_t spa;    // sender protocol address
	mac_t    tha;    // target hardware address
	uint32_t tpa;    // target protocol address

	enum
	{
		Opcode_req = 1,
		Opcode_rep = 2
	};

	static inline size_t size() { return sizeof(Arp_packet_t); }

	void dump(size_t deep, size_t len) const
	{
		const char* space = deep2str(deep);
		wrm_logd("%s ARP packet (%u):\n", space, len);
		if (len < size())
		{
			wrm_loge("%s too small ARP packet:  %u bytes.\n", space, len);
			return;
		}
		wrm_logd("%s htype:  0x%04x\n", space, htype);
		wrm_logd("%s ptype:  0x%04x\n", space, ptype);
		wrm_logd("%s hlen:     % 4u\n", space, hlen);
		wrm_logd("%s plen:     % 4u\n", space, plen);
		wrm_logd("%s oper:     % 4u (%s)\n", space, oper, oper==Opcode_req?"request":"reply");
		wrm_logd("%s sha:    %s\n", space, sha.str());
		wrm_logd("%s spa:    %s\n", space, iaddr2str(spa));
		wrm_logd("%s tha:    %s\n", space, tha.str());
		wrm_logd("%s tpa:    %s\n", space, iaddr2str(tpa));
	}
} __attribute__((packed));

//--------------------------------------------------------------------------------------------------
struct Eth_frame_t
{
	mac_t    dst;
	mac_t    src;
	uint16_t etype;

	enum
	{
		Etype_ip  = 0x0800,
		Etype_arp = 0x0806
	};

	static inline size_t  hdrlen()        { return 14; }
	inline uint8_t*       payload()       { return (uint8_t*)this + hdrlen(); }
	inline const uint8_t* payload() const { return (uint8_t*)this + hdrlen(); }
	void payload(const uint8_t* data, size_t len) { memcpy(payload(), data, len); }

	void dump(size_t deep, size_t len) const
	{
		const char* space = deep2str(deep);
		wrm_logd("%s ETH frame (%u):\n", space, len);
		if (len < sizeof(Eth_frame_t))
		{
			wrm_loge("%s too small ETH frame:  %u bytes.\n", space, len);
			return;
		}
		size_t pload_len = len - hdrlen();
		wrm_logd("%s mac_dst:  %s\n",     space, dst.str());
		wrm_logd("%s mac_src:  %s\n",     space, src.str());
		wrm_logd("%s ethtype:  0x%04x\n", space, etype);
		wrm_logd("%s payload (%u):\n",    space, pload_len);
		switch (etype)
		{
			case Etype_ip:
			{
				Ip_packet_t* ip = (Ip_packet_t*) payload();
				ip->dump(deep + 1, pload_len);
				break;
			}
			case Etype_arp:
			{
				Arp_packet_t* arp = (Arp_packet_t*) payload();
				arp->dump(deep + 1, pload_len);
				break;
			}
			default:
				wrm_logw("%s unknown payload.\n", space);
		}
	}
} __attribute__((packed));

#endif // PACKETS_H
