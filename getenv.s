	.file	"getenv.c"
	.text
	.globl	_findenv
	.type	_findenv, @function
_findenv:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movq	%rsi, -8(%rbp)
	movq	_impure_ptr(%rip), %rax
	movq	-8(%rbp), %rdx
	movq	%rdi, %rsi
	movq	%rax, %rdi
	call	_findenv_r
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	_findenv, .-_findenv
	.globl	getenv
	.type	getenv, @function
getenv:
.LFB1:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movq	%rdi, -24(%rbp)
	movq	_impure_ptr(%rip), %rax
	leaq	-4(%rbp), %rdx
	movq	-24(%rbp), %rcx
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	_findenv_r
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1:
	.size	getenv, .-getenv
	.ident	"GCC: (GNU) 14.1.0"
