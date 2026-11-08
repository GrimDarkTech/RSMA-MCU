# RSMA MCU

`rsma-mcu` - среда сборки и исполнения кастомных прошивок виртуальных микроконтроллеров RSMA.

Проект предназначен для сценария, в котором разработчик пишет C/C++-подобную прошивку в стиле Arduino или STM32-HAL, а `rsma-build` собирает ее в переносимый пакет `.rsfw`. Компонент Virtual MCU в RSMA исполняет bytecode этого пакета и связывает виртуальные GPIO, таймеры, UART, PWM и другие периферийные блоки с моделью сцены.

```text
Firmware source
    |
    v
RSMA Firmware Builder + bundled Clang/LLVM
    |
    v
RSMA Firmware Package (.rsfw)
    |
    v
C# Virtual MCU in RSMA
    |
    v
RSMA peripheral and signal backends
    |
    +--> Emulator backend
    +--> HITL serial/network backend
```

## Статус

Репозиторий находится на этапе проектирования и подготовки toolchain. Первый инкремент должен включать:

- формат `.rsfw` версии 1;
- bytecode VM с памятью, стеком, вызовами, ветвлениями и ожиданием;
- Arduino-like GPIO API;
- модель платы STM32F103-подобного профиля;
- виртуальные GPIO, SysTick, таймер, UART и прерывания;
- общий контракт для EmulatorBackend и HitlBackend;
- тестовый сценарий `Blink -> Button -> Interrupt -> PWM -> UART`.

Подробные решения находятся в [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), [docs/FIRMWARE-FORMAT.md](docs/FIRMWARE-FORMAT.md) и [docs/DEVELOPMENT-PLAN.md](docs/DEVELOPMENT-PLAN.md).

## Сборка

```powershell
git submodule update --init --recursive
cmake -S . -B build -DRSMA_MCU_BUILD_TESTS=OFF
cmake --build build --config Release
```

Пока сборка создает только проверочный инструмент `rsma-mcu-info`. Полная сборка bundled Clang/LLVM описана в [docs/BUILDING.md](docs/BUILDING.md) и выполняется отдельным bootstrap-шагом.

## LLVM и Clang

LLVM подключается одним submodule `external/llvm-project`. Clang является частью монорепозитория LLVM, поэтому отдельный submodule для Clang не нужен.

Submodule фиксируется на конкретном commit. Это обеспечивает воспроизводимую версию компилятора и позволяет пользователю получить исходники одной командой. Готовые бинарники не хранятся в Git: они собираются в локальный каталог `toolchains/llvm` и исключены из репозитория.

Важно: submodule избавляет от ручного поиска и настройки версии Clang/LLVM, но первоначальная сборка все равно требует системный bootstrap-компилятор, CMake, Ninja или генератор Visual Studio и Git.
