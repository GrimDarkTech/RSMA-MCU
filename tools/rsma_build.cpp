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
    std::vector<CXCursor> functionDefinitions;
    std::vector<CXCursor> globalVariables;
    std::exception_ptr failure;
};

CXChildVisitResult find_main(CXCursor cursor, CXCursor, CXClientData clientData)
{
    auto& context = *static_cast<AstContext*>(clientData);
    try {
        if (clang_getCursorKind(cursor) == CXCursor_FunctionDecl &&
            clang_getCursorKind(clang_getCursorLexicalParent(cursor)) == CXCursor_TranslationUnit &&
            clang_isCursorDefinition(cursor)) {
            context.functionDefinitions.push_back(cursor);
            if (ClangString(clang_getCursorSpelling(cursor)).str() == "main") {
                context.mainFunction = cursor;
                ++context.mainDefinitions;
            }
        }
        if (clang_getCursorKind(cursor) == CXCursor_VarDecl &&
            clang_getCursorKind(clang_getCursorLexicalParent(cursor)) == CXCursor_TranslationUnit)
            context.globalVariables.push_back(cursor);
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

void append_u16(Bytes& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<Byte>(value));
    bytes.push_back(static_cast<Byte>(value >> 8));
}

void append_string(Bytes& bytes, const std::string& value)
{
    if (value.size() > std::numeric_limits<std::uint16_t>::max())
        throw FirmwareError("symbol/debug string exceeds format limit");
    append_u16(bytes, static_cast<std::uint16_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

void append_magic(Bytes& bytes, const char (&magic)[5])
{
    bytes.insert(bytes.end(), magic, magic + 4);
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

struct PendingCall
{
    std::size_t instruction;
    CXCursor target;
    CXCursor caller;
};

struct FunctionLayout
{
    CXCursor function;
    std::vector<CXCursor> parameters;
    std::vector<std::pair<CXCursor, std::uint16_t>> locals;
};

struct CompileContext
{
    std::vector<CXCursor> functions;
    std::vector<PendingCall> calls;
    std::vector<std::pair<CXCursor, std::uint32_t>> functionEntries;
    std::vector<FunctionLayout> layouts;
    std::vector<std::pair<CXCursor, std::uint16_t>> globals;
};

ResolvedFunction resolve_called_function(CXCursor call);
bool is_builtin_hal_declaration(CXCursor declaration);
std::vector<CXCursor> cursor_children(CXCursor cursor);

CXCursor find_function_definition(CXCursor declaration, CompileContext& compiler)
{
    const auto definition = clang_getCursorDefinition(declaration);
    for (const auto function : compiler.functions) {
        if (clang_equalCursors(function, declaration) ||
            (!clang_Cursor_isNull(definition) && clang_equalCursors(function, definition))) {
            return function;
        }
    }
    return clang_getNullCursor();
}

CXCursor function_body(CXCursor function)
{
    const auto children = cursor_children(function);
    for (const auto child : children) {
        if (clang_getCursorKind(child) == CXCursor_CompoundStmt) return child;
    }
    return clang_getNullCursor();
}

struct BodyContext
{
    BytecodeProgram* program;
    std::vector<std::string>* peripherals;
    CompileContext* compiler;
    CXCursor function;
    std::uint32_t functionEntry;
    bool isMain;
    bool returnsInt;
    bool hasCallFrame;
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

CXChildVisitResult collect_variable_declarations(CXCursor cursor, CXCursor, CXClientData clientData)
{
    if (clang_getCursorKind(cursor) == CXCursor_VarDecl)
        static_cast<std::vector<CXCursor>*>(clientData)->push_back(cursor);
    return CXChildVisit_Recurse;
}

std::uint16_t local_slot(CXCursor declaration, BodyContext& context)
{
    for (const auto& local : context.locals) {
        if (clang_equalCursors(local.first, declaration)) return local.second;
    }
    fail_at(declaration, "variable is not a supported local in this function");
}

std::uint16_t global_slot(CXCursor declaration, BodyContext& context)
{
    for (const auto& global : context.compiler->globals) {
        if (clang_equalCursors(global.first, declaration)) return global.second;
    }
    fail_at(declaration, "global variable is not supported");
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

void emit_global_slot(BytecodeProgram& program, Opcode opcode, std::uint16_t slot)
{
    emit_slot(program, opcode, slot);
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
        const auto global = std::find_if(context.compiler->globals.begin(), context.compiler->globals.end(),
            [&](const auto& item) { return clang_equalCursors(item.first, declaration); });
        if (global != context.compiler->globals.end())
            emit_global_slot(*context.program, Opcode::GlobalLoad, global->second);
        else
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
        if (function.name == "digitalRead" || function.name == "analogRead") {
            if (!is_builtin_hal_declaration(function.declaration))
                fail_at(cursor, "HAL call must resolve to the declaration in <rsma_mcu/Arduino.h>");
            if (clang_Cursor_getNumArguments(cursor) != 1)
                fail_at(cursor, function.name + " expects one argument");
            const auto pin = constant_integer(clang_Cursor_getArgument(cursor, 0));
            if (pin < 0 || pin > std::numeric_limits<Byte>::max()) {
                fail_at(clang_Cursor_getArgument(cursor, 0), "channel must be between 0 and 255");
            }
            append_hal_call(*context.program,
                function.name == "analogRead" ? HalService::AnalogRead : HalService::DigitalRead,
                static_cast<Byte>(pin), 0);
            context.peripherals->emplace_back(function.name == "analogRead" ? "ADC" : "GPIO");
            return;
        }
        const auto target = find_function_definition(function.declaration, *context.compiler);
        if (clang_Cursor_isNull(target)) {
            fail_at(cursor, "call target '" + function.name + "' has no definition in this source");
        }
        const auto targetLayout = std::find_if(context.compiler->layouts.begin(), context.compiler->layouts.end(),
            [&](const FunctionLayout& layout) { return clang_equalCursors(layout.function, target); });
        if (targetLayout == context.compiler->layouts.end())
            throw FirmwareError("internal compiler error: missing function layout");
        if (clang_Cursor_getNumArguments(cursor) != static_cast<int>(targetLayout->parameters.size()))
            fail_at(cursor, "function call has the wrong number of arguments");
        if (ClangString(clang_getTypeSpelling(clang_getCursorResultType(target))).str() != "int") {
            fail_at(cursor, "void functions cannot be used as value expressions");
        }
        for (int index = 0; index < clang_Cursor_getNumArguments(cursor); ++index) {
            emit_expression(clang_Cursor_getArgument(cursor, static_cast<unsigned>(index)), context);
        }
        const auto instruction = context.program->instructions.size();
        context.program->instructions.push_back({Opcode::Call, Bytes(4, 0)});
        context.compiler->calls.push_back({instruction, target, context.function});
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
    const bool halName = name == "pinMode" || name == "digitalWrite" ||
        name == "digitalRead" || name == "analogRead" || name == "delay" || name == "yield";
    if (halName && !is_builtin_hal_declaration(function.declaration)) {
        fail_at(cursor, "HAL call must resolve to the declaration in <rsma_mcu/Arduino.h>");
    }
    if (!halName) {
        const auto target = find_function_definition(function.declaration, *context.compiler);
        if (clang_Cursor_isNull(target)) {
            fail_at(cursor, "call target '" + (name.empty() ? std::string("<unknown>") : name) +
                "' has no definition in this source");
        }
        const auto targetLayout = std::find_if(context.compiler->layouts.begin(), context.compiler->layouts.end(),
            [&](const FunctionLayout& layout) { return clang_equalCursors(layout.function, target); });
        if (targetLayout == context.compiler->layouts.end())
            throw FirmwareError("internal compiler error: missing function layout");
        if (clang_Cursor_getNumArguments(cursor) != static_cast<int>(targetLayout->parameters.size()))
            fail_at(cursor, "function call has the wrong number of arguments");
        if (ClangString(clang_getTypeSpelling(clang_getCursorResultType(target))).str() != "void") {
            fail_at(cursor, "int-returning functions must be used as value expressions");
        }
        for (int index = 0; index < clang_Cursor_getNumArguments(cursor); ++index) {
            emit_expression(clang_Cursor_getArgument(cursor, static_cast<unsigned>(index)), context);
        }
        const auto instruction = program.instructions.size();
        program.instructions.push_back({Opcode::Call, Bytes(4, 0)});
        context.compiler->calls.push_back({instruction, target, context.function});
        return;
    }
    const auto argumentCount = clang_Cursor_getNumArguments(cursor);
    if (name == "yield") {
        if (argumentCount != 0) fail_at(cursor, "yield expects no arguments");
        program.instructions.push_back({Opcode::Yield, {}});
        return;
    }
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
    if (name == "analogRead") {
        if (argumentCount != 1) fail_at(cursor, "analogRead expects one argument");
        append_hal_call(program, HalService::AnalogRead, byteArgument(0, "ADC channel"), 0);
        peripherals.emplace_back("ADC");
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
        if (kind == CXCursor_CompoundStmt && !context.hasCallFrame) context.locals.resize(localCount);
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
            std::uint16_t slot;
            if (context.hasCallFrame) {
                slot = local_slot(cursor, context);
            } else {
                if (context.nextSlot >= 256) fail_at(cursor, "VM supports at most 256 local variables");
                slot = context.nextSlot++;
                context.locals.emplace_back(cursor, slot);
            }
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
            const auto global = std::find_if(context.compiler->globals.begin(), context.compiler->globals.end(),
                [&](const auto& item) { return clang_equalCursors(item.first, declaration); });
            if (global != context.compiler->globals.end())
                emit_global_slot(*context.program, Opcode::GlobalStore, global->second);
            else
                emit_slot(*context.program, Opcode::Store, local_slot(declaration, context));
            return CXChildVisit_Continue;
        }
        case CXCursor_ReturnStmt: {
            CallChildren children;
            clang_visitChildren(cursor, collect_call_children, &children);
            if (context.isMain) {
                if (!children.cursors.empty() && clang_getCursorKind(children.cursors.front()) == CXCursor_CallExpr)
                    emit_expression(children.cursors.front(), context);
                context.program->instructions.push_back({Opcode::Halt, {}});
            } else if (context.returnsInt) {
                if (children.cursors.size() != 1) fail_at(cursor, "int function must return a value");
                emit_expression(children.cursors.front(), context);
                context.program->instructions.push_back({Opcode::Return, {}});
            } else {
                if (!children.cursors.empty()) fail_at(cursor, "void function cannot return a value");
                context.program->instructions.push_back({Opcode::Return, {}});
            }
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
            if (!context.hasCallFrame) context.locals.resize(localCount);
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
            ClangString(clang_getTypeSpelling(clang_getCursorResultType(ast.mainFunction))).str() != "int")
            fail_at(ast.mainFunction, "entry point must have signature int main() with no arguments");

        CompileContext compiler;
        compiler.functions = std::move(ast.functionDefinitions);
        if (ast.globalVariables.size() > 256)
            fail_at(ast.globalVariables[256], "VM supports at most 256 global variables");
        for (std::size_t globalSlotIndex = 0; globalSlotIndex < ast.globalVariables.size(); ++globalSlotIndex) {
            const auto global = ast.globalVariables[globalSlotIndex];
            if (clang_Cursor_getStorageClass(global) == CX_SC_Extern)
                fail_at(global, "extern global variables are not supported");
            const auto typeName = ClangString(clang_getTypeSpelling(clang_getCursorType(global))).str();
            if (typeName != "int" && typeName != "bool")
                fail_at(global, "global variables currently support only int and bool");
            compiler.globals.emplace_back(global, static_cast<std::uint16_t>(globalSlotIndex));
        }
        std::sort(compiler.functions.begin(), compiler.functions.end(), [&](CXCursor left, CXCursor right) {
            return clang_equalCursors(left, ast.mainFunction) && !clang_equalCursors(right, ast.mainFunction);
        });

        for (const auto function : compiler.functions) {
            const auto name = ClangString(clang_getCursorSpelling(function)).str();
            const auto result = ClangString(clang_getTypeSpelling(clang_getCursorResultType(function))).str();
            if (name == "main" && !clang_equalCursors(function, ast.mainFunction))
                fail_at(function, "multiple main() definitions are not supported");
            if (name != "main" && result != "int" && result != "void")
                fail_at(function, "user-defined functions must return int or void");
            FunctionLayout layout;
            layout.function = function;
            for (int argumentIndex = 0; argumentIndex < clang_Cursor_getNumArguments(function); ++argumentIndex) {
                const auto parameter = clang_Cursor_getArgument(function, static_cast<unsigned>(argumentIndex));
                const auto parameterType = ClangString(clang_getTypeSpelling(clang_getCursorType(parameter))).str();
                if (parameterType != "int" && parameterType != "bool")
                    fail_at(parameter, "function parameters currently support only int and bool");
                if (layout.locals.size() >= 256)
                    fail_at(parameter, "VM supports at most 256 function slots");
                layout.parameters.push_back(parameter);
                layout.locals.emplace_back(parameter, static_cast<std::uint16_t>(layout.locals.size()));
            }

            const auto body = function_body(function);
            if (clang_Cursor_isNull(body)) fail_at(function, "function definition has no body");
            std::vector<CXCursor> declarations;
            clang_visitChildren(body, collect_variable_declarations, &declarations);
            for (const auto child : declarations) {
                if (clang_Cursor_getStorageClass(child) != CX_SC_None)
                    fail_at(child, "static and extern local variables are not supported");
                const auto localType = ClangString(clang_getTypeSpelling(clang_getCursorType(child))).str();
                if (localType != "int" && localType != "bool")
                    fail_at(child, "local variables currently support only int and bool");
                if (layout.locals.size() >= 256)
                    fail_at(child, "VM supports at most 256 function slots");
                layout.locals.emplace_back(child, static_cast<std::uint16_t>(layout.locals.size()));
            }
            compiler.layouts.push_back(std::move(layout));
        }

        FirmwarePackage package;
        package.manifest.boardId = "RSMA-STM32F103";
        package.manifest.flashSize = 64 * 1024;
        package.manifest.ramSize = 20 * 1024;
        package.manifest.entryPointName = "main";
        package.bytecode.entryPoint = 0;
        std::vector<std::string> peripherals;
        for (const auto function : compiler.functions) {
            const auto name = ClangString(clang_getCursorSpelling(function)).str();
            const bool isMain = name == "main";
            const auto resultType = ClangString(clang_getTypeSpelling(clang_getCursorResultType(function))).str();
            const auto entry = current_instruction(BodyContext{
                &package.bytecode, &peripherals, &compiler, function,
                static_cast<std::uint32_t>(package.bytecode.instructions.size()), isMain, resultType == "int"});
            compiler.functionEntries.emplace_back(function, entry);
            const auto layout = std::find_if(compiler.layouts.begin(), compiler.layouts.end(),
                [&](const FunctionLayout& candidate) { return clang_equalCursors(candidate.function, function); });
            if (layout == compiler.layouts.end()) throw FirmwareError("internal compiler error: missing function layout");
            const auto body = function_body(function);
            BodyContext bodyContext{
                &package.bytecode, &peripherals, &compiler, function, entry, isMain,
                resultType == "int", !isMain};
            if (!isMain) {
                bodyContext.locals = layout->locals;
                bodyContext.nextSlot = static_cast<std::uint16_t>(layout->locals.size());
            }
            if (isMain) {
                for (std::size_t globalIndex = 0; globalIndex < ast.globalVariables.size(); ++globalIndex) {
                    const auto global = ast.globalVariables[globalIndex];
                    const auto initializers = cursor_children(global);
                    const auto initialValue = initializers.empty() ? 0 : constant_integer(initializers.front());
                    if (initialValue < std::numeric_limits<std::int32_t>::min() ||
                        initialValue > std::numeric_limits<std::int32_t>::max())
                        fail_at(global, "global initializer is outside the VM int32 range");
                    emit_const(package.bytecode, static_cast<std::int32_t>(initialValue));
                    emit_global_slot(package.bytecode, Opcode::GlobalStore, static_cast<std::uint16_t>(globalIndex));
                }
            }
            for (auto parameter = layout->parameters.rbegin(); parameter != layout->parameters.rend(); ++parameter)
                emit_slot(package.bytecode, Opcode::Store, local_slot(*parameter, bodyContext));
            clang_visitChildren(body, lower_statement, &bodyContext);
            if (bodyContext.failure) std::rethrow_exception(bodyContext.failure);
            if (!bodyContext.terminated) {
                if (isMain) package.bytecode.instructions.push_back({Opcode::Halt, {}});
                else if (resultType == "void") package.bytecode.instructions.push_back({Opcode::Return, {}});
                else fail_at(function, "int function must return a value on every path");
            }
        }

        for (const auto& pending : compiler.calls) {
            for (const auto& entry : compiler.functionEntries) {
                if (clang_equalCursors(entry.first, pending.target)) {
                    patch_jump(package.bytecode, pending.instruction, entry.second);
                    break;
                }
            }
        }

        for (const auto& pending : compiler.calls) {
            const bool callerFound = std::any_of(compiler.functionEntries.begin(), compiler.functionEntries.end(),
                [&](const auto& entry) { return clang_equalCursors(entry.first, pending.caller); });
            const bool targetFound = std::any_of(compiler.functionEntries.begin(), compiler.functionEntries.end(),
                [&](const auto& entry) { return clang_equalCursors(entry.first, pending.target); });
            if (!callerFound || !targetFound)
                throw FirmwareError("internal compiler error: unresolved function call");
        }

        Bytes symbols;
        append_magic(symbols, "SYM1");
        append_u16(symbols, 1);
        append_u16(symbols, static_cast<std::uint16_t>(compiler.functionEntries.size()));
        Bytes debug;
        append_magic(debug, "DBG1");
        append_u16(debug, 1);
        append_u16(debug, static_cast<std::uint16_t>(compiler.functionEntries.size()));
        for (const auto& entry : compiler.functionEntries) {
            const auto name = ClangString(clang_getCursorSpelling(entry.first)).str();
            append_u32(symbols, entry.second);
            append_string(symbols, name);
            append_u32(debug, entry.second);
            append_string(debug, name);
            CXFile file = nullptr;
            unsigned line = 0;
            clang_getSpellingLocation(clang_getCursorLocation(entry.first), &file, &line, nullptr, nullptr);
            append_string(debug, file == nullptr ? std::string{} : ClangString(clang_getFileName(file)).str());
            append_u32(debug, line);
        }
        package.additionalSections.push_back({SectionType::Symbols, SectionFlags::ReadOnly, std::move(symbols)});
        package.additionalSections.push_back({SectionType::Debug, SectionFlags::ReadOnly, std::move(debug)});

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