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
    // Temporary t0-t15
    for (int i = 0; i <= 15; ++i)
        m_registers.push_back({"",  "temporary", "caller-saved; short-lived temporaries", false});
    // Argument a0-a15
    for (int i = 0; i <= 15; ++i)
        m_registers.push_back({"", "argument", "caller-saved; function arguments and return values", false});
    // Saved s0-s31
    for (int i = 0; i <= 31; ++i)
        m_registers.push_back({"", "saved", "callee-saved; must be preserved by called functions", false});

    // Fix names for GPRs (we stored them with empty names above; rebuild properly)
    m_registers.clear();
    for (int i = 0; i <= 15; ++i) {
        m_registers.push_back({std::string_view{}, "temporary",
            "Caller-saved. Free to use as scratch. No guarantee of preservation across calls.", false});
    }
    for (int i = 0; i <= 15; ++i) {
        m_registers.push_back({std::string_view{}, "argument",
            "Caller-saved. Pass function arguments and receive return values via a0, a1, …", false});
    }
    for (int i = 0; i <= 31; ++i) {
        m_registers.push_back({std::string_view{}, "saved",
            "Callee-saved. Called function must push/pop these if it uses them.", false});
    }

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
    for (const auto& r : specials)
        m_registers.push_back(r);

    // Build index: we need stable string keys, use a static array for GPR names
    static std::vector<std::string> gprNames;
    if (gprNames.empty()) {
        for (int i = 0; i <= 15; ++i) gprNames.push_back("t" + std::to_string(i));
        for (int i = 0; i <= 15; ++i) gprNames.push_back("a" + std::to_string(i));
        for (int i = 0; i <= 31; ++i) gprNames.push_back("s" + std::to_string(i));
    }

    // Fix string_views to point at stable storage, rebuild whole list properly
    m_registers.clear();
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
