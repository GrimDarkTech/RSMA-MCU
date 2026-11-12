#include <clang-c/Index.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "rsma_mcu/firmware_format.h"

namespace {

using namespace rsma_mcu;

class ClangString final
{
public:
    explicit ClangString(CXString value) : value_(value) {}
    ~ClangString() { clang_disposeString(value_); }

    std::string str() const
    {
        const auto* text = clang_getCString(value_);
        return text == nullptr ? std::string{} : std::string(text);
    }

private:
    CXString value_;
};

struct AstContext
{
    CXCursor mainFunction = clang_getNullCursor();
    unsigned mainDefinitions = 0;
    std::exception_ptr failure;
};

CXChildVisitResult find_main(CXCursor cursor, CXCursor, CXClientData clientData)
{
    auto& context = *static_cast<AstContext*>(clientData);
    try {
        if (clang_getCursorKind(cursor) == CXCursor_FunctionDecl &&
            clang_getCursorKind(clang_getCursorLexicalParent(cursor)) == CXCursor_TranslationUnit &&
            ClangString(clang_getCursorSpelling(cursor)).str() == "main" &&
            clang_isCursorDefinition(cursor)) {
            context.mainFunction = cursor;
            ++context.mainDefinitions;
        }
    } catch (...) {
        context.failure = std::current_exception();
        return CXChildVisit_Break;
    }
    return CXChildVisit_Continue;
}

void append_u32(Bytes& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<Byte>(value));
    bytes.push_back(static_cast<Byte>(value >> 8));
    bytes.push_back(static_cast<Byte>(value >> 16));
    bytes.push_back(static_cast<Byte>(value >> 24));
}

void append_hal_call(BytecodeProgram& program, HalService service, Byte argument, Byte value)
{
    const auto id = static_cast<std::uint16_t>(service);
    program.instructions.push_back({Opcode::HalCall, {
        static_cast<Byte>(id), static_cast<Byte>(id >> 8), argument, value,
    }});
}

std::string cursor_location(CXCursor cursor)
{
    CXFile file = nullptr;
    unsigned line = 0;
    unsigned column = 0;
    clang_getSpellingLocation(clang_getCursorLocation(cursor), &file, &line, &column, nullptr);
    if (file == nullptr) {
        return "source";
    }
    return ClangString(clang_getFileName(file)).str() + ":" + std::to_string(line) + ":" +
        std::to_string(column);
}

[[noreturn]] void fail_at(CXCursor cursor, const std::string& message)
{
    throw FirmwareError(cursor_location(cursor) + ": " + message);
}

std::int64_t constant_integer(CXCursor cursor)
{
    CXEvalResult result = clang_Cursor_Evaluate(cursor);
    if (result == nullptr) {
        fail_at(cursor, "expected a compile-time integer constant");
    }
    const auto kind = clang_EvalResult_getKind(result);
    if (kind != CXEval_Int) {
        clang_EvalResult_dispose(result);
        fail_at(cursor, "expected an integer compile-time constant");
    }
    const auto value = static_cast<std::int64_t>(clang_EvalResult_getAsLongLong(result));
    clang_EvalResult_dispose(result);
    return value;
}

struct CallChildren
{
    std::vector<CXCursor> cursors;
};

CXChildVisitResult collect_call_children(CXCursor cursor, CXCursor, CXClientData clientData)
{
    static_cast<CallChildren*>(clientData)->cursors.push_back(cursor);
    return CXChildVisit_Continue;
}

struct ResolvedFunction
{
    CXCursor declaration = clang_getNullCursor();
    std::string name;
};

ResolvedFunction resolve_called_function(CXCursor call);
bool is_builtin_hal_declaration(CXCursor declaration);

struct BodyContext
{
    BytecodeProgram* program;
    std::vector<std::string>* peripherals;
    std::vector<std::pair<CXCursor, std::uint16_t>> locals;
    std::uint16_t nextSlot = 0;
    bool terminated = false;
    std::exception_ptr failure;
};

