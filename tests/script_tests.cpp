#include "risha/script/Script.hpp"
#include <iostream>
#include <vector>

int main() {
    risha::script::Compiler compiler;
    risha::script::VirtualMachine vm;
    risha::script::Program program;
    std::vector<risha::script::Diagnostic> diagnostics;
    std::vector<std::wstring> output;

    const std::wstring source =
        L"عرّف الاسم = \"ريشة # لغة\"\n"
        L"عرّف الرقم = 20\n"
        L"اطبع الاسم + \" 0.4\"\n"
        L"اطبع الرقم + 22\n"
        L"اطبع 7 * 6 - 5\n"
        L"اطبع 20 / 4\n"
        L"إذا الرقم > 10\n"
        L"اطبع \"شرط صحيح\"\n"
        L"وإلا\n"
        L"اطبع \"شرط خطأ\"\n"
        L"نهاية\n"
        L"كرر 3\n"
        L"اطبع \"تكرار\"\n"
        L"نهاية\n";

    if (!compiler.compile(source, program, diagnostics)) return 1;
    if (!vm.run(program, output, diagnostics)) return 2;
    if (output.size() != 8) return 3;
    if (output[0] != L"ريشة # لغة 0.4") return 4;
    if (output[1] != L"42") return 5;
    if (output[2] != L"37") return 6;
    if (output[3] != L"5") return 7;
    if (output[4] != L"شرط صحيح") return 8;
    if (output[5] != L"تكرار" || output[6] != L"تكرار" || output[7] != L"تكرار") return 9;

    output.clear();
    diagnostics.clear();
    const std::wstring nested =
        L"كرر 2\n"
        L"    كرر 2\n"
        L"        اطبع \"داخل\"\n"
        L"    نهاية\n"
        L"نهاية\n";
    if (!compiler.compile(nested, program, diagnostics)) return 10;
    if (!vm.run(program, output, diagnostics)) return 11;
    if (output.size() != 4) return 12;

    output.clear();
    diagnostics.clear();
    if (!compiler.compile(L"كرر -1\nاطبع \"خطأ\"\nنهاية\n", program, diagnostics)) return 13;
    if (!vm.run(program, output, diagnostics)) return 14;
    if (!output.empty()) return 15;

    output.clear();
    diagnostics.clear();
    if (!compiler.compile(L"اطبع المفقود\n", program, diagnostics)) return 16;
    if (vm.run(program, output, diagnostics)) return 17;
    if (diagnostics.empty()) return 18;

    output.clear();
    diagnostics.clear();
    const std::wstring assignment =
        L"عرّف عداد = 3\n"
        L"طالما عداد > 0\n"
        L"اطبع عداد\n"
        L"غيّر عداد = عداد - 1\n"
        L"نهاية\n";
    if (!compiler.compile(assignment, program, diagnostics)) return 19;
    if (!vm.run(program, output, diagnostics)) return 20;
    if (output.size() != 3 || output[0] != L"3" || output[1] != L"2" || output[2] != L"1") return 21;

    output.clear();
    diagnostics.clear();
    if (!compiler.compile(L"اطبع 5 / 0\n", program, diagnostics)) return 22;
    if (vm.run(program, output, diagnostics)) return 23;
    if (diagnostics.empty()) return 24;

    output.clear();
    diagnostics.clear();
    if (!compiler.compile(L"اطبع (2 + 3) * 4\n", program, diagnostics)) return 25;
    if (!vm.run(program, output, diagnostics)) return 26;
    if (output.size() != 1 || output[0] != L"20") return 27;

    output.clear();
    diagnostics.clear();
    if (!compiler.compile(L"اطبع 17 % 5\nاطبع \"سطر1\\nسطر2\"\n", program, diagnostics)) return 28;
    if (!vm.run(program, output, diagnostics)) return 29;
    if (output.size() != 2 || output[0] != L"2" || output[1] != L"سطر1\nسطر2") return 30;


    output.clear();
    diagnostics.clear();
    if (!compiler.compile(L"اطبع جذر(81) + مطلق(-3)\nاطبع تقريب(2.6)\n", program, diagnostics)) return 31;
    if (!vm.run(program, output, diagnostics)) return 32;
    if (output.size() != 2 || output[0] != L"12" || output[1] != L"3") return 33;

    output.clear();
    diagnostics.clear();
    risha::script::NativeFunction host = [](std::wstring_view name, const std::vector<risha::script::Value>& args, risha::script::Value& result, std::wstring& error) {
        if (name == L"اختبار" && args.size() == 2 && args[0].type == risha::script::Value::Type::Number && args[1].type == risha::script::Value::Type::Number) {
            result = {risha::script::Value::Type::Number, args[0].number + args[1].number};
            return true;
        }
        error = L"اختبار فاشل";
        return false;
    };
    if (!compiler.compile(L"اطبع اختبار(20, 22)\n", program, diagnostics)) return 34;
    if (!vm.run(program, output, diagnostics, host)) return 35;
    if (output.size() != 1 || output[0] != L"42") return 36;

    output.clear();
    diagnostics.clear();
    if (!compiler.compile(L"كرر 1000\nاطبع \"رسالة\"\nنهاية\n", program, diagnostics)) return 37;
    if (!vm.run(program, output, diagnostics)) return 38;
    if (output.size() != 1000) return 39;

    output.clear();
    diagnostics.clear();
    if (!compiler.compile(L"اطبع \"مرحباً من ريشة\"\n", program, diagnostics)) return 40;
    if (!vm.run(program, output, diagnostics)) return 41;
    if (output.size() != 1 || output[0] != L"مرحباً من ريشة") return 42;

    std::cout << "script tests passed\n";
    return 0;
}
