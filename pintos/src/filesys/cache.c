#include "cache.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "filesys.h"
#include <debug.h>

struct cached_block block_cache[CACHE_SIZE];

uint8_t hand;



void 
cache_init (void)
{
	int i;
	for (i = 0; i < CACHE_SIZE; i++)
		lock_init (&block_cache[i].lock);

	hand = 0;
	lock_init (&cache_lock);
}

struct cached_block *
cache_find_unused (void)
{
	int i;
	struct cached_block *b;
	lock_acquire (&cache_lock);
	for (i = 0; i < CACHE_SIZE; i++)
	{
		b = &block_cache[i];
		if (!b->in_use)
		{
			b->pinned = true;
			lock_release (&cache_lock);
			return b;
		}
	}
	lock_release (&cache_lock);
	return NULL;
}

struct cached_block *
cache_allocate (block_sector_t sector)
{
	struct cached_block *b = cache_find_unused ();
	if (b == NULL)
	{
		b = cache_evict ();
	}
	b->in_use = true;
	b->sector = sector;
	return b;
}

struct cached_block *
cache_evict (void)
{
	lock_acquire (&cache_lock);

	struct cached_block *block_to_evict = cache_run_clock ();

	lock_acquire( &block_to_evict->lock);
	block_to_evict->pinned = true;
	block_to_evict->in_use = false;

	lock_release (&cache_lock);

	if (block_to_evict->dirty)
		block_write (fs_device, block_to_evict->sector, block_to_evict->data);
	block_to_evict->dirty = false;
	block_to_evict->accessed = false;

	lock_release (&block_to_evict->lock);

	return block_to_evict;
}

struct cached_block *
cache_run_clock (void)
{
	ASSERT (lock_held_by_current_thread (&cache_lock));

	struct cached_block *b = NULL;
	while (true)
	{
		hand ++;
		b = &block_cache[hand];
		if (!b->accessed)
		{
			if (!b->pinned)
				return b;
		}
		else
			b->accessed = false;			
	}

	//TODO: keep looping or wait on a condition variable?
}


struct cached_block *
cache_lookup (block_sector_t sector)
{
	int i;
	struct cached_block *b;
	lock_acquire (&cache_lock);
	for (i = 0; i < CACHE_SIZE; i++)
	{
		b = &block_cache[i];
		if (b->sector == sector && b->in_use)
		{
			b->pinned = true;
			lock_release (&cache_lock);
			return b;
		}
	}
	lock_release (&cache_lock);
	return NULL;
}