#include "features/SignatureHelp.h"
#include "kb/KnowledgeBase.h"
#include <sstream>
#include <cctype>

namespace misa::features {

using namespace misa::lang;
using namespace misa::lsp;

static std::string operandKindLabel(kb::OperandKind k) {
    switch (k) {
        case kb::OperandKind::RegW:      return "dest";
        case kb::OperandKind::RegR:      return "src";
        case kb::OperandKind::RegRInt:   return "int_src";
        case kb::OperandKind::RegRFlt:   return "float_src";
        case kb::OperandKind::RegRng:    return "range";
        case kb::OperandKind::RegRngS:   return "range_start";
        case kb::OperandKind::TypeK:     return "Type";
        case kb::OperandKind::CondK:     return "Condition";
        case kb::OperandKind::SysK:      return "Syscall";
        case kb::OperandKind::LblOrExpr: return "label";
        case kb::OperandKind::AnyExpr:   return "expr";
        case kb::OperandKind::StrLit:    return "\"name\"";
        default:                         return "arg";
    }
}

std::optional<SignatureHelp> provideSignatureHelp(const Compilation& /*c*/, const SourceFile& f,
                                                  Position pos) {
    const auto& kb = kb::KnowledgeBase::get();

    auto lineText = f.doc().lineText(pos.line);
    uint32_t offset = f.doc().positionToOffset(pos);
    uint32_t lstart = f.doc().positionToOffset({pos.line, 0});
    uint32_t col    = offset - lstart;
    if (col > lineText.size()) col = (uint32_t)lineText.size();

    // Extract the mnemonic (first word on line, after an optional "label:")
    auto skipBlanks = [&](size_t k) {
        while (k < lineText.size() && (lineText[k] == ' ' || lineText[k] == '\t')) ++k;
        return k;
    };
    size_t i = skipBlanks(0);
    {
        size_t j = i;
        while (j < lineText.size() && (std::isalnum((unsigned char)lineText[j]) ||
               lineText[j] == '_' || lineText[j] == '.' || lineText[j] == '@')) ++j;
        if (j < lineText.size() && lineText[j] == ':') i = skipBlanks(j + 1);
    }
    size_t mnStart = i;
    while (i < lineText.size() && (std::isalnum((unsigned char)lineText[i]) || lineText[i] == '_'))
        ++i;
    std::string mnemonic = std::string(lineText.substr(mnStart, i - mnStart));

    if (mnemonic.empty()) return std::nullopt;

    // Count commas before cursor to determine active parameter
    uint32_t commas = 0;
    for (uint32_t j = (uint32_t)i; j < col; ++j)
        if (lineText[j] == ',') ++commas;

    // ── Syscall signature ─────────────────────────────────────────────────────
    if (mnemonic == "syscall") {
        // Try to find the syscall name already typed
        while (i < lineText.size() && (lineText[i] == ' ' || lineText[i] == '\t')) ++i;
        size_t snStart = i;
        while (i < lineText.size() && (std::isalnum((unsigned char)lineText[i]) || lineText[i] == '_'))
            ++i;
        std::string syscallName = std::string(lineText.substr(snStart, i - snStart));

        const kb::SyscallInfo* sys = kb.lookupSyscall(syscallName);
        if (!sys) return std::nullopt;

        SignatureInformation sig;
        sig.label = std::string(sys->name);
        sig.documentation = std::string(sys->description);

        for (size_t idx = 0; idx < sys->args.size(); ++idx) {
            const auto& a = sys->args[idx];
            ParameterInformation param;
            param.label = std::string(a.reg) + ": " + std::string(a.description);
            if (a.isFloat) param.documentation = "(float)";
            sig.parameters.push_back(std::move(param));
        }
        if (!sys->returns.empty()) {
            ParameterInformation ret;
            ret.label = "→ " + std::string(sys->returns[0].reg) + ": " +
                        std::string(sys->returns[0].description);
            sig.parameters.push_back(std::move(ret));
        }

        SignatureHelp help;
        help.signatures = {std::move(sig)};
        help.activeSignature = 0;
        help.activeParameter = commas < sig.parameters.size() ? commas : 0;
        return help;
    }

    // ── Instruction signature ─────────────────────────────────────────────────
    const auto* info = kb.lookupInstruction(mnemonic);
    if (!info || info->operands.empty()) return std::nullopt;

    // Build base form signature
    std::ostringstream label;
    label << info->mnemonic;
    std::vector<ParameterInformation> params;
    for (size_t idx = 0; idx < info->operands.size(); ++idx) {
        std::string opLabel = operandKindLabel(info->operands[idx]);
        label << (idx == 0 ? " " : ", ") << opLabel;
        ParameterInformation p;
        p.label = opLabel;
        params.push_back(std::move(p));
    }

    SignatureInformation sig;
    sig.label = label.str();
    sig.documentation = std::string(info->description) +
                        "\n\n```\n" + std::string(info->pseudocode) + "\n```";
    sig.parameters = std::move(params);

    // Compact form signature
    SignatureInformation sigCompact;
    bool hasCompact = info->compact && info->operands.size() >= 2;
    if (hasCompact) {
        std::ostringstream clabel;
        clabel << info->mnemonic;
        std::vector<ParameterInformation> cparams;
        for (size_t idx = 1; idx < info->operands.size(); ++idx) {
            std::string opLabel = operandKindLabel(info->operands[idx]);
            clabel << (idx == 1 ? " " : ", ") << opLabel;
            ParameterInformation p;
            p.label = opLabel + " (compact: dest = first operand)";
            cparams.push_back(std::move(p));
        }
        sigCompact.label = clabel.str() + "  [compact]";
        sigCompact.documentation = sig.documentation;
        sigCompact.parameters = std::move(cparams);
    }

    SignatureHelp help;
    help.signatures.push_back(std::move(sig));
    if (hasCompact) help.signatures.push_back(std::move(sigCompact));
    help.activeSignature = 0;
    help.activeParameter = commas < help.signatures[0].parameters.size()
                         ? commas : (uint32_t)help.signatures[0].parameters.size() - 1;
    return help;
}

} // namespace misa::features
