//##################################################################################################
//
//  Shell engine.
//
//##################################################################################################

#ifndef SHELL_H
#define SHELL_H

#include "list.h"
#include <string.h>

class Shell_t
{
	typedef int (*handler_t)(unsigned argc, char** argp);

	// commands
	struct cmd_t
	{
		const char* name;
		unsigned    name_len;
		handler_t   handler;

		cmd_t(const char* n, handler_t h) : name(n), handler(h)
		{
			name_len = strlen(name);
		}
	};
	typedef list_t <cmd_t, 16> cmds_t;
	cmds_t _cmds;

	// history
	enum
	{
		Line_sz    = 32,
		History_sz = 32,
	};
	struct line_t
	{
		char str [Line_sz];
		unsigned len;
		line_t() : len(0) {}
		void clear() { len = 0; }
		void set(const line_t* l)
		{
			strncpy(str, l->str, l->len);
			len = l->len;
		}
	};
	typedef line_t history_t [History_sz];
	history_t _history;     // history lines
	unsigned  _hist_first;  // id of first history line
	unsigned  _hist_last;   // id of last  history line
	unsigned  _hist_cur;    // id of cur   history line

	char _prompt[32];

	enum
	{
		Show_list,
		Not_show_list
	};

public:

	Shell_t();
	void prompt(const char* str);
	void add_cmd(const char* name, handler_t handler);
	void shell();

private:

	void make_words(char* str, unsigned slen, char** argv, int* argc);
	int find_cmd(line_t* line, unsigned wp, int show_list);
	int run_cmd(const line_t* line);
	void history_last(line_t* line);
	void history_set(const line_t* line);
	void history_get(line_t* line);
	unsigned history_next(line_t* line, unsigned wp);
	unsigned history_prev(line_t* line, unsigned wp);
	void update_line(const line_t* line, size_t wp);
	void insert_char(line_t* line, size_t wp, char ch);
	void remove_char(line_t* line, size_t wp);
};

#endif // SHELL_H
