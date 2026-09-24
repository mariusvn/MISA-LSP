## Keyboard helpers, included by ../main.asm (relative paths start from this file).

def QUIT_KEY 'q'

sbmk "print_key(key: u32t, flags: u32t): void"
## Prints the key in a0 if it is a printable key press.
## Parameters:
## > a0 - key code (ASCII or KEY_*)
## > a1 - event flags (KBE_*)
print_key:
    and cr, a1, KBE_PRESSED
    jfs @done+
    and cr, a0, 0x80        # non-printable keys use codes >= 0x80
    jtr @done+
    str u8t, KEY_BUFFER, a0
    mov a0, KEY_BUFFER
    syscall SYS_PRINT_STRING
@done:
    ret
