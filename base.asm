section .text
global _start

_start:
	mov rcx, _start
	mov r8, _start
	mov r9, _start
	add r9, 0x100
	mov byte [disp_data+2], ' '
	mov rsi, disp_data
	mov rdi, 0
disp_line:
	add r8, 0x10
disp_byte:
	push rcx
	mov rax, [rcx]
	call asciibyte
	mov rax, 1
	mov rdx, 3
	syscall
	pop rcx
	inc rcx
	cmp rcx, r8
	jl disp_byte
	mov byte [disp_data], 0xa
	mov rax, 1
	mov rdx, 1
	push rcx
	syscall
	pop rcx
	cmp rcx, r9
	jle disp_line

	mov rax, 60
	mov rdi, 0
	syscall

asciibyte:
	mov rbx, rax
	call asciinibble
	mov [rsi+1], bl
	mov rbx, rax
	shr rbx, 4
	call asciinibble
	mov [rsi], bl
	ret

asciinibble:
	and rbx, 0xf
	cmp rbx, 10
	jl asciinibble_num
	sub rbx, 10
	add rbx, 'a'
	jmp asciinibble_out
asciinibble_num:
	add rbx, '0'
asciinibble_out:
	ret

section .bss
disp_data: resb 3

