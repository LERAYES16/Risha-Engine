# بناء محرك ريشة 0.5 على Windows

## المتطلبات

- Windows 8.1 أو أحدث.
- GCC/MinGW-w64 64-bit يعمل من Command Prompt عبر `g++ --version`.
- CMake يعمل عبر `cmake --version`.

## البناء التلقائي

من مجلد المشروع شغّل:

```bat
build_windows.bat
```

الملف يقوم بالتأكد من CMake وGCC، ثم يجهز المشروع، ويبنيه، ويشغل الاختبارات.

## البناء اليدوي

```bat
cmake -S . -B build -G "MinGW Makefiles" -DRISHA_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

بعد نجاح الاختبارات:

```bat
build\risha_editor.exe
```

## ملاحظة

كود Windows يستخدم Win32 Unicode APIs ويدعم النص العربي واتجاه RTL. لم يتم إدخال SDL أو GLFW أو مكتبة نافذة خارجية.
