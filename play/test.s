.data
.balign 8
str:
	.ascii "hello world"
	.byte 0
/* end data */

.data
.balign 8
a:
	.int 1
	.int 2
	.int 3
	.byte 0
/* end data */

.bss
.balign 8
b:
	.fill 1000,1,0
/* end data */

.data
.balign 8
c:
	.quad 0
	.quad c+0
/* end data */

.text
.balign 16
getone:
	endbr64
	subq $16, %rsp
	movq %rdi, 0(%rsp)
	movl 0(%rsp), %eax
	addq $16, %rsp
	ret
.type getone, @function
.size getone, .-getone
/* end function getone */

.text
.balign 16
loop:
	endbr64
	movl $100, %eax
.p2align 4
.Lbb4:
	subl $1, %eax
	jnz .Lbb4
	ret
.type loop, @function
.size loop, .-loop
/* end function loop */

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
