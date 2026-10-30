#include "risha/script/Script.hpp"
#include "risha/core/Text.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_set>

namespace risha::script {
namespace {

enum class TokenType {
    Number, String, Identifier, Plus, Minus, Star, Slash, Percent, Equal, NotEqual,
    Greater, Less, GreaterEqual, LessEqual, Not, And, Or, LeftParen, RightParen, Comma, End
};

struct Token {
    TokenType type{TokenType::End};
    std::wstring text;
    double number{0.0};
    Token() = default;
    explicit Token(TokenType token_type) : type(token_type) {}
    Token(TokenType token_type, std::wstring token_text, double token_number = 0.0)
        : type(token_type), text(std::move(token_text)), number(token_number) {}
};

bool is_space(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\r' || c == 0x00A0 || c == 0x200E || c == 0x200F ||
           (c >= 0x202A && c <= 0x202E) || (c >= 0x2066 && c <= 0x2069) || c == 0x061C || c == 0xFEFF;
}

std::wstring trim(const std::wstring& value) {
    size_t first = 0;
    while (first < value.size() && is_space(value[first])) ++first;
    size_t last = value.size();
    while (last > first && is_space(value[last - 1])) --last;
    return value.substr(first, last - first);
}

int digit_value(wchar_t c) {
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= 0x0660 && c <= 0x0669) return c - 0x0660;
    if (c >= 0x06F0 && c <= 0x06F9) return c - 0x06F0;
    return -1;
}

bool is_decimal_point(wchar_t c) { return c == L'.' || c == 0x066B; }
bool is_comma(wchar_t c) { return c == L',' || c == 0x060C; }

bool is_word_stop(wchar_t c) {
    return is_space(c) || c == L'(' || c == L')' || c == L'+' || c == L'-' || c == L'*' || c == L'/' ||
           c == L'%' || c == L'=' || c == L'<' || c == L'>' || c == L'!' || c == L'"' || is_comma(c) ||
           c == 0x00AB || c == 0x00BB;
}

bool valid_identifier(const std::wstring& value) {
    if (value.empty() || digit_value(value.front()) >= 0 || is_decimal_point(value.front())) return false;
    for (wchar_t c : value) if (is_word_stop(c) || c == L'#' || c == L'\'' || is_decimal_point(c)) return false;
    return true;
}

bool starts_with(const std::wstring& value, std::wstring_view prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

const std::unordered_set<std::wstring>& keyword_set() {
    static const std::unordered_set<std::wstring> keys = {
        L"عرف", L"غير", L"عام", L"اطبع", L"اذا", L"والا", L"نهايه", L"طالما", L"كرر", L"اخرج",
        L"استمر", L"داله", L"ارجع", L"توقف", L"و", L"او", L"ليس", L"صحيح", L"خطا"
    };
    return keys;
}

std::wstring first_word(const std::wstring& text, size_t& end) {
    size_t i = 0;
    while (i < text.size() && !is_space(text[i]) && text[i] != L'(' && text[i] != L'"') ++i;
    end = i;
    return text.substr(0, i);
}

std::wstring rest_after(const std::wstring& text, size_t end) {
    return trim(text.substr(std::min(end, text.size())));
}

bool lex(const std::wstring& source, std::vector<Token>& tokens, std::wstring& error) {
    tokens.clear();
    size_t i = 0;
    while (i < source.size()) {
        const wchar_t c = source[i];
        if (is_space(c)) { ++i; continue; }
        if (c == L'"' || c == 0x00AB) {
            const wchar_t closing = c == L'"' ? L'"' : 0x00BB;
            ++i;
            std::wstring value;
            bool closed = false;
            while (i < source.size()) {
                if (source[i] == closing) { ++i; closed = true; break; }
                if (source[i] == L'\\' && i + 1 < source.size()) {
                    const wchar_t next = source[i + 1];
                    if (next == L'"' || next == L'\\') { value.push_back(next); i += 2; continue; }
                    if (next == L'n') { value.push_back(L'\n'); i += 2; continue; }
                    if (next == L't') { value.push_back(L'\t'); i += 2; continue; }
                }
                value.push_back(source[i++]);
            }
            if (!closed) { error = L"النص غير مغلق؛ أضف علامة اقتباس في نهايته."; return false; }
            tokens.emplace_back(TokenType::String, std::move(value));
            continue;
        }
        if (digit_value(c) >= 0 || (is_decimal_point(c) && i + 1 < source.size() && digit_value(source[i + 1]) >= 0)) {
            double value = 0.0;
            double scale = 0.0;
            bool dot = false;
            const size_t start = i;
            while (i < source.size()) {
                const int digit = digit_value(source[i]);
                if (digit >= 0) {
                    if (dot) { scale /= 10.0; value += digit * scale; }
                    else value = value * 10.0 + digit;
                    ++i;
                } else if (is_decimal_point(source[i]) && !dot) {
                    dot = true;
                    scale = 1.0;
                    ++i;
                } else break;
            }
            if (!std::isfinite(value)) { error = L"الرقم كبير جداً."; return false; }
            tokens.emplace_back(TokenType::Number, source.substr(start, i - start), value);
            continue;
        }
        if (c == L'(') { tokens.emplace_back(TokenType::LeftParen); ++i; continue; }
        if (c == L')') { tokens.emplace_back(TokenType::RightParen); ++i; continue; }
        if (is_comma(c)) { tokens.emplace_back(TokenType::Comma); ++i; continue; }
        if (c == L'+') { tokens.emplace_back(TokenType::Plus); ++i; continue; }
        if (c == L'-') { tokens.emplace_back(TokenType::Minus); ++i; continue; }
        if (c == L'*') { tokens.emplace_back(TokenType::Star); ++i; continue; }
        if (c == L'/') { tokens.emplace_back(TokenType::Slash); ++i; continue; }
        if (c == L'%') { tokens.emplace_back(TokenType::Percent); ++i; continue; }
        if (c == L'=') {
            if (i + 1 < source.size() && source[i + 1] == L'=') { tokens.emplace_back(TokenType::Equal); i += 2; continue; }
            error = L"استخدم == للمقارنة، أما = فللإسناد فقط.";
            return false;
        }
        if (c == L'!') {
            if (i + 1 < source.size() && source[i + 1] == L'=') { tokens.emplace_back(TokenType::NotEqual); i += 2; }
            else { tokens.emplace_back(TokenType::Not); ++i; }
            continue;
        }
        if (c == L'>') {
            if (i + 1 < source.size() && source[i + 1] == L'=') { tokens.emplace_back(TokenType::GreaterEqual); i += 2; }
            else { tokens.emplace_back(TokenType::Greater); ++i; }
            continue;
        }
        if (c == L'<') {
            if (i + 1 < source.size() && source[i + 1] == L'=') { tokens.emplace_back(TokenType::LessEqual); i += 2; }
            else { tokens.emplace_back(TokenType::Less); ++i; }
            continue;
        }
        const size_t start = i;
        while (i < source.size() && !is_word_stop(source[i])) ++i;
        if (start == i) { error = L"رمز غير معروف."; return false; }
        std::wstring word = source.substr(start, i - start);
        const std::wstring key = normalize_keyword(word);
        if (key == L"و") tokens.emplace_back(TokenType::And, std::move(word));
        else if (key == L"او") tokens.emplace_back(TokenType::Or, std::move(word));
        else if (key == L"ليس") tokens.emplace_back(TokenType::Not, std::move(word));
        else if (!valid_identifier(word)) { error = L"اسم غير صالح: " + word; return false; }
        else tokens.emplace_back(TokenType::Identifier, std::move(word));
    }
    tokens.emplace_back(TokenType::End);
    return true;
}

class ExpressionParser {
public:
    ExpressionParser(const std::vector<Token>& tokens, int line, Program& program, std::vector<Diagnostic>& diagnostics)
        : tokens_(tokens), line_(line), program_(program), diagnostics_(diagnostics) {}

    bool parse() {
        if (!parse_or()) return false;
        if (!at(TokenType::End)) {
            fail(L"هناك رموز زائدة في التعبير.");
            return false;
        }
        return true;
    }

private:
    const std::vector<Token>& tokens_;
    size_t index_{0};
    int line_;
    Program& program_;
    std::vector<Diagnostic>& diagnostics_;
    bool failed_{false};

    bool at(TokenType type) const { return tokens_[index_].type == type; }
    bool fail(const std::wstring& message) {
        if (!failed_) diagnostics_.push_back({line_, message, 0});
        failed_ = true;
        return false;
    }
    bool eat(TokenType type) {
        if (!at(type)) return false;
        ++index_;
        return true;
    }
    void emit(OpCode op) { program_.code.push_back(Instruction{op}); }

    bool parse_or() {
        if (!parse_and()) return false;
        while (eat(TokenType::Or)) {
            if (!parse_and()) return false;
            emit(OpCode::Or);
        }
        return true;
    }

    bool parse_and() {
        if (!parse_not()) return false;
        while (eat(TokenType::And)) {
            if (!parse_not()) return false;
            emit(OpCode::And);
        }
        return true;
    }

    bool parse_not() {
        if (eat(TokenType::Not)) {
            if (!parse_not()) return false;
            emit(OpCode::Not);
            return true;
        }
        return parse_compare();
    }

    bool parse_compare() {
        if (!parse_add()) return false;
        while (true) {
            OpCode operation = OpCode::Equal;
            if (eat(TokenType::Equal)) operation = OpCode::Equal;
            else if (eat(TokenType::NotEqual)) operation = OpCode::NotEqual;
            else if (eat(TokenType::GreaterEqual)) operation = OpCode::GreaterEqual;
            else if (eat(TokenType::LessEqual)) operation = OpCode::LessEqual;
            else if (eat(TokenType::Greater)) operation = OpCode::Greater;
            else if (eat(TokenType::Less)) operation = OpCode::Less;
            else break;
            if (!parse_add()) return false;
            emit(operation);
        }
        return true;
    }

    bool parse_add() {
        if (!parse_multiply()) return false;
        while (true) {
            if (eat(TokenType::Plus)) {
                if (!parse_multiply()) return false;
                emit(OpCode::Add);
            } else if (eat(TokenType::Minus)) {
                if (!parse_multiply()) return false;
                emit(OpCode::Subtract);
            } else break;
        }
        return true;
    }

    bool parse_multiply() {
        if (!parse_unary()) return false;
        while (true) {
            OpCode operation = OpCode::Multiply;
            if (eat(TokenType::Star)) operation = OpCode::Multiply;
            else if (eat(TokenType::Slash)) operation = OpCode::Divide;
            else if (eat(TokenType::Percent)) operation = OpCode::Modulo;
            else break;
            if (!parse_unary()) return false;
            emit(operation);
        }
        return true;
    }

    bool parse_unary() {
        if (eat(TokenType::Minus)) {
            if (!parse_unary()) return false;
            emit(OpCode::Negate);
            return true;
        }
        if (eat(TokenType::LeftParen)) {
            if (!parse_or()) return false;
            if (!eat(TokenType::RightParen)) return fail(L"القوس غير مغلق.");
            return true;
        }
        if (at(TokenType::Number)) {
            program_.code.push_back(Instruction{OpCode::PushNumber, {}, tokens_[index_].number});
            ++index_;
            return true;
        }
        if (at(TokenType::String)) {
            program_.code.push_back(Instruction{OpCode::PushString, tokens_[index_].text});
            ++index_;
            return true;
        }
        if (at(TokenType::Identifier)) {
            const std::wstring name = tokens_[index_].text;
            const std::wstring key = normalize_keyword(name);
            ++index_;
            if (key == L"صحيح") program_.code.push_back(Instruction{OpCode::PushBool, {}, 0.0, 0, true});
            else if (key == L"خطا") program_.code.push_back(Instruction{OpCode::PushBool, {}, 0.0, 0, false});
            else if (eat(TokenType::LeftParen)) {
                int argument_count = 0;
                if (!at(TokenType::RightParen)) {
                    do {
                        if (!parse_or()) return false;
                        ++argument_count;
                    } while (eat(TokenType::Comma));
                }
                if (!eat(TokenType::RightParen)) return fail(L"قوس استدعاء الدالة غير مغلق: " + name);
                program_.code.push_back(Instruction{OpCode::Call, name, 0.0, argument_count});
            } else program_.code.push_back(Instruction{OpCode::Load, name});
            return true;
        }
        if (at(TokenType::End)) return fail(L"التعبير ناقص.");
        return fail(L"التعبير غير صالح.");
    }
};

bool compile_expression(const std::wstring& text, int line, Program& program, std::vector<Diagnostic>& diagnostics) {
    if (trim(text).empty()) {
        diagnostics.push_back({line, L"التعبير ناقص.", 0});
        return false;
    }
    std::vector<Token> tokens;
    std::wstring error;
    if (!lex(text, tokens, error)) {
        diagnostics.push_back({line, error, 0});
        return false;
    }
    ExpressionParser parser(tokens, line, program, diagnostics);
    return parser.parse();
}

std::wstring format_number(double value) {
    if (std::fabs(value) < 1e-12) value = 0.0;
    std::wostringstream stream;
    stream.imbue(std::locale::classic());
    stream.precision(12);
    stream << value;
    return stream.str();
}

bool truthy(const Value& value) {
    if (value.type == Value::Type::Bool) return value.boolean;
    if (value.type == Value::Type::Number) return value.number != 0.0;
    return !value.text.empty();
}

bool same_value(const Value& left, const Value& right) {
    if (left.type != right.type) return false;
    if (left.type == Value::Type::Number) return left.number == right.number;
    if (left.type == Value::Type::String) return left.text == right.text;
    return left.boolean == right.boolean;
}

std::wstring value_to_string(const Value& value) {
    if (value.type == Value::Type::String) return value.text;
    if (value.type == Value::Type::Bool) return value.boolean ? L"صحيح" : L"خطأ";
    return format_number(value.number);
}

bool find_assignment(const std::wstring& text, std::wstring& name, wchar_t& op, std::wstring& expression) {
    int depth = 0;
    bool in_string = false;
    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t c = text[i];
        if (c == L'"') in_string = !in_string;
        if (in_string) continue;
        if (c == L'(') ++depth;
        else if (c == L')') --depth;
        else if (c == L'=' && depth == 0) {
            const bool next_equal = i + 1 < text.size() && text[i + 1] == L'=';
            if (next_equal) { ++i; continue; }
            wchar_t compound = 0;
            size_t name_end = i;
            if (i > 0 && (text[i - 1] == L'+' || text[i - 1] == L'-' || text[i - 1] == L'*' || text[i - 1] == L'/' || text[i - 1] == L'%')) {
                compound = text[i - 1];
                name_end = i - 1;
            } else if (i > 0 && (text[i - 1] == L'<' || text[i - 1] == L'>' || text[i - 1] == L'!')) {
                continue;
            }
            name = trim(text.substr(0, name_end));
            expression = trim(text.substr(i + 1));
            op = compound;
            return true;
        }
    }
    return false;
}

class StatementCompiler {
public:
    StatementCompiler(std::vector<std::pair<int, std::wstring>> lines, Program& program, std::vector<Diagnostic>& diagnostics)
        : lines_(std::move(lines)), program_(program), diagnostics_(diagnostics) {}

