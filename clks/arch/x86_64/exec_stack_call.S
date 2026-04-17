.intel_syntax noprefix

.global clks_exec_call_on_stack_x86_64
.type clks_exec_call_on_stack_x86_64, @function
.global clks_exec_abort_to_caller_x86_64
.type clks_exec_abort_to_caller_x86_64, @function

clks_exec_call_on_stack_x86_64:
    mov r11, rsp
    mov rsp, rsi
    and rsp, -16
    sub rsp, 56
    mov [rsp], r11
    mov [rsp + 8], rbx
    mov [rsp + 16], rbp
    mov [rsp + 24], r12
    mov [rsp + 32], r13
    mov [rsp + 40], r14
    mov [rsp + 48], r15

    call rdi

    mov rbx, [rsp + 8]
    mov rbp, [rsp + 16]
    mov r12, [rsp + 24]
    mov r13, [rsp + 32]
    mov r14, [rsp + 40]
    mov r15, [rsp + 48]
    mov rsp, [rsp]
    ret

.size clks_exec_call_on_stack_x86_64, .-clks_exec_call_on_stack_x86_64

clks_exec_abort_to_caller_x86_64:
    mov rax, rsi
    mov rsp, rdi
    mov rbx, [rsp + 8]
    mov rbp, [rsp + 16]
    mov r12, [rsp + 24]
    mov r13, [rsp + 32]
    mov r14, [rsp + 40]
    mov r15, [rsp + 48]
    mov rsp, [rsp]
    ret

.size clks_exec_abort_to_caller_x86_64, .-clks_exec_abort_to_caller_x86_64
.section .note.GNU-stack,"",@progbits
