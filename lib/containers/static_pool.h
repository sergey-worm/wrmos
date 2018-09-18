//##################################################################################################
//
//  Static pool - container for store static data and allocator for it.
//
//##################################################################################################

#ifndef STATIC_POOL_H
#define STATIC_POOL_H

#include <stdint.h>
#include "bitmap.h"

template<size_t S, size_t N>
class Static_pool_t
{
	typedef long Word;
	enum
	{
		Item_bytes = align_up(S, sizeof(Word)),
		Item_words = Item_bytes / sizeof(Word)
	};

	Word pool[Item_words * N];
	bitmap_t<N> bmap;

public:

	Static_pool_t()
	{
		bmap.init(N);
	}

	void* alloc()
	{
		int pos = bmap.getfree();
		//printf("Stat_pool::%s:  pos=%d.\n", __func__, pos);
		if (pos < 0)
			return 0;
		assert((size_t)pos < N);
		return pool + pos * Item_words;
	}

	bool free(void* p)
	{
		//printf("Stat_pool::%s:  p=%p.\n", __func__, p);
		if (p < pool  ||  p >= (pool+Item_words*N))
			return false;
		unsigned pos = ((addr_t)p - (addr_t)pool) / Item_bytes;
		//printf("Stat_pool::%s:  p=%p, pos=%d, bit=%d.\n", __func__, p, pos, bmap.get(pos));
		assert(bmap.get(pos) == 1);
		bmap.clear(pos);
		return true;
	}

	size_t capacity() const { return bmap.capacity(); }
	size_t count()    const { return bmap.count(); }
};

#endif // STATIC_POOL_H