CXChildVisitResult lower_statement(CXCursor cursor, CXCursor parent, CXClientData clientData);

std::uint32_t current_instruction(const BodyContext& context)
{
    if (context.program->instructions.size() >= kMaxInstructionCount) {
        throw FirmwareError("bytecode instruction limit exceeded");
    }
    return static_cast<std::uint32_t>(context.program->instructions.size());
}

std::uint16_t local_slot(CXCursor declaration, BodyContext& context)
{
    for (const auto& local : context.locals) {
        if (clang_equalCursors(local.first, declaration)) return local.second;
    }
    fail_at(declaration, "variable is not a supported local in main()");
}

void emit_const(BytecodeProgram& program, std::int32_t value)
{
    Bytes operands;
    append_u32(operands, static_cast<std::uint32_t>(value));
    program.instructions.push_back({Opcode::ConstI32, std::move(operands)});
}

void emit_slot(BytecodeProgram& program, Opcode opcode, std::uint16_t slot)
{
    program.instructions.push_back({opcode, {
        static_cast<Byte>(slot), static_cast<Byte>(slot >> 8),
    }});
}

std::vector<CXCursor> cursor_children(CXCursor cursor)
{
    CallChildren children;
    clang_visitChildren(cursor, collect_call_children, &children);
    return std::move(children.cursors);
}

void emit_expression(CXCursor cursor, BodyContext& context)
{
    const auto kind = clang_getCursorKind(cursor);
    if (kind == CXCursor_IntegerLiteral || kind == CXCursor_CXXBoolLiteralExpr ||
        kind == CXCursor_CharacterLiteral) {
        const auto value = constant_integer(cursor);
        if (value < std::numeric_limits<std::int32_t>::min() ||
            value > std::numeric_limits<std::int32_t>::max()) {
            fail_at(cursor, "integer expression is outside the VM int32 range");
        }
        emit_const(*context.program, static_cast<std::int32_t>(value));
        return;
    }
    if (kind == CXCursor_DeclRefExpr) {
        const auto declaration = clang_getCursorReferenced(cursor);
        if (clang_Cursor_isNull(declaration)) fail_at(cursor, "unresolved variable reference");
        emit_slot(*context.program, Opcode::Load, local_slot(declaration, context));
        return;
    }
    if (kind == CXCursor_ParenExpr || kind == CXCursor_UnexposedExpr) {
        const auto children = cursor_children(cursor);
        if (children.size() != 1) fail_at(cursor, "unsupported expression wrapper");
        emit_expression(children.front(), context);
        return;
    }
    if (kind == CXCursor_CallExpr) {
        const auto function = resolve_called_function(cursor);
        if (function.name != "digitalRead" || !is_builtin_hal_declaration(function.declaration)) {
            fail_at(cursor, "only digitalRead() may be used as a value expression");
        }
        if (clang_Cursor_getNumArguments(cursor) != 1) fail_at(cursor, "digitalRead expects one argument");
        const auto pin = constant_integer(clang_Cursor_getArgument(cursor, 0));
        if (pin < 0 || pin > std::numeric_limits<Byte>::max()) {
            fail_at(clang_Cursor_getArgument(cursor, 0), "pin must be between 0 and 255");
        }
        append_hal_call(*context.program, HalService::DigitalRead, static_cast<Byte>(pin), 0);
        context.peripherals->emplace_back("GPIO");
        return;
    }
    if (kind == CXCursor_UnaryOperator) {
        const auto children = cursor_children(cursor);
        if (children.size() != 1) fail_at(cursor, "unsupported unary expression");
        const auto operation = ClangString(clang_getUnaryOperatorKindSpelling(
            clang_getCursorUnaryOperatorKind(cursor))).str();
        if (operation == "!") {
            emit_expression(children.front(), context);
            emit_const(*context.program, 0);
            context.program->instructions.push_back({Opcode::CmpEqI32, {}});
            return;
        }
        if (operation == "-") {
            emit_const(*context.program, 0);
            emit_expression(children.front(), context);
            context.program->instructions.push_back({Opcode::SubI32, {}});
            return;
        }
        fail_at(cursor, "unsupported unary operator '" + operation + "'");
    }
    if (kind == CXCursor_BinaryOperator) {
        const auto children = cursor_children(cursor);
        if (children.size() != 2) fail_at(cursor, "unsupported binary expression");
        emit_expression(children[0], context);
        emit_expression(children[1], context);
        const auto operation = clang_getCursorBinaryOperatorKind(cursor);
        Opcode opcode;
        switch (operation) {
        case CXBinaryOperator_Add: opcode = Opcode::AddI32; break;
        case CXBinaryOperator_Sub: opcode = Opcode::SubI32; break;
        case CXBinaryOperator_EQ: opcode = Opcode::CmpEqI32; break;
        case CXBinaryOperator_NE: opcode = Opcode::CmpNeI32; break;
        case CXBinaryOperator_LT: opcode = Opcode::CmpLtI32; break;
        case CXBinaryOperator_LE: opcode = Opcode::CmpLeI32; break;
        case CXBinaryOperator_GT: opcode = Opcode::CmpGtI32; break;
        case CXBinaryOperator_GE: opcode = Opcode::CmpGeI32; break;
        default:
            fail_at(cursor, "unsupported binary operator '" +
                ClangString(clang_getBinaryOperatorKindSpelling(operation)).str() + "'");
        }
        context.program->instructions.push_back({opcode, {}});
        return;
    }
    fail_at(cursor, "unsupported value expression");
}

