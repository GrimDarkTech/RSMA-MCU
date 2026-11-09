# Написание и сборка прошивок RSMA Virtual MCU

## 1. Важное ограничение текущей версии

`rsma-mcu` уже содержит формат `.rsfw`, validator, bytecode VM-контракт и native writer/reader. Полноценный компилятор Arduino/C++-подобного исходного кода в bytecode пока не включён в репозиторий.

Поэтому текущий рабочий способ написания прошивки — это C++-программа, которая создаёт `FirmwarePackage`, заполняет его bytecode-инструкциями и записывает пакет через `write_firmware()`.

Пример готовой прошивки находится здесь:

```text
F:\RSMA-Refactored\rsma-mcu\examples\pwm_motor_test.cpp
```

Она предназначена для подключения:

```text
Virtual pin 10 -> motor driver IN1
Virtual pin 11 -> motor driver IN2
Virtual pin 13 -> motor driver PWM
```

Пакет `.rsfw` не является физическим STM32 `.elf`, `.hex` или `.bin`. Это переносимый пакет bytecode для `RSMAVirtualMicrocontroller`.

## 2. Быстрая сборка PWM-теста на Windows

Команды выполняются из каталога `rsma-mcu`:

```powershell
cd F:\RSMA-Refactored\rsma-mcu

git submodule update --init --recursive

cmake -S . -B build -DRSMA_MCU_BUILD_TOOLS=ON -DRSMA_MCU_BUILD_TESTS=ON
cmake --build build --config Release --target rsma-pwm-motor-test rsma-firmware-inspect
```

Создать прошивку в Unity Resources:

```powershell
New-Item -ItemType Directory -Force `
  F:\RSMA-Refactored\RSMA\Assets\Resources\RSMA\Firmware | Out-Null

build\Release\rsma-pwm-motor-test.exe `
  F:\RSMA-Refactored\RSMA\Assets\Resources\RSMA\Firmware\pwm_motor_test.rsfw.bytes
```

Проверить созданный пакет:

```powershell
build\Release\rsma-firmware-inspect.exe `
  F:\RSMA-Refactored\RSMA\Assets\Resources\RSMA\Firmware\pwm_motor_test.rsfw.bytes
```

Ожидаемые основные строки:

```text
Board: RSMA-STM32F103
Entry: reset (instruction 0)
Instructions: 47
Peripherals: GPIO SYSTICK TIM
```

Для полного запуска native-тестов:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

## 3. Подключение прошивки в Unity

После генерации файл должен находиться в Unity-проекте по абсолютному пути:

```text
F:\RSMA-Refactored\RSMA\Assets\Resources\RSMA\Firmware\pwm_motor_test.rsfw.bytes
```

Unity импортирует `.bytes` как `TextAsset`. Суффикс `.bytes` нужен, чтобы бинарный пакет не был обработан как обычный текст.

В компоненте `RSMAVirtualMicrocontroller` назначьте поле `Firmware` на этот `TextAsset`. Если asset загружается по умолчанию через `Resources`, его имя без расширения:

```text
RSMA/Firmware/pwm_motor_test
```

В `RSMAMotorDriver` назначьте:

```text
Virtual Microcontroller = тот же RSMAVirtualMicrocontroller
Virtual Pin 1           = 10
Virtual Pin 2           = 11
Virtual PWM Pin         = 13
```

Для стандартного профиля `RSMA-STM32F103` эти virtual pin IDs должны быть привязаны к соответствующим GPIO pins через default board profile. При использовании пользовательских bindings добавьте три записи в `RSMAVirtualMicrocontroller.pinBindings` и проверьте, что они указывают на реальные pins `RSMAGPIO`.

## 4. Что делает пример `pwm_motor_test.cpp`

Прошивка сначала переводит три выхода в `Output`:

```cpp
{Opcode::HalCall, hal(HalService::PinMode, 10, 1)},
{Opcode::HalCall, hal(HalService::PinMode, 11, 1)},
{Opcode::HalCall, hal(HalService::PinMode, 13, 1)},
```

Затем она повторяет цикл:

1. Остановка: `IN1=0`, `IN2=0`, `PWM=0`.
2. Вперёд: `IN1=1`, `IN2=0`.
3. PWM duty: `64`, `128`, `192`, `255`, затем обратно до `0`.
4. Остановка.
5. Назад: `IN1=0`, `IN2=1`.
6. Повтор того же PWM ramp.
7. Остановка обоих direction pins и переход к началу цикла.

Каждый промежуточный уровень удерживается 500 ms, максимальный duty — 1000 ms.

`PwmWrite` использует диапазон `0..255`:

```text
0   = 0%
64  ≈ 25%
128 ≈ 50%
192 ≈ 75%
255 = 100%
```

В Unity это представляется как нормализованное значение `0..1`. В текущей модели PWM не создаёт отдельные высокочастотные импульсы, а передаёт усреднённый duty cycle компонентам сцены.

## 5. Bytecode и HAL_CALL

Каждая HAL-инструкция имеет ровно четыре operand bytes:

```text
[service low byte, service high byte, argument/pin, value]
```

Например:

```cpp
hal(HalService::PwmWrite, 13, 128)
```

создаёт operand bytes:

```text
07 00 0D 80
```

где:

```text
07 00 = service PwmWrite (ID 7, little-endian)
0D    = pin 13
80    = duty 128
```

Поддерживаемые HAL service IDs:

| ID | Service | `argument` | `value` |
|---:|---|---|---|
| 1 | `PinMode` | virtual pin | `0=Input`, `1=Output` |
| 2 | `DigitalWrite` | virtual pin | `0=Low`, `1=High` |
| 3 | `DigitalRead` | virtual pin | игнорируется; результат попадает в VM stack |
| 4 | `DelayMs` | не используется | задержка `0..255` ms |
| 5 | `Millis` | не используется | игнорируется; результат попадает в VM stack |
| 6 | `ConfigurePin` | virtual pin | packed mode/pull/output type |
| 7 | `PwmWrite` | virtual pin | duty `0..255` |

Детальное описание wire format находится в [FIRMWARE-FORMAT.md](FIRMWARE-FORMAT.md).

## 6. Инструкции управления временем и циклом

`WAIT` принимает `uint32` milliseconds в little-endian:

```cpp
{Opcode::Wait, u32(500)},
```

`JUMP` использует абсолютный индекс инструкции, а не относительное смещение:

```cpp
{Opcode::Jump, u32(3)},
```

Это возвращает исполнение к инструкции с индексом `3`.

При добавлении или удалении инструкций нужно проверять все абсолютные targets `JUMP`, `JUMP_IF`, `CALL` и `ATTACH_IRQ`.

## 7. Как создать собственную прошивку

### Шаг 1. Скопировать пример

```powershell
Copy-Item `
  F:\RSMA-Refactored\rsma-mcu\examples\pwm_motor_test.cpp `
  F:\RSMA-Refactored\rsma-mcu\examples\my_firmware.cpp
