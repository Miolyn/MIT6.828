# xv6

## 一些问题

如果我们用哈佛架构的话，到时操作系统设置eip之类的会不会有很大冲突，可不可以提前改成冯诺依曼架构

## lab1

#### bios启动过程

bios会在某个时刻加载磁盘的引导块，找到boot loader所在的boot.S文件，然后把它加载到物理内存0x7c00处，然后将计算机控制权交给它。

> boot.S

此时计算机将会关闭中断，设置基本的段寄存器。然后关闭A20功能，避免超高1MB的地址会被绕回0的问题出现。然后加载gdt，全局描述符段表，修改cr0进入保护模式。然后将保护模式下的段选择符给段寄存器。最后将0x7c00赋值给%esp寄存器，然后调用bootmain进入c函数进行下一步操作。

> bootmain

这个函数主要功能就是将内核开始代码从磁盘加载到内存处。

首先从磁盘位置0开始读取4kb一页的内容到内存的0x10000处。判断读取的内容是不是ELF文件。如果是ELF文件就将ELF文件的代码段读取从磁盘加载到内存对应程序物理地址处。然后系统转交控制权给 ELF 中相应代码的虚拟地址(entry.S)。

> Entry.S

函数主要功能就是开启页表进入虚拟地址时代，这里使用汇编写是因为C语言要求使用虚拟内存访问虚拟地址，但是现在还没有开启虚拟内存。

这里一开始就将页表物理地址放入cr3页目录基址寄存器(其中页表已经使用c代码定义存放在了对应位置了[entrypgdir.c中，将0-4MB和KERNBASE-KERNBASE+4MB都映射到了0-4MB]，页表应该在bootmain从磁盘加载的4kb里面)。然后修改cr0开启虚拟地址模式，开启页表等。然后设置ebp栈底指针为0，设置栈顶指针为bootstacktop与之相对应的是bootstack栈底。然后调用一个迁移函数(relocated, 这里relocated是虚拟地址)，将ebp设置为0，设置新的esp然后调用i386_init函数(c代码，因为已经进入虚拟内存时代了)。

在页表已经正常运行的情况下，程序仍然运行在低位地址，却完全正常。因为页表将高位地址和地位地址都映射到了低位地址处。

现在这个时候暂时内核只有4MB内存可以使用(因为设置的页表就映射了4MB也够启动程序用了)，要等到使用mem_init设置页表之后才能进入真正的大内存时代。

#### 格式化控制台的输出

> 解释 `printf.c` 和 `console.c` 之间的接口。尤其是，`console.c` 出口的函数是什么？这个函数是如何被 `printf.c` 使用的？

printf和console之间主要的联系就是printf会调用console中的cputchar函数，效果就是打印一个字符

> 在 `console.c` 中解释下列的代码：

```c
	// What is the purpose of this?
	// 显示缓冲区满了
	if (crt_pos >= CRT_SIZE) {
		int i;
		// memmove(void *dst, const void *src, size_t n) 
		// 从crt_buf + CRT_COLS向crt_buf移动除最后一行外的数据
		// 相当于控制台最上面一行清空，然后最下面新起一行，就是整个控制台向下翻了一行
		memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
        // 移动完之后，还得把最后一行，即[CRT_SOZE-CRT_COLS, CRT_SIZE)这个区间的格子全部换成空白符 ' '。
		for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
			crt_buf[i] = 0x0700 | ' ';
        // buf区的光标也要上移
		crt_pos -= CRT_COLS;
	}
```

就是显示的缓冲区满了，需要将整个控制台向下翻一行

