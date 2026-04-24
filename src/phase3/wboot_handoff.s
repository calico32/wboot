.section .text
.globl wboot_handoff

wboot_handoff: # rcx = boot_params, rdx = kernel_entry
    # load GDT
    lgdt gdtr(%rip)

    # reload cs via a far jump to the next instruction
    leaq .Lreload_cs(%rip), %rax
    pushq $0x10 # 0x10 -> __BOOT_CS
    pushq %rax
    lretq

.Lreload_cs:
    # set DS, ES, SS to __BOOT_DS
    movw $0x18, %ax # 0x18 -> __BOOT_DS
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    # clear fs and gs
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs

    # disable interrupts
    cli
    # set rsi to point to boot_params
    movq %rcx, %rsi

    # jump to kernel entry point
    jmpq *%rdx

    .align 8
gdt:
    .quad 0x0000000000000000 # [0x00] null
    .quad 0x0000000000000000 # [0x08] unused
    .quad 0x00AF9A000000FFFF # [0x10] __BOOT_CS
    .quad 0x00CF92000000FFFF # [0x18] __BOOT_DS
gdtr:
    .word gdtr - gdt - 1
    .quad gdt