    bool run() {
        size_t position = 0;
        Terminator terminator = Terminator::Eof;
        if (!block(position, terminator)) return false;
        if (terminator != Terminator::Eof) {
            fail(lines_[position].first, L"كلمة غير متوقعة: " + lines_[position].second);
            return false;
        }
        if (program_.code.empty() || program_.code.back().op != OpCode::Halt) program_.code.push_back(Instruction{OpCode::Halt});
        return diagnostics_.empty();
    }

private:
    enum class Terminator { Eof, End, Else, ElseIf };
    enum class Kind { Empty, End, Else, ElseIf, Other };
    struct LoopContext {
        std::vector<size_t> breaks;
        std::vector<size_t> continues;
    };

    std::vector<std::pair<int, std::wstring>> lines_;
    Program& program_;
    std::vector<Diagnostic>& diagnostics_;
    std::vector<LoopContext> loops_;
    int loop_id_{0};
    int function_depth_{0};

    bool fail(int line, const std::wstring& message) {
        diagnostics_.push_back({line, message, 0});
        return false;
    }

    size_t emit(Instruction instruction) {
        program_.code.push_back(std::move(instruction));
        return program_.code.size() - 1;
    }

    void patch(size_t index) { program_.code[index].target = static_cast<int>(program_.code.size()); }

