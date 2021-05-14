/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */

// 定义中断处理函数，这里是有errorcode的情况，到时候硬件会推入一个error code进来
// 然后推入中断标号num(向量号)，最后调用统一中断处理_alltraps
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
 // 定义中断处理函数，这是没有error code 的情况，就推入一个0代替errorcode，然后推入中断标号(向量号)，最后调用统一中断处理_alltraps
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

#define SIMPLE_TRAPHANDLER(name, num)				\
.data;												\
	.long name, num;								\
.text;												\
	.global name;									\
	.type name, @function;							\
	.align 2;										\
name:												\
	pushl	$(num);									\
	jmp		_alltraps

#define SIMPLE_TRAPHANDLER_NOEC(name, num)			\
.data;												\
	.long name, num;								\
.text;												\
	.global name;									\
	.type name, @function;							\
	.align 2;										\
name:												\
	pushl	$0;										\
	pushl	$(num);									\
	jmp		_alltraps


// #define TRAPHANDLER_MINE(num) TRAPHANDLER(num##_HANDLER, num)
// #define TRAPHANDLER_NOEC_MINE(num) TRAPHANDLER_NOEC(num##_HANDLER, num)
#define TRAPHANDLER_MINE(num) SIMPLE_TRAPHANDLER(num##_HANDLER, num)
#define TRAPHANDLER_NOEC_MINE(num) SIMPLE_TRAPHANDLER_NOEC(num##_HANDLER, num)


// !!!! 声明全局变量
.data
	.global entryPointOfTraps
	entryPointOfTraps:

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
/*
Divide error                       0            No
Debug exceptions                   1            No
Breakpoint                         3            No
Overflow                           4            No
Bounds check                       5            No
Invalid opcode                     6            No
Coprocessor not available          7            No
System error                       8            Yes (always 0)
Coprocessor Segment Overrun        9            No
Invalid TSS                       10            Yes
Segment not present               11            Yes
Stack exception                   12            Yes
General protection fault          13            Yes
Page fault                        14            Yes
Coprocessor error                 16            No
Two-byte SW interrupt             0-255         No
*/

TRAPHANDLER_NOEC_MINE(T_DIVIDE)
TRAPHANDLER_NOEC_MINE(T_DEBUG)
TRAPHANDLER_NOEC_MINE(T_NMI)
TRAPHANDLER_NOEC_MINE(T_BRKPT)
TRAPHANDLER_NOEC_MINE(T_OFLOW)
TRAPHANDLER_NOEC_MINE(T_BOUND)
TRAPHANDLER_NOEC_MINE(T_ILLOP)
TRAPHANDLER_NOEC_MINE(T_DEVICE)

TRAPHANDLER_MINE(T_DBLFLT)
TRAPHANDLER_MINE(T_TSS)
TRAPHANDLER_MINE(T_SEGNP)
TRAPHANDLER_MINE(T_STACK)
TRAPHANDLER_MINE(T_GPFLT)
TRAPHANDLER_MINE(T_PGFLT)

TRAPHANDLER_NOEC_MINE(T_FPERR)

TRAPHANDLER_NOEC_MINE(T_ALIGN)

TRAPHANDLER_NOEC_MINE(T_MCHK)
TRAPHANDLER_NOEC_MINE(T_SIMDERR)
TRAPHANDLER_NOEC_MINE(T_SYSCALL)
TRAPHANDLER_NOEC_MINE(T_DEFAULT)


/*
 * Lab 3: Your code here for _alltraps
 */

/*
Your _alltraps should:
1. push values to make the stack look like a struct Trapframe
2. load GD_KD into %ds and %es
3. pushl %esp to pass a pointer to the Trapframe as an argument to trap()
4. call trap (can trap ever return?)

struct Trapframe {
	struct PushRegs tf_regs;
	uint16_t tf_es;
	uint16_t tf_padding1;
	uint16_t tf_ds;
	uint16_t tf_padding2;
	uint32_t tf_trapno;
	below here defined by x86 hardware
	因为是push to stack 所以顺序是反过来的，其中tf_regs推入stack中可以由pushal指令来完成
*/

/*
 * 注意压栈的顺序是从struct Trapframe的底部往上压
 * 看一下前面的宏，已经压参数，压到了tf_trapno这里了。
 * 注意：使用pusha指令
 */
.global _alltraps
_alltraps:
    /*
     * 注意这里直接用了pushl前面自动补0
     * 如果要严格的对应
     * - pushw $0
     * - pushw %ds
     * - pushw $0
     * - pushw %es
     */
	pushl	%ds
	pushl 	%es
	pushal
	/*然后指向内核数据段
     * 硬件上中断门描述符进来的时候
     * 已经把CPU设置成了GD_KT也就是内核代码段。
     * 这个是硬件操作
     */
	movw $GD_KD, %ax // GD_KD内核数据段
	movw %ax, %ds
	movw %ax, %es
	/*
     * 这里是因为后面要调用trap函数
     * 1. 
     *   trap函数的定义是trap(struct Trapframe *tf)
     *   这里还有一个指针
     *   这个时候压入pushl %esp这个寄存器的内容。
     *   也就刚好是真正的指向struct Trapframe这个object的起始地址
     * 2. 
     *   如果trap函数的定义是trap(struct Trapframe tfObject)
     *   那么这个pushl %esp是没有必要压进去的
     */
	pushl %esp
	call trap
	/* 操作完成之后，
     * 没有必要要按照反方向的顺序返回
     * 因为trap函数最终会走到env_pop_tf()这个函数
     * movl $tf, %esp
     * popal
     * popl %es
     * popl %ds
     * addl $0x08, %esp
     * iret
     */

/*
发生异常后，处理器硬件会先进行一些寄存器的压入，完成后内核栈的内容为
有error code 的时候：			没有error code 的时候：
 - - - - - - - -				 - - - - - - - -
| OLD ELFAGS    |				| OLD ELFAGS    |
 - - - - - - - -				 - - - - - - - -
|		|OLD CS |				|		|OLD CS |
 - - - - - - - -				 - - - - - - - -
| OLD EIP 		|				| OLD EIP 		|
 - - - - - - - -				 - - - - - - - -
| ERROR CODE 	|
 - - - - - - - -

算上这个trapentry中操作系统做的事情，这个栈就会变成，这样就统一了格式了，完成了必要环境压栈后就调用操作系统的trap来分配异常处理函数
有error code				 无error code
 - - - - - - - -			 - - - - - - - -
|		|OLD SS |			|		|OLD SS |
 - - - - - - - -			 - - - - - - - -
| OLD ELFAGS    |			| OLD ELFAGS    |
 - - - - - - - -			 - - - - - - - -
|		|OLD CS |			|		|OLD CS |
 - - - - - - - -			 - - - - - - - -
| OLD EIP 		|			| OLD EIP 		|
 - - - - - - - -			 - - - - - - - -
| ERROR CODE 	|			| 000000000		|
 - - - - - - - -			 - - - - - - - -
｜VECTOR NO		|			｜VECTOR NO		 |
 - - - - - - - -			 - - - - - - - -
|		|OLD DS |			|		|OLD DS |
 - - - - - - - -			 - - - - - - - -
|		|OLD ES |			|		|OLD ES |
 - - - - - - - -			 - - - - - - - -
|PUSHAL(eax-edi)|			|PUSHAL(eax-edi)|
 - - - - - - - -			 - - - - - - - -
| OLD ESP 		|			| OLD ESP 		|
 - - - - - - - -			 - - - - - - - -

*/