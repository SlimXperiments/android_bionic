/* Generated by gensyscalls.py. Do not edit. */

#include <private/bionic_asm.h>

ENTRY(__exit)
    movl    $__NR_exit, %eax
    syscall
    cmpq    $-MAX_ERRNO, %rax
    jb      1f
    negl    %eax
    movl    %eax, %edi
    call    __set_errno
    orq     $-1, %rax
1:
    ret
END(__exit)
.hidden __exit
