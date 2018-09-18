//##################################################################################################
//
//  console - implementation console multiplexer as single application.
//
//##################################################################################################

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#include "l4_api.h"
#include "l4_ipcerr.h"
#include "wrmos.h"
#include "list.h"
#include "shell.h"
#include "cbuf.h"
#include "console_opcodes.h"

struct App_t
{
	Wrm_mtx_t     mtx;      // global access mutex

	// local threads
	L4_thrid_t    loc_drv;
	L4_thrid_t    loc_cli;

	// remote drv threads
	L4_thrid_t    drv_tx;
	L4_thrid_t    drv_rx;
};

static App_t app;

Cbuf_t rx_cbuf;

//--------------------------------------------------------------------------------------------------
struct Read_request_t
{
	L4_thrid_t cli;
	size_t     rxsz;

	Read_request_t(L4_thrid_t c, size_t s) : cli(c), rxsz(s) {}
};

//--------------------------------------------------------------------------------------------------
class Cli_app_t
{
	L4_thrid_t _owner;             // creator
	unsigned   _owner_thrno_begin; // app threads begin
	unsigned   _owner_thrno_end;   // app threads end
	char       _name[9];           // app name

	typedef list_t<Read_request_t, 8> rxreqs_t;
	rxreqs_t   _rxreqs;            // read requests

	rxreqs_t::iter_t find_it(L4_thrid_t cli)
	{
		for (rxreqs_t::iter_t it=_rxreqs.begin(); it!=_rxreqs.end(); ++it)
			if (it->cli == cli)
				return it;
		return _rxreqs.end();
	}

public:

	Cli_app_t(L4_thrid_t cli, unsigned thrno_begin, unsigned thrno_end, word_t name) :
		_owner(cli), _owner_thrno_begin(thrno_begin), _owner_thrno_end(thrno_end)
	{
		int maxlen = sizeof(word_t);
		memcpy(_name, &name, maxlen);
		_name[maxlen] = '\0';
	}

	bool is_owner(L4_thrid_t id) const
	{
		return id.number() >= _owner_thrno_begin  &&  id.number() < _owner_thrno_end;
	}

	bool is_owner(unsigned id) const
	{
		return id >= _owner_thrno_begin  &&  id < _owner_thrno_end;
	}

	L4_thrid_t   owner()             const { return _owner;             }
	unsigned     owner_thrno_begin() const { return _owner_thrno_begin; }
	unsigned     owner_thrno_end()   const { return _owner_thrno_end;   }
	const char*  name()              const { return _name;              }

	int wait(L4_thrid_t cli, unsigned rxsz)
	{
		if (_rxreqs.capacity() == _rxreqs.size())
			return -1;  // no free space
		rxreqs_t::iter_t it = find_it(cli);
		if (it != _rxreqs.end())
			return -2;  // already exist
		_rxreqs.push_back(Read_request_t(cli, rxsz));
		return 0;
	}

	int get_wait_request(L4_thrid_t* cli, unsigned* rxsz)
	{
		if (_rxreqs.empty())
			return -1;
		
		rxreqs_t::iter_t it = _rxreqs.begin();
		*cli  = it->cli;
		*rxsz = it->rxsz;
		_rxreqs.erase(it);
		return 0;
	}
};

//--------------------------------------------------------------------------------------------------
class Cli_apps_t
{
	typedef list_t<Cli_app_t, 8> cli_apps_t;
	cli_apps_t _cli_apps;
	Cli_app_t* _input_app;

private:

	cli_apps_t::iter_t find_it(L4_thrid_t cli)
	{
		for (cli_apps_t::iter_t it=_cli_apps.begin(); it!=_cli_apps.end(); ++it)
			if (it->is_owner(cli))
				return it;
		return _cli_apps.end();
	}

	cli_apps_t::iter_t find_it(unsigned cli)
	{
		for (cli_apps_t::iter_t it=_cli_apps.begin(); it!=_cli_apps.end(); ++it)
			if (it->is_owner(cli))
				return it;
		return _cli_apps.end();
	}

public:

	Cli_apps_t() : _input_app(0) {}