    Kind classify(const std::wstring& text) const {
        if (text.empty()) return Kind::Empty;
        size_t end = 0;
        const std::wstring key = normalize_keyword(first_word(text, end));
        if (key == L"نهايه" && rest_after(text, end).empty()) return Kind::End;
        if (key == L"والا") {
            const std::wstring rest = rest_after(text, end);
            if (rest.empty()) return Kind::Else;
            size_t second_end = 0;
            if (normalize_keyword(first_word(rest, second_end)) == L"اذا") return Kind::ElseIf;
        }
        return Kind::Other;
    }

    bool block(size_t& position, Terminator& terminator) {
        while (position < lines_.size()) {
            const Kind kind = classify(lines_[position].second);
            if (kind == Kind::Empty) { ++position; continue; }
            if (kind == Kind::End) { terminator = Terminator::End; return true; }
            if (kind == Kind::Else) { terminator = Terminator::Else; return true; }
            if (kind == Kind::ElseIf) { terminator = Terminator::ElseIf; return true; }
            const size_t before = program_.code.size();
            const int line = lines_[position].first;
            if (!statement(position)) return false;
            for (size_t i = before; i < program_.code.size(); ++i) if (program_.code[i].line == 0) program_.code[i].line = line;
        }
        terminator = Terminator::Eof;
        return true;
    }

