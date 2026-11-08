# Архитектура RSMA MCU

## 1. Назначение

RSMA MCU - виртуальная платформа микроконтроллера, а не эмуляция конкретного набора инструкций ARM или AVR. Ее задача - дать воспроизводимое исполнение прошивки с привычной моделью Arduino/STM32-HAL и связать ее с виртуальной периферией RSMA.

Прошивка, собранная для `rsma-vm`, не является бинарно совместимой с физическим STM32 или Arduino. Совместимость достигается на уровне исходного API и поведения, а не машинного кода.

## 2. Слои

```text
Application API
    Arduino-like API / RSMA HAL
              |
Firmware Builder
    Clang frontend -> RSMA bytecode backend -> .rsfw
              |
Virtual MCU runtime
    CPU VM / memory / scheduler / interrupts
              |
MCU platform model
    board profile / GPIO / timer / UART / ADC / SPI / I2C
              |
I/O backend
    in-process RSMA / external emulator / HITL
              |
RSMA scene and peripherals
```

Каждый слой должен иметь тестируемый контракт. Класс Unity-периферии не должен знать, исполняется ли прошивка внутри C# VM, во внешнем процессе или на физической плате.

## 3. Режимы исполнения

### In-process VM

Основной режим для быстрого SITL. C#-компонент RSMA загружает `.rsfw` и исполняет VM в процессе Unity. Сигналы GPIO и периферия передаются без сетевого hop.

### External VM

Опциональный режим для изоляции и профилирования. VM работает отдельным процессом, а RSMA использует IPC. Для команд и редких событий допустим ZeroMQ; для высокочастотных сигналов нужен shared-memory ring buffer или эквивалентный транспорт.

### HITL

Физическая плата подключается через Serial/USB или Ethernet Bridge. Используется тот же boundary-контракт сигналов, что и у VM. HITL добавляет timestamp, sequence number, heartbeat, CRC, таймаут и безопасное состояние при потере связи.

## 4. Виртуальная машина

VM содержит:

- program counter;
- виртуальные регистры и flags;
- call stack и data stack;
- Flash с bytecode и константами;
- RAM для глобальных данных, heap-подмножества и стека;
- memory-mapped peripheral space;
- scheduler событий;
- interrupt controller.

Первая версия использует интерпретатор. JIT и threaded dispatch рассматриваются только после профилирования.

Минимальные группы инструкций:

- загрузка/сохранение значений;
- integer/float арифметика;
- сравнение и переходы;
- вызов/возврат;
- доступ к памяти;
- `WAIT`, `YIELD` и получение времени;
- запрос/запись периферийного регистра;
- операции прерываний.

## 5. HAL и board profiles

HAL - API виртуальной платформы, а не копия всех функций конкретного производителя. Он должен иметь совместимую семантику для выбранного подмножества:

- `pinMode`, `digitalWrite`, `digitalRead`;
- `delay`, `millis`;
- GPIO configuration, read/write/toggle;
- timer/PWM;
- UART;
- external interrupt;
- ADC на следующем этапе.

Board profile описывает mapping Arduino pin names или STM32 ports/pins на виртуальные периферийные блоки, ограничения частот, память, reset state и поддержанные функции. Mapping не должен быть зашит в C#-эмулятор.

## 6. Электрическая модель

Состояние вывода разделяется на:

- configuration: input/output/alternate/analog;
- pull: none/up/down;
- output type: push-pull/open-drain;
- output latch;
- effective line level;
- input-sampled level;
- high impedance и conflict state.

Простое `float value` из текущего GPIO-слоя RSMA не является достаточным контрактом для новой VM, но существующий API должен мигрироваться постепенно.

## 7. Границы ответственности

- Builder отвечает за исходный код, проверку API, bytecode, metadata и диагностику.
- VM отвечает за исполнение bytecode, память, время и события.
- Board model отвечает за регистры/периферию виртуальной платы.
- Backend отвечает за связь с RSMA или физическим устройством.
- RSMA peripheral отвечает за реакцию на сигнал и физическую/визуальную модель.
