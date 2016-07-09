        .global _start

        .text
_start:
	mov $0, %rbx # code len counter
	mov $0, %rcx # reception counter
read:
# save context (syscall erases rcx)
	push %rcx

# read byte
        mov     $0, %rax # read             
        mov     $3, %rdi # 3
	lea  8(%rsp), %rsi # store the byte read 8 words over the stack
        mov     $1, %rdx # 1 byte
        syscall

# write byte back (rsi already set correctly)
	mov $1, %rax # write
	mov $4, %rdi # 4
	mov $1, %rdx # 1 byte
	syscall

# check if code length (4 bytes) has been transfered
	pop %rcx
	inc %rcx
	cmp $4, %rcx
	jg ck_end

# add a byte (in NBO) of length
# save rcx
	mov %rcx, %rdx
# shift = 8 * (4 - i)
	mov $4, %r8b
	sub %cl, %r8b
	shl $3, %r8b # 8 = 2^3
	mov %r8b, %cl

# len += c << shift
	mov (%rsi), %rax
	shl %cl, %rax
	add %rax, %rbx
# restore rcx
	mov %rdx, %rcx

ck_end:
# over if len + 4 bytes have been received
	mov %rbx, %r8
	add $4, %r8
	cmp %rcx, %r8
	jne read

        mov     $60, %rax # exit
        mov     $0, %rdi # 0
        syscall
