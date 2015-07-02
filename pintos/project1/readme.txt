
任务分析：
Alarm-Clock：pintos中的线程状态只有两个，就绪态（ready）和运行态（running）
需要给线程添加一个新的状态，阻塞态（block）
Priority Scheduling：
pintos现有的调度机制是采用的FIFO（先进先出）现在需要修改使之实现基于优先级的调度机制。
Advanced Scheduler：
pintos现有的调度机制是基于时间片的调度方法（轮转法RR）
在任务二中实现了基于时间片的优先级调度，不过该调度方法容易引起线程饥饿。希望能够实现多级反馈队列调度。

具体修改文件情况：
thread.h thread.c
struct thread结构中
/*add by team5 huoxin for alarm-clock*/
int64_t ticks_blocked;
/*add by team5 gaoganyu for priority-scheduling-* */
int base_priority;
struct list locks;
struct lock *lock_waiting;
.h文件中添加的函数
/*add by team5 qiaoqi for priority_change priority_preempt */
void thread_set_priority (int);//用于给当前线程设置新的优先级
/*add by team5 gaoganyu for advanced-scheduler* */
void thread_remove_lock(struct lock *lock);//移除锁
void thread_hold_the_lock(struct lock *lock);//设置锁
void thread_donate_priority(struct thread *t);//优先级捐赠
void thread_update_priority(struct thread *t);//更新该进程的优先级
/*add by team5 houxin for alarm-clock*/
void blocked_thread_check(struct thread *t,void *aux UNUSED);//检查当前被阻塞的进程
bool thread_cmp_priority(const struct list_elem *a,const struct list_elem *b,void *aux UNUSED);
/*add by team5 for mlfqs* */
void thread_mlfqs_increase_recent_cpu_by_one(void);
void thread_mlfqs_update_load_avg_and_recent_cpu(void);
void thread_mlfqs_update_priority(struct thread *t);

synch.h synch.c
/*add by team5 gaoganyu for advanced-scheduler-* */
struct list_elem elem;
int max_priority;
bool lock_cmp_priority (const struct list_elem *a,const struct list_elem *b,void *aux);
void sema_down (struct semaphore *sema) 
void sema_up (struct semaphore *sema)
void lock_acquire (struct lock *lock)
void cond_signal (struct condition *cond, struct lock *lock UNUSED)
bool lock_cmp_priority(const struct list_elem *a,const struct list_elem *b,void *aux UNUSED)


time.h timer.c
/*add by team5 houxin for alarm-clock*/
void timer_sleep (int64_t ticks);//时间片到达之后进程释放CPU

fixed_point.h
浮点数运算的实现