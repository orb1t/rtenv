	.syntax unified
	.cpu cortex-m3
	.fpu softvfp
	.thumb

.macro syscall_define name, num
.global \name
\name:
	push {r7}
	mov r7, \num
	svc 0
	nop
	pop {r7}
	bx lr
.endm

syscall_define fork, #0x01
syscall_define getpid, #0x02
syscall_define write, #0x03
syscall_define read, #0x04
syscall_define interrupt_wait, #0x05
syscall_define getpriority, #0x06
syscall_define setpriority, #0x07
syscall_define mknod, #0x08
syscall_define sleep, #0x09
syscall_define lseek, #0x0a
syscall_define setrlimit, #0x0b
syscall_define rmnod, #0x0c
syscall_define exit, #0x0d
syscall_define waitpid, #0x0e
syscall_define mmap, #0x0f
syscall_define exec_addr, #0x10
