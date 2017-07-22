//##################################################################################################
//
//  Socket - posix socket implementation.
//
//##################################################################################################

#ifndef SOCKET_H
#define SOCKET_H

//--------------------------------------------------------------------------------------------------
// Tcp tream
struct Stream_t
{
	int      state;
	uint32_t rem_addr;
	uint16_t rem_port;

	uint32_t seq_rx;
	uint32_t seq_tx;
	uint32_t init_seq_rx;
	uint32_t init_seq_tx;

	// TODO:  do Lbuf - linear biffer
	uint8_t  buf[0x100];
	size_t   sz;
	size_t   wp;

	enum
	{
		Closed,
		Listen,
		Syn_sent,
		Syn_received,
		Established,
		Fin_wait1,
		Close_wait,
		Fin_wait2,
		Last_ack,
		Time_wait,
		Closing
	};

	Stream_t(uint32_t addr, uint16_t port) :
		state(Closed), rem_addr(addr), rem_port(port),
		seq_rx(0), seq_tx(0), init_seq_rx(0), init_seq_tx(0), sz(sizeof(buf)), wp(0) {}
};

//--------------------------------------------------------------------------------------------------
class Streams_t
{
	typedef list_t<Stream_t, 8> streams_t;
	streams_t _streams;

private:

	streams_t::iter_t find_it(uint32_t addr, uint16_t port)
	{
		for (streams_t::iter_t it=_streams.begin(); it!=_streams.end(); ++it)
			if (it->rem_addr == addr  &&  it->rem_port == port)
				return it;
		return _streams.end();
	}

	streams_t::iter_t find_it(Stream_t* stream)
	{
		for (streams_t::iter_t it=_streams.begin(); it!=_streams.end(); ++it)
			if (&*it == stream)
				return it;
		return _streams.end();
	}

public:

	Stream_t* add(uint32_t addr, uint16_t port)
	{
		_streams.push_back(Stream_t(addr, port));
		return &_streams.back();
	}

	int remove(Stream_t* stream)
	{
		streams_t::iter_t it = find_it(stream);
		if (it != _streams.end())
			_streams.erase(it);

		return it == _streams.end();
	}

	size_t free_sz()
	{
		return _streams.capacity() - _streams.size();
	}
};

static Streams_t allstreams;

uint16_t alloc_free_port(int proto);
bool is_saddr_free(int proto, const sockaddr_in* saddr);

//--------------------------------------------------------------------------------------------------
class Socket_t
{
	static int _cnt;

protected:

	L4_thrid_t _owner;             // creator
	unsigned   _owner_thrno_begin; // app threads begin
	unsigned   _owner_thrno_end;   // app threads end
	L4_thrid_t _client;            // client thread
	L4_thrid_t _recv_client;       // client thread for receive operation
	int _id;
	int _domain;                   // AF_INET
	int _type;                     // SOCK_DGRAM, SOCK_STREAM
	int _proto;                    // UDP, TCP

	int _net_state;                // network state
	int _cli_state;                // client state, to reply in blocking user operations

	bool _bound;                   // bind flag
	sockaddr_in _saddr;            // my saddr

	// storage to wait Arp for sending operation
	bool _wait_arp;
	Eth_frame_t* _tx_eth_frame;
	size_t _tx_eth_frame_len;

	enum Proto_t
	{
		Udp = IPPROTO_UDP,
		Tcp = IPPROTO_TCP
	};

public:

	Socket_t(L4_thrid_t owner, unsigned owner_thrno_begin, unsigned owner_thrno_end,
		     int domain, int type, int proto) :
		_owner(owner), _owner_thrno_begin(owner_thrno_begin), _owner_thrno_end(owner_thrno_end),
		_id(++_cnt), _domain(domain), _type(type), _proto(proto),
		_net_state(0), _cli_state(0), _bound(false),
		_wait_arp(false), _tx_eth_frame(0), _tx_eth_frame_len(0)
	{
		memset(&_saddr, 0, sizeof(_saddr));
	}

