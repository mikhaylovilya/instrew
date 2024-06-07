    .intel_syntax noprefix
    .text
    .global _start
_start:
    mov rdi, rsp
    mov eax, 0xdf
    mov ecx, 8
    rep stosb
    call foo
.Laftercall:

    mov edi, 1 // exit status
    mov rsi, 0xdfdfdfdfdfdfdfdf
    cmp [rsp], rsi
    jne .Lexit
    lea rsi, [rip + .Laftercall]
    cmp [rsp-8], rsi
    jne .Lexit

    xor edi, edi
.Lexit:
    mov eax, 231
    syscall
    ud2

foo:
    ret
