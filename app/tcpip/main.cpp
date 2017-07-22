//##################################################################################################
//
//  tcpip - implementation of TCP/IP protocols as single application.
//
//##################################################################################################

#include "l4api.h"
#include "l4ipcerr.h"
#include "wrmos.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "panic.h"
#include "list.h"
#include "psocket_opcodes.h"

// posix sockets
#include <netinet/in.h>

#include "packets.h"
#include "helpers.h"
#include "socket.h"

//##################################################################################################
//  NETWORK PART
//##################################################################################################
//##################################################################################################
//  Net ifaces
//##################################################################################################
//##################################################################################################
//--------------------------------------------------------------------------------------------------
class Eth_iface_t
{
	mac_t _addr;

public:

	Eth_iface_t(const mac_t* mac) { _addr = *mac; }
	const mac_t addr() const { return _addr; }
};

//--------------------------------------------------------------------------------------------------
class Eth_ifaces_t
{
	typedef list_t<Eth_iface_t, 1> eth_ifaces_t;
	eth_ifaces_t _eth_ifaces;

public:

	Eth_iface_t* add(const mac_t* mac)
	{
		_eth_ifaces.push_back(Eth_iface_t(mac));
		return &_eth_ifaces.back();
	}

	Eth_iface_t* find(const uint8_t* mac)
	{
		for (eth_ifaces_t::iter_t it=_eth_ifaces.begin(); it!=_eth_ifaces.end(); ++it)
			if ((it->addr() == mac))
				return &*it;
		return 0;
	}
};

//##################################################################################################
//--------------------------------------------------------------------------------------------------
class Ip_iface_t
{
	const Eth_iface_t* _ethif;
	uint32_t           _addr;
	uint32_t           _mask;

public:

	Ip_iface_t(uint32_t addr, uint32_t mask, const Eth_iface_t* eth) :
		_ethif(eth), _addr(addr), _mask(mask) {}

	uint32_t addr()            const { return _addr;  }
	uint32_t mask()            const { return _mask;  }
	const Eth_iface_t* ethif() const { return _ethif; }
};

//--------------------------------------------------------------------------------------------------
class Ip_ifaces_t
{
	typedef list_t<Ip_iface_t, 8> ip_ifaces_t;
	ip_ifaces_t _ip_ifaces;

public:

	Ip_iface_t* add(uint32_t addr, uint32_t mask, Eth_iface_t* eth)
	{
		_ip_ifaces.push_back(Ip_iface_t(addr, mask, eth));
		return &_ip_ifaces.back();
	}

	Ip_iface_t* find(uint32_t addr)
	{
		for (ip_ifaces_t::iter_t it=_ip_ifaces.begin(); it!=_ip_ifaces.end(); ++it)
			if (it->addr() == addr)
				return &*it;
		return 0;
	}

	Ip_iface_t* find_masked(uint32_t addr)
	{
		for (ip_ifaces_t::iter_t it=_ip_ifaces.begin(); it!=_ip_ifaces.end(); ++it)
			if ((it->addr() & it->mask())  ==  (addr & it->mask()))
				return &*it;
		return 0;
	}
};

//##################################################################################################
class Eth_frame_t;

//##################################################################################################
//  ~ Net ifaces
//##################################################################################################

//##################################################################################################
//  Frames pool
//##################################################################################################
class Frames_t
{
	enum
	{
		Free = 0,
		Busy = 1,
	};

	struct Frame_t
	{
		uint8_t* _buf;
		bool     _busy;

		Frame_t(uint8_t* bf, bool busy) : _buf(bf), _busy(busy) {}
		uint8_t* buf()  { return _buf;   }
		uint8_t* end()  { return _buf + Frame_sz;   }
		bool     busy() { return _busy;  }
		bool     free() { return !_busy; }
		void     busy(bool b) { _busy = b; }
	};

	typedef list_t <Frame_t, 16> frames_t;
	frames_t _frames;

public:

	enum { Frame_sz = 1518 }; // FIXME:  use low level enum value

	void init(uint8_t* buf, size_t sz)
	{
		uint8_t* ptr = (uint8_t*) round_up((unsigned)buf, 4);
		while ((ptr + Frame_sz) <= (buf + sz))
		{
			_frames.push_back(Frame_t(ptr, Free));
			ptr += Frame_sz;
			ptr = (uint8_t*) round_up((unsigned)ptr, 4);
		}
		// TODO:  mutex.init
		wrm_logw("Frame_pool:  frames=%u, lost_mem=%d.\n", _frames.size(), (buf+sz) - ptr);
	}

	uint8_t* get()
	{
		// TODO:  mutex.lock();
		uint8_t* res = 0;
		for (frames_t::iter_t it=_frames.begin(); it!=_frames.end(); ++it)
		{
			if (it->free())
			{
				it->busy(true);
				res = it->buf();
				break;
			}
		}
		// TODO:  mutex.unlock();
		return res;
	}

	void free(void* buf)
	{
		uint8_t* b = (uint8_t*)buf;
		// TODO:  mutex.lock();
		for (frames_t::iter_t it=_frames.begin(); it!=_frames.end(); ++it)
		{
			if (b >= it->buf()  &&  b < it->end())
			//if (b == it->buf())
			{
				assert(it->busy());
				it->busy(false);
				// TODO:  mutex.unlock();
				return;
			}
		}
		assert(false && "Attempt to free alien buffer");
		// TODO:  mutex.unlock();
	}
};

//##################################################################################################
// Arp cache
//##################################################################################################
class Arp_cache_t
{
	struct record_t
	{
		uint32_t   ip;
		mac_t      mac;
		L4_clock_t timeout;
		record_t(uint32_t i, mac_t m, L4_clock_t t) : ip(i), mac(m), timeout(t) {}
	};

	enum { Timeout_usec = 30*1000*1000 }; // 30 sec

	typedef list_t <record_t, 8> records_t;
	records_t _records;

	records_t::iter_t find_record(uint32_t ip)
	{
		for (records_t::iter_t it=_records.begin(); it!=_records.end(); ++it)
			if (it->ip == ip)
				return it;
		return _records.end();
	}

public:

	mac_t find(uint32_t ip)
	{
		mac_t nil;
		records_t::iter_t it = find_record(ip);
		if (it == _records.end())
			return nil;

		L4_clock_t clock = l4_system_clock();
		if (clock > it->timeout)
		{
			_records.erase(it);
			return nil;
		}
		return it->mac;
	}

	void add(uint32_t ip, const mac_t& mac)
	{
		records_t::iter_t it = find_record(ip);
		if (it != _records.end())
		{
			if (it->mac != mac)
			{
				wrm_logw("arp cache:  redefined mac=%s (prev=%s) for ip=%s.\n",
					mac.str(), it->mac.str(), iaddr2str(ip));
				it->mac = mac;
			}
			it->timeout = l4_system_clock() + Timeout_usec;
		}
		else
		{
			if (_records.size() == _records.capacity())
				_records.erase(_records.begin());  // remove oldest record
			_records.push_back(record_t(ip, mac, l4_system_clock() + Timeout_usec));
		}
	}
};

//##################################################################################################
//  Common stack data
//##################################################################################################
struct Net_stack_t
{
	Wrm_mtx_t     mtx;    // global access mutex

	// local threads
	L4_thrid_t    ip_eth;
	L4_thrid_t    ip_client;

	// remote eth threads
	L4_thrid_t    eth_tx;
	L4_thrid_t    eth_rx;

	Frames_t      frames;

	Arp_cache_t   arp_cache;
	Eth_ifaces_t  eth_ifaces;
	Ip_ifaces_t   ip_ifaces;
	Udp_sockets_t udp_sockets;
	Tcp_sockets_t tcp_sockets;
};

static Net_stack_t net_stack;

bool is_saddr_free(int proto, const sockaddr_in* saddr)
{
	if (proto == IPPROTO_UDP)
		return net_stack.udp_sockets.is_saddr_free(saddr);
	if (proto == IPPROTO_TCP)
		return net_stack.tcp_sockets.is_saddr_free(saddr);
	return 0;
}

//##################################################################################################

static int send_to_eth_drv(const unsigned char* buf, size_t len, size_t* sent);

//--------------------------------------------------------------------------------------------------
int send_icmp_dest_unreach(const Ip_iface_t* ipif, const Eth_frame_t* eth_req,
                           const Ip_packet_t* ip_req, size_t ip_req_len, int icmp_code)
{
	Eth_frame_t* eth = (Eth_frame_t*) net_stack.frames.get();
	if (!eth)
		return 1;

	// build eth
	eth->src   = ipif->ethif()->addr();
	eth->dst   = eth_req->src;
	eth->etype = Eth_frame_t::Etype_ip;

	// build ip
	Ip_packet_t* ip = (Ip_packet_t*) eth->payload();
	ip->version         = 4;
	ip->header_length   = 5;
	ip->dscp            = 0;
	ip->ecn             = 0;
	ip->total_length    = ip->hdrlen() + Icmp_packet_t::hdrlen() + Icmp_dest_unreach_t::hdrlen() + ip_req_len;
	ip->identification  = 0;
	ip->flag0_reserved  = 0;
	ip->flag1_df        = 1;
	ip->flag2_mf        = 0;
	ip->fragment_offset = 0;
	ip->time_to_live    = 64;
	ip->header_checksum = 0;
	ip->protocol        = Ip_packet_t::Proto_icmp;
	ip->dst             = ip_req->src;
	ip->src             = ipif->addr();
	ip->header_checksum = inet_checksum((uint8_t*)ip, ip->hdrlen());

	// build icmp
	Icmp_packet_t* icmp = (Icmp_packet_t*) ip->payload();
	icmp->type = Icmp_packet_t::Type_dest_unreach;
	icmp->code = icmp_code;

	// build Icmp-dest-unreach
	Icmp_dest_unreach_t* dest_unreach = (Icmp_dest_unreach_t*) icmp->payload();
	dest_unreach->payload(ip_req, ip_req_len);

	// send
	size_t sent = 0;
	size_t send = eth->hdrlen() + ip->total_length;
	int rc = send_to_eth_drv((uint8_t*)eth, send, &sent);
	assert(!rc);
	assert(send == sent);

	net_stack.frames.free(eth);
	return 0;
}

static int reply_to_client(L4_thrid_t cli, int ecode,
                           const word_t* words = NULL, size_t wordscnt = 0,
                           const uint8_t* bf = NULL, size_t bfsz = 0, size_t* sent = 0);