	L4_thrid_t   owner()             const { return _owner;             }
	unsigned     owner_thrno_begin() const { return _owner_thrno_begin; }
	unsigned     owner_thrno_end()   const { return _owner_thrno_end;   }
	L4_thrid_t   client()            const { return _client;            }
	L4_thrid_t   recv_client()       const { return _recv_client;       }
	int          id()                const { return _id;                }
	int          domain()            const { return _domain;            }
	int          type()              const { return _type;              }
	int          proto()             const { return _proto;             }
	bool         bound()             const { return _bound;             }
	//int        net_state()         const { return _net_state;         }
	int          cli_state()         const { return _cli_state;         }
	Eth_frame_t* eth_frame()         const { return _tx_eth_frame;      }
	size_t       eth_frame_len()     const { return _tx_eth_frame_len;  }
	uint32_t     saddr_addr()        const { return _saddr.sin_addr.s_addr; }
	uint16_t     saddr_port()        const { return _saddr.sin_port;    }
	const sockaddr_in* saddr()       const { return &_saddr;            }

	bool is_net_state_idle() const { return !_net_state; }
	bool is_cli_state_idle() const { return !_cli_state; }

	bool is_owner(L4_thrid_t id) const
	{
		return id.number() >= _owner_thrno_begin  &&
		       id.number() <  _owner_thrno_end;
	}

	void client(L4_thrid_t id)      { _client = id; }
	void recv_client(L4_thrid_t id) { _recv_client = id; }

	// auto bind (send,recv,...) if have not been bound
	bool bind(uint32_t iaddr)
	{
		if (_bound)
			return true;

		uint16_t port = alloc_free_port(_proto);
		if (!port)
			return false;

		_saddr.sin_family      = AF_INET;
		_saddr.sin_addr.s_addr = htonl(iaddr);
		_saddr.sin_port        = htons(port);
		_bound = true;
		return true;
	}

	// client bind request
	int bind(const sockaddr_in* saddr)
	{
		if (_bound)
			return 1;

		// FIXME:  think
		//bool free = is_saddr_free(_proto, saddr);
		//if (!free)
		//	return 2;

		if (_domain != saddr->sin_family)
			return 3;

		memcpy(&_saddr, saddr, sizeof(_saddr));
		_bound = true;
		return 0;
	}

	int bind(uint32_t addr, uint16_t port)
	{
		sockaddr_in saddr = {(sa_family_t)_domain, port, addr };
		return bind(&saddr);
	}

	bool is_wait_arp() const { return _wait_arp; }
	void wait_arp(bool v)    { _wait_arp = v;    }

	void store_tx_eth_frame(Eth_frame_t* f, size_t l)
	{
		// TODO:  mtx.lock
		//assert(_state == Idle);
		//assert(_bound);
		_tx_eth_frame     = f;
		_tx_eth_frame_len = l;
		//_state         = Tx_wait_arp;
		// TODO:  mtx.unlock
	}
};

// static data
int Socket_t::_cnt = 0;

//--------------------------------------------------------------------------------------------------
class Udp_socket_t : public Socket_t
{
public:

	enum Net_state_t
	{
		Net_idle,
//		Net_wait_arp_for_tx
	};

	enum Cli_state_t
	{
		Cli_idle,      // no current blocking operations
		Cli_send,      // client calls some tx operation and wait reply
		Cli_recv,      // client calls some rx operation and wait reply
	};

	Udp_socket_t(L4_thrid_t owner, unsigned owner_thrno_begin, unsigned owner_thrno_end
	             /*, int domain, int type, int proto, int state*/) :
		Socket_t(owner, owner_thrno_begin, owner_thrno_end, AF_INET, SOCK_DGRAM, IPPROTO_UDP) {}

	Net_state_t net_state() const { return (Net_state_t) _net_state; }
	Cli_state_t cli_state() const { return (Cli_state_t) _cli_state; }

	void net_state(Net_state_t s) { _net_state = s; }
	void cli_state(Cli_state_t s) { _cli_state = s; }

	void recv()
	{
		// TODO:  mtx.lock
		assert(_proto == Udp);
		assert(_bound);
		_cli_state = Cli_recv;
		// TODO:  mtx.unlock
	}

private:

	// received packets

	struct Pkt_t
	{
		sockaddr_in saddr;
		unsigned len;
		unsigned char buf[0x100];

		// for replacement new in list_t
		static void* operator new(size_t sz, Udp_socket_t::Pkt_t* ptr)
		{
			(void)sz;
			return ptr;
		}

		Pkt_t() : len(0) {}
		int save(const sockaddr_in* addr, const unsigned char* bf, unsigned sz)
		{
			if (sz > sizeof(buf))
				return 2;
			memcpy(&saddr, addr, sizeof(sockaddr_in));
			memcpy(buf, bf, sz);
			len = sz;
			return 0;
		}
	};

	typedef list_t<Pkt_t, 16> pkts_t;

	pkts_t _pkts;

public:

	int get_received_packet(sockaddr_in** addr, unsigned char** bf, unsigned* sz)
	{
		if (_pkts.empty())
			return 1;
		*addr = &_pkts.front().saddr;
		*bf = _pkts.front().buf;
		*sz = _pkts.front().len;
		return 0;
	}

	void free_received_packet()
	{
		assert(_pkts.size());
		_pkts.erase(_pkts.begin());
	}

	int save_received_packet(const sockaddr_in* addr, unsigned char* bf, unsigned sz)
	{
		if (_pkts.capacity() == _pkts.size())
			return 1;
		_pkts.push_back();
		return _pkts.back().save(addr, bf, sz);
	}

	// ~ received packets
};

//--------------------------------------------------------------------------------------------------
class Tcp_socket_t : public Socket_t
{
	enum { Backlog_min = 1 };
	enum { Backlog_max = 3 };

private:

	// for listen state
	typedef list_t <Stream_t*, Backlog_max> stream_ptrs_t;
	stream_ptrs_t _streams;             // ptrs to sockets in connecting state
	stream_ptrs_t _connected_queue;     // ptrs to streams that connected and wait for accept()
	size_t        _connected_queue_sz;  // queue size

	// for connected
	Stream_t* _stream;

public:

	// socket type, not state
	enum Net_state_t
	{
		Net_idle,
		Net_listen,      // listen socket - may has many streams
		Net_connection   // connection socket - has only one stream
	};

	enum Cli_state_t
	{
		Cli_idle,      // no current blocking operations
		Cli_connect,   // client calls connect() and wait reply
		Cli_accept,    // client calls accept() and wait reply
		Cli_recv,      // client calls some rx operation and wait reply
		Cli_send,      // client calls some tx operation and wait reply
		Cli_close,     // client calls close() and wait reply
		Cli_closed,    // socket exist but is already closed by client, wait timeout
	};

	Net_state_t net_state() const { return (Net_state_t) _net_state; }
	Cli_state_t cli_state() const { return (Cli_state_t) _cli_state; }

	void net_state(Net_state_t s) { _net_state = s; }
	void cli_state(Cli_state_t s) { _cli_state = s; }

public:

	Tcp_socket_t(L4_thrid_t owner, unsigned owner_thrno_begin, unsigned owner_thrno_end,
	             /*int domain, int type, int proto, int state,*/ Stream_t* stream) :
		Socket_t(owner, owner_thrno_begin, owner_thrno_end, AF_INET, SOCK_STREAM, IPPROTO_TCP/*, state*/),
		_connected_queue_sz(0), _stream(stream)
	{
		if (stream)
			_net_state = Net_connection/*ed*/;
	}

	void listen(int backlog)
	{
		// TODO:  mtx.lock
		assert(_net_state == Net_idle);
		assert(_cli_state == Cli_idle);
		assert(_proto == Tcp);
		assert(_bound);

		// correct backlog value [min, max]
		if (backlog <= 0)
			backlog = Backlog_min;
		else if (backlog > Backlog_max)
			backlog = Backlog_max;

		_connected_queue_sz = backlog;
		_net_state = Net_listen;
		// TODO:  mtx.unlock
	}

	void recv()
	{
		// TODO:  mtx.lock
		assert(_net_state == Net_connection);
		assert(_cli_state == Cli_idle);
		assert(_proto == Tcp);
		assert(_bound);

		_cli_state = Cli_recv;
		// TODO:  mtx.unlock
	}

