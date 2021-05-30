#include <inc/string.h>
#include <inc/partition.h>

#include "fs.h"

// --------------------------------------------------------------
// Super block
// --------------------------------------------------------------

// Validate the file system super-block.
void
check_super(void)
{
	if (super->s_magic != FS_MAGIC)
		panic("bad file system magic number");

	if (super->s_nblocks > DISKSIZE/BLKSIZE)
		panic("file system is too large");

	cprintf("superblock is good\n");
}

// --------------------------------------------------------------
// Free block bitmap
// --------------------------------------------------------------

// Check to see if the block bitmap indicates that block 'blockno' is free.
// Return 1 if the block is free, 0 if not.
bool
block_is_free(uint32_t blockno)
{
	if (super == 0 || blockno >= super->s_nblocks)
		return 0;
	if (bitmap[blockno / 32] & (1 << (blockno % 32)))
		return 1;
	return 0;
}

// Mark a block free in the bitmap
// 释放一个block，将其标记为free
void
free_block(uint32_t blockno)
{
	// Blockno zero is the null pointer of block numbers.
	if (blockno == 0)
		panic("attempt to free zero block");
	bitmap[blockno/32] |= 1<<(blockno%32);
}

// Search the bitmap for a free block and allocate it.  When you
// allocate a block, immediately flush the changed bitmap block
// to disk.
//
// Return block number allocated on success,
// -E_NO_DISK if we are out of blocks.
//
// Hint: use free_block as an example for manipulating the bitmap.
// 从bitmap中找到一个没有分配的磁盘块，然后将其标记为in-use
int
alloc_block(void)
{
	// The bitmap consists of one or more blocks.  A single bitmap block
	// contains the in-use bits for BLKBITSIZE blocks.  There are
	// super->s_nblocks blocks in the disk altogether.

	// LAB 5: Your code here.
	// panic("alloc_block not implemented");
	uint32_t i;
	for(i = 0; i < super->s_nblocks; i++){
		if(block_is_free(i)){
			bitmap[i/32] ^= 1<<(i%32);
			flush_block((void*)&bitmap[i / 32]);
			return i;
		}
	}
	// flush_block()
	return -E_NO_DISK;
}

// Validate the file system bitmap.
//
// Check that all reserved blocks -- 0, 1, and the bitmap blocks themselves --
// are all marked as in-use.
// 查看用于bitmap的block是否在bitmap中已经标记为in-use
void
check_bitmap(void)
{
	uint32_t i;

	// Make sure all bitmap blocks are marked in-use
	// 相当于测试每个block要一个bit容纳，一共要多少个block，且这些block都处于in-use
	for (i = 0; i * BLKBITSIZE < super->s_nblocks; i++)
		assert(!block_is_free(2+i));

	// Make sure the reserved and root blocks are marked in-use.
	assert(!block_is_free(0));
	assert(!block_is_free(1));

	cprintf("bitmap is good\n");
}

// --------------------------------------------------------------
// File system structures
// --------------------------------------------------------------



// Initialize the file system
void
fs_init(void)
{
	static_assert(sizeof(struct File) == 256);

	// Find a JOS disk.  Use the second IDE disk (number 1) if available
	if (ide_probe_disk1())
		ide_set_disk(1);
	else
		ide_set_disk(0);
	bc_init();

	// Set "super" to point to the super block.
	super = diskaddr(1);
	check_super();

	// Set "bitmap" to the beginning of the first bitmap block.
	bitmap = diskaddr(2);
	check_bitmap();
	
}

