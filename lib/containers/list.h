//##################################################################################################
//
//  list.h - template container.
//
//##################################################################################################

#ifndef LIST_H
#define LIST_H

typedef void (*list_dprint_t)(const char* format, ...) __attribute__((format(printf, 1, 2)));
#define List_no_dprint ((list_dprint_t)0)

#ifdef DEBUG
#  define print(...)  if (dprint) dprint(__VA_ARGS__)
#else
#  define print(...) (void)dprint
#endif

#include <stdint.h>
#include <stddef.h>
#include "wlibc_assert.h"
#include "wlibc_panic.h"

// FIXME:
typedef unsigned long addr_t;

typedef void* (*list_malloc_t)(size_t sz_req, size_t* sz_rep);

// Sorted dynamicaly allocated doubly linked list.
// Memory is allocating by pages.
template <typename T, size_t N = 0>
class list_t
{
	// list item
	struct Item
	{
		Item* prev;
		Item* next;
		T data;
	};

	enum where_t
	{
		Next,
		Prev
	};

	// internal doubly linked list
	struct Xlist
	{
		Item* begin;
		Item* last;
		size_t sz;
		Xlist() : begin(0), last(0), sz(0) {}

		// check xlist correctness and pos_in_list, need for debug
		inline void do_check(Item* pos) const
		{
			wassert(sz || (begin==0 && last==0));
			wassert(sz<=1 || begin!=last);
			wassert(sz>1 || begin==last);

			// check forward
			unsigned cnt = 0;
			Item* item = begin;
			Item* last_item = 0;
			bool pos_in_list = false;
			while (item)
			{
				cnt++;
				last_item = item ? item : last_item;
				pos_in_list |= pos == item;
				item = item->next;
			}
			wassert(last == last_item);
			wassert(sz == cnt);
			wassert(pos_in_list || (!sz && !pos));

			// check backward
			cnt = 0;
			item = last;
			last_item = 0;
			pos_in_list = false;
			while (item)
			{
				cnt++;
				last_item = item ? item : last_item;
				pos_in_list |= pos == item;
				item = item->prev;
			}
			wassert(begin == last_item);
			wassert(sz == cnt);
			wassert(pos_in_list || (!sz && !pos));
		}

		inline void check(Item* pos) const
		{
			#ifdef DEBUG
			do_check(pos);
			#else
			(void) pos;
			#endif
		}

		inline void remove(Item* item)
		{
			//print("    Xlist::%s:  before:  bgn=%x, lst=%x, sz=%zu, itm=%x.\n", __func__, begin, last, sz, item);

			wassert(sz);
			check(begin);

			if (item->next)
				item->next->prev = item->prev;
			if (item->prev)
				item->prev->next = item->next;
			if (item == begin)
				begin = item->next;
			if (item == last)
				last = item->prev;
			sz--;

			//print("    Xlist::%s:  after:   bgn=%x, lst=%x, sz=%zu, itm=%x.\n", __func__, begin, last, sz, item);
			check(last);
		}

		inline void add(Item* pos, where_t where, Item* item, bool need_check = true)
		{
			//print("    Xlist::%s:  before:  bgn=%x, lst=%x, sz=%zu, pos=%x, where=%s, new_itm=%x.\n",
			//	__func__, begin, last, sz, pos, where==Next?"next":"prev", item);

			if (need_check)
				check(pos);

			if (!sz)
			{
				begin = last = item;
				item->prev = item->next = 0;
			}
			else
			if (where == Next)
			{
				if (pos->next)
					pos->next->prev = item;
				item->next = pos->next;
				pos->next = item;
				item->prev = pos;
				if (pos == last)
					last = item;
			}
			else
			if (where == Prev)
			{
				if (pos->prev)
					pos->prev->next = item;
				item->prev = pos->prev;
				pos->prev = item;
				item->next = pos;
				if (pos == begin)
					begin = item;
			}
			sz++;

			//print("    Xlist::%s:  after:   bgn=%x, lst=%x, sz=%zu, pos_itm=%x, where=%s, new_itm=%x.\n",
			//	__func__, begin, last, sz, pos, where==Next?"next":"prev", item);
			if (need_check)
				check(last);
		}

		// find first (pos->data >= val) or NULL
		inline Item* find_pos(T val) const
		{
			if (!sz)
				return 0;

			Item* item = begin;
			while (item)
			{
				if (!(item->data < val)) // >=
					return item;
				item = item->next;
			}
			return 0;
		}
	};

	// data
	list_dprint_t dprint;
	const char*   name;
	list_malloc_t malloc_func;
	Xlist busy;
	Xlist free;

private:

	// put new items to free list
	void add_mem(void* base, size_t sz)
	{
		print("  List::%p::%s:  items=%zu, itemsz=%zu, base=0x%p, sz=%zu.\n",
			this, __func__, sz / sizeof(Item), sizeof(Item), base, sz);

		/* DELME after 01.01.2019
		// address must be alligned to 8 byte, cause type 'T' may contain
		// 64-bit data and access to them may be through double instructions
		wassert(!((addr_t)base & (sizeof(uint64_t) - 1)));
		*/

		addr_t addr = (addr_t)base;
		size_t rest = sz;
		while (rest >= sizeof(Item))
		{
			Item* cur = (Item*)addr;
			free.add(free.last, Next, cur, false);  // false - don't check correctness, it is too long
			addr += sizeof(Item);
			rest -= sizeof(Item);
			//print("    List::%s:  added new item:  prev=0x%p, cur=0x%p, next=0x%p.\n", __func__, cur->prev, cur, cur->next);
		}
		#ifdef DEBUG
		check(__func__);
		#endif
		print("  List::%p::%s:  done.\n", this, __func__);
	}

	inline bool check_for_alloc()
	{
		if (!free.sz)
		{
			size_t sz = 0;
			void* mem = malloc_func ? malloc_func(Item_size, &sz) : 0;
			if (!mem)
				panic("list=%s is full, free=%zu, busy=%zu, N=%zu, typesz=%zu, base_file=%s, malloc_func=%p.\n",
					name?name:"---", free.sz, busy.sz, N, sizeof(T), __BASE_FILE__, malloc_func);
			add_mem(mem, sz);
		}
		return free.sz;
	}

	void check_for_free()
	{
		// TODO:  allocator free
	}

public:

	enum
	{
		Item_size = sizeof(Item)
	};

	// data
	// TODO:  replace to private !!!
	uint8_t local_buf [Item_size * N] __attribute__((aligned(sizeof(uint64_t)))); // dword aligned

	class iter_t
	{
		friend class list_t<T,N>; // to allow access to private 'item' members
		Item* item;
	public:
		iter_t () : item(0) {}
		iter_t (Item* p) : item(p) {}
		void raw(void* p) { item = (Item*)p; }
		void* raw() { return item; }
		inline iter_t& operator ++ () { item = item->next;  return *this; }
		inline iter_t& operator -- () { item = item->prev;  return *this; }
		inline T& operator * () { return item->data; }
		inline T* operator -> () { return &item->data; }
		inline bool operator != (const iter_t& a) const { return item != a.item; }
		inline bool operator == (const iter_t& a) const { return item == a.item; }
		inline iter_t operator + (int v)
		{
			iter_t r(item);
			if (v < 0)
				while (v++) --r;
			else
				while (v--) ++r;
			return r;
		}
		inline iter_t operator - (int v) { return operator + (-v); }
		#ifdef DEBUG
		inline void* item_cur()  { return item; }
		inline void* item_next() { return item ? item->next : 0; }
		inline void* item_prev() { return item ? item->prev : 0; }
		#endif
	};

	// const iter
	class citer_t
	{
		friend class list_t<T,N>; // to allow access to 'item'
		Item* item;
	public:
		citer_t (Item* p) { item = p; }
		const void* raw() const { return item; }
		inline citer_t& operator ++ () { item = item->next;  return *this; }
		inline citer_t& operator -- () { item = item->prev;  return *this; }
		inline const T& operator * () { return item->data; }
		inline const T* operator -> () const { return &item->data; }
		inline bool operator != (const citer_t& a) const { return item != a.item; }
		inline bool operator == (const citer_t& a) const { return item == a.item; }
		inline citer_t operator + (int v)
		{
			citer_t r(item);
			if (v < 0)
				while (v++) --r;
			else
				while (v--) ++r;
			return r;
		}
		inline citer_t operator - (int v) { return operator + (-v); }
	};

private:

