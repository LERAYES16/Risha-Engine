# بناء محرك ريشة 0.5 على Linux

المتطلبات الأساسية هي CMake 3.16 أو أحدث، مترجم C++17، ومكتبات X11 التطويرية.

```text
cmake -S . -B build -DRISHA_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/risha_editor
```

لفتح مشهد مباشرة:

```text
./build/risha_editor examples/demo.risha
```

التثبيت المحلي:

```text
./install_linux.sh
```

التثبيت يضع المحرك في `~/.local/bin` ويضع التوثيق والأمثلة في `~/.local/share/risha` ويضيف مدخلاً إلى قائمة التطبيقات.

الإزالة:

```text
./uninstall_linux.sh
```

لا يحتاج التثبيت المحلي إلى صلاحيات root.
