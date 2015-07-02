/* 此文件所有中文注释由陈希文所写 */

#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
//* My Implementation *//
#include "threads/vaddr.h"
#include "threads/init.h"
#include "userprog/process.h"
#include <list.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "threads/synch.h"
//* My Implementation *//

static void syscall_handler (struct intr_frame *);

//* My Implementation *//
typedef int pid_t;

/* 定义系统调用的函数 */
//int sys_exit (int status);
static int sys_write (int fd, const void *buffer, unsigned length);
static int sys_halt (void);
static int sys_create (const char *file, unsigned initial_size);
static int sys_open (const char *file);
static int sys_close (int fd);
static int sys_read (int fd, void *buffer, unsigned size);
static int sys_exec (const char *cmd);
static int sys_wait (pid_t pid);
static int sys_filesize (int fd);
static int sys_tell (int fd);
static int sys_seek (int fd, unsigned pos);
static int sys_remove (const char *file);

/* 定义系统调用的一些辅助函数 */
static struct file *find_file_by_fd (int fd);
static struct fd_elem *find_fd_elem_by_fd (int fd);
static int alloc_fid (void);
static struct fd_elem *find_fd_elem_by_fd_in_process (int fd);

/* 定义系统调用的handler的类型，它接收32位长的参数地址，这由三个指针来提供 */
typedef int (*handler) (uint32_t, uint32_t, uint32_t);
static handler syscall_vec[128];/* 存放系统调用的handler的数组 */
static struct lock file_lock;/* 访问文件的锁 */

/* 定义一个结构体将线程、文件与fd关联起来 */
struct fd_elem
  {
    int fd;
    struct file *file;
    struct list_elem elem;
    struct list_elem thread_elem;
  };
  
static struct list file_list;/* 所有当前已打开的文件的列表 */

//* My Implementation *//

void
syscall_init (void) /* 初始化系统调用，main中调用 */
{
	/* 将系统调用处理函数syscall_handler注册为中断向量号(0x30)，
	 * 参数列表中的3代表用户可以调用这个中断 */
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  //* My Implementation *//
  /* 用syscall_vec[]来将系统调用号与系统调用一一对应。
   * 各个系统调用的编号在syscall-nr.h中定义 */
  syscall_vec[SYS_EXIT] = (handler)sys_exit;
  syscall_vec[SYS_HALT] = (handler)sys_halt;
  syscall_vec[SYS_CREATE] = (handler)sys_create;
  syscall_vec[SYS_OPEN] = (handler)sys_open;
  syscall_vec[SYS_CLOSE] = (handler)sys_close;
  syscall_vec[SYS_READ] = (handler)sys_read;
  syscall_vec[SYS_WRITE] = (handler)sys_write;
  syscall_vec[SYS_EXEC] = (handler)sys_exec;
  syscall_vec[SYS_WAIT] = (handler)sys_wait;
  syscall_vec[SYS_FILESIZE] = (handler)sys_filesize;
  syscall_vec[SYS_SEEK] = (handler)sys_seek;
  syscall_vec[SYS_TELL] = (handler)sys_tell;
  syscall_vec[SYS_REMOVE] = (handler)sys_remove;
  
  list_init (&file_list);
  lock_init (&file_lock);
  //* My Implementation *//
}

static void
syscall_handler (struct intr_frame *f /* Old Implementation： UNUSED */) 
{
	/* Old implementation
  printf ("system call!\n");
  thread_exit ();*/
  
  //* My Implementation *//
  handler h;
  int *p;
  int ret;
  
  p = f->esp;/* 获取这次要调用的系统调用的编号所存放的地址 */
  
  if (!is_user_vaddr (p))/* 不能访问内核地址 */
    goto terminate;
  
  if (*p < SYS_HALT || *p > SYS_INUMBER)/* 超出了系统调用编号的范围 */
    goto terminate;
  
  h = syscall_vec[*p];/* 从syscall_vec[]中取出需要的系统调用函数赋给h */
  
  /* 下一个参数必须是在用户虚存内 */
  if (!(is_user_vaddr (p + 1) && is_user_vaddr (p + 2) && is_user_vaddr (p + 3)))
    goto terminate;
  
  ret = h (*(p + 1), *(p + 2), *(p + 3));/* 执行相应系统调用 */
  
  f->eax = ret;/* 将返回参数保存在f-eax中 */
  
  return;
  
terminate:
  sys_exit (-1);
  //*  My Implementation *//
}