void emit_jump(BytecodeProgram& program, Opcode opcode, std::uint32_t target)
{
    Bytes operands;
    append_u32(operands, target);
    program.instructions.push_back({opcode, std::move(operands)});
}

void patch_jump_target(BytecodeInstruction& instruction, std::uint32_t target);

std::size_t emit_jump_placeholder(BytecodeProgram& program, Opcode opcode)
{
    const auto index = program.instructions.size();
    program.instructions.push_back({opcode, Bytes(4, 0)});
    return index;
}

void patch_jump(BytecodeProgram& program, std::size_t instruction, std::uint32_t target)
{
    patch_jump_target(program.instructions.at(instruction), target);
}

void lower_increment(CXCursor cursor, BodyContext& context)
{
    const auto children = cursor_children(cursor);
    if (children.size() != 1 || clang_getCursorKind(children[0]) != CXCursor_DeclRefExpr)
        fail_at(cursor, "for increment must target a local variable");

    const auto declaration = clang_getCursorReferenced(children[0]);
    const auto operation = ClangString(clang_getUnaryOperatorKindSpelling(
        clang_getCursorUnaryOperatorKind(cursor))).str();
    if (operation != "++" && operation != "--")
        fail_at(cursor, "for increment supports only ++ and --");

    const auto slot = local_slot(declaration, context);
    emit_slot(*context.program, Opcode::Load, slot);
    emit_const(*context.program, 1);
    context.program->instructions.push_back({
        operation == "++" ? Opcode::AddI32 : Opcode::SubI32, {}});
    emit_slot(*context.program, Opcode::Store, slot);
}

ResolvedFunction resolve_called_function(CXCursor call)
{
    const auto referenced = clang_getCursorReferenced(call);
    if (!clang_Cursor_isNull(referenced)) {
        return {referenced, ClangString(clang_getCursorSpelling(referenced)).str()};
    }

    CallChildren children;
    clang_visitChildren(call, collect_call_children, &children);
    for (const auto child : children.cursors) {
        if (clang_getCursorKind(child) == CXCursor_DeclRefExpr) {
            const auto declaration = clang_getCursorReferenced(child);
            if (!clang_Cursor_isNull(declaration)) {
                return {declaration, ClangString(clang_getCursorSpelling(declaration)).str()};
            }
        }
    }
    return {};
}

std::string normalized_path(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return path;
}