    bool nested_block(size_t& position, Terminator& terminator, int opening_line, const std::wstring& what) {
        if (!block(position, terminator)) return false;
        if (terminator == Terminator::Eof) return fail(opening_line, L"كتلة " + what + L" تحتاج إلى نهاية.");
        return true;
    }

    bool define_variable(int line, const std::wstring& name, const std::wstring& expression, OpCode store) {
        if (!valid_identifier(name) || is_keyword(name) || starts_with(name, L"__عداد_ريشة_")) return fail(line, L"اسم المتغير غير صالح: " + name);
        if (!compile_expression(expression, line, program_, diagnostics_)) return false;
        emit(Instruction{store, name});
        return true;
    }

    bool statement(size_t& position) {
        const int line = lines_[position].first;
        const std::wstring& text = lines_[position].second;
        size_t word_end = 0;
        const std::wstring word = first_word(text, word_end);
        const std::wstring key = normalize_keyword(word);
        const std::wstring rest = rest_after(text, word_end);

        if (key == L"توقف" && rest.empty()) {
            emit(Instruction{OpCode::Halt});
            ++position;
            return true;
        }
        if (key == L"اطبع") {
            if (rest.empty()) return fail(line, L"استخدم: اطبع قيمة");
            if (!compile_expression(rest, line, program_, diagnostics_)) return false;
            emit(Instruction{OpCode::Print});
            ++position;
            return true;
        }
        if (key == L"عرف" || key == L"غير" || key == L"عام") {
            std::wstring name, expression;
            wchar_t op = 0;
            if (!find_assignment(rest, name, op, expression) || (op != 0 && key != L"غير")) return fail(line, L"التعريف يحتاج إلى صيغة: " + word + L" الاسم = قيمة");
            if (key == L"غير" && op != 0) return assign_compound(line, name, op, expression, position);
            const OpCode store = key == L"عرف" ? OpCode::Store : (key == L"غير" ? OpCode::Assign : OpCode::StoreGlobalInit);
            if (!define_variable(line, name, expression, store)) return false;
            ++position;
            return true;
        }
        if (key == L"اخرج" || key == L"استمر") {
            if (!rest.empty()) return fail(line, L"التعليمة " + word + L" لا تحتاج إلى قيمة.");
            if (loops_.empty()) return fail(line, L"التعليمة " + word + L" مسموحة داخل حلقة فقط.");
            const size_t jump = emit(Instruction{OpCode::Jump});
            (key == L"اخرج" ? loops_.back().breaks : loops_.back().continues).push_back(jump);
            ++position;
            return true;
        }
        if (key == L"ارجع") {
            if (function_depth_ == 0) return fail(line, L"أرجع مسموحة داخل دالة فقط.");
            if (rest.empty()) emit(Instruction{OpCode::PushNumber, {}, 0.0});
            else if (!compile_expression(rest, line, program_, diagnostics_)) return false;
            emit(Instruction{OpCode::Return});
            ++position;
            return true;
        }
        if (key == L"اذا") return if_statement(position, rest);
        if (key == L"طالما") return while_statement(position, rest);
        if (key == L"كرر") return repeat_statement(position, rest);
        if (key == L"داله") return function_statement(position, rest);
        if (key == L"والا" || key == L"نهايه") return fail(line, L"كلمة غير متوقعة: " + text);

        std::wstring name, expression;
        wchar_t op = 0;
        if (find_assignment(text, name, op, expression)) {
            if (op != 0) return assign_compound(line, name, op, expression, position);
            if (!valid_identifier(name)) return fail(line, L"اسم المتغير غير صالح: " + name);
            if (!define_variable(line, name, expression, OpCode::Assign)) return false;
            ++position;
            return true;
        }
        if (text.find(L'(') != std::wstring::npos && valid_identifier(word) && !is_keyword(word)) {
            if (!compile_expression(text, line, program_, diagnostics_)) return false;
            emit(Instruction{OpCode::Discard});
            ++position;
            return true;
        }
        return fail(line, L"تعليمة غير معروفة: " + text);
    }