//* My addition *//

/* 从buffer里向fd对应的文件里写入length个字节 */
static int
sys_write (int fd, const void *buffer, unsigned length)
{
  struct file * f;
  int ret;
  
  ret = -1;
  lock_acquire (&file_lock);
  /* 如果是标准输出，调用putbuf(console.c中定义)向屏幕输出字符 */
  if (fd == STDOUT_FILENO) 
    putbuf (buffer, length);
  else if (fd == STDIN_FILENO) /* 如果是标准输入，出错 */
    goto done;
  /* 如果缓冲区超出了用户能访问的虚存范围，终止用户程序 */
  else if (!is_user_vaddr (buffer) || !is_user_vaddr (buffer + length))
    {
      lock_release (&file_lock);
      sys_exit (-1);
    }
  else/* 如果缓冲区地址合法，开始操作 */
    {
      f = find_file_by_fd (fd);/* 通过fd号获取要被写的文件f */
      if (!f)
        goto done;
        
      ret = file_write (f, buffer, length);/* 向f写入 */
    }
    
done:
  lock_release (&file_lock);
  return ret;/* 返回实际写入的字节数 */
}

/* 退出当前用户程序 */
int
sys_exit (int status)
{
  struct thread *t;
  struct list_elem *l;
  
  t = thread_current ();
  while (!list_empty (&t->files))/* 关闭用户程序打开的所有文件 */
    {
      l = list_begin (&t->files);
      sys_close (list_entry (l, struct fd_elem, thread_elem)->fd);
    }
  
  t->ret_status = status;
  thread_exit ();
  return -1;
}

/* 关闭pintos */
static int
sys_halt (void)
{
  power_off ();
}

/* 创建一个指定了初始大小的文件 */
static int
sys_create (const char *file, unsigned initial_size)
{
  if (!file)
    return sys_exit (-1);
  return filesys_create (file, initial_size);
}

/* 打开一个文件 */
static int
sys_open (const char *file)
{
  struct file *f;
  struct fd_elem *fde;
  int ret;
  
  ret = -1; /* 初始化为-1 */
  if (!file) /* 如果文件名为null，返回 */
    return -1;
  if (!is_user_vaddr (file))/* 如果不是用户文件，终止程序 */
    sys_exit (-1);
  f = filesys_open (file);
  if (!f) /* 打开文件失败 */
    goto done;
    
  fde = (struct fd_elem *)malloc (sizeof (struct fd_elem));
  if (!fde) /* 没有足够空间来为要打开的文件创建一个fd_elem */
    {
      file_close (f);
      goto done;
    }
    
  fde->file = f;
  fde->fd = alloc_fid ();/* 分配一个fd号 */
  /* 将打开的文件存入file_list和当前线程的files列表里 */
  list_push_back (&file_list, &fde->elem);
  list_push_back (&thread_current ()->files, &fde->thread_elem);
  ret = fde->fd;
done:
  return ret;
}

/* 关闭fd号对应的文件 */
static int
sys_close(int fd)
{
  struct fd_elem *f;
  int ret;
  
  /* 通过fd找到对应的fd_elem */
  f = find_fd_elem_by_fd_in_process (fd);
  
  if (!f) /* 如果是无效的fd则返回 */
    goto done;
   /* 关闭文件，并从file_list和当前线程的file列表中移除它 */
  file_close (f->file);
  list_remove (&f->elem);
  list_remove (&f->thread_elem);
  free (f);/* 释放对应的结构体fd_elem */
  
done:
  return 0;
}