![img](https://img-blog.csdnimg.cn/20190501170902526.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3c1NTEwMA==,size_16,color_FFFFFF,t_70)

> 下列的问题你可能需要参考第一节课中的笔记。这些笔记涵盖了 GCC 在 x86 上的调用规则。
> 一步一步跟踪下列代码的运行：

```c
int x = 1, y = 3, z = 4;
cprintf("x %d, y %x, z %d\n", x, y, z);
```

1. 在调用 `cprintf()` 时，`fmt` 做了些什么？`ap` 做了些什么？
2. （按运行顺序）列出 `cons_putc`、`va_arg`、以及 `vcprintf` 的调用列表。对于 `cons_putc`，同时列出它的参数。对于`va_arg`，列出调用之前和之后的 `ap` 内容？对于 `vcprintf`，列出它的两个参数值。

首先看cprintf函数定义

```c
int cprintf(const char *fmt, ...){
    // 初始化一个 可变参数指针va_list
	va_list ap;
	int cnt;
	// 宏va_start(ap,lastfix)是为了初始化变参指针ap，以指向可变参数列表中未命名的第一个参数，
	// 即指向lastfix后的第一个变参。它必须在指针使用之前调用一次该宏，参数列表中至少有一个未命名的可变参数。
	// 这里表示fmt后面开始是一个可变参数，使用va_start进行初始化
	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	// 宏va_end(ap)功能是完成清除变量ap的作用，表明程序以后不再使用，若该指针变量需再使用，
	// 必须重新调用宏va_start以启动该变量。
	va_end(ap);

	return cnt;
}
```

其中fmt指的是cprintf最前面的参数的一串字符串的指针，ap指的是fmt后面变量列表var_list的起始地址，这里可以理解为x的地址。ap要使用va_start函数来找到var_list起始地址。

调用关系`cprintf -> vcprintf -> putch -> cputchar`

>  运行以下代码

```c
unsigned int i = 0x00646c72;
cprintf("H%x Wo%s", 57616, &i);
```

57616=0xE110

&i就是表示i的地址，相当于把i的地址理解成char\* 地址传递给了printf，让printf将其作为char\*来翻译

 0x00646c72按照小端存储的时候就是72 6c 64 00对应ASCII码来看就是rld\0

> 在下列代码中，`y=` 会输出什么？（注意：这个问题没有确切值）为什么会发生这种情况？

```c
cprintf("x=%d y=%d", 3);
```

输出的y是不确定的，y的值是根据传递参数3对应地址的后一个地址的数翻译成int显示出来

就是把第一个数字3的地址+4Bit之后，将新的地址当成int来看，强行读取并打印。

> 假设修改了 GCC 的调用规则，以便于按声明的次序在栈上推送参数，这样最后的参数就是最后一个推送进去的。那你如何去改变 `cprintf` 或者它的接口，以便它仍然可以传递数量可变的参数？

相当于fmt在栈底，要根据fmt的位置-4才能得到第一个ap的位置(栈是向下增长的，每次都是esp-=4这样)

#### 栈

> 实现mon_backtrace

```c
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
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
```

> 实现查找行号

```c
// Search within [lline, rline] for the line number stab.
	// If found, set info->eip_line to the right line number.
	// If not found, return -1.
	//
	// Hint:
	//	There's a particular stabs type used for line numbers.
	//	Look at the STABS documentation and <inc/stab.h> to find
	//	which one.
	// Your code here.
	stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
	if(lline > rline)
		return -1;
	info->eip_line = stabs[rline].n_desc;
```



## lab2

lab2主要是配置了jos的基本页表的虚拟内存管理机制。

在lab1中初始化设置了基本的控制台输出之类的东西，现在使用mem_init初始化内存管理。

目前使用的内存还是在entry.S中设置的4MB小内存，主要就是为了让内核的初始化c代码可以正常运行才设置的暂时性4MB页表虚拟内存(因为c代码需要运行在虚拟地址上)。现在就要通过mem_init正式开启超大内存的新进展。

首先通过i386_detect_memory函数获取当前可用内存情况。其中basemem就是0-640k之间的memory,extmem是1M以后的memory。在qemu虚拟机中的内存情况是：Physical memory: 66556K available, base = 640K, extended = 65532K。

知道了内存情况之后，由于现在还没有建立正规的页表，所以使用boot_alloc函数来进行内存分配。在boot_alloc函数中，使用了end这个指针，指向的内存地址是bss段后的第一个字节，然后还需要将其页对齐。然后就根据这个地址开始进行内存分配。

首先使用boot_alloc，分配一页4KB的内存来存储内核的页目录项kern_pgdir。然后设置kern_pgdir，先设置一个用户只读的虚拟页表的映射。地址映射从UVPT映射到kern_pgdir的物理地址处，其中kern_pgdir的地址是页对齐的。(UVPT是ULIM下的第一个页目录大小的内容，大小为PTSIZE)。

然后再分配足量的内存来存放PageInfo数组，用于管理物理页。

建立基本的页表管理所需内容后，开始页表初始化，首先初始化PageInfo数组，并设置闲置页链表。在page_init中进行页表初始化。bootAllocBaseAddr地址指向boot_alloc函数分配完内存后指向的最后一个字节的内存地址。在物理页初始化的时候有部分页是不能分配的。物理内存中第一页的内存要存储IDT中断描述符表和BIOS结构体，所以这个页不能分配从[IOPHYSMEM,bootAllocPageNum)之间的内容不能分配，因为从[IOPHYSMEM0x0A0000,EXTPHYSMEM0x100000)之间的内存是IO HOLE，从EXTPHYSMEM0x100000开始的部分内容是存储内核启动程序和数据的，然后从bss段开始到bootAllocBaseAddr这段内存用来存放页目录和PageInfo了，也不能分配，其余内存都可以分配。然后用链表将闲置的物理页穿起来。

初始化完页表后进行用户空间和内核空间的一些内存映射。将pages(PageInfo物理页管理数组)这个物理页描述结构体数组映射到用户空间去，从UPAGES映射到pages的物理地址处。[UPAGES, UPAGES+PTSIZE)->[PADDR(pages), PADDR(pages)+PTSIZE)。然后映射内核栈，映射[KSTACKTOP-KSTKSIZE,KSTACKTOP)这段地址到[PADDR(bootstack), PADDR(bootstack)+KSTKSIZE)，实际上内核栈的内存是占据着PTSIZE这么大的，但是避免内核栈溢出，修改重要内容，留出[KSTACKTOP-PTSIZE, KSTACKTOP-KSTKSIZE)这段内存避免修改重要数据。最后将KERNBASE开始往后的内存都映射到低地址处，[KERNBASE, 2\^32)->[0,2\^32)。

完成页表设置后，就改变cr3页目录基地址寄存器，将内核页目录的物理地址放入其中。现在就正式从4MB小内存的页表正式升级为超大页表了，可喜可贺～。接下来的工作就是设置cr0的一些设置了。

至此完成了内存管理的基本初始化工作。

## lab3

在lab3中主要的任务就是完成基本的用户环境和异常处理。

env即JOS环境属于耦合了线程和地址空间的概念。线程主要由保存的寄存器env_tf定义，地址空间由页目录和env_pgdir所指向的页表定义，为运行一个环境，内核必须使用保存的寄存器值和相关的地址空间去设置 CPU。在 JOS 中，内核中任意时间只能有一个 JOS 环境处于活动中，因此，JOS 仅需要一个单个的内核栈。

首先我们要做的就是初始化用户环境env，这样我们就可以有NENV个可用的用户环境了。

初始化用户环境env要做的先是在mem_init部分初始化envs结构体数组所需的内存并映射到UENVS这个区域，然后再对结构体数组envs进行初始化。初始化主要就是遍历数组中的每一个结构体，将他们的状态设置为自由可用以及设置初始env_id为0并将它们按顺序连接到env_free_list链表上。

现在讲一下目前为止的用户环境的一个简单声明周期。

若要创建一个用户环境env，首先使用env_alloc，在其中会使用env_setup_vm，先初始化用户环境的虚拟内存环境，就是初始化一个用户页表，然后将内核页表复制到用户页表中，然后修改用户页表中的用户页表映射为刚初始化的用户页表(原本内核处对应位置设置的是内核页表为用户页表)。完成了用户环境虚拟内存环境初始化后就进行用户环境寄存器和基本设置的初始化，设置环境的id等变量，然后清空trapframe避免上一个环境的寄存器污染了现在的用户环境。然后设置用户环境的段寄存器。设置对应的用户数据段代码段和用户栈段，同时设置优先级rpl为用户级别。完成了基本的用户环境初始化然后就是使用load_icode将用户代码装入用户环境的内存中。这里根据ELF文件进行相关装载，在装载前需要将页表切换成用户页表，这样才能将程序真正加载到用户空间去，同时这段用户程序对于内核来说其实也是不可见的。装载完程序段后再切换回内核页表，设置用户环境的入口地址，然后分配一页内存给用户栈。至此完成了用户环境的创建。

若要运行用户环境则是进行环境变量相关设置后，切换至用户环境页表，然后使用env_pop_tf，即将用户环境的trapframe弹出值到对应寄存器中，然后使用iret跳转到用户环境中进行运行。

摧毁用户环境就是释放用户环境中用户空间的所有内存然后将环境放回空闲环境链表中。



一旦处理器进入用户模式，将无法返回，所以中断和异常是非常重的，这样可以帮助内核恢复对处理器的控制。可以说中断就是一个受保护的控制转移。

关于异常和中断的初始化，首先就是填写trapentry.S。设置各种异常中断的入口。其中分为有errorcode 和没有error code的情况，没有error code 的中断就使用0代替，保证栈上的结构统一。最后栈会变成这样。

在error code以及之前的压栈都不是操作系统完成的，因为`x86`的CPU硬件在遇到中断的时候，会进行自动化的处理。

如果是在ring 0，那么直接使用当前的ss/esp2. 
如果是在ring 3, 那么使用当前tss段里面的ss0/esp0。然后开始压栈

```
有error code				 无error code
 - - - - - - - -			 - - - - - - - -
| OLD ESP 		|			| OLD ESP 		| 
 - - - - - - - -			 - - - - - - - -
|		|OLD SS |			|		|OLD SS |
 - - - - - - - -			 - - - - - - - -
| OLD ELFAGS    |			| OLD ELFAGS    |
 - - - - - - - -			 - - - - - - - -
|		|OLD CS |			|		|OLD CS |
 - - - - - - - -			 - - - - - - - -
| OLD EIP 		|			| OLD EIP 		|
 - - - - - - - -			 - - - - - - - -
| ERROR CODE 	|			| 000000000		|
 - - - - - - - -			 - - - - - - - -
｜VECTOR NO		|			｜VECTOR NO		 |
 - - - - - - - -			 - - - - - - - -
|		|OLD DS |			|		|OLD DS |
 - - - - - - - -			 - - - - - - - -
|		|OLD ES |			|		|OLD ES |
 - - - - - - - -			 - - - - - - - -
|PUSHAL(eax-edi)|			|PUSHAL(eax-edi)|
 - - - - - - - -			 - - - - - - - -
| OLD ESP 		|			| OLD ESP 		| // 这里的esp是相当于trap函数调用传递的参数
 - - - - - - - -			 - - - - - - - -
```

完成相关环境压栈后，就会调用trap函数，操作系统会对中断进行分派处理，

再讲trap_init部分，这部分其实就是将trapentry.S设置的中断入口都填写到idt表中。并设置相关的权限。

在trap中断言已经关中断，然后如果中断是由用户程序产生的(判断cs权限)，然后将当前env的env_tf保存为现在的trapframe即保存现场。然后进行trap_dispatch，判断中断类型，如果是如系统调用之类的就让程序继续执行(这个时候会切换回用户段继续执行)，如果不是的话就摧毁用户环境，结束运行。如果继续运行的话可以调用env_run，其中会有env_pop_tf恢复现场继续运行。

中断的小总结

1. 发生中断或者trap，从ldtr里面找到ldt。
2. 根据中断号找到这一项，即ldt[中断号]
3. 根据ldt[中断号] == SETGATE(idt[T_MCHK], 0, GD_KT, T_MCHK_handler, 0);   取出当时设置的中断处理函数
4. 跳转到中断函数
5. 中断处理函数再跳转到trap函数。
6. trap函数再根据tf->trap_no中断号来决定分发给哪个函数。





现在有一个小问题就是内核本身是个大程序那他怎么把user的那些小程序放到内核程序中的呢。这个技巧主要在Kern/Makefrag里

首先是定义需要生成的`binary`的文件列表

```makefile
 Binary program images to embed within the kernel.
# Binary files for LAB3
KERN_BINFILES :=    user/hello \
            user/buggyhello \
            user/buggyhello2 \
            user/evilhello \
            user/testbss \
            user/divzero \
            user/breakpoint \
            user/softint \
            user/badsegment \
            user/faultread \
            user/faultreadkernel \
            user/faultwrite \
            user/faultwritekernel
KERN_OBJFILES := $(patsubst %.c, $(OBJDIR)/%.o, $(KERN_SRCFILES))
KERN_OBJFILES := $(patsubst %.S, $(OBJDIR)/%.o, $(KERN_OBJFILES))
KERN_OBJFILES := $(patsubst $(OBJDIR)/lib/%, $(OBJDIR)/kern/%, $(KERN_OBJFILES))
KERN_BINFILES := $(patsubst %, $(OBJDIR)/%, $(KERN_BINFILES))
```

通过如下这种方式把`binary`放到kernel中。

```makefile
# How to build the kernel itself
$(OBJDIR)/kern/kernel: $(KERN_OBJFILES) $(KERN_BINFILES) kern/kernel.ld \
      $(OBJDIR)/.vars.KERN_LDFLAGS
    @echo + ld $@
    $(V)$(LD) -o $@ $(KERN_LDFLAGS) $(KERN_OBJFILES) $(GCC_LIB) -b binary $(KERN_BINFILES)
    $(V)$(OBJDUMP) -S $@ > $@.asm
    $(V)$(NM) -n $@ > $@.sym
```

下面是链接命令。注意后面`-b`这个参数就是把后面的文件直接加载到kernel里面。由于我们现在没有文件系统，内核就把用户程序一股脑链接到自己身上，在以后有了文件系统就不需要了。但是它给了我们一个便利，我们现在可以直接在内存上运行它。

```makefile
echo @ld -o obj/kern/kernel \
    -m elf_i386 \
    -T kern/kernel.ld \
    -nostdlib \
        obj/kern/entry.o \
        obj/kern/entrypgdir.o \
        obj/kern/init.o \
        obj/kern/console.o \
        obj/kern/monitor.o \
        obj/kern/pmap.o \
        obj/kern/env.o \
        obj/kern/kclock.o \
        obj/kern/printf.o \
        obj/kern/trap.o \
        obj/kern/trapentry.o \
        obj/kern/syscall.o \
        obj/kern/kdebug.o  \
        obj/kern/printfmt.o  \
        obj/kern/readline.o  \
        obj/kern/string.o \
        /usr/lib/gcc/i686-linux-gnu/4.8/libgcc.a \
    -b binary  \
        obj/user/hello  \
        obj/user/buggyhello \
        obj/user/buggyhello2  \
        obj/user/evilhello  \
        obj/user/testbss  \
        obj/user/divzero  \
        obj/user/breakpoint \
        obj/user/softint \
        obj/user/badsegment \
        obj/user/faultread \
        obj/user/faultreadkernel \
        obj/user/faultwrite \
        obj/user/faultwritekernel
```

可执行程序现在是加载到kernel的镜像里面了。可是如果想运行的时候，又如何定位到这些程序呢？

这个时候如果去看`obj/kern/kernel.sym`，就会发现这里面定义了很多变量。`gcc`生成的`.sym`文件里面包含的就是编译器生成的变量表，左边是虚拟地址，右边就是对应的变量。

```makefile
f011b356 D _binary_obj_user_hello_start
f0122b88 D _binary_obj_user_buggyhello_start
f0122b88 D _binary_obj_user_hello_end
f012a3bf D _binary_obj_user_buggyhello2_start
f012a3bf D _binary_obj_user_buggyhello_end
f0131c11 D _binary_obj_user_buggyhello2_end
f0131c11 D _binary_obj_user_evilhello_start
f0139447 D _binary_obj_user_evilhello_end
f0139447 D _binary_obj_user_testbss_start
f0140c94 D _binary_obj_user_divzero_start
f0140c94 D _binary_obj_user_testbss_end
f01484dd D _binary_obj_user_breakpoint_start
f01484dd D _binary_obj_user_divzero_end
f014fd14 D _binary_obj_user_breakpoint_end
f014fd14 D _binary_obj_user_softint_start
f0157548 D _binary_obj_user_badsegment_start
f0157548 D _binary_obj_user_softint_end
f015ed7f D _binary_obj_user_badsegment_end
f015ed7f D _binary_obj_user_faultread_start
f01665b5 D _binary_obj_user_faultread_end
f01665b5 D _binary_obj_user_faultreadkernel_start
f016ddf1 D _binary_obj_user_faultreadkernel_end
f016ddf1 D _binary_obj_user_faultwrite_start
f0175628 D _binary_obj_user_faultwrite_end
f0175628 D _binary_obj_user_faultwritekernel_start
f017ce65 D _binary_obj_user_faultwritekernel_end
```



中断

![img](/Users/liyining/home/课件/龙芯杯/参考材料/操作系统/tmp/lab3.idt.jpeg)



> **问题**
> 在你的 `answers-lab3.txt` 中回答下列问题：

1. 为每个异常/中断设置一个独立的服务程序函数的目的是什么？（即：如果所有的异常/中断都传递给同一个服务程序，在我们的当前实现中能否提供这样的特性？）
2. 你需要做什么事情才能让 `user/softint` 程序正常运行？评级脚本预计将会产生一个一般保护故障（trap 13），但是 `softint` 的代码显示为 `int $14`。为什么它产生的中断向量是 13？如果内核允许 `softint` 的 `int $14` 指令去调用内核页故障的服务程序（它的中断向量是 14）会发生什么事情？ “`



1. 不同的中断需要的操作是不同的；特别是error code，有的压入，有的不压入，如果用同一个handler处理，需要两套Trapframe结构体，并且需要特判，增加了程序的复杂度。最最关键的，我们无法得知trapno，也就无法判断到底发生了啥中断，无法做分发。
2. 所有的gate中，除了系统调用门，其他的门，都只允许从特权级进入。在本程序试图进入14号特权门的时候，检查发现特权级不够，所以触发了一般保护错误。这样的设计是合理的，因为一旦允许用户自行触发缺页错误，操作系统将会很容易被攻击。

> **问题**

1. 在断点测试案例中，根据你在 IDT 中如何初始化断点条目的不同情况（即：你的从 `trap_init` 到 `SETGATE` 的调用），既有可能产生一个断点异常，也有可能产生一个一般保护故障。为什么？为了能够像上面的案例那样工作，你需要如何去设置它，什么样的不正确设置才会触发一个一般保护故障？
2. 你认为这些机制的意义是什么？尤其是要考虑 `user/softint` 测试程序的工作原理。



1. 就是设置中断门的时候，最后一个参数。如果为0，那么从用户态触发中断就会触发一般保护错误；如果为3，就能正常触发。

2. 这些措施都是为了保护操作系统内核，隔离用户代码与内核代码。



## lab4

在这个实验中主要要做的就是实现抢占式的多任务处理机制。

首先要做的就是要让操作系统支持多处理器。并为用户环境提供基本的环境管理方面的系统调用syscall。

我们想让JOS支持对称多处理器(symmetric multiprocessing，SMP)。在这种模型中所有CPU对系统资源都可以等效访问。虽然所有CPU在SMP中功能相同，但是根据引导启动担任的功能可以分为：引导处理器（BSP）：负责初始化系统和引导操作系统，和应用程序处理器（AP）：只有在操作系统启动并运行后，BSP才会激活应用程序处理器。

具体哪个处理器是BSP是由硬件和BIOS系统决定的。到目前为止，我们完成的JOS code都在BSP上运行。哪一个CPU是BSP由硬件和BISO决定，到目前位置所有JOS代码都运行在BSP上。

在SMP系统中，每个CPU都有一个对应的local APIC（LAPIC），负责传递中断。CPU通过内存映射IO(MMIO)访问它对应的APIC，这样就能通过访问内存达到访问设备寄存器的目的。LAPIC从物理地址0xFE000000开始，JOS将通过MMIOBASE(KSTACKTOP-2PTSIZE)虚拟地址访问该物理地址。

```
 *                     |~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~| RW/--
 *                     |                              | RW/--
 *                     |   Remapped Physical Memory   | RW/--
 *                     |                              | RW/--
 *    KERNBASE, ---->  +------------------------------+ 0xf0000000      --+
 *    KSTACKTOP        |     CPU0's Kernel Stack      | RW/--  KSTKSIZE   |
 *                     | - - - - - - - - - - - - - - -|                   |
 *                     |      Invalid Memory (*)      | --/--  KSTKGAP    |
 *                     +------------------------------+                   |
 *                     |     CPU1's Kernel Stack      | RW/--  KSTKSIZE   |
 *                     | - - - - - - - - - - - - - - -|                 PTSIZE
 *                     |      Invalid Memory (*)      | --/--  KSTKGAP    |
 *                     +------------------------------+                   |
 *                     :              .               :                   |
 *                     :              .               :                   |
 *    MMIOLIM ------>  +------------------------------+ 0xefc00000      --+
 *                     |       Memory-mapped I/O      | RW/--  PTSIZE
 * ULIM, MMIOBASE -->  +------------------------------+ 0xef800000
 *                     |  Cur. Page Table (User R-)   | R-/R-  PTSIZE
 *    UVPT      ---->  +------------------------------+ 0xef400000
```

那么首先多处理器是怎么初始化的呢。其实要做的就是先用MMIOBASE这块内存的内存映射IO来访问对应处理器的APIC获取对应设备的相关信息。所以预备工作就是为mmio编写一块专门为内存的IO映射函数，然后从MMIOBASE开始分配内存，并将分配的内存映射(从一个MMIOBASE增长的虚拟地址)到指定的物理地址处。这个分配用于内存IO的函数将会用在local APIC的初始化中，lapic会在内存中对应的物理地址中有映射，改变这块内存就相当于直接与lapci交互了，然后用这个内存分配的函数来做虚拟内存到这块物理内存的映射。

做了准备工作之后，就开始多处理器的初始化了，mp_init中还会在启动AP之前，BSP收集有关多处理器系统的信息，这个时候就要调用mp_init，这个函数将会读取内存中位于BIOS区域里的MP配置表来获得一系列的信息，其中就包括了LAPIC的MMIO地址，CPU总数，它们的APIC ID。

做好了处理器信息收集之后就会调用lapic_init，进行lapic的相关初始化，在这个函数中会进行相关APIC的初始化，包括初始化计时器中断，和一些硬件中断。包括分配lapic内存，映射这个lapic物理地址，让我们可以用虚拟地址来访问。

在完成了这些基本的多处理器初始化之后就可以使用pic_init，初始化8259A中断控制器，将0~15的IRQ编号映射到IDT表项 ，[IRQ_OFFSET ~ IRQ_OFFSET +15]。IRQ_OFFSET的选择主要是为了设备中断不与处理器异常重叠。

完成这些之后就BSP就要准备开启其他AP处理器了，在此之前需要使用大内存锁来加锁，避免到时候env调度的时候出现错误。

然后就可以调用boot_aps进行驱动AP的引导过程了。在这个函数中，将会把mpentry.S这个存在内核代码段中的代码复制到可在实模式下寻址的内存位置(MPENTRY_PADDR 0x7000)处。然后就会开始对于每一个ap处理器逐一调用lapic_startap进行启动，等到一个处理器启动完成之后在进行下一个ap处理器的启动。在lapic_startap启动函数中将会通过发送STARTUP的IPI(处理器间中断)信号到AP的LAPIC单元来一个个地激活AP，然后执行mpentry.S的代码。(在boot_ap中会设置mpentry_kstack，设置该CPU对应的内核栈，然后在mpentry.S中设置esp)

在mpentry.S中，会和boot.S中类似，经过一些简单配置后，会使AP进入分页机制的保护模式，然后调用C语言函数mp_main(在这个函数中会设置页表，调用lapic_init,env_init_percpu,trap_init_percpu然后设置cpu_status为启动，然后等待获取锁之后进行env调度)。然后boot_aps等待AP在其结构CpuInfo的cpu_status字段中发出CPU_STARTED标志信号，然后再唤醒下一个。

在这个时候BSP基本就完成了AP的启动和多处理环境的初始化了，然后就会调用env调度，因为此时BSP已经获取大内存锁所以只会有BSP一个处理器进行env的调度。然后BSP就会采取简单的轮回机制，在所有env中选择从当前env中的下一个env开始，找到一个处于可运行状态的env来运行这个env，如果找不到准备运行的env，如果当前的env正在这个处理器上运行，就继续运行这个env。然后调用env_run的时候就会将curenv状态改为可运行，然后切换至新的env中运行，同时在切换用户环境之前释放大内存锁，然后其他的处理器就可以获取大内存锁进行env调度了。

至此完成了jos的初始化。

现在再说一些关于jos多处理器的环境相关问题。

关于jos的多处理器机制，有一个比较重要的结构体

```c
// CpuInfo它保存了每个 CPU 的变量
// Per-CPU state
struct CpuInfo {
	uint8_t cpu_id;                 // Local APIC ID; index into cpus[] below
	volatile unsigned cpu_status;   // The status of the CPU
	struct Env *cpu_env;            // The currently-running environment.
	struct Taskstate cpu_ts;        // Used by x86 to find stack for interrupt
};
```

在这个CpuInfo中将会保存每一个cpu的lapic ID，以及CPU的状态，CPU当前运行的env以及Taskstate保存的一堆CPU私有的寄存器，以及内核态esp以及用户态esp还有内核态及用户态的代码段，这个用于中断的内核和用户态的切换。

为了避免处理器相互干扰，每个CPU都会在内存中有各自的内核栈。



在完成了多处理器的支持之后，介绍一下fork的写时复制

实现fork()有多种方式，一种是将父进程的内容全部拷贝一次，这样的话父进程和子进程就能做到进程隔离，但是这种方式的缺点在于耗时，且不一定有用。因为fork()函数后面大概率会紧跟在子进程中调用exec()函数，替代原来子进程的内存空间为新的程序。所以如果fork时即复制，那么有很大概率白费功夫，因为在fork() 和 exec() 之前需要用mem的情况很少。

另一种方式就是**Copy-on-Write Fork**，父进程将自己的页目录和页表复制给子进程，并同时将shared-pages 改为 read-only。这样父进程和子进程就能访问相同的内容。只有当一方执行写操作时，发生 `page fault`，然后生成新的可写的page进行复制这一页。这样既能做到地址空间隔离，又能节省了大量的拷贝工作——很可能fork()后紧跟exec()的进程只需要copy 1 页(current page of the stack)。

Copy On Write的原理是：fork()之后，kernel把父进程中所有的内存页的权限都设为read-only，然后子进程的地址空间指向父进程。当父子进程都只读内存时，相安无事。当其中某个进程写内存时，CPU硬件检测到内存页是read-only的，于是触发页异常中断（page-fault），陷入kernel的一个中断例程。中断例程中，kernel就会**把触发的异常的页复制一份**，于是父子进程各自持有独立的一份。

所以想要实现写时复制的fork就要实现用户级别的缺页中断处理函数。

除此之外还要修改env结构体，为每个env添加一个属性就是用来处理自己的缺页错误的处理函数，env_pgfault_upcall。

当缺页中断发生时，内核会返回用户模式来处理该中断。我们需要一个用户异常栈，来模拟内核异常栈。JOS的用户异常栈被定义在虚拟地址UXSTACKTOP。

到目前为止出现了三个栈：

```
　　[KSTACKTOP-KSTKSIZE,  KSTACKTOP) 
　　内核态系统栈

　　[UXSTACKTOP - PGSIZE, UXSTACKTOP )
　　用户态错误处理栈

　　[UTEXT, USTACKTOP)
　　用户态运行栈
```

设置完这些支撑的属性了，现在就要真正的完成page_fault_handler缺页处理函数了。在缺页中断的时候会进入内核的trap然后派发给page_fault_handler来处理缺页中断。

在这个函数中要做到几个事情，判断当前env是否设置了用户缺页处理函数env_pgfault_upcall如果没有设置就没办法修复缺页问题，直接销毁进程。然后就是修改esp切换到用户异常栈，在栈上压入一个UTrapframe结构，将eip设置为当前env的缺页处理函数env_pgfault_upcall的入口然后开始执行代码。

在pgfault_upcall中会调用设置好的_pgfault_handler来进行真正的缺页处理，处理完成后使用UTrapframe进行环境恢复，跳转到出错前返回地址。

简单总结一下缺页处理的整个流程。

先铺垫一下，在fork的时候，lib/fork.C的fork已经使用set_pgfault_handler设置好了\_pgfault_upcall和\_pgfault_handler两个函数，这才让这个进程有缺页处理函数。

首先就是在程序中fork的时候，将父子进程中共享的内存也都设置为read-only了，然后父进程或者子进程想要修改内存的时候，就会触发缺页异常，就是写只读页异常。这个时候程序就会进入异常进入trap函数，trap将异常派发给page_fault_handler进行统一的缺页异常处理，在page_fault_handler中确认了这个异常是由用户程序发出的之后就判断该进程是否有缺页异常处理函数env_pgfault_upcall，如果没有就直接摧毁进程了，若有就继续进行缺页处理。首先将UTrapframe压入异常处理栈中，然后将eip设置为进程的缺页处理函数入口env_pgfault_upcall处，然后将esp设置为异常处理栈的相应位置(此处已经处理了嵌套缺页异常了)。然后调用env_run就从内核态切换到用户态进行缺页异常的处理了。这个时候会进入pfentry.S中进行真正的解决缺页问题，首先调用_pgfault_handler处理缺页问题，然后就是使用栈中的UTrapframe进行环境恢复了，比如填入返回地址eip和切换esp等然后使用ret进行返回，就解决完缺页问题继续执行用户程序了。

现在讲讲fork的流程。

fork首先使用set_pgfault_handler设置两个缺页处理函数\_pgfault_handler和env_pgfault_upcall。然后使用sys_exofork创建一个子进程，复制当前用户环境的寄存器状态，但是UTOP下的目录还没有建立，还不能运行，然后就要将父进程的页表和页目录都拷贝到子进程，对于可写的页，将对应的PTE的PTE_COW位设置为1并取消PTE_W设置为只读。最后将子进程状态设置为ENV_RUNNABLE。





讲完fork再将外部中断来调度进程。

目前程序一旦进入用户模式，除非发生中断，否则CPU永远不会再执行内核代码。为了避免CPU资源被恶意抢占，需要开启时钟中断，强迫进入内核，然后内核就可以切换另一个进程执行。

外部中断（即，设备中断）被称为IRQs。有16个可能的IRQ，编号为0到15。从IRQ编号到IDT条目的映射不是固定的。picirq.c中的pic_init通过IRQ_OFFSET+15将IRQ 0-15映射到IDT条目IRQ_OFFSET。

External interrupts are controlled by the FL_IF flag bit of the %eflags register (see inc/mmu.h).外部中断由`%eflags`的FL_IF flag位控制。**设置了该位，外部中断启用。** bootloader的第一条指令是屏蔽外部中断，不签位置还没有开启中断。
所以要做的事首先就是在trap.C中初始化IDT条目。

值得一提的就是IDT表象每一项都初始化为中断门，这样在发生任何中断/异常的时候，陷入内核态的时候，CPU都会将%eflags寄存器上的FL_IF标志位清0，关闭中断；切换回用户态的时候，CPU将内核栈中保存的%eflags寄存器弹回%eflags寄存器，恢复原来的状态。否则就无法关中断。

始终中断产生的调度主要就是靠外部时钟中断，然后进入trap中派发，判断异常码是IRO_OFFSET + IRO_TIMER就调用lapic_eoi();回应中断并使用sched_yield();进行调度。

现在再来讲最后一部分的进程间通信。

JOS中进程间通信的“消息”包含两部分：

1. 一个32位的值。
2. 可选的页映射关系。

sys_ipc_recv()和sys_ipc_try_send()是这么协作的：

1. 当某个进程调用sys_ipc_recv()后，该进程会阻塞（状态被置为ENV_NOT_RUNNABLE），直到另一个进程向它发送“消息”。当进程调用sys_ipc_recv()传入dstva参数时，表明当前进程准备接收页映射。
2. 进程可以调用sys_ipc_try_send()向指定的进程发送“消息”，如果目标进程已经调用了sys_ipc_recv()，那么就发送数据，然后返回0，否则返回-E_IPC_NOT_RECV，表示目标进程不希望接受数据。当传入srcva参数时，表明发送进程希望和接收进程共享srcva对应的物理页。如果发送成功了发送进程的srcva和接收进程的dstva将指向相同的物理页。

> **问题 2**、看上去使用一个大内核锁，可以保证在一个时间中只有一个 CPU 能够运行内核代码。为什么每个 CPU 仍然需要单独的内核栈？描述一下使用一个共享内核栈出现错误的场景，即便是在它使用了大内核锁保护的情况下。

在处理异常的时候，在trapentry.S中的_alltraps，就是没有上内存锁，这个时候还会改变内核栈的值，如果不加大内存锁的话如果有其他cpu进入内核态就会破坏栈中的上下文信息。

并且如果在内核栈中留下了不同CPU要使用的数据也会产生混乱

> **问题 3**、在你实现的 `env_run()` 中，你应该会调用 `lcr3()`。在调用 `lcr3()` 的之前和之后，你的代码引用（至少它应该会）变量 `e`，它是 `env_run` 的参数。在加载 `%cr3` 寄存器时，MMU 使用的地址上下文将马上被改变。但一个虚拟地址（即 `e`）相对一个给定的地址上下文是有意义的 —— 地址上下文指定了物理地址到那个虚拟地址的映射。为什么指针 `e` 在地址切换之前和之后被解除引用？

因为现在运行在内核态，对于内核态的一些页表都有访问权限，且每一个env的页表中也映射了所有内核态的页表，每个进程页表中虚拟地址高于UTOP之上的地方，只有UVPT不一样，其余的都是一样的，只不过在用户态下是看不到的。所以虽然这个时候的页表换成了下一个要运行的进程的页表，但是映射也没变，还是依然有效的。

> **问题 4**、无论何时，内核从一个环境切换到另一个环境，它必须要确保旧环境的寄存器内容已经被保存，以便于它们稍后能够正确地还原。为什么？这种事件发生在什么地方？

在trap中触发异常的时候会在trap函数中处理异常前将trapframe保存到curenv->env_tf中。

用户进程之间的切换，会调用系统调用sched_yield()；用户态陷入到内核态，可以通过中断、异常、系统调用；这样的切换之处都是要在系统栈上建立用户态的TrapFrame，在进入trap()函数后，语句curenv->env_tf = *tf;将内核栈上需要保存的寄存器的状态实际保存在用户环境的env_tf域中。

疑问：内核的环境状态在哪里保存。

靠用户态切换到内核态的转换来说明内核态的环境如何加载的。

当进程调用了int syscall系统调用的中断之后，就会int指令会进行一系列的操作。

（1） 由于INT指令发生了不同优先级之间的控制转移，所以首先从TSS（任务状态段）中获取高优先级的核心堆栈信息（SS和ESP）；  (硬件完成)
 （2） 把低优先级堆栈信息（SS和ESP）保留到高优先级堆栈（即核心栈）中；(硬件完成)
 （3） 把EFLAGS，外层CS，EIP推入高优先级堆栈（核心栈）中。  (硬件完成)
 （4） 通过IDT加载CS，EIP（控制转移至中断处理函数）  (硬件完成，相当于跳转到了xxx_HANDLER然后再跳转到_alltraps最后跳转到trap()中)

在int指令中就已经完成了用户态到内核态的数据段切换和eip切换以及内核栈切换等。主要还是靠着TSS任务状态段来完成(这个TSS在每个cpu的初始化中有设置，然后还装载了TSS段描述符到TSS的段寄存器中)

内核态的环境直接靠trapentry.S跳转，在_alltraps中会保存用户态的trapframe，然后将GD_KD内核态的段选择符放入ds和es中。

进入内核态的时候不需要改大部分的eax之类的通用寄存器，因为eax之类的这些普通寄存器要用来传递系统调用函数的参数。

## lab5

在lab5中要做的主要就是完成一个文件系统一次管理磁盘空间，然后要实现spawn，这样就可以加载和运行磁盘上的文件，以使操作系统在控制台上运行一个shell。

文件系统的实现主要包括四个部分的工作。

1. 引入一个**文件系统进程（FS进程）**的特殊进程，该进程提供文件操作的接口。
2. **建立RPC机制**，客户端进程向FS进程发送请求，FS进程真正执行文件操作，并将数据返回给客户端进程。
3. 更高级的抽象，引入**文件描述符**。通过文件描述符这一层抽象就可以将**控制台，pipe，普通文件**，统统按照文件来对待。（文件描述符和pipe实现原理）
4. 支持从磁盘**加载程序**并运行。

我们将要实现的文件系统会比真正的文件系统要简单，但是能满足基本的创建，读，写，删除文件的功能。但是不支持链接，符号链接，时间戳等特性。

在jos中的文件系统设计比较简单，根本不用节点，简单的将文件的所有元数据保存在描述那个文件的目录条目中，就是File数据结构又可以包含元数据也包含着数据的指针。

在jos文件系统中，逻辑上的目录和文件都是由数据块block组成，可以零散的分散在磁盘上，都是靠File之间的链接来寻找。有点类似于页表。

首先介绍jos操作系统对于磁盘的理解，在磁盘上，磁盘将自己的读写数据粒度分割成一个个扇区sector，每个扇区都是512字节。而操作系统上是在扇区基础上再进行抽象，按照块block来理解磁盘，进行相应的磁盘使用和分配。比如jos中理解的块block就是一页大小4KB相当于4KB/512B 个扇区。在操作系统层面将扇区抽象成了块，所以所有的操作都可以通过bc中的函数封装。比如flush_block中使用ide_write就是对扇区抽象成块的操作。

```c
ide_write(blockno * (BLKSIZE / SECTSIZE), addr, (BLKSIZE / SECTSIZE)))
```

上面就是写地址从块转换成扇区，写数据的多少也从块转换成了扇区，即(BLKSIZE / SECTSIZE)。

现在说说操作系统对磁盘中block的一些基础全局设定。

![img](https://pic3.zhimg.com/80/v2-db4ccda2ceaa00eedf50612078487602_720w.png)

Block0号块一般是保留的，用于去保存引导加载程序和分区表，因此文件系统一般不会去使用磁盘上比较靠前的块。block1是用来存放Superblock的，superblock中包括一些基础配置和根文件File。随后是若干个block来存储Bitmap用来确认哪个block已经使用过了

再介绍jos文件系统的File数据结构

<img src="https://blog-1253119293.cos.ap-beijing.myqcloud.com/6.828/lab5/lab5_2_file%E7%BB%93%E6%9E%84.PNG" alt="File文件结构" style="zoom: 50%;" />

在File中有文件的名称、大小、类型，以及f_direct和f_indirect，direct是可以直接获取到blockno的，f_direct[i]代表的就是文件第i个blockno，f_indirect代表非直接可获取的文件数据块，f_indirect存放的是一个blockno指向一个数据块，这个数据块中的每一个unit32_t都代表着这个文件的一个blockno。



完成了fs中基本的遍历File的函数之后，fs中就可以调用这些函数提供操作文件的各种功能了。但是其他用户进程不能直接调用这些函数。我们通过进程间函数调用(RPC)对其它进程提供文件系统服务。RPC机制原理如下：

```
      Regular env           FS env
   +---------------+   +---------------+
   |      read     |   |   file_read   |
   |   (lib/fd.c)  |   |   (fs/fs.c)   |
