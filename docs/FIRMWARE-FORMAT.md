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
