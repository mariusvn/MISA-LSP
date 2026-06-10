// Instructions.inc — included inside KnowledgeBase.cpp
// OK = OperandKind alias is defined in KnowledgeBase.cpp before this include.
// Format: mnemonic, name, category, compact, description, pseudocode, {operands}


// ── Data Transfer ─────────────────────────────────────────────────────────────
{"lod", "Load", "Data Transfer", false,
 "Loads a value from a pa-relative address (usually a label) into a register.",
 "dest = M[pa + address][N:0]",
 {OK::TypeK, OK::RegW, OK::LblOrExpr}},

{"str", "Store", "Data Transfer", false,
 "Stores a register value to a pa-relative address.",
 "M[pa + address][N:0] = value[N:0]",
 {OK::TypeK, OK::LblOrExpr, OK::RegR}},

{"cea", "Compute Effective Address", "Data Transfer", false,
 "Sets the ea register: ea = (pa + base_address) + (index * scale).",
 "ea = (pa + base_address) + (index * scale)",
 {OK::LblOrExpr, OK::RegR, OK::AnyExpr}},

{"lde", "Load Effective", "Data Transfer", false,
 "Loads from the effective address ea + displacement.",
 "dest = M[ea + displacement][N:0]",
 {OK::TypeK, OK::RegW, OK::AnyExpr}},

{"ste", "Store Effective", "Data Transfer", false,
 "Stores to the effective address ea + displacement.",
 "M[ea + displacement][N:0] = value[N:0]",
 {OK::TypeK, OK::AnyExpr, OK::RegR}},

{"tpr", "To Program Relative", "Data Transfer", true,
 "Converts an absolute address to a pa-relative offset.",
 "dest = address - pa",
 {OK::RegW, OK::RegR}},

{"tpa", "To Program Absolute", "Data Transfer", true,
 "Converts a pa-relative offset to an absolute address.",
 "dest = address + pa",
 {OK::RegW, OK::RegR}},

{"psh", "Push", "Data Transfer", false,
 "Pushes a value onto the stack.",
 "sp -= 4; M[sp] = value",
 {OK::RegR}},

{"pop", "Pop", "Data Transfer", false,
 "Pops a value from the stack into a register.",
 "dest = M[sp]; sp += 4",
 {OK::RegW}},

{"mov", "Move", "Data Transfer", false,
 "Copies a source value into a destination register.",
 "dest = source",
 {OK::RegW, OK::RegR}},

{"mvc", "Move Conditional", "Data Transfer", false,
 "Copies source into dest only if the cr register is set.",
 "if (cr): dest = source",
 {OK::RegW, OK::RegR}},

{"sel", "Select", "Data Transfer", false,
 "Selects between two values based on cr.",
 "dest = cr ? true_value : false_value",
 {OK::RegW, OK::RegR, OK::RegR}},

{"swp", "Swap", "Data Transfer", false,
 "Swaps the values of two registers.",
 "swap(a, b)",
 {OK::RegW, OK::RegW}},

// ── Integer Arithmetic ────────────────────────────────────────────────────────
{"add", "Add", "Integer Arithmetic", true,
 "Adds two integer values.",
 "dest = a + b",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"sub", "Subtract", "Integer Arithmetic", true,
 "Subtracts b from a.",
 "dest = a - b",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"inc", "Increment", "Integer Arithmetic", false,
 "Increments a register by 1 in place.",
 "dest++",
 {OK::RegW}},

{"dec", "Decrement", "Integer Arithmetic", false,
 "Decrements a register by 1 in place.",
 "dest--",
 {OK::RegW}},

{"mul", "Multiply", "Integer Arithmetic", true,
 "Multiplies two integers; result is the lower 32 bits.",
 "dest = (a * b)[31:0]",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"mlh", "Multiply High", "Integer Arithmetic", true,
 "Multiplies two integers; result is the upper 32 bits.",
 "dest = (a * b)[63:32]",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"pow", "Power", "Integer Arithmetic", true,
 "Raises base to an integer exponent.",
 "dest = base ** exponent",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"div", "Divide", "Integer Arithmetic", true,
 "Integer division.",
 "dest = dividend / divisor",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"rem", "Remainder", "Integer Arithmetic", true,
 "Remainder; preserves the dividend's sign.",
 "dest = dividend % divisor",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"mod", "Modulo", "Integer Arithmetic", true,
 "Modulo; preserves the divisor's sign.",
 "dest = dividend %% divisor",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"neg", "Negate", "Integer Arithmetic", true,
 "Arithmetic negation.",
 "dest = -value",
 {OK::RegW, OK::RegRInt}},

