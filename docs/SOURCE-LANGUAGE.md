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
`spiTransfer(bus, value)` и `i2cTransfer(address, value)` принимают route/address
как compile-time integer и TX byte как runtime expression; они понижаются в
stack-based `HAL_CALL`, возвращая RX byte как `int`. SPI bus ограничен `0..255`,
I2C address ограничен `0..127`.
Synchronous DMA API `dmaTransfer(channel, value)` сохраняется для обмена одним
byte. Bounded asynchronous DMA использует четыре staging buffers по 256 byte и
очередь до 16 незакрытых transfers: `dmaBufferWrite(buffer, offset, value)` и
`dmaBufferRead(buffer, offset)` обращаются к TX staging buffer;
`dmaSubmit(channel, buffer, length)` копирует TX snapshot и возвращает ticket,
`DMA_QUEUE_FULL` при заполненной очереди. Допустимы channels/buffers `0..3`,
offset `0..255`, length `1..256`.
`dmaStatus(ticket)` возвращает `DMA_QUEUED`, `DMA_ACTIVE`, `DMA_COMPLETE` либо
`dmaReadByte(ticket, offset)` читает RX только после completion,
`dmaRelease(ticket)` освобождает завершённый transfer; invalid status/release
используют `DMA_INVALID`. Максимум один transfer на
channel активен одновременно; разные channels выполняются параллельно, transfers
одного channel запускаются FIFO. VM HALT и `VirtualMachine.Reset()` не изменяют
hardware queue; явный `VirtualGpioBank.ResetDma()` отменяет transfers и очищает
staging buffers. Completion детерминированно следует simulated time и
`VirtualGpioBank.DmaByteTimeMilliseconds`; default — 0.01 ms на byte. Firmware
должна yield/wait между submit и polling: status polling само не продвигает
виртуальное время. Буфер snapshot можно сразу переиспользовать после submit.
EEPROM-подобное storage предоставляет `eepromRead(address)` и
`eepromWrite(address, value)`: address `0..255` должен быть compile-time
константой, write value может быть runtime expression и ограничен byte range.
Read возвращает byte на VM stack, write снимает byte со stack. Storage принадлежит
HAL backend, поэтому сохраняется между `VirtualMachine.Reset()` и очищается только
явной backend-операцией reset/clear.
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

Поддерживаются board profiles `RSMA-STM32F103` и `RSMA-AVR-ATmega328P`.
Профиль проверяет board ID, flash/RAM capacity и список требуемых периферий.
STM32F103 предоставляет GPIO, SYSTICK, IRQ, PWM, UART, ADC, SPI, I2C, DMA и
EEPROM; ATmega328P предоставляет GPIO, SYSTICK, IRQ, PWM, UART, ADC, SPI, I2C и
EEPROM, но не DMA. Firmware с несовместимым board ID, превышенной capacity или
неподдерживаемой периферией отклоняется profile-aware validator-ом.