```

Измените функцию `make_pwm_motor_test_package()` или переименуйте её, оставив создание `FirmwarePackage`.

### Шаг 2. Указать manifest

Минимальные поля:

```cpp
package.manifest.boardId = "RSMA-STM32F103";
package.manifest.flashSize = 64 * 1024;
package.manifest.ramSize = 20 * 1024;
package.manifest.entryPointName = "reset";
package.manifest.requiredPeripherals = {"GPIO", "SYSTICK"};
```

`boardId` должен совпадать с `expectedBoardId` компонента `RSMAVirtualMicrocontroller`, если это поле не очищено в Inspector.

### Шаг 3. Собрать bytecode

Каждая запись добавляется в `package.bytecode.instructions`:

```cpp
package.bytecode.entryPoint = 0;
package.bytecode.instructions = {
    {Opcode::HalCall, hal(HalService::PinMode, 13, 1)},
    {Opcode::HalCall, hal(HalService::PwmWrite, 13, 128)},
    {Opcode::Wait, u32(500)},
    {Opcode::HalCall, hal(HalService::PwmWrite, 13, 0)},
    {Opcode::Jump, u32(1)},
};
```

### Шаг 4. Добавить executable target

В `CMakeLists.txt` добавьте отдельную цель по аналогии с PWM-примером:

```cmake
add_executable(my-firmware examples/my_firmware.cpp)
target_link_libraries(my-firmware PRIVATE rsma_mcu_core)
```

После изменения CMake-конфигурации повторите configure и build:

```powershell
cmake -S . -B build -DRSMA_MCU_BUILD_TOOLS=ON
cmake --build build --config Release --target my-firmware
```

### Шаг 5. Записать пакет

Программа должна вызвать:

```cpp
write_firmware(make_my_package(), outputPath);
```

Генерируйте сразу в Unity Resources:

```powershell
build\Release\my-firmware.exe `
  F:\RSMA-Refactored\RSMA\Assets\Resources\RSMA\Firmware\my_firmware.rsfw.bytes
```

### Шаг 6. Проверить до запуска Unity

```powershell
build\Release\rsma-firmware-inspect.exe `
  F:\RSMA-Refactored\RSMA\Assets\Resources\RSMA\Firmware\my_firmware.rsfw.bytes
```

Если пакет повреждён, содержит неизвестный HAL service, неправильное число operands или неверный jump target, reader/validator завершится с ошибкой и Unity не должна запускать частично загруженную прошивку.

## 8. Unity `.meta` файл

При создании файла из PowerShell Unity обычно сама создаёт `.meta` после импорта. Если файл добавляется в Git до открытия Unity, можно:

1. открыть проект Unity и дождаться импорта asset;
2. добавить созданный `.meta` в Git;
3. не удалять `.meta` при переносе прошивки.

Файл `.rsfw.bytes` и соответствующий `.meta` должны находиться рядом:

```text
my_firmware.rsfw.bytes
my_firmware.rsfw.bytes.meta
```

## 9. Проверка виртуального исполнения

После загрузки Unity выполнить Play Mode и проверить:

- pin переведён в `Output` до `DigitalWrite`/`PwmWrite`;
- `PwmWrite(13, 0)` выключает LED/мотор;
- `PwmWrite(13, 128)` даёт приблизительно 50%;
- `DigitalWrite(10, 1), DigitalWrite(11, 0)` дают одно направление;
- `DigitalWrite(10, 0), DigitalWrite(11, 1)` дают противоположное направление;
- оба direction pins `0` останавливают мотор;
- при выключении `RSMAVirtualMicrocontroller` выходы переходят в safe state.

Для автоматической проверки пакета следует использовать C# VM tests и native CTest. Проверка только наличия `.rsfw` в каталоге недостаточна: она не гарантирует валидность checksum, operand width и board profile.

## 10. Будущее расширение

Планируемый следующий уровень toolchain — отдельный frontend, который будет принимать более удобный исходник, например Arduino-подобный C++:

```cpp
void setup() { pinMode(13, OUTPUT); }
void loop() { analogWrite(13, 128); delay(500); }
```

Такой frontend должен компилировать исходник в тот же проверяемый `FirmwarePackage`. До его появления необходимо использовать явный C++ builder из `examples/` или существующие factory functions в native tools.