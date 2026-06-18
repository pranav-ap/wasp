.data
.balign 8
str:
	.ascii "hello world"
	.byte 0
/* end data */

.text
.balign 16
.globl main
main:
	endbr64
	pushq %rbp
	movq %rsp, %rbp
	leaq str(%rip), %rdi
	callq puts
	movl $0, %eax
	leave
	ret
.type main, @function
.size main, .-main
/* end function main */

.section .note.GNU-stack,"",@progbits
