//##################################################################################################
//
//  Memory pool - contents flex pages and allow to store and allocate fpage.
//
//##################################################################################################

#include "list.h"
#include "wrm_mpool.h"
#include "wrm_mtx.h"
#include "wrm_log.h"
#include <assert.h>

class Memory_pool_t
{
	enum
	{
		Log2sz_min = 12,           // FIXME:  add this param to cfg
		Log2sz_max = 31,
		Sizes_num  = Log2sz_max - Log2sz_min
	};
	typedef list_t <L4_fpage_t, 32> fpages_t;
	fpages_t _fpages [Sizes_num];
	size_t _pool_sz_bytes;

private:

	// split big pages to smaller to get page with size = log2sz
	bool split(size_t log2sz)
	{
		//wrm_logd("%s:  log2sz=%u.\n", __func__, log2sz);
		if (log2sz < Log2sz_min  ||  log2sz > Log2sz_max)
			return false;

		fpages_t& pgs = _fpages[log2sz - Log2sz_min];

		if (pgs.empty())
			split(log2sz + 1);  // split recursively

		if (pgs.empty())
			return false;

		fpages_t::iter_t it = pgs.begin();
		L4_fpage_t pg1 = *it;
		L4_fpage_t pg2 = pg1;
		pgs.erase(it);
		size_t new_sz = log2sz - 1;
		pg1.log2sz(new_sz);
		pg2.log2sz(new_sz);
		pg2.addr(pg1.addr() + pg2.size());
		_fpages[new_sz - Log2sz_min].push_back(pg1);
		_fpages[new_sz - Log2sz_min].push_back(pg2);
		return true;
	}

	size_t log2sz(size_t sz_bytes)
	{
		//wrm_logd("%s:  sz_bytes=0x%x.\n", __func__, sz_bytes);
		size_t log2sz = 0;
		word_t bytes = sz_bytes;
		while (!(bytes & 0x1))
		{
			bytes >>= 1;
			log2sz += 1;
		}

		// checking
		//wrm_logd("%s:  sz_bytes=0x%x --> log2sz=%u, before checking.\n", __func__, sz_bytes, log2sz);
		if ((size_t)(1<<log2sz) != sz_bytes)
			return 0;

		//wrm_logd("%s:  sz_bytes=0x%x --> log2sz=%u.\n", __func__, sz_bytes, log2sz);
		return log2sz;
	}

public:

	static Memory_pool_t& inst() { static Memory_pool_t mpool;  return mpool; }

	Memory_pool_t() : _pool_sz_bytes(0) {}

	void dump()
	{
		wrm_logd("Dump memory pool (%#zx bytes):\n", _pool_sz_bytes);
		for (unsigned i=0; i<Sizes_num; ++i)
		{
			fpages_t& pgs = _fpages[i];
			if (pgs.empty())
				continue;
			for (fpages_t::citer_t it=pgs.cbegin(); it!=pgs.cend(); ++it)
				wrm_logd("    addr=%#8lx, size=%#8lx/%u, acc=%d\n",
					it->addr(), it->size(), it->log2sz(), it->access());
		}
	}

	void add(L4_fpage_t fpage)
	{
		assert(fpage.log2sz() >= Log2sz_min);
		assert(fpage.log2sz() <= Log2sz_max);
		_fpages[fpage.log2sz() - Log2sz_min].push_back(fpage);
		_pool_sz_bytes += fpage.size();
	}

	L4_fpage_t alloc_log2sz(size_t log2sz)
	{
		//wrm_logd("%s:  log2sz=%u, pool_sz=0x%x (&pool_sz=0x%x).\n", __func__, log2sz, _pool_sz_bytes, &_pool_sz_bytes);
		if (log2sz < Log2sz_min  ||  log2sz > Log2sz_max)
		{
			wrm_loge("wrong log2sz=%zu.\n", log2sz);
			return L4_fpage_t::create_nil();
		}

		if (_pool_sz_bytes < (size_t)(1<<log2sz))
		{
			wrm_logw("no mem:  pool_sz=0x%zx, require=0x%x.\n", _pool_sz_bytes, 1<<log2sz);
			return L4_fpage_t::create_nil();
		}

		L4_fpage_t res = L4_fpage_t::create_nil();
		fpages_t& pgs = _fpages[log2sz - Log2sz_min];
		if (pgs.empty())
			split(log2sz + 1);

		//assert(!pgs.empty())

		if (!pgs.empty())
		{
			fpages_t::iter_t it = pgs.begin();
			res = *it;
			pgs.erase(it);
			_pool_sz_bytes -= res.size();
		}
		return res;
	}

	L4_fpage_t alloc(size_t sz)
	{
		//wrm_logd("%s:  sz=0x%zx, pool_sz=0x%zx.\n", __func__, sz, _pool_sz_bytes);
		L4_fpage_t res = alloc_log2sz(log2sz(sz));
		return res;
	}

	size_t size() const { return _pool_sz_bytes; }
};

//--------------------------------------------------------------------------------------------------
// C API
//--------------------------------------------------------------------------------------------------
extern "C" void wrm_mpool_dump()
{
	Memory_pool_t::inst().dump();
}

extern "C" void wrm_mpool_add(L4_fpage_t fpage)
{
	Memory_pool_t::inst().add(fpage);
}

extern "C" L4_fpage_t wrm_mpool_alloc(size_t sz)
{
	return Memory_pool_t::inst().alloc(sz);
}

extern "C" L4_fpage_t wrm_mpool_alloc_log2sz(size_t log2sz)
{
	return Memory_pool_t::inst().alloc_log2sz(log2sz);
}

extern "C" size_t wrm_mpool_size()
{
	return Memory_pool_t::inst().size();
}

// TODO:  try to use wrm_mpool_alloc() allocator
extern "C" void* wrm_malloc(size_t sz)
{
	// FIXME:  do separate initialize functon, now may be raice condition
	static Wrm_mtx_t mtx;
	static int first = 1;
	if (first)
	{
		first = 0;
		int rc = wrm_mtx_init(&mtx);
		if (rc)
		{
			wrm_loge("%s:  wrm_mtx_init() - rc=%d.\n", __func__, rc);
			return 0;
		}
	}

	sz = round_up(sz, 8);

	enum { Sz = 256*1024 };
	static char pool[Sz] __attribute__((aligned(8)));
	static size_t free_sz = Sz;
	static size_t free_pos = 0;

	int rc = wrm_mtx_lock(&mtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_lock() - rc=%d.\n", __func__, rc);
		return 0;
	}

	assert(sz <= free_sz);

	if (sz > free_sz)
		return 0;

	void* res = &pool[free_pos];
	free_sz  -= sz;
	free_pos += sz;

	//wrm_logd("%s:  size=%4u, ok, free=%u.\n", __func__, sz, free_sz);

	rc = wrm_mtx_unlock(&mtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_unlock() - rc=%d.\n", __func__, rc);
		return 0;
	}

	return res;
}