	// list API
	typedef cli_apps_t::citer_t citer_t;
	citer_t cbegin() const { return _cli_apps.cbegin(); }
	citer_t cend()   const { return _cli_apps.cend(); }
	typedef cli_apps_t::iter_t iter_t;
	iter_t begin() { return _cli_apps.begin(); }
	iter_t end()   { return _cli_apps.end(); }

	int add(L4_thrid_t cli, unsigned thrno_begin, unsigned thrno_end, word_t name)
	{
		if (_cli_apps.capacity() == _cli_apps.size())
			return -1;  // no free spzce
		cli_apps_t::iter_t it = find_it(cli);
		if (it != _cli_apps.end())
			return -2;  // already exist
		_cli_apps.push_back(Cli_app_t(cli, thrno_begin, thrno_end, name));
		if (_cli_apps.size() == 1)
			_input_app = &_cli_apps.back();
		return 0;
	}

	int remove(L4_thrid_t cli)
	{
		cli_apps_t::iter_t it = find_it(cli);
		if (it != _cli_apps.end())
		{
			if (&*it == _input_app)
				_input_app = 0;
			_cli_apps.erase(it);
		}
		return it!=_cli_apps.end() ? 0 : -1;
	}

	Cli_app_t* get(L4_thrid_t cli)
	{
		cli_apps_t::iter_t it = find_it(cli);
		return it==_cli_apps.end() ? 0 : &*it;
	}

	Cli_app_t* get(unsigned cli)
	{
		cli_apps_t::iter_t it = find_it(cli);
		return it==_cli_apps.end() ? 0 : &*it;
	}

	size_t free_sz()
	{
		return _cli_apps.capacity() - _cli_apps.size();
	}

	size_t size()
	{
		return _cli_apps.size();
	}

	int empty()
	{
		return _cli_apps.empty();
	}

	Cli_app_t* input_cli_app() const { return _input_app; }
	void input_cli_app(Cli_app_t* v) { _input_app = v;    }
};

static Cli_apps_t cli_apps;


static int reply_to_client(L4_thrid_t cli, int ecode,
                           const word_t* words = NULL, unsigned wordscnt = 0,
                           const char* bf = NULL, unsigned bfsz = 0, unsigned* sent = 0);

//##################################################################################################
//  Client's requests processing
//##################################################################################################
//--------------------------------------------------------------------------------------------------

// return ecode
static int process_cli_open(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 4  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  open:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 4);
		return -1;
	}

	// incoming params
	word_t name = mr[4];

	//wrm_logd("cli:  open:  cli=%u, name=%.4s.\n", cli.number(), (char*)&name);

	// get owner threads
	unsigned thrno_begin = 0;
	unsigned thrno_end = 0;
	int rc = wrm_app_threads(cli, &thrno_begin, &thrno_end);
	if (rc)
	{
		wrm_loge("cli:  open:  wrm_app_threads() - rc=%d.\n", rc);
		reply_to_client(cli, 5);
		return -2;
	}

	rc = cli_apps.add(cli, thrno_begin, thrno_end, name);
	if (rc)
	{
		wrm_loge("cli:  open:  failed to add new client, rc=%d.\n", rc);
		reply_to_client(cli, 6);
		return -3;
	}

	reply_to_client(cli, 0);  // ok
	return 0;
}

// return ecode
static int process_cli_close(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 3  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  close:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 4);
		return -1;
	}

	// incoming params - no params

	//wrm_logd("cli:  close:  cli=%u.\n", cli.number());

	int rc = cli_apps.remove(cli);
	if (rc)
	{
		wrm_loge("cli:  close:  failed to remove client, rc=%d.\n", rc);
		reply_to_client(cli, 5);
		return -2;
	}

	reply_to_client(cli, 0);  // ok
	return 0;
}


//--------------------------------------------------------------------------------------------------

static int send_to_drv(const char* buf, unsigned len, unsigned* sent);

