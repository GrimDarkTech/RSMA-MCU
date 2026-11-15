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

Source-level facades `Serial`, `Serial1`, `Serial2` и `Serial3` используют
UART ports `0..3` соответственно. `begin(baudPreset)` принимает presets `0..4`
(`9600..115200`), `available()` возвращает размер RX FIFO, `read()` извлекает
следующий byte, а `write(value)` создаёт внешний `UartTransmit` trace event.
RX injection и FIFO overflow остаются
host/backend operations; polling не продвигает simulated time без `delay()` или
`yield()`.

`Serial.read()` на пустом FIFO и переполнение FIFO являются диагностируемыми HAL
ошибками, а не неявным значением `0`. Это позволяет source firmware отличить
отсутствующий byte от настоящего нулевого byte.

Для deterministic host input `VirtualUartBank.ScheduleReceived(port, timestamp,
data)` планирует RX packet на абсолютное simulated time. События упорядочиваются
по `(timestamp, insertion sequence)`, bytes одного packet сохраняют FIFO-порядок,
а VM учитывает ближайший UART timestamp как event boundary во время `WAIT`.
После доставки каждый byte генерирует `UartReceive` trace event; непосредственный
`InjectReceived` сохраняет прежнюю семантику немедленной host-инъекции.

Source-level UART callbacks используют `Serial.attachReceive(handler)` и
`Serial.detachReceive()` (аналогично для `Serial1`–`Serial3`). Frontend
понижает их в `ATTACH_UART_RX`/`DETACH_UART_RX`; operand layout — порт `u8` и
адрес bytecode handler `u32`. Handler завершается `RETURN_UART_RX`, сохраняет
прерванное состояние VM и не смешивается с GPIO `RETURN_IRQ`. Callback
диспетчеризуется после `UartReceive`, когда byte уже доступен в FIFO; trace
порядок для echo: `UartReceive -> UartCallback -> UartTransmit`.
Dispatch non-reentrant, а queued receives сохраняют FIFO/insertion ordering.

Packet API использует отдельный `ATTACH_UART_PACKET_RX`/`DETACH_UART_PACKET_RX`
binding и `RETURN_UART_PACKET_RX`. `UartPacketAvailable`, `UartPacketRead` и
`UartPacketType` — HAL services `25..27`; они обращаются к текущему valid HITL
packet на выбранном порту. CRC-invalid/incomplete frames отбрасываются
streaming decoder-ом и не создают packet callback. `UartPacketReceive` и
`UartPacketCallback` trace events позволяют отличить frame delivery от обычного
byte-level `UartReceive`. После `RETURN_UART_PACKET_RX` текущий packet context
закрывается и непрочитанный payload discard-ится.

Host/backend transport должен передавать сырые chunks в transport ingress
(`HitlTransportIngress::ingest` в native boundary или
`VirtualUartBank.InjectTransportBytes` в Unity). Chunk может быть fragment,
один frame или несколько concatenated frames; backend не обязан сам находить
границы `RSMH`. Ingress bounded: максимум 256 ожидающих валидных frames;
native adapter сохраняет FIFO и при overflow отбрасывает newest frame с
счётчиком `droppedFrameCount`, Unity adapter отклоняет batch атомарно.

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

Типовые preset-модели используют physical ranges `temperature: -40..125 degC`,
`light: 0..1000 lux`, `distance: 0..400 cm` и `position: 0..100 %`. Эти
диапазоны являются telemetry/Inspector metadata; ADC передаёт только
калиброванный 12-bit raw sample. При `samplePeriodMilliseconds > 0` модель
обновляет held sample на границе периода, а между границами возвращает тот же
sample независимо от количества `analogRead` и host frames. При нулевом
периоде sample вычисляется в текущей simulated time. Noise вычисляется
детерминированно из `(seed, sample index)` и не зависит от порядка чтений.

Model и backend contract покрыты Unity EditMode tests для waveform, sample-and-
hold, deterministic noise seed, calibration, duplicate channel и
`analogRead` по simulated time. Source-built scene fixture дополнительно
проверяет `VirtualAnalogSensor -> analogRead -> threshold -> dynamic
analogWrite -> RSMAGPIO PWM output`, а unmapped ADC channel завершается
диагностируемой ошибкой до запуска sensor.

