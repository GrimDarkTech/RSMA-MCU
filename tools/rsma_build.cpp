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

void lower_call(CXCursor cursor, BytecodeProgram& program,
    std::vector<std::string>& peripherals)
{
    const auto function = resolve_called_function(cursor);
    const auto& name = function.name;
    if (name != "pinMode" && name != "digitalWrite" && name != "delay") {
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

struct BodyContext
{
    BytecodeProgram* program;
    std::vector<std::string>* peripherals;
    bool terminated = false;
    std::exception_ptr failure;
};

CXChildVisitResult lower_statement(CXCursor cursor, CXCursor, CXClientData clientData)
{
    auto& context = *static_cast<BodyContext*>(clientData);
    if (context.terminated) return CXChildVisit_Continue;

    try {
        switch (clang_getCursorKind(cursor)) {
        case CXCursor_CallExpr:
            lower_call(cursor, *context.program, *context.peripherals);
            return CXChildVisit_Continue;
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
            return CXChildVisit_Recurse;
        }
        default:
            fail_at(cursor, "unsupported statement in main(); this compiler slice accepts HAL calls and return only");
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