// return ecode
static int process_cli_write(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 3  ||  tag.typed() != 2)
	{
		wrm_loge("cli:  write:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 4);
		return -1;
	}

	// incoming params
	L4_string_item_t sitem;
	sitem.set(mr[4], mr[5]);
	assert(sitem.is_string_item());

	//wrm_logd("cli:  write:  cli=%u, bfsz=%u.\n", cli.number(), sitem.length());

	// find client
	Cli_app_t* c = cli_apps.get(cli);
	if (!c)
	{
		wrm_loge("cli:  write:  unknown cli=%d.\n", cli.number());
		reply_to_client(cli, 5);
		return -2;
	}

	unsigned sent = 0;
	int rc = send_to_drv((char*)sitem.pointer(), sitem.length(), &sent);
	if (rc)
	{
		wrm_loge("cli:  write:  send_to_drv() - rc=%d.\n", rc);
		reply_to_client(cli, 6);
		return -3;
	}

	reply_to_client(cli, 0, (word_t*)&sent, 1);
	return 0;
}

static int receive_from_drv(char* buf, unsigned len, unsigned* received);

static void process_cli_read(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli)
{
	if (tag.untyped() != 5  ||  tag.typed() != 0)
	{
		wrm_loge("cli:  read:  wrong msg format:  u=%u, t=%u.\n", tag.untyped(), tag.typed());
		reply_to_client(cli, 2);
		return;
	}

	// incoming params
	int inc_sz = mr[4];

	//wrm_logd("cli:  read:  cli=%u, bfsz=%u.\n", cli.number(), inc_sz);

	// find client
	Cli_app_t* c = cli_apps.get(cli);
	if (!c)
	{
		wrm_loge("cli:  read:  unknown cli=%d.\n", cli.number());
		reply_to_client(cli, 5);
		return;
	}

	// check input owner
	int inp = c == cli_apps.input_cli_app();
	if (!inp)
	{
		//wrm_logd("cli:  read:  no input cli_app, hang.\n");
		int rc = c->wait(cli, inc_sz);
		if (rc)
		{
			wrm_loge("cli:  read:  failed to add waiter, rc=%d.\n", rc);
			reply_to_client(cli, 6);
		}
		return;
	}

	char buf[256];
	unsigned read = rx_cbuf.read(buf, min(inc_sz, sizeof(buf)));
	if (!read)
	{
		//wrm_logd("cli:  read:  no data for read, wait rx.\n");
		int rc = c->wait(cli, inc_sz);
		if (rc)
		{
			wrm_loge("cli:  read:  failed to add waiter, rc=%d.\n", rc);
			reply_to_client(cli, 7);
		}
		return;
	}

	unsigned sent = 0;
	int rc = reply_to_client(cli, 0, 0, 0, buf, read, &sent);
	if (rc  ||  read != sent)
	{
		wrm_loge("cli:  read:  reply_to_client() - rc=%d, send=%u, sent=%u, lost=%u.\n",
			rc, read, sent, read-sent);
	}
}

//--------------------------------------------------------------------------------------------------
// return errcode
static void process_client_request(L4_msgtag_t tag, const word_t* mr, L4_thrid_t cli, char* buf)
{
	unsigned reqid = mr[3];
	switch (reqid)
	{
		case Console_open:   process_cli_open(tag, mr, cli);   break;
		case Console_close:  process_cli_close(tag, mr, cli);  break;
		case Console_read:   process_cli_read(tag, mr, cli);   break;
		case Console_write:  process_cli_write(tag, mr, cli);  break;
		default:
			wrm_loge("cli:  unknown req_id=%u.\n", reqid);
			reply_to_client(cli, 3);
	}
}
//##################################################################################################
//  Client's requests processing
//##################################################################################################
//##################################################################################################
//  ~ NETWORK PART
//##################################################################################################

#include "wlibc_cb.h"
/*
void out_string(const char* str, size_t len)
{
	unsigned sent = 0;
	int rc = send_to_drv(str, len, &sent);
	if (rc || sent != len)
	{
		wrm_loge("%s:  rc=%d, sent=%d, expected %u.\n", __func__, rc, sent, len);
	}
}
*/
int in_char()
{
	unsigned received = 0;
	char ch = 0;
	int rc = receive_from_drv(&ch, 1, &received);
	if (rc || received != 1)
	{
		wrm_loge("%s:  rc=%d, received=%d, expected 1.\n", __func__, rc, received);
	}
	return ch;
}

// after call this function printf() will read/write to/from uart driver
void libcio_set_drv_input()
{
	Wlibc_callbacks_t* cb = wlibc_callbacks_get();
	cb->in_char = in_char;
}

