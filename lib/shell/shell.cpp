//##################################################################################################
//
//  Shell engine.
//
//##################################################################################################

#include "shell.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// uClibc/stdio.h contents putchar() and getchar() as macro, avoid it
#undef putchar
#undef getchar

Shell_t::Shell_t()
{
}

void Shell_t::prompt(const char* str)
{
	assert(strlen(str) < sizeof(_prompt));
	strncpy(_prompt, str, sizeof(_prompt));
	_prompt[sizeof(_prompt) - 1] = '\0';
}

void Shell_t::add_cmd(const char* name, Shell_t::handler_t handler)
{
	_cmds.push_back(cmd_t(name, handler));
}

void Shell_t::shell()
{
	line_t line;
	unsigned wp = 0;
	int prev_c  = 0;

	update_line(&line, wp);

	fflush(stdin);

	while (1)
	{
		int c = getchar();

		// printf("c=0x%x\n\n", c);

		switch (c)
		{
			// check spec chars
			case 0x09:    // Tab
			{
				wp = find_cmd(&line, wp, prev_c==0x09 ? Show_list : Not_show_list);
				break;
			}
			case 0x0d:    // CR, enter
			{
				int exit = run_cmd(&line);
				history_last(&line);
				if (exit)
					return;
				wp = 0;
				break;
			}
			case 0x1b:     // ESC
			{
				int c1 = 0;
				c1 = getchar();
				if (c1 == 0x5b) // SPEC ?
				{
					int c2 = 0;
					c2 = getchar();
					if (c2 == 0x33)  // ?
					{
						int c3 = 0;
						c3 = getchar();
						if (c3 == 0x7e) // delete
						{
							if (wp != line.len)
								remove_char(&line, wp);
						}
					}
					else if (c2 == 0x41)  // up
					{
						wp = history_prev(&line, wp);
					}
					else if (c2 == 0x42)  // down
					{
						wp = history_next(&line, wp);
					}
					else if (c2 == 0x43)  // right
					{
						wp = wp == line.len  ?  wp  :  (wp + 1);
					}
					else if (c2 == 0x44)  // left
					{
						wp = wp  ?  (wp - 1)  :  0;
					}
				}
				break;
			}

			case 0x7f:     // DEL, backspace
				if (wp)
					remove_char(&line, --wp);
				break;

			// printable chars
			default:
			{
				if ((c >= '0' && c<='9')  ||
				    (c >= 'a' && c<='z')  ||
				    (c >= 'A' && c<='Z')  ||
				    c == ' ')
				{
					if (wp < Line_sz)
						insert_char(&line, wp++, c);
				}
			}
		}

		update_line(&line, wp);
		prev_c = c;
	}
}

void Shell_t::make_words(char* str, unsigned slen, char** argv, int* argc)
{
	int word_start = -1;
	for (unsigned i=0; i<slen; ++i)
	{
		if (word_start == -1  &&  str[i] != ' ')
			word_start = i;

		if (word_start != -1  &&  (str[i] == ' '  ||  (i + 1) == slen))
		{
			argv[(*argc)++] = &str[word_start];
			word_start = -1;
			int null_pos = str[i]==' ' ? i : i+1;
			str[null_pos] = '\0';
		}
	}
}