//--------------------------------------------------------------------------------------------------
// create new socket and send reply to client
void accept_connected_stream(Tcp_socket_t* socket, Stream_t* stream)
{
	assert(socket->cli_state() == Tcp_socket_t::Cli_accept);
	assert(stream->state == Stream_t::Established);

	uint32_t iaddr = socket->saddr_addr();
	if (iaddr == INADDR_ANY)
	{
		Ip_iface_t* ipif = net_stack.ip_ifaces.find_masked(stream->rem_addr);
		assert(ipif);
		iaddr = ipif->addr();
	}

	Socket_t* newsocket = net_stack.tcp_sockets.add(socket, stream, iaddr);
	if (!newsocket)
	{
		wrm_loge("XXX stream:  accept:  failed to add new socket, no capacity.\n");
		reply_to_client(socket->client(), 7);
		socket->cli_state(Tcp_socket_t::Cli_idle);
		socket->client(L4_thrid_t::Nil);
	}
	else
	{
		wrm_logw("XXX stream:  send accept reply to client.\n");
		word_t word[5];
		word[0] = newsocket->id();
		sockaddr_in* saddr = (sockaddr_in*)&word[1];
		saddr->sin_family      = AF_INET;
		saddr->sin_addr.s_addr = stream->rem_addr;
		saddr->sin_port        = stream->rem_port;
		reply_to_client(socket->client(), 0, word, 5);
		socket->cli_state(Tcp_socket_t::Cli_idle);
		socket->client(L4_thrid_t::Nil);
	}
}

//--------------------------------------------------------------------------------------------------
// this func is called for each socket and checks awaiting of iaddr
int check_arp_waiting_sockets(Socket_t* socket, int v1, int v2)
{
	uint32_t iaddr = v1;
	const mac_t* mac = (mac_t*) v2;

	if (socket->is_wait_arp())
	{
		Eth_frame_t* eth = (Eth_frame_t*) socket->eth_frame();
		Ip_packet_t* ip  = (Ip_packet_t*) eth->payload();
		if (ip->dst == iaddr)
		{
			eth->dst = *mac;

			// send to driver
			size_t send = socket->eth_frame_len();
			size_t sent = 0;
			int rc = send_to_eth_drv((uint8_t*)eth, send, &sent);
			assert(!rc);
			assert(send == sent);

			socket->wait_arp(false);
			net_stack.frames.free(eth);

			// send to client
			if (socket->proto() == IPPROTO_UDP)
			{
				Udp_socket_t* usock = (Udp_socket_t*)socket;
				if (usock->cli_state() == Udp_socket_t::Cli_send)
				{
					size_t udp_sent = sent - eth->hdrlen() - ip->hdrlen() - Udp_packet_t::hdrlen();
					reply_to_client(socket->client(), 0, (word_t*)&udp_sent, 1);
					usock->cli_state(Udp_socket_t::Cli_idle);
					socket->client(L4_thrid_t::Nil);
				}
			}
			else if (socket->proto() == IPPROTO_TCP)
			{
				Tcp_socket_t* tsock = (Tcp_socket_t*)socket;
				if (tsock->cli_state() == Tcp_socket_t::Cli_send)
				{
					Tcp_packet_t* tcp = (Tcp_packet_t*) ip->payload();
					size_t tcp_sent = sent - eth->hdrlen() - ip->hdrlen() - tcp->hdrlen();
					reply_to_client(socket->client(), 0, (word_t*)&tcp_sent, 1);
					tsock->cli_state(Tcp_socket_t::Cli_idle);
					socket->client(L4_thrid_t::Nil);
				}
			}
		}
	}
	return 0;
}

//##################################################################################################
//  Packets processing
//##################################################################################################
//--------------------------------------------------------------------------------------------------
void send_arp_request(const Ip_iface_t* ipif, uint32_t iaddr)
{
	// eth
	Eth_frame_t* eth = (Eth_frame_t*) net_stack.frames.get();
	if (!eth)
		assert(false  &&  "Implement me!");

	eth->dst.set_bcast();
	eth->src = ipif->ethif()->addr();
	eth->etype = Eth_frame_t::Etype_arp;

	// arp
	Arp_packet_t* arp = (Arp_packet_t*) eth->payload();
	arp->htype = 0x0001 /* Ethernet */;
	arp->ptype = 0x0800 /* Ip */;
	arp->hlen  = 6;
	arp->plen  = 4;
	arp->oper  = Arp_packet_t::Opcode_req;
	arp->sha   = ipif->ethif()->addr();
	arp->spa   = ipif->addr();
	arp->tha.set_nil();
	arp->tpa   = iaddr;

	// send
	size_t send = eth->hdrlen() + arp->size();
	size_t sent = 0;
	int rc = send_to_eth_drv((uint8_t*)eth, send, &sent);
	assert(!rc);
	assert(send == sent);

	net_stack.frames.free(eth);
}