// Find the disk block number slot for the 'filebno'th block in file 'f'.
// Set '*ppdiskbno' to point to that slot.
// The slot will be one of the f->f_direct[] entries,
// or an entry in the indirect block.
// When 'alloc' is set, this function will allocate an indirect block
// if necessary.
//
// Returns:
//	0 on success (but note that *ppdiskbno might equal 0).
//	-E_NOT_FOUND if the function needed to allocate an indirect block, but
//		alloc was 0.
//	-E_NO_DISK if there's no space on the disk for an indirect block.
//	-E_INVAL if filebno is out of range (it's >= NDIRECT + NINDIRECT).
//
// Analogy: This is like pgdir_walk for files.
// Hint: Don't forget to clear any block you allocate.
// 查找f指向文件结构的第filebno个block的存储地址，保存到ppdiskbno中。如果f->f_indirect还没有分配，
// 且alloc为真，那么将分配要给新的block作为该文件的f->f_indirect。类比页表管理的pgdir_walk()。
// 其中ppdiskbno保存的不是blockno而是保存 保存blockno的uint32的地址
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
    // LAB 5: Your code here.
    // panic("file_block_walk not implemented");
	if(filebno >= NDIRECT + NINDIRECT){
		return -E_INVAL;
	}
	// 若要找到blockno在可以直接寻址的block中
	if(filebno < NDIRECT){
		*ppdiskbno = &(f->f_direct[filebno]);
	} else{
		if(f->f_indirect){
			uint32_t *addr = diskaddr(f->f_indirect);
			*ppdiskbno = &(addr[filebno - NDIRECT]);
		} else{
			if(!alloc) return -E_NOT_FOUND;
			int blockno;
			if((blockno = alloc_block()) < 0){
				return -E_NO_DISK;
			}
			// f_indirect保存的是对应的磁盘block
			f->f_indirect = blockno;
			flush_block(diskaddr(blockno));
			uint32_t *addr = diskaddr(blockno);
			*ppdiskbno = &(addr[filebno - NDIRECT]);
		}
	}
	return 0;
}

// Set *blk to the address in memory where the filebno'th
// block of file 'f' would be mapped.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_NO_DISK if a block needed to be allocated but the disk is full.
//	-E_INVAL if filebno is out of range.
//
// Hint: Use file_block_walk and alloc_block.
// 在File文件中获取该文件的第filebno个block，并将block的内存中地址存入blk中
// 如果第filebno个block并没有分配磁盘空间，则给这个block分配磁盘空间
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
    // LAB 5: Your code here.
    // panic("file_get_block not implemented");
	int r;
	uint32_t *ppdiskbno;
	if((r = file_block_walk(f, filebno, &ppdiskbno, 1)) < 0){
		return r;
	}

	if(!(*ppdiskbno)){
		if((r = alloc_block()) < 0){
			return r;
		}
		*ppdiskbno = r;
		flush_block(diskaddr(r));
	}
	*blk = diskaddr(*ppdiskbno);
	return 0;
}

