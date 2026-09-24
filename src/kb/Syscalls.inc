// Syscalls.inc — included inside KnowledgeBase.cpp
// Format: name, description, {args}, {returns}

{"SYS_PRINT_INT", "Prints an integer value to the terminal.",
 {{"a0", "value to print", false}}, {}},

{"SYS_PRINT_LINE_INT", "Prints an integer value to the terminal, followed by a newline.",
 {{"a0", "value to print", false}}, {}},

{"SYS_PRINT_FLOAT", "Prints a float to the terminal.",
 {{"a0", "float value", true}, {"a1", "decimal places (0 = default)", false}}, {}},

{"SYS_PRINT_LINE_FLOAT", "Prints a float to the terminal, followed by a newline.",
 {{"a0", "float value", true}, {"a1", "decimal places (0 = default)", false}}, {}},

{"SYS_PRINT_STRING", "Prints a null-terminated string to the terminal.",
 {{"a0", "pa-relative address of string", false}}, {}},

{"SYS_PRINT_LINE_STRING", "Prints a null-terminated string to the terminal, followed by a newline.",
 {{"a0", "pa-relative address of string", false}}, {}},

{"SYS_DRAW_RECT", "Draws a filled rectangle on the back buffer.",
 {{"a0","pos_x",false},{"a1","pos_y",false},{"a2","size_x",false},{"a3","size_y",false},{"a4","luma",false}}, {}},

{"SYS_DRAW_TEXTURE", "Draws a luma texture on the back buffer. Flags: bit 1 = X mirror, bit 0 = Y mirror.",
 {{"a0","pa-relative texture address",false},{"a1","pos_x",false},{"a2","pos_y",false},{"a3","flags",false}}, {}},

{"SYS_DRAW_TEXTURE_REGION", "Draws a sub-region of a luma texture. Flags: bit 1 = X mirror, bit 0 = Y mirror.",
 {{"a0","pa-relative texture address",false},{"a1","pos_x",false},{"a2","pos_y",false},
  {"a3","region_x",false},{"a4","region_y",false},{"a5","region_w",false},{"a6","region_h",false},{"a7","flags",false}}, {}},

{"SYS_STORAGE_READ", "Reads bytes from persistent storage into memory.",
 {{"a0","dst pa-relative address",false},{"a1","src storage offset",false},{"a2","size in bytes",false}}, {}},

{"SYS_STORAGE_WRITE", "Writes bytes from memory to persistent storage.",
 {{"a0","dst storage offset",false},{"a1","src pa-relative address",false},{"a2","size in bytes",false}}, {}},

{"SYS_MEM_COPY", "Copies a block of memory.",
 {{"a0","dst pa-relative",false},{"a1","src pa-relative",false},{"a2","size in bytes",false}}, {}},

{"SYS_MEM_SET", "Sets a block of memory to a constant byte value.",
 {{"a0","address pa-relative",false},{"a1","size in bytes",false},{"a2","byte value",false}}, {}},

{"SYS_PRESERVE_BACK_BUFFER", "Skips clearing the back buffer at the end of the current _draw frame.",
 {}, {}},

{"SYS_PRESERVE_FRONT_BUFFER", "Skips copying back buffer → front buffer at the end of the current _draw frame.",
 {}, {}},

{"SYS_GET_INPUT", "Returns the current input bitmask. Use BTN_* constants to test buttons.",
 {}, {{"a0","input bitmask (BTN_*)",false}}},

{"SYS_GET_MOUSE_POSITION", "Returns the current mouse position in screen coordinates.",
 {}, {{"a0","pos_x",false},{"a1","pos_y",false}}},

{"SYS_GET_MOUSE_BUTTON_INPUT", "Returns the current mouse button bitmask. Use MOUSE_BTN_* constants to test buttons.",
 {}, {{"a0","mouse button bitmask (MOUSE_BTN_*)",false}}},

{"SYS_GET_KEYBOARD_INPUT", "Returns the next buffered keyboard event. Printable keys use ASCII codes, others use KEY_* codes. Returns 0 in both registers when no event is buffered.",
 {}, {{"a0","key code (ASCII or KEY_*)",false},{"a1","event flags (KBE_*)",false}}},

{"SYS_GET_TERMINAL_INPUT_SIZE", "Returns the size of the next terminal input string, including the null byte (0 if the queue is empty).",
 {}, {{"a0","string size in bytes",false}}},

{"SYS_READ_TERMINAL_INPUT", "Reads the next terminal input string into memory and removes it from the queue. Throws if the buffer is too small.",
 {{"a0","dst pa-relative address",false},{"a1","buffer size in bytes",false}}, {{"a0","string size in bytes (0 if the queue is empty)",false}}},

{"SYS_GET_UNIX_TIME", "Returns the current Unix timestamp.",
 {}, {{"a0","unix timestamp",false}}},

{"SYS_GET_RUNNING_TIME", "Returns elapsed seconds since VM startup as a float.",
 {}, {{"a0","elapsed seconds (float)",true}}},

{"SYS_GET_UPDATE_DELTA", "Returns elapsed seconds since the last _update as a float.",
 {}, {{"a0","delta seconds (float)",true}}},

{"SYS_GET_DRAW_DELTA", "Returns elapsed seconds since the last _draw as a float.",
 {}, {{"a0","delta seconds (float)",true}}},

{"SYS_SET_RNG_SEED", "Seeds the random number generator.",
 {{"a0","seed value",false}}, {}},

{"SYS_ALLOW_UNSAFE_JUMP", "Allows the next control-flow instruction to jump to an address that may not be the start of an instruction. Advanced use only.",
 {}, {}},