    bool assign_compound(int line, const std::wstring& name, wchar_t op, const std::wstring& expression, size_t& position) {
        if (!valid_identifier(name) || is_keyword(name)) return fail(line, L"اسم المتغير غير صالح: " + name);
        emit(Instruction{OpCode::Load, name});
        if (!compile_expression(expression, line, program_, diagnostics_)) return false;
        const OpCode operation = op == L'+' ? OpCode::Add : op == L'-' ? OpCode::Subtract : op == L'*' ? OpCode::Multiply : op == L'/' ? OpCode::Divide : OpCode::Modulo;
        emit(Instruction{operation});
        emit(Instruction{OpCode::Assign, name});
        ++position;
        return true;
    }

    bool if_statement(size_t& position, const std::wstring& condition) {
        const int line = lines_[position].first;
        if (!compile_expression(condition, line, program_, diagnostics_)) return false;
        size_t jump_false = emit(Instruction{OpCode::JumpIfFalse});
        ++position;
        Terminator terminator = Terminator::Eof;
        if (!nested_block(position, terminator, line, L"إذا")) return false;
        std::vector<size_t> end_jumps;
        while (terminator == Terminator::ElseIf) {
            const int branch_line = lines_[position].first;
            end_jumps.push_back(emit(Instruction{OpCode::Jump}));
            patch(jump_false);
            size_t word_end = 0;
            first_word(lines_[position].second, word_end);
            const std::wstring after_else = rest_after(lines_[position].second, word_end);
            size_t second_end = 0;
            first_word(after_else, second_end);
            const std::wstring branch_condition = rest_after(after_else, second_end);
            const size_t before = program_.code.size();
            if (!compile_expression(branch_condition, branch_line, program_, diagnostics_)) return false;
            for (size_t i = before; i < program_.code.size(); ++i) program_.code[i].line = branch_line;
            jump_false = emit(Instruction{OpCode::JumpIfFalse});
            program_.code[jump_false].line = branch_line;
            ++position;
            if (!nested_block(position, terminator, line, L"وإلا إذا")) return false;
        }
        if (terminator == Terminator::Else) {
            end_jumps.push_back(emit(Instruction{OpCode::Jump}));
            patch(jump_false);
            jump_false = std::numeric_limits<size_t>::max();
            ++position;
            if (!nested_block(position, terminator, line, L"وإلا")) return false;
            if (terminator != Terminator::End) return fail(lines_[position].first, L"بعد وإلا يجب أن تأتي نهاية.");
        }
        if (terminator != Terminator::End) return fail(lines_[position].first, L"كلمة غير متوقعة: " + lines_[position].second);
        if (jump_false != std::numeric_limits<size_t>::max()) patch(jump_false);
        for (size_t jump : end_jumps) patch(jump);
        ++position;
        return true;
    }

    void close_loop(size_t continue_target) {
        LoopContext context = std::move(loops_.back());
        loops_.pop_back();
        for (size_t jump : context.continues) program_.code[jump].target = static_cast<int>(continue_target);
        for (size_t jump : context.breaks) patch(jump);
    }

    bool while_statement(size_t& position, const std::wstring& condition) {
        const int line = lines_[position].first;
        const size_t loop_start = program_.code.size();
        if (!compile_expression(condition, line, program_, diagnostics_)) return false;
        const size_t jump_end = emit(Instruction{OpCode::JumpIfFalse});
        ++position;
        loops_.emplace_back();
        Terminator terminator = Terminator::Eof;
        if (!nested_block(position, terminator, line, L"طالما")) return false;
        if (terminator != Terminator::End) return fail(lines_[position].first, L"كلمة غير متوقعة: " + lines_[position].second);
        const size_t jump_back = emit(Instruction{OpCode::Jump, {}, 0.0, static_cast<int>(loop_start)});
        program_.code[jump_back].line = line;
        patch(jump_end);
        close_loop(loop_start);
        ++position;
        return true;
    }

    bool repeat_statement(size_t& position, const std::wstring& count) {
        const int line = lines_[position].first;
        const std::wstring counter = L"__عداد_ريشة_" + std::to_wstring(++loop_id_);
        if (!compile_expression(count, line, program_, diagnostics_)) return false;
        emit(Instruction{OpCode::Store, counter});
        const size_t loop_start = program_.code.size();
        emit(Instruction{OpCode::Load, counter});
        emit(Instruction{OpCode::PushNumber, {}, 0.0});
        emit(Instruction{OpCode::Greater});
        const size_t jump_end = emit(Instruction{OpCode::JumpIfFalse});
        ++position;
        loops_.emplace_back();
        Terminator terminator = Terminator::Eof;
        if (!nested_block(position, terminator, line, L"كرر")) return false;
        if (terminator != Terminator::End) return fail(lines_[position].first, L"كلمة غير متوقعة: " + lines_[position].second);
        const size_t step = program_.code.size();
        emit(Instruction{OpCode::Load, counter});
        emit(Instruction{OpCode::PushNumber, {}, 1.0});
        emit(Instruction{OpCode::Subtract});
        emit(Instruction{OpCode::Store, counter});
        emit(Instruction{OpCode::Jump, {}, 0.0, static_cast<int>(loop_start)});
        for (size_t i = step; i < program_.code.size(); ++i) program_.code[i].line = line;
        patch(jump_end);
        close_loop(step);
        ++position;
        return true;
    }

