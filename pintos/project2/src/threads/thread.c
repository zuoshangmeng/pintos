
#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
//* My implementation *//
#include "threads/fixed_point.h"
//* My implementation *//
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

//* My implementation *//
static struct list block_list;
//* My implementation *//

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

//* My addition *//
static int load_avg;
//* My addition *//

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

//* My addition 以下静态变量是为了实现alarm clock *//
static struct list sleep_list;
static int sleep_count;
static int64_t max_tick;
static struct sleep_record *theTop;
//* My addition *//

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  
  //* My implementation *//
    list_init(&sleep_list); 
	theTop = NULL;   
	max_tick = 100000000;   
	sleep_count = 0;
	
   load_avg=0;
//* My implementation *//
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  //* My Implementation *//
  ASSERT (priority >= PRI_MIN && priority <= PRI_MAX);
#ifdef USERPROG
  if (list_size (&all_list) >= 35) /* 如果已达最大线程数目，创建线程失败 */
    return TID_ERROR;
#endif
  //* My Implementation *//
  
  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  //* My implementation *//
  if(thread_mlfqs)
  thread_calculate_priority(t);
  if(priority>thread_current()->priority)
  thread_yield();
  
#ifdef USERPROG
  sema_init (&t->wait, 0);
  t->ret_status = RET_STATUS_DEFAULT;
  list_init (&t->files);
  t->parent = NULL;
#endif
  //* My implementation *//

  return tid;
}


/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  
  //* My implementation */
  
  //list_remove(&thread_current()->elem);
  //list_push_back (&block_list, &thread_current ()->elem);
  //* My implementation *//
  
  schedule ();
}


bool
com_pri(const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux);
/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  
  /* Old implementation */
  /*list_push_back (&ready_list, &t->elem);*/
  /* Old implementation */
  
  //* My implementation *//
  //list_remove(&t->elem);
  list_insert_ordered (&ready_list, &t->elem,
                     com_pri, NULL);
  //* My implementation *//
  
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

//* My addition *//
bool
com_pri(const struct list_elem *a,
         const struct list_elem *b,
            void *aux)
{
	struct thread *thread1,*thread2;
	thread1=list_entry(a,struct thread,elem);
	thread2=list_entry(b,struct thread,elem);
	return(thread1->priority>thread2->priority);
}
//* My addition *//

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it call schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
  /* Old implementation */
  /* list_push_back (&ready_list, &cur->elem); */
  /* Old implementation */
  
  //* My implementation *//
  list_insert_ordered (&ready_list, &cur->elem,com_pri, NULL);
  //* My implementation *//
  
                     
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
	/* Old implementation */
  /*thread_current ()->priority = new_priority;*/
    /* Old implementation */
    
    //* My implementation *//
    real_set_priority(thread_current(),new_priority,true);
    //* My implementation *//   
}

//* My addition *//
void
real_set_priority(struct thread *curr,int new_priority,bool forced)
{
	if(!curr->donated)
	curr->priority=curr->base_priority=new_priority;
	else if(forced)
	{
		if(curr->priority>new_priority)
		curr->base_priority=new_priority;
		else
		curr->priority=new_priority;
	}
	else
		curr->priority=new_priority;
	
	if(list_entry(list_begin(&ready_list),
	struct thread,elem)->priority>new_priority)
	thread_yield();	
}
//* My addition *//

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice 
/* Old implementation: UNUSED */) 
{
	/* Old implementation*/
  /* Not yet implemented. */
  /* Old implementation*/
  
  //* My implementation *//
  ASSERT(nice>=NICE_MIN&&nice<=NICE_MAX); 
  /* 依照文档规定，线程的nice值在-20到20之间，默认为0. */
  
  struct thread *curr;
  curr=thread_current();
  curr->nice=nice;
  thread_calculate_recent_cpu(curr);
  thread_calculate_priority(curr); 
  //* My implementation *//
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  /* Old implementation*/
  /* Not yet implemented. */
  /*return 0;*/
  /* Old implementation*/
  
  //* My implementation *//
  return thread_current()->nice;
  //* My implementation *//
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  /* Old implementation*/
  /* Not yet implemented. */
  /*return 0;*/
  /* Old implementation*/
  
   //* My implementation *//
   return CONVERT_TO_INT_NEAR(100*load_avg);
    //* My implementation *//
}


