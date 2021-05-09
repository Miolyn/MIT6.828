// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's cputchar().

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>


static void
putch(int ch, int *cnt)
{
	cputchar(ch);
	*cnt++;
}

int
vcprintf(const char *fmt, va_list ap)
{
	int cnt = 0;

	vprintfmt((void*)putch, &cnt, fmt, ap);
	return cnt;
}

int
cprintf(const char *fmt, ...)
{
	// 初始化一个 可变参数指针va_list
	va_list ap;
	int cnt;
	// 宏va_start(ap,lastfix)是为了初始化变参指针ap，以指向可变参数列表中未命名的第一个参数，
	// 即指向lastfix后的第一个变参。它必须在指针使用之前调用一次该宏，参数列表中至少有一个未命名的可变参数。
	// 这里表示fmt后面开始是一个可变参数，使用va_start进行初始化，根据fmt的位置取得第一个变长变量的地址赋值给ap
	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	// 宏va_end(ap)功能是完成清除变量ap的作用，表明程序以后不再使用，若该指针变量需再使用，
	// 必须重新调用宏va_start以启动该变量。
	// va_end就是把指针va_list 置0，即va置0	
	va_end(ap);

	return cnt;
}

