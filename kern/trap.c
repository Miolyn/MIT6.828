#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}

#define make_gate(name, is_trap, dpl) \
	void name##_HANDLER (); \
	SETGATE(idt[name], is_trap, GD_KT, name##_HANDLER, dpl)

void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	extern long entryPointOfTraps[][2];
	int i;
	for(i = 0; i <= 20; ++i){
		if(entryPointOfTraps[i][1] == T_BRKPT || entryPointOfTraps[i][1] == T_SYSCALL){
			SETGATE(idt[entryPointOfTraps[i][1]], 1, GD_KT, entryPointOfTraps[i][0], 3);
		} 
		else SETGATE(idt[entryPointOfTraps[i][1]], 0, GD_KT, entryPointOfTraps[i][0], 0);

		// 如果在用户程序中int 13想要触发缺页中断就将这个缺页的处理段设置为3用户态也能访问，但实际上这样是不科学的。
		// 缺页只能由操作系统来进行处理，所以需要设置dpl为0，内核权限级。如果用户态想要触发缺页中断的话就会转变成int 13 General Protection
		// else if(entryPointOfTraps[i][1] == T_PGFLT){
		// 	SETGATE(idt[entryPointOfTraps[i][1]], 0, GD_KT, entryPointOfTraps[i][0], 3);
		// }
	}
	/*
	make_gate(T_DIVIDE, 0, 0);
	make_gate(T_DEBUG, 0, 0);
	make_gate(T_NMI, 0, 0);
	make_gate(T_BRKPT, 1, 3);
	make_gate(T_OFLOW, 0, 0);
	make_gate(T_BOUND, 0, 0);
	make_gate(T_ILLOP, 0, 0);
	make_gate(T_DEVICE, 0, 0);
	make_gate(T_DBLFLT, 0, 0);
	make_gate(T_TSS, 0, 0);
	make_gate(T_SEGNP, 0, 0);
	make_gate(T_STACK, 0, 0);
	make_gate(T_GPFLT, 0, 0);
	make_gate(T_PGFLT, 0, 0);
	make_gate(T_FPERR, 0, 0);
	make_gate(T_ALIGN, 0, 0);
	make_gate(T_MCHK, 0, 0);
	make_gate(T_SIMDERR, 0, 0);
	make_gate(T_SYSCALL, 1, 3);
	make_gate(T_DEFAULT, 0, 0);
	*/
	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//   - Initialize cpu_ts.ts_iomb to prevent unauthorized environments
	//     from doing IO (0 is not the correct value!)
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:
	
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - thiscpu->cpu_id * (KSTKSIZE + KSTKGAP);
	// thiscpu->cpu_ts.ts_esp0 = (uintptr_t)percpu_kstacks[thiscpu->cpu_id];
	thiscpu->cpu_ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	// ts.ts_esp0 = KSTACKTOP;
	// ts.ts_ss0 = GD_KD;

	// GD_TSS0   0x28     // Task segment selector for CPU 0
	// Initialize the TSS slot of the gdt.
	// 段选择符分为3部分，从左到右第一部分是13bit的index，第二部分是1bit的TI表示描述符在LDT中，然后是2bit的RPL优先级
	// 所以GD_TSS0的index就要先右移3bit
	gdt[(GD_TSS0 >> 3) + thiscpu->cpu_id] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
					sizeof(struct Taskstate), 0);
	gdt[(GD_TSS0 >> 3) + thiscpu->cpu_id].sd_s = 0;

	// 可以通过 ltr指令跟上TSS段描述符的选择子来加载TSS段。该指令是特权指令，只能在特权级为0的情况下使用。
	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	// 为了装载正确的gdt就要加上这个CPU对应的index，所以要加到段选择符的index段上，因此cpu_id要右移3bit
	ltr(GD_TSS0 + (thiscpu->cpu_id << 3));
	
	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	cprintf("tf trapno:%d,trapname:%s\n", tf->tf_trapno, trapname(tf->tf_trapno));

	switch (tf->tf_trapno)
	{
	case T_SYSCALL:
		tf->tf_regs.reg_eax = syscall(
			tf->tf_regs.reg_eax, 
			tf->tf_regs.reg_edx, 
			tf->tf_regs.reg_ecx, 
			tf->tf_regs.reg_ebx, 
			tf->tf_regs.reg_edi, 
			tf->tf_regs.reg_esi
		);
		return;
	case T_PGFLT:
		cprintf("\ninto T_PGFLT handler\n");
		page_fault_handler(tf);
		return;
	case T_BRKPT:
		monitor(tf);
		return;
	case T_DEBUG:
		monitor(tf);
		return;
	default:
		// env_destroy(curenv);
		break;
	}

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		lock_kernel();
		assert(curenv);

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();
	// LAB 3: Your code here.

	// Handle kernel-mode page faults.
	// 虽然user/softint是通过int指令，想要触发page fault，
	// 但是，由于user/softint运行在用户态下，
	//而page fault的Interrupt descriptor中的DPL标识调用page fault的handler function需要内核级别特权，
	// 即0，因此，根据Intel IA-32 developer’s manual，会由硬件检测出general protection exception
	if((tf->tf_cs & 3) == 0){
		panic(":( Your kernel triger a page fault at va@0x%08x !Bad kernel", fault_va);
	}

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').
	
	// LAB 4: Your code here.
	if(curenv->env_pgfault_upcall){
		uintptr_t stack_top;
		if(curenv->env_tf.tf_esp >= UXSTACKTOP - PGSIZE && curenv->env_tf.tf_esp <= UXSTACKTOP - 1){
			// the user environment is already running on the exception stack when an exception occurs.
			// Thus,we should start the new stack frame just under the tf->tf_esp.We have to push a word
			// at the top of the trap-time stack as a scratch space.
			user_mem_assert(curenv, (void*)(curenv->env_tf.tf_esp - 4), 4, PTE_U | PTE_W);
			stack_top = curenv->env_tf.tf_esp - 4;
		} else{
			stack_top = UXSTACKTOP;
		}
		stack_top -= sizeof(struct UTrapframe);
		user_mem_assert(curenv, (void*)stack_top, sizeof(struct UTrapframe), PTE_U | PTE_W);
		struct UTrapframe *utf = (struct UTrapframe*)stack_top;
		/*
<-- UXSTACKTOP
    trap-time esp
    trap-time eflags
    trap-time eip
    trap-time eax     start of struct PushRegs
    trap-time ecx
    trap-time edx
    trap-time ebx
    trap-time esp
    trap-time ebp
    trap-time esi
    trap-time edi      end of struct PushRegs
    tf_err (error code)
    fault_va           <-- %esp when handler is run
		*/
		// faulting address
		utf->utf_fault_va = fault_va;
		utf->utf_err = tf->tf_err;
		utf->utf_regs = tf->tf_regs;
		// 页错误处理程序入口
		utf->utf_eip = tf->tf_eip;
		utf->utf_eflags = tf->tf_eflags;
		utf->utf_esp = tf->tf_esp;

		tf->tf_eip = (uintptr_t)curenv->env_pgfault_upcall;
		tf->tf_esp = stack_top;
		env_run(curenv);
	}
	

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

