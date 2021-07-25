#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>


// process's state in his life cycle
enum proc_state {
    PROC_UNINIT = 0,  // uninitialized
    PROC_SLEEPING,    // sleeping
    PROC_RUNNABLE,    // runnable(maybe running)
    PROC_ZOMBIE,      // almost dead, and wait parent proc to reclaim his resource
};

// 为内核上下文切换保存寄存器。
// 不需要保存所有的%fs等段寄存器，
// 因为它们在内核上下文中是常量。
// 保存所有常规寄存器，这样我们就不需要关心了
// 它们是调用者save，而不是返回寄存器%eax。
// (不保存%eax只是简化了切换代码。)
// context的布局必须与switch.S中的代码匹配。
struct context {
    uint32_t eip;//eip寄存器里存储的是CPU下次要执行的指令的地址。
    uint32_t esp;//esp寄存器里存储的是在调用函数fun()之后，栈的栈顶，并且始终指向栈顶。
    uint32_t ebx;//基底寄存器，在内存寻址时存放基地址。
    uint32_t ecx;//计数寄存器，是重复(rep)前缀指令和loop指令的内定计数器。
    uint32_t edx;//资料寄存器，用来放整数除法产生的余数。
    uint32_t esi;//源索引寄存器，在很多字符串操作指令中, DS:ESI指向源串。
    uint32_t edi;//目标索引寄存器，在很多字符串操作指令中, DS:EDI指向目标串，
    uint32_t ebp;//ebp寄存器里存储的是是栈的栈底指针，通常叫栈基址，这个是一开始进行fun()函数调用之前，由esp传递给ebp的。（在函数调用前可以理解为：esp存储的是栈顶地址，也是栈底地址）
};

#define PROC_NAME_LEN               15
#define MAX_PROCESS                 4096
#define MAX_PID                     (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct {
    enum proc_state state;                      // Process state
    int pid;                                    // Process ID
    int runs;                                   // the running times of Proces
    uintptr_t kstack;                           // Process kernel stack
    volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;                 // the parent process
    struct mm_struct *mm;                       // Process's memory management field
    struct context context;                     // Switch here to run process
    struct trapframe *tf;                       // Trap frame for current interrupt
    uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
    uint32_t flags;                             // Process flag
    char name[PROC_NAME_LEN + 1];               // Process name
    list_entry_t list_link;                     // Process link list 
    list_entry_t hash_link;                     // Process hash list
};

#define le2proc(le, member)         \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void) __attribute__((noreturn));

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int do_exit(int error_code);

#endif /* !__KERN_PROCESS_PROC_H__ */

