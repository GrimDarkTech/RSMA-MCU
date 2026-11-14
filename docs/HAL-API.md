# RSMA HAL API

## 1. Принцип

RSMA HAL повторяет полезную семантику Arduino и STM32-HAL, но предоставляет ее виртуальной машине. API разделен на application-level Arduino layer и low-level RSMA HAL.

Один и тот же исходный проект может иметь разные target profiles:

```text
rsma-vm       -> .rsfw
stm32f103     -> physical ARM binary
arduino-nano  -> physical AVR binary
```

Полная бинарная совместимость с vendor HAL не является целью первого релиза.

## 2. GPIO

Минимальная семантика:

```cpp
pinMode(pin, INPUT);
pinMode(pin, INPUT_PULLUP);
pinMode(pin, OUTPUT);
digitalWrite(pin, HIGH);
digitalWrite(pin, LOW);
int value = digitalRead(pin);
```

STM32-подобный слой:

```cpp
HAL_GPIO_Init(port, &config);
HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
HAL_GPIO_TogglePin(port, pin);
GPIO_PinState state = HAL_GPIO_ReadPin(port, pin);
```

Нужно сохранять различие между output latch и effective line level. Для `INPUT` запись может менять pull-up только если это предусмотрено выбранной Arduino-платформой; для STM32 режим конфигурации должен определять допустимые операции.

## 3. Время

```cpp
delay(milliseconds);
uint32_t millis();
HAL_Delay(milliseconds);
HAL_GetTick();
```

В VM эти функции используют simulated time. При паузе Unity поведение определяется режимом clock policy, а не случайным `Time.timeScale` внутри прошивки.

## 4. Таймеры, PWM и UART

Второй инкремент API:

- timer start/stop/configure;
- PWM frequency/duty cycle;
- UART init/transmit/receive;
- receive callbacks и внешние IRQ.

Для VM UART v1 использует четыре фиксированных HAL service ID:

| ID | Service | `argument` | `value` / результат |
|---:|---|---|---|
| 8 | `UartBegin` | порт `0..3` | baud preset `0..4`: `9600`, `19200`, `38400`, `57600`, `115200` |
| 9 | `UartAvailable` | порт `0..3` | количество байт в RX FIFO на VM stack |
| 10 | `UartRead` | порт `0..3` | следующий байт из RX FIFO на VM stack |
| 11 | `UartWrite` | порт `0..3` | байт берётся с VM stack и передаётся наружу |

Каждый UART port имеет FIFO приёма ёмкостью 256 байт. Порядок RX сохраняется. Чтение пустого FIFO и переполнение являются диагностируемыми ошибками HAL. Передача является событием `Transmitted(port, byte)`; Unity может подписаться на него или внедрить входные байты через `InjectUartReceive`.

В bytecode `UartAvailable` и `UartRead` помещают результат на stack. `UartWrite` снимает одно целое значение со stack и принимает только диапазон `0..255`. Пример прошивки с polling и echo находится в `examples/uart_echo.cpp`.

Значения частоты и задержки должны иметь фиксированные единицы и явно документированную точность. Сетевой backend не должен передавать каждую VM-инструкцию; границей обмена являются периферийные события.

## 5. ADC и виртуальный аналоговый датчик

Текущий firmware API предоставляет чтение raw ADC sample:

```cpp
int sample = analogRead(channel); // 0..4095
```

`VirtualAnalogSensor` реализован как Unity component, подключённый к ADC
channel через backend. Pure C# sensor model отделяет физический сигнал от
firmware ADC-контракта и задаёт:

- `waveform`: constant, ramp, sine или пользовательская кривая;
- deterministic `noise` с seed и амплитудой;
- `sampling` period/frequency и момент обновления sample;
- `calibration`: raw range, offset, gain и clamping;
- physical `units` для инспектора и telemetry.

