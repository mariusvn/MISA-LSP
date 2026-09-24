#include "kb/KnowledgeBase.h"
#include <limits>

namespace misa::kb {

const KnowledgeBase& KnowledgeBase::get() {
    static KnowledgeBase instance;
    return instance;
}

KnowledgeBase::KnowledgeBase() {
    // ── Instructions ─────────────────────────────────────────────────────────
    {
        using OK = OperandKind;
        m_instructions = {
#include "Instructions.inc"
        };
    }
    for (size_t i = 0; i < m_instructions.size(); ++i)
        m_instrIdx[std::string(m_instructions[i].mnemonic)] = i;

    // ── Registers ─────────────────────────────────────────────────────────────
    // Special-purpose registers
    static const RegisterInfo specials[] = {
        {"zr", "special", "Zero register. Always reads 0; writes are silently discarded.", false},
        {"cr", "special", "Comparison Result. Set by `cmp`; read by `jtr`, `jfs`, `mvc`, `sel`.", false},
        {"ea", "special", "Effective Address. Set by `cea`; used by `lde`, `ste`.", false},
        {"pa", "special", "Program Address (read-only). Base address of the program in memory.", true},
        {"ba", "special", "Back Buffer Address (read-only). Base address of the back buffer.", true},
        {"sp", "special", "Stack Pointer. Managed by `psh`/`pop`/`cal`/`ret`. Advanced use only.", false},
        {"fp", "special", "Frame Pointer. Managed by `cal`/`ret`. Advanced use only.", false},
        {"pc", "special", "Program Counter. Managed by the VM. Do NOT write manually.", true},
    };

    // GPR names need stable storage since RegisterInfo holds string_views.
    static std::vector<std::string> gprNames;
    if (gprNames.empty()) {
        for (int i = 0; i <= 15; ++i) gprNames.push_back("t" + std::to_string(i));
        for (int i = 0; i <= 15; ++i) gprNames.push_back("a" + std::to_string(i));
        for (int i = 0; i <= 31; ++i) gprNames.push_back("s" + std::to_string(i));
    }

    for (int i = 0; i <= 15; ++i)
        m_registers.push_back({gprNames[i], "temporary",
            "Caller-saved. Free to use as scratch.", false});
    for (int i = 0; i <= 15; ++i)
        m_registers.push_back({gprNames[16+i], "argument",
            "Caller-saved. Function arguments and return values.", false});
    for (int i = 0; i <= 31; ++i)
        m_registers.push_back({gprNames[32+i], "saved",
            "Callee-saved. Must be preserved by called functions.", false});
    for (const auto& r : specials)
        m_registers.push_back(r);

    for (size_t i = 0; i < m_registers.size(); ++i)
        m_regIdx[std::string(m_registers[i].name)] = i;

    // ── Syscalls ──────────────────────────────────────────────────────────────
    m_syscalls = {
#include "Syscalls.inc"
    };
    for (size_t i = 0; i < m_syscalls.size(); ++i)
        m_syscallIdx[std::string(m_syscalls[i].name)] = i;

    // ── Types ─────────────────────────────────────────────────────────────────
    m_types = {
        {"i8t",    "Signed 8-bit integer",                           false},
        {"u8t",    "Unsigned 8-bit integer",                         false},
        {"i16t",   "Signed 16-bit integer",                          false},
        {"u16t",   "Unsigned 16-bit integer",                        false},
        {"i32t",   "Signed 32-bit integer",                          false},
        {"u32t",   "Unsigned 32-bit integer",                        false},
        {"f32t",   "32-bit IEEE 754 float",                          false},
        {"string", "Null-terminated ASCII string (embed only)",      true},
        {"file",   ".png (→ luma texture) or .bin file (embed only)", true},
    };
    for (size_t i = 0; i < m_types.size(); ++i)
        m_typeIdx[std::string(m_types[i].name)] = i;

    // ── Conditions ────────────────────────────────────────────────────────────
    m_conditions = {
        {"eq",    "Equal (a == b)",                                false},
        {"neq",   "Not Equal (a != b)",                            false},
        {"lt",    "Less Than signed (a < b)",                      false},
        {"lte",   "Less Than or Equal signed (a <= b)",            false},
        {"gt",    "Greater Than signed (a > b)",                   false},
        {"gte",   "Greater Than or Equal signed (a >= b)",         false},
        {"ltu",   "Less Than unsigned (a < b)",                    false},
        {"lteu",  "Less Than or Equal unsigned (a <= b)",          false},
        {"gtu",   "Greater Than unsigned (a > b)",                 false},
        {"gteu",  "Greater Than or Equal unsigned (a >= b)",       false},
        {"feqa",  "Float Equal Approx. (|b - a| < ε)",            true},
        {"fneqa", "Float Not Equal Approx. (|b - a| >= ε)",       true},
        {"flt",   "Float Less Than (a < b)",                       true},
        {"fgt",   "Float Greater Than (a > b)",                    true},
        {"fnan",  "Float is NaN",                                   true},
        {"finf",  "Float is Infinity",                              true},
    };
    for (size_t i = 0; i < m_conditions.size(); ++i)
        m_condIdx[std::string(m_conditions[i].name)] = i;

    // ── Built-in symbols ──────────────────────────────────────────────────────
    m_builtins = {
        {"true",         false,  1.0,               "Boolean true constant"},
        {"false",        false,  0.0,               "Boolean false constant"},
        {"PI",           true,   3.14159265358979,  "π — ratio of circumference to diameter"},
        {"TAU",          true,   6.28318530717959,  "τ = 2π"},
        {"EXP1",         true,   2.71828182845905,  "e — Euler's number"},
        {"INF",          true,   std::numeric_limits<double>::infinity(),  "Positive infinity"},
        {"NAN",          true,   std::numeric_limits<double>::quiet_NaN(), "Not a Number"},
        {"SCREEN_WIDTH", false,  320.0,             "Screen width in pixels"},
        {"SCREEN_HEIGHT",false,  240.0,             "Screen height in pixels"},
        {"BTN_SELECT",   false,  512.0,             "Button bitmask: SELECT"},
        {"BTN_START",    false,  256.0,             "Button bitmask: START"},
        {"BTN_LEFT",     false,  128.0,             "Button bitmask: LEFT"},
        {"BTN_RIGHT",    false,  64.0,              "Button bitmask: RIGHT"},
        {"BTN_UP",       false,  32.0,              "Button bitmask: UP"},
        {"BTN_DOWN",     false,  16.0,              "Button bitmask: DOWN"},
        {"BTN_A",        false,  8.0,               "Button bitmask: A"},
        {"BTN_B",        false,  4.0,               "Button bitmask: B"},
        {"BTN_X",        false,  2.0,               "Button bitmask: X"},
        {"BTN_Y",        false,  1.0,               "Button bitmask: Y"},
        {"MAX_TERMINAL_INPUT_SIZE", false, 256.0,   "Maximum terminal input string size in bytes, including the null byte"},
        {"MOUSE_BTN_LEFT",       false, 1.0,        "Mouse button bitmask: LEFT"},
        {"MOUSE_BTN_RIGHT",      false, 2.0,        "Mouse button bitmask: RIGHT"},
        {"MOUSE_BTN_MIDDLE",     false, 4.0,        "Mouse button bitmask: MIDDLE"},
        {"MOUSE_BTN_WHEEL_UP",   false, 8.0,        "Mouse button bitmask: WHEEL UP"},
        {"MOUSE_BTN_WHEEL_DOWN", false, 16.0,       "Mouse button bitmask: WHEEL DOWN"},
        {"KBE_PRESSED",  false,  1.0,               "Keyboard event flag: key pressed (clear = released)"},
        {"KBE_REPEAT",   false,  2.0,               "Keyboard event flag: repeat event"},
        {"KBE_CTRL",     false,  4.0,               "Keyboard event flag: Ctrl modifier held"},
        {"KBE_SHIFT",    false,  8.0,               "Keyboard event flag: Shift modifier held"},
        {"KBE_ALT",      false,  16.0,              "Keyboard event flag: Alt modifier held"},
        {"KEY_TAB",      false,  128.0,             "Key code 0x80: Tab"},
        {"KEY_BACKSPACE",false,  129.0,             "Key code 0x81: Backspace"},
        {"KEY_ENTER",    false,  130.0,             "Key code 0x82: Enter"},
        {"KEY_ESC",      false,  131.0,             "Key code 0x83: Escape"},
        {"KEY_CTRL",     false,  132.0,             "Key code 0x84: Ctrl"},
        {"KEY_SHIFT",    false,  133.0,             "Key code 0x85: Shift"},
        {"KEY_ALT",      false,  134.0,             "Key code 0x86: Alt"},
        {"KEY_LEFT",     false,  135.0,             "Key code 0x87: Left arrow"},
        {"KEY_RIGHT",    false,  136.0,             "Key code 0x88: Right arrow"},
        {"KEY_UP",       false,  137.0,             "Key code 0x89: Up arrow"},
        {"KEY_DOWN",     false,  138.0,             "Key code 0x8a: Down arrow"},
        {"KEY_INSERT",   false,  139.0,             "Key code 0x8b: Insert"},
        {"KEY_DELETE",   false,  140.0,             "Key code 0x8c: Delete"},
        {"KEY_HOME",     false,  141.0,             "Key code 0x8d: Home"},
        {"KEY_END",      false,  142.0,             "Key code 0x8e: End"},
        {"KEY_PAGE_UP",  false,  143.0,             "Key code 0x8f: Page Up"},
        {"KEY_PAGE_DOWN",false,  144.0,             "Key code 0x90: Page Down"},
        {"$",            false,  0.0,               "Current assembler address (pa-relative)"},
    };
    for (size_t i = 0; i < m_builtins.size(); ++i)
        m_builtinIdx[std::string(m_builtins[i].name)] = i;
}

const InstructionInfo* KnowledgeBase::lookupInstruction(std::string_view mn) const {
    auto it = m_instrIdx.find(std::string(mn));
    return it != m_instrIdx.end() ? &m_instructions[it->second] : nullptr;
}
const RegisterInfo* KnowledgeBase::lookupRegister(std::string_view name) const {
    auto it = m_regIdx.find(std::string(name));
    return it != m_regIdx.end() ? &m_registers[it->second] : nullptr;
}
const SyscallInfo* KnowledgeBase::lookupSyscall(std::string_view name) const {
    auto it = m_syscallIdx.find(std::string(name));
    return it != m_syscallIdx.end() ? &m_syscalls[it->second] : nullptr;
}
const TypeInfo* KnowledgeBase::lookupType(std::string_view name) const {
    auto it = m_typeIdx.find(std::string(name));
    return it != m_typeIdx.end() ? &m_types[it->second] : nullptr;
}
const ConditionInfo* KnowledgeBase::lookupCondition(std::string_view name) const {
    auto it = m_condIdx.find(std::string(name));
    return it != m_condIdx.end() ? &m_conditions[it->second] : nullptr;
}
const BuiltinSymbol* KnowledgeBase::lookupBuiltin(std::string_view name) const {
    auto it = m_builtinIdx.find(std::string(name));
    return it != m_builtinIdx.end() ? &m_builtins[it->second] : nullptr;
}

} // namespace misa::kb