/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
	/* Old implementation*/
  /* Not yet implemented. */
  /*return 0;*/
  /* Old implementation*/
   //* My implementation *//
   return CONVERT_TO_INT_NEAR(100*thread_current()->recent_cpu);
    //* My implementation *//
}


//* My addition *//
void
thread_calculate_priority(struct thread *t)
{
	ASSERT(is_thread(t));
	
	if(t==idle_thread)
	return;
	
	t->priority=PRI_MAX-CONVERT_TO_INT_NEAR(t->recent_cpu/4)-t->nice*2;/* 根据文档中给的公式计算该线程的优先级。 */
                                                                              /* 其中CONVERT_TO_INT_NEAR是fixed_point.h*/	
                                                                              /* 文件中定义的宏，作用是将浮点数转换为整型以便CPU计算。 */
    if(t->priority>PRI_MAX)/* 如果计算得到的优先级超出预定的优先级范围，将其置为最大/最小优先级。 */
    t->priority=PRI_MAX;
    if(t->priority<PRI_MIN)
    t->priority=PRI_MIN;
}

void/* 与thread_calculate_priority功能相同，是为了在thread_foreach里调用才重新定义 */
thread_calculate_priority2(struct thread *t,void *aux)
{
	ASSERT(is_thread(t));
	
	if(t==idle_thread)
	return;
	
	t->priority=PRI_MAX-CONVERT_TO_INT_NEAR(t->recent_cpu/4)-t->nice*2;/* 根据文档中给的公式计算该线程的优先级。 */
                                                                              /* 其中CONVERT_TO_INT_NEAR是fixed_point.h*/	
                                                                              /* 文件中定义的宏，作用是将浮点数转换为整型以便CPU计算。 */
    if(t->priority>PRI_MAX)/* 如果计算得到的优先级超出预定的优先级范围，将其置为最大/最小优先级。 */
    t->priority=PRI_MAX;
    if(t->priority<PRI_MIN)
    t->priority=PRI_MIN;
}

void
thread_calculate_load_avg (void)
{
  int ready_threads_num;
  
  if (thread_current () != idle_thread)
    ready_threads_num = list_size (&ready_list) + 1;
  else
    ready_threads_num = list_size (&ready_list);
  load_avg = FP_MUL (CONVERT_TO_FP (59) / 60, load_avg) + CONVERT_TO_FP (1) / 60 * ready_threads_num;
}

void
thread_calculate_recent_cpu(struct thread *t)
{
  ASSERT (is_thread (t));
  
  if (t == idle_thread)
    return;
    
  int load = 2 * load_avg;
  t->recent_cpu = INT_ADD (FP_MUL (FP_DIV (load, INT_ADD (load, 1)), t->recent_cpu), t->nice);
}

void/* 与thread_calculate_recent_cpu功能相同，是为了在thread_foreach里调用才重新定义 */
thread_calculate_recent_cpu2(struct thread *t,void *aux)
{
  ASSERT (is_thread (t));
  
  if (t == idle_thread)
    return;
    
  int load = 2 * load_avg;
  t->recent_cpu = INT_ADD (FP_MUL (FP_DIV (load, INT_ADD (load, 1)), t->recent_cpu), t->nice);
}
//* My addition *//



