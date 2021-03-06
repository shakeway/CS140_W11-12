#include "sharing.h"
#include <hash.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "userprog/pagedir.h"
#include <string.h>
#include "frame.h"
#include "page.h"
#include "threads/malloc.h"


unsigned 
exec_hash (const struct hash_elem *se_, void *aux UNUSED) {
    const struct shared_executable *se = hash_entry(se_, struct shared_executable, elem);
    char buf[sizeof (off_t) + sizeof (struct inode *)];
    memcpy (buf, &se->offset, sizeof (off_t));
    memcpy (buf + sizeof (off_t), &se->inode, sizeof (struct inode *));
    return hash_bytes(buf, sizeof(buf));
}

bool 
exec_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
    const struct shared_executable *a = hash_entry(a_, struct shared_executable, elem);
    const struct shared_executable *b = hash_entry(b_, struct shared_executable, elem);
    return a->inode < b->inode || a->offset < b->offset;
}


void 
sharing_register_page (struct page *page)
{

	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

	if (shared_exec)
	{
		list_push_front (&shared_exec->user_pages, &page->exec_elem);
	}
	else
	{
		shared_exec = malloc (sizeof (struct shared_executable));
		ASSERT (shared_exec);
		shared_exec->inode = file_get_inode (page->file);
		shared_exec->offset = page->offset;
		list_init (&shared_exec->user_pages);

		list_push_front (&shared_exec->user_pages, &page->exec_elem);

		hash_insert (&executable_table, &shared_exec->elem);
	}
	lock_release (&executable_table_lock);
}

void sharing_unregister_page (struct page *page)
{
	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

	ASSERT (shared_exec);

	list_remove (&page->exec_elem);
	if (list_empty (&shared_exec->user_pages))
	{
		hash_delete (&executable_table, &shared_exec->elem);
		free (shared_exec);
	}
	else
	{
		//If there is another page, and the page being removed is the one
		// in memory, we must transfer ownership of the physical memory
		lock_acquire (&frame_table_lock);
		if (hash_find (&frame_table, &page->frame_elem) == &page->frame_elem)
		{
			lock_release (&frame_table_lock);
			ASSERT (page->paddr);
			list_remove (&page->exec_elem);
			struct list_elem *e = list_begin (&shared_exec->user_pages);
			struct page *sharing_page = list_entry (e, struct page, exec_elem);
			lock_acquire (&sharing_page->busy);
			if (pagedir_get_page (sharing_page->pd, sharing_page->vaddr) == NULL)
			{
				sharing_page->paddr = page->paddr;
				pagedir_set_page (sharing_page->pd, sharing_page->vaddr, sharing_page->paddr, false);
			}
			lock_acquire (&frame_table_lock);
			hash_delete (&frame_table, &page->frame_elem);
			hash_insert (&frame_table, &sharing_page->frame_elem);
			lock_release (&sharing_page->busy);
		}
		page->paddr = NULL;
		pagedir_clear_page (page->pd, page->vaddr);
		lock_release (&frame_table_lock);
	}

	lock_release (&executable_table_lock);
}

struct shared_executable *
sharing_lookup (struct page *page)
{
	ASSERT (page->type == EXECUTABLE && page->writable == false);
	ASSERT (page->file);
	ASSERT (page->offset != (off_t) -1);

	struct  shared_executable se;
	se.inode = file_get_inode (page->file);
	se.offset = page->offset;

	struct hash_elem *e = hash_find (&executable_table, &se.elem);

	if (e)
		return hash_entry (e, struct shared_executable, elem);
	else
		return NULL;
}

bool 
sharing_scan_and_clear_accessed_bit (struct page *page)
{
	bool accessed = false;

	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

	ASSERT (shared_exec);

	struct list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      accessed = pagedir_is_accessed (sharing_page->pd, sharing_page->vaddr);
      pagedir_set_accessed (sharing_page->pd, sharing_page->vaddr, false);
    }
    lock_release (&executable_table_lock);
    return accessed;
}

void *
sharing_find_shared_frame (struct page *page)
{
	lock_acquire (&executable_table_lock);
    struct shared_executable *shared_exec = sharing_lookup (page);

    ASSERT (shared_exec);

	struct list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      if (sharing_page->paddr)
      {
      	lock_release (&executable_table_lock);
      	return sharing_page->paddr;
      }
    }
    lock_release (&executable_table_lock);
    return NULL;
}

void
sharing_invalidate (struct page *page)
{
	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

    ASSERT (shared_exec);

	struct list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      sharing_page->paddr = 0;
      pagedir_clear_page (sharing_page->pd, sharing_page->vaddr);
    }
	lock_release (&executable_table_lock);
}

bool
sharing_pinned (struct page *page)
{
	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

    ASSERT (shared_exec);

	struct list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      if (sharing_page->pinned)
      {
      	lock_release (&executable_table_lock);
      	return true;
      }
    }
	lock_release (&executable_table_lock);
	return false;
}

