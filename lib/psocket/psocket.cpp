//##################################################################################################
//
//  psocket - POSIX Socket implementation;
//            used POSIX Socket API from client side and internal PSOCKET API
//            from net_server side.
//
//##################################################################################################

#include "psocket.h"
#include "psocket_opcodes.h"
#include "wrmos.h"
#include "l4_api.h"
#include <assert.h>
#include <unistd.h>

// posix socket api
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//--------------------------------------------------------------------------------------------------
// app local data
//--------------------------------------------------------------------------------------------------

struct Psocket_data_t
{
	struct Net_server_t
	{
		word_t key0;
		word_t key1;
		L4_thrid_t id;
	};
	Net_server_t netsrv;
};
static Psocket_data_t _psocket;

//--------------------------------------------------------------------------------------------------
// Posix socket API
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int init_posix_sockets()
{
	// get thread ID by name
	const char* name = "tcpip-server";
	int rc = wrm_nthread_get_id(name, &_psocket.netsrv.id, &_psocket.netsrv.key0, &_psocket.netsrv.key1);
	if (rc)
	{
		wrm_loge("get_id:  wrm_nthread_get_id(%s) failed, rc=%u.\n", name, rc);
		return 1;
	}
	wrm_logd("Attached to tcpip=%u, key:  0x%x 0x%x.\n",
		_psocket.netsrv.id.number(), _psocket.netsrv.key0, _psocket.netsrv.key1);

	return 0;
}

//--------------------------------------------------------------------------------------------------
int inet_aton(const char* str, struct in_addr* addr)
{
	uint8_t* bytes = (uint8_t*)&addr->s_addr;
	int byte_id = 0;
	int byte = -1;

	while (1)
	{
		char ch = *str++;
		if (ch == '.' ||  ch == '\0')
		{
			if (byte    == -1)  return 0;  // no digits between dots
			if (byte_id ==  4)  return 0;  // too many bytes in string
			bytes[byte_id++] = byte;
			byte = -1;
			if (ch == '\0')
				break;
		}
		else if (ch >= '0'  &&  ch <= '9')
		{
			if (byte == -1)
				byte = ch - '0';
			else
				byte = byte * 10 + (ch - '0');
			if (byte > 0xff)
				return 0;                  // too many digits for byte
		}
		else
			return 0;                      // wrong char
	}
	return 1;
}

//--------------------------------------------------------------------------------------------------
in_addr_t inet_addr(const char* str)
{
	struct in_addr addr;
	int rc = inet_aton(str, &addr);
	return rc ? addr.s_addr : INADDR_NONE;
}

