// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "test", "make some test about the lab", mon_test},
	{ "backtrace", "Display function stack one line at a time", mon_backtrace},
	{ "si", "Step instruction.", mon_stepi},
	{ "c", "Continue.", mon_continue},

};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int mon_test(int argc, char **argv, struct Trapframe *tf){
	int x = 1, y = 3, z = 4;
	cprintf("x %d, y %x, z %d\n", x, y, z);
	unsigned int i = 0x00646c72;
	// 57616=0xE110
	// &i就是表示i的地址，相当于把i的地址理解成char* 地址传递给了printf，让printf将其作为char*来翻译
	// 0x00646c72按照小端存储的时候就是72 6c 64 00对应ASCII码来看就是rld\0
	cprintf("H%x Wo%s\n", 57616, &i);
	cprintf("x=%d y=%d\n", 3);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf){
	cprintf("Stack backtrace:\n");
	uint32_t *p = (uint32_t*)(&argc);
	struct Eipdebuginfo info;	// 定义在kern/kdebug.h	
	// 栈帧: 从左到右 地址递增 ： 本地变量、旧的ebp、返回地址、函数参数
	// uint32_t *ebp = p - 2;
	uint32_t *ebp = (uint32_t*)read_ebp();
	uint32_t *eip;
	// Your code here.
	while (ebp != 0){
		int i = 0;
		cprintf (" ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", ebp, ebp[1]
                 , ebp[2], ebp[3], ebp[4], ebp[5], ebp[6]);
		if (debuginfo_eip(ebp[1], &info) != -1){
			cprintf ("%s:%d: %.*s+%d\n",info.eip_file
                    ,info.eip_line
                    ,info.eip_fn_namelen,info.eip_fn_name
                    ,ebp[1]-info.eip_fn_addr);
		}
		else
			cprintf("Unknown failed.\n");
		ebp = (uint32_t*)*(ebp);

	}


	return 0;
}

int mon_stepi(int argc, char **argv, struct Trapframe *tf){
	if(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG){
		// [Trap flag]   将该位设置为1以允许单步调试模式，清零则禁用该模式。
		tf->tf_eflags |= FL_TF;
	} else{
		cprintf("You can only use stepi when the processor encounter Debug or Breakpoint exceptions.\n");
	}
	return -1;
}


int mon_continue(int argc, char **argv, struct Trapframe *tf){
	if(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG){
		tf->tf_eflags &= ~FL_TF;
	}
	return -1;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
