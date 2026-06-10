#include <catch2/catch_test_macros.hpp>
#include "lang/Lexer.h"
#include "lang/Parser.h"

using namespace misa::lang;

static std::vector<Statement> parse(const std::string& src) {
    Lexer lexer(src);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

TEST_CASE("Parser: empty source", "[parser]") {
    auto stmts = parse("");
    REQUIRE(stmts.empty());
}

TEST_CASE("Parser: global label", "[parser]") {
    auto stmts = parse("foo:");
    REQUIRE(stmts.size() == 1);
    auto& lbl = std::get<LabelDefStmt>(stmts[0]);
    REQUIRE(lbl.name == "foo");
    REQUIRE(lbl.kind == LabelDefStmt::Kind::Global);
}

TEST_CASE("Parser: local label", "[parser]") {
    auto stmts = parse(".bar:");
    REQUIRE(stmts.size() == 1);
    auto& lbl = std::get<LabelDefStmt>(stmts[0]);
    REQUIRE(lbl.name == "bar");
    REQUIRE(lbl.kind == LabelDefStmt::Kind::Local);
}

TEST_CASE("Parser: reusable label", "[parser]") {
    auto stmts = parse("@loop:");
    REQUIRE(stmts.size() == 1);
    auto& lbl = std::get<LabelDefStmt>(stmts[0]);
    REQUIRE(lbl.name == "loop");
    REQUIRE(lbl.kind == LabelDefStmt::Kind::Reusable);
}

TEST_CASE("Parser: simple instruction no operands", "[parser]") {
    auto stmts = parse("exit");
    REQUIRE(stmts.size() == 1);
    auto& instr = std::get<InstructionStmt>(stmts[0]);
    REQUIRE(instr.mnemonic.text == "exit");
    REQUIRE(instr.operands.empty());
}

TEST_CASE("Parser: instruction with registers", "[parser]") {
    auto stmts = parse("add t0, t1, t2");
    REQUIRE(stmts.size() == 1);
    auto& instr = std::get<InstructionStmt>(stmts[0]);
    REQUIRE(instr.mnemonic.text == "add");
    REQUIRE(instr.operands.size() == 3);
    REQUIRE(instr.operands[0].regName == "t0");
    REQUIRE(instr.operands[1].regName == "t1");
    REQUIRE(instr.operands[2].regName == "t2");
}

TEST_CASE("Parser: compact form instruction", "[parser]") {
    auto stmts = parse("add t0, 1");
    auto& instr = std::get<InstructionStmt>(stmts[0]);
    REQUIRE(instr.operands.size() == 2);
}

TEST_CASE("Parser: def directive", "[parser]") {
    auto stmts = parse("def MY_CONST 42");
    REQUIRE(stmts.size() == 1);
    auto& def = std::get<DefDirective>(stmts[0]);
    REQUIRE(def.name == "MY_CONST");
    REQUIRE(!def.isLocal);
}

TEST_CASE("Parser: local def directive", "[parser]") {
    auto stmts = parse("def .LOCAL 10");
    auto& def = std::get<DefDirective>(stmts[0]);
    REQUIRE(def.name == "LOCAL");
    REQUIRE(def.isLocal);
}

TEST_CASE("Parser: undef directive", "[parser]") {
    auto stmts = parse("undef MY_CONST");
    REQUIRE(stmts.size() == 1);
    auto& undef = std::get<UndefDirective>(stmts[0]);
    REQUIRE(undef.name == "MY_CONST");
}

TEST_CASE("Parser: emb directive", "[parser]") {
    auto stmts = parse("emb u32t 42");
    auto& emb = std::get<EmbDirective>(stmts[0]);
    REQUIRE(emb.typeName == "u32t");
    REQUIRE(emb.values.size() == 1);
}

TEST_CASE("Parser: res directive", "[parser]") {
    auto stmts = parse("res u8t 1024");
    auto& res = std::get<ResDirective>(stmts[0]);
    REQUIRE(res.typeName == "u8t");
}

TEST_CASE("Parser: bmk directive", "[parser]") {
    auto stmts = parse("bmk \"Section\"");
    auto& bmk = std::get<BmkDirective>(stmts[0]);
    REQUIRE(!bmk.isSub);
    REQUIRE(bmk.label == "Section");
}

TEST_CASE("Parser: sbmk directive", "[parser]") {
    auto stmts = parse("sbmk \"Sub\"");
    auto& bmk = std::get<BmkDirective>(stmts[0]);
    REQUIRE(bmk.isSub);
}

TEST_CASE("Parser: doc comment", "[parser]") {
    auto stmts = parse("## My function description");
    REQUIRE(stmts.size() == 1);
    auto& dc = std::get<DocCommentStmt>(stmts[0]);
    REQUIRE(dc.text.find("My function") != std::string::npos);
}

TEST_CASE("Parser: multiple lines", "[parser]") {
    auto stmts = parse("_start:\n    mov t0, 0\n    exit\n");
    // label + 2 instructions
    REQUIRE(stmts.size() == 3);
}

TEST_CASE("Parser: register range operand", "[parser]") {
    auto stmts = parse("vpsh s0..s3");
    auto& instr = std::get<InstructionStmt>(stmts[0]);
    REQUIRE(instr.operands.size() == 1);
    REQUIRE(instr.operands[0].kind == OperandNode::Kind::Range);
    REQUIRE(instr.operands[0].range.regStart == "s0");
    REQUIRE(instr.operands[0].range.regEnd == "s3");
}

TEST_CASE("Parser: syscall instruction", "[parser]") {
    auto stmts = parse("syscall SYS_PRINT_INT");
    auto& instr = std::get<InstructionStmt>(stmts[0]);
    REQUIRE(instr.mnemonic.text == "syscall");
    REQUIRE(instr.operands[0].regName == "SYS_PRINT_INT");
}