/* 从fd对应的文件读取size个字节到缓冲区 */
static int
sys_read (int fd, void *buffer, unsigned size)
{
  struct file * f;
  unsigned i;
  int ret;
  
  ret = -1; /* 初始化为-1 */
  lock_acquire (&file_lock);
  /* 如果是标准输入，调用input_getc(input.c中定义)从键盘读取输入 */
  if (fd == STDIN_FILENO) 
    {
      for (i = 0; i != size; ++i)
        *(uint8_t *)(buffer + i) = input_getc ();
      ret = size;
      goto done;
    }
  else if (fd == STDOUT_FILENO) /* 如果是标准输出，出错 */
      goto done;
      /* 如果缓冲区超出了用户徐村地址，终止用户程序 */
  else if(!is_user_vaddr(buffer)||!is_user_vaddr(buffer+size))
    {
      lock_release (&file_lock);
      sys_exit (-1);
    }
  else
    {
      f = find_file_by_fd (fd);/* 通过fd找出对应的文件 */
      if (!f)
        goto done;
      ret = file_read (f, buffer, size);/* 读取文件 */
    }
    
done:    
  lock_release (&file_lock);
  return ret;
}

/* 执行命令 */
static int
sys_exec (const char *cmd)
{
  int ret;
  
  /* 如果命令为空或者非法访问了内存，返回 */
  if (!cmd || !is_user_vaddr (cmd)) 
    return -1;
  lock_acquire (&file_lock);
  ret = process_execute (cmd);/* 调用process_execute执行命令 */
  lock_release (&file_lock);
  return ret;
}

/* 等待进程号为pid的进程消亡 */
static int
sys_wait (pid_t pid)
{
  return process_wait (pid);
}

/* 返回文件的字节数 */
static int
sys_filesize (int fd)
{
  struct file *f;
  
  f = find_file_by_fd (fd);
  if (!f)
    return -1;
  return file_length (f);
}

/* 返回文件中下一个被操作的位置 */
static int
sys_tell (int fd)
{
  struct file *f;
  
  f = find_file_by_fd (fd);
  if (!f)
    return -1;
  return file_tell (f);
}

/* 将文件下一个要操作的位置设为pos */
static int
sys_seek (int fd, unsigned pos)
{
  struct file *f;
  
  f = find_file_by_fd (fd);
  if (!f)
    return -1;
  file_seek (f, pos);
  return 0;
}

/* 删除文件 */
static int
sys_remove (const char *file)
{
  if (!file)
    return false;
  if (!is_user_vaddr (file))
    sys_exit (-1);
    
  return filesys_remove (file);
}


/* ************辅助函数*********** */
static struct file *
find_file_by_fd (int fd)
{
  struct fd_elem *ret;
  
  ret = find_fd_elem_by_fd (fd);
  if (!ret)
    return NULL;
  return ret->file;
}

static struct fd_elem *
find_fd_elem_by_fd (int fd)
{
  struct fd_elem *ret;
  struct list_elem *l;
  
  for (l = list_begin (&file_list); 
       l != list_end (&file_list); 
       l = list_next (l))
    {
      ret = list_entry (l, struct fd_elem, elem);
      if (ret->fd == fd)
        return ret;
    }
    
  return NULL;
}

static struct fd_elem *
find_fd_elem_by_fd_in_process (int fd)
{
  struct fd_elem *ret;
  struct list_elem *l;
  struct thread *t;
  
  t = thread_current ();
  
  for (l = list_begin (&t->files); l != list_end (&t->files); l = list_next (l))
    {
      ret = list_entry (l, struct fd_elem, thread_elem);
      if (ret->fd == fd)
        return ret;
    }
    
  return NULL;
}

static int
alloc_fid (void)
{
  static int fid = 2;/* 因为0与1保留作标准输入与输出 */
  return fid++;
}
/* ************辅助函数*********** */

//* My addition *//
