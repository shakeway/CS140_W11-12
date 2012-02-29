#include "sharing.h"
#include <hash.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "userprog/pagedir.h"
#include <string.h>


unsigned 
exec_hash (const struct hash_elem *p_, void *aux UNUSED) {
    const struct shared_exec *p = hash_entry(f_, struct shared_exec, elem);
    char buf[sizeof (off_t) + size (block_sector_t)];
    memset (buf, &shared_exec->offset, sizeof (off_t));
    memset (buf + sizeof (off_t), &shared_exec->sector, sizeof (block_sector_t));
    return hash_bytes(buf, sizeof(buf));
}

bool 
exec_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
    const struct shared_exec *a = hash_entry(a_, struct shared_exec, elem);
    const struct shared_exec *b = hash_entry(b_, struct shared_exec, elem);
    return a->block < b->block || a->offset < b->offset;
}


void 
sharing_register_page (struct page *page)
{

	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

	if (shared_exec)
	{
		lock_acquire (&shared_exec->busy);
		list_push_front (&shared_exec->user_pages, page->exec_elem);
		lock_release (&shared_exec->busy);
	}
	else
	{
		shared_exec = malloc (sizeof (struct shared_executable));
		ASSERT (shared_exec);
		shared_exec->sector = page->file->inode->sector;
		shared_exec->offset = page->offset;
		shared_exec->kpage = NULL;
		list_init (&shared_exec->user_pages);
		lock_init (&shared_exec->busy);

		list_push_front (&shared_exec->user_pages, page->exec_elem);

		hash_insert (&executable_table, shared_exec->elem);
	}
	lock_release (&executable_table_lock);
}

void sharing_unregister_page (struct page *page)
{
	lock_acquire (&executable_table_lock);
	struct shared_executable *shared_exec = sharing_lookup (page);

	ASSERT (shared_exec);

	lock_acquire (&shared_exec->busy);
	list_remove (&shared_exec->user_pages, page->exec_elem);
	lock_release (&shared_exec->busy);
	if (list_empty (&shared_exec->user_pages))
	{
		hash_delete (&executable_table, &shared_exec->elem);
		free (shared_exec);
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
	se.sector = page->file->inode->sector;
	se.offset = page->offset;

	hash_elem *e = hash_find (&executable_table, se.elem);

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

	struc list_elem *e;
	struct page *sharing_page;

	for (e = list_begin (&shared_exec->user_pages); e != list_end (&shared_exec->user_pages); e = list_next (e))
    {
      sharing_page = list_entry (e, struct page, exec_elem);
      accessed = pagedir_is_accessed (sharing_page->pd, sharing_page->vaddr);
      pagedir_set_accessed (sharing_page->pd, sharing_page->vaddr, false);
    }

    return accessed;
}