//--------------------------------------------------------------------------------------------------
static int cmd_exit(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	printf("  Go back to stdin/stdout ...\n");
	return 1;
}

//--------------------------------------------------------------------------------------------------
static int cmd_kdb(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	int error_entry = 0;
	l4_kdb(L4_kdb_console, error_entry, (void*)"Console", 0);
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int cmd_list(unsigned argc, char** argv)
{
	(void) argc;
	(void) argv;
	printf("  client_apps(%zu):\n", cli_apps.size());
	printf("    name  owner  input\n");
	for (Cli_apps_t::citer_t it=cli_apps.cbegin(); it!=cli_apps.cend(); ++it)
		printf("    %4.4s    %3u      %c\n",
			it->name(), it->owner().number(), cli_apps.input_cli_app()==&*it?'*':'-');
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int cmd_input(unsigned argc, char** argv)
{
	// get inc param

	if (argc != 2)
	{
		printf("  Use:  input <id>\n");
		return 0;
	}
	unsigned cli = strtoul(argv[1], NULL, 0);

	// set input app as input app

	Cli_app_t* cli_app = cli_apps.get(cli);
	if (!cli_app)
	{
		printf("  Bad id %u.\n", cli);
		return 0;
	}
	if (cli_apps.input_cli_app() == cli_app)
	{
		return 0;  // new id is the same input app
	}
	cli_apps.input_cli_app(cli_app);
	printf("  Set new input app %u.\n", cli);
	rx_cbuf.clear();
	return 0;
}


//--------------------------------------------------------------------------------------------------
class Cons_shell_t
{
	static Shell_t _shell;

public:

	static void init()
	{
		_shell.add_cmd("exit",    cmd_exit);
		_shell.add_cmd("kdb",     cmd_kdb);
		_shell.add_cmd("list",    cmd_list);
		_shell.add_cmd("input",   cmd_input);

		_shell.prompt("\x1b[0;36mcon> \x1b[0m");
	}

	// entry may be kernel/user and error/noerror
	static void entry(const char* prompt)
	{
		init();

		const char* line    = "+-------------------------------------------------------------------------------\n";
		const char* color   = "\x1b[0;36m";
		const char* nocolor = "\x1b[0m";

		puts("");
		printf(color);
		printf(line);
		printf("|  %s\n", prompt);
		printf(line);
		printf(nocolor);

		_shell.shell();
	}
};

// static data for Cons_shell_t
Shell_t Cons_shell_t::_shell;

enum
{
	Key_ctrl_a = 0x01,
	Key_ctrl_e = 0x05,
	Key_ctrl_w = 0x17,

	Key_entry_shell  = Key_ctrl_w,
	Key_entry_kdb    = Key_ctrl_a,
	Key_switch_input = Key_ctrl_e,
};

static int process_input_chars(char* buf, unsigned sz)
{
	//wrm_logd("%s:  sz=%u, buf[0]=0x%x.\n", __func__, sz, buf[0]);

	// find Ch_ctrl_w
	bool need_shell = 0;
	bool need_kdb = 0;
	bool need_sw_inp = 0;
	for (unsigned i=0; i<sz; ++i)
	{
		if (buf[i] == Key_entry_shell)
		{
			need_shell = 1;
			break;
		}
		if (buf[i] == Key_entry_kdb)
		{
			need_kdb = 1;
			break;
		}
		if (buf[i] == Key_switch_input)
		{
			need_sw_inp = 1;
			break;
		}
	}

	if (need_shell)
	{
		// discard all input data and execute console shell
		rx_cbuf.clear();
		Cons_shell_t::entry("Wrmos console shell");
	}
	else if (need_kdb)
	{
		// discard all input data and entry kdb
		rx_cbuf.clear();
		int error_entry = 0;
		l4_kdb(L4_kdb_console, error_entry, (void*)"Console", 0);
	}
	else if (need_sw_inp)
	{
		// discard all input data and switch input app
		rx_cbuf.clear();
		if (cli_apps.empty())
		{
			printf("[cons]  no app for input.\n");
		}
		else
		{
			Cli_apps_t::iter_t it = cli_apps.begin();
			for ( ; it!=cli_apps.end() && cli_apps.input_cli_app()!=&*it; ++it);  // find cur input
			if (it == cli_apps.end()  ||  (it+1) == cli_apps.end())
				it = cli_apps.begin();
			else
				++it;
			cli_apps.input_cli_app(&*it);
			printf("[cons]  new input app is %u/%s.\n", it->owner().number(), it->name());
		}
	}
	else
	{
		// send read reply if need
		Cli_app_t* input_app = cli_apps.input_cli_app();
		if (input_app)
		{
			unsigned sent = 0;
			L4_thrid_t read_cli;
			unsigned read_sz;
			int rc = input_app->get_wait_request(&read_cli, &read_sz);
			if (!rc)
			{
				int rc = reply_to_client(read_cli, 0, 0, 0, buf, min(read_sz, sz), &sent);
				if (rc)
					wrm_loge("reply_to_client() - rc=%d.\n", rc);
			}
			if (sent != sz)
			{
				// store incomming data to rx_cbuf
				//wrm_logd("store inc data to rx_cbuf:  inc=%u, sent_to_cli=%u, store=%u.\n", sz, sent, sz-sent);
				unsigned written = rx_cbuf.write(buf+sent, sz-sent);
				if (written != (sz-sent))
					wrm_loge("internal rx_cbuf overfull:  write=%u, written=%u.\n", sz-sent, written);
			}
		}
	}
	return 0;
}

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
	wrm_logi("attach:  got id=%u, for thread '%s', key:  %lx/%lx.\n", id.number(), name, key0, key1);

	// attach to thread - send attach msg
	L4_utcb_t* utcb = l4_utcb();
	L4_msgtag_t tag;
	tag.untyped(2);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = key0;
	utcb->mr[2] = key1;
	L4_thrid_t from = L4_thrid_t::Nil;
	rc = l4_ipc(id, id, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), &from);
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
		wrm_loge("attach:  error reply, ecode=%ld.\n", ecode);
		return 4;
	}

	*thrid = id;
	return 0;
}