// Try to find a file named "name" in dir.  If so, set *file to it.
//
// Returns 0 and sets *file on success, < 0 on error.  Errors are:
//	-E_NOT_FOUND if the file is not found
// 在File dir中寻找名称为name的文件，并将这个File存储到file中
static int
dir_lookup(struct File *dir, const char *name, struct File **file)
{
	int r;
	uint32_t i, j, nblock;
	char *blk;
	struct File *f;

	// Search dir for name.
	// We maintain the invariant that the size of a directory-file
	// is always a multiple of the file system's block size.
	// 目录类型的File下存储的block都是File元信息，所以文件大小必定和BLKSIZE对齐
	// 且一个block中可以存储多个File元信息
	assert((dir->f_size % BLKSIZE) == 0);
	// 通过文件大小计算这个目录下有多少个block
	nblock = dir->f_size / BLKSIZE;
	for (i = 0; i < nblock; i++) {
		// 获取File下第i个block
		if ((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		// 因为dir是目录类型，所以目录下的block都是File类型，且一个block中可以容纳多个File元信息，
		// 所以将其转换为File数组类型进行解读
		f = (struct File*) blk;
		for (j = 0; j < BLKFILES; j++)
			if (strcmp(f[j].f_name, name) == 0) {
				*file = &f[j];
				return 0;
			}
	}
	return -E_NOT_FOUND;
}

// Set *file to point at a free File structure in dir.  The caller is
// responsible for filling in the File fields.
// 在dir中寻找一个block中的空位来存放file信息
static int
dir_alloc_file(struct File *dir, struct File **file)
{
	int r;
	uint32_t nblock, i, j;
	char *blk;
	struct File *f;

	assert((dir->f_size % BLKSIZE) == 0);
	// 查看dir中有多少个block
	nblock = dir->f_size / BLKSIZE;
	for (i = 0; i < nblock; i++) {
		// 获取dir中第i个block
		if ((r = file_get_block(dir, i, &blk)) < 0)
			return r;
		// 将其转换成File数组理解
		f = (struct File*) blk;
		// 遍历File数组
		for (j = 0; j < BLKFILES; j++)
			// 找到已有block中的一个File空位
			if (f[j].f_name[0] == '\0') {
				*file = &f[j];
				return 0;
			}
	}
	// 在已有block空位中找不到空位，在blockno=i的地方再分配一个block
	dir->f_size += BLKSIZE;
	if ((r = file_get_block(dir, i, &blk)) < 0)
		return r;
	f = (struct File*) blk;
	*file = &f[0];
	return 0;
}

// Skip over slashes.
static const char*
skip_slash(const char *p)
{
	while (*p == '/')
		p++;
	return p;
}

// Evaluate a path name, starting at the root.
// On success, set *pf to the file we found
// and set *pdir to the directory the file is in.
// If we cannot find the file but find the directory
// it should be in, set *pdir and copy the final path
// element into lastelem.
// 遍历路径path，看能否找到path对应的文件，如果找到了就将最后的目录File赋值给pgdir，把找到的文件赋值给pf
// 若没找到则把最后找不到的文件名称赋值给lastelem，并将最后遍历到的目录路File赋值给pgdir
static int
walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem)
{
	const char *p;
	char name[MAXNAMELEN];
	struct File *dir, *f;
	int r;

	// if (*path != '/')
	//	return -E_BAD_PATH;
	path = skip_slash(path);
	f = &super->s_root;
	dir = 0;
	name[0] = 0;

	if (pdir)
		*pdir = 0;
	*pf = 0;
	while (*path != '\0') {
		dir = f;
		p = path;
		// 从path开始找到第一个 ‘/’，p指向上一次的开头，path指向一个名称的结尾
		while (*path != '/' && *path != '\0')
			path++;
		if (path - p >= MAXNAMELEN)
			return -E_BAD_PATH;
		// 将p～path的一段Filename复制到name中
		memmove(name, p, path - p);
		name[path - p] = '\0';
		// path跳过‘/’
		path = skip_slash(path);
		// 若当前dir不是目录类型的File则报错
		if (dir->f_type != FTYPE_DIR)
			return -E_NOT_FOUND;
		// 在当前dir下寻找名称为name的File
		if ((r = dir_lookup(dir, name, &f)) < 0) {
			// 找不到name名称的File了，且path已经遍历到底，说明当前目录下没有name名称的File了
			if (r == -E_NOT_FOUND && *path == '\0') {
				// 将最后遍历到的目录赋值给pdir
				if (pdir)
					*pdir = dir;
				// 将最后找不到文件的名字name赋值给lastelem
				if (lastelem)
					strcpy(lastelem, name);
				// 由于找不到File所以pf为0
				*pf = 0;
			}
			return r;
		}
	}
	// 若找到则赋值当前目录以及目标文件，且lastelem为空
	if (pdir)
		*pdir = dir;
	*pf = f;
	return 0;
}

// --------------------------------------------------------------
// File operations
// --------------------------------------------------------------

// Create "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
// 创建一个路径为path的文件，并将File赋值给pf
int
file_create(const char *path, struct File **pf)
{
	char name[MAXNAMELEN];
	int r;
	struct File *dir, *f;
	// 找到path这个路径下对应的目录，理论上应该是最后找到name上一层的目录但是没有name这个文件
	// 若文件已存在则报错
	if ((r = walk_path(path, &dir, &f, name)) == 0)
		return -E_FILE_EXISTS;
	// 除了文件已存在的其他报错
	if (r != -E_NOT_FOUND || dir == 0)
		return r;
	// 在dir目录下分配一个文件f
	if ((r = dir_alloc_file(dir, &f)) < 0)
		return r;

	strcpy(f->f_name, name);
	*pf = f;
	file_flush(dir);
	return 0;
}

// Open "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
// 打开路径path的文件，并将文件File赋值给pf
int
file_open(const char *path, struct File **pf)
{
	return walk_path(path, 0, pf, 0);
}

// Read count bytes from f into buf, starting from seek position
// offset.  This meant to mimic the standard pread function.
// Returns the number of bytes read, < 0 on error.
// 从文件file中读取count个字节的内容，并且偏移量是offset，将内容写入buf中，最后返回读取的字节数
ssize_t
file_read(struct File *f, void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;

	if (offset >= f->f_size)
		return 0;
	// 确定真实可读取的字节数量
	count = MIN(count, f->f_size - offset);
	// 从offset开始读取count个字节
	for (pos = offset; pos < offset + count; ) {
		// 根据pos确定在文件上是哪个blockno，然后获取对应block在内存上的地址blk
		if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		// 判断能从这个block上读取多少个字节
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		// 将磁盘上对应block的字节读取到buf中
		memmove(buf, blk + pos % BLKSIZE, bn);
		pos += bn;
		buf += bn;
	}

	return count;
}


// Write count bytes from buf into f, starting at seek position
// offset.  This is meant to mimic the standard pwrite function.
// Extends the file if necessary.
// Returns the number of bytes written, < 0 on error.
// 在文件File中写入字节，从文件offset开始写入count个字节，内容在buf中
int
file_write(struct File *f, const void *buf, size_t count, off_t offset)
{
	int r, bn;
	off_t pos;
	char *blk;

	// Extend file if necessary
	// 判断要写的内容是否超出了文件大小，如果超出了就使用file_set_size额外给文件分配空间，设定新的大小
	if (offset + count > f->f_size)
		if ((r = file_set_size(f, offset + count)) < 0)
			return r;
	// 将内容写入文件对应位置
	for (pos = offset; pos < offset + count; ) {
		if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
			return r;
		bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
		memmove(blk + pos % BLKSIZE, buf, bn);
		pos += bn;
		buf += bn;
	}

	return count;
}

// Remove a block from file f.  If it's not there, just silently succeed.
// Returns 0 on success, < 0 on error.
// 释放File对应filebno的磁盘空间
static int
file_free_block(struct File *f, uint32_t filebno)
{
	int r;
	uint32_t *ptr;
	// 在file中找到对应filebno的block在内存中的地址
	if ((r = file_block_walk(f, filebno, &ptr, 0)) < 0)
		return r;
	// 如果存在则释放
	if (*ptr) {
		free_block(*ptr);
		*ptr = 0;
	}
	return 0;
}

// Remove any blocks currently used by file 'f',
// but not necessary for a file of size 'newsize'.
// For both the old and new sizes, figure out the number of blocks required,
// and then clear the blocks from new_nblocks to old_nblocks.
// If the new_nblocks is no more than NDIRECT, and the indirect block has
// been allocated (f->f_indirect != 0), then free the indirect block too.
// (Remember to clear the f->f_indirect pointer so you'll know
// whether it's valid!)
// Do not change f->f_size.
// 若新的文件大小小于旧的则把多出来的block全部释放，若新的文件大小甚至用不上非直接block块(f_indirect)则释放f_indirect
// 不需要为多出来的size分配block，因为在用file get block的时候，如果对应的block没有分配磁盘空间，他自己就会自动分配了
static void
file_truncate_blocks(struct File *f, off_t newsize)
{
	int r;
	uint32_t bno, old_nblocks, new_nblocks;

	old_nblocks = (f->f_size + BLKSIZE - 1) / BLKSIZE;
	new_nblocks = (newsize + BLKSIZE - 1) / BLKSIZE;
	for (bno = new_nblocks; bno < old_nblocks; bno++)
		if ((r = file_free_block(f, bno)) < 0)
			cprintf("warning: file_free_block: %e", r);

	if (new_nblocks <= NDIRECT && f->f_indirect) {
		free_block(f->f_indirect);
		f->f_indirect = 0;
	}
}

// Set the size of file f, truncating or extending as necessary.
// 为文件f设置新的文件大小
int
file_set_size(struct File *f, off_t newsize)
{
	if (f->f_size > newsize)
		file_truncate_blocks(f, newsize);
	f->f_size = newsize;
	flush_block(f);
	return 0;
}

// Flush the contents and metadata of file f out to disk.
// Loop over all the blocks in file.
// Translate the file block number into a disk block number
// and then check whether that disk block is dirty.  If so, write it out.
// 将文件内容从内存写回到磁盘中
void
file_flush(struct File *f)
{
	int i;
	uint32_t *pdiskbno;

	for (i = 0; i < (f->f_size + BLKSIZE - 1) / BLKSIZE; i++) {
		if (file_block_walk(f, i, &pdiskbno, 0) < 0 ||
		    pdiskbno == NULL || *pdiskbno == 0)
			continue;
		flush_block(diskaddr(*pdiskbno));
	}
	flush_block(f);
	if (f->f_indirect)
		flush_block(diskaddr(f->f_indirect));
}


// Sync the entire file system.  A big hammer.
// 刷新整个磁盘内容，从内存写到磁盘中
void
fs_sync(void)
{
	int i;
	for (i = 1; i < super->s_nblocks; i++)
		flush_block(diskaddr(i));
}

