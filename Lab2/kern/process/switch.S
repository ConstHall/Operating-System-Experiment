.text
.globl switch_to
switch_to:                      //switch_to(from, to)

    # save from's registers
    movl 4(%esp), %eax          //保存from的首地址
    popl 0(%eax)                //将返回值保存到context的eip
    //以上上2条指令用于保存前一个进程from的执行现场
    movl %esp, 4(%eax)          //保存esp的值到context的esp
    movl %ebx, 8(%eax)          //保存ebx的值到context的ebx
    movl %ecx, 12(%eax)         //保存ecx的值到context的ecx
    movl %edx, 16(%eax)         //保存edx的值到context的edx
    movl %esi, 20(%eax)         //保存esi的值到context的esi
    movl %edi, 24(%eax)         //保存edi的值到context的edi
    movl %ebp, 28(%eax)         //保存ebp的值到context的ebp
    //以上7条指令保存前一个进程的其他7个寄存器到进程上下文context中的相应位置。

    # restore to's registers
    movl 4(%esp), %eax          //保存to的首地址到eax
                                
                                
    movl 28(%eax), %ebp         //保存context的ebp到ebp寄存器
    movl 24(%eax), %edi         //保存context的ebp到ebp寄存器
    movl 20(%eax), %esi         //保存context的esi到esi寄存器
    movl 16(%eax), %edx         //保存context的edx到edx寄存器
    movl 12(%eax), %ecx         //保存context的ecx到ecx寄存器
    movl 8(%eax), %ebx          //保存context的ebx到ebx寄存器
    movl 4(%eax), %esp          //保存context的esp到esp寄存器
    //以上7条指令的作用为：从进程上下文context的高位地址ebp开始，按顺序把相应位置的7个值赋给对应的寄存器。

    pushl 0(%eax)               //将context的eip压入栈中
    //实际上就是把下一个进程要执行的指令地址放到栈顶，

    ret                         //把栈顶的内容赋值给eip寄存器，至此已完成进程切换。