//--------------------------------------------------------------------------------------------------
static int reply_to_client(L4_thrid_t cli, int ecode, const word_t* words, unsigned wordscnt,
                           const char* bf, unsigned bfsz, unsigned* sent)
{
	//wrm_logd("reply to cli:  ecode=%d, words=%u, bfsz=%u, sent_ptr=0x%p.\n", ecode, wordscnt, bfsz, sent);

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
	utcb->sender(app.loc_cli);
	int rc = l4_send(cli, L4_time_t::Zero);
	if (rc == L4_ipc_overflow)
	{
		// some data was been transfered
		unsigned offset = utcb->ipc_error_code().transferred();
		wrm_logw("reply_to_client:  msglen=%u, sent=%u.\n", bfsz, offset);
		if (sent)
			*sent = offset;
	}
	else if (rc) // some error
	{
		if (rc != L4_ipc_timeout)  // Timeout possible then rx from client was with Overflow
			wrm_loge("reply_to_client:  l4_send(cli) rc=%d/%s, cli=%u.\n", rc, l4_ipcerr2str(rc), cli.number());
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
static int send_to_drv(const char* buf, unsigned len, unsigned* sent)
{
	//wrm_logd("TX: sz=%u.\n", len);

	*sent = 0;

	//((Eth_frame_t*)buf)->dump(1, len);

	L4_utcb_t* utcb = l4_utcb();

	// send propagate msg on behalf of c-dr
	L4_string_item_t sitem;
	sitem.simple((word_t)buf, len);
	L4_msgtag_t tag;
	tag.propagated(true);
	tag.untyped(0);
	tag.typed(2);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = sitem.word0();
	utcb->mr[2] = sitem.word1();
	utcb->sender(app.loc_drv);
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_thrid_t to = app.drv_tx;
	int rc = l4_ipc(to, to, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), &from);
	if (rc)
	{
		wrm_loge("l4_ipc(drv_tx) - rc=%u.\n", rc);
		return -1;
	}

	// copy msg to preserve MRs
	tag = utcb->msgtag();
	word_t ecode = utcb->mr[1];
	word_t bytes = utcb->mr[2];

	//wrm_logi("received IPC:  from=%u, tag=0x%x, u=%u, t=%u, ecode=%ld, sent=%u.\n",
	//	from.raw(), from.number(), tag.untyped(), tag.typed(), mr[1], mr[2]);

	if (ecode)
	{
		wrm_loge("drv return ecode=%ld.\n", ecode);
		return -2;
	}

	*sent = bytes;
	//wrm_logi("sent to drv:  len=%u.\n", *sent);

	return 0;
}

//--------------------------------------------------------------------------------------------------
static int receive_from_drv(char* buf, unsigned len, unsigned* received)
{
	//wrm_logi("receive from drv:  len=%u.\n", len);

	L4_utcb_t* utcb = l4_utcb();

	L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), true);  // allow strings
	L4_string_item_t bitem = L4_string_item_t::create_simple((word_t)buf, len);
	utcb->br[0] = acceptor.raw();
	utcb->br[1] = bitem.word0();
	utcb->br[2] = bitem.word1();

	L4_msgtag_t tag;
	tag.untyped(1);
	tag.typed(0);
	utcb->mr[0] = tag.raw();
	utcb->mr[1] = len;
	L4_thrid_t from = L4_thrid_t::Nil;
	L4_thrid_t to = app.drv_rx;
	int rc = l4_ipc(to, to, L4_timeouts_t(L4_time_t::Never, L4_time_t::Never), &from);
	if (rc)
	{
		if (rc == L4_ipc_overflow)
		{
			// some data was been transfered
			L4_string_item_t sitem;
			sitem.set(utcb->mr[2], utcb->mr[3]);
			unsigned offset = utcb->ipc_error_code().transferred();
			wrm_logw("receive from drv:  msglen=%u, transfered=%u.\n", sitem.length(), offset);
			assert(0 && "TODO ME");
		}
		else
		{
			wrm_loge("l4_ipc(drv_rx) - rc=%d.\n", rc);
			return 1;
		}
	}

	// copy msg to preserve MRs
	tag = utcb->msgtag();
	word_t mr[256];
	memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));

	//wrm_logi("received ipc:  from=%u, tag=0x%x, u=%u, t=%u, ecode=%ld.\n",
	//	from.number(), tag.raw(), tag.untyped(), tag.typed(), mr[1]);

	assert(tag.untyped() == 1);
	assert(tag.typed()   == 2);

	L4_string_item_t sitem;
	sitem.set(mr[2], mr[3]);
	assert(sitem.is_string_item());
	assert(sitem.pointer() == (word_t)buf);

	*received = sitem.length();
	//wrm_logi("received from drv:  len=%u:\n", *received);

	return mr[1]; // return ecode from drv
}