	void init(list_dprint_t dbg_print = List_no_dprint, const char* n = 0)
	{
		print("  List::%p::%s:  before:  free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);
		dprint = dbg_print;
		name = n;
		malloc_func = 0;
		add_mem(local_buf, sizeof(local_buf));
		print("  List::%p::%s:  after:   free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);
	}

public:

	list_t(list_dprint_t dbg_print = List_no_dprint, const char* n = 0)
	{
		dprint = dbg_print;
		print("  List::%p::%s:  hello.\n", this, __func__);
		init(dbg_print, n);
		print("  List::%p::%s:  after:   free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);
	}

	list_t(void* mem, size_t sz, list_dprint_t dbg_print = List_no_dprint, const char* n = 0)
	{
		dprint = dbg_print;
		print("  List::%p::%s:  hello, mem=0x%x, sz=%zu.\n", this, __func__, mem, sz);
		init(dbg_print, n);
		add_mem(mem, sz);
	}

	~list_t()
	{
		print("  List::%p::%s:  local_buf=0x%p.\n", this, __func__, local_buf);
		if ((free.sz + busy.sz) != N)
			panic("  List::%p::%s:  unsupported dtor() for allocated list:  name=%s, free=%zu,"
				" busy=%zu, N=%zu, typesz=%zu, base_file=%s.\n",
				this, __func__, name?name:"---", free.sz, busy.sz, N, sizeof(T), __BASE_FILE__);
	}

	list_t& operator = (const list_t& right)
	{
		print("  List::%p::%s.\n", this, __func__);

		// TODO:  think
		if (!capacity())
			init();  // no ctors was called --> initialize

		malloc_func = right.malloc_func;
		clear();
		for (citer_t it=right.begin(); it!=right.end(); ++it)
			push_back(*it);

		return *this;
	}


	void set_allocator(list_malloc_t new_malloc)
	{
		malloc_func = new_malloc;
	}

	void add_memory(void* mem, size_t sz)
	{
		add_mem(mem, sz);
	}

	void add_free_item(void* p)
	{
		add_mem(p, sizeof(Item));
	}

	void* del_free_item()
	{
		Item* item = free.begin;
		free.remove(item);
		return item;
	}

	inline const citer_t begin()  const { return citer_t(busy.begin); }
	inline const citer_t end()    const { return citer_t(0);          }
	inline const citer_t cbegin() const { return citer_t(busy.begin); }
	inline const citer_t cend()   const { return citer_t(0);          }

	inline iter_t begin()    { return iter_t(busy.begin); }
	inline iter_t last()     { return iter_t(busy.last);  }
	inline iter_t end()      { return iter_t(0);          }
	inline size_t size()      const { return busy.sz;            }
	inline size_t free_size() const { return free.sz;            }
	inline size_t capacity()  const { return busy.sz + free.sz;  }
	inline bool   empty()     const { return !busy.sz;           }

	// dbg:  check correctness
	void check(const char* lable)
	{
		print("  List::%p::%s:  lable='%s'.\n", this, __func__, lable);
		free.check(free.begin);
		busy.check(busy.begin);
		print("  List::%p::%s:  lable='%s', ok.\n", this, __func__, lable);
	}

	// XXX:  non standart - call ctor instead of copy data
	T& push_back()
	{
		print("  List::%p::%s:  before:  free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);

		check_for_alloc();

		Item* item = free.begin;
		free.remove(item);
		busy.add(busy.last, Next, item);
		new (&item->data) T;

		print("  List::%p::%s:  after:   free.sz=%zu, busy.sz=%zu, add data=%p.\n",
			this, __func__, free.sz, busy.sz, &item->data);

		return item->data;
	}

	void push_back(const T& val)
	{
		print("  List::%p::%s:  before:  free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);

		check_for_alloc();

		Item* item = free.begin;
		free.remove(item);
		busy.add(busy.last, Next, item);
		item->data = val;

		print("  List::%p::%s:  after:   free.sz=%zu, busy.sz=%zu, add data=%p.\n",
			this, __func__, free.sz, busy.sz, &item->data);
	}

	iter_t insert_before(iter_t pos, const T& val)
	{
		print("  List::%p::%s:  before:  free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);

		check_for_alloc();

		Item* pos_item = pos.item;
		Item* new_item = free.begin;
		free.remove(new_item);
		busy.add(pos_item, Prev, new_item);
		new_item->data = val;

		print("  List::%p::%s:  after:   free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);

		return iter_t(new_item);
	}

	iter_t insert_after(iter_t pos, const T& val)
	{
		print("  List::%p::%s:  before:  free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);

		check_for_alloc();

		Item* pos_item = pos.item;
		Item* new_item = free.begin;
		free.remove(new_item);
		busy.add(pos_item, Next, new_item);
		new_item->data = val;

		return iter_t(new_item);
	}

	iter_t insert_sort(const T& val)
	{
		print("  List::%p::%s:  before:  free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);

		check_for_alloc();

		Item* pos_item = busy.find_pos(val);
		Item* new_item = free.begin;
		free.remove(new_item);
		new_item->data = val;
		if (pos_item)
			busy.add(pos_item, Prev, new_item);  // insert
		else
			busy.add(busy.last, Next, new_item); // push back

		return iter_t(new_item);
	}

	iter_t erase(iter_t pos)
	{
		print("  List::%p::%s:  before:  free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);

		wassert(busy.sz);

		Item* item = pos.item;
		Item* ret = item->next;
		busy.remove(item);
		free.add(free.begin, Prev, item);

		check_for_free();

		return iter_t(ret);
	}

	// TODO:  may be optimized, I suppose
	void clear()
	{
		print("  List::%p::%s:  before:  free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);

		iter_t it = begin();
		while (it != end())
			it = erase(it);

		print("  List::%p::%s:  after:   free.sz=%zu, busy.sz=%zu.\n", this, __func__, free.sz, busy.sz);
	}

	T& front() {  wassert(busy.sz);  return busy.begin->data;  }
	T& back()  {  wassert(busy.sz);  return busy.last->data;   }
	const T& front() const {  wassert(busy.sz);  return busy.begin->data;  }
	const T& back()  const {  wassert(busy.sz);  return busy.last->data;   }
};

#undef print

#endif // LIST_H
