#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
 
/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc, 
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:
                                            
  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+ 
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  + 
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep 
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit   
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT          10
#define HASH_LIST_SIZE      (1 << HASH_SHIFT)
#define pid_hashfn(x)       (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    //LAB4:EXERCISE1 YOUR CODE
    /*
     * below fields in proc_struct need to be initialized
     *       enum proc_state state;                      // 进程状态
     *       int pid;                                    // 进程 ID
     *       int runs;                                   // 进程运行时间
     *       uintptr_t kstack;                           // 进程内核栈
     *       volatile bool need_resched;                 // bool值:是否需要重新调度以释放CPU?
     *       struct proc_struct *parent;                 // 父进程
     *       struct mm_struct *mm;                       // 进程的内存管理区域
     *       struct context context;                     // 进程的上下文，用于进程的切换
     *       struct trapframe *tf;                       // 当前中断帧的指针
     *       uintptr_t cr3;                              // CR3寄存器:页目录表(PDT)的基本地址
     *       uint32_t flags;                             // 进程标志符
     *       char name[PROC_NAME_LEN + 1];               // 进程名
     */
    proc->state=PROC_UNINIT;//设置进程为未初始化状态（因为新创建的进程还未被分配物理页）
    proc->pid=-1;//若进程未初始化，则其pid为-1
    proc->runs=0;//初始化时间片,对于刚初始化的进程来说，运行时间为0
    proc->kstack=0;//内核栈地址,该进程分配的地址为0，因为还没有执行，也没有被重定位。
    proc->need_resched=0;//刚刚分配出来的进程，尚未进入CPU，所以无需重新调度释放CPU
    proc->parent=NULL;//父进程为空
    proc->mm=NULL;//虚拟内存为空		
    memset(&(proc->context),0,sizeof(struct context));//初始化上下文（进程的上下文用于进程切换）
    //为context结构体申请空间
    proc->tf=NULL;//中断帧指针为空
    proc->cr3=boot_cr3; //页目录表地址设为内核页目录表基址
    //cr3中含有页目录表物理内存基地址，而boot_cr3指向了ucore启动时建立好的内核虚拟空间的页目录表首地址，所以我们将cr3指向boot_cr3
    proc->flags=0;//标志位为0
    memset(proc->name,0,PROC_NAME_LEN+1);//为进程名申请空间，建立大小为（PROC_NAME_LEN+1）的char数组
    }
    return proc;
}

// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name) {
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc) {
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - alloc a unique pid for process
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);//静态断言，用于发现运行期间的错误
    //判定MAX_PID是否大于MAX_PROCESS，防止程序报错
    ////实际上，宏定义里已经定义了MAX_PID=2*MAX_PROCESS，所以PID的总数目必定大于PROCESS的总数目
    struct proc_struct *proc;//创立新进程proc
    list_entry_t *list = &proc_list, *le;//list指向进程列表proc_list的头
    static int next_safe = MAX_PID, last_pid = MAX_PID;//注意是静态变量
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;//每次last_pid超过MAX_PID时就重置为1，从头开始寻找没用过的pid
        goto inside;//跳转至inside部分
    }
    if (last_pid >= next_safe) {//若last_pid超过安全区间上限
    inside:
        next_safe = MAX_PID;//将安全区间上限重置为MAX_PID
    repeat:
        le = list;//le赋值为进程的链表头
        while ((le = list_next(le)) != list) {//遍历链表
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {//当一个已经存在的进程的pid和last_pid相等时，则在下一步判定时将last_pid+1
            //由于last_pid是静态变量，所以在下一次循环时会保留这次的+1
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;//若超出安全区间且大于MAX_PID，则将last_pid重置为1，重新遍历寻找
                    }
                    next_safe = MAX_PID;//因为当前区间内有已用的pid，所以重置next_safe为MAX_PID，重新遍历确定安全区间
                    goto repeat;//重新遍历
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) {//意味着没有与last_pid相等的proc->pid
                next_safe = proc->pid;//缩小安全区间
            }
        }
    }
    return last_pid;//为新进程分配唯一可用的pid号
}

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load  base addr of "proc"'s new PDT
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {//判断proc是否为当前正在执行的进程
        bool intr_flag;//中断变量
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);//屏蔽中断，避免在切换新线程时发生嵌套中断
        {
            current = proc;//设置当前进程为待调度的进程
            load_esp0(next->kstack + KSTACKSIZE);//设置任务状态段ts中特权态0下的栈顶指针esp0为next内核线程initproc的内核栈的栈顶
            //因为ucore中的每个线程都需要有自己的内核栈。所以在进行线程调度切换时，也需要及时的修改esp0的值，使之指向新线程的内核栈顶。
            lcr3(next->cr3);//设置CR3寄存器的值为next内核线程initproc的页目录表起始地址next->cr3
            //从而完成进程间的页表切换
            switch_to(&(prev->context), &(next->context));//完成具体的两个线程的执行现场切换
        }
        local_intr_restore(intr_flag);//恢复中断
    }
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void) {
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc) {
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid) {
    if (0 < pid && pid < MAX_PID) {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list) {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid) {
                return proc;
            }
        }
    }
    return NULL;
}

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.tf_cs = KERNEL_CS;
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS;
    tf.tf_regs.reg_ebx = (uint32_t)fn;
    tf.tf_regs.reg_edx = (uint32_t)arg;
    tf.tf_eip = (uint32_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc) {
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - 释放进程内核栈的内存空间
static void
put_kstack(struct proc_struct *proc) {
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// copy_mm - process "proc" duplicate OR share process "current"'s mm according clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc) {
    assert(current->mm == NULL);
    /* do nothing in this project */
    return 0;
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;
    proc->tf->tf_regs.reg_eax = 0;
    proc->tf->tf_esp = esp;
    proc->tf->tf_eflags |= FL_IF;

    proc->context.eip = (uintptr_t)forkret;
    proc->context.esp = (uintptr_t)(proc->tf);
}

/* do_fork -     一个新的子进程的父进程
 * @clone_flags: 用于指导如何克隆子进程
 * @stack:       父节点的用户堆栈指针。如果stack==0，意味着分叉一个内核线程。
 * @tf:          trapframe信息，它将被复制到子进程的proc->tf
 */
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;//尝试创造一个新的过程
    struct proc_struct *proc;//生成新进程proc
    if (nr_process >= MAX_PROCESS) {//若生成进程数已超过上限
        goto fork_out;//若分配失败则直接返回处理
	//fork_out内部执行的是return ret，且ret初始值为-E_NO_MEM（内存不足导致请求失败）
    }
    ret = -E_NO_MEM;//内存不足导致请求失败
    //LAB4:EXERCISE2 YOUR CODE
    /*
     * 一些有用的宏、函数和定义，您可以在下面的实现中使用它们。
     * 宏和函数:
     *   alloc_proc:   创建一个proc结构体和init字段(lab4:exercise1)
     *   setup_kstack: 分配大小为KSTACKPAGE的页面作为进程内核堆栈
     *   copy_mm:      进程proc还是共享当前进程current，由clone_flags决定，如果clone_flags&CLONE_VM为真，则共享，否则复制     
     *   copy_thread:  在进程的内核堆栈顶部设置trapframe，并设置进程的内核入口点和堆栈
     *   hash_proc:    将proc添加到proc hash_list（进程哈希列表）中
     *   get_pid:      为进程分配一个唯一的pid
     *   wakup_proc:   设置proc->state=PROC_RUNNABLE，唤醒进程
     * 变量:
     *   proc_list:    进程集合的列表
     *   nr_process:   进程集合的数量
     */

    //    1. 调用alloc_proc分配proc_struct（获得一块用户信息块），即分配并初始化进程控制块
    //    2. 调用setup_kstack为子进程分配并初始化内核堆栈
    //    3. 调用copy_mm并根据clone_flag来复制或共享进程内存管理结构
    //    4. 调用copy_thread在proc_struct中设置tf和context（中断帧和进程上下文）
    //    5. 把proc_struct插入到hash_list && proc_list这两个全局进程链表中
    //    6. 调用wakup_proc使新的子进程可运行（设置为就绪态）
    //    7. 使用子程序的pid设置ret值

    if((proc=alloc_proc())==NULL){//1.调用alloc_proc()函数申请内存块
        goto fork_out;//若分配失败则直接返回处理
	//fork_out内部执行的是return ret，且ret初始值为-E_NO_MEM（内存不足导致请求失败）
    }
    proc->parent=current;//将子进程的父节点设置为当前进程
    if(setup_kstack(proc)!=0){//2.调用setup_stack()函数为进程分配一个内核栈
        goto bad_fork_cleanup_proc;//若分配失败则释放kmalloc申请的内存空间
	//并跳转至fork_out直接返回处理
    }
    if(copy_mm(clone_flags,proc)!=0){//3.调用copy_mm()函数复制父进程的内存信息到子进程
        goto bad_fork_cleanup_kstack;//复制失败则释放内存页
    }
    copy_thread(proc,stack,tf);//4.调用copy_thread()函数复制父进程的中断帧tf和进程上下文context
    //5.将新进程添加到进程的（hash）列表中
    bool intr_flag;
    local_intr_save(intr_flag);//屏蔽中断，并将intr_flag置为1
    {
        proc->pid=get_pid();//获取当前进程的pid
        hash_proc(proc); //建立hash映射
        list_add(&proc_list,&(proc->list_link));//加入进程链表
        nr_process++;//进程数加1
    }
    local_intr_restore(intr_flag);//恢复中断
    wakeup_proc(proc);//6.唤醒子进程
    ret=proc->pid;//7.返回子进程的唯一pid
fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int
do_exit(int error_code) {
    panic("process exit!!.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg) {
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
    return 0;
}

// proc_init - set up the first kernel thread idleproc "idle" by itself and 
//           - create the second kernel thread init_main
void
proc_init(void) {
    int i;

    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i ++) {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL) {
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, "idle");
    nr_process ++;

    current = idleproc;

    int pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0) {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - 在kern_init的末尾，第一个内核线程idleproc将执行以下工作
void
cpu_idle(void) {
    while (1) {
        if (current->need_resched) {
            schedule();
        }
    }
}