//--------------------------------------------------------------------------------------------------
// wait data from driver
static long driver_rx_thread(long unused)
{
	L4_utcb_t* utcb = l4_utcb();
	wrm_logi("drv:  myid=%u.\n", utcb->global_id().number());

	int rc = attach_to_thread("uart-tx-stream", &app.drv_tx);
	assert(!rc);

	rc = attach_to_thread("uart-rx-stream", &app.drv_rx);
	assert(!rc);

	static char buffer[0x1000]; // static to be global, not stack
	char* buf = buffer;
	while (1)
	{
		unsigned received = 0;
		rc = receive_from_drv(buf, sizeof(buffer), &received);
		if (rc)
		{
			wrm_loge("receive_from_drv() - rc=%d.\n", rc);
			continue;
		}

		//wrm_logi("RX:  buf=0x%x, sz=%u:  %.*s.\n", buf, received, received, buf);

		wrm_mtx_lock(&app.mtx);
		process_input_chars(buf, received);
		wrm_mtx_unlock(&app.mtx);
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------
// wait msgs from clients
static long client_thread(long unused)
{
	L4_utcb_t* utcb = l4_utcb();
	wrm_logi("cli:  myid=%u.\n", utcb->global_id().number());

	// register thread by name
	word_t key0 = 0;
	word_t key1 = 0;
	const char* thread_name = "console-server";
	int rc = wrm_nthread_register(thread_name, &key0, &key1);
	if (rc)
	{
		wrm_loge("cli:  wrm_nthread_register(%s) failed, rc=%u.\n", thread_name, rc);
		assert(false);
	}
	wrm_logi("cli:  thread '%s' is registered, key:  %lx/%lx.\n", thread_name, key0, key1);


	// wait client's request loop
	L4_thrid_t from = L4_thrid_t::Nil;
	static char buffer[0x1000]; // static to be global, not stack
	char* buf = buffer;
	while (1)
	{
		L4_acceptor_t acceptor = L4_acceptor_t::create(L4_fpage_t::create_nil(), true);  // allow strings
		L4_string_item_t bitem = L4_string_item_t::create_simple((word_t)buf, sizeof(buffer));
		utcb->br[0] = acceptor.raw();
		utcb->br[1] = bitem.word0();
		utcb->br[2] = bitem.word1();

		//wrm_logd("cli:  wait client's request.\n");
		int rc = l4_receive(L4_thrid_t::Any, L4_time_t::Never, &from);
		if (rc && rc != L4_ipc_overflow)
		{
			L4_ipcerr_t ipcerr = utcb->ipc_error_code();
			wrm_loge("cli:  l4_receive() - rc=%d/%s, trans=%lu, %s%s.\n", rc, l4_ipcerr2str(rc),
				ipcerr.transferred(), ipcerr.is_snd_error()?"snderr":"", ipcerr.is_rcv_error()?"rcverr":"");
			continue;
		}

		word_t ecode = 0;
		L4_msgtag_t tag = utcb->msgtag();
		word_t mr[256];
		memcpy(mr, utcb->mr, (1 + tag.untyped() + tag.typed()) * sizeof(word_t));

		// each client's request starts from 2 key words
		word_t k0 = mr[1];
		word_t k1 = mr[2];
		if (tag.untyped() < 3)  // k0, k1, request_id
		{
			wrm_loge("cli:  wrong msg format.\n");
			ecode = 1;
		}

		if (!ecode  &&  (k0 != key0  ||  k1 != key1))
		{
			wrm_loge("cli:  wrong keys.\n");
			ecode = 2;
		}

		if (!ecode)
		{
			wrm_mtx_lock(&app.mtx);
			process_client_request(tag, mr, from, buf);
			wrm_mtx_unlock(&app.mtx);
		}
		else
		{
			reply_to_client(from, ecode);  // send error reply
		}
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
	wrm_logi("hello:  myid=%u.\n", l4_utcb()->global_id().number());

	int rc = wrm_mtx_init(&app.mtx);
	assert(!rc && "wrm_mtx_init() - failed");

	static char rx_buf[0x1000];
	rx_cbuf.init(rx_buf, sizeof(rx_buf));

	// create driver thread
	L4_fpage_t stack_fp = wrm_pgpool_alloc(Cfg_page_sz);
	L4_fpage_t utcb_fp = wrm_pgpool_alloc(Cfg_page_sz);
	assert(!stack_fp.is_nil());
	assert(!utcb_fp.is_nil());
	rc = wrm_thr_create(utcb_fp, driver_rx_thread, 0, stack_fp.addr(), stack_fp.size(),
	                    255, "c-dr", Wrm_thr_flag_no, &app.loc_drv);
	wrm_logi("create_thread:  rc=%d, id=%u.\n", rc, app.loc_drv.number());
	assert(!rc && "failed to create driver thread");

	// create Client thread
	stack_fp = wrm_pgpool_alloc(Cfg_page_sz);
	utcb_fp = wrm_pgpool_alloc(Cfg_page_sz);
	assert(!stack_fp.is_nil());
	assert(!utcb_fp.is_nil());
	rc = wrm_thr_create(utcb_fp, client_thread, 0, stack_fp.addr(), stack_fp.size(),
	                    255, "c-cl", Wrm_thr_flag_no, &app.loc_cli);
	wrm_logi("create_thread:  rc=%d, id=%u.\n", rc, app.loc_cli.number());
	assert(!rc && "failed to create Client thread");

	libcio_set_drv_input();

	while (1)
	{
		sleep(1);
		//wrm_logw("I'm alive!\n");
	}

	return 0;
}
//##################################################################################################
//  CLIENT-SERVER PART
//##################################################################################################
