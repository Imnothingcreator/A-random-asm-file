[bits 16]
[org 0x7c00]

start:
    ; --------------------------------------
    ; Initialize Segments & Stack
    ; --------------------------------------
    cli                     ; Disable interrupts
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00          ; Set stack top
    sti                     ; Re-enable interrupts

    ; Print startup banner
    mov si, variableName
    call printString

    ; --------------------------------------
    ; 1. ENTER USERNAME
    ; --------------------------------------
    mov si, promptMsg
    call printString

    mov di, nameBuffer      ; Set DI to point to nameBuffer
    call readString         ; Read input into nameBuffer

    ; --------------------------------------
    ; 2. PASSWORD SETUP FLOW
    ; --------------------------------------
password_setup:
    ; Enter first password
    mov si, passPrompt1
    call printString
    
    mov di, passBuffer1     ; Set DI to point to passBuffer1
    call readPassword       ; Read with '*' masking

    ; Confirm password
    mov si, passPrompt2
    call printString
    
    mov di, passBuffer2     ; Set DI to point to passBuffer2
    call readPassword       ; Read with '*' masking

    ; Compare the two passwords
    mov si, passBuffer1
    mov di, passBuffer2
    call compareString
    jc password_success     ; If Carry Flag is set, they match!

    ; If they don't match, print error and let them try again
    mov si, passMismatchMsg
    call printString
    jmp password_setup

    ; --------------------------------------
    ; 3. SUCCESS & WELCOME
    ; --------------------------------------
password_success:
    mov si, passSuccessMsg
    call printString

    ; Print "Welcome to my OS, [Name]"
    mov si, echoMsg
    call printString

    mov si, nameBuffer
    call printString
    
    mov si, newline
    call printString

    jmp $                   ; Infinite hang (OS pauses here)

; --------------------------------------
; Subroutines
; --------------------------------------

; Prints null-terminated string at SI
printString:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0e
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    popa
    ret

; Reads input string while printing '*' (DI must point to destination buffer)
readPassword:
    mov cx, 0               ; Character counter

.readLoop:
    mov ah, 0x00
    int 0x16                ; Read keypress

    cmp al, 0x0d            ; Enter key?
    je .enterPressed
    cmp al, 0x08            ; Backspace key?
    je .backspace
    cmp cx, 15              ; Limit to 15 chars to fit in buffer
    jge .readLoop

    mov [di], al            ; Store character in buffer
    inc di
    inc cx

    ; Mask character with '*'
    mov ah, 0x0e
    mov al, '*'
    mov bh, 0
    int 0x10
    jmp .readLoop

.backspace:
    cmp cx, 0               ; Nothing to delete?
    je .readLoop

    dec di
    dec cx
    mov byte [di], 0

    ; Visual backspace sequence (Back, Space, Back)
    mov ah, 0x0e
    mov bh, 0
    mov al, 0x08
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 0x08
    int 0x10
    jmp .readLoop

.enterPressed:
    mov byte [di], 0        ; Null terminate the string
    mov ah, 0x0e
    mov bh, 0
    mov al, 0x0d
    int 0x10
    mov al, 0x0a
    int 0x10
    ret

; Reads visible string input (DI must point to destination buffer)
readString:
    mov cx, 0

.readLoop:
    mov ah, 0x00
    int 0x16

    cmp al, 0x0d
    je .enterPressed
    cmp al, 0x08
    je .backspace
    cmp cx, 15
    jge .readLoop

    mov [di], al
    inc di
    inc cx

    mov ah, 0x0e            ; Echo the actual character back to screen
    mov bh, 0
    int 0x10
    jmp .readLoop

.backspace:
    cmp cx, 0
    je .readLoop

    dec di
    dec cx
    mov byte [di], 0

    mov ah, 0x0e
    mov bh, 0
    mov al, 0x08
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 0x08
    int 0x10
    jmp .readLoop

.enterPressed:
    mov byte [di], 0
    mov ah, 0x0e
    mov bh, 0
    mov al, 0x0d
    int 0x10
    mov al, 0x0a
    int 0x10
    ret

; Compares strings at SI and DI. Sets Carry Flag if they match.
compareString:
    pusha
.loop:
    mov al, [si]
    mov bl, [di]
    cmp al, bl
    jne .not_equal
    cmp al, 0               ; End of string reached?
    je .equal
    inc si
    inc di
    jmp .loop
.not_equal:
    popa
    clc                     ; Clear Carry Flag (Passwords don't match)
    ret
.equal:
    popa
    stc                     ; Set Carry Flag (Passwords match)
    ret

; --------------------------------------
; Data & Variables
; --------------------------------------

variableName:     db "Bishesh OS", 0x0d, 0x0a, 0
promptMsg:        db "Enter your name: ", 0
passPrompt1:      db "Enter password: ", 0
passPrompt2:      db "Confirm password: ", 0
passMismatchMsg:  db 0x0d, 0x0a, "Passwords do not match! Try again.", 0x0d, 0x0a, 0x0d, 0x0a, 0
passSuccessMsg:   db 0x0d, 0x0a, "Setup Successful!", 0x0d, 0x0a, 0
echoMsg:          db 0x0d, 0x0a, "Welcome to my OS, ", 0
newline:          db 0x0d, 0x0a, 0

; Memory Buffers (16 bytes each to fit safely inside 512 bytes)
nameBuffer:       times 16 db 0
passBuffer1:      times 16 db 0
passBuffer2:      times 16 db 0

; Boot sector padding & magic signature
times 510-($-$$) db 0
dw 0xaa55
