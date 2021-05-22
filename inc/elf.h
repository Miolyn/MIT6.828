#ifndef JOS_INC_ELF_H
#define JOS_INC_ELF_H
#include <inc/types.h>
#define ELF_MAGIC 0x464C457FU /* "\x7FELF" in little endian */

struct Elf {
    uint32_t e_magic;  // must equal ELF_MAGIC // 标识文件是否是 ELF 文件
    uint8_t e_elf[12]; // 魔数和相关信息
    uint16_t e_type; // 文件类型
    uint16_t e_machine; // 针对体系结构
    uint32_t e_version; // 版本信息
    uint32_t e_entry; // Entry point 程序入口点，是虚拟的链接地址
    uint32_t e_phoff; // 程序头表偏移量，是程序头表的第一项相对于 ELF 文件的开始位置的偏移
    uint32_t e_shoff; // 节头表偏移量
    uint32_t e_flags; // 处理器特定标志
    uint16_t e_ehsize; // 文件头长度
    uint16_t e_phentsize; // 程序头部长度
    uint16_t e_phnum; // 程序头部表项个数
    uint16_t e_shentsize; // 节头部长度
    uint16_t e_shnum; // 节头部个数
    uint16_t e_shstrndx; // 节头部字符索引
};
// 程序头表项
struct Proghdr {
    uint32_t p_type; // 段类型
    uint32_t p_offset;  // 段位置相对于 ELF 文件开始处的偏移量，标识出了磁盘位置(相对的位置，这里没有包括 ELF 文件之前的磁盘)
    uint32_t p_va; // 段放置在内存中的地址(虚拟的链接地址)
    uint32_t p_pa; // 段的物理地址
    uint32_t p_filesz; // 段在文件中的长度
    uint32_t p_memsz; // 段在内存中的长度
    uint32_t p_flags; // 段标志
    uint32_t p_align; // 段在内存中的对齐标志
};
// 节头表
struct Secthdr {
    uint32_t sh_name; // 节名称
    uint32_t sh_type; // 节类型
    uint32_t sh_flags; // 节标志
    uint32_t sh_addr; // 节在内存中的线性地址
    uint32_t sh_offset; // 相对于文件首部的偏移
    uint32_t sh_size; // 节大小(字节数)
    uint32_t sh_link; // 与其它节的关系
    uint32_t sh_info; // 其它信息
    uint32_t sh_addralign; // 字节对齐标志
    uint32_t sh_entsize; // 表项大小
};

// Values for Proghdr::p_type
#define ELF_PROG_LOAD 1

// Flag bits for Proghdr::p_flags
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4

// Values for Secthdr::sh_type
#define ELF_SHT_NULL 0
#define ELF_SHT_PROGBITS 1
#define ELF_SHT_SYMTAB 2
#define ELF_SHT_STRTAB 3

// Values for Secthdr::sh_name
#define ELF_SHN_UNDEF 0

#endif /* !JOS_INC_ELF_H */
