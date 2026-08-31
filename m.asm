bits 64

%define SYS_WRITE (1)
%define SYS_EXIT (60)
%define STDOUT (1)
%define SCALE (1024)
%define FOUR (4096)
%define MAX_ITER (32)
%define W (80)
%define H (30)
%define RSPAN (3584)
%define RMIN (2560)
%define ISPAN (2458)
%define IMIN (1229)

section .data
palette: db ` .:-=+*#%@`
.len equ $ - palette

section .text
global main

main:
	push rbp
	mov rbp, rsp
	sub rsp, 96
	mov r12, 0
row_loop:
	mov rax, r12
	imul rax, ISPAN
	mov rcx, H
	cqo
	idiv rcx
	sub rax, IMIN
	mov r9, rax
	mov r13, 0
col_loop:
	mov rax, r13
	imul rax, RSPAN
	mov rcx, W
	cqo
	idiv rcx
	sub rax, RMIN
	mov r8, rax
	mov r14, 0
	mov r15, 0
	mov rbx, 0
iter_loop:
	mov rax, r14
	imul rax, r14
	mov rcx, SCALE
	cqo
	idiv rcx
	mov r10, rax
	mov rax, r15
	imul rax, r15
	mov rcx, SCALE
	cqo
	idiv rcx
	mov r11, rax
	mov rax, r10
	add rax, r11
	cmp rax, FOUR
	jle .if_end_0
	jmp plot
.if_end_0:
	mov rax, r14
	imul rax, r15
	imul rax, 2
	mov rcx, SCALE
	cqo
	idiv rcx
	add rax, r9
	mov rsi, rax
	mov rax, r10
	sub rax, r11
	add rax, r8
	mov r14, rax
	mov r15, rsi
	add rbx, 1
	cmp rbx, MAX_ITER
	jge .if_end_1
	jmp iter_loop
.if_end_1:
plot:
	mov rax, rbx
	imul rax, 9
	mov rcx, MAX_ITER
	cqo
	idiv rcx
	mov rsi, palette
	add rsi, rax
	movzx rdx, byte [rsi]
	lea rdi, [rbp - 81]
	add rdi, r13
	mov byte [rdi], dl
	add r13, 1
	cmp r13, W
	jge .if_end_2
	jmp col_loop
.if_end_2:
	lea rdi, [rbp - 81]
	add rdi, W
	mov rax, 10
	mov byte [rdi], al
	mov rax, SYS_WRITE
	mov rdi, STDOUT
	lea rsi, [rbp - 81]
	mov rdx, W
	add rdx, 1
	syscall
	add r12, 1
	cmp r12, H
	jge .if_end_3
	jmp row_loop
.if_end_3:
	mov rax, SYS_EXIT
	mov rdi, 0
	syscall
