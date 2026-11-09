# Формат RSMA Firmware Package

## 1. Назначение

`.rsfw` - детерминированный пакет прошивки для RSMA Virtual MCU. Это контейнер bytecode, описания целевой платы и данных для диагностики. Он не является ELF, HEX или BIN физического микроконтроллера.

## 2. Версионирование

Формат имеет независимые версии:

- `format_version` - структура контейнера;
- `vm_version` - набор и семантика инструкций;
- `hal_version` - API виртуальной платформы;
- `board_profile_version` - mapping и возможности платы.

Загрузчик обязан отклонять неизвестные major-версии и явно сообщать несовместимую minor-версию.

## 3. Логическая структура

```text
Header
Manifest
Bytecode
Constant pool
Memory image
Board requirements
Symbol table
Debug map
Resource limits
Checksum/signature (optional in v1)
```

Header должен содержать magic `RSFW`, версии, архитектуру VM, размер пакета, число секций и контрольную сумму.

Manifest должен описывать:

```json
{
  "format": 1,
  "vm": 1,
  "hal": 1,
  "board": "RSMA-STM32F103",
  "entryPoint": "reset",
  "flashSize": 65536,
  "ramSize": 20480,
  "requiredPeripherals": ["GPIO", "SYSTICK", "TIM", "UART"]
}
```

Фактический wire format должен быть бинарным и иметь фиксированные integer widths, endian rule и границы секций. JSON выше является только человекочитаемым представлением metadata.

### RSFW v1 wire layout

Все целые числа имеют little-endian порядок байт. Размеры header и section entry фиксированы:

```text
Offset  Size  Field
0       4     magic = "RSFW"
4       2     header_size = 64
6       2     format_major
8       2     format_minor
10      2     vm_version
12      2     hal_version
14      2     reserved
16      4     package_flags
20      4     section_count
24      8     section_table_offset
32      4     entry_point_instruction
36      4     entry_point_section_type
40      8     total_size
48      4     payload_crc32
52      4     header_crc32
56      8     reserved
```

Каждая запись таблицы секций имеет 32 байта:

```text
Offset  Size  Field
0       4     section_type
4       4     section_flags
8       8     payload_offset
16      8     payload_size
24      4     payload_crc32
28      4     reserved
```

В v1 обязательны секции `Manifest` (type 1) и `Bytecode` (type 2). Payload checksum считается по конкатенации payload-секций в порядке таблицы. Header checksum считается по 64-байтовому header с нулевым `header_crc32`.

## 4. Bytecode

Первый набор инструкций:

```text
NOP, CONST, LOAD, STORE
ADD, SUB, MUL, DIV, NEG
CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE
JUMP, JUMP_IF, CALL, RETURN
LOAD_MEM, STORE_MEM
READ_TIME, WAIT, YIELD
READ_PERIPHERAL, WRITE_PERIPHERAL
ENABLE_IRQ, DISABLE_IRQ, RETURN_IRQ
HAL_CALL, TRAP, HALT
```

Инструкции должны использовать числовые IDs, а не строки. Имена функций и исходные строки хранятся только в debug section.

Текущая реализация v1 поддерживает базовый executable subset: `NOP`, `HALT`, `CONST_I32`, `LOAD`, `STORE`, `ADD_I32`, `SUB_I32`, `JUMP`, `JUMP_IF`, `CALL`, `RETURN`, `READ_TIME`, `WAIT`, `ATTACH_IRQ`, `ENABLE_IRQ`, `DISABLE_IRQ`, `RETURN_IRQ` и `HAL_CALL`. Формат operand payload у каждой инструкции задается ее размером в bytecode stream; проверка допустимых размеров выполняется validator-ом.

В v1 `JUMP`, `JUMP_IF` и `CALL` используют абсолютный индекс инструкции в 4-байтовом little-endian operand. `WAIT` содержит длительность в миллисекундах как `uint32`. VM предоставляет 256 слотов `LOAD`/`STORE`, адресуемых двухбайтовым индексом. `HAL_CALL` содержит ровно четыре байта: `service:uint16`, `argument:uint8`, `value:uint8`. HAL service IDs: `PinMode=1`, `DigitalWrite=2`, `DigitalRead=3`, `DelayMs=4`, `Millis=5`, `ConfigurePin=6`, `PwmWrite=7`. Для `ConfigurePin` value packed как `mode bit0`, `pull bits1..2` (`None=0`, `PullUp=1`, `PullDown=2`), `output type bit3` (`PushPull=0`, `OpenDrain=1`); остальные биты зарезервированы. `PwmWrite` задаёт 8-битный duty cycle `0..255` в аргументе `value`; virtual/legacy GPIO представляет его как нормализованное значение `0..1` для компонентов сцены. Длинные задержки кодируются инструкцией `WAIT`; `DelayMs` ограничен 255 ms.

`ATTACH_IRQ` содержит шесть байт: `pin:uint8`, `edge_mask:uint8`, `target_instruction:uint32`. В `edge_mask` bit 0 означает rising, bit 1 — falling; target — абсолютный индекс инструкции обработчика. `ENABLE_IRQ` и `DISABLE_IRQ` не имеют operands. Edge events dispatch-ятся deferred FIFO в simulated timestamp order; `RETURN_IRQ` возвращает VM к прерванному instruction/wait state. Входные события GPIO также timestamped и стабильно упорядочены по `(timestamp, insertion_sequence)`. `VirtualButton` моделирует active-low input: press переводит линию в `0`, release — в `1`; повторная установка текущего уровня не генерирует edge.

## 5. Проверка до исполнения

Загрузчик проверяет:

- magic и версии;
- границы всех секций;
- отсутствие переполнений размеров;
- валидность instruction opcode и operand widths;
- существование entry point;
- ограничения памяти;
- требования board profile;
- checksum;
- лимиты стека и количества событий.

Невалидный пакет не должен частично запускаться.

## 6. Детерминизм

В bytecode запрещены неявные зависимости от wall clock, случайных значений ОС и порядка потоков. Источники недетерминизма должны приходить через явные VM services: simulated time, seeded RNG и external input events.

## 7. Debug information

Debug section может содержать:

- source file и line;
- function ID/name;
- mapping bytecode offset -> source location;
- variable location metadata;
- peripheral symbol names.

Она не должна быть обязательной для исполнения и может удаляться в release package.