//--------------------------------------------------------------------------------------------------
char* inet_ntoa(struct in_addr in)
{
	enum { Str_num = 2 };
	typedef char string_t [32];
	static string_t strings[Str_num];
	static unsigned id = 0;
	char* str = strings[id];
	id = (id+1)==Str_num ? 0 : (id+1);

	uint8_t* a = (uint8_t*) &in.s_addr;
	snprintf(str, sizeof(string_t), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
	str[sizeof(string_t)-1] = '\0';
	return str;
}

//--------------------------------------------------------------------------------------------------
int socket(int domain, int type, int protocol)
{
	//wrm_logd("psocket:  socket.\n");

	if (_psocket.netsrv.id.is_nil())
	{
		wrm_loge("psocket:  socket:  not inited.\n");
		return -1;
	}

	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(6);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = _psocket.netsrv.key0;
	utcb->mr[2] = _psocket.netsrv.key1;
	utcb->mr[3] = Socket_create;
	utcb->mr[4] = domain;
	utcb->mr[5] = type;
	utcb->mr[6] = protocol;
	L4_thrid_t from = L4_thrid_t::Nil;
	int rc = l4_ipc(_psocket.netsrv.id, _psocket.netsrv.id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_logi("psocket:  socket:  l4_ipc(netsrv) failed, rc=%u.\n", rc);
		return -1;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	word_t sock  = utcb->mr[2];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0)  &&    // err
	   (!(tag.untyped() == 2  &&  tag.typed() == 0)))      // ok
	{
		wrm_loge("psocket:  socket:  l4_ipc(netsrv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -1;
	}

	if (ecode)
	{
		wrm_loge("psocket:  socket:  l4_ipc(netsrv) received ecode=%u.\n", ecode);
		return -1;
	}

	//wrm_logd("psocket:  socket, sock=%d.\n", sock);

	return sock;
}

//--------------------------------------------------------------------------------------------------
int close(int sock)
{
	//wrm_logd("psocket:  close.\n");

	if (_psocket.netsrv.id.is_nil())
	{
		wrm_loge("psocket:  close:  not inited.\n");
		return -1;
	}

	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(4);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = _psocket.netsrv.key0;
	utcb->mr[2] = _psocket.netsrv.key1;
	utcb->mr[3] = Socket_close;
	utcb->mr[4] = sock;
	L4_thrid_t from = L4_thrid_t::Nil;
	int rc = l4_ipc(_psocket.netsrv.id, _psocket.netsrv.id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_logi("psocket:  close:  l4_ipc(netsrv) failed, rc=%u.\n", rc);
		return -1;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0))
	{
		wrm_loge("psocket:  close:  l4_ipc(netsrv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -1;
	}

	if (ecode)
	{
		wrm_loge("psocket:  close:  l4_ipc(netsrv) received ecode=%u.\n", ecode);
		return -1;
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------
int shutdown(int sock, int how)
{
	assert(0 && "Implement me!");
	return 1;
}

//--------------------------------------------------------------------------------------------------
enum
{
	Socket_addr_len_words = 4,
	Socket_addr_len = Socket_addr_len_words * sizeof(word_t)
};

//--------------------------------------------------------------------------------------------------
ssize_t bind(int sock, const struct sockaddr* myaddr, socklen_t mylen)
{
	//wrm_logd("psocket:  bind.\n");

	if (_psocket.netsrv.id.is_nil())
	{
		wrm_loge("psocket:  bind:  not inited.\n");
		return -1;
	}

	if (mylen != Socket_addr_len)
	{
		wrm_loge("psocket:  bind:  unsipported socket address len=%u.\n", mylen);
		return -1;
	}

	const word_t* addr = (word_t*)myaddr;
	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(8);
	tag.typed(0);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0]  = tag.raw();
	utcb->mr[1]  = _psocket.netsrv.key0;
	utcb->mr[2]  = _psocket.netsrv.key1;
	utcb->mr[3]  = Socket_bind;
	utcb->mr[4]  = sock;
	utcb->mr[5]  = addr[0];
	utcb->mr[6]  = addr[1];
	utcb->mr[7]  = addr[2];
	utcb->mr[8]  = addr[3];
	L4_thrid_t from = L4_thrid_t::Nil;
	int rc = l4_ipc(_psocket.netsrv.id, _psocket.netsrv.id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_loge("psocket:  bind:  l4_ipc(netsrv) failed, rc=%u.\n", rc);
		return -1;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0))
	{
		wrm_loge("psocket:  bind:  l4_ipc(netsrv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -1;
	}

	if (ecode)
	{
		wrm_loge("psocket:  bind:  l4_ipc(netsrv) received ecode=%u.\n", ecode);
		return -1;
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------
int listen(int sock, int backlog)
{
	//wrm_logd("psocket:  listen.\n");

	if (_psocket.netsrv.id.is_nil())
	{
		wrm_loge("psocket:  listen:  not inited.\n");
		return -1;
	}

	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(5);
	tag.typed(0);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0]  = tag.raw();
	utcb->mr[1]  = _psocket.netsrv.key0;
	utcb->mr[2]  = _psocket.netsrv.key1;
	utcb->mr[3]  = Socket_listen;
	utcb->mr[4]  = sock;
	utcb->mr[5]  = backlog;
	L4_thrid_t from = L4_thrid_t::Nil;
	int rc = l4_ipc(_psocket.netsrv.id, _psocket.netsrv.id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_loge("psocket:  listen:  l4_ipc(netsrv) failed, rc=%u.\n", rc);
		return -1;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	word_t lsock = utcb->mr[2];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0)  &&    // err
	   (!(tag.untyped() == 2  &&  tag.typed() == 0)))      // ok
	{
		wrm_loge("psocket:  listen:  l4_ipc(netsrv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -1;
	}

	if (ecode)
	{
		wrm_loge("psocket:  listen:  l4_ipc(netsrv) received ecode=%u.\n", ecode);
		return -1;
	}

	return lsock;
}

//--------------------------------------------------------------------------------------------------
int accept(int sock, struct sockaddr* fromaddr, socklen_t* fromlen)
{
	//wrm_logd("psocket:  accept.\n");

	if (_psocket.netsrv.id.is_nil())
	{
		wrm_loge("psocket:  accept:  not inited.\n");
		return -1;
	}

	if (fromaddr  &&  *fromlen != Socket_addr_len)
	{
		wrm_loge("psocket:  accept:  unsupported socket address len=%u.\n", *fromlen);
		return -1;
	}

	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(4);
	tag.typed(0);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0]  = tag.raw();
	utcb->mr[1]  = _psocket.netsrv.key0;
	utcb->mr[2]  = _psocket.netsrv.key1;
	utcb->mr[3]  = Socket_accept;
	utcb->mr[4]  = sock;
	L4_thrid_t from = L4_thrid_t::Nil;
	int rc = l4_ipc(_psocket.netsrv.id, _psocket.netsrv.id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_loge("psocket:  accept:  l4_ipc(netsrv) failed, rc=%u.\n", rc);
		return -1;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	word_t asock = utcb->mr[2];
	if (fromaddr)
	{
		word_t* addr = (word_t*)fromaddr;
		addr[0] = utcb->mr[3];
		addr[1] = utcb->mr[4];
		addr[2] = utcb->mr[5];
		addr[3] = utcb->mr[6];
	}

	if (!(tag.untyped() == 1  &&  tag.typed() == 0)  &&   // err
	    !(tag.untyped() == 6  &&  tag.typed() == 0))      // ok
	{
		wrm_loge("psocket:  accept:  l4_ipc(netsrv) received wrong msg format:  u=%u, t=%u.\n",
			tag.untyped(), tag.typed());
		return -1;
	}

	if (ecode)
	{
		wrm_loge("psocket:  accept:  l4_ipc(netsrv) received ecode=%u.\n", ecode);
		return -1;
	}

	return asock;
}


//--------------------------------------------------------------------------------------------------
int connect(int sock, const struct sockaddr* toaddr, socklen_t tolen)
{
	//wrm_logd("psocket:  connect.\n");

	if (_psocket.netsrv.id.is_nil())
	{
		wrm_loge("psocket:  connect:  not inited.\n");
		return -1;
	}

	if (tolen != Socket_addr_len)
	{
		wrm_loge("psocket:  connect:  unsipported socket address len=%u.\n", tolen);
		return -1;
	}

	const word_t* addr = (word_t*)toaddr;
	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(8);
	tag.typed(0);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0]  = tag.raw();
	utcb->mr[1]  = _psocket.netsrv.key0;
	utcb->mr[2]  = _psocket.netsrv.key1;
	utcb->mr[3]  = Socket_connect;
	utcb->mr[4]  = sock;
	utcb->mr[5]  = addr[0];
	utcb->mr[6]  = addr[1];
	utcb->mr[7]  = addr[2];
	utcb->mr[8]  = addr[3];
	L4_thrid_t from = L4_thrid_t::Nil;
	int rc = l4_ipc(_psocket.netsrv.id, _psocket.netsrv.id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_loge("psocket:  connect:  l4_ipc(netsrv) failed, rc=%u.\n", rc);
		return -1;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0))
	{
		wrm_loge("psocket:  sendto:  l4_ipc(netsrv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -1;
	}

	if (ecode)
	{
		wrm_loge("psocket:  sendto:  l4_ipc(netsrv) received ecode=%u.\n", ecode);
		return -1;
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------
ssize_t sendto(int sock, const void* buf, size_t len, int flags, const struct sockaddr* toaddr, socklen_t tolen)
{
	//wrm_logd("psocket:  sendto:  bufsz=%u.\n", len);

	if (_psocket.netsrv.id.is_nil())
	{
		wrm_loge("psocket:  sendto:  not inited.\n");
		return -1;
	}

	if (toaddr  &&  tolen != Socket_addr_len)
	{
		wrm_loge("psocket:  sendto:  unsipported socket address len=%u.\n", tolen);
		return -1;
	}

	const word_t* addr = (word_t*)toaddr;
	L4_string_item_t sitem;
	sitem.simple((word_t)buf, len);
	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(9);
	tag.typed(2);
	L4_utcb_t* utcb = l4_utcb();
	utcb->mr[0]  = tag.raw();
	utcb->mr[1]  = _psocket.netsrv.key0;
	utcb->mr[2]  = _psocket.netsrv.key1;
	utcb->mr[3]  = Socket_sendto;
	utcb->mr[4]  = sock;
	utcb->mr[5]  = flags;
	utcb->mr[6]  = addr ? addr[0] : 0;
	utcb->mr[7]  = addr ? addr[1] : 0;
	utcb->mr[8]  = addr ? addr[2] : 0;
	utcb->mr[9]  = addr ? addr[3] : 0;
	utcb->mr[10] = sitem.word0();
	utcb->mr[11] = sitem.word1();
	L4_thrid_t from = L4_thrid_t::Nil;
	int rc = l4_ipc(_psocket.netsrv.id, _psocket.netsrv.id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	if (rc)
	{
		wrm_loge("psocket:  sendto:  l4_ipc(netsrv) failed, rc=%u.\n", rc);
		return -1;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	word_t sent  = utcb->mr[2];

	if (!(tag.untyped() == 1  &&  tag.typed() == 0)  &&   // err
	   (!(tag.untyped() == 2  &&  tag.typed() == 0)))     // ok
	{
		wrm_loge("psocket:  sendto:  l4_ipc(netsrv) received wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		return -1;
	}

	if (ecode)
	{
		wrm_loge("psocket:  sendto:  l4_ipc(netsrv) received ecode=%u.\n", ecode);
		return -1;
	}

	//wrm_logd("psocket:  sendto:  bufsz=%u, sent=%u.\n", len, sent);

	return sent;
}

//--------------------------------------------------------------------------------------------------
ssize_t send(int sock, const void* buf, size_t len, int flags)
{
	return sendto(sock, buf, len, flags, NULL, 0);
}

//--------------------------------------------------------------------------------------------------
ssize_t write(int sock, const void* buf, size_t len)
{
	return send(sock, buf, len, 0);
}

//--------------------------------------------------------------------------------------------------
int recvfrom(int sock, void* buf, size_t len, int flags, struct sockaddr* fromaddr, socklen_t* fromlen)
{
	//wrm_logd("psocket:  sock=%d:  recvfrom:  bufsz=%u.\n", sock, len);

	if (_psocket.netsrv.id.is_nil())
	{
		wrm_loge("psocket:  recvfrom:  not inited.\n");
		return -1;
	}

	if (fromaddr  &&  *fromlen != Socket_addr_len)
	{
		wrm_loge("psocket:  recvfrom:  unsupported socket address len=%u.\n", *fromlen);
		return -1;
	}

	L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), true);  // allow strings
	L4_string_item_t bitem = L4_string_item_t::create_simple((word_t)buf, len);
	L4_utcb_t* utcb = l4_utcb();
	utcb->br[0] = acceptor.raw();
	utcb->br[1] = bitem.word0();
	utcb->br[2] = bitem.word1();

	L4_msgtag_t tag;
	tag.propagated(false);
	tag.untyped(5);
	tag.typed(0);
	utcb->mr[0]  = tag.raw();
	utcb->mr[1]  = _psocket.netsrv.key0;
	utcb->mr[2]  = _psocket.netsrv.key1;
	utcb->mr[3]  = Socket_recvfrom;
	utcb->mr[4]  = sock;
	utcb->mr[5]  = flags;
	L4_thrid_t from = L4_thrid_t::Nil;
	int rc = l4_ipc(_psocket.netsrv.id, _psocket.netsrv.id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), from);
	unsigned received = 0;
	if (rc == 4) // MsgOverflow
	{
		// some data was been transfered, but incoming buffer to small
		received = utcb->ipc_error_code().transferred();
		rc = 0;
	}
	else if (rc)
	{
		wrm_loge("psocket:  recvfrom:  l4_ipc(netsrv) failed, rc=%u.\n", rc);
		return -1;
	}

	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	if (fromaddr)
	{
		word_t* addr = (word_t*)fromaddr;
		addr[0] = utcb->mr[2];
		addr[1] = utcb->mr[3];
		addr[2] = utcb->mr[4];
		addr[3] = utcb->mr[5];
	}
	L4_string_item_t sitem;
	sitem.set(utcb->mr[6], utcb->mr[7]);

	if (!(tag.untyped() == 1  &&  tag.typed() == 0)  &&                             // err
	    !(tag.untyped() == 5  &&  tag.typed() == 2   &&  sitem.is_string_item()))   // ok
	{
		wrm_loge("psocket:  recvfrom:  l4_ipc(netsrv) received wrong msg format:  u=%u, t=%u, str=%d.\n",
			tag.untyped(), tag.typed(), sitem.is_string_item());
		return -1;
	}

	if (ecode)
	{
		wrm_loge("psocket:  recvfrom:  l4_ipc(netsrv) received ecode=%u.\n", ecode);
		return -1;
	}

	assert(sitem.pointer() == (word_t)buf);

	if (sitem.pointer() != (word_t)buf)
	{
		wrm_loge("psocket:  recvfrom:  sitem.pointer() != buf, something going wrong.\n");
		return -1;
	}

	if (received)
		wrm_logw("psocket:  recvfrom:  receive=%u, but msgsz=%u.\n", received, sitem.length());

	//wrm_logd("psocket:  sock=%d:  recvfrom:  exit, len=%u.\n", sock, received ? received : sitem.length());

	return received ? received : sitem.length();
}

//--------------------------------------------------------------------------------------------------
ssize_t recv(int sock, void* buf, size_t len, int flags)
{
	return recvfrom(sock, buf, len, flags, NULL, 0);
}

//--------------------------------------------------------------------------------------------------
ssize_t read(int sock, void* buf, size_t len)
{
	return recv(sock, buf, len, 0);
}

//--------------------------------------------------------------------------------------------------
int getsockopt(int sock, int level, int optname, void *optval, socklen_t *optlen)
{
	assert("IMPL ME");
	return 1;
}

//--------------------------------------------------------------------------------------------------
int setsockopt(int sock, int level, int optname, const void *optval, socklen_t optlen)
{
	assert("IMPL ME");
	return 1;
}

//--------------------------------------------------------------------------------------------------
int getpeername(int sock, struct sockaddr* addr, socklen_t* addrlen)
{
	assert("IMPL ME");
	return 1;
}
//--------------------------------------------------------------------------------------------------
// Net_server API
//--------------------------------------------------------------------------------------------------
