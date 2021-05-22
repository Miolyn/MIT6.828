/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

static void boot_aps(void);


void
i386_init(void)
{
	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	cprintf("6828 decimal is %o octal!\n", 6828);

	// Lab 2 memory management initialization functions
	mem_init();

	// Lab 3 user environment initialization functions
	env_init();
	trap_init();

	// Lab 4 multiprocessor initialization functions
	// 收集多处理的信息
	mp_init();
	// 初始化自己lapic  这个时候其他CPU还没有启动，此时还是BSP
	lapic_init();

	// Lab 4 multitasking initialization functions
	pic_init();

	// Acquire the big kernel lock before waking up APs
	// Your code here:
	lock_kernel();

	// Starting non-boot CPUs
	// 驱动 AP 的引导过程
	boot_aps();

	// Start fs.
	ENV_CREATE(fs_fs, ENV_TYPE_FS);

#if defined(TEST)
	// Don't touch -- used by grading script!
	ENV_CREATE(TEST, ENV_TYPE_USER);
#else
	// Touch all you want.
	ENV_CREATE(user_icode, ENV_TYPE_USER);
#endif // TEST*

	// Should not be necessary - drains keyboard because interrupt has given up.
	kbd_intr();

	// Schedule and run the first user environment!
	sched_yield();
}

// While boot_aps is booting a given CPU, it communicates the per-core
// stack pointer that should be loaded by mpentry.S to that CPU in
// this variable.
void *mpentry_kstack;

// APs在实模式下启动，与boot/boot.S的引导过程相似：boot_aps()将AP入口代码（kern/mpentry.S）拷贝到实模式下的一个可寻址的内存位置。
// 与boot/boot.S的引导过程不同的是，jos会控制AP将会在哪里开始执行代码；jos将入口代码拷贝到0x7000（MPENTRY_PADDR），但640KB以下的任何unused、page-aligned的物理地址都被使用。
// Start the non-boot (AP) processors.
// 此后，boot_aps()逐一激活APs，通过给匹配AP的LAPIC发送STARTUP IPIs以及一个初始CS:IP地址，AP将在该地址上（即MPENTRY_PADDR）执行入口代码。
// kern/mpentry.S在简单设置之后将AP运行模式设为保护模式，开启分页，然后调用c函数mp_main()（also in kern/init.c）。
// boot_aps()等待AP发送CPU_STARTED信号（见struct CpuInfo的cpu_status域），然后激活下一个AP。
static void
boot_aps(void)
{
	extern unsigned char mpentry_start[], mpentry_end[];
	void *code;
	struct CpuInfo *c;

	// Write entry code to unused memory at MPENTRY_PADDR
	// AP 们在实模式中开始，因此，boot_aps() 将 AP 入口代码（kern/mpentry.S）复制到实模式中的那个可寻址内存地址上。
	code = KADDR(MPENTRY_PADDR);
	// 将AP入口代码（kern/mpentry.S）拷贝到实模式下的一个可寻址的内存位置。
	memmove(code, mpentry_start, mpentry_end - mpentry_start);

	// Boot each AP one at a time
	for (c = cpus; c < cpus + ncpu; c++) {
		if (c == cpus + cpunum())  // We've started already.
			continue;

		// Tell mpentry.S what stack to use 
		// 每个CPU的内核栈，在mpentry.S中将会movl    mpentry_kstack, %esp
		// 启动ap处理器的时候设置他们各自的内核栈
		mpentry_kstack = percpu_kstacks[c - cpus] + KSTKSIZE;
		// Start the CPU at mpentry_start
		// 发送STARTUP处理器中断IPI让ap使用code代码启动处理器并做初始化
		lapic_startap(c->cpu_id, PADDR(code));
		// Wait for the CPU to finish some basic setup in mp_main()
		// boot_aps()等待AP执行完code启动完成后设置好cpu状态，然后激活下一个AP。
		while(c->cpu_status != CPU_STARTED)
			;
	}
	// We only have one user environment for now, so just run it.
	// env_run(&envs[0]);
}

// Setup code for APs
void
mp_main(void)
{
	// We are in high EIP now, safe to switch to kern_pgdir 
	// 设置内核页表
	lcr3(PADDR(kern_pgdir));
	cprintf("SMP: CPU %d starting\n", cpunum());
	// 初始化ap处理器的lapic
	lapic_init();
	// 初始化处理器的env，包括设置ES, DS, and SS段寄存器的段选择符，设置为内核段的data和text段
	env_init_percpu();
	// 初始化ap处理器的trap，包括设置tss描述符(填写内核的栈地址，内核data段选择符，以及设置tss描述到GDT中然后加载TSS段寄存器)
	trap_init_percpu();
	xchg(&thiscpu->cpu_status, CPU_STARTED); // tell boot_aps() we're up

	// Now that we have finished some basic setup, call sched_yield()
	// to start running processes on this CPU.  But make sure that
	// only one CPU can enter the scheduler at a time!
	//
	// Your code here:
	lock_kernel();
	sched_yield();
	// Remove this after you finish Exercise 4
	// for (;;);
}

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	__asm __volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic on CPU %d at %s:%d: ", cpunum(), file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
