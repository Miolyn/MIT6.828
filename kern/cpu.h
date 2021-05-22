
#ifndef JOS_INC_CPU_H
#define JOS_INC_CPU_H

#include <inc/types.h>
#include <inc/memlayout.h>
#include <inc/mmu.h>
#include <inc/env.h>

// Maximum number of CPUs
#define NCPU  8

// 定义了大部分每个 CPU 的状态
// Values of status in struct Cpu
enum {
	CPU_UNUSED = 0,
	CPU_STARTED,
	CPU_HALTED,
};

// CpuInfo它保存了每个 CPU 的变量
// Per-CPU state
struct CpuInfo {
	uint8_t cpu_id;                 // Local APIC ID; index into cpus[] below
	volatile unsigned cpu_status;   // The status of the CPU
	struct Env *cpu_env;            // The currently-running environment.
	struct Taskstate cpu_ts;        // Used by x86 to find stack for interrupt
};

// Initialized in mpconfig.c
extern struct CpuInfo cpus[NCPU];
extern int ncpu;                    // Total number of CPUs in the system
extern struct CpuInfo *bootcpu;     // The boot-strap processor (BSP)
extern physaddr_t lapicaddr;        // Physical MMIO address of the local APIC

// Per-CPU kernel stacks
// 因为内核能够同时捕获多个 CPU，因此，我们需要为每个 CPU 准备一个单独的内核栈，以防止它们运行的程序之间产生相互干扰
extern unsigned char percpu_kstacks[NCPU][KSTKSIZE];
// cpunum总是返回调用它的那个 CPU 的 ID
int cpunum(void);
// 宏 thiscpu 是当前 CPU 的 struct CpuInfo 缩略表示。
#define thiscpu (&cpus[cpunum()])

void mp_init(void);
void lapic_init(void);
void lapic_startap(uint8_t apicid, uint32_t addr);
void lapic_eoi(void);
void lapic_ipi(int vector);

#endif
