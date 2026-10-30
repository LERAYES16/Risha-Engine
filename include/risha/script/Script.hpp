#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace risha::script {

enum class OpCode {
    PushString,
    PushNumber,
    PushBool,
    Load,
    Store,
    StoreGlobalInit,
    Assign,
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    Negate,
    Not,
    And,
    Or,
    Call,
    Return,
    Jump,
    JumpIfFalse,
    Print,
    Discard,
    Halt
};

struct Value {
    enum class Type { Number, String, Bool } type{Type::Number};
    double number{0.0};
    std::wstring text;
    bool boolean{false};
    Value() = default;
    Value(Type value_type, double value_number, std::wstring value_text = {}, bool value_boolean = false) : type(value_type), number(value_number), text(std::move(value_text)), boolean(value_boolean) {}
    static Value from_number(double value) { return Value(Type::Number, value); }
    static Value from_bool(bool value) { return Value(Type::Bool, 0.0, {}, value); }
    static Value from_text(std::wstring value) { return Value(Type::String, 0.0, std::move(value)); }
};

struct Instruction {
    OpCode op{OpCode::Halt};
    std::wstring text;
    double number{0.0};
    int target{0};
    bool boolean{false};
    int line{0};
    Instruction() = default;
    explicit Instruction(OpCode operation) : op(operation) {}
    Instruction(OpCode operation, std::wstring value) : op(operation), text(std::move(value)) {}
    Instruction(OpCode operation, std::wstring value, double value_number, int value_target = 0, bool value_boolean = false)
        : op(operation), text(std::move(value)), number(value_number), target(value_target), boolean(value_boolean) {}
};

struct Function {
    int entry{0};
    std::vector<std::wstring> parameters;
};

struct Program {
    std::vector<Instruction> code;
    std::unordered_map<std::wstring, Function> functions;
};

struct Diagnostic {
    int line{0};
    std::wstring message;
    int column{0};
};

using NativeFunction = std::function<bool(std::wstring_view, const std::vector<Value>&, Value&, std::wstring&)>;
using Globals = std::unordered_map<std::wstring, Value>;

class Compiler final {
public:
    bool compile(std::wstring_view source, Program& program, std::vector<Diagnostic>& diagnostics) const;
};

class VirtualMachine final {
public:
    bool run(const Program& program, std::vector<std::wstring>& output, std::vector<Diagnostic>& diagnostics) const;
    bool run(const Program& program, std::vector<std::wstring>& output, std::vector<Diagnostic>& diagnostics, const NativeFunction& native_function) const;
    bool run(const Program& program, std::vector<std::wstring>& output, std::vector<Diagnostic>& diagnostics, const NativeFunction& native_function, Globals* globals) const;
};

std::wstring normalize_keyword(std::wstring_view word);
bool is_keyword(std::wstring_view word);
const std::vector<std::wstring>& builtin_names();

}