int Shell_t::find_cmd(line_t* line, unsigned wp, int show_list)
{
	// get first word
	char* word = 0;
	unsigned word_len = 0;
	for (unsigned i=0; i<wp; ++i)
	{
		if (line->str[i] != ' ')
		{
			word = &line->str[i];
			word_len = wp - i;
			break;
		}
	}

	// find matched commands
	cmds_t cmds;
	for (cmds_t::citer_t it=_cmds.cbegin(); it!=_cmds.cend(); ++it)
		if (!strncmp(word, it->name, word_len))
			cmds.push_back(*it);

	// show list commands if need
	if (show_list == Show_list  &&  cmds.size())
	{
		printf("\n\r");
		for (cmds_t::citer_t it=cmds.cbegin(); it!=cmds.cend(); ++it)
			printf(" %s\n", it->name);
	}

	// insert some ar all chars
	if (cmds.size())
	{
		// calc common chars
		unsigned common_chars = 0;
		for (unsigned i=0; i<Line_sz; ++i)
		{
			int ch = cmds.back().name[i];
			for (cmds_t::citer_t it=cmds.cbegin(); it!=cmds.cend(); ++it)
			{
				if (ch == '\0'  ||  ch != it->name[i])
				{
					common_chars = i;
					i = Line_sz;  // to break
					break;
				}
			}
		}
		// insert chars
		assert(common_chars >= word_len);
		const char* full_name = cmds.back().name;
		for (unsigned i=0; i<common_chars-word_len; ++i)
			insert_char(line, wp++, full_name[word_len+i]);

		if (cmds.size() == 1)
			insert_char(line, wp++, ' ');
	}
	return wp;
}

// return shell-exit-flag
int Shell_t::run_cmd(const line_t* line)
{
	char str[Line_sz + 1];
	char* argv[Line_sz / 2];
	int argc = 0;
	strncpy(str, line->str, line->len);
	make_words(str, line->len, argv, &argc);

	puts("");  // new line

	if (argc)
	{
		for (cmds_t::citer_t it=_cmds.cbegin(); it!=_cmds.cend(); ++it)
		{
			if (!strcmp(argv[0], it->name))
			{
				return it->handler(argc, argv);
			}
		}
		printf("%s: command not found\n", argv[0]);
	}
	return 0;
}

// write and clear
void Shell_t::history_last(line_t* line)
{
	if (!line->len)
		return;

	_history[_hist_last].set(line);

	_hist_last = (_hist_last + 1) == History_sz  ?  0  :  (_hist_last + 1);
	if (_hist_first == _hist_last)
		_hist_first = (_hist_first + 1) == History_sz  ?  0  :  (_hist_first + 1);

	_hist_cur = _hist_last;

	_history[_hist_last].clear();
	line->clear();
}

void Shell_t::history_set(const line_t* line)
{
	_history[_hist_cur].set(line);
}

void Shell_t::history_get(line_t* line)
{
	line->set(&_history[_hist_cur]);
}

// return new or old wp
unsigned Shell_t::history_next(line_t* line, unsigned wp)
{
	if (_hist_cur == _hist_last)
		return wp;

	history_set(line);
	_hist_cur = (_hist_cur + 1) == History_sz  ?  0  :  (_hist_cur + 1);
	history_get(line);
	return line->len;
}

// if updated - return true
unsigned Shell_t::history_prev(line_t* line, unsigned wp)
{
	if (_hist_cur == _hist_first)
		return wp;

	history_set(line);
	_hist_cur = _hist_cur ?  (_hist_cur - 1)  : (History_sz - 1);
	history_get(line);
	return line->len;
}

// print current line
void Shell_t::update_line(const line_t* line, size_t wp)
{
	putchar('\r');
	printf("%s", _prompt);
	assert(wp <= line->len);
	printf("%.*s", line->len, line->str);
	for (int i=line->len; i<Line_sz; ++i)
		putchar(' ');
	for (int i=wp; i<Line_sz; ++i)
	{
		// left
		putchar(0x1b);
		putchar(0x5b);
		putchar(0x44);
	}
}

// insert char to current line
void Shell_t::insert_char(line_t* line, size_t wp, char ch)
{
	assert(wp <= line->len);
	assert(line->len <= Line_sz);

	// right shift
	unsigned begin = line->len == Line_sz  ? Line_sz - 1  :  line->len;
	for (unsigned i=begin; i>wp; --i)
		line->str[i] = line->str[i-1];

	// insert
	line->str[wp] = ch;
	if (line->len < Line_sz)
		line->len++;
}

// remove char from current line
void Shell_t::remove_char(line_t* line, size_t wp)
{
	assert(wp <= line->len);
	assert(line->len);

	// left shift
	for (unsigned i=wp; i<line->len-1; ++i)
		line->str[i] = line->str[i+1];

	// remove
	line->len--;
}
