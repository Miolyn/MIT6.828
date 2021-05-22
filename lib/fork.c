// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	extern volatile pte_t uvpt[];
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	// 只有写操作时 的写错误缺页异常 并且是 写时复制可以处理，其余情况都panic
	if (!((err & FEC_WR) && (uvpt[PGNUM(addr)] & PTE_COW))) {
        panic("pgfault: not copy-on-write\n");
    }
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	// envid_t envid = sys_getenvid();    // do not use thisenv!
	// 为当前进程的PFTEMP地址处分配一块内存
	if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
        panic("sys_page_alloc: %e", r);
	// 将addr处的内容都移到PFTEMP处
	memmove(PFTEMP, (void*)ROUNDDOWN(addr, PGSIZE), PGSIZE);
	// 将addr地址映射到PFTEMP对应的物理地址处
    if ((r = sys_page_map(0, (void*)PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
        panic("sys_page_map: %e", r);
	// 取消PFTEMP虚拟地址的映射
    if ((r = sys_page_unmap(0, (void*)PFTEMP)) < 0)
        panic("sys_page_unmap: %e", r);
	// panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	extern volatile pte_t uvpt[];
	// LAB 4: Your code here.
	// envid_t parentEid = sys_getenvid();
	uintptr_t pn_va = pn << PGSHIFT;
	// if(uvpt[pn] & (PTE_W | PTE_COW)){
	// PTE_COW marks copy-on-write page table entries.
	if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
		// 设置COW位并设置取消PTE_W设置为不可读。
		// 对于UTOP以下的可写的或者写时拷贝的页，拷贝映射关系的同时，需要同时标记当前进程和子进程的页表项为PTE_COW
		if((r = sys_page_map(0, (void*)pn_va, envid, (void*)pn_va, (PTE_COW | PTE_P | PTE_U)))){
			return r;
		}
		if((r = sys_page_map(0, (void*)pn_va, 0, (void*)pn_va, (PTE_COW | PTE_P | PTE_U)))){
			return r;
		}
	} else{
		// 对于只读的页，只需要拷贝映射关系即可
		if((r = sys_page_map(0, (void*)pn_va, envid, (void*) pn_va, (PTE_U | PTE_P)))){
			return r;
		}
	}

 	// panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	int r;
	uintptr_t addr;
	extern volatile pde_t uvpd[];
    extern volatile pte_t uvpt[];
	// 设置缺页处理函数
	set_pgfault_handler(pgfault);
	// 创建子进程
	envid_t envid = sys_exofork();
	if(envid < 0){
		panic("sys_exofork: %e", envid);
	}else if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
		// uvpt在用户程序的入口代码lib/entry.c中定义，它的值就为UVPT。
		// 在pgdir中，定义UVPT页目录项的位置映射的是到pgdir的地址
		// 在lib/entry.S中对uvpd的定义是.set uvpd, (UVPT+(UVPT>>12)*4)
		// 其中UVPT代表的其实就是pgdir基地址，(UVPT>>12)*4代表的是pgdir在pgdir中存储的位置
		// env_pgdir[PDX(UVPT)]=[PDX(UVPT)]->PADDR(pgdir)
		// UVPT=pd|pt|off, pd->PADDR(pgdir),pt->(UVPT>>12)*4(在pgdir页目录中，指向pgdir实地址的页目录项)
		// 所以uvpd可以根据索引获得第index的页目录，uvpt可以获取整个用户页表(1024*1024)
	for (addr = (uintptr_t) UTEXT; addr < USTACKTOP; addr += PGSIZE){
		size_t pn = PGNUM(addr);
		// 所有存在的页且用户可写的页都要设置成共享页
		if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[pn] & PTE_P) && (uvpt[pn] & PTE_U)){
			// 拷贝当前进程映射关系到子进程
			duppage(envid, pn);
		}
	}
	extern void _pgfault_upcall(void);
	// 为子进程分配异常栈
	if((r = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
        panic("sys_page_alloc: %e", r);
	// 为子进程设置_pgfault_upcall，因为上面设置的是父进程的
    if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall) < 0))
        panic("sys_env_set_pgfault_upcall failed\n");
	// 设置子进程状态为可运行状态
    if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
        panic("sys_env_set_status: %e", envid);

	return envid;
	// panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