bool is_builtin_hal_declaration(CXCursor declaration)
{
    CXFile file = nullptr;
    clang_getSpellingLocation(clang_getCursorLocation(declaration), &file, nullptr, nullptr, nullptr);
    if (file == nullptr) return false;
    const auto declarationPath = ClangString(clang_getFileName(file)).str();
    return normalized_path(declarationPath) == normalized_path(RSMA_MCU_ARDUINO_HEADER);
}

void lower_call(CXCursor cursor, BodyContext& context)
{
    auto& program = *context.program;
    auto& peripherals = *context.peripherals;
    const auto function = resolve_called_function(cursor);
    const auto& name = function.name;
    if (name != "pinMode" && name != "digitalWrite" && name != "digitalRead" && name != "delay") {
        fail_at(cursor, "unsupported function call '" + (name.empty() ? std::string("<unknown>") : name) + "'");
    }
    if (!is_builtin_hal_declaration(function.declaration)) {
        fail_at(cursor, "HAL call must resolve to the declaration in <rsma_mcu/Arduino.h>");
    }
    const auto argumentCount = clang_Cursor_getNumArguments(cursor);
    auto argument = [&](int index) {
        if (index >= argumentCount) {
            fail_at(cursor, name + " has too few arguments");
        }
        return clang_Cursor_getArgument(cursor, static_cast<unsigned>(index));
    };
    auto byteArgument = [&](int index, std::string_view description) {
        const auto value = constant_integer(argument(index));
        if (value < 0 || value > std::numeric_limits<Byte>::max()) {
            fail_at(argument(index), std::string(description) + " must be between 0 and 255");
        }
        return static_cast<Byte>(value);
    };

    if (name == "pinMode") {
        if (argumentCount != 2) fail_at(cursor, "pinMode expects two arguments");
        const auto pin = byteArgument(0, "pin");
        const auto mode = byteArgument(1, "pin mode");
        if (mode > 1) fail_at(argument(1), "pinMode supports only INPUT and OUTPUT");
        append_hal_call(program, HalService::PinMode, pin, mode);
        peripherals.emplace_back("GPIO");
        return;
    }
    if (name == "digitalWrite") {
        if (argumentCount != 2) fail_at(cursor, "digitalWrite expects two arguments");
        const auto pin = byteArgument(0, "pin");
        const auto value = byteArgument(1, "digital value");
        if (value > 1) fail_at(argument(1), "digitalWrite supports only LOW and HIGH");
        append_hal_call(program, HalService::DigitalWrite, pin, value);
        peripherals.emplace_back("GPIO");
        return;
    }
    if (name == "digitalRead") {
        if (argumentCount != 1) fail_at(cursor, "digitalRead expects one argument");
        append_hal_call(program, HalService::DigitalRead, byteArgument(0, "pin"), 0);
        peripherals.emplace_back("GPIO");
        return;
    }
    if (name == "delay") {
        if (argumentCount != 1) fail_at(cursor, "delay expects one argument");
        const auto milliseconds = constant_integer(argument(0));
        if (milliseconds < 0 || milliseconds > std::numeric_limits<std::uint32_t>::max()) {
            fail_at(argument(0), "delay must be between 0 and 4294967295 milliseconds");
        }
        Bytes operands;
        append_u32(operands, static_cast<std::uint32_t>(milliseconds));
        program.instructions.push_back({Opcode::Wait, std::move(operands)});
        peripherals.emplace_back("SYSTICK");
        return;
    }

}

void lower_selected_statement(CXCursor cursor, BodyContext& context)
{
    const auto kind = clang_getCursorKind(cursor);
    if (kind == CXCursor_CompoundStmt || kind == CXCursor_DeclStmt) {
        const auto localCount = context.locals.size();
        clang_visitChildren(cursor, lower_statement, &context);
        if (kind == CXCursor_CompoundStmt) context.locals.resize(localCount);
    } else {
        lower_statement(cursor, clang_getNullCursor(), &context);
    }
    if (context.failure) std::rethrow_exception(context.failure);
}

