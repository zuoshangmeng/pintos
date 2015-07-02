
#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>

//* My implementation *//
#include "threads/synch.h"
//* My implementation *//

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

//* My addition *//
#define NICE_MAX 20
#define NICE_DEFAULT 0
#define NICE_MIN -20

#ifdef USERPROG
# define RET_STATUS_DEFAULT 0xcdcdcdcd
#endif
//* My addition *//

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    
    //* My addition *//
    //int block_ticks;                   /* 用来记录进程已经被阻塞了多少个ticks */ 
    int base_priority;                 /* 被捐助前的优先级 */
    bool donated;                       /* 是否被捐助了优先级 */
    struct list locks;                 /* 该线程所有的锁构成的列表 */
    struct lock *blocked;               /* 该线程因哪个锁而阻塞 */
    
    int nice;                            /* 线程的nice值 */
    int recent_cpu;                     /* 线程最近使用的cpu时间 */
    //* My addition *//

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    
    //* My addition *//
    struct semaphore wait;              /* 竞争process_wait所用的信号量 */
    int ret_status;                      /* 线程执行结束时返回的状态 */
    struct list files;                   /* 所有当前线程打开的文件列表 */
    struct file *self;                   /* 该线程对应的执行文件 */
    struct thread *parent;               /* 父线程 */
    //* My addition *//
    
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };
  

//* My addition *//
/* 这个结构体用来记录睡眠的线程 */
struct sleep_record
{  
	int tick;			     /* 该线程要睡眠到ticks=tick的时候 */
	struct thread *t;      /* 对应的睡眠线程 */  
	struct list_elem elem;    /* 与thread结构体里的elem作用一样，用来插入睡眠队列 */
};
//* My addition *//

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

//* My implementation *//
void real_set_priority(struct thread *curr,int new_priority,bool forced);

void sort_thread_list (struct list *l);

bool com_pri(const struct list_elem *a,const struct list_elem *b,void *aux);
void thread_calculate_load_avg (void);
void thread_calculate_recent_cpu (struct thread *t);
void thread_calculate_priority (struct thread *t);
void thread_calculate_recent_cpu2(struct thread *t,void *aux);
void thread_calculate_priority2(struct thread *t,void *aux);

void thread_sleep(int64_t ticks);
void thread_wake(int64_t deadline);
bool tick_less(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
struct thread *get_thread_by_tid (tid_t);
//* My implementation *//

#endif /* threads/thread.h */
