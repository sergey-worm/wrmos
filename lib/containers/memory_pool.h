//##################################################################################################
//
//  Memory pool - contents memory region and allow to allocate blocks.
//
//##################################################################################################

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include "wrm_log.h"
#include <assert.h>

class Memory_pool_t
{
public:

	enum
	{
		Lgsz_min  =  5,
		Lgsz_max  = 31,
		Sizes_num = Lgsz_max - Lgsz_min + 1,
	};

private:

	class descr_t
	{
		// data size must by aligne at 8-byte, cause malloc should return 64-bit aligned pointer
		descr_t* _region_prev;
		descr_t* _region_next;
		descr_t* _block_prev;
		descr_t* _block_next;
		unsigned _lgsz;
		int      _busy;
		char     _buf[];

	public:

		int      busy() const   { return _busy;           }
		unsigned lgsz() const   { return _lgsz;           }
		size_t   size() const   { return 1 << _lgsz;      }
		addr_t   addr() const   { return (addr_t) this;   }
		addr_t   end()  const   { return addr() + size(); }
		void*    buf()          { return _buf;            }

		const descr_t* region_prev() const { return _region_prev; }
		const descr_t* region_next() const { return _region_next; }
		descr_t*       region_prev()       { return _region_prev; }
		descr_t*       region_next()       { return _region_next; }
		const descr_t* block_next()  const { return _block_next;  }
		const descr_t* block_prev()  const { return _block_prev;  }

		void   lgsz(unsigned v) { _lgsz = v; }
		void   busy(int v)      { _busy = v; }
		void   init(unsigned lgsz)
		{
			_region_prev = 0;
			_region_next = 0;
			_block_prev  = 0;
			_block_next  = 0;
			_lgsz        = lgsz;
			_busy        = 0;
		}

		void insert_region_before(descr_t* base, descr_t** head)
		{
			this->_region_prev = base->_region_prev;
			this->_region_next = base;
			base->_region_prev = this;
			if (base == *head)
				*head = this;
		}

		void insert_region_after(descr_t* base)
		{
			this->_region_next = base->_region_next;
			this->_region_prev = base;
			base->_region_next = this;
		}

		void remove_region(descr_t** head)
		{
			if (this->_region_prev)
				this->_region_prev->_region_next = this->_region_next;
			else
				*head = this->_region_next;  // new list head

			if (this->_region_next)
				this->_region_next->_region_prev = this->_region_prev;

			this->_region_prev = 0;
			this->_region_next = 0;
		}

		void add_block(descr_t** head)
		{
			if (*head)
			{
				this->_block_next = *head;
				(*head)->_block_prev = this;
			}
			*head = this;
		}

		void remove_block(descr_t** head)
		{
			if (this->_block_prev)
				this->_block_prev->_block_next = this->_block_next;
			else
				*head = (*head)->_block_next;  // new list head

			if (this->_block_next)
				this->_block_next->_block_prev = this->_block_prev;

			this->_block_prev = 0;
			this->_block_next = 0;
		}
	};

	descr_t* _regions;             // consistent list
	descr_t* _blocks [Sizes_num];  // by size list
	size_t   _pool_sz_bytes;       // summary size for statistics


	// split big block to smaller to get page with size = lgsz
	bool split(unsigned lgsz)
	{
		//wrm_logd("%s:  lgsz=%u.\n", __func__, lgsz);
		if (lgsz <= Lgsz_min  ||  lgsz > Lgsz_max)
			return false;

		descr_t** blks = &_blocks[lgsz - Lgsz_min];

		if (!*blks)
			split(lgsz + 1);  // split recursively

		if (!*blks)
			return false;

		// modify blocks
		descr_t* d1 = *blks; // get first item
		assert(!d1->busy());
		d1->remove_block(blks);
		unsigned new_lgsz = d1->lgsz() - 1;
		descr_t* d2 = (descr_t*)(d1->addr() + (1 << new_lgsz));
		descr_t** new_blks = &_blocks[new_lgsz - Lgsz_min];
		d1->lgsz(new_lgsz);
		d2->init(new_lgsz);
		d1->add_block(new_blks);
		d2->add_block(new_blks);

		// modify regions
		d2->insert_region_after(d1);
		return true;
	}