Для observability VM предоставляет bounded `VirtualTraceBuffer` и события
`TimeAdvanced`, `AnalogSample`, `DigitalOutput`, `PwmOutput`, `HalCall`,
`StateTransition`, `Interrupt`, `UartCallback`, `UartTransmit`, `UartReceive`,
`RejectedMapping`, `SchedulingDiagnostic` и `Fault`.
Каждое событие содержит monotonic sequence,
simulated timestamp, channel/value и optional message. Trace доступен одинаково
из standalone `VirtualMachine` и Unity `RSMAVirtualMicrocontroller`; переполнение
буфера отбрасывает старые события и увеличивает `DroppedEventCount`.

Source-level `millis()` и `micros()` используют тот же simulated clock: вызов не
продвигает время и возвращает младшие 32 бита timestamp текущей VM точки исполнения
в миллисекундах или микросекундах. При wrap-around прошивка использует wrap-safe
разность unsigned time values.
После `delay()`/`WAIT` значение отражает достигнутое время, что позволяет
строить deterministic elapsed-time control logic без зависимости от Unity frame
timing.

Source-level IRQ API использует deterministic GPIO edge queue:
`attachInterrupt(pin, handler, CHANGE|RISING|FALLING)` регистрирует `void()` ISR,
а `detachInterrupt(pin)` удаляет только binding указанного pin. ISR возвращается
через `RETURN_IRQ`, восстанавливая interrupted VM state/wake time. Повторный edge
после detach не dispatch-ится; edge mask `CHANGE` соответствует rising|falling.

IRQ/scheduling policy задаётся virtual backend и не зависит от количества host
frames. Default contract: non-reentrant ISR, `QueueWhileMasked`, queue capacity
256, debounce `0 ms`, ordering по timestamp, затем priority descending и
monotonic input sequence. Queue overflow/masked drop/debounce увеличивают
lost-event counter и публикуют diagnostics/trace с кодами
`IRQ_QUEUE_OVERFLOW`, `IRQ_MASKED`, `IRQ_DEBOUNCED`.

Host clock policy явно определяет поведение при paused Unity и `Time.timeScale`:
`ScaledDeltaTime`, `UnscaledDeltaTime` или `Paused`. Delta выше configured
maximum clamp-ится с diagnostic, поэтому большой host tick не создаёт
неограниченный catch-up. Periodic callbacks используют simulated timestamp и
bounded callback budget.

UART contract: baud presets `0..4` соответствуют `9600`, `19200`, `38400`,
`57600`, `115200`; каждый frame занимает 10 bits. TX и RX FIFO имеют capacity
256 bytes. TX overflow и RX overflow используют drop-newest policy и публикуют
`UART_TX_OVERFLOW`/`UART_RX_OVERFLOW` с lost-byte counters. `read()` при пустом
FIFO возвращает диагностируемую ошибку, а `available()` не продвигает время.
Scheduled RX, TX frame completion и callbacks привязаны к одному simulated
clock; reset очищает очереди, parser state, counters и diagnostics. Unity
backend предоставляет observable transmitted-byte endpoint и UART diagnostics.

PWM source API поддерживает `analogWrite(pin, duty)` и overload
`analogWrite(pin, duty, frequencyHz)`. Duty имеет диапазон `0..255`, frequency
`1..1000000` Hz и передаётся в `PwmWriteFrequency` HAL service. Pin должен иметь
`Pwm` capability и `PWM` alternate route; native validator и Unity backend
проверяют это до первого VM tick.

Virtual PWM backend хранит отдельно duty (`0..255`), digital threshold level,
frequency в Hz, phase и timestamp последнего update. Для legacy двухаргументного
`analogWrite` frequency равна `490 Hz`. Phase вычисляется как fractional part
`timestamp_ms * frequency_hz / 1000`; effective PWM level сравнивает phase с
normalized duty и не зависит от числа Unity frames. `Reset`, safe-state, stop,
VM fault и backend disconnect выставляют duty/effective output в zero.

Исполнительный слой отделён от PWM HAL. `VirtualPwmActuatorModel` применяет
saturation и optional slew rate, а также поддерживает `Led`, `Relay`, `Servo`
и `DcMotor`; motor дополнительно имеет direction, enable, brake и fault.
Physical output trace вычисляется по simulated time: firmware остаётся
единственным источником команды, а Unity component только потребляет состояние
actuator. Servo physical range по умолчанию `0..180`, DC motor использует
signed range `-1..1`.

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

Текущие runtime/bytecode возможности IRQ и UART не считаются готовым
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
