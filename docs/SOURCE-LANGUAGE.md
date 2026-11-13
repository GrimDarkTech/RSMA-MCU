# Контракт исходного языка RSMA MCU

## Точка входа

Первая версия frontend `rsma-build` принимает исходную программу с единственным
определением точки входа:

```cpp
int main()
{
    // Firmware code
}
```

`main` не принимает аргументов. Arduino-функции `setup()` и `loop()` не требуются,
не генерируются и не вызываются неявно. Индекс первой инструкции скомпилированной
функции `main` записывается в поле точки входа bytecode RSFW; диагностическое имя
точки входа в manifest равно `main`. При исполнении используется числовой индекс.

Возврат из `main` завершает VM. Возвращаемое целое число не является кодом
завершения процесса и отбрасывается VM.

## Начальный охват языка

Общий целевой охват frontend соответствует этапу 1 `DEVELOPMENT-PLAN.md`:
целые числа, числа с плавающей точкой и логические значения; функции; условия;
циклы; статическая память. Первый исполняемый срез `rsma-build` уже использует
Clang для разбора C++, но пока принимает в `main()` только прямые вызовы
`pinMode(pin, INPUT|OUTPUT)`, `digitalWrite(pin, LOW|HIGH)`, `digitalRead(pin)`,
`delay(constant)` и `return [constant]`. Поддерживаются локальные `int`/`bool`
переменные, простые присваивания, сложение/вычитание, сравнения `==`, `!=`, `<`,
`<=`, `>`, `>=`, логическое `!`, а также runtime `if/else`, `while` и простой
`for (int i = start; i < end; ++i)`/`--i`. Runtime
условия понижаются в stack bytecode, comparisons — в opcodes `CMP_*_I32`.
Вызов `yield()` без аргументов понижает в opcode `YIELD`: VM приостанавливает
текущий `Advance()` до следующего host tick, не добавляя времени ожидания.
`analogRead(channel)` принимает compile-time channel `0..255` и понижает в
`HAL_CALL` service `AnalogRead`; runtime возвращает 12-bit sample `0..4095`.
Низкоуровневые `HAL_CALL` services `SpiTransfer` и `I2cTransfer` принимают
TX byte со stack и возвращают RX byte на stack; I2C address ограничен `0..127`.
Arduino-like wrappers для SPI/I2C пока не входят в frontend contract.
Поддерживаются глобальные пользовательские `int`/`void` функции с параметрами и
локальными переменными типов `int`/`bool`. Аргументы и `int`-результат передаются
через VM value stack; вызовы компилируются в `CALL`, возвраты — в `RETURN`. VM
сохраняет слоты вызывающего кода в call frame; поддерживается рекурсия. `main()`
остаётся entry point и заканчивается `HALT`. Глобальные `int`/`bool` используют
отдельное persistent VM storage (`GLOBAL_LOAD`/`GLOBAL_STORE`) и compile-time
инициализацию при старте `main()`; storage сохраняется между вызовами функций и
очищается при `Reset()`.
Аргументы pin/mode/delay HAL должны оставаться compile-time константами. `do-while`,
типы функций, локальных и глобальных переменных, отличные от `int`/`bool`, динамические
глобальные initializer-ы, PWM и UART пока отклоняются с
диагностикой; неполная прошивка не создаётся.

Для сборки target `rsma-build` включается опцией
`-DRSMA_MCU_BUILD_BUILDER=ON`; требуется установленная libclang или заданный
`LLVM_INSTALL_PREFIX`. Результат проходит тот же validator, что и остальные
RSFW v1 пакеты.

Builder добавляет optional read-only секции `Symbols` (`SYM1`) и `Debug` (`DBG1`).
`SYM1` version 1 содержит пары `instruction:u32, name:utf8[length:u16]`; `DBG1`
version 1 хранит `instruction:u32, name:utf8[length:u16], file:utf8[length:u16],
line:u32`. Они описывают entry points глобальных функций и не требуются VM для
исполнения.

Frontend формирует существующий `FirmwarePackage` в формате RSFW v1. Физические
MCU targets и их startup/runtime соглашения являются отдельными целями сборки и
не изменяют контракт VM `main()`.