#include <inc/elf.h>
#include <inc/x86.h>
/**********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(boot.S and main.c) is the bootloader.  It should
 *    be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in boot.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 **********************************************************************/

#define SECTSIZE 512
#define ELFHDR ((struct Elf*)0x10000)  // scratch space

void readsect(void*, uint32_t);
void readseg(uint32_t, uint32_t, uint32_t);

void bootmain(void) {
    struct Proghdr *ph, *eph;

    // read 1st page off disk
    // 相当于从磁盘位置0开始读了SECTSIZE*8个字节到物理内存ELFHDR处，读取了4KB一页内容
    // 读取4kb内容加载到内存的0x10000处(内核的加载地址)
    readseg((uint32_t)ELFHDR, SECTSIZE * 8, 0);

    // is this a valid ELF?
    // 判断是否是合法的ELF文件
    if (ELFHDR->e_magic != ELF_MAGIC)
        goto bad;

    // load each program segment (ignores ph flags)
    // e_phoff、e_phentsize、e_phnum：描述Program Header
    // Table的偏移、大小、结构。
    ph = (struct Proghdr*)((uint8_t*)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph++)
        // p_pa is the load address of this segment (as well
        // as the physical address)
        // ph->p_pa物理地址寻址相关的地址
        // ph->p_memsz该字段给出了内存镜像中该段的大小，可能为0。
        // ph->p_offset该字段给出了从文件开始到该段开头的第一个字节的偏移。
        readseg(ph->p_pa, ph->p_memsz, ph->p_offset);

    // call the entry point from the ELF header
    // note: does not return!
    // ELFHDR->e_entry这一项为系统转交控制权给 ELF 中相应代码的虚拟地址。
    // 执行了entry函数，启动了内核开始的代码，然后就进入entry.S的代码了
    ((void (*)(void))(ELFHDR->e_entry))();

bad:
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);
    while (1)
        /* do nothing */;
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked
// 第一个参数是物理地址，表示读取磁盘的内容放在物理内存地址pa处，
// 第二个是读取的字节数，第三个是从磁盘的第几个字节开始读取。
void readseg(uint32_t pa, uint32_t count, uint32_t offset) {
    uint32_t end_pa;

    end_pa = pa + count;

    // round down to sector boundary
    pa &= ~(SECTSIZE - 1);

    // translate from bytes to sectors, and kernel starts at sector 1
    offset = (offset / SECTSIZE) + 1;

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    while (pa < end_pa) {
        // Since we haven't enabled paging yet and we're using
        // an identity segment mapping (see boot.S), we can
        // use physical addresses directly.  This won't be the
        // case once JOS enables the MMU.
        readsect((uint8_t*)pa, offset);
        pa += SECTSIZE;
        offset++;
    }
}

void waitdisk(void) {
    // wait for disk reaady
    while ((inb(0x1F7) & 0xC0) != 0x40)
        /* do nothing */;
}

void readsect(void* dst, uint32_t offset) {
    // wait for disk to be ready
    waitdisk();

    outb(0x1F2, 1);  // count = 1
    outb(0x1F3, offset);
    outb(0x1F4, offset >> 8);
    outb(0x1F5, offset >> 16);
    outb(0x1F6, (offset >> 24) | 0xE0);
    outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector
    insl(0x1F0, dst, SECTSIZE / 4);
}