...|.......|.......|...|.......^.......|...............
   |       v       |   |       |       | RPC mechanism
   |  devfile_read |   |  serve_read   |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |     fsipc     |   |     serve     |
   |  (lib/file.c) |   |  (fs/serv.c)  |
   |       |       |   |       ^       |
   |       v       |   |       |       |
   |   ipc_send    |   |   ipc_recv    |
   |       |       |   |       ^       |
   +-------|-------+   +-------|-------+
           |                   |
           +-------------------+
```

本质上RPC还是借助IPC机制实现的，普通进程通过IPC向FS进程间发送具体操作和操作数据，然后FS进程执行文件操作，最后又将结果通过IPC返回给普通进程。



在这里，其实是操作系统运行了一个进程fs相当于一个服务器可以对其他进程提供操作系统服务。

在fs进程中，首先会使用serve_init初始化打开的文件描述符表(记录了openFileId，打开的文件File，以及打开文件的模式，打开文件页记录打开的设备，偏移量和打开模式等)。相关数据结构关系如下所示

![文件系统相关数据结构关系](https://blog-1253119293.cos.ap-beijing.myqcloud.com/6.828/lab5/lab5_4_%E6%96%87%E4%BB%B6%E7%B3%BB%E7%BB%9F%E6%95%B0%E6%8D%AE%E7%BB%93%E6%9E%84.png)

然后使用fs_init初始化文件系统，包括设置使用的磁盘，初始化磁盘块的缺页处理函数设置superblock和bitmap。

然后就使用serve开始运行文件系统的服务，这里就是一个死循环，一直接受进程间通讯请求，然后处理请求并将结果通过ipc发送给请求的进程

对于客户端来说：发送一个32位的值作为请求类型，发送一个Fsipc结构作为请求参数，该数据结构通过IPC的页共享发给FS进程，在FS进程可以通过访问fsreq(0x0ffff000)来访问客户进程发来的Fsipc结构。
对于服务端来说：FS进程返回一个32位的值作为返回码，对于FSREQ_READ和FSREQ_STAT这两种请求类型，还额外通过IPC返回一些数据。

客户端要使用文件系统的接口就可以通过lib/file.c中的函数来调用相关的接口，这里边的函数就会使用ipc进行进程间通信调用文件系统的接口。

![lab5_7_open原理.png](https://blog-1253119293.cos.ap-beijing.myqcloud.com/6.828/lab5/lab5_7_open()%E5%8E%9F%E7%90%86.png)

每个进程从虚拟地址0xD0000000开始，每一页对应一个Fd结构，也就是说文件描述符0对应的Fd结构地址为0xD0000000，文件描述符1对应的Fd描述符结构地址为0xD0000000+PGSIZE（被定义为4096），以此类推，。可以通过检查某个Fd结构的虚拟地址是否已经分配，来判断这个文件描述符是否被分配。如果一个文件描述符被分配了，那么该文件描述符对应的Fd结构开始的一页将被映射到和FS进程相同的物理地址处。

unix文件描述符是一个综合概念，包括文件、管道、console I/O等设备类型。在jos中，每一个设备类型都有相关联的struct Dev，以及读写相关的函数指针。每一个struct Fd指明了它的设备类型，lib/fd.c的大多数函数只是负责分发处理函数到对应的struct Dev。

lib/fd.c同时负责维护每个应用进程的文件描述符表区域，该区域从FDTABLE开始，每个文件描述符对应一个页，一个进程一次最多可以打开32个文件描述符。当且仅当文件描述符被使用时，其相关的文件描述符表页才被映射。每个文件描述符还有一个可选的数据页，从FILEDATA地址开始。

spawn是创建一个新的进程，就是从文件系统加载用户程序然后启动进程来运行这个程序，类似于fork然后立刻exec。spawn主要做的是几个过程，首先从文件系统打开prog程序，然后调用sys_exofork老创建新的Env，初始化栈，将启动函数的调用参数都压入栈中然后根据ELF将程序读入内存中，复制共享页，用sys_env_set_trapframe设置新的寄存器环境，最后用sys_env_set_status设置进程可运行。



现在简单说一下文件系统自底向上的构造。首先最底层的就是文件系统的PIO驱动，在ide.c中，是以硬件角度来看待磁盘的，在这里磁盘的基本单位是sector扇区而不是块。然后往上抽象一层就是操作系统对硬件磁盘进行抽象理解即bc.c中，将磁盘以块为单位进行管理，即操作系统将磁盘的基本单位理解成块，其中块可以由几个扇区组成，在jos中一个块的大小和页的大小相同都是4kb，所以一个块由4kb/512B个扇区组成。再往上抽象一层就是fs.c，在这里就会对块进行管理和操作，并且将块和struct File结合起来，这里就会将整个磁盘理解成很多个块，其中块1是superblock，然后就跟着bitmap管理磁盘上所有块的使用情况，同时fs.c还提供了有关于block和File的相关操作，比如在File中寻找相关的block，遍历目录FIle，在File下申请一个File等操作。这属于fs.c中的上半部分，提供的是block与File紧密关联的比较基础的块操作，fs.c的下半部分就是提供文件操作了，比如创建文件，打开文件，读写文件等。

上一段已经从硬件层面到块的文件层面进行了很好的文件系统抽象了，接下来就是要将这些功能函数构造成结构给其他进程提供文件服务，就是文件系统服务器serv.c。在这一层服务器对文件系统的服务进行封装并通过进程间通信对外暴露接口来提供服务。整体而言还是使用fs.c中的功能，只是进行了一些封装。

从服务器层面理解：

在serv.c这一层还使用了OpenFile对所有文件描述符以及对应的File进行链接管理，简而言之就是文件服务器管理着一个OpenFile数组，代表着所有进程的文件描述符以及对应的File，并且OpenFile中包含着File和Fd文件描述符，就是对二者进行统一管理。每次进程想要打开一个文件都需要从OpenFile数组中挑选一个空闲的OpenFile作为这个进程的打开文件管理(即这个OpenFile中的文件描述符fd没有分配对应的物理页)，然后就会在这个OpenFile中初始化文件描述符fd并将打开的文件File存储到OpenFile的file中，最后会将文件描述符对应的物理页通过进程间通讯返回给请求进程。同时还有一点就是，用户进程只会保存文件描述符fd，OpenFIle是文件系统服务器用来管理用的，所以fd中还会保存着对应的OpenFileId即Fd中的FdFile的fd_file的id，这样就可以让服务器在以后的请求中知道请求的是哪个OpenFile了。

文件描述符相当于对File的一个抽象，让用户程序不再需要知道他们打开的是哪个File，只要知道打开了一个文件之后这个文件代表的文件描述符是fd即可，剩下的找File都可以通过文件描述靠文件系统服务器完成，这样也相当于保护了操作系统的文件系统基础构造。同时文件描述符也是对操作系统所有类型的文件的一种抽象表达，一种文件描述符就可能是文件设备，命令行设备或者管道管道。这些都可以用文件描述符来表示，并且可以通过文件描述符中存储的devId找到对应设备的统一操作接口进行操作。比如设备的打开就是一个函数指针，不同的设备有不同的处理方式。

注：OpenFile数组的保存地址在文件系统env的虚拟地址FILEVA=0xD0000000处，最多保存MAXOPEN=1024个打开的文件。

从用户角度来理解：

从用户角度来理解，打开一个文件就是获取一个文件描述符，这个文件描述代表着这个文件，同时文件描述打开一个文件本质上来讲应该是打开一个设备，这个设备可能是文件设备或者控制台设备或者管道设备。

对于用户来说，对文件描述符的操作也相当于对文件描述代表的设备提供的接口进行操作。

在lib/fd.c中就是提供了很多关于文件描述操作的函数基础的操作有在用户空间中对应位置分配一个空闲文件描述符，关闭文件描述符，找到对应文件描述符等。

注：在用户空间中文件描述符保数组存在FDTABLE=0xD0000000处，每个文件描述符占一页内存，一个进程可以打开的最多文件描述符数量为MAXFD=32，同时每一个文件描述符在用户空间中都有可选的文件描述符数据页，数据页数组在内存中的FDTABLE + MAXFD*PGSIZE位置。

同时在fd.c中还有关于文件描述对应设备的一些操作，fd.c中保存着文件描述可以代表的三种设备devfile，devpipe，devcons。以及相关的抽象操作，根据设备id查找设备。还有关闭文件描述符，复制一份文件描述符dup，这个dup利用的是虚拟内存的小技巧，复制的新的文件描述符和旧的文件描述符本质上是一体的二者的虚拟内存都映射到同一个物理页上，所以一个文件描述符的操作也会影响到另一个。还有的操作就是read和write，由于从设备上不能一次性读取大量字节，所以有readn，通过多次读取来读取到指定的字节数。等等。

再来看设备这个东西

```c
// Per-device-class file descriptor operations
struct Dev {
	int dev_id;
	const char *dev_name;
	ssize_t (*dev_read)(struct Fd *fd, void *buf, size_t len);
	ssize_t (*dev_write)(struct Fd *fd, const void *buf, size_t len);
	int (*dev_close)(struct Fd *fd);
	int (*dev_stat)(struct Fd *fd, struct Stat *stat);
	int (*dev_trunc)(struct Fd *fd, off_t length);
};
```

设备这个结构体可以说是一个设备class，不同的设备可以通过定义函数指针来完成不同的操作。

在lib中file.c, console.c pipe.c分别对应着三种设备的实现。

file就是通过进程间通信调用文件系统服务器进行文件相关操作。

console就是使用sys_cgetc和sys_cputs等来操作控制台。

pipe管道则是创建两个文件描述符，并给第一个文件描述符对应的数据页分配物理页，然后第二个文件描述符的数据页映射到第一个文件描述符数据页的物理页上，然后设置一个文件描述符是只读的一个是只写的，然后将文件描述符的编号赋值给pipe(int pfd[2])中的pfd。而两个文件描述符对应的数据页就是存储着数据结构struct Pipe。pfd[0]代表的是只读的，pfd[1]代表的是只写的