void patch_jump_target(BytecodeInstruction& instruction, std::uint32_t target)
{
    if (instruction.operands.size() != 4) {
        throw FirmwareError("internal compiler error: malformed jump placeholder");
    }
    instruction.operands[0] = static_cast<Byte>(target);
    instruction.operands[1] = static_cast<Byte>(target >> 8);
    instruction.operands[2] = static_cast<Byte>(target >> 16);
    instruction.operands[3] = static_cast<Byte>(target >> 24);
}

CXChildVisitResult lower_statement(CXCursor cursor, CXCursor, CXClientData clientData)
{
    auto& context = *static_cast<BodyContext*>(clientData);
    if (context.terminated) return CXChildVisit_Continue;

    try {
        switch (clang_getCursorKind(cursor)) {
        case CXCursor_CallExpr:
            lower_call(cursor, context);
            return CXChildVisit_Continue;
        case CXCursor_DeclStmt:
            return CXChildVisit_Recurse;
        case CXCursor_VarDecl: {
            if (clang_Cursor_getStorageClass(cursor) != CX_SC_None) {
                fail_at(cursor, "static and extern local variables are not supported");
            }
            const auto typeName = ClangString(clang_getTypeSpelling(clang_getCursorType(cursor))).str();
            if (typeName != "int" && typeName != "bool") {
                fail_at(cursor, "local variables currently support only int and bool");
            }
            if (context.nextSlot >= 256) fail_at(cursor, "VM supports at most 256 local variables");
            const auto slot = context.nextSlot++;
            context.locals.emplace_back(cursor, slot);
            const auto children = cursor_children(cursor);
            if (children.empty()) emit_const(*context.program, 0);
            else if (children.size() == 1) emit_expression(children.front(), context);
            else fail_at(cursor, "unsupported local variable initializer");
            emit_slot(*context.program, Opcode::Store, slot);
            return CXChildVisit_Continue;
        }
        case CXCursor_BinaryOperator: {
            if (clang_getCursorBinaryOperatorKind(cursor) != CXBinaryOperator_Assign) {
                fail_at(cursor, "only simple local assignment is supported as a statement");
            }
            const auto children = cursor_children(cursor);
            if (children.size() != 2 || clang_getCursorKind(children[0]) != CXCursor_DeclRefExpr) {
                fail_at(cursor, "assignment target must be a local variable");
            }
            const auto declaration = clang_getCursorReferenced(children[0]);
            emit_expression(children[1], context);
            emit_slot(*context.program, Opcode::Store, local_slot(declaration, context));
            return CXChildVisit_Continue;
        }
        case CXCursor_ReturnStmt: {
            CallChildren children;
            clang_visitChildren(cursor, collect_call_children, &children);
            if (!children.cursors.empty()) {
                const auto value = constant_integer(children.cursors.front());
                if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
                    fail_at(cursor, "main return value is outside the int range");
                }
            }
            context.program->instructions.push_back({Opcode::Halt, {}});
            context.terminated = true;
            return CXChildVisit_Continue;
        }
        case CXCursor_NullStmt:
            return CXChildVisit_Continue;
        case CXCursor_CompoundStmt: {
            lower_selected_statement(cursor, context);
            return CXChildVisit_Continue;
        }
        case CXCursor_IfStmt: {
            CallChildren children;
            clang_visitChildren(cursor, collect_call_children, &children);
            if (children.cursors.size() < 2 || children.cursors.size() > 3) {
                fail_at(cursor, "if statement form is not supported");
            }
            emit_expression(children.cursors[0], context);
            emit_const(*context.program, 0);
            context.program->instructions.push_back({Opcode::CmpEqI32, {}});
            const auto elseJump = emit_jump_placeholder(*context.program, Opcode::JumpIf);
            const bool wasTerminated = context.terminated;
            lower_selected_statement(children.cursors[1], context);
            const bool thenTerminated = context.terminated;
            context.terminated = wasTerminated;
            if (children.cursors.size() == 3) {
                const auto endJump = thenTerminated
                    ? std::numeric_limits<std::size_t>::max()
                    : emit_jump_placeholder(*context.program, Opcode::Jump);
                patch_jump(*context.program, elseJump, current_instruction(context));
                lower_selected_statement(children.cursors[2], context);
                const bool elseTerminated = context.terminated;
                if (endJump != std::numeric_limits<std::size_t>::max()) {
                    patch_jump(*context.program, endJump, current_instruction(context));
                }
                context.terminated = thenTerminated && elseTerminated;
            } else {
                patch_jump(*context.program, elseJump, current_instruction(context));
                context.terminated = wasTerminated;
            }
            return CXChildVisit_Continue;
        }
        case CXCursor_WhileStmt: {
            CallChildren children;
            clang_visitChildren(cursor, collect_call_children, &children);
            if (children.cursors.size() != 2) fail_at(cursor, "while statement form is not supported");
            const auto loopStart = static_cast<std::uint32_t>(context.program->instructions.size());
            emit_expression(children.cursors[0], context);
            emit_const(*context.program, 0);
            context.program->instructions.push_back({Opcode::CmpEqI32, {}});
            const auto exitJump = emit_jump_placeholder(*context.program, Opcode::JumpIf);
            lower_selected_statement(children.cursors[1], context);
            const bool bodyTerminated = context.terminated;
            if (!bodyTerminated) emit_jump(*context.program, Opcode::Jump, loopStart);
            patch_jump(*context.program, exitJump, current_instruction(context));
            context.terminated = false;
            return CXChildVisit_Continue;
        }
        case CXCursor_ForStmt: {
            const auto children = cursor_children(cursor);
            if (children.size() != 4 || clang_getCursorKind(children[2]) != CXCursor_UnaryOperator)
                fail_at(cursor, "for requires init, condition, ++/-- increment, and body");

            const auto localCount = context.locals.size();
            lower_selected_statement(children[0], context);
            const auto loopStart = current_instruction(context);
            emit_expression(children[1], context);
            emit_const(*context.program, 0);
            context.program->instructions.push_back({Opcode::CmpEqI32, {}});
            const auto exitJump = emit_jump_placeholder(*context.program, Opcode::JumpIf);
            lower_selected_statement(children[3], context);
            const bool bodyTerminated = context.terminated;
            context.terminated = false;
            if (!bodyTerminated) {
                lower_increment(children[2], context);
                emit_jump(*context.program, Opcode::Jump, loopStart);
            }
            patch_jump(*context.program, exitJump, current_instruction(context));
            context.locals.resize(localCount);
            context.terminated = false;
            return CXChildVisit_Continue;
        }
        default:
            fail_at(cursor, "unsupported statement in main(); supported statements are HAL calls, "
                "locals, assignment, return, if/else, and while");
        }
    } catch (...) {
        context.failure = std::current_exception();
        return CXChildVisit_Break;
    }
    return CXChildVisit_Continue;
}

