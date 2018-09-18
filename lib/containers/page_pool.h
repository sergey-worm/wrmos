//##################################################################################################
//
//  Page pool - contents flex pages and allow to store and allocate fpage.
//
//##################################################################################################

#ifndef PG_POOL_H
#define PG_POOL_H

#include "list.h"
#include "wrm_log.h"
#include "l4_types.h"
#include <assert.h>

class Page_pool_t
{
	enum
	{
		Lgsz_min = 12,
		Lgsz_max = 31,
		Sizes_num  = Lgsz_max - Lgsz_min + 1
	};
	static_assert(Cfg_page_sz == (1 << Lgsz_min));

	typedef list_t <L4_fpage_t, 32> fpages_t;
	fpages_t _fpages [Sizes_num];
	size_t   _pool_sz_bytes;

private:

	// split big pages to smaller to get page with size = lgsz
	bool split(size_t lgsz)
	{
		//wrm_logd("%s:  lgsz=%u.\n", __func__, lgsz);
		if (lgsz < Lgsz_min  ||  lgsz > Lgsz_max)
			return false;

		fpages_t& pgs = _fpages[lgsz - Lgsz_min];

		if (pgs.empty())
			split(lgsz + 1);  // split recursively

		if (pgs.empty())
			return false;

		fpages_t::iter_t it = pgs.begin();
		L4_fpage_t pg1 = *it;
		L4_fpage_t pg2 = pg1;
		pgs.erase(it);
		size_t new_sz = lgsz - 1;
		pg1.log2sz(new_sz);
		pg2.log2sz(new_sz);
		pg2.addr(pg1.addr() + pg2.size());
		_fpages[new_sz - Lgsz_min].push_back(pg1);
		_fpages[new_sz - Lgsz_min].push_back(pg2);
		return true;
	}

	size_t lgsz(size_t sz_bytes)
	{
		//wrm_logd("%s:  sz_bytes=0x%x.\n", __func__, sz_bytes);
		if (!sz_bytes)
			return 0;
		size_t lgsz = 0;
		word_t bytes = sz_bytes;
		while (!(bytes & 0x1))
		{
			bytes >>= 1;
			lgsz += 1;
		}

		// checking
		//wrm_logd("%s:  sz_bytes=0x%x --> lgsz=%u, before checking.\n", __func__, sz_bytes, lgsz);
		if ((size_t)(1lu<<lgsz) != sz_bytes)
			return 0;

		//wrm_logd("%s:  sz_bytes=0x%x --> lgsz=%u.\n", __func__, sz_bytes, lgsz);
		return lgsz;
	}

	L4_fpage_t alloc_lgsz(size_t lgsz)
	{
		//wrm_logd("%s:  lgsz=%zu, pool_sz=0x%zx (&pool_sz=0x%p).\n", __func__, lgsz, _pool_sz_bytes, &_pool_sz_bytes);
		if (lgsz < Lgsz_min  ||  lgsz > Lgsz_max)
		{
			wrm_loge("%s:  wrong lgsz=%zu.\n", __func__, lgsz);
			return L4_fpage_t::create_nil();
		}

		if (_pool_sz_bytes < (size_t)(1<<lgsz))
		{
			wrm_logw("%s:  no mem:  pool_sz=0x%zx, require=0x%x.\n", __func__, _pool_sz_bytes, 1<<lgsz);
			return L4_fpage_t::create_nil();
		}

		L4_fpage_t res = L4_fpage_t::create_nil();
		fpages_t& pgs = _fpages[lgsz - Lgsz_min];
		if (pgs.empty())
			split(lgsz + 1);

		if (!pgs.empty())
		{
			fpages_t::iter_t it = pgs.begin();
			res = *it;
			pgs.erase(it);
			_pool_sz_bytes -= res.size();
		}
		return res;
	}

public:

	Page_pool_t() : _pool_sz_bytes(0) {}

	size_t size() const { return _pool_sz_bytes; }

	void dump()
	{
		wrm_logd("Dump page pool (%#zx bytes):\n", _pool_sz_bytes);
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
		assert(fpage.log2sz() >= Lgsz_min);
		assert(fpage.log2sz() <= Lgsz_max);
		_fpages[fpage.log2sz() - Lgsz_min].push_back(fpage);
		_pool_sz_bytes += fpage.size();
	}

	L4_fpage_t alloc(size_t sz)
	{
		//wrm_logd("%s:  sz=0x%zx, pool_sz=0x%zx.\n", __func__, sz, _pool_sz_bytes);
		L4_fpage_t res = alloc_lgsz(lgsz(sz));
		return res;
	}
};

#endif // PG_POOL_H