/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
 
 /* Old Implementation */            /* 由于有了多级反馈队列，对线程优先级的设置不能像原来那么简单了； */
  /* t->priority = priority; */     /* 同时，由于为了实现复杂功能对thread结构体加了许多新属性，这些属性也需要初始化 */
  /* Old Implementation */
  
  //* My Implementation *//
  if (!thread_mlfqs)
  {
    t->base_priority = t->priority = priority;
    t->donated = false;
  }
  t->blocked = NULL;
  list_init (&t->locks);
  if (thread_mlfqs)
  {
    t->nice = NICE_DEFAULT; /* 默认nice值为0 */
    if (t == initial_thread)
      t->recent_cpu = 0;
    else
      t->recent_cpu = thread_get_recent_cpu ();
  }
  //* My Implementation *//
 
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev) 
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  schedule_tail (prev); 
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);


//***********************************************************************
//* My addition 以下函数实现的是alarm clock部分 *//
void
thread_sleep(int64_t ticks)
{   
	if (ticks <= 0)  
		return; 
	enum intr_level old_level;  
	struct thread *cur = running_thread();  
	struct sleep_record sr;  
	sr.t = cur;                     /* 将当前正在运行的线程对应为一个睡眠记录 */
	sr.tick = ticks + timer_ticks();  /* 设定当前线程要睡到什么时候 */
	old_level = intr_disable(); 
	list_insert_ordered(&sleep_list, &sr.elem, tick_less, NULL); /* 插入睡眠列表 */
	if (ticks < max_tick)/* 保证了tick数小的睡眠线程在列表的前面。 */   
	{      
		theTop = list_entry(list_begin(&sleep_list), struct sleep_record, elem);      
		max_tick = theTop->tick; 
	}   
	sleep_count++; 
	thread_block(); 
	intr_set_level(old_level);
}

/*	比较两个睡眠线程的tick数，较小的那个睡眠线程会被插在睡眠列表的前面 */
bool
tick_less(const struct list_elem *a, const struct list_elem *b        
, void *aux UNUSED)
{  
	const struct sleep_record *sr1= list_entry(a, struct sleep_record, elem);   
	const struct sleep_record *sr2 = list_entry(b, struct sleep_record, elem);  
	return sr1->tick <= sr2->tick;
}

void 
thread_wake(int64_t deadline)
{   
	if (deadline < max_tick) /* 还没有线程睡够，返回。 */   
		return;  
	if (!sleep_count)  /* 有可能是没有函数在睡，但是当前ticks仍旧小于max_tick */
		return;   
	enum intr_level old_level = intr_disable(); 
	theTop = list_entry(list_begin(&sleep_list), struct sleep_record, elem);  
	while (sleep_count && theTop != NULL && theTop->tick <= deadline) 
	{     /* 当睡眠列表中有线程已睡够，将其弹出并解阻塞 */
		sleep_count--;    
		list_pop_front(&sleep_list);    
		thread_unblock(theTop->t);     
		theTop = list_entry(list_begin(&sleep_list), struct sleep_record, elem); 
	}  
	if (!sleep_count)   /* 如果已经没有线程处于睡眠状态，将max_tick恢复为默认值， */
		max_tick = 10000000;  /* 为新一轮的睡眠线程的维护做准备 */
	intr_set_level(old_level);
}
//***********************************************************************


//***********************************************************************
//* My addtion 以下函数是为了实现列表的排序 *//

static bool
thread_sort_less (const struct list_elem *lhs, const struct list_elem *rhs, void *aux UNUSED)
{
  struct thread *a, *b;
  
  ASSERT (lhs != NULL && rhs != NULL);
  
  a = list_entry (lhs, struct thread, elem);
  b = list_entry (rhs, struct thread, elem);
  
  return (a->priority > b->priority);
}

void
sort_thread_list (struct list *l)
{
  if (list_empty (l))
    return;

  list_sort (l, thread_sort_less, NULL);
}

struct thread *
get_thread_by_tid (tid_t tid)
{
  struct list_elem *f;
  struct thread *ret;
  
  ret = NULL;
  for (f = list_begin (&all_list); f != list_end (&all_list); f = list_next (f))
    {
      ret = list_entry (f, struct thread, allelem);
      ASSERT (is_thread (ret));
      if (ret->tid == tid)
        return ret;
    }
    
  return NULL;
}
//* My addtion *//
//***********************************************************************