//--------------------------------------------------------------------------------------------------
// send tcp without payload
void send_tcp_msg(Socket_t* socket, Stream_t* stream, int flags)
{
	// find mac addr by iaddr
	mac_t dst_mac = net_stack.arp_cache.find(stream->rem_addr);
	//assert(!dst_mac.is_nil());

	// build frame:
	const size_t hdrs_len = Eth_frame_t::hdrlen() + Ip_packet_t::Typically_hdr_len + Tcp_packet_t::Typically_hdr_len;
	//uint8_t buf[hdrs_len];
	uint8_t* buf = net_stack.frames.get();
	assert(buf);

	/*
	size_t hdrs_space = sitem->pointer() - (word_t)frame;
	size_t hdrs_len = Eth_frame_t::hdrlen() + Ip_packet_t::Typically_hdr_len + Tcp_packet_t::Typically_hdr_len;
	assert(hdrs_space <= Frames_t::Frame_sz  &&  "something going wrong");
	assert(hdrs_space >= hdrs_len  &&  "something going wrong");
	*/

	//Ip_iface_t* ipif = net_stack.ip_ifaces.find(socket->saddr_addr());
	// find ip iface  // ??? THINK ???
	Ip_iface_t* ipif = socket->saddr_addr() == INADDR_ANY ?
	                   net_stack.ip_ifaces.find_masked(stream->rem_addr) :
	                   net_stack.ip_ifaces.find(socket->saddr_addr());
	assert(ipif);

	// build eth
	Eth_frame_t* eth = (Eth_frame_t*)buf;
	eth->src   = ipif->ethif()->addr();
	eth->dst   = dst_mac;
	eth->etype = Eth_frame_t::Etype_ip;

	// build ip
	Ip_packet_t* ip = (Ip_packet_t*) eth->payload();
	ip->version         = 4;
	ip->header_length   = 5;
	ip->dscp            = 0;
	ip->ecn             = 0;
	ip->total_length    = ip->hdrlen() + Tcp_packet_t::Typically_hdr_len;
	ip->identification  = 0;
	ip->flag0_reserved  = 0;
	ip->flag1_df        = 1;
	ip->flag2_mf        = 0;
	ip->fragment_offset = 0;
	ip->time_to_live    = 64;
	ip->header_checksum = 0;
	ip->protocol        = Ip_packet_t::Proto_tcp;
	ip->dst             = stream->rem_addr;
	ip->src             = ipif->addr();
	ip->header_checksum = inet_checksum((uint8_t*)ip, ip->hdrlen());

	// build tcp
	Tcp_packet_t* tcp   = (Tcp_packet_t*) ip->payload();
	tcp->src            = socket->saddr_port();
	tcp->dst            = stream->rem_port;
	tcp->seq            = stream->seq_tx;
	tcp->ack            = stream->seq_rx;
	tcp->hlen           = Tcp_packet_t::Typically_hdr_len / 4;
	tcp->reserved       = 0;
	tcp->flags          = flags;
	tcp->win_sz         = 100; // ???
	tcp->checksum       = 0;
	tcp->urgent_ptr     = 0;
	uint8_t* opts       = tcp->optptr(0);
	*opts               = Tcp_packet_t::Opt_end;
	tcp->checksum       = tcp->calc_checksum(/*sitem->length() +*/ tcp->hlen * 4, ip->src, ip->dst, ip->protocol);

	stream->seq_tx += 0;

	size_t send = hdrs_len; // + sitem->length();

	if (dst_mac.is_nil())
	{
		// unknown dest mac -- send arp request
		assert(!socket->is_wait_arp());

		// store cur socket state
		socket->store_tx_eth_frame(eth, send);
		socket->wait_arp(true);

		send_arp_request(ipif, stream->rem_addr);
	}
	else
	{
		// send to eth drv
		size_t sent = 0;
		int rc = send_to_eth_drv((uint8_t*)eth, send, &sent);
		assert(!rc);
		assert(send == sent);
		net_stack.frames.free(buf);
	}
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_tcp_to_socket(Tcp_socket_t* socket, const Ip_iface_t* ipif, Eth_frame_t* eth,
                                 Ip_packet_t* ip, Tcp_packet_t* tcp, size_t tcplen)
{
	// get stream
	Stream_t* stream = 0;
	if (socket->net_state() == Tcp_socket_t::Net_listen)
	{
		stream = socket->get_stream(ip->src, tcp->src);
		if (!stream  &&  (tcp->flags & Tcp_packet_t::Syn))
		{
			wrm_logw("%s:  add new stream ...\n", __func__);
			stream = socket->add_stream(ip->src, tcp->src);
			if (stream)
			{
				stream->init_seq_tx = 123; // TODO:  is need to use rand()?
				stream->seq_tx      = 123; // TODO:  is need to use rand()?
			}
			else
				assert(false);
		}
	}
	else if (socket->net_state() == Tcp_socket_t::Net_connection)
	{
		stream = socket->stream();
		assert(stream);
	}
	else
		assert(false);

	if (!stream)
	{
		wrm_loge("stream:  stream not found, packet is ignored.\n");
		return 1;
		assert(stream);
	}

	bool new_stream = stream->state == Stream_t::Closed  ||  stream->state == Stream_t::Syn_sent;
	if (!new_stream  &&  tcp->seq != stream->seq_rx)
	{
		wrm_logw("stream:  wrong sequence=%u, expected=%u, packet is ignored.\n",
			tcp->seq, stream->seq_rx);
		return 1;
	}

	// Syn
	if (tcp->flags & Tcp_packet_t::Syn)
	{
		stream->init_seq_rx = tcp->seq + 1;
		stream->seq_rx      = tcp->seq + 1;       // expected in next msg
	}
	else
	{
		// FIXME
		stream->seq_rx += tcplen - tcp->hdrlen(); // expected in next msg
	}

	// Rst
	if (tcp->flags & Tcp_packet_t::Rst)
	{
		stream->seq_tx++;
		stream->seq_rx++;
		send_tcp_msg(socket, stream, Tcp_packet_t::Rst | Tcp_packet_t::Ack);
		stream->state = Stream_t::Closed;
		{
			if (socket->cli_state() == Tcp_socket_t::Cli_connect)
			{
				reply_to_client(socket->client(), 11);
				socket->cli_state(Tcp_socket_t::Cli_idle);
				socket->client(L4_thrid_t::Nil);
			}
			else
				assert(false);
		}
	}

	switch (stream->state)
	{
		case Stream_t::Closed:
		{
			if (tcp->flags & Tcp_packet_t::Syn)
			{
				send_tcp_msg(socket, stream, Tcp_packet_t::Syn | Tcp_packet_t::Ack);
				stream->seq_tx++;
				stream->state = Stream_t::Syn_received;  // wait ack
			}
			else
			{
				//assert(false);
			}
			break;
		}
		case Stream_t::Syn_sent:
		{
			if (tcp->flags & Tcp_packet_t::Ack)
			{
				wrm_logw("stream:  connected.\n");

				stream->state = Stream_t::Established;
				stream->seq_tx++;

				send_tcp_msg(socket, stream, Tcp_packet_t::Ack);

				assert(socket->cli_state() == Tcp_socket_t::Cli_connect);
				reply_to_client(socket->client(), 0);
				socket->cli_state(Tcp_socket_t::Cli_idle);
				socket->client(L4_thrid_t::Nil);
			}
			else
				assert(false);
			break;
		}
		case Stream_t::Syn_received:
		{
			if (tcp->flags & Tcp_packet_t::Ack)
			{
				wrm_logw("stream:  connected.\n");
				stream->state = Stream_t::Established;

				int rc = socket->move_to_connected_queue(stream);
				if (rc)
				{
					assert(false && "Connected queue is full. Implement me - send reject");
				}

				// send reply to client if it waits accept()
				if (socket->cli_state() == Tcp_socket_t::Cli_accept)
					accept_connected_stream(socket, stream);

				// TODO:  send data from stream->tx_buf
			}
			else
				assert(false);
			break;
		}
		case Stream_t::Established:
		{
			wrm_logw("stream:  establ:  rx msg.\n");
			// TODO:  put data to rx_buf
			size_t len = tcplen - tcp->hdrlen();
			size_t free = stream->sz - stream->wp;
			assert(len <= free);
			memcpy(stream->buf + stream->wp, tcp->payload(), len);
			stream->wp += len;

			if (tcp->flags & Tcp_packet_t::Psh)
			{
				if (socket->cli_state() == Tcp_socket_t::Cli_recv)
				{
					wrm_logw("stream:  send recv reply to client.\n");
					sockaddr_in saddr = { AF_INET, stream->rem_port, stream->rem_addr };
					size_t sent = 0;
					reply_to_client(socket->client(), 0, (word_t*)&saddr, 4, stream->buf, stream->wp, &sent);
					// TODO: optimizeme
					if (sent != stream->wp)
					{
						assert(sent < stream->wp);
						memcpy(stream->buf, stream->buf + sent, stream->wp - sent);
						stream->wp = stream->wp - sent;
					}
					else
						stream->wp = 0;
					// ~ TODO: optimizeme
					socket->cli_state(Tcp_socket_t::Cli_idle);
					socket->client(L4_thrid_t::Nil);
				}
			}
			if (tcp->flags & Tcp_packet_t::Fin)
			{
				stream->seq_rx += 1; // expected in next msg
				send_tcp_msg(socket, stream, Tcp_packet_t::Fin | Tcp_packet_t::Ack);
				if (0 /* exist data for tx */)
					stream->state = Stream_t::Close_wait;
				else
					stream->state = Stream_t::Last_ack;

				// notify client about disconnecting
				wrm_logw("stream:  cli_state=%d.\n\n\n", socket->cli_state());
				if (socket->cli_state() == Tcp_socket_t::Cli_recv)
				{
					wrm_logw("stream:  send close reply to client.\n");
					sockaddr_in saddr = { AF_INET, stream->rem_port, stream->rem_addr };
					reply_to_client(socket->client(), 0, (word_t*)&saddr, 4, (uint8_t*)1, 0);
					stream->wp = 0;
					socket->cli_state(Tcp_socket_t::Cli_idle);
					socket->client(L4_thrid_t::Nil);
				}
			}
			break;
		}
		case Stream_t::Fin_wait1:
		{
			// disconnecting
			if (tcp->flags & Tcp_packet_t::Fin  &&  tcp->flags & Tcp_packet_t::Ack)
			{
				stream->state = Stream_t::Time_wait;
				stream->seq_rx += 1; // expected in next msg
				stream->seq_tx += 1; // FIXME
				send_tcp_msg(socket, stream, Tcp_packet_t::Ack);

				assert(socket->cli_state() == Tcp_socket_t::Cli_close);
				reply_to_client(socket->client(), 0);
				socket->cli_state(Tcp_socket_t::Cli_closed);

				// TODO:  start timer=2*MSL to wait timeout and switch:  Time_wait -> Idle

				// Workaround:  without timer
				//              simulate that I'm in Time_wait and got timeout - remove socket
				int rc = socket->rem_stream();
				assert(!rc);
				rc = net_stack.tcp_sockets.remove(socket->id(), socket->client());
				assert(!rc);
			}
			break;
		}
		case Stream_t::Close_wait:
			assert(false);

		case Stream_t::Fin_wait2:
			assert(false);

		case Stream_t::Last_ack:
		{
			if (tcp->flags & Tcp_packet_t::Ack) // last ack
			{
				stream->state = Stream_t::Closed;

				if (socket->cli_state() == Tcp_socket_t::Cli_close)
				{
					// remove socket
					L4_thrid_t cli = socket->client();
					int rc = socket->rem_stream();
					assert(!rc);
					rc = net_stack.tcp_sockets.remove(socket->id(), cli);
					assert(!rc);

					wrm_logw("stream:  send closed reply to client.\n");
					socket->cli_state(Tcp_socket_t::Cli_closed);
					reply_to_client(cli, 0);
				}
			}
			else
			{
				assert(0 && "Something going wrong")
			}
			break;
		}
		case Stream_t::Time_wait:
			assert(false);

		case Stream_t::Closing:
			assert(false);
	}
	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_tcp_packet(const Ip_iface_t* ipif, Eth_frame_t* eth, Ip_packet_t* ip,
                              Tcp_packet_t* tcp, size_t tcplen)
{
	if (tcplen < Tcp_packet_t::Length_min  ||  tcplen < tcp->hdrlen())
	{
		wrm_loge("tcp:  wrong tcp->length, too small.\n");
		return 1;
	}

	if (tcp->calc_checksum(tcplen, ip->src, ip->dst, ip->protocol))
	{
		wrm_loge("tcp:  wrong checksum=0x%04x, msg is ignored.\n", tcp->checksum);
		tcp->checksum = 0;
		wrm_loge("tcp:  expected=0x%04x.\n", tcp->calc_checksum(tcplen, ip->src, ip->dst, ip->protocol));
		return 1;
	}

	Tcp_socket_t* socket = net_stack.tcp_sockets.find_connected_socket(ip->dst, tcp->dst, ip->src, tcp->src);
	if (socket)
	{
		process_tcp_to_socket(socket, ipif, eth, ip, tcp, tcplen);
	}
	else if ((socket = net_stack.tcp_sockets.find_listening_socket(ip->dst, tcp->dst)))
	{
		process_tcp_to_socket(socket, ipif, eth, ip, tcp, tcplen);
	}
	else
	{
		wrm_loge("tcp:  destination unreacheable.\n");
		//assert(false && "test:  del me");
		send_icmp_dest_unreach(ipif, eth, ip, ip->hdrlen() + tcplen, Icmp_packet_t::Code_dst_port_inreach);
	}

	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_udp_packet(const Ip_iface_t* ipif, Eth_frame_t* eth, Ip_packet_t* ip,
                              Udp_packet_t* udp, size_t udplen)
{
	if (udplen < Udp_packet_t::Length_min  ||  udplen < udp->length)
	{
		wrm_loge("udp:  wrong udp->length, too small.\n");
		return 1;
	}

	if (udp->calc_checksum(ip->src, ip->dst, ip->protocol))
	{
		wrm_loge("udp:  wrong checksum=0x%04x, msg is ignored.\n", udp->checksum);
		udp->checksum = 0;
		wrm_loge("udp:  expected=0x%04x.\n", udp->calc_checksum(ip->src, ip->dst, ip->protocol));
		return 1;
	}

	// TODO:  use find _socket()
	Udp_socket_t* socket = net_stack.udp_sockets.find(ip->dst, udp->dst);
	if (socket)
	{
		sockaddr_in saddr;
		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = ip->src;
		saddr.sin_port = udp->src;

		if (socket->cli_state() == Udp_socket_t::Cli_recv
			/*WA*/|| !socket->recv_client().is_nil() /*~WA*/)  // FIXME
		{
			L4_thrid_t cli = socket->recv_client();
			/*WA*/if (socket->cli_state() == Udp_socket_t::Cli_recv)  // FIXME
				socket->cli_state(Udp_socket_t::Cli_idle);
			socket->recv_client(L4_thrid_t::Nil);
			reply_to_client(cli, 0, (word_t*)&saddr, sizeof(saddr)/sizeof(word_t),
			                udp->payload(), udp->payload_len());
		}
		else
		{
			//wrm_logw("udp:  sock=%d:  save_received_packet, state=%d.\n", socket->id(), socket->cli_state());
			int rc = socket->save_received_packet(&saddr, udp->payload(), udp->payload_len());
			if (rc)
				wrm_loge("udp:  sock=%d:  save_received_packet() - failed, rc=%d.\n", socket->id(), rc);
			//assert(!rc && "no space to store udp pkt");
		}
	}
	else
	{
		wrm_loge("udp:  destination unreacheable.\n");
		send_icmp_dest_unreach(ipif, eth, ip, ip->hdrlen() + udplen, Icmp_packet_t::Code_dst_port_inreach);
	}

	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_icmp_packet(Eth_frame_t* eth, Ip_packet_t* ip, Icmp_packet_t* icmp, size_t icmplen)
{
	if (icmplen < Icmp_packet_t::Length_min)
	{
		wrm_loge("icmp:  too small icmp packet:  %u bytes.\n", icmplen);
		return 1;
	}

	if (icmp->calc_checksum(icmplen))
	{
		wrm_loge("icmp:  wrong checksum=0x%04x, msg is ignored.\n", icmp->checksum);
		return 1;
	}

	int res = 0;
	switch (icmp->type)
	{
		case Icmp_packet_t::Type_echo_reply:
		{
			wrm_loge("icmp:  ping reply.\n");
			res = 1;
			break;
		}
		case Icmp_packet_t::Type_echo_request:
		{
			wrm_logi("icmp:  ping request.\n");

			// reuse incomming buffer to send reply
			// icmp
			icmp->type     = Icmp_packet_t::Type_echo_reply;
			icmp->code     = 0;
			icmp->checksum = 0;
			icmp->checksum = inet_checksum((uint8_t*)icmp, icmplen);

			// ip
			uint32_t addr = ip->src;
			ip->src = ip->dst;
			ip->dst = addr;

			// eth
			mac_t mac = eth->src;
			eth->src = eth->dst;
			eth->dst = mac;

			size_t sent = 0;
			size_t send = eth->hdrlen() + ip->hdrlen() + icmplen;
			int rc = send_to_eth_drv((uint8_t*)eth, send, &sent);
			assert(!rc);
			assert(send == sent);

			res = 1; // allow to reuse incomming buffer
			break;
		}
		default:
		{
			wrm_loge("icmp:  unsupported icmp type %u.\n", icmp->type);
			res = 1;
		}
	}
	return res;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_ip_packet(Eth_frame_t* eth, Ip_packet_t* ip, size_t iplen)
{
	if (iplen < Ip_packet_t::Length_min)
	{
		wrm_loge("ip:  too small ip packet:  %u bytes.\n", iplen);
		return 1;
	}

	if (iplen < ip->total_length)
	{
		wrm_loge("ip:  iplen(%u) < total_length(%u).\n", iplen, ip->total_length);
		return 1;
	}

	Ip_iface_t* ipif = net_stack.ip_ifaces.find(ip->dst);
	if (!ipif)
	{
		wrm_loge("ip:  alien ip address:  %s.\n", iaddr2str(ip->dst));
		return 1;
	}

	net_stack.arp_cache.add(ip->src, eth->src);

	if (ip->calc_checksum())
	{
		wrm_loge("ip:  wrong checksum=0x%04x, msg is ignored.\n", ip->header_checksum);
		return 1;
	}

	int res = 0;
	switch (ip->protocol)
	{
		case Ip_packet_t::Proto_icmp:
		{
			Icmp_packet_t* icmp = (Icmp_packet_t*)ip->payload();
			res = process_icmp_packet(eth, ip, icmp, ip->total_length - ip->hdrlen());
			break;
		}
		case Ip_packet_t::Proto_tcp:
		{
			Tcp_packet_t* tcp = (Tcp_packet_t*)ip->payload();
			res = process_tcp_packet(ipif, eth, ip, tcp, ip->total_length - ip->hdrlen());
			break;
		}
		case Ip_packet_t::Proto_udp:
		{
			Udp_packet_t* udp = (Udp_packet_t*)ip->payload();
			res = process_udp_packet(ipif, eth, ip, udp, ip->total_length - ip->hdrlen());
			break;
		}
		default:
		{
			wrm_loge("ip:  unsupported IP protocol %u.\n", ip->protocol);
			send_icmp_dest_unreach(ipif, eth, ip, iplen, Icmp_packet_t::Code_dst_proto_unreach);
			res = 1;
		}
	}
	return res;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_arp_packet(Eth_frame_t* eth, Arp_packet_t* arp, size_t arplen)
{
	if (arplen < sizeof(Arp_packet_t))
	{
		wrm_loge("arp:  too small arp packet:  %u bytes.\n", arplen);
		return 1;
	}

	if (arp->htype != 0x0001 /* Ethernet */)
	{
		wrm_loge("arp:  unsupported htype=0x%x.\n", arp->htype);
		return 1;
	}

	if (arp->ptype != 0x0800 /* Ip */)
	{
		wrm_loge("arp:  unsupported ptype=0x%x.\n", arp->ptype);
		return 1;
	}

	if (arp->oper == Arp_packet_t::Opcode_req)
	{
		wrm_logd("arp:  request:  who has %s asks %s.\n", iaddr2str(arp->tpa), iaddr2str(arp->spa));

		Ip_iface_t* ipif = net_stack.ip_ifaces.find(arp->tpa);
		if (ipif)
		{
			// reuse incomming buffer to send reply
			// arp
			arp->oper = Arp_packet_t::Opcode_rep;
			arp->sha = ipif->ethif()->addr();
			arp->spa = arp->tpa;
			arp->tha.set_nil();
			arp->tpa = 0;

			// eth
			memswap(eth->src.buf(), eth->dst.buf(), 6);

			// send
			size_t send = arplen + eth->hdrlen();
			size_t sent = 0;
			int rc = send_to_eth_drv((uint8_t*)eth, send, &sent);
			assert(!rc);
			assert(send == sent);
		}
		else
		{
			wrm_logw("arp:  request:  unknown ip iface %s.\n", iaddr2str(arp->tpa));
		}
	}
	else if (arp->oper == Arp_packet_t::Opcode_rep)
	{
		wrm_logi("arp:  reply:  %s is at %s\n", iaddr2str(arp->spa), arp->sha.str());
		net_stack.arp_cache.add(arp->spa, arp->sha);
		net_stack.udp_sockets.foreach(check_arp_waiting_sockets, (int)arp->spa, (int)&arp->sha);
		net_stack.tcp_sockets.foreach(check_arp_waiting_sockets, (int)arp->spa, (int)&arp->sha);
	}
	else
	{
		wrm_loge("arp:  unknown opcode %u.\n", arp->oper);
	}
	return 1; // allow to reuse incomming buffer
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_eth_frame(Eth_frame_t* eth, size_t len)
{
	if (len < sizeof(Eth_frame_t))
	{
		wrm_loge("eth:  too small eth frame:  %u bytes.\n", len);
		return 1;
	}

	Eth_iface_t* ethif = net_stack.eth_ifaces.find(eth->dst.buf());
	if (!ethif && !eth->dst.is_bcast())
	{
		wrm_loge("eth:  alien eth address:  %s.\n", eth->dst.str());
		return 1;
	}

	int res = 0;
	switch (eth->etype)
	{
		case Eth_frame_t::Etype_ip:
		{
			Ip_packet_t* ip = (Ip_packet_t*)eth->payload();
			res = process_ip_packet(eth, ip, len - sizeof(Eth_frame_t));
			break;
		}
		case Eth_frame_t::Etype_arp:
		{
			Arp_packet_t* arp = (Arp_packet_t*)eth->payload();
			res = process_arp_packet(eth, arp, len - sizeof(Eth_frame_t));
			break;
		}
		default:
		{
			wrm_logw("eth:  unknown payload.\n");
			res = 1;
		}
	}
	return res;
}
//##################################################################################################
//  ~ Packets processing
//##################################################################################################
//##################################################################################################
//  Client's requests processing
//##################################################################################################
//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_socket(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 6  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  socket:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return 1;
	}

	// incoming params
	int domain = mr[4];
	int type   = mr[5];
	int proto  = mr[6];

	if (domain != AF_INET)
	{
		wrm_loge("cli:  socket:  unsupported domain=%d, (supported %d).\n", domain, AF_INET);
		reply_to_client(cli, 3);
		return 1;
	}

	wrm_logd("cli:  socket:  domain=%d, type=%d, proto=%d.\n", domain, type, proto);

	// get owner threads
	unsigned thrno_begin = 0;
	unsigned thrno_end = 0;
	int rc = wrm_app_threads(cli, &thrno_begin, &thrno_end);
	if (rc)
	{
		wrm_loge("cli:  socket:  wrm_app_threads() - failed, rc=%d.\n", rc);
		reply_to_client(cli, 4);
		return 1;
	}

	int id = -1;
	switch (type)
	{
		case SOCK_DGRAM:
		{
			if (!proto)
				proto = IPPROTO_UDP;

			if (proto != IPPROTO_UDP)
			{
				wrm_loge("cli:  socket:  unsupported proto=%d, (supported %d).\n", proto, IPPROTO_UDP);
				reply_to_client(cli, 5);
				return 1;
			}

			Socket_t* socket = net_stack.udp_sockets.add(cli, thrno_begin, thrno_end /*, domain, type, proto*/);
			if (!socket)
			{
				wrm_loge("cli:  socket:  failed to add new udp socket, no capacity.\n");
				reply_to_client(cli, 6);
				return 1;
			}
			id = socket->id();
			break;
		}
		case SOCK_STREAM:
		{
			if (!proto)
				proto = IPPROTO_TCP;

			if (proto != IPPROTO_TCP)
			{
				wrm_loge("cli:  socket:  unsupported proto=%d, (supported %d).\n", proto, IPPROTO_TCP);
				reply_to_client(cli, 7);
				return 1;
			}

			Socket_t* socket = net_stack.tcp_sockets.add(cli, thrno_begin, thrno_end /*, domain, type, proto*/);
			if (!socket)
			{
				wrm_loge("cli:  socket:  failed to add new tcp socket, no capacity.\n");
				reply_to_client(cli, 8);
				return 1;
			}
			id = socket->id();
			break;
		}
		default:
			wrm_loge("cli:  socket:  unsupported type=%d, (supported %d, %d).\n",
				type, SOCK_DGRAM, SOCK_STREAM);
			reply_to_client(cli, 9);
			return 1;
	}

	reply_to_client(cli, 0, (word_t*)&id, 1);

	return 1;
}

//--------------------------------------------------------------------------------------------------
void process_cli_close_tcp(Tcp_socket_t* socket, L4_thrid_t cli)
{
	wrm_logd("%s() - socket->net_state=%d.\n", __func__, socket->net_state());

	if (socket->net_state() == Tcp_socket_t::Net_listen)
	{
		if (socket->streams_count())
		{
			assert(false && "TODO:  close all streams and after close socket.");
		}
		else
		{
			// remove socket
			socket->cli_state(Tcp_socket_t::Cli_closed);
			int rc = net_stack.tcp_sockets.remove(socket->id(), cli);
			assert(!rc);
			reply_to_client(cli, 0);
		}
	}
	else if (socket->net_state() == Tcp_socket_t::Net_connection)
	{
		Stream_t* stream = socket->stream();
		assert(stream);

		if (stream->state == Stream_t::Closed)
		{
			// remove socket
			socket->cli_state(Tcp_socket_t::Cli_closed);
			int rc = socket->rem_stream();
			assert(!rc);
			rc = net_stack.tcp_sockets.remove(socket->id(), cli);
			assert(!rc);
			reply_to_client(cli, 0);
		}
		else if (stream->state == Stream_t::Established)
		{
			// start disconnecting
			socket->cli_state(Tcp_socket_t::Cli_close);
			socket->client(cli);
			stream->state = Stream_t::Fin_wait1;
			send_tcp_msg(socket, stream, Tcp_packet_t::Fin | Tcp_packet_t::Ack);
		}
		else
			assert(false && "TODO");
	}
	else
		assert(false && "TODO");
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_close(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 4  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  close:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return 1;
	}

	// incoming params
	int sock = mr[4];

	// find socket
	Socket_t* socket = net_stack.udp_sockets.find(sock);
	if (!socket)
		socket = net_stack.tcp_sockets.find(sock);

	if (!socket)
	{
		wrm_loge("cli:  close:  unknown sock_id=%d.\n", sock);
		reply_to_client(cli, 3);
		return 1;
	}

	if (!socket->is_owner(cli))
	{
		wrm_loge("cli:  close:  wrong socket owner.\n");
		reply_to_client(cli, 4);
		return 1;
	}

	if (!socket->is_cli_state_idle())
	{
		wrm_loge("cli:  close:  socket busy:  cli_state is not idle.\n");
		reply_to_client(cli, 5);
		return 1;
	}

	wrm_logd("cli:  close:  sock=%d.\n", sock);

	if (socket->proto() == IPPROTO_UDP)
	{
		// remove socket
		int rc = net_stack.udp_sockets.remove(sock, cli);
		if (rc)
		{
			wrm_loge("cli:  could not remove socket, wrong sock or owner.\n");
			reply_to_client(cli, 6);
			return 1;
		}
		reply_to_client(cli, 0);
	}
	else if (socket->proto() == IPPROTO_TCP)
	{
		process_cli_close_tcp((Tcp_socket_t*)socket, cli);
	}
	else
		assert(false);

	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_bind(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 8  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  bind:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return 1;
	}

	// incoming params
	int sock  = mr[4];
	sockaddr_in saddr;
	word_t* addr = (word_t*) &saddr;
	addr[0] = mr[5];
	addr[1] = mr[6];
	addr[2] = mr[7];
	addr[3] = mr[8];

	// find socket
	Socket_t* socket = net_stack.udp_sockets.find(sock);
	if (!socket)
		socket = net_stack.tcp_sockets.find(sock);

	if (!socket)
	{
		wrm_loge("cli:  bind:  unknown sock_id=%d.\n", sock);
		reply_to_client(cli, 3);
		return 1;
	}

	if (!socket->is_owner(cli))
	{
		wrm_loge("cli:  bind:  wrong socket owner.\n");
		reply_to_client(cli, 4);
		return 1;
	}

	if (socket->domain() != saddr.sin_family)
	{
		wrm_loge("cli:  bind:  wrong saddr.family=%d for socket.domain=%d.\n",
			saddr.sin_family, socket->domain());
		reply_to_client(cli, 5);
		return 1;
	}

	if (!socket->is_cli_state_idle())
	{
		wrm_loge("cli:  bind:  socket busy:  cli_state is not idle.\n");
		reply_to_client(cli, 6);
		return 1;
	}

	if (!socket->is_net_state_idle())
	{
		wrm_loge("cli:  bind:  socket busy:  net_state is not idle.\n");
		reply_to_client(cli, 6);
		return 1;
	}

	// check family
	if (saddr.sin_family != AF_INET)
	{
		wrm_loge("cli:  bind:  unsupported sock_family=%d.\n", saddr.sin_family);
		reply_to_client(cli, 7);
		return 1;
	}

	// check ip iface
	if (saddr.sin_addr.s_addr != INADDR_ANY)
	{
		Ip_iface_t* ipif = net_stack.ip_ifaces.find(saddr.sin_addr.s_addr);
		if (!ipif)
		{
			wrm_loge("cli:  bind:  can not assign requested address.\n");
			reply_to_client(cli, 8);
			return 1;
		}
	}

	// check binding
	if (socket->bound())
	{
		wrm_loge("cli:  bind:  already bound.\n");
		reply_to_client(cli, 9);
		return 1;
	}

	// bind socket
	int rc = socket->bind(&saddr);
	if (rc)
	{
		wrm_loge("cli:  bind:  could not bind socket, rc=%d.\n", rc);
		reply_to_client(cli, 10);
		return 1;
	}

	reply_to_client(cli, 0);
	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_listen(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 5  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  listen:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return 1;
	}

	// incoming params
	int sock    = mr[4];
	int backlog = mr[5];

	// find socket
	Tcp_socket_t* socket = net_stack.tcp_sockets.find(sock);
	if (!socket)
	{
		wrm_loge("cli:  listen:  unknown sock_id=%d.\n", sock);
		reply_to_client(cli, 3);
		return 1;
	}

	if (!socket->is_owner(cli))
	{
		wrm_loge("cli:  listen:  wrong socket owner.\n");
		reply_to_client(cli, 4);
		return 1;
	}

	if (socket->proto() != IPPROTO_TCP)
	{
		wrm_loge("cli:  listen:  unsupported proto=%u.\n", socket->proto());
		reply_to_client(cli, 5);
		return 1;
	}

	if (!socket->is_cli_state_idle())
	{
		wrm_loge("cli:  listen:  socket busy:  cli_state is not idle.\n");
		reply_to_client(cli, 6);
		return 1;
	}

	if (!socket->is_net_state_idle())
	{
		wrm_loge("cli:  listen:  socket busy:  net_state is not idle.\n");
		reply_to_client(cli, 6);
		return 1;
	}

	wrm_logi("cli:  listen:  socket=%d in listen state.\n", sock);

	socket->listen(backlog);
	reply_to_client(cli, 0);

	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_accept(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 4  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  accept:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return 1;
	}

	// incoming params
	int sock   = mr[4];

	// find socket
	Tcp_socket_t* socket = net_stack.tcp_sockets.find(sock);
	if (!socket)
	{
		wrm_loge("cli:  accept:  unknown sock_id=%d.\n", sock);
		reply_to_client(cli, 3);
		return 1;
	}

	if (!socket->is_owner(cli))
	{
		wrm_loge("cli:  accept:  wrong socket owner.\n");
		reply_to_client(cli, 4);
		return 1;
	}

	if (socket->proto() != IPPROTO_TCP)
	{
		wrm_loge("cli:  accept:  unsupported proto=%u.\n", socket->proto());
		reply_to_client(cli, 5);
		return 1;
	}

	if (socket->cli_state() != Tcp_socket_t::Cli_idle)
	{
		wrm_loge("cli:  accept:  socket busy:  cli_state=%d.\n", socket->cli_state());
		reply_to_client(cli, 6);
		return 1;
	}

	if (socket->net_state() != Tcp_socket_t::Net_listen)
	{
		wrm_loge("cli:  accept:  socket is not in listen state (%d).\n", socket->net_state());
		reply_to_client(cli, 7);
		return 1;
	}

	wrm_logi("cli:  accept:  socket=%d waits connection.\n", sock);

	// accept new socket or block user to wait for some stream connected
	socket->cli_state(Tcp_socket_t::Cli_accept);
	socket->client(cli);
	Stream_t* stream = socket->get_connected_stream();
	if (stream)
		accept_connected_stream(socket, stream);

	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_connect(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 8  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  connect:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return 1;
	}

	// incoming params
	int sock  = mr[4];
	sockaddr_in saddr;
	word_t* addr = (word_t*) &saddr;
	addr[0] = mr[5];
	addr[1] = mr[6];
	addr[2] = mr[7];
	addr[3] = mr[8];

	// find socket
	Tcp_socket_t* socket = net_stack.tcp_sockets.find(sock);

	if (!socket)
	{
		wrm_loge("cli:  connect:  unknown sock_id=%d.\n", sock);
		reply_to_client(cli, 3);
		return 1;
	}

	if (!socket->is_owner(cli))
	{
		wrm_loge("cli:  connect:  wrong socket owner.\n");
		reply_to_client(cli, 4);
		return 1;
	}

	if (!socket->is_cli_state_idle())
	{
		wrm_loge("cli:  connect:  socket busy:  cli_state is not idle.\n");
		reply_to_client(cli, 5);
		return 1;
	}

	if (!socket->is_net_state_idle())
	{
		wrm_loge("cli:  connect:  socket busy:  net_state is not idle.\n");
		reply_to_client(cli, 6);
		return 1;
	}

	assert(!socket->stream());

	// find ip iface
	Ip_iface_t* ipif = net_stack.ip_ifaces.find_masked(saddr.sin_addr.s_addr);
	if (!ipif)
	{
		wrm_loge("cli:  connest:  no ipif fo dest=%s.\n", iaddr2str(saddr.sin_addr.s_addr));
		reply_to_client(cli, 7);
		return 1;
	}

	// auto bind
	bool bound = socket->bind(ipif->addr());
	if (!bound)
	{
		wrm_loge("cli:  connect:  coud not bind socket.\n");
		reply_to_client(cli, 8);
		return 1;
	}

	socket->net_state(Tcp_socket_t::Net_connection);
	socket->cli_state(Tcp_socket_t::Cli_connect);
	socket->client(cli);

	// add stream
	Stream_t* stream = socket->stream(saddr.sin_addr.s_addr, saddr.sin_port);
	assert(stream);

	stream->state = Stream_t::Syn_sent;
	send_tcp_msg(socket, stream, Tcp_packet_t::Syn);

	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_sendto_tcp(Tcp_socket_t* socket, sockaddr_in* saddr, L4_string_item_t* sitem,
                                  L4_thrid_t cli, uint8_t* frame)
{
	// NOTE:  saddr is ignored, send to connected stream

	if (socket->net_state() != Tcp_socket_t::Net_connection)
	{
		wrm_loge("cli:  sendto:  socket is not connection (%d).\n", socket->net_state());
		reply_to_client(cli, 7);
		return 1;
	}

	Stream_t* stream = socket->stream();
	assert(stream);

	if (stream->state != Stream_t::Established)
	{
		wrm_loge("cli:  sendto:  connection is not established (%d).\n", stream->state);
		reply_to_client(cli, 8);
		return 1;
	}

	// find ip iface  // ??? THINK ???
	Ip_iface_t* ipif = socket->saddr_addr() == INADDR_ANY ?
	                   net_stack.ip_ifaces.find_masked(stream->rem_addr) :
	                   net_stack.ip_ifaces.find(socket->saddr_addr());
	if (!ipif)
	{
		wrm_loge("cli:  sendto:  no ipif, something going wrong.\n");
		reply_to_client(cli, 9);
		return 1;
	}

	// bind socket if need
	bool bound = socket->bind(ipif->addr());
	if (!bound)
	{
		wrm_loge("cli:  sendto:  coud not bind socket.\n");
		reply_to_client(cli, 10);
		return 1;
	}

	// find mac addr by iaddr
	mac_t mac = net_stack.arp_cache.find(stream->rem_addr/*saddr->sin_addr.s_addr*/);

	// build frame:  sitem point to buffer in 'frame', at the start of frame reserve space for hdrs
	size_t hdrs_space = sitem->pointer() - (word_t)frame;
	size_t hdrs_len = Eth_frame_t::hdrlen() + Ip_packet_t::Typically_hdr_len + Tcp_packet_t::Typically_hdr_len;
	assert(hdrs_space <= Frames_t::Frame_sz  &&  "something going wrong");
	assert(hdrs_space >= hdrs_len  &&  "something going wrong");

	// build eth
	Eth_frame_t* eth = (Eth_frame_t*)(sitem->pointer() - hdrs_len);
	eth->src   = ipif->ethif()->addr();
	eth->dst   = mac;
	eth->etype = Eth_frame_t::Etype_ip;

	// build ip
	Ip_packet_t* ip = (Ip_packet_t*) eth->payload();
	ip->version         = 4;
	ip->header_length   = 5;
	ip->dscp            = 0;
	ip->ecn             = 0;
	ip->total_length    = ip->hdrlen() + Tcp_packet_t::Typically_hdr_len + sitem->length();
	ip->identification  = 0;
	ip->flag0_reserved  = 0;
	ip->flag1_df        = 1;
	ip->flag2_mf        = 0;
	ip->fragment_offset = 0;
	ip->time_to_live    = 64;
	ip->header_checksum = 0;
	ip->protocol        = Ip_packet_t::Proto_tcp;
	ip->dst             = stream->rem_addr;
	ip->src             = ipif->addr();
	ip->header_checksum = inet_checksum((uint8_t*)ip, ip->hdrlen());

	// build udp
	Tcp_packet_t* tcp   = (Tcp_packet_t*) ip->payload();
	tcp->src            = socket->saddr_port();
	tcp->dst            = stream->rem_port;
	tcp->seq            = stream->seq_tx;
	tcp->ack            = stream->seq_rx;
	tcp->hlen           = Tcp_packet_t::Typically_hdr_len / 4;
	tcp->flags          = Tcp_packet_t::Psh | Tcp_packet_t::Ack;
	tcp->win_sz         = 100; // ???
	tcp->checksum       = 0;
	tcp->urgent_ptr     = 0;
	uint8_t* opts       = tcp->optptr(0);
	*opts               = Tcp_packet_t::Opt_end;
	tcp->checksum       = tcp->calc_checksum(sitem->length() + tcp->hlen * 4, ip->src, ip->dst, ip->protocol);
	assert((word_t)tcp->payload() == sitem->pointer());

	stream->seq_tx += sitem->length();

	size_t send = hdrs_len + sitem->length();

	int res = 0;
	if (mac.is_nil())
	{
		// unknown dest mac -- send arp request
		if (socket->is_wait_arp())
		{
			wrm_loge("cli:  sendto:  socket already waits arp, something going wrong.\n");
			reply_to_client(cli, 11);
			return 1;
		}

		// store cur socket state
		socket->store_tx_eth_frame(eth, send);
		socket->wait_arp(true);
		socket->cli_state(Tcp_socket_t::Cli_send);
		socket->client(cli);

		send_arp_request(ipif, stream->rem_addr);
		res = 0; // keep current eth frame, don't reuse it
	}
	else
	{
		// send to eth drv
		size_t sent = 0;
		int rc = send_to_eth_drv((uint8_t*)eth, send, &sent);
		assert(!rc);
		assert(send == sent);

		size_t tcp_sent = sent - eth->hdrlen() - ip->hdrlen() - tcp->hdrlen();
		reply_to_client(cli, 0, (word_t*)&tcp_sent, 1);
		res = 1; // allow to reuse current eth frame
	}
	return res;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_sendto_udp(Udp_socket_t* socket, sockaddr_in* saddr, L4_string_item_t* sitem,
                                  L4_thrid_t cli, uint8_t* frame)
{
	if (socket->net_state() != Udp_socket_t::Net_idle)
	{
		wrm_loge("cli:  sendto:  socket busy:  net_state=%d.\n", socket->net_state());
		reply_to_client(cli, 7);
		return 1;
	}

	// check family
	if (saddr->sin_family != AF_INET)
	{
		wrm_loge("cli:  sendto:  unsupported sock_family=%d.\n", saddr->sin_family);
		reply_to_client(cli, 8);
		return 1;
	}

	// find ip iface
	Ip_iface_t* ipif = net_stack.ip_ifaces.find_masked(saddr->sin_addr.s_addr);
	if (!ipif)
	{
		wrm_loge("cli:  sendto:  unknown ip_addr=%s.\n", iaddr2str(saddr->sin_addr.s_addr));
		reply_to_client(cli, 9);
		return 1;
	}

	// bind socket if need
	bool bound = socket->bind(ipif->addr());
	if (!bound)
	{
		wrm_loge("cli:  sendto:  coud not bind socket.\n");
		reply_to_client(cli, 10);
		return 1;
	}

	// find mac addr by iaddr
	mac_t mac = net_stack.arp_cache.find(saddr->sin_addr.s_addr);

	// build frame:  sitem point to buffer in 'frame', at the start of frame reserve space for hdrs
	size_t hdrs_space = sitem->pointer() - (word_t)frame;
	size_t hdrs_len = Eth_frame_t::hdrlen() + Ip_packet_t::Typically_hdr_len + Udp_packet_t::hdrlen();
	assert(hdrs_space <= Frames_t::Frame_sz  &&  "something going wrong");
	assert(hdrs_space >= hdrs_len  &&  "something going wrong");

	// build eth
	Eth_frame_t* eth = (Eth_frame_t*)(sitem->pointer() - hdrs_len);
	eth->src   = ipif->ethif()->addr();
	eth->dst   = mac;
	eth->etype = Eth_frame_t::Etype_ip;

	// build ip
	Ip_packet_t* ip = (Ip_packet_t*) eth->payload();
	ip->version         = 4;
	ip->header_length   = 5;
	ip->dscp            = 0;
	ip->ecn             = 0;
	ip->total_length    = ip->hdrlen() + Udp_packet_t::hdrlen() + sitem->length();
	ip->identification  = 0;
	ip->flag0_reserved  = 0;
	ip->flag1_df        = 1;
	ip->flag2_mf        = 0;
	ip->fragment_offset = 0;
	ip->time_to_live    = 64;
	ip->header_checksum = 0;
	ip->protocol        = Ip_packet_t::Proto_udp;
	ip->dst             = saddr->sin_addr.s_addr;
	ip->src             = ipif->addr();
	ip->header_checksum = inet_checksum((uint8_t*)ip, ip->hdrlen());

	// build udp
	Udp_packet_t* udp   = (Udp_packet_t*) ip->payload();
	udp->src            = socket->saddr_port();
	udp->dst            = saddr->sin_port;
	udp->length         = Udp_packet_t::hdrlen() + sitem->length();
	udp->checksum       = 0;
	udp->checksum       = udp->calc_checksum(ip->src, ip->dst, ip->protocol);
	assert((word_t)udp->payload() == sitem->pointer());

	size_t send = hdrs_len + sitem->length();

	int res = 0;
	if (mac.is_nil())
	{
		// unknown dest mac -- send arp request
		if (socket->is_wait_arp())
		{
			wrm_loge("cli:  sendto:  socket already waits arp, something going wrong.\n");
			reply_to_client(cli, 11);
			return 1;
		}

		// store cur socket state
		socket->store_tx_eth_frame(eth, send);
		socket->wait_arp(true);
		socket->cli_state(Udp_socket_t::Cli_send);
		socket->client(cli);

		send_arp_request(ipif, saddr->sin_addr.s_addr);
		res = 0; // keep current eth frame, don't reuse it
	}
	else
	{
		// send to eth drv
		size_t sent = 0;
		int rc = send_to_eth_drv((uint8_t*)eth, send, &sent);
		assert(!rc);
		assert(send == sent);

		size_t udp_sent = sent - eth->hdrlen() - ip->hdrlen() - udp->hdrlen();
		reply_to_client(cli, 0, (word_t*)&udp_sent, 1);
		res = 1; // allow to reuse current eth frame
	}
	return res;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_sendto(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli, uint8_t* frame)
{
	if (tag.untyped() != 9  ||  tag.typed() != 2)
	{
		wrm_loge("cli:  sendto:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return 1;
	}

	// incoming params
	int sock  = mr[4];
	//int flags = mr[5];  // ignore
	sockaddr_in saddr;
	word_t* addr = (word_t*) &saddr;
	addr[0] = mr[6];
	addr[1] = mr[7];
	addr[2] = mr[8];
	addr[3] = mr[9];
	L4_string_item_t sitem;
	sitem.set(mr[10], mr[11]);
	assert(sitem.is_string_item());

	// find socket
	Socket_t* socket = net_stack.udp_sockets.find(sock);
	if (!socket)
		socket = net_stack.tcp_sockets.find(sock);

	if (!socket)
	{
		wrm_loge("cli:  sendto:  unknown sock_id=%d.\n", sock);
		reply_to_client(cli, 3);
		return 1;
	}

	if (!socket->is_owner(cli))
	{
		wrm_loge("cli:  sendto:  wrong socket owner.\n");
		reply_to_client(cli, 4);
		return 1;
	}

	if (!socket->is_cli_state_idle())
	{
		// wrm_logw("cli:  sendto:  socket busy:  cli_state is not idle (%d), FIXME.\n", socket->cli_state());
		// WA:  allow sendto while recvfrom waits
		if ((socket->proto() == IPPROTO_UDP  &&  socket->cli_state() == Udp_socket_t::Cli_recv)  ||
		    (socket->proto() == IPPROTO_TCP  &&  socket->cli_state() == Tcp_socket_t::Cli_recv))
		{
			// allow to send
			// FIXME:  do parallel working of psocket API
		}
		else
		{
			reply_to_client(cli, 5);
			return 1;
		}
	}

	int res = 0;
	if (socket->proto() == IPPROTO_UDP)
	{
		res = process_cli_sendto_udp((Udp_socket_t*)socket, &saddr, &sitem, cli, frame);
	}
	else if (socket->proto() == IPPROTO_TCP)
	{
		res = process_cli_sendto_tcp((Tcp_socket_t*)socket, &saddr, &sitem, cli, frame);
	}
	else
	{
		wrm_loge("cli:  sendto:  unsupported proto=%u.\n", socket->proto());
		reply_to_client(cli, 6);
		return 1;
	}

	return res;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_recvfrom_tcp(Tcp_socket_t* socket, L4_thrid_t cli)
{
	Stream_t* stream = socket->stream();
	assert(stream);

	assert(socket->net_state() == Tcp_socket_t::Net_connection);
	assert(socket->cli_state() == Tcp_socket_t::Cli_idle);

	if (stream->state == Stream_t::Established)
	{
		if (stream->wp)
		{
			wrm_logw("stream:  send recv reply to client.\n");
			sockaddr_in saddr = { AF_INET, stream->rem_port, stream->rem_addr };
			size_t sent = 0;
			reply_to_client(cli, 0, (word_t*)&saddr, 4, stream->buf, stream->wp, &sent);
			// TODO: optimizeme
			if (sent != stream->wp)
			{
				assert(sent < stream->wp);
				memcpy(stream->buf, stream->buf + sent, stream->wp - sent);
				stream->wp = stream->wp - sent;
			}
			else
				stream->wp = 0;
			// ~ TODO: optimizeme
		}
		else
		{
			socket->cli_state(Tcp_socket_t::Cli_recv);  // wait
			socket->client(cli);
		}
	}
	else
	{
		wrm_logw("stream:  send disconnect reply to client.\n");
		sockaddr_in saddr = { AF_INET, stream->rem_port, stream->rem_addr };
		reply_to_client(socket->client(), 0, (word_t*)&saddr, 4, (uint8_t*)1, 0);
	}
	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_cli_recvfrom(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 5  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  recvfrom:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return 1;
	}

	// incoming params
	int sock   = mr[4];
	int flags  = mr[5];  // ignore
	(void) flags;

	// find socket
	Socket_t* socket = net_stack.udp_sockets.find(sock);
	if (!socket)
		socket = net_stack.tcp_sockets.find(sock);

	if (!socket)
	{
		wrm_loge("cli:  recvfrom:  unknown sock_id=%d.\n", sock);
		reply_to_client(cli, 3);
		return 1;
	}

	if (!socket->is_owner(cli))
	{
		wrm_loge("cli:  recvfrom:  wrong socket owner.\n");
		reply_to_client(cli, 4);
		return 1;
	}

	if (!socket->is_cli_state_idle())
	{
		wrm_loge("cli:  recvfrom:  socket busy:  cli_state is not idle.\n");
		reply_to_client(cli, 5);
		return 1;
	}

	if (socket->proto() == IPPROTO_UDP)
	{
		Udp_socket_t* usock = (Udp_socket_t*)socket;
		if (usock->net_state() != Udp_socket_t::Net_idle)
		{
			wrm_loge("cli:  recvfrom:  socket busy:  net_state=%d.\n", usock->net_state());
			reply_to_client(cli, 6);
			return 1;
		}

		sockaddr_in* sa = 0;;
		unsigned char* bf = 0;
		size_t sz = 0;
		int rc = usock->get_received_packet(&sa, &bf, &sz);
		if (!rc)
		{
			// data exists
			reply_to_client(cli, 0, (word_t*)sa, sizeof(sockaddr_in)/sizeof(word_t), bf, sz);
			usock->free_received_packet();
		}
		else
		{
			// no data, wait
			usock->cli_state(Udp_socket_t::Cli_recv);
			socket->recv_client(cli);
		}
	}
	else if (socket->proto() == IPPROTO_TCP)
	{
		Tcp_socket_t* tsock = (Tcp_socket_t*)socket;
		if (tsock->net_state() != Tcp_socket_t::Net_connection)
		{
			wrm_loge("cli:  recvfrom:  socket is not connection:  state=%d.\n", tsock->net_state());
			reply_to_client(cli, 6);
			return 1;
		}
		process_cli_recvfrom_tcp(tsock, cli);
	}
	else
	{
		wrm_loge("cli:  recvfrom:  unsupported proto=%u.\n", socket->proto());
		reply_to_client(cli, 6);
		return 1;
	}

	//wrm_logi("cli:  recvfrom:  socket=%d waits data.\n", sock);

	return 1;
}

//--------------------------------------------------------------------------------------------------
// return 0 -- if receiver manages current rx buffer yourself
static int process_client_request(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli, uint8_t* frame)
{
	unsigned reqid = mr[3];
	int res = 0;
	switch (reqid)
	{
		case Socket_create:    res = process_cli_socket(tag, mr, cli);         break;
		case Socket_close:     res = process_cli_close(tag, mr, cli);          break;
		case Socket_bind:      res = process_cli_bind(tag, mr, cli);           break;
		case Socket_listen:    res = process_cli_listen(tag, mr, cli);         break;
		case Socket_accept:    res = process_cli_accept(tag, mr, cli);         break;
		case Socket_connect:   res = process_cli_connect(tag, mr, cli);        break;
		case Socket_sendto:    res = process_cli_sendto(tag, mr, cli, frame);  break;
		case Socket_recvfrom:  res = process_cli_recvfrom(tag, mr, cli);       break;
		default:
			wrm_loge("cli:  unknown req_id=%u.\n", reqid);
			reply_to_client(cli, 1);
			res = 1;
	}
	return res;
}
//##################################################################################################
//  Client's requests processing
//##################################################################################################
//##################################################################################################
//  ~ NETWORK PART
//##################################################################################################
//##################################################################################################
//  CLIENT-SERVER PART
//##################################################################################################
static int attach_to_thread(const char* name, L4_thrid_t* thrid)
{
	// get thread ID by name
	word_t key0 = 0;
	word_t key1 = 0;
	L4_thrid_t id = L4_thrid_t::Nil;
	int rc = wrm_nthread_get_id(name, &id, &key0, &key1);
	if (rc)
	{
		wrm_loge("attach:  wrm_nthread_get_id(%s) failed, rc=%u.\n", name, rc);
		return 1;
	}
	wrm_logi("attach:  got id=0x%x/%u, for thread '%s', key:  0x%x 0x%x.\n",
		id.raw(), id.number(), name, key0, key1);

	// attach to thread - send attach msg
	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.untyped(2);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = key0;
	utcb->mr[2] = key1;
	L4_thrid_t from = L4_thrid_t::Nil;
	rc = l4_ipc(id, id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_loge("attach:  l4_ipc() failed, rc=%u.\n", rc);
		return 2;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	if (tag.untyped() != 1  ||  tag.typed() != 0)
	{
		wrm_loge("attach:  wrong msg format.\n");
		return 3;
	}

	if (ecode)
	{
		wrm_loge("attach:  error reply, ecode=%u.\n", ecode);
		return 4;
	}

	*thrid = id;
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int reply_to_client(L4_thrid_t cli, int ecode, const word_t* words, size_t wordscnt,
                           const uint8_t* bf, size_t bfsz, size_t* sent)
{
	//wrm_logw("reply to cli:  ecode=%u, words=%u, bfsz=%u, sent_ptr=0x%x.\n", ecode, wordscnt, bfsz, sent);

	assert(!cli.is_nil());

	L4_utcb_t* utcb = l4_utcb();

	// send propagate msg on behalf of ip-c
	L4_msgtag_t tag;
	tag.propagated(true);
	tag.untyped(1 + wordscnt);
	tag.typed(0);
	int w = 0;
	// put words
	utcb->mr[w++] = tag.raw();
	utcb->mr[w++] = ecode;
	for (unsigned i=0; i<wordscnt; ++i)
	{
		utcb->mr[w++] = words[i];
	}
	// put buffer (buf may have sz = 0)
	if (bf)
	{
		L4_string_item_t sitem;
		sitem.simple((word_t)bf, bfsz);
		utcb->mr[w++] = sitem.word0();
		utcb->mr[w++] = sitem.word1();
		tag.typed(2);
		utcb->mr[0] = tag.raw();
	}
	utcb->sender(net_stack.ip_client);
	int rc = l4_send(cli, L4_time_t::Zero);
	if (rc == Ipc_overflow)
	{
		// some data was been transfered
		unsigned offset = utcb->ipc_error_code().transferred();
		wrm_logw("reply_to_client:  msglen=%u, sent=%u.\n", bfsz, offset);
		if (sent)
			*sent = offset;
	}
	else if (rc) // some error
	{
		wrm_loge("l4_send(cli) failed, rc=%u, cli=%u.\n", rc, cli.number());
		assert(0 && "failed to send(cli) reply");
		return 1;
	}
	else // no error
	{
		if (sent)
			*sent = bfsz;
	}

	//wrm_logw("reply to cli:  done.\n");
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int send_to_eth_drv(const unsigned char* buf, size_t len, size_t* sent)
{
	//wrm_logd("TX: sz=%u.\n", len);

	*sent = 0;

	//((Eth_frame_t*)buf)->dump(1, len);

	L4_utcb_t* utcb = l4_utcb();

	// send propagate msg on behalf of ip-e
	L4_string_item_t sitem;
	sitem.simple((word_t)buf, len);
	L4_msgtag_t tag;
	tag.propagated(true);
	tag.untyped(0);
	tag.typed(2);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = sitem.word0();
	utcb->mr[2] = sitem.word1();
	utcb->sender(net_stack.ip_eth);
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_thrid_t eth = net_stack.eth_tx;
	int rc = l4_ipc(eth, eth, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_loge("l4_ipc(eth_tx) failed, rc=%u.\n", rc);
		return 1;
	}

	// copy msg to preserve MRs
	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	word_t bytes = utcb->mr[2];

	//wrm_logi("received IPC:  from=0x%x/%u, tag=0x%x, u=%u, t=%u, ecode=%u, sent=%u.\n",
	//	from.raw(), from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1], mr[2]);

	if (ecode)
	{
		wrm_loge("eth_drv return ecode=%u.\n", ecode);
		return 2;
	}

	*sent = bytes;
	//wrm_logi("sent to eth:  len=%u.\n", *sent);

	return 0;
}

//--------------------------------------------------------------------------------------------------
static int receive_from_eth_drv(uint8_t* buf, size_t len, size_t* received)
{
	//wrm_logi("receive from eth:  len=%u.\n", len);

	L4_utcb_t* utcb = l4_utcb();

	L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), true);  // allow strings
	L4_string_item_t bitem = L4_string_item_t::create_simple((word_t)buf, len);
	utcb->br[0] = acceptor.raw();
	utcb->br[1] = bitem.word0();
	utcb->br[2] = bitem.word1();

	L4_msgtag_t tag;
	tag.untyped(0);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_thrid_t eth = net_stack.eth_rx;
	int rc = l4_ipc(eth, eth, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		if (rc == Ipc_overflow)
		{
			// some data was been transfered
			L4_string_item_t sitem;
			sitem.set(utcb->mr[2], utcb->mr[3]);
			unsigned offset = utcb->ipc_error_code().transferred();
			wrm_logw("receive from eth:  msglen=%u, transfered=%u.\n", sitem.length(), offset);
			assert(0 && "TODO ME");
		}
		else
		{
			wrm_loge("l4_ipc(eth_rx) failed, rc=%u.\n", rc);
			return 1;
		}
	}

	// copy msg to preserve MRs
	tag = utcb->msgtag();
	word_t mr[64];
	memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));

	//wrm_logi("received IPC:  from=0x%x/%u, tag=0x%x, u=%u, t=%u, ecode=%u.\n",
	//	from.raw(), from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1]);

	assert(tag.untyped() == 1);
	assert(tag.typed()   == 2);

	L4_string_item_t sitem;
	sitem.set(mr[2], mr[3]);
	assert(sitem.is_string_item());
	assert(sitem.pointer() == (word_t)buf);

	*received = sitem.length();
	//wrm_logi("received from eth:  len=%u:\n", *received);

	return mr[1]; // return code from eth
}

//--------------------------------------------------------------------------------------------------
static int eth_thread(int unused)
{
	L4_utcb_t* utcb = l4_utcb();
	wrm_logi("eth_thread:  my global_id=0x%x/%u.\n", utcb->global_id().raw(), utcb->global_id().number());

	int rc = attach_to_thread("eth-tx-stream", &net_stack.eth_tx);
	assert(!rc);

	rc = attach_to_thread("eth-rx-stream", &net_stack.eth_rx);
	assert(!rc);

	uint8_t* buf = 0;
	while (1)
	{
		if (!buf)
			buf = net_stack.frames.get();

		if (!buf)
		{
			wrm_loge("no free frames for rx from eth.\n");
			l4_kdb("No free frames for rx from eth.");
			// TODO:  sleep(1); continue;
		}

		size_t received = 0;
		rc = receive_from_eth_drv(buf, Frames_t::Frame_sz, &received);
		if (rc)
		{
			wrm_loge("receive_from_eth_drv() - failed, rc=%d.\n", rc);
			continue;
		}

		//wrm_logi("RX:  buf=0x%x, sz=%u:\n", buf, received);

		Eth_frame_t* frame = (Eth_frame_t*)buf;
		//frame->dump(1, received);

		wrm_mtx_lock(&net_stack.mtx);
		int res = process_eth_frame(frame, received);
		wrm_mtx_unlock(&net_stack.mtx);

		// if res == 0 -- get new rx buffer, receiver manages current rx buffer yourself
		// if res != 0 -- reuse rx buffers
		if (!res)
			buf = 0;
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------
// wait msgs from clients
static int client_thread(int unused)
{
	L4_utcb_t* utcb = l4_utcb();
	wrm_logi("cli_th:  my global_id=0x%x/%u.\n", utcb->global_id().raw(), utcb->global_id().number());

	// register thread by name
	word_t key0 = 0;
	word_t key1 = 0;
	const char* thread_name = "tcpip-server";
	int rc = wrm_nthread_register(thread_name, &key0, &key1);
	if (rc)
	{
		wrm_loge("cli_th:  wrm_nthread_register(%s) failed, rc=%u.\n", thread_name, rc);
		assert(false);
	}
	wrm_logi("cli_th:  thread '%s' is registered, key:  0x%x 0x%x.\n", thread_name, key0, key1);


	// wait client's request loop
	L4_thrid_t from = L4_thrid_t::Nil;
	uint8_t* buf = 0;
	while (1)
	{
		if (!buf)
			buf = net_stack.frames.get();

		if (!buf)
		{
			wrm_loge("no free frames for tx from eth.\n");
			l4_kdb("No free frames for tx from eth.");
			// TODO:  sleep(1); continue;
		}

		// reserve space for headers (eth, ip, ...)
		size_t space = Eth_frame_t::hdrlen() + Ip_packet_t::Typically_hdr_len +
		               max(Udp_packet_t::hdrlen(), Tcp_packet_t::Typically_hdr_len);
		L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), true);  // allow strings
		L4_string_item_t bitem = L4_string_item_t::create_simple((word_t)buf+space, Frames_t::Frame_sz-space);
		utcb->br[0] = acceptor.raw();
		utcb->br[1] = bitem.word0();
		utcb->br[2] = bitem.word1();

		//wrm_logi("cli_th:  wait client's request.\n");
		int rc = l4_receive(L4_thrid_t::Any, L4_time_t::Never, from);
		if (rc)
		{
			wrm_loge("cli_th:  l4_receive() failed, rc=%u.\n", rc);
			continue;
		}

		word_t ecode = 0;
		L4_msgtag_t tag = utcb->msgtag();
		word_t mr[64];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));

		// each client's request starts from 2 key words
		word_t k0 = mr[1];
		word_t k1 = mr[2];
		if (tag.untyped() < 3)  // k0, k1, Socket_request
		{
			wrm_loge("cli_th:  wrong msg format.\n");
			ecode = 1;
		}

		if (!ecode  &&  (k0 != key0  ||  k1 != key1))
		{
			wrm_loge("cli_th:  wrong keys.\n");
			ecode = 2;
		}

		if (!ecode)
		{
			wrm_mtx_lock(&net_stack.mtx);
			ecode = process_client_request(tag, mr, from, buf);
			wrm_mtx_unlock(&net_stack.mtx);
		}
		else
			reply_to_client(from, ecode/*, NULL, 0, NULL, 0*/);  // send error reply

		// if ecode == 0 -- get new rx buffer, receiver manages current rx buffer yourself
		// if ecode != 0 -- reuse rx buffers
		if (!ecode)
			buf = 0;
	}
	return 0;
}

uint32_t frames_buf[0x800];

//--------------------------------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
	wrm_logi("hello.\n");

	int rc = wrm_mtx_init(&net_stack.mtx);
	assert(!rc && "wrm_mtx_init() - failed");

	net_stack.frames.init((uint8_t*)frames_buf, sizeof(frames_buf));

	L4_utcb_t* utcb = l4_utcb();
	wrm_logi("my global_id=0x%x/%u.\n", utcb->global_id().raw(), utcb->global_id().number());

	// create interfaces  FIXME:  get ifaces from config
	uint64_t mac = 0x2233445566770000ULL;
	mac_t xxx = (uint8_t*)&mac;
	Eth_iface_t* eth_iface = net_stack.eth_ifaces.add(&xxx);
	//uint32_t ip = (192<<24) | (168<<16) |  (2<<8) | 200;
	uint32_t ip = (192<<24) | (168<<16) |  (0<<8) | 100;
	Ip_iface_t* ip_iface = net_stack.ip_ifaces.add(ip, 0xffffff00, eth_iface);
	(void) ip_iface;


	// FIXME:  get app memory from Alpha
	static uint8_t memory [4*Cfg_page_sz] __attribute__((aligned(4*Cfg_page_sz)));
	wrm_mpool_add(L4_fpage_t::create((addr_t)memory, sizeof(memory), Acc_rw));
	// ~FIXME

	// create Eth thread
	L4_fpage_t stack_fp = wrm_mpool_alloc(Cfg_page_sz);
	L4_fpage_t utcb_fp = wrm_mpool_alloc(Cfg_page_sz);
	assert(!stack_fp.is_nil());
	assert(!utcb_fp.is_nil());
	rc = wrm_thread_create(utcb_fp.addr(), eth_thread, 0, stack_fp.addr(), stack_fp.size(),
	                      255, "ip-e", Wrm_thr_flag_no, &net_stack.ip_eth);
	wrm_logi("create_thread:  rc=%d, id=%u.\n", rc, net_stack.ip_eth.number());
	assert(!rc && "failed to create Eth thread");

	// create Client thread
	stack_fp = wrm_mpool_alloc(Cfg_page_sz);
	utcb_fp = wrm_mpool_alloc(Cfg_page_sz);
	assert(!stack_fp.is_nil());
	assert(!utcb_fp.is_nil());
	rc = wrm_thread_create(utcb_fp.addr(), client_thread, 0, stack_fp.addr(), stack_fp.size(),
	                      255, "ip-c", Wrm_thr_flag_no, &net_stack.ip_client);
	wrm_logi("create_thread:  rc=%d, id=%u.\n", rc, net_stack.ip_client.number());
	assert(!rc && "failed to create Client thread");


	// some activity
	while (1)
	{
		// block execution - wait msg from self
		wrm_logw("tcpip helper thread:  block.\n");
		L4_thrid_t from;
		rc = l4_receive(utcb->global_id(), L4_time_t::Never, from);
		if (rc)
		{
			wrm_loge("tcpip:  l4_receive() failed, rc=%u.\n", rc);
			continue;
		}
	}

	return 0;
}
//##################################################################################################
//  CLIENT-SERVER PART
//##################################################################################################