{"abs", "Absolute Value", "Integer Arithmetic", true,
 "Absolute value.",
 "dest = abs(value)",
 {OK::RegW, OK::RegRInt}},

{"sgn", "Sign", "Integer Arithmetic", true,
 "Returns the sign of the value: -1, 0, or 1.",
 "dest = sign(value)",
 {OK::RegW, OK::RegRInt}},

{"min", "Minimum", "Integer Arithmetic", true,
 "Returns the smaller of two integers.",
 "dest = min(a, b)",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"max", "Maximum", "Integer Arithmetic", true,
 "Returns the larger of two integers.",
 "dest = max(a, b)",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"clp", "Clamp", "Integer Arithmetic", true,
 "Clamps value between min and max (swaps if min > max).",
 "dest = clamp(value, min, max)",
 {OK::RegW, OK::RegRInt, OK::RegRInt, OK::RegRInt}},

{"rnd", "Random", "Integer Arithmetic", false,
 "Returns a random integer in [min, max). max is exclusive.",
 "dest = rand_range(min, max)",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

// ── Floating-Point ────────────────────────────────────────────────────────────
{"fcti", "Float to Int", "Floating-Point", true,
 "Truncates a float to a signed integer.",
 "dest = (int)value",
 {OK::RegW, OK::RegRFlt}},

{"fctf", "Int to Float", "Floating-Point", true,
 "Converts a signed integer to a float.",
 "dest = (float)value",
 {OK::RegW, OK::RegRInt}},

{"fadd", "Float Add", "Floating-Point", true,
 "Adds two floats.",
 "dest = a + b",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"fsub", "Float Subtract", "Floating-Point", true,
 "Subtracts two floats.",
 "dest = a - b",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"fmul", "Float Multiply", "Floating-Point", true,
 "Multiplies two floats.",
 "dest = a * b",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"ffma", "Fused Multiply-Add", "Floating-Point", true,
 "Computes a * b + c in a single operation.",
 "dest = a * b + c",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt, OK::RegRFlt}},

{"fpow", "Float Power", "Floating-Point", true,
 "Raises base to a float exponent.",
 "dest = base ** exponent",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"fdiv", "Float Divide", "Floating-Point", true,
 "Divides two floats.",
 "dest = dividend / divisor",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"frem", "Float Remainder", "Floating-Point", true,
 "Float remainder; preserves dividend's sign.",
 "dest = dividend % divisor",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"fmod", "Float Modulo", "Floating-Point", true,
 "Float modulo; preserves divisor's sign.",
 "dest = dividend %% divisor",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"fwrp", "Float Wrap", "Floating-Point", true,
 "Wraps a float value within [min, max].",
 "dest = wrap(value, min, max)",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt, OK::RegRFlt}},

{"fneg", "Float Negate", "Floating-Point", true,
 "Negates a float.",
 "dest = -value",
 {OK::RegW, OK::RegRFlt}},

{"fabs", "Float Absolute", "Floating-Point", true,
 "Absolute value of a float.",
 "dest = abs(value)",
 {OK::RegW, OK::RegRFlt}},

{"fsgn", "Float Sign", "Floating-Point", true,
 "Returns the sign of a float as -1.0, 0.0, or 1.0.",
 "dest = sign(value)",
 {OK::RegW, OK::RegRFlt}},

{"fmin", "Float Minimum", "Floating-Point", true,
 "Returns the smaller of two floats.",
 "dest = min(a, b)",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"fmax", "Float Maximum", "Floating-Point", true,
 "Returns the larger of two floats.",
 "dest = max(a, b)",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"fclp", "Float Clamp", "Floating-Point", true,
 "Clamps a float between min and max.",
 "dest = clamp(value, min, max)",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt, OK::RegRFlt}},

