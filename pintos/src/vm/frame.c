#include "frame.h"
#include <debug.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "page.h"
#include "swap.h"
#include <hash.h>
#include <string.h>
#include <stdio.h>
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

struct hash frame_table;
struct lock frame_table_lock;
void *frame_evict (void);

/* Returns a hash for frame f */
unsigned 
frame_hash (const struct hash_elem *f_, void *aux UNUSED) {
    const struct page *p = hash_entry(f_, struct page, frame_elem);
    return hash_bytes(&p->paddr, sizeof(p->paddr));
}

/* Returns true if frame a precedes frame b in physical memory */
bool 
frame_less (const struct hash_elem *a_, const struct hash_elem *b_,
void *aux UNUSED) {
    const struct page *a = hash_entry(a_, struct page, frame_elem);
    const struct page *b = hash_entry(b_, struct page, frame_elem);
    return a->paddr < b->paddr;
}


void
frame_allocate (struct page *page)
{
    ASSERT (pagedir_get_page (page->pd, page->upage) == NULL );
    ASSERT (page->paddr == NULL);

    void *kpage = palloc_get_page (PAL_USER);
    // struct frame *new_frame;
    if (kpage == NULL)
    {
        //TODO: eviction
        kpage = frame_evict();
    }

    page->paddr = kpage;
    page->pinned = true;
    lock_acquire (&frame_table_lock);
    hash_insert (&frame_table, &page->frame_elem);
    lock_release (&frame_table_lock);


    // else{
        
    //     new_frame = malloc (sizeof (struct frame));
    //     ASSERT (new_frame);
    //     new_frame->pinned = true;
    //     new_frame->paddr = kpage; // TODO - PHYS_BASE?
    //     lock_acquire (&frame_table_lock);
    //     hash_insert (&frame_table, &new_frame->elem);
    //     lock_release (&frame_table_lock);
    // }
    // // add frame to frame table

    // new_frame->owner_thread = thread_current ();
    // new_frame->upage = upage;

    // return new_frame->paddr;
}

// void
// frame_free (void *kpage)
// {
//     struct frame f;
//     // struct hash_elem *e;

//     struct frame *frame_to_delete;

//     f.paddr = kpage;
//     lock_acquire (&frame_table_lock);
//     frame_to_delete = hash_entry (hash_find (&frame_table, &f.elem), struct frame, elem);
//     if (frame_to_delete->owner_thread == thread_current ())
//     {
//         hash_delete (&frame_table, &f.elem);
//         free (frame_to_delete);
//         palloc_free_page (kpage);
//     }

//     lock_release (&frame_table_lock);

//     // ASSERT (e);


//     // free (hash_entry (e, struct frame, elem));
// }

void *
frame_evict (void)
{
    struct hash_iterator it;
    lock_acquire (&frame_table_lock);
    hash_first (&it, &frame_table);
    struct page *page_to_evict;

    do
    {
        page_to_evict = hash_entry (hash_next (&it), struct page, frame_elem);
    }
    while (page_to_evict->pinned == true);

    ASSERT (page_to_evict->paddr);
    ASSERT (pagedir_get_page (page_to_evict->pd, page_to_evict->vaddr));

    void *kpage = page_to_evict->paddr;
    lock_acquire (&page_to_evict->busy);
    pagedir_clear_page (page_to_evict->pd, page_to_evict->vaddr);
    

    // frame_to_evict->pinned = true;
    // struct page *page_to_evict = page_lookup(frame_to_evict->owner_thread->supp_page_table, frame_to_evict->upage);
    // frame_to_evict->owner_thread = NULL;
    // ASSERT (pagedir_get_page (page_to_evict->pd, page_to_evict->vaddr));



    switch (page_to_evict->type)
    {
        case EXECUTABLE:
        case ZERO:
            if (pagedir_is_dirty (page_to_evict->pd, page_to_evict->vaddr))
            {
                //move to swap
                page_to_evict->type = SWAP;
                page_to_evict->swap_slot = swap_allocate_slot ();
                lock_acquire (&filesys_lock);
                swap_write_page ( page_to_evict->swap_slot, frame_to_evict->paddr);
                lock_release (&filesys_lock);
            }
            break;
        case SWAP:
            page_to_evict->swap_slot = swap_allocate_slot ();
            lock_acquire (&filesys_lock);
            swap_write_page ( page_to_evict->swap_slot, frame_to_evict->paddr);
            lock_release (&filesys_lock);
            break;
        case MMAPPED:
            if (pagedir_is_dirty (page_to_evict->pd, page_to_evict->vaddr))
            {
                //copy back to disk
                lock_acquire (&filesys_lock);
                file_seek (page_to_evict->file, page_to_evict->offset);
                file_write (page_to_evict->file, frame_to_evict->paddr, page_to_evict->valid_bytes);
                lock_release (&filesys_lock);
            }
            break;            
    }


    hash_delete (&frame_table, &page_to_evict->frame_elem);
    lock_release (&page_to_evict->busy);

    page_to_evict->paddr = NULL;

    // void *result_kpage = frame_to_evict->paddr;
    lock_release (&frame_table_lock);
    // frame_free (result_kpage);
    return kpage;

}

// struct frame *
// frame_lookup (void *paddr)
// {
//     struct frame f;
//     f.paddr = paddr;
//     struct hash_elem *e;
//     if (!lock_held_by_current_thread (&frame_table_lock))
//     {
//         lock_acquire (&frame_table_lock);
//         e = hash_find (&frame_table, &f.elem);
//         lock_release (&frame_table_lock);
//     }
//     else
//         e = hash_find (&frame_table, &f.elem);
    
//     return e!= NULL ? hash_entry (e, struct frame, elem) : NULL; 
// }

void 
frame_pin (void *vaddr)
{
    lock_acquire (&frame_table_lock);
    void *upage = pg_round_down (vaddr);
    struct page *supp_page = page_lookup (thread_current ()->supp_page_table, upage);

    supp_page->pinned = true;
    lock_release (&frame_table_lock);

    if (supp_page->paddr == NULL)
    {
        page_in (supp_page);
    }
}

void 
frame_unpin (void *vaddr)
{
    void *upage = pg_round_down (vaddr);
    struct page *supp_page = page_lookup (thread_current ()->supp_page_table, upage);

    ASSERT (supp_page->paddr);

    supp_page->pinned = false;
}

void frame_dump_frame ( struct hash_elem *elem, void *aux UNUSED)
{
  struct page *page = hash_entry (elem, struct page, frame_elem);
  printf ("paddr: %p , vaddr: %p\n", page->paddr, page->vaddr);
}

void frame_dump_table (void)
{
  lock_acquire (&frame_table_lock);
  hash_apply (&frame_table, frame_dump_frame);
  lock_release (&frame_table_lock);
}