Сигнал вычисляется по simulated time и становится доступен прошивке только
через `analogRead(channel)`. Sensor не вызывает VM напрямую: он регистрирует
sampler в `IVirtualAnalogInputSource`, а runtime обновляет sample при движении
виртуального времени. Один channel должен иметь однозначное подключение;
duplicate sensor, отсутствующий scene mapping и некорректный ADC range
диагностируются при регистрации. Raw значение после calibration ограничивается
диапазоном `0..4095`, а units не влияют на значение, видимое firmware.

Model и backend contract покрыты Unity EditMode tests для waveform, sample-and-
hold, deterministic noise seed, calibration, duplicate channel и
`analogRead` по simulated time. Полная готовность sensor/control vertical slice
требует дополнительного scene-теста `VirtualAnalogSensor -> analogRead ->
threshold -> GPIO/PWM output` с firmware, собранной `rsma-build`.

## 6. Целевой контракт sensor/control platform

Для полноценной разработки прикладной системы HAL должен описывать не только
отдельные функции, но и сквозную границу `firmware -> VM -> board profile ->
Emulator backend -> scene peripheral`. Один и тот же `.rsfw` должен иметь
одинаковые semantics в standalone fixture и Unity scene.

### GPIO и board capabilities

Board profile обязан предоставлять для каждого pin mapping и capabilities:

- digital input/output;
- pull-up/pull-down;
- push-pull/open-drain и high-impedance release;
- ADC channel;
- PWM timer/channel;
- external IRQ edge;
- alternate-function/bus assignment.

Конфликт pin, duplicate physical mapping, unsupported mode и отсутствующий
board capability должны отклоняться до первого VM tick. Reset и safe-state
должны быть определены отдельно для latch, effective line, sensor input и
actuator output.

### Source API gaps

Следующий source-level increment должен синхронно добавить в `Arduino.h`,
builder, validator, bytecode и runtime:

```cpp
analogWrite(pin, duty);
attachInterrupt(pin, handler, edge);
detachInterrupt(pin);
millis();
micros();
serialBegin(port, baud);
serialAvailable(port);
serialRead(port);
serialWrite(port, value);
```

Текущие runtime/bytecode возможности PWM, IRQ и UART не считаются готовым
прикладным API, пока их нельзя использовать из пользовательского C++ source.

### Device buses

`spiTransfer(bus, value)` и `i2cTransfer(address, value)` остаются минимальным
byte-level compatibility layer. Для sensor drivers потребуются transaction-level
API и virtual devices:

- I2C register address, multi-byte read/write, ACK/NACK, device presence,
  repeated START, bus speed и timeout;
- SPI device/chip-select, transaction boundary, CPOL/CPHA, bit order, speed и
  full-duplex buffer;
- register-map device model, reset state, configurable response и fault
  injection;
- единый device/bus mapping в board profile и Unity scene.

### Errors, timing и lifecycle

Каждый peripheral contract должен определить units, simulated-time behavior,
reset/cancel semantics, queue/FIFO limits, timeout, retryability и safe-state.
Ошибки должны быть диагностируемыми для неизвестного pin/device, disconnected
sensor, NACK, bus timeout, FIFO overflow, DMA queue full, stalled transfer,
conflicting output и VM fault. Бесконечный polling без timeout/error result не
является допустимым прикладным контрактом.

### Приёмочный вертикальный срез

Минимальная приемка платформы — source firmware, собранная `rsma-build`, которая
читает `VirtualAnalogSensor`, применяет threshold/filter, управляет GPIO или
PWM actuator и отправляет UART telemetry. Следующие срезы должны покрыть
button/encoder IRQ и хотя бы один register-level I2C/SPI device. Для каждого
среза обязательны generated `.rsfw`, Unity scene, EditMode/PlayMode test,
expected event trace и negative/error cases.

## 7. Ошибки

HAL должен возвращать диагностируемый статус для:

- неизвестного pin/port;
- неподдержанного режима;
- обращения к отключенному тактированием периферийному блоку;
- переполнения UART buffer;
- конфликтующих драйверов линии;
- отсутствующего board capability.
