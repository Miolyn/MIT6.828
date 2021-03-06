/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, s, len, PTE_U | PTE_P);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
// 创建一个新的进程，用户地址空间没有映射，不能运行，寄存器状态和父环境一致。
// 在父进程中sys_exofork()返回新进程的envid，子进程返回0。
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here.
	struct Env *newEnv;
	size_t r = env_alloc(&newEnv, curenv->env_id);
	if(r){
		return r;
	}
	// 复制tf，这个tf当前运行的位置应该是fork 之后的第一条语句
	newEnv->env_tf = curenv->env_tf;
	newEnv->env_status = ENV_NOT_RUNNABLE;
	// newEnv->env_parent_id = curenv->env_id;

	// sys_exofork should return 0 in child environment by set register %eax with 0.Howerver,we set newEnv->env_tf 
	// with ENV_NOT_RUNNABLE above,the new environment can't run util parent environment has allowed it explicitly.
	// 返回值变成0，这个reg_eax=0是fork的子进程返回env id为0的关键，因为eax中放的就是返回值，
	// 所以父进程得到的是子进程的id，子进程得到的就是0
	newEnv->env_tf.tf_regs.reg_eax = 0;
	return newEnv->env_id;
	// panic("sys_exofork not implemented");
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
// 设置一个特定进程的状态为ENV_RUNNABLE或ENV_NOT_RUNNABLE。
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.
	if(status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE){
		return -E_INVAL;
	}
	struct Env* env;
	if(envid2env(envid, &env, true) != 0){
		return -E_BAD_ENV;
	}
	env->env_status = status;
	return 0;
	// panic("sys_env_set_status not implemented");
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
// 该系统调用为指定的用户环境设置env_pgfault_upcall。
// 缺页中断发生时，会执行env_pgfault_upcall指定位置的代码。当执行env_pgfault_upcall指定位置的代码时，
// 栈已经转到异常栈，并且压入了UTrapframe结构。
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env *env;
	if(envid2env(envid, &env, true) < 0){
		return -E_BAD_ENV;
	}
	env->env_pgfault_upcall = func;
	return 0;
	// panic("sys_env_set_pgfault_upcall not implemented");
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
// 为特定进程分配一个物理页，映射指定线性地址va到该物理页
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
	int32_t leastPerm = (PTE_U | PTE_P);
	if(va >= (void*)UTOP || (uintptr_t)va % PGSIZE){
		return -E_INVAL;
	}
	if(!(perm & PTE_U) || (perm & ~PTE_SYSCALL)){
		return -E_INVAL;
	}
	// if(va >= (void*)UTOP || ROUNDDOWN(va, PGSIZE) != va || ((perm & PTE_SYSCALL) != perm) 
	// 	|| ((perm & leastPerm) != leastPerm)){
	// 	return -E_INVAL;
	// }
	struct Env *env;
	if(envid2env(envid, &env, true) != 0){
		return -E_BAD_ENV;
	}
	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	if(!pp) return -E_NO_MEM;
	if(page_insert(env->env_pgdir, pp, va, perm) < 0){
		page_free(pp);
		return -E_NO_MEM;
	}
	return 0;
	// panic("sys_page_alloc not implemented");
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
// 拷贝页表，使指定进程共享当前进程相同的映射关系。本质上是修改特定进程的页目录和页表。
// 共享同样的地址空间，而不是拷贝page的内容
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	int32_t leastPerm = (PTE_U | PTE_P);
	struct Env * srcEnv, *dstEnv;
	if(envid2env(srcenvid, &srcEnv, true) || envid2env(dstenvid, &dstEnv, true)){
		return -E_BAD_ENV;
	}
	if(srcva >= (void*)UTOP || dstva >= (void*)UTOP || ROUNDDOWN(srcva, PGSIZE) != srcva || ROUNDDOWN(dstva, PGSIZE) != dstva
		|| ((perm & PTE_SYSCALL) != perm) || ((perm & leastPerm) != leastPerm)){
			return -E_INVAL;
	}
	pte_t *srcVaPageTable;
	struct PageInfo* srcPP = page_lookup(srcEnv->env_pgdir, srcva, &srcVaPageTable);
	if(!srcPP){
		return -E_INVAL;
	}
	if(!(*srcVaPageTable & PTE_W) && (perm & PTE_W)){
		return -E_INVAL;
	}
	if(page_insert(dstEnv->env_pgdir, srcPP, dstva, perm) != 0){
		return -E_NO_MEM;
	}
	return 0;
	// panic("sys_page_map not implemented");
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
// 解除页映射关系。
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.
	struct Env *env;
	if(envid2env(envid, &env, true) != 0){
		return -E_BAD_ENV;
	}
	if(va >= (void*)UTOP || ROUNDDOWN(va, PGSIZE) != va){
		return -E_INVAL;
	}
	page_remove(env->env_pgdir, va);
	return 0;
	// panic("sys_page_unmap not implemented");
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	struct Env *targetEnv, *currentEnv;
	int r;
	if((r = envid2env(envid, &targetEnv, 0)) < 0 || (r = envid2env(0, &currentEnv, 0)) < 0){
		return -E_BAD_ENV;
	}
	if(!targetEnv->env_ipc_recving){
		return -E_IPC_NOT_RECV;
	}
	targetEnv->env_ipc_recving = 0;
	targetEnv->env_ipc_from = curenv->env_id;
	targetEnv->env_ipc_value = value;
	targetEnv->env_status = ENV_RUNNABLE;
	targetEnv->env_tf.tf_regs.reg_eax = 0;
	if((uintptr_t)srcva < UTOP){
		if((uintptr_t)srcva & (PGSIZE - 1)) return -E_INVAL;
		if(!(perm & PTE_P) || !(perm & PTE_U) || (perm & (~PTE_SYSCALL))) return -E_INVAL;
		pte_t *pte;
		struct PageInfo* pp;
		if(!(pp = page_lookup(currentEnv->env_pgdir, srcva, &pte))){
			return -E_INVAL;
		}
		if((perm & PTE_W) && !(*pte & PTE_W)){
			return -E_INVAL;
		}
		if((uintptr_t)targetEnv->env_ipc_dstva < UTOP){
			// 共享相同的映射关系
			if((r = page_insert(targetEnv->env_pgdir, pp, targetEnv->env_ipc_dstva, perm)) < 0) return r;
			targetEnv->env_ipc_perm = perm;
		}
		
	} else{
		targetEnv->env_ipc_perm = 0;
	}
	return 0;
	// panic("sys_ipc_try_send not implemented");
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if(dstva < (void*)UTOP && (size_t)dstva % PGSIZE) return -E_INVAL;
	struct Env *currentEnv;
	if(envid2env(0, &currentEnv, 0) < 0){
		return -E_BAD_ENV;
	}
	currentEnv->env_ipc_recving = 1;
	currentEnv->env_ipc_dstva = dstva;
	currentEnv->env_status = ENV_NOT_RUNNABLE;
	currentEnv->env_ipc_from = 0;
	sys_yield();
	// panic("sys_ipc_recv not implemented");
	return 0;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	// panic("syscall not implemented");

	switch (syscallno) {
	case SYS_cputs:
		// cprintf("SYS_cputs\n");
		sys_cputs((char*)a1, (size_t)a2);
		return 0;
	case SYS_cgetc:
		// cprintf("SYS_cgetc\n");
		return sys_cgetc();
	case SYS_getenvid:
		// cprintf("SYS_getenvid\n");
		return sys_getenvid();
	case SYS_env_destroy:
		// cprintf("SYS_env_destroy\n");
		return sys_env_destroy(a1);
	case SYS_yield:
		// cprintf("SYS_yield\n");
		sys_yield();
		return 0;
	case SYS_exofork:
		// cprintf("SYS_exofork\n");
		return sys_exofork();
	case SYS_env_set_status:
		// cprintf("SYS_env_set_status\n");
		return sys_env_set_status((envid_t)a1, (int)a2);
	case SYS_page_alloc:
		// cprintf("SYS_page_alloc\n");
		return sys_page_alloc((envid_t)a1, (void*)a2, (int)a3);
	case SYS_page_map:
		// cprintf("SYS_page_map\n");
		return sys_page_map((envid_t)a1, (void*)a2, (envid_t)a3, (void*)a4, (int)a5);
	case SYS_page_unmap:
		// cprintf("SYS_page_unmap\n");
		return sys_page_unmap((envid_t)a1, (void*)a2);
	case SYS_env_set_pgfault_upcall:
		// cprintf("SYS_env_set_pgfault_upcall\n");
		return sys_env_set_pgfault_upcall((envid_t)a1,(void*) a2);
	case SYS_ipc_try_send:
		return sys_ipc_try_send((envid_t)a1, (uint32_t)a2, (void*)a3, (unsigned int) a4);
	case SYS_ipc_recv:
		return sys_ipc_recv((void*) a1);
	default:
		return -E_NO_SYS;
	}
}

