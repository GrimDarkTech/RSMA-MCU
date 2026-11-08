# Сборка

## 1. Требования bootstrap

Пользователю не требуется отдельно искать, скачивать и настраивать совместимую версию Clang/LLVM. Исходники LLVM/Clang находятся в Git submodule `external/llvm-project`, а версия фиксируется commit-ом родительского репозитория.

Однако сборка LLVM неизбежно требует bootstrap-среду:

- Git;
- CMake 3.20 или новее;
- Ninja или Visual Studio generator на Windows;
- системный C++ compiler для первой стадии сборки;
- Python, если это требуется конкретной версией LLVM.

После bootstrap пользователю не нужно добавлять Clang в PATH: проект может использовать `toolchains/llvm/bin/clang` через явно переданный CMake toolchain file.

## 2. Получение исходников

```powershell
git clone <rsma-mcu-repository>
cd rsma-mcu
git submodule update --init --recursive
```

Проверка фиксации:

```powershell
git submodule status
```

Submodule должен быть в состоянии clean и указывать на commit, зафиксированный родительским репозиторием.

## 3. Bootstrap LLVM на Windows

Планируемый скрипт `tools/bootstrap-llvm.ps1` должен:

1. проверить наличие Git/CMake/bootstrap compiler;
2. проверить наличие `external/llvm-project/llvm` и `external/llvm-project/clang`;
3. сконфигурировать LLVM с `LLVM_ENABLE_PROJECTS=clang`;
4. использовать Ninja или выбранный Visual Studio generator;
5. собрать только нужные host tools;
6. установить их в `toolchains/llvm`;
7. записать commit LLVM и параметры сборки в manifest.

Пример ручного эквивалента:

```powershell
cmake -S external/llvm-project/llvm -B external/llvm-project/build `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DLLVM_ENABLE_PROJECTS=clang `
  -DLLVM_TARGETS_TO_BUILD=Native `
  -DCMAKE_INSTALL_PREFIX="$PWD/toolchains/llvm"

cmake --build external/llvm-project/build --target install
```

Для кроссплатформенного CI параметры generator и install prefix должны задаваться скриптом, а не вручную.

## 4. Сборка rsma-mcu

```powershell
cmake -S . -B build -DRSMA_MCU_BUILD_TOOLS=ON
cmake --build build --config Release
```

На Linux/macOS:

```bash
cmake -S . -B build -DRSMA_MCU_BUILD_TOOLS=ON
cmake --build build --config Release
```

## 5. Воспроизводимость

В release metadata фиксируются:

- commit `rsma-mcu`;
- commit `llvm-project`;
- версия CMake;
- generator;
- host/compiler triple;
- параметры LLVM и Builder.

Готовые toolchain binaries не коммитятся в Git и должны публиковаться как versioned CI artifacts или release archives.