{"frnd", "Float Random", "Floating-Point", false,
 "Returns a random float in [min, max).",
 "dest = rand_range(min, max)",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"fsqrt", "Square Root", "Floating-Point", true,
 "Square root of a float.",
 "dest = sqrt(value)",
 {OK::RegW, OK::RegRFlt}},

{"frsqrt", "Reciprocal Square Root", "Floating-Point", true,
 "Reciprocal square root of a float.",
 "dest = 1.0 / sqrt(value)",
 {OK::RegW, OK::RegRFlt}},

{"fflo", "Floor", "Floating-Point", true,
 "Floor of a float.",
 "dest = floor(value)",
 {OK::RegW, OK::RegRFlt}},

{"fcei", "Ceiling", "Floating-Point", true,
 "Ceiling of a float.",
 "dest = ceil(value)",
 {OK::RegW, OK::RegRFlt}},

{"frou", "Round", "Floating-Point", true,
 "Rounds a float to the nearest integer (round-half-to-even).",
 "dest = round(value)",
 {OK::RegW, OK::RegRFlt}},

{"ftru", "Truncate", "Floating-Point", true,
 "Truncates a float toward zero.",
 "dest = trunc(value)",
 {OK::RegW, OK::RegRFlt}},

{"ffra", "Fractional", "Floating-Point", true,
 "Returns the fractional part of a float.",
 "dest = fract(value)",
 {OK::RegW, OK::RegRFlt}},

{"fsin", "Sine", "Floating-Point", true,
 "Sine of a float (radians).",
 "dest = sin(value)",
 {OK::RegW, OK::RegRFlt}},

{"fcos", "Cosine", "Floating-Point", true,
 "Cosine of a float (radians).",
 "dest = cos(value)",
 {OK::RegW, OK::RegRFlt}},

{"ftan", "Tangent", "Floating-Point", true,
 "Tangent of a float (radians).",
 "dest = tan(value)",
 {OK::RegW, OK::RegRFlt}},

{"fasin", "Arcsin", "Floating-Point", true,
 "Arc sine (radians).",
 "dest = asin(value)",
 {OK::RegW, OK::RegRFlt}},

{"facos", "Arccos", "Floating-Point", true,
 "Arc cosine (radians).",
 "dest = acos(value)",
 {OK::RegW, OK::RegRFlt}},

{"fatan", "Arctan", "Floating-Point", true,
 "Arc tangent (radians).",
 "dest = atan(value)",
 {OK::RegW, OK::RegRFlt}},

{"fatan2", "Arctan2", "Floating-Point", true,
 "Arc tangent of y/x (radians), respecting quadrant.",
 "dest = atan2(y, x)",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt}},

{"frtd", "Radians to Degrees", "Floating-Point", true,
 "Converts radians to degrees.",
 "dest = rad_to_deg(value)",
 {OK::RegW, OK::RegRFlt}},

{"fdtr", "Degrees to Radians", "Floating-Point", true,
 "Converts degrees to radians.",
 "dest = deg_to_rad(value)",
 {OK::RegW, OK::RegRFlt}},

{"flog", "Natural Log", "Floating-Point", true,
 "Natural logarithm.",
 "dest = log(value)",
 {OK::RegW, OK::RegRFlt}},

{"fexp", "Exponential", "Floating-Point", true,
 "e raised to the power of value.",
 "dest = exp(value)",
 {OK::RegW, OK::RegRFlt}},

{"flrp", "Linear Interpolation", "Floating-Point", false,
 "Linearly interpolates between a and b by t.",
 "dest = lerp(a, b, t)",
 {OK::RegW, OK::RegRFlt, OK::RegRFlt, OK::RegRFlt}},

// ── Logic and Bitwise ─────────────────────────────────────────────────────────
{"cmp", "Compare", "Logic / Bitwise", false,
 "Compares two values and writes the boolean result to cr.",
 "cr = (a Condition b) ? 1 : 0",
 {OK::CondK, OK::RegR, OK::RegR}},

{"not", "Bitwise NOT", "Logic / Bitwise", true,
 "Bitwise complement.",
 "dest = ~value",
 {OK::RegW, OK::RegRInt}},

