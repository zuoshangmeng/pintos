
���������
Alarm-Clock��pintos�е��߳�״ֻ̬������������̬��ready��������̬��running��
��Ҫ���߳����һ���µ�״̬������̬��block��
Priority Scheduling��
pintos���еĵ��Ȼ����ǲ��õ�FIFO���Ƚ��ȳ���������Ҫ�޸�ʹ֮ʵ�ֻ������ȼ��ĵ��Ȼ��ơ�
Advanced Scheduler��
pintos���еĵ��Ȼ����ǻ���ʱ��Ƭ�ĵ��ȷ�������ת��RR��
���������ʵ���˻���ʱ��Ƭ�����ȼ����ȣ������õ��ȷ������������̼߳�����ϣ���ܹ�ʵ�ֶ༶�������е��ȡ�

�����޸��ļ������
thread.h thread.c
struct thread�ṹ��
/*add by team5 huoxin for alarm-clock*/
int64_t ticks_blocked;
/*add by team5 gaoganyu for priority-scheduling-* */
int base_priority;
struct list locks;
struct lock *lock_waiting;
.h�ļ�����ӵĺ���
/*add by team5 qiaoqi for priority_change priority_preempt */
void thread_set_priority (int);//���ڸ���ǰ�߳������µ����ȼ�
/*add by team5 gaoganyu for advanced-scheduler* */
void thread_remove_lock(struct lock *lock);//�Ƴ���
void thread_hold_the_lock(struct lock *lock);//������
void thread_donate_priority(struct thread *t);//���ȼ�����
void thread_update_priority(struct thread *t);//���¸ý��̵����ȼ�
/*add by team5 houxin for alarm-clock*/
void blocked_thread_check(struct thread *t,void *aux UNUSED);//��鵱ǰ�������Ľ���
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
void timer_sleep (int64_t ticks);//ʱ��Ƭ����֮������ͷ�CPU

fixed_point.h
�����������ʵ��