	// combine small blocks to bigger
	void combine(descr_t* d1)
	{
		//wrm_logd("%s:  reg: a=0x%p s=0x%zx.\n", __func__, d1, d1->size());

		// choose prev or nex list item
		descr_t* d2 = is_aligned(d1->addr(), d1->size()*2) ? d1->region_next() : d1->region_prev();
		if (!d2  ||  d1->lgsz() != d2->lgsz()  ||  d2->busy())
			return;

		//wrm_logd("%s:  combine with:  a=0x%lx s=0x%zx.\n", __func__, d2->addr(), d2->size());

		unsigned lgsz = d1->lgsz();

		// make d1 - left, d2 - right
		if (d1->addr() > d2->addr())
		{
			descr_t* t = d1;
			d1 = d2;
			d2 = t;
		}

		// modify regions
		d1->lgsz(lgsz + 1);
		d2->remove_region(&_regions);

		// modify blocks
		unsigned new_lgsz = lgsz + 1;
		descr_t** blks = &_blocks[lgsz - Lgsz_min];
		descr_t** new_blks = &_blocks[new_lgsz - Lgsz_min];
		d1->remove_block(blks);
		d2->remove_block(blks);
		d1->add_block(new_blks);

		combine(d1);  // try combine any more
	}

	// find max block with size 2^N and return N
	unsigned find_max_block(addr_t addr, size_t sz)
	{
		addr_t end = addr + sz;
		for (unsigned i=Lgsz_max; i>=Lgsz_min; --i)
		{
			size_t s = 1 << i;
			if (s <= sz  &&  is_aligned(addr, s)  &&  (addr+s) <= end)
				return i;
		}
		return 0;
	}

public:

	Memory_pool_t() : _pool_sz_bytes(0) {}

	size_t size() const { return _pool_sz_bytes; }

	void dump() const
	{
		wrm_logd("Dump malloc regions (%zu/0x%zx bytes):\n", _pool_sz_bytes, _pool_sz_bytes);
		for (const descr_t* p=_regions; p; p=p->region_next())
			wrm_logd("    %p:  addr=%#8lx, size=%#8zx/%2u, busy=%d\n",
				p, p->addr(), p->size(), p->lgsz(), p->busy());

		wrm_logd("Dump malloc blocks (%zu/0x%zx bytes):\n", _pool_sz_bytes, _pool_sz_bytes);
		for (unsigned i=0; i<Sizes_num; ++i)
		{
			descr_t* blks = _blocks[i];
			//wrm_logd("  blks_sz=0x%zx\n", 1<<(i+Lgsz_min));
			for (const descr_t* p=blks; p; p=p->block_next())
				wrm_logd("    %p:  addr=%#8lx, size=%#8zx/%2u\n",
					p, p->addr(), p->size(), p->lgsz());
		}
	}

	void check(const char* mark) const
	{
		int err = 0;
		const descr_t* prev = 0;
		for (const descr_t* p=_regions; p; p=p->region_next())
		{
			if (prev)
			{
				if ((addr_t)p < (prev->addr() + prev->size()))
				{
					wrm_loge("  %s:    BAD addr/sz 1:  %p:  addr=%#8lx, size=%#8zx/%u, busy=%d\n",
						mark, p, p->addr(), p->size(), p->lgsz(), p->busy());
					err = 1;
				}
			}
			prev = p;
		}

		for (unsigned i=0; i<Sizes_num; ++i)
		{
			descr_t* blks = _blocks[i];
			//wrm_logd("  blks_sz=0x%zx\n", 1<<(i+Lgsz_min));
			for (const descr_t* p=blks; p; p=p->block_next())
			{
				if (p->busy())
				{
					wrm_loge("  %s:  BAD busy:  %p:  addr=%#8lx, size=%#8zx/%u\n",
						mark, p, p->addr(), p->size(), p->lgsz());
					err = 1;
				}

				size_t sz = 1 << (i + Lgsz_min);
				if (p->size() != sz)
				{
					wrm_loge("  %s:  BAD size:  %p:  addr=%#8lx, size=%#8zx/%u, expected sz=0x%zx\n",
						mark, p, p->addr(), p->size(), p->lgsz(), sz);
					err = 1;
				}
			}
		}
		if (err)
			dump();
		assert(!err);
	}