{"and", "Bitwise AND", "Logic / Bitwise", true,
 "Bitwise AND.",
 "dest = a & b",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"orr", "Bitwise OR", "Logic / Bitwise", true,
 "Bitwise OR.",
 "dest = a | b",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"xor", "Bitwise XOR", "Logic / Bitwise", true,
 "Bitwise XOR.",
 "dest = a ^ b",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"sar", "Shift Arithmetic Right", "Logic / Bitwise", true,
 "Sign-preserving right shift.",
 "dest = value >> shift",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"sll", "Shift Logical Left", "Logic / Bitwise", true,
 "Left shift.",
 "dest = value << shift",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"slr", "Shift Logical Right", "Logic / Bitwise", true,
 "Zero-filling right shift.",
 "dest = value >>> shift",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"rol", "Rotate Left", "Logic / Bitwise", true,
 "Rotates bits to the left.",
 "dest = rotate_left(value, shift)",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"ror", "Rotate Right", "Logic / Bitwise", true,
 "Rotates bits to the right.",
 "dest = rotate_right(value, shift)",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

// ── Bit Manipulation ─────────────────────────────────────────────────────────
{"rvb", "Reverse Bits", "Bit Manipulation", true,
 "Reverses the bit order of a 32-bit value.",
 "dest = reverse_bits(value)",
 {OK::RegW, OK::RegRInt}},

{"ppc", "Population Count", "Bit Manipulation", true,
 "Counts the number of set bits.",
 "dest = popcount(value)",
 {OK::RegW, OK::RegRInt}},

{"clz", "Count Leading Zeros", "Bit Manipulation", true,
 "Counts leading zero bits.",
 "dest = clz(value)",
 {OK::RegW, OK::RegRInt}},

{"ctz", "Count Trailing Zeros", "Bit Manipulation", true,
 "Counts trailing zero bits.",
 "dest = ctz(value)",
 {OK::RegW, OK::RegRInt}},

{"sbx", "Bit Field Extract (Signed)", "Bit Manipulation", false,
 "Extracts a bit field and sign-extends it.",
 "dest = sign_extend(value[offset+width-1 : offset])",
 {OK::RegW, OK::RegRInt, OK::AnyExpr, OK::AnyExpr}},

{"ubx", "Bit Field Extract (Unsigned)", "Bit Manipulation", false,
 "Extracts a bit field without sign extension.",
 "dest = value[offset+width-1 : offset]",
 {OK::RegW, OK::RegRInt, OK::AnyExpr, OK::AnyExpr}},

{"bfi", "Bit Field Insert", "Bit Manipulation", false,
 "Inserts bits from source into dest at the given offset.",
 "dest[offset+width-1 : offset] = source[width-1 : 0]",
 {OK::RegW, OK::RegRInt, OK::AnyExpr, OK::AnyExpr}},

{"pbx", "Parallel Bit Extract", "Bit Manipulation", false,
 "Extracts bits selected by mask and packs them.",
 "dest = pext(value, mask)",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"pbd", "Parallel Bit Deposit", "Bit Manipulation", false,
 "Deposits bits from value into positions selected by mask.",
 "dest = pdep(value, mask)",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

// ── Control Flow ──────────────────────────────────────────────────────────────
{"nop", "No Operation", "Control Flow", false,
 "Does nothing. Useful for padding.",
 "—",
 {}},

{"cal", "Call", "Control Flow", false,
 "Pushes the return address and fp, then jumps to a pc-relative address.",
 "push(pc+4); push(fp); pc = address",
 {OK::LblOrExpr}},

{"ret", "Return", "Control Flow", false,
 "Restores fp and pc from the stack.",
 "fp = pop(); pc = pop()",
 {}},

{"jmp", "Jump", "Control Flow", false,
 "Unconditional jump to a pc-relative address.",
 "pc = address",
 {OK::LblOrExpr}},

{"jtr", "Jump If True", "Control Flow", false,
 "Jumps if the cr register is set (non-zero).",
 "if (cr): pc = address",
 {OK::LblOrExpr}},

{"jfs", "Jump If False", "Control Flow", false,
 "Jumps if the cr register is clear (zero).",
 "if (!cr): pc = address",
 {OK::LblOrExpr}},

// ── System ────────────────────────────────────────────────────────────────────
{"syscall", "System Call", "System", false,
 "Invokes a system call by name.",
 "kernel_call(SyscallName)",
 {OK::SysK}},

{"break", "Break", "System", false,
 "Transfers control to the debugger.",
 "debug_trap()",
 {}},

{"yield", "Yield", "System", false,
 "Suspends the current process and places it at the end of the scheduler queue.",
 "suspend(); schedule_next()",
 {}},

{"exit", "Exit", "System", false,
 "Terminates the current process instance.",
 "process_exit()",
 {}},

// ── Drawing ───────────────────────────────────────────────────────────────────
{"gbpx", "Get Back Buffer Pixel", "Drawing", false,
 "Reads a luma byte from the back buffer at (x, y).",
 "dest = M[ba + y * SCREEN_WIDTH + x]",
 {OK::RegW, OK::RegRInt, OK::RegRInt}},

{"sbpx", "Set Back Buffer Pixel", "Drawing", false,
 "Writes a luma byte to the back buffer at (x, y).",
 "M[ba + y * SCREEN_WIDTH + x] = luma",
 {OK::RegRInt, OK::RegRInt, OK::RegRInt}},

{"gtpx", "Get Texture Pixel", "Drawing", false,
 "Reads a luma byte from a texture at pa-relative address, at (x, y).",
 "dest = texture[y * width + x]",
 {OK::RegW, OK::LblOrExpr, OK::RegRInt, OK::RegRInt}},

{"stpx", "Set Texture Pixel", "Drawing", false,
 "Writes a luma byte to a texture at pa-relative address, at (x, y).",
 "texture[y * width + x] = luma",
 {OK::LblOrExpr, OK::RegRInt, OK::RegRInt, OK::RegRInt}},

{"norm", "Normalize", "Drawing", true,
 "Converts a luma byte [1,255] to a float [0.0,1.0].",
 "dest = clamp(value-1, 0, 254) / 254.0",
 {OK::RegW, OK::RegRInt}},

{"dnrm", "Denormalize", "Drawing", true,
 "Converts a float [0.0,1.0] to a luma byte [1,255].",
 "dest = clamp((int)(value*254.0+1.0), 1, 255)",
 {OK::RegW, OK::RegRFlt}},

// ── Vector ────────────────────────────────────────────────────────────────────
{"vpsh", "Vector Push", "Vector", false,
 "Pushes a contiguous register range onto the stack.",
 "for r in range: push(r)",
 {OK::RegRng}},

{"vpop", "Vector Pop", "Vector", false,
 "Pops a contiguous register range from the stack.",
 "for r in range: r = pop()",
 {OK::RegRng}},

{"vmov", "Vector Move", "Vector", false,
 "Copies a register range to another.",
 "for i: dest[i] = src[i]",
 {OK::RegRng, OK::RegRng}},

{"vfadd", "Vector Float Add", "Vector", true,
 "Element-wise float add across a register range.",
 "for i: dest[i] = a[i] + b",
 {OK::RegRng, OK::RegRngS, OK::RegRFlt}},

{"vfsub", "Vector Float Subtract", "Vector", true,
 "Element-wise float subtract across a register range.",
 "for i: dest[i] = a[i] - b",
 {OK::RegRng, OK::RegRngS, OK::RegRFlt}},

{"vfmul", "Vector Float Multiply", "Vector", true,
 "Element-wise float multiply across a register range.",
 "for i: dest[i] = a[i] * b",
 {OK::RegRng, OK::RegRngS, OK::RegRFlt}},

{"vffma", "Vector Float FMA", "Vector", true,
 "Element-wise fused multiply-add across a register range.",
 "for i: dest[i] = a[i] * b + c",
 {OK::RegRng, OK::RegRngS, OK::RegRFlt, OK::RegRFlt}},

{"vfdiv", "Vector Float Divide", "Vector", true,
 "Element-wise float divide across a register range.",
 "for i: dest[i] = a[i] / b",
 {OK::RegRng, OK::RegRngS, OK::RegRFlt}},
