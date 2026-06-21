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
	movl $30, %eax
	ret
.type main, @function
.size main, .-main
/* end function main */

.section .note.GNU-stack,"",@progbits