void print_diagnostics(CXTranslationUnit translationUnit, bool errorsOnly)
{
    const auto count = clang_getNumDiagnostics(translationUnit);
    for (unsigned i = 0; i < count; ++i) {
        CXDiagnostic diagnostic = clang_getDiagnostic(translationUnit, i);
        const auto severity = clang_getDiagnosticSeverity(diagnostic);
        if (!errorsOnly || severity >= CXDiagnostic_Error) {
            const auto options = clang_defaultDiagnosticDisplayOptions();
            std::cerr << ClangString(clang_formatDiagnostic(diagnostic, options)).str() << '\n';
        }
        clang_disposeDiagnostic(diagnostic);
    }
}

FirmwarePackage compile_source(const std::filesystem::path& sourcePath)
{
    const auto source = std::filesystem::absolute(sourcePath).string();
    const std::string includeArgument = std::string("-I") + RSMA_MCU_INCLUDE_DIR;
    const char* arguments[] = {"-x", "c++", "-std=c++20", includeArgument.c_str()};
    CXIndex index = clang_createIndex(0, 0);
    if (index == nullptr) throw FirmwareError("could not create Clang index");

    CXTranslationUnit translationUnit = nullptr;
    const auto parseError = clang_parseTranslationUnit2(index, source.c_str(), arguments,
        static_cast<int>(sizeof(arguments) / sizeof(arguments[0])), nullptr, 0,
        CXTranslationUnit_KeepGoing, &translationUnit);
    if (translationUnit == nullptr) {
        clang_disposeIndex(index);
        throw FirmwareError("Clang could not parse source (error " + std::to_string(parseError) + ")");
    }

    try {
        print_diagnostics(translationUnit, false);
        if (parseError != CXError_Success) throw FirmwareError("Clang failed to parse source");
        for (unsigned i = 0; i < clang_getNumDiagnostics(translationUnit); ++i) {
            CXDiagnostic diagnostic = clang_getDiagnostic(translationUnit, i);
            const bool isError = clang_getDiagnosticSeverity(diagnostic) >= CXDiagnostic_Error;
            clang_disposeDiagnostic(diagnostic);
            if (isError) throw FirmwareError("source contains Clang errors");
        }

        AstContext ast;
        clang_visitChildren(clang_getTranslationUnitCursor(translationUnit), find_main, &ast);
        if (ast.failure) std::rethrow_exception(ast.failure);
        if (ast.mainDefinitions != 1) {
            throw FirmwareError("source must define exactly one global int main() function");
        }
        if (clang_Cursor_getNumArguments(ast.mainFunction) != 0 ||
            ClangString(clang_getTypeSpelling(clang_getCursorResultType(ast.mainFunction))).str() != "int") {
            fail_at(ast.mainFunction, "entry point must have signature int main() with no arguments");
        }

        CallChildren functionChildren;
        clang_visitChildren(ast.mainFunction, collect_call_children, &functionChildren);
        CXCursor body = clang_getNullCursor();
        for (const auto child : functionChildren.cursors) {
            if (clang_getCursorKind(child) == CXCursor_CompoundStmt) {
                body = child;
                break;
            }
        }
        if (clang_Cursor_isNull(body)) fail_at(ast.mainFunction, "main() has no function body");

        FirmwarePackage package;
        package.manifest.boardId = "RSMA-STM32F103";
        package.manifest.flashSize = 64 * 1024;
        package.manifest.ramSize = 20 * 1024;
        package.manifest.entryPointName = "main";
        package.bytecode.entryPoint = 0;
        std::vector<std::string> peripherals;
        BodyContext bodyContext{&package.bytecode, &peripherals};
        clang_visitChildren(body, lower_statement, &bodyContext);
        if (bodyContext.failure) std::rethrow_exception(bodyContext.failure);
        if (!bodyContext.terminated) package.bytecode.instructions.push_back({Opcode::Halt, {}});

        for (const auto& peripheral : peripherals) {
            if (std::find(package.manifest.requiredPeripherals.begin(),
                    package.manifest.requiredPeripherals.end(), peripheral) ==
                package.manifest.requiredPeripherals.end()) {
                package.manifest.requiredPeripherals.push_back(peripheral);
            }
        }

        clang_disposeTranslationUnit(translationUnit);
        clang_disposeIndex(index);
        return package;
    } catch (...) {
        clang_disposeTranslationUnit(translationUnit);
        clang_disposeIndex(index);
        throw;
    }
}

int usage()
{
    std::cerr << "Usage: rsma-build <source.cpp> -o <output.rsfw>\n";
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 4 || std::string_view(argv[2]) != "-o") return usage();
    try {
        const auto package = compile_source(argv[1]);
        write_firmware(package, argv[3]);
        std::cout << "Created " << argv[3] << " (" << package.bytecode.instructions.size()
                  << " instructions)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rsma-build: " << error.what() << '\n';
        return 1;
    }
}