/* See COPYRIGHT for copyright information. */

#ifndef JOS_INC_ENV_H
#define JOS_INC_ENV_H

#include <inc/types.h>
#include <inc/trap.h>
#include <inc/memlayout.h>

typedef int32_t envid_t;

// An environment ID 'envid_t' has three parts:
//
// +1+---------------21-----------------+--------10--------+
// |0|          Uniqueifier             |   Environment    |
// | |                                  |      Index       |
// +------------------------------------+------------------+
//                                       \--- ENVX(eid) --/
//
// The environment index ENVX(eid) equals the environment's offset in the
// 'envs[]' array.  The uniqueifier distinguishes environments that were
// created at different times, but share the same environment index.
//
// All real environments are greater than 0 (so the sign bit is zero).
// envid_ts less than 0 signify errors.  The envid_t == 0 is special, and
// stands for the current environment.

#define LOG2NENV		10
#define NENV			(1 << LOG2NENV)
#define ENVX(envid)		((envid) & (NENV - 1))

// Values of env_status in struct Env
enum {
	//  表示那个 Env 结构是非活动的，并且因此它还在 env_free_list上。
	ENV_FREE = 0, 
	//表示那个 Env 结构所表示的是一个僵尸环境。一个僵尸环境将在下一次被内核捕获后被释放。
	ENV_DYING,
	 // 表示那个 Env 结构所代表的环境正等待被调度到处理器上去运行。
	ENV_RUNNABLE, 
	// 表示那个 Env 结构所代表的环境当前正在运行中。
	ENV_RUNNING,
	// 表示那个 Env 结构所代表的是一个当前活动的环境，但不是当前准备去运行的：
	// 例如，因为它正在因为一个来自其它环境的进程间通讯（IPC）而处于等待状态。
	ENV_NOT_RUNNABLE
};

// Special environment types
enum EnvType {
	ENV_TYPE_USER = 0,
	ENV_TYPE_FS,		// File system server
};

struct Env {
	// 它用于在那个环境不运行时保持它保存在寄存器中的值，
	// 即：当内核或一个不同的环境在运行时。当从用户模式切换到内核模式时，
	// 内核将保存这些东西，以便于那个环境能够在稍后重新运行时回到中断运行的地方。
	struct Trapframe env_tf;	// Saved registers 
	// 这是一个链接，它链接到在 env_free_list 上的下一个 Env 上
	struct Env *env_link;		// Next free Env
	// 内核在数据结构 Env 中保存了一个唯一标识当前环境的值（即：使用数组 envs中的特定槽位）。
	// 在一个用户环境终止之后，内核可能给另外的环境重新分配相同的数据结构 Env —— 但是新的环境将有一个与已终止的旧的环境不同的 env_id，
	// 即便是新的环境在数组 envs 中复用了同一个槽位。
	envid_t env_id;			// Unique environment identifier
	// 内核使用它来保存创建这个环境的父级环境的 env_id。通过这种方式，
	// 环境就可以形成一个“家族树”，这对于做出“哪个环境可以对谁做什么”这样的安全决策非常有用。
	envid_t env_parent_id;		// env_id of this env's parent
	// 它用于去区分特定的环境。对于大多数环境，它将是 ENV_TYPE_USER 的。
	// 在稍后的实验中，针对特定的系统服务环境，我们将引入更多的几种类型。
	enum EnvType env_type;		// Indicates special system environments
	// env_status： 这个变量持有以下几个值之一：ENV_FREE、ENV_DYING、ENV_RUNNABLE、ENV_RUNNING、ENV_NOT_RUNNABLE
	unsigned env_status;		// Status of the environment
	uint32_t env_runs;		// Number of times environment has run
	int env_cpunum;			// The CPU that the env is running on

	// Address space
	// 这个变量持有这个环境的内核虚拟地址的页目录。
	pde_t *env_pgdir;		// Kernel virtual address of page dir

	// Exception handling
	void *env_pgfault_upcall;	// Page fault upcall entry point

	// Lab 4 IPC
	bool env_ipc_recving;		// Env is blocked receiving
	void *env_ipc_dstva;		// VA at which to map received page
	uint32_t env_ipc_value;		// Data value sent to us
	envid_t env_ipc_from;		// envid of the sender
	int env_ipc_perm;		// Perm of page mapping received
};

#endif // !JOS_INC_ENV_H
