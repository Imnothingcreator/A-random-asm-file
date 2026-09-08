[org 0x7c00]
[bits 16]

start:
    ; Setup segment registers and stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Set video mode to standard 80x25 text mode (clears screen)
    mov ah, 0x00
    mov al, 0x03
    int 0x10

    ; Print Welcome Header
    mov si, msg_title
    call print_string

    ; Print Loading Text & Opening Bracket
    mov si, msg_loading
    call print_string

    ; Draw progress bar blocks dynamically
    mov cx, 24              ; Number of blocks in the loading bar
load_loop:
    push cx
    
    ; Print block character using BIOS teletype
    mov ah, 0x0E
    mov al, 219             ; ASCII 219 is a solid block character (█)
    mov bl, 0x0A            ; Bright green text attribute
    int 0x10

    ; Delay loop so the animation is visible
    call delay

    pop cx
    loop load_loop

    ; Print completion message
    mov si, msg_done
    call print_string

halt:
    cli
    hlt
    jmp halt

; --- Subroutine: Print Null-Terminated String ---
print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07            ; Default light grey text
    int 0x10
    jmp print_string
.done:
    ret

; --- Subroutine: Simple Delay Loop ---
delay:
    push cx
    mov cx, 0xFFFF          ; Adjust this hex value to speed up or slow down loading
.delay_loop:
    nop
    loop .delay_loop
    pop cx
    ret

; --- Data Section ---
msg_title:   db 13, 10, "   ========================================", 13, 10, \
                        "        CUSTOM 16-BIT OS LOADING...", 13, 10, \
                        "   ========================================", 13, 10, 13, 10, 0
msg_loading: db "   Initializing: [", 0
msg_done:    db "]", 13, 10, 13, 10, "   [OK] System ready. Starting kernel...", 13, 10, 0

; --- Boot Sector Padding & Signature ---
times 510 - ($ - $$) db 0   ; Fill the rest of the 512-byte sector with zeros
dw 0xaa55                   ; Magic boot signature required by BIOS