	// split at max-size blocks and add it to pool
	int add(void* buf, size_t sz)
	{
		addr_t start = (addr_t) buf;
		addr_t end = start + sz;
		//wrm_logd("%s:  orig:  buf=0x%lx, sz=0x%zx.\n", __func__, start, sz);
		start = round_up(start, 1 << Lgsz_min);
		end = round_down(end, 1 << Lgsz_min);
		if (start >= end)
			return -1;
		//wrm_logd("%s:  norm:  buf=0x%lx, sz=0x%lx.\n", __func__, start, end - start);
		// find insert position inside _region
		descr_t* prev = _regions;
		for (descr_t* p=_regions; p; p=p->region_next())
		{
			if (start >= p->end()  &&  (!p->region_next()  ||  end <= p->region_next()->addr()))
			{
				prev = p;
				break;
			}
		}
		// insert blocks after prev
		unsigned lgsz = 0;
		while ((lgsz = find_max_block(start, end-start)))
		{
			//wrm_logd("%s:    lgsz=%u, blksz=0x%lx.\n", __func__, lgsz, 1lu << lgsz);
			descr_t* d = (descr_t*) start;
			d->init(lgsz);
			if (!_regions)
				_regions = d;
			else
				d->insert_region_after(prev);
			descr_t** blks = &_blocks[lgsz - Lgsz_min];
			if (!*blks)
				*blks = d;
			else
				d->add_block(blks);
			prev = d;
			size_t sz = 1 << lgsz;
			_pool_sz_bytes += sz;
			start += sz;
			//wrm_logd("%s:    end.\n", __func__);
		}
		//wrm_logd("%s:  end.\n", __func__);
		return 0;
	}

	descr_t* alloc_block(size_t lgsz)
	{
		//wrm_logd("%s:  lgsz=%zu, pool_sz=0x%zx.\n", __func__, lgsz, _pool_sz_bytes);
		if (lgsz > Lgsz_max)
		{
			wrm_loge("%s:  wrong lgsz=%zu.\n", __func__, lgsz);
			return 0;
		}

		if (lgsz < Lgsz_min)
			lgsz = Lgsz_min;

		if (_pool_sz_bytes < (size_t)(1<<lgsz))
		{
			//wrm_logw("%s:  no mem:  pool_sz=0x%zx, require=0x%x.\n", __func__, _pool_sz_bytes, 1<<lgsz);
			return 0;
		}

		descr_t** blks = &_blocks[lgsz - Lgsz_min];
		if (!*blks)
			split(lgsz + 1);

		if (!*blks)
			return 0;

		descr_t* d = *blks; // get first item
		d->remove_block(blks);
		d->busy(1);
		_pool_sz_bytes -= d->size();
		return d;
	}

	void* alloc(size_t sz)
	{
		//wrm_logd("%s:  before:  sz=0x%zx, pool_sz=0x%zx.\n", __func__, sz, _pool_sz_bytes);
		if (!sz)
			return 0;
		sz += sizeof(descr_t);
		descr_t* d = alloc_block(get_log2sz_top_range(sz));
		//wrm_logd("%s:  after:    a=0x%p, pool_sz=0x%zx.\n", __func__, d->buf(), _pool_sz_bytes);
		return d ? d->buf() : 0;
	}

	size_t free_block(descr_t* d)
	{
		//wrm_logd("%s:  addr=0x%p, sz=0x%zx.\n", __func__, d, d->size());
		assert(d->busy());
		unsigned lgsz = d->lgsz();
		if (lgsz < Lgsz_min  ||  lgsz > Lgsz_max)
		{
			wrm_loge("%s:  bad size=0x%zx, lgsz=%u, addr=0x%lx.\n", __func__, d->size(), lgsz, d->addr());
			return 0;
		}
		descr_t** blks = &_blocks[lgsz - Lgsz_min];
		d->add_block(blks);
		d->busy(0);
		size_t free_sz = d->size();
		combine(d);
		_pool_sz_bytes += free_sz;
		return free_sz;
	}

	size_t free(void* p)
	{
		if (!p)
			return 0;
		return free_block((descr_t*)((addr_t)p - sizeof(descr_t)));
	}
};

#endif // MEMORY_POOL_H
