/*	$NetBSD: stdarg.h,v 1.12 1995/12/25 23:15:31 mycroft Exp $	*/

#ifndef JOS_INC_STDARG_H
#define	JOS_INC_STDARG_H

typedef __builtin_va_list va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
// type va_arg ( va_list ap, type )
// va_list 是一个字符指针，可以理解为指向当前参数的一个指针，取参必须通过这个指针进行。
#define va_arg(ap, type) __builtin_va_arg(ap, type)

#define va_end(ap) __builtin_va_end(ap)
// 通常va_start和va_end是成对出现。
#endif	/* !JOS_INC_STDARG_H */
