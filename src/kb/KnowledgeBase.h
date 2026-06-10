#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>

namespace misa::kb {

// ── Operand kinds ─────────────────────────────────────────────────────────────

enum class OperandKind : uint8_t {
    RegW,       // writable register destination
    RegR,       // readable register or immediate (context determines int vs float)
    RegRInt,    // readable register or integer immediate only
    RegRFlt,    // readable register or float immediate only
    RegRng,     // register range (s0..s2)
    RegRngS,    // register range start continuation (t4..)
    TypeK,      // type keyword (i8t, u8t, …)
    CondK,      // condition keyword (eq, lt, …)
    SysK,       // syscall name (SYS_*)
    LblOrExpr,  // label or assemble-time expression
    AnyExpr,    // any expression/literal
    StrLit,     // string literal (for bmk/sbmk)
};

// ── Instruction ───────────────────────────────────────────────────────────────

struct InstructionInfo {
    std::string_view mnemonic;
    std::string_view name;
    std::string_view category;
    bool             compact;     // supports [c] form (dest omitted)
    std::string_view description;
    std::string_view pseudocode;
    std::vector<OperandKind> operands; // base form operands
};

// ── Register ──────────────────────────────────────────────────────────────────

struct RegisterInfo {
    std::string_view name;
    std::string_view group;   // "temporary" | "argument" | "saved" | "special"
    std::string_view role;    // ABI description
    bool             readOnly; // writing is ignored or illegal
};

// ── Syscall ───────────────────────────────────────────────────────────────────

struct ArgInfo {
    std::string_view reg;
    std::string_view description;
    bool             isFloat = false;
};

struct SyscallInfo {
    std::string_view       name;
    std::string_view       description;
    std::vector<ArgInfo>   args;    // input arguments
    std::vector<ArgInfo>   returns; // return values
};

// ── Type ──────────────────────────────────────────────────────────────────────

struct TypeInfo {
    std::string_view name;
    std::string_view description;
    bool             embedOnly; // only for emb, not for lod/str/lde/ste
};

// ── Condition ─────────────────────────────────────────────────────────────────

struct ConditionInfo {
    std::string_view name;
    std::string_view description;
    bool             isFloat;
};

// ── Built-in symbol ───────────────────────────────────────────────────────────

struct BuiltinSymbol {
    std::string_view name;
    bool             isFloat;
    double           value;
    std::string_view note;
};

// ── KnowledgeBase ─────────────────────────────────────────────────────────────

class KnowledgeBase {
public:
    static const KnowledgeBase& get();

    const InstructionInfo* lookupInstruction(std::string_view mnemonic) const;
    const RegisterInfo*    lookupRegister(std::string_view name) const;
    const SyscallInfo*     lookupSyscall(std::string_view name) const;
    const TypeInfo*        lookupType(std::string_view name) const;
    const ConditionInfo*   lookupCondition(std::string_view name) const;
    const BuiltinSymbol*   lookupBuiltin(std::string_view name) const;

    bool isRegister(std::string_view name) const { return lookupRegister(name) != nullptr; }
    bool isInstruction(std::string_view name) const { return lookupInstruction(name) != nullptr; }
    bool isType(std::string_view name) const { return lookupType(name) != nullptr; }
    bool isCondition(std::string_view name) const { return lookupCondition(name) != nullptr; }
    bool isSyscall(std::string_view name) const { return lookupSyscall(name) != nullptr; }
    bool isBuiltin(std::string_view name) const { return lookupBuiltin(name) != nullptr; }

    // Lists for completion
    const std::vector<InstructionInfo>& instructions() const { return m_instructions; }
    const std::vector<RegisterInfo>&    registers()    const { return m_registers; }
    const std::vector<SyscallInfo>&     syscalls()     const { return m_syscalls; }
    const std::vector<TypeInfo>&        types()        const { return m_types; }
    const std::vector<ConditionInfo>&   conditions()   const { return m_conditions; }
    const std::vector<BuiltinSymbol>&   builtins()     const { return m_builtins; }

    // Built-in entry point names
    static constexpr std::string_view ENTRY_POINTS[] = {
        "_start", "_update", "_draw", "_input"
    };

private:
    KnowledgeBase();

    std::vector<InstructionInfo> m_instructions;
    std::vector<RegisterInfo>    m_registers;
    std::vector<SyscallInfo>     m_syscalls;
    std::vector<TypeInfo>        m_types;
    std::vector<ConditionInfo>   m_conditions;
    std::vector<BuiltinSymbol>   m_builtins;

    std::unordered_map<std::string, size_t> m_instrIdx;
    std::unordered_map<std::string, size_t> m_regIdx;
    std::unordered_map<std::string, size_t> m_syscallIdx;
    std::unordered_map<std::string, size_t> m_typeIdx;
    std::unordered_map<std::string, size_t> m_condIdx;
    std::unordered_map<std::string, size_t> m_builtinIdx;
};

} // namespace misa::kb