    bool function_statement(size_t& position, const std::wstring& signature) {
        const int line = lines_[position].first;
        if (function_depth_ > 0) return fail(line, L"لا يمكن تعريف دالة داخل دالة.");
        if (!loops_.empty()) return fail(line, L"لا يمكن تعريف دالة داخل حلقة.");
        const size_t open = signature.find(L'(');
        const size_t close = signature.rfind(L')');
        if (open == std::wstring::npos || close == std::wstring::npos || close < open || !trim(signature.substr(close + 1)).empty()) return fail(line, L"صيغة الدالة: دالة الاسم(معامل1، معامل2)");
        const std::wstring name = trim(signature.substr(0, open));
        if (!valid_identifier(name) || is_keyword(name)) return fail(line, L"اسم الدالة غير صالح: " + name);
        if (program_.functions.count(name)) return fail(line, L"الدالة معرّفة مسبقاً: " + name);
        Function function;
        const std::wstring list = trim(signature.substr(open + 1, close - open - 1));
        if (!list.empty()) {
            size_t start = 0;
            while (start <= list.size()) {
                size_t stop = start;
                while (stop < list.size() && !is_comma(list[stop])) ++stop;
                const std::wstring parameter = trim(list.substr(start, stop - start));
                if (!valid_identifier(parameter) || is_keyword(parameter)) return fail(line, L"اسم المعامل غير صالح: " + parameter);
                if (std::find(function.parameters.begin(), function.parameters.end(), parameter) != function.parameters.end()) return fail(line, L"اسم المعامل مكرر: " + parameter);
                function.parameters.push_back(parameter);
                start = stop + 1;
            }
        }
        const size_t skip = emit(Instruction{OpCode::Jump});
        function.entry = static_cast<int>(program_.code.size());
        program_.functions[name] = function;
        ++position;
        ++function_depth_;
        Terminator terminator = Terminator::Eof;
        const bool ok = nested_block(position, terminator, line, L"دالة");
        --function_depth_;
        if (!ok) return false;
        if (terminator != Terminator::End) return fail(lines_[position].first, L"كلمة غير متوقعة: " + lines_[position].second);
        emit(Instruction{OpCode::PushNumber, {}, 0.0});
        emit(Instruction{OpCode::Return});
        for (size_t i = static_cast<size_t>(function.entry); i < program_.code.size(); ++i) if (program_.code[i].line == 0) program_.code[i].line = line;
        patch(skip);
        ++position;
        return true;
    }
};

struct Frame {
    int return_pc{0};
    Globals locals;
};

}

std::wstring normalize_keyword(std::wstring_view word) {
    std::wstring result;
    result.reserve(word.size());
    for (wchar_t c : word) {
        if ((c >= 0x064B && c <= 0x065F) || c == 0x0640 || c == 0x0670) continue;
        if (c == 0x0623 || c == 0x0625 || c == 0x0622 || c == 0x0671) c = 0x0627;
        else if (c == 0x0649) c = 0x064A;
        else if (c == 0x0629) c = 0x0647;
        result.push_back(c);
    }
    return result;
}

bool is_keyword(std::wstring_view word) {
    return keyword_set().count(normalize_keyword(word)) != 0;
}

const std::vector<std::wstring>& builtin_names() {
    static const std::vector<std::wstring> names = {
        L"مطلق", L"جذر", L"جيب", L"جيب_تمام", L"مستقيم", L"تقريب", L"أرضية", L"سقف", L"قوة", L"أدنى", L"أقصى",
        L"حد", L"إشارة", L"طول", L"نص", L"رقم", L"يحتوي"
    };
    return names;
}

bool Compiler::compile(std::wstring_view source, Program& program, std::vector<Diagnostic>& diagnostics) const {
    program.code.clear();
    program.functions.clear();
    diagnostics.clear();
    std::vector<std::pair<int, std::wstring>> lines;
    std::wistringstream input{std::wstring(source)};
    std::wstring raw;
    int line_number = 0;
    while (std::getline(input, raw)) {
        ++line_number;
        bool in_string = false;
        bool escaped = false;
        for (size_t i = 0; i < raw.size(); ++i) {
            const wchar_t c = raw[i];
            if (c == L'\\' && in_string && !escaped) { escaped = true; continue; }
            if (c == L'"' && !escaped) in_string = !in_string;
            if (c == L'#' && !in_string) { raw.erase(i); break; }
            escaped = false;
        }
        lines.emplace_back(line_number, trim(raw));
    }
    StatementCompiler compiler(std::move(lines), program, diagnostics);
    return compiler.run();
}

bool VirtualMachine::run(const Program& program, std::vector<std::wstring>& output, std::vector<Diagnostic>& diagnostics) const {
    return run(program, output, diagnostics, {}, nullptr);
}

bool VirtualMachine::run(const Program& program, std::vector<std::wstring>& output, std::vector<Diagnostic>& diagnostics, const NativeFunction& native_function) const {
    return run(program, output, diagnostics, native_function, nullptr);
}

