// User-level page fault handler support.
// Rather than register the C page fault handler directly with the
// kernel as the page fault handler, we register the assembly language
// wrapper in pfentry.S, which in turns calls the registered C
// function.

#include <inc/lib.h>


// Assembly language pgfault entrypoint defined in lib/pfentry.S.
// 这个全局变量定义在pfentry.S文件中的 .globl _pgfault_upcall
// 这个.S文件主要做的事就是从栈中的UTrapframe恢复用户环境，然后设置好环境切换到eip即env_pgfault_upcall进行处理
extern void _pgfault_upcall(void);

// Pointer to currently installed C-language pgfault handler.
void (*_pgfault_handler)(struct UTrapframe *utf);

//
// Set the page fault handler function.
// If there isn't one yet, _pgfault_handler will be 0.
// The first time we register a handler, we need to
// allocate an exception stack (one page of memory with its top
// at UXSTACKTOP), and tell the kernel to call the assembly-language
// _pgfault_upcall routine when a page fault occurs.
//
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
	int r;

	if (_pgfault_handler == 0) {
		// First time through!
		// LAB 4: Your code here.
		r = sys_page_alloc(0, (void*)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W);
		if(r < 0){
			panic("panic sys_page_alloc lib/pgfault.c:%e", r);
		}
		r = sys_env_set_pgfault_upcall(0, _pgfault_upcall);
		if(r < 0){
			panic("panic lib/pgfault.c:%e", r);
		}
		// panic("set_pgfault_handler not implemented");
	}

	// Save handler pointer for assembly to call.
	_pgfault_handler = handler;
}
