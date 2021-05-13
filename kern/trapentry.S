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


.global _alltraps
_alltraps:
	pushl	%ds
	pushl 	%es
	pushal
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp
	call trap