	int move_to_connected_queue(Stream_t* stream)
	{
		// TODO:  mtx.lock
		assert(_net_state == Net_listen);
		assert(_cli_state == Cli_accept);
		assert(_proto == Tcp);
		assert(_bound);
		int res = 1;
		if (_connected_queue.size() != _connected_queue_sz)
		{
			res = 2;
			stream_ptrs_t::iter_t it = find_stream(_streams, stream);
			if (it != _streams.end())
			{
				res = 0;
				_connected_queue.push_back(*it);
				_streams.erase(it);
			}
		}
		return res;
		// TODO:  mtx.unlock

	}

	Stream_t* get_connected_stream()
	{
		// TODO:  mtx.lock
		assert(_net_state == Net_listen);
		assert(_cli_state == Cli_accept);
		assert(_proto == Tcp);
		assert(_bound);
		Stream_t* res = 0;
		stream_ptrs_t::iter_t it = _connected_queue.begin();
		if (it != _connected_queue.end())
		{
			res = *it;
			_connected_queue.erase(it);
		}
		return res;
		// TODO:  mtx.unlock

	}

private:

	stream_ptrs_t::iter_t find_stream(stream_ptrs_t& streams, Stream_t* ptr)
	{
		for (stream_ptrs_t::iter_t it=streams.begin(); it!=streams.end(); ++it)
			if (*it == ptr)
				return it;
		return _streams.end();
	}

	stream_ptrs_t::iter_t find_stream(stream_ptrs_t& streams, uint32_t addr, uint16_t port)
	{
		for (stream_ptrs_t::iter_t it=streams.begin(); it!=streams.end(); ++it)
			if ((*it)->rem_addr == addr  &&  (*it)->rem_port == port)
				return it;
		return _streams.end();
	}

public:

	// add stream to listen socket
	Stream_t* add_stream(uint32_t addr, uint16_t port)
	{
		// TODO:  mtx.lock
		assert(_net_state == Net_listen);
		//assert(_cli_state == Cli_accept);
		assert(_proto == Tcp);
		assert(_bound);
		Stream_t* res = 0;

		if (allstreams.free_sz()  &&  _streams.size() != _streams.capacity())
		{
			assert(find_stream(_streams, addr, port) == _streams.end());
			Stream_t* s = allstreams.add(addr, port);
			assert(s);
			_streams.push_back(s);
			res = s;
		}
		// TODO:  mtx.unlock
		return res;
	}

	// get connecting stream
	Stream_t* get_stream(uint32_t addr, uint16_t port)
	{
		// TODO:  mtx.lock
		assert(_net_state == Net_listen);
		//assert(_cli_state == Cli_accept);
		assert(_proto == Tcp);
		assert(_bound);
		stream_ptrs_t::iter_t it = find_stream(_streams, addr, port);
		Stream_t* res = it==_streams.end() ? 0 : *it;
		// TODO:  mtx.unlock
		return res;
	}

	// remove stream from connected_queue
	int rem_stream(const Stream_t* stream)
	{
		// TODO:  mtx.lock
		assert(_net_state == Net_listen);
		//assert(_cli_state == Cli_accept);
		assert(_proto == Tcp);
		assert(_bound);
		stream_ptrs_t::iter_t it = find_stream(_connected_queue, stream->rem_addr, stream->rem_port);
		if (it != _connected_queue.end())
			_connected_queue.erase(it);
		// TODO:  mtx.unlock
		return it != _connected_queue.end();
	}

	size_t streams_count()
	{
		return _streams.size() + _connected_queue.size();
	}

	// get stream for connected socket
	Stream_t* stream()
	{
		return _stream;
	}

	// create stream for connecting socket
	Stream_t* stream(uint32_t addr, uint16_t port)
	{
		// TODO:  mtx.lock
		assert(_net_state == Net_connection);
		assert(_proto == Tcp);
		assert(_bound);
		Stream_t* res = 0;

		if (allstreams.free_sz()  &&  _streams.size() != _streams.capacity())
		{
			Stream_t* s = allstreams.add(addr, port);
			assert(s);
			_stream = s;
			res = s;
		}
		// TODO:  mtx.unlock
		return res;
	}

