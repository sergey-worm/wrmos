//##################################################################################################
//
//  Memory pool - page and memory allocators.
//
//##################################################################################################

#include "page_pool.h"
#include "memory_pool.h"
#include "wrm_mtx.h"

//--------------------------------------------------------------------------------------------------
//  Page pool - contents flex pages and allow to store and allocate fpage.
//--------------------------------------------------------------------------------------------------

// use singltone to allow use pool before ctors
static Page_pool_t* pginst()
{
	static Page_pool_t _page_pool;
	return &_page_pool;
}

static Wrm_mtx_t _pgmtx = Wrm_mtx_initializer;

extern "C" void wrm_pgpool_dump()
{
	int rc = wrm_mtx_lock(&_pgmtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_lock() - rc=%d.\n", __func__, rc);
		return;
	}
	pginst()->dump();
	rc = wrm_mtx_unlock(&_pgmtx);
	if (rc)
		wrm_loge("%s:  wrm_mtx_unlock() - rc=%d.\n", __func__, rc);
}

extern "C" void wrm_pgpool_add(L4_fpage_t fpage)
{
	int rc = wrm_mtx_lock(&_pgmtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_lock() - rc=%d.\n", __func__, rc);
		return;
	}
	pginst()->add(fpage);
	rc = wrm_mtx_unlock(&_pgmtx);
	if (rc)
		wrm_loge("%s:  wrm_mtx_unlock() - rc=%d.\n", __func__, rc);
}

extern "C" L4_fpage_t wrm_pgpool_alloc(size_t sz)
{
	int rc = wrm_mtx_lock(&_pgmtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_lock() - rc=%d.\n", __func__, rc);
		return L4_fpage_t::create_nil();
	}
	L4_fpage_t res = pginst()->alloc(sz);
	rc = wrm_mtx_unlock(&_pgmtx);
	if (rc)
		wrm_loge("%s:  wrm_mtx_unlock() - rc=%d.\n", __func__, rc);
	return res;
}

extern "C" size_t wrm_pgpool_size()
{
	return pginst()->size();  // without mutex
}

//--------------------------------------------------------------------------------------------------
//  Memory pool - contents memory region and allow to allocate blocks.
//--------------------------------------------------------------------------------------------------

// use singltone to allow use pool before ctors
static Memory_pool_t* minst()
{
	static Memory_pool_t _memory_pool;
	return &_memory_pool;
}

static Wrm_mtx_t _mmtx = Wrm_mtx_initializer;

extern "C" void wrm_mpool_dump()
{
	int rc = wrm_mtx_lock(&_mmtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_lock() - rc=%d.\n", __func__, rc);
		return;
	}
	minst()->dump();
	rc = wrm_mtx_unlock(&_mmtx);
	if (rc)
		wrm_loge("%s:  wrm_mtx_unlock() - rc=%d.\n", __func__, rc);
}

extern "C" int wrm_mpool_add(void* buf, size_t sz)
{
	int rc = wrm_mtx_lock(&_mmtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_lock() - rc=%d.\n", __func__, rc);
		return -1;
	}
	int res = minst()->add(buf, sz);
	rc = wrm_mtx_unlock(&_mmtx);
	if (rc)
		wrm_loge("%s:  wrm_mtx_unlock() - rc=%d.\n", __func__, rc);
	return res;
}

extern "C" void* wrm_mpool_alloc(size_t sz)
{
	int rc = wrm_mtx_lock(&_mmtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_lock() - rc=%d.\n", __func__, rc);
		return 0;
	}
	void* res = minst()->alloc(sz);
	rc = wrm_mtx_unlock(&_mmtx);
	if (rc)
		wrm_loge("%s:  wrm_mtx_unlock() - rc=%d.\n", __func__, rc);
	return res;
}

extern "C" void wrm_mpool_free(void* ptr)
{
	int rc = wrm_mtx_lock(&_mmtx);
	if (rc)
	{
		wrm_loge("%s:  wrm_mtx_lock() - rc=%d.\n", __func__, rc);
		return;
	}
	minst()->free(ptr);
	rc = wrm_mtx_unlock(&_mmtx);
	if (rc)
		wrm_loge("%s:  wrm_mtx_unlock() - rc=%d.\n", __func__, rc);
}

extern "C" size_t wrm_mpool_size()
{
	return minst()->size();  // without mutex
}
