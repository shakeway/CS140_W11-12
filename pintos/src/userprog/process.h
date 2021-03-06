#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);


extern struct list process_list;
struct lock process_lock;


typedef int fd_t;

struct process
{
    tid_t parent_tid;
    tid_t tid;
    bool finished; // toggled when the process has terminated
    bool parent_finished; //toggled when the parent thread has terminated
    struct semaphore sema_finished; //upped when terminated, downed when waiting
    int exit_code;
    struct file *executable; //pointer to file to keep open
    struct hash supp_page_table;
    struct list_elem elem;
};

struct file_wrapper
{
    fd_t fd;
    void *file_or_dir;
    bool is_dir;
    struct list_elem elem;
};

struct file_wrapper * lookup_fd ( fd_t fd);
struct file_wrapper *wrap_file (void *file_or_dir, bool is_dir);



typedef int mapid_t;
#define MAP_FAILED ((mapid_t) -1)



struct mmapped_file
{
    struct file *file;
    void *base_page; //virtual page at which the file starts
    mapid_t mapid;
    struct list_elem elem;
};

struct mmapped_file * lookup_mmapped ( mapid_t mapid);
void process_munmap (mapid_t mapid);

#endif /* userprog/process.h */