bool VirtualMachine::run(const Program& program, std::vector<std::wstring>& output, std::vector<Diagnostic>& diagnostics, const NativeFunction& native_function, Globals* globals) const {
    output.clear();
    diagnostics.clear();
    std::vector<Value> stack;
    std::vector<Frame> frames(1);
    Globals fallback_globals;
    Globals& shared = globals ? *globals : fallback_globals;
    constexpr size_t maximum_steps = 1000000;
    constexpr size_t maximum_frames = 200;
    size_t steps = 0;
    int current_line = 0;

    auto fail = [&](const std::wstring& message) {
        diagnostics.push_back({current_line, message, 0});
        return false;
    };
    auto pop = [&](Value& value) {
        if (stack.empty()) return false;
        value = std::move(stack.back());
        stack.pop_back();
        return true;
    };
    auto lookup = [&](const std::wstring& name) -> Value* {
        auto local = frames.back().locals.find(name);
        if (local != frames.back().locals.end()) return &local->second;
        auto global = shared.find(name);
        if (global != shared.end()) return &global->second;
        return nullptr;
    };

    int pc = 0;
    while (pc >= 0 && pc < static_cast<int>(program.code.size())) {
        if (++steps > maximum_steps) { current_line = 0; return fail(L"تم إيقاف البرنامج لأنه تجاوز حد التنفيذ؛ ربما توجد حلقة لا تنتهي."); }
        const Instruction& instruction = program.code[static_cast<size_t>(pc)];
        current_line = instruction.line;
        int next = pc + 1;
        switch (instruction.op) {
            case OpCode::Halt:
                return true;
            case OpCode::PushString:
                stack.push_back(Value::from_text(instruction.text));
                break;
            case OpCode::PushNumber:
                stack.push_back(Value::from_number(instruction.number));
                break;
            case OpCode::PushBool:
                stack.push_back(Value::from_bool(instruction.boolean));
                break;
            case OpCode::Load: {
                const Value* found = lookup(instruction.text);
                if (!found) return fail(L"المتغير غير معرّف: " + instruction.text);
                stack.push_back(*found);
                break;
            }
            case OpCode::Store: {
                Value value;
                if (!pop(value)) return fail(L"خطأ داخلي أثناء تخزين المتغير.");
                frames.back().locals[instruction.text] = std::move(value);
                break;
            }
            case OpCode::StoreGlobalInit: {
                Value value;
                if (!pop(value)) return fail(L"خطأ داخلي أثناء تخزين المتغير.");
                if (!shared.count(instruction.text)) shared[instruction.text] = std::move(value);
                break;
            }
            case OpCode::Assign: {
                Value value;
                if (!pop(value)) return fail(L"خطأ داخلي أثناء تخزين المتغير.");
                Value* found = lookup(instruction.text);
                if (!found) return fail(L"المتغير غير معرّف: " + instruction.text + L"؛ عرّفه أولاً باستخدام عرّف.");
                *found = std::move(value);
                break;
            }
            case OpCode::Add:
            case OpCode::Subtract:
            case OpCode::Multiply:
            case OpCode::Divide:
            case OpCode::Modulo:
            case OpCode::Equal:
            case OpCode::NotEqual:
            case OpCode::Greater:
            case OpCode::Less:
            case OpCode::GreaterEqual:
            case OpCode::LessEqual:
            case OpCode::And:
            case OpCode::Or: {
                Value right;
                Value left;
                if (!pop(right) || !pop(left)) return fail(L"قيم غير كافية للعملية.");
                const OpCode op = instruction.op;
                if (op == OpCode::And) { stack.push_back(Value::from_bool(truthy(left) && truthy(right))); break; }
                if (op == OpCode::Or) { stack.push_back(Value::from_bool(truthy(left) || truthy(right))); break; }
                if (op == OpCode::Add && (left.type == Value::Type::String || right.type == Value::Type::String)) {
                    stack.push_back(Value::from_text(value_to_string(left) + value_to_string(right)));
                    break;
                }
                if (op == OpCode::Equal || op == OpCode::NotEqual) {
                    const bool equal = same_value(left, right);
                    stack.push_back(Value::from_bool(op == OpCode::Equal ? equal : !equal));
                    break;
                }
                if (left.type == Value::Type::String && right.type == Value::Type::String && (op == OpCode::Greater || op == OpCode::Less || op == OpCode::GreaterEqual || op == OpCode::LessEqual)) {
                    const int order = left.text.compare(right.text);
                    const bool result = op == OpCode::Greater ? order > 0 : op == OpCode::Less ? order < 0 : op == OpCode::GreaterEqual ? order >= 0 : order <= 0;
                    stack.push_back(Value::from_bool(result));
                    break;
                }
                if (left.type != Value::Type::Number || right.type != Value::Type::Number) return fail(L"هذه العملية تحتاج إلى رقمين.");
                const double a = left.number;
                const double b = right.number;
                switch (op) {
                    case OpCode::Add: stack.push_back(Value::from_number(a + b)); break;
                    case OpCode::Subtract: stack.push_back(Value::from_number(a - b)); break;
                    case OpCode::Multiply: stack.push_back(Value::from_number(a * b)); break;
                    case OpCode::Divide:
                        if (b == 0.0) return fail(L"لا يمكن القسمة على صفر.");
                        stack.push_back(Value::from_number(a / b));
                        break;
                    case OpCode::Modulo:
                        if (b == 0.0) return fail(L"لا يمكن حساب باقي القسمة على صفر.");
                        stack.push_back(Value::from_number(std::fmod(a, b)));
                        break;
                    case OpCode::Greater: stack.push_back(Value::from_bool(a > b)); break;
                    case OpCode::Less: stack.push_back(Value::from_bool(a < b)); break;
                    case OpCode::GreaterEqual: stack.push_back(Value::from_bool(a >= b)); break;
                    default: stack.push_back(Value::from_bool(a <= b)); break;
                }
                break;
            }
            case OpCode::Negate: {
                Value value;
                if (!pop(value) || value.type != Value::Type::Number) return fail(L"السالب يحتاج إلى رقم.");
                value.number = -value.number;
                stack.push_back(std::move(value));
                break;
            }
            case OpCode::Not: {
                Value value;
                if (!pop(value)) return fail(L"ليس يحتاج إلى قيمة.");
                stack.push_back(Value::from_bool(!truthy(value)));
                break;
            }
            case OpCode::Call: {
                if (instruction.target < 0 || instruction.target > static_cast<int>(stack.size())) return fail(L"عدد معاملات الدالة غير صالح.");
                std::vector<Value> arguments(static_cast<size_t>(instruction.target));
                for (int i = instruction.target - 1; i >= 0; --i) {
                    if (!pop(arguments[static_cast<size_t>(i)])) return fail(L"تعذر قراءة معاملات الدالة: " + instruction.text);
                }
                const auto user = program.functions.find(instruction.text);
                if (user != program.functions.end()) {
                    const Function& function = user->second;
                    if (function.parameters.size() != arguments.size()) return fail(L"عدد معاملات الدالة " + instruction.text + L" يجب أن يكون " + std::to_wstring(function.parameters.size()) + L".");
                    if (frames.size() >= maximum_frames) return fail(L"تجاوز استدعاء الدوال المتداخلة الحد المسموح.");
                    Frame frame;
                    frame.return_pc = pc + 1;
                    for (size_t i = 0; i < arguments.size(); ++i) frame.locals[function.parameters[i]] = std::move(arguments[i]);
                    frames.push_back(std::move(frame));
                    next = function.entry;
                    break;
                }
                Value result;
                bool handled = false;
                const std::wstring& name = instruction.text;
                const bool one_number = arguments.size() == 1 && arguments[0].type == Value::Type::Number;
                const bool two_numbers = arguments.size() == 2 && arguments[0].type == Value::Type::Number && arguments[1].type == Value::Type::Number;
                auto number = [&](double value) { result = Value::from_number(value); handled = true; };
                if (one_number) {
                    const double x = arguments[0].number;
                    if (name == L"مطلق") number(std::fabs(x));
                    else if (name == L"جذر" && x >= 0.0) number(std::sqrt(x));
                    else if (name == L"جيب") number(std::sin(x));
                    else if (name == L"جيب_تمام") number(std::cos(x));
                    else if (name == L"مستقيم") number(std::tan(x));
                    else if (name == L"تقريب") number(std::round(x));
                    else if (name == L"أرضية") number(std::floor(x));
                    else if (name == L"سقف") number(std::ceil(x));
                    else if (name == L"إشارة") number(x > 0.0 ? 1.0 : (x < 0.0 ? -1.0 : 0.0));
                } else if (two_numbers) {
                    const double x = arguments[0].number;
                    const double y = arguments[1].number;
                    if (name == L"قوة") { const double value = std::pow(x, y); if (std::isfinite(value)) number(value); }
                    else if (name == L"أدنى") number(std::min(x, y));
                    else if (name == L"أقصى") number(std::max(x, y));
                } else if (arguments.size() == 3 && name == L"حد" && arguments[0].type == Value::Type::Number && arguments[1].type == Value::Type::Number && arguments[2].type == Value::Type::Number) {
                    number(std::min(std::max(arguments[0].number, arguments[1].number), arguments[2].number));
                } else if (arguments.size() == 1 && name == L"طول" && arguments[0].type == Value::Type::String) {
                    number(static_cast<double>(arguments[0].text.size()));
                } else if (arguments.size() == 1 && name == L"نص") {
                    result = Value::from_text(value_to_string(arguments[0]));
                    handled = true;
                } else if (arguments.size() == 1 && name == L"رقم") {
                    if (arguments[0].type == Value::Type::Number) number(arguments[0].number);
                    else if (arguments[0].type == Value::Type::Bool) number(arguments[0].boolean ? 1.0 : 0.0);
                    else {
                        std::vector<Token> tokens;
                        std::wstring error;
                        const std::wstring text = trim(arguments[0].text);
                        const bool negative = !text.empty() && text[0] == L'-';
                        if (lex(negative ? text.substr(1) : text, tokens, error) && tokens.size() == 2 && tokens[0].type == TokenType::Number) number(negative ? -tokens[0].number : tokens[0].number);
                        else return fail(L"لا يمكن تحويل النص إلى رقم: " + arguments[0].text);
                    }
                } else if (arguments.size() == 2 && name == L"يحتوي" && arguments[0].type == Value::Type::String && arguments[1].type == Value::Type::String) {
                    result = Value::from_bool(arguments[0].text.find(arguments[1].text) != std::wstring::npos);
                    handled = true;
                }
                if (!handled && native_function) {
                    std::wstring error;
                    handled = native_function(name, arguments, result, error);
                    if (!handled && !error.empty()) return fail(error);
                }
                if (!handled) return fail(L"الدالة غير معروفة أو معاملاتها غير صالحة: " + name);
                stack.push_back(std::move(result));
                break;
            }
            case OpCode::Return: {
                Value value;
                if (!pop(value)) return fail(L"لا توجد قيمة للإرجاع.");
                if (frames.size() <= 1) return fail(L"أرجع خارج دالة.");
                next = frames.back().return_pc;
                frames.pop_back();
                stack.push_back(std::move(value));
                break;
            }
            case OpCode::Jump:
                if (instruction.target < 0 || instruction.target > static_cast<int>(program.code.size())) return fail(L"قفزة داخلية غير صالحة.");
                next = instruction.target;
                break;
            case OpCode::JumpIfFalse: {
                Value value;
                if (!pop(value)) return fail(L"شرط فارغ.");
                if (!truthy(value)) {
                    if (instruction.target < 0 || instruction.target > static_cast<int>(program.code.size())) return fail(L"قفزة شرطية غير صالحة.");
                    next = instruction.target;
                }
                break;
            }
            case OpCode::Print: {
                Value value;
                if (!pop(value)) return fail(L"لا توجد قيمة للطباعة.");
                output.push_back(value_to_string(value));
                break;
            }
            case OpCode::Discard: {
                Value value;
                if (!pop(value)) return fail(L"لا توجد قيمة للتجاهل.");
                break;
            }
        }
        pc = next;
    }
    current_line = 0;
    return fail(L"انتهى البرنامج دون تعليمة توقف.");
}

}
