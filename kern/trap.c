#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>

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
	make_gate(T_BRKPT, 0, 0);
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
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
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
	cprintf("tf trapno:%d\n", tf->tf_trapno);

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

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	cprintf("Incoming TRAP frame at %p\n", tf);

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		assert(curenv);

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

	// Return to the current environment, which should be running.
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.
	// 虽然user/softint是通过int指令，想要触发page fault，
	// 但是，由于user/softint运行在用户态下，
	//而page fault的Interrupt descriptor中的DPL标识调用page fault的handler function需要内核级别特权，
	// 即0，因此，根据Intel IA-32 developer’s manual，会由硬件检测出general protection exception
	if((tf->tf_cs & 3) == 0){
		panic(":( Your kernel triger a page fault at va@0x%08x !Bad kernel", fault_va);
	}
	// LAB 3: Your code here.

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}