	// remove stream from connected socket
	int rem_stream()
	{
		// TODO:  mtx.lock
		wrm_logd("%s() - _stream=0x%x.\n", __func__, _stream);
		assert(_net_state == Net_connection);
		assert(_cli_state == Cli_closed);
		assert(_proto == Tcp);
		assert(_bound);
		int rc = allstreams.remove(_stream);
		// TODO:  mtx.unlock
		return rc;
	}
};

//--------------------------------------------------------------------------------------------------
class Udp_sockets_t
{
	typedef list_t<Udp_socket_t, 8> sockets_t;
	sockets_t _sockets;

private:

	sockets_t::iter_t find_it(int id)
	{
		for (sockets_t::iter_t it=_sockets.begin(); it!=_sockets.end(); ++it)
			if (it->id() == id)
				return it;
		return _sockets.end();
	}

	sockets_t::iter_t find_it(uint32_t loc_addr, uint16_t loc_port)
	{
		for (sockets_t::iter_t it=_sockets.begin(); it!=_sockets.end(); ++it)
			if ((it->saddr_addr() == INADDR_ANY  ||  it->saddr_addr() == loc_addr)  &&
			    it->saddr_port() == loc_port)
				return it;
		return _sockets.end();
	}

public:

	Socket_t* add(L4_thrid_t owner, unsigned owner_thrno_begin, unsigned owner_thrno_end
	              /*, int domain, int type, int proto, int state = Socket_t::Idle*/)
	{
		Socket_t* res = 0;
		if (_sockets.size() != _sockets.capacity())
		{
			_sockets.push_back(Udp_socket_t(owner, owner_thrno_begin, owner_thrno_end /*, domain, type, proto, state*/));
			res = &_sockets.back();
		}
		return res;
	}

	int remove(int id, L4_thrid_t owner)
	{
		sockets_t::iter_t it = find_it(id);
		if (it != _sockets.end()  &&  it->owner() == owner)
		{
			_sockets.erase(it);
			return 0; // ok
		}
		return 1; // no id or wrong owner
	}

	Socket_t* find(int id)
	{
		sockets_t::iter_t it = find_it(id);
		return it == _sockets.end() ? 0 : &*it;
	}

	Udp_socket_t* find(uint32_t loc_addr, uint16_t loc_port)
	{
		sockets_t::iter_t it = find_it(loc_addr, loc_port);
		return it == _sockets.end() ? 0 : &*it;
	}

	// return !0 if packet is accepted and need break cycle
	typedef int(foreach_func_t)(Socket_t*, int v1, int v2);
	int foreach(foreach_func_t func, int v1, int v2)
	{
		int rc = 0;
		for (sockets_t::iter_t it=_sockets.begin(); it!=_sockets.end(); ++it)
			if ((rc = func(&*it, v1, v2)))
				break;
		return rc;
	}

	static uint16_t alloc_free_port()
	{
		// FIXME
		static unsigned port = 2000;
		if (port > 3000)
			port = 2000;
		return port++;
	}

	bool is_saddr_free(const sockaddr_in* saddr)
	{
		sockets_t::iter_t it = find_it(saddr->sin_addr.s_addr, saddr->sin_port);
		return it == _sockets.end();
	}
};

//--------------------------------------------------------------------------------------------------
class Tcp_sockets_t
{
	typedef list_t<Tcp_socket_t, 8> sockets_t;
	sockets_t _sockets;

private:

	sockets_t::iter_t find_it(int id)
	{
		for (sockets_t::iter_t it=_sockets.begin(); it!=_sockets.end(); ++it)
			if (it->id() == id)
				return it;
		return _sockets.end();
	}

	// find connected stream socket
	sockets_t::iter_t find_it(uint32_t loc_addr, uint16_t loc_port, uint32_t rem_addr, uint16_t rem_port)
	{
		for (sockets_t::iter_t it=_sockets.begin(); it!=_sockets.end(); ++it)
			if ((it->saddr_addr() == INADDR_ANY  ||  it->saddr_addr() == loc_addr)  &&
			    it->saddr_port() == loc_port  &&
			    it->stream()  &&
			    it->stream()->rem_addr == rem_addr  &&  it->stream()->rem_port == rem_port)
				return it;
		return _sockets.end();
	}

	// find listening stream socket
	sockets_t::iter_t find_it(uint32_t loc_addr, uint16_t loc_port)
	{
		for (sockets_t::iter_t it=_sockets.begin(); it!=_sockets.end(); ++it)
			if ((it->saddr_addr() == INADDR_ANY  ||  it->saddr_addr() == loc_addr)  &&
			    it->saddr_port() == loc_port)
				return it;
		return _sockets.end();
	}

public:

	Socket_t* add(L4_thrid_t owner, unsigned owner_thrno_begin, unsigned owner_thrno_end,
	              /*int domain, int type, int proto, int state = Socket_t::Idle,*/ Stream_t* stream = 0)
	{
		Socket_t* res = 0;
		if (_sockets.size() != _sockets.capacity())
		{
			_sockets.push_back(Tcp_socket_t(owner, owner_thrno_begin, owner_thrno_end,
			                   /*domain, type, proto, state,*/ stream));
			res = &_sockets.back();
		}
		return res;
	}

	// create new connected socket
	Socket_t* add(Tcp_socket_t* parent, Stream_t* stream, uint32_t loc_addr)
	{
		assert(parent->net_state() == Tcp_socket_t::Net_listen);

		Socket_t* newsocket = add(parent->owner(), parent->owner_thrno_begin(), parent->owner_thrno_end(),
		                          /*parent->domain(), parent->type(), parent->proto(),
		                            Socket_t::Tcp_connected,*/ stream);
		if (newsocket)
		{
			parent->rem_stream(stream);
			int rc = newsocket->bind(loc_addr, parent->saddr_port());
			assert(!rc && "bind failed");
		}
		return newsocket;
	}

	int remove(int id, L4_thrid_t owner)
	{
		sockets_t::iter_t it = find_it(id);
		if (it != _sockets.end()  &&  it->owner() == owner)
		{
			_sockets.erase(it);
			return 0; // ok
		}
		return 1; // no id or wrong owner
	}

	Tcp_socket_t* find(int id)
	{
		sockets_t::iter_t it = find_it(id);
		return it == _sockets.end() ? 0 : &*it;
	}

	Tcp_socket_t* find_connected_socket(uint32_t loc_addr, uint16_t loc_port, uint32_t rem_addr, uint16_t rem_port)
	{
		sockets_t::iter_t it = find_it(loc_addr, loc_port, rem_addr, rem_port);
		return it == _sockets.end() ? 0 : &*it;
	}

	Tcp_socket_t* find_listening_socket(uint32_t loc_addr, uint16_t loc_port)
	{
		sockets_t::iter_t it = find_it(loc_addr, loc_port);
		return it == _sockets.end() ? 0 : &*it;
	}

	// return !0 if packet is accepted and need break cycle
	typedef int(foreach_func_t)(Socket_t*, int v1, int v2);
	int foreach(foreach_func_t func, int v1, int v2)
	{
		int rc = 0;
		for (sockets_t::iter_t it=_sockets.begin(); it!=_sockets.end(); ++it)
			if ((rc = func(&*it, v1, v2)))
				break;
		return rc;
	}

	static uint16_t alloc_free_port()
	{
		// FIXME
		static unsigned port = 2000;
		if (port > 3000)
			port = 2000;
		return port++;
	}

	bool is_saddr_free(const sockaddr_in* saddr)
	{
		sockets_t::iter_t it = find_it(saddr->sin_addr.s_addr, saddr->sin_port);
		return it == _sockets.end();
	}
};

uint16_t alloc_free_port(int proto)
{
	if (proto == IPPROTO_UDP)
		return Udp_sockets_t::alloc_free_port();
	if (proto == IPPROTO_TCP)
		return Tcp_sockets_t::alloc_free_port();
	return 0;
}

#endif // SOCKET_H
