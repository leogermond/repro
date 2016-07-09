        .global _start

        .text
_start:
        # write(4, message, 10)
        mov     $1, %rax                # system call 1 is write
        mov     $4, %rdi                # file handle
        mov     $message, %rsi          # address of string to output
        mov     $10, %rdx               # number of bytes
        syscall                         # invoke operating system to do the write

        # exit(0)
        mov     $60, %rax               # system call 60 is exit
        xor     %rdi, %rdi              # we want return code 0
        syscall                         # invoke operating system to exit
message:
        .ascii  "\0\0\0\6coucou"

