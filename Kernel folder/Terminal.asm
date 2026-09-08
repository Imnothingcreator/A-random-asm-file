[org 0x1000]
[bits 16]

shell_main:
    mov si, msg_shell_welcome
    call shell_print

shell_loop:
    mov si, prompt_str
    call shell_print

    mov di, input_buffer
    call read_line

    ; Check command: "fastfetch"
    mov si, input_buffer
    mov di, cmd_fastfetch
    call strcmp
    jc execute_fastfetch

    ; Check command: "clear"
    mov si, input_buffer
    mov di, cmd_clear
    call strcmp
    jc execute_clear

    ; Check command: "sudo apt install"
    mov si, input_buffer
    mov di, cmd_apt_prefix
    call strncmp
    jc execute_apt

    ; Empty input check
    cmp byte [input_buffer], 0
    je shell_loop

    ; Unknown command handler
    mov si, msg_not_found
    call shell_print
    mov si, input_buffer
    call shell_print
    mov si, newline
    call shell_print
    jmp shell_loop

; --- COMMAND: fastfetch with 'U' Banner ---
execute_fastfetch:
    mov si, banner_u
    call shell_print
    jmp shell_loop

; --- COMMAND: clear ---
execute_clear:
    mov ax, 0x0003
    int 0x10
    jmp shell_loop

; --- COMMAND: sudo apt install ---
execute_apt:
    mov si, msg_apt_start
    call shell_print
    mov si, input_buffer + 17   ; Skip prefix to print package name
    call shell_print
    mov si, msg_apt_end
    call shell_print
    jmp shell_loop

; --- Subroutines ---
shell_print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bl, 0x07
    int 0x10
    jmp shell_print
.done:
    ret

read_line:
    xor cx, cx
.read_loop:
    mov ah, 0x00
    int 0x16
    cmp al, 0x0D
    je .enter
    cmp al, 0x08
    je .backspace
    cmp cx, 63
    jge .read_loop
    mov ah, 0x0E
    int 0x10
    stosb
    inc cx
    jmp .read_loop
.backspace:
    cmp cx, 0
    jle .read_loop
    dec di
    dec cx
    mov ah, 0x0E
    mov al, 0x08
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 0x08
    int 0x10
    jmp .read_loop
.enter:
    mov byte [di], 0
    mov si, newline
    call shell_print
    ret

strcmp:
    push si
    push di
.l1:
    mov al, [si]
    mov bl, [di]
    cmp al, bl
    jne .no
    test al, al
    jz .yes
    inc si
    inc di
    jmp .l1
.yes:
    pop di
    pop si
    stc
    ret
.no:
    pop di
    pop si
    clc
    ret

strncmp:
    push si
    push di
.l2:
    mov bl, [di]
    test bl, bl
    jz .yes2
    mov al, [si]
    cmp al, bl
    jne .no2
    inc si
    inc di
    jmp .l2
.yes2:
    pop di
    pop si
    stc
    ret
.no2:
    pop di
    pop si
    clc
    ret

; --- Data Section ---
msg_shell_welcome: db 13, 10, "Shell Ready! Type 'fastfetch' or 'sudo apt install <pkg>'", 13, 10, 13, 0
prompt_str:        db "root@customos:/# ", 0
newline:           db 13, 10, 0
msg_not_found:     db "Command not found: ", 0

cmd_fastfetch:     db "fastfetch", 0
cmd_clear:         db "clear", 0
cmd_apt_prefix:    db "sudo apt install ", 0

banner_u:          db 13, 10, \
                      "  _   _          OS: CustomOS 16-bit", 13, 10, \
                      " | | | |         Shell: ASM Terminal", 13, 10, \
                      " | | | |         Uptime: Active", 13, 10, \
                      " | |_| |         Status: Stable", 13, 10, \
                      "  \\___/          CPU: x86 Real Mode", 13, 10, 13, 0

msg_apt_start:     db 13, 10, "[sudo] password for root: ", 13, 10, \
                      "Reading package lists... Done", 13, 10, \
                      "Unpacking package: ", 0
msg_apt_end:       db " (1.0.0)... [OK] Installed successfully!", 13, 10, 13, 0

input_buffer:      times 64 db 0
