## Multi-file example: types every pressed key back to the terminal.
## Open lib/keys.asm on its own: the server analyses it through this file,
## so KEY_BUFFER below is known there.

include "lib/keys.asm"
include "lib/keys.asm"   # include-once: this second include has no effect

KEY_BUFFER: res u8t 2     # one character + null terminator

_start:
    mov a0, banner
    syscall SYS_PRINT_LINE_STRING
    exit

_keyboard_input:
    syscall SYS_GET_KEYBOARD_INPUT
    cal print_key
    exit

banner: emb string "Type something! (\'q\' quits)"
