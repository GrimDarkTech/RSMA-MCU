# План разработки RSMA MCU

## Этап 0. Контракты и границы

- Зафиксировать `rsma-vm` как кастомную виртуальную платформу.
- Разделить `.rsfw` для VM и физические ELF/BIN/HEX для реальных MCU.
- Утвердить версии format/VM/HAL/board profile.
- Утвердить первый профиль `RSMA-STM32F103` и Arduino-like facade.
- Описать общий `IMcuIoBackend` для Emulator и HITL.

Критерий: архитектурные документы и тестовые векторы согласованы.

## Этап 1. Builder и формат

- Реализовать C++ CLI `rsma-build` как отдельный target.
- Зафиксировать исходную точку входа как единственную функцию `int main()` без автоматических `setup()`/`loop()`.
- Контракт исходного языка: [SOURCE-LANGUAGE.md](SOURCE-LANGUAGE.md).
- Подключить Clang/LLVM frontend через bundled submodule.
- Ограничить первую версию языка: integer/float/bool, функции, условия, циклы, статическая память.
- Определить бинарный `.rsfw`, section table, checksum и validator.
- Генерировать symbol/debug map.

Критерий: исходник Blink превращается в валидный `.rsfw`, а поврежденные пакеты отклоняются до запуска.

## Этап 2. C# VM в RSMA

- Реализовать verifier и loader.
- Реализовать interpreter, stack, calls, memory и simulated time.
- Добавить `WAIT`, `YIELD`, HAL calls и лимиты ресурсов.
- Интегрировать runtime с компонентом Unity без зависимости от coroutine-порядка.

Статус: `WAIT`, `YIELD`, HAL calls и базовые лимиты ресурсов реализованы в C# VM;
`yield()` доступен через Arduino-like facade. Нужна проверка Unity EditMode/PlayMode
в Editor для подтверждения host tick integration.

Критерий: Blink и Button исполняются воспроизводимо в EditMode/PlayMode тестах.

## Этап 3. GPIO и базовый board profile

- Реализовать typed GPIO state: mode, pull, output type, latch, line level.
- Добавить Arduino pin mapping и STM32 port/pin mapping.
- Реализовать reset state, conflict detection и diagnostics.
- Подключить LED, button и базовый motor driver RSMA.

Статус: typed GPIO, проверка конфликтов virtual/physical mapping, reset state и
адаптер RSMAGPIO реализованы. Профиль `RSMA-STM32F103` покрывает подтверждённые
firmware pin mappings PC10, PC11 и PC13 (PWM motor/LED); расширение на другие
пины требует подтверждённой схемы платы. Добавлен EditMode regression-тест,
сравнивающий одинаковый Blink через C++ Arduino facade и прямые HAL-вызовы;
требуется выполнение этого теста в Unity Test Runner.

Критерий: одинаковый сценарий проходит для Arduino-like API и low-level HAL API.

## Этап 4. Таймеры, IRQ, PWM и UART

- Добавить SysTick-like clock.
- Реализовать interrupt controller и edge-triggered GPIO IRQ.
- Реализовать timer/PWM с определенной точностью.
- Реализовать buffered UART и callbacks.

Статус: SysTick-like simulated time, edge-triggered GPIO IRQ, PWM, buffered UART
и transmission callbacks реализованы. Добавлен PlayMode trace-тест полного
сценария `Blink -> Button -> Interrupt -> PWM -> UART`; его runtime execution
нужно подтвердить в Unity Test Runner.

Критерий: `Blink -> Button -> Interrupt -> PWM -> UART` работает в VM с трассировкой событий.

## Этап 5. HITL

- Реализовать serial framing, CRC, timestamps и sequence numbers.
- Реализовать heartbeat, timeout, reconnect и safe state.
- Сделать bridge для физического STM32/Arduino adapter firmware.
- Добавить conformance tests с диапазонами задержки и частоты.

Статус: добавлен pure C# HITL transport contract с framing `RSMH`, version,
message type, flags, sequence, timestamp, payload length и CRC32. Реализованы
stream parser для fragmented frames и session monitor с heartbeat timeout,
reconnect и однократным safe-state callback. EditMode tests покрывают round-trip,
CRC corruption, fragmented parsing, duplicate sequence и timeout/reconnect;
нужно подключить реальный serial backend после фиксации wire contract.
Общий wire layout задокументирован в [HITL-PROTOCOL.md](HITL-PROTOCOL.md),
а portable C++ codec добавлен для adapter firmware и host-side conformance.

Критерий: один набор функциональных сценариев выполняется с EmulatorBackend и HitlBackend.

## Этап 6. Расширение SDK

- ADC.
- SPI и I2C.
- DMA в ограниченном виде.
- EEPROM-подобное persistent storage.
- Дополнительные Arduino и STM32 board profiles.
- Профилирование и threaded dispatch только после измерений.

Статус: добавлены ADC, SPI, I2C, synchronous и bounded asynchronous DMA,
EEPROM-подобное persistent storage. Async DMA имеет четыре 256-byte staging
buffers, очередь на 16 outstanding transfers, per-channel FIFO и детерминированное
completion по simulated time; C++ facade предоставляет submit/status/read/release,
а существующий `dmaTransfer` не изменён. `analogRead`, `spiTransfer`, `i2cTransfer`
и DMA calls понижаются builder-ом в stack-based HAL calls; EEPROM storage имеет
256 байт, сохраняется между VM reset и очищается явным backend clear. Добавлены
profile-aware validation и Unity mappings для `RSMA-STM32F103` и
`RSMA-AVR-ATmega328P`; AVR profile намеренно не поддерживает DMA.

### Реализовано: `VirtualAnalogSensor`

Добавлен Unity-компонент виртуального аналогового датчика, подключённый к ADC
channel через Emulator backend и simulated time. Pure C# model отделена от
Unity-компонента, поэтому waveform/calibration/noise тестируются без сцены.
Поддерживаются:

- источник сигнала `waveform`: constant, ramp, sine и пользовательская кривая;
- детерминированный noise с фиксируемым seed и настраиваемой амплитудой;
- sampling policy: частота/период выборки, момент применения sample и поведение
  между выборками;
- calibration: raw ADC range, offset, gain и clamping перед преобразованием в
  12-bit sample `0..4095`;
- физические `units` и масштабирование для отображения/телеметрии без изменения
  raw ADC-контракта прошивки;
- диагностику неподключённого channel, выхода за диапазон и несовместимого
  board/pin mapping.

Датчик не вызывает firmware или VM напрямую: он регистрирует sampler в ADC
backend, а прошивка получает значение только через `analogRead(channel)`.
Runtime обновляет sampler при продвижении simulated time, поэтому одинаковые
seed, waveform и sampling settings дают воспроизводимый результат независимо
от порядка Unity frames. Duplicate ADC channel и unmapped scene channel
диагностируются при регистрации.

Проверки: model waveform/sample-and-hold/deterministic-noise/calibration,
backend registration, `VirtualMachine -> analogRead` по simulated time и
source-built end-to-end scene fixture с `RSMAGPIO` adapter, GPIO/PWM output и
unmapped-channel negative case покрыты Unity EditMode/PlayMode tests.

## Этап 7. Полноценная разработка систем «датчики -> управление»

Текущий runtime уже позволяет проверять отдельные HAL-вызовы и простые
сценарии GPIO, но ещё не предоставляет сквозной контракт для разработки
законченного прикладного проекта в Unity. Главный риск — расхождение между
возможностями standalone `VirtualGpioBank`, C++ facade, RSFW validator и
scene-level `RSMAGpioHalAdapter`. Следующий этап должен закрыть этот разрыв.

### 7.1. Единый Emulator/peripheral backend — P0

- Ввести composable backend, через который одна VM получает GPIO, PWM, ADC,
  IRQ, UART, SPI, I2C, EEPROM и DMA.
- Убрать расхождение между `VirtualGpioBank` и
  `RSMAGpioHalAdapter`: одинаковый firmware package должен одинаково работать
  в EditMode fixture и в Unity scene.
- Зафиксировать lifecycle: create, configure, reset, run, stop, safe-state.
- Определить ownership и reset policy для очередей, FIFO, sensor state,
  persistent storage и output state.
- Добавить capability query до запуска firmware: board ID, доступные buses,
  channels, pins, devices и ограничения.

Критерий: один и тот же `.rsfw` запускается в in-process fixture и в Unity
scene без ручной замены HAL-объектов; отсутствующий peripheral диагностируется
до первого VM tick.

Статус: **завершён**. `VirtualMcuBackend` объединяет GPIO, PWM (включая
совместимый alias `TIM`), ADC, IRQ, UART, SPI, I2C, EEPROM и DMA capability
providers, выполняет board/required-peripheral preflight при `Configure` и
управляет lifecycle (`Created -> Configured -> Running`, `Reset`, `Stop`,
`SafeState`, `Faulted`). Одинаковый `pwm_motor_test.rsfw` проверяется через
backend в standalone PlayMode fixture и через `RSMAVirtualMicrocontroller` /
`RSMAGpioHalAdapter` в Unity scene; EditMode покрывает capability discovery,
отказ preflight до VM tick и reset/safe-state.

Ownership/reset contract: backend владеет lifecycle и объединением providers;
отдельные периферийные providers владеют своими очередями, FIFO, DMA и output
state. Reset возвращает периферийные регистры/выходы в reset state, отменяет
очередь DMA и планированные входные события; EEPROM остаётся persistent при
обычном reset и очищается только явной storage-clear операцией; bindings и
зарегистрированные scene sensors принадлежат Unity-компоненту и отключаются
при его уничтожении. Отсутствующие firmware capabilities диагностируются до
первого VM tick. Подробная проверка board pin capabilities и электрических
состояний остаётся в границах этапа 7.2.

### 7.2. Board profile, pin capabilities и электрическая модель — P0

- Расширить STM32/AVR pin maps с текущих демонстрационных наборов до полного
  согласованного набора pins выбранной виртуальной платы.
- Для каждого pin описать capabilities: digital input/output, pull-up/down,
  open-drain, analog channel, PWM timer/channel, external IRQ и alternate
  function.
- Связать один источник правды между native board profile, RSFW manifest,
  Unity `VirtualBoardProfile` и scene bindings.
- Реализовать validation конфликтов: duplicate physical pin, ADC/PWM/alternate
  function collision, unsupported pin mode, unavailable bus route.
- Довести electrical model до явных состояний output latch, effective line,
  high impedance, external drive и conflict; open-drain `HIGH` должен означать
  release линии, а не обычный push-pull HIGH.
- Добавить reset defaults, safe-state и диагностические сообщения для каждого
  mapped pin.

Критерий: board configuration отклоняет конфликтную схему до загрузки
firmware, а тест pin capabilities проходит одинаково в validator и Unity.

Статус: **завершён**. Добавлена typed pin schema в native `BoardProfile` и Unity
`VirtualBoardProfile`: digital input/output, pull-up/down, open-drain, analog,
PWM и external IRQ capabilities, reset defaults и alternate-function routes.
Оба профиля проверяют duplicate virtual/physical mappings, duplicate alternate
routes и несовместимые reset mode/pull/output settings; Unity adapter отклоняет
unsupported GPIO/PWM/ADC operations до обращения к scene GPIO. STM32/AVR
демонстрационные mappings теперь описаны в обеих моделях одинаковыми route
идентификаторами и проверяется доступность SPI/I2C routes. Backend выполняет
preflight конкретных GPIO/PWM/ADC/IRQ routes до первого VM tick, а scene
bindings обязаны совпадать с board profile. Runtime electrical model содержит
output latch, effective line, high-Z, pull bias, external drive, open-drain
release и push-pull conflict diagnostics; reset/safe-state очищают external
drive и запланированные входы и возвращают mapped pins в reset defaults.

### 7.3. Source-level API и язык прикладной прошивки — P0

Расширить `Arduino.h`, builder и validator согласованным API:

- `analogWrite`/PWM с duty, frequency и допустимыми PWM pins;
- `attachInterrupt`/`detachInterrupt` с rising, falling и change edge;
- `millis`/`micros` и правила overflow/precision;
- UART/Serial: begin, available, read, write и диагностируемые FIFO errors;
- `configurePin` либо полный compile-time pin mode API;
- по возможности `setup()`/`loop()` или документированный scheduler-friendly
  replacement для текущего единственного `main()`.

Для каждого API должны существовать: declaration, compile-time validation,
HAL service/bytecode lowering, runtime implementation, board capability check,
source example и negative test. Сейчас PWM, IRQ и UART частично существуют в
runtime/bytecode, но не образуют полного пользовательского C++-контракта.

Реализовано: `Arduino.h`, libclang builder, native validator, RSFW v1 HAL и
Unity runtime согласованы для source-level PWM (duty/frequency/capability), IRQ
(rising/falling/change), `millis`/`micros`, `Serial` polling, `configurePin` и
диагностируемых UART FIFO errors. `main()` документирован как scheduler-friendly
замена `setup()`/`loop()` через `delay()`/`yield()`. Positive/negative source
fixtures проверяют lowering и compile-time/board-capability diagnostics.

Критерий закрыт для поддержанного source subset: пользователь может собрать
sensor/control firmware без ручного bytecode, reflection или C#-only calls.

### 7.4. ADC и модели аналоговых датчиков — P0/P1

- Реализовать `VirtualAnalogSensor` как Unity component, подключённый к ADC
  channel через backend.
- Поддержать waveform `constant`, `ramp`, `sine` и пользовательскую кривую.
- Добавить deterministic noise с seed, амплитудой и воспроизводимой генерацией.
- Задать sampling period/frequency, sample-and-hold и поведение между samples.
- Добавить calibration: physical range, raw range, offset, gain и clamping.
- Хранить physical units для Inspector/telemetry, не меняя raw firmware
  contract `analogRead() -> 0..4095`.
- Добавить модели типовых датчиков: temperature, light, distance/position и
  test signal; отделить sensor model от ADC backend.
- Диагностировать disconnected sensor, duplicate channel, invalid range и
  unsupported ADC pin.

Критерий: `VirtualAnalogSensor -> analogRead -> threshold/filter -> output`
выполняется по simulated time, а waveform, noise, sampling и calibration
проверяются trace-тестами.

Реализовано: `VirtualAnalogSensor` подключается к ADC backend через
`IVirtualAnalogInputSource`; Unity-independent model поддерживает constant,
ramp, sine и AnimationCurve, deterministic noise по seed, sample-and-hold,
physical units, raw range calibration, offset/gain и clamping в `0..4095`.
Добавлены presets temperature, light, distance, position и test signal.
Backend preflight диагностирует неподключённый ADC channel, duplicate sensor,
unsupported ADC capability и invalid ranges до первого VM tick. `AnalogSample`
trace и source-built PlayMode fixture проверяют путь `sensor -> analogRead ->
threshold -> PWM` по simulated time. Raw firmware contract остаётся
`analogRead() -> 0..4095`.

### 7.5. PWM, timers и исполнительные механизмы — P0/P1

- Открыть PWM через C++ source API и связать его с board pin capabilities.
- Зафиксировать timer/PWM frequency, duty resolution, phase и update timing.
- Разделить digital level, PWM duty и effective actuator value.
- Добавить scene components для LED, relay, servo и DC motor/driver с enable,
  direction, brake/fault и ограничениями диапазона.
- Добавить safe-state при reset, stop, VM fault, disable и backend disconnect.
- Проверить saturation, slew/ramp и физический output trace.

Критерий: firmware может управлять хотя бы LED, relay и PWM motor/servo без
вызова C# facade; actuator получает детерминированный trace по simulated time.

Реализовано: virtual PWM state хранит duty, digital level, frequency,
phase и simulated-time update timestamp; legacy `analogWrite(pin, duty)`
использует deterministic default `490 Hz`, overload с frequency сохраняет
явную частоту. Добавлен Unity-independent `VirtualPwmActuatorModel` с
saturation, direction, enable/brake/fault, slew/ramp и немедленным safe-state.
Unity scene adapters доступны для LED, relay, servo и DC motor; они получают
команду только из firmware PWM/GPIO state и не требуют C# facade calls.
Reset, stop, VM fault, component disable и backend safe-state обнуляют output.
EditMode/native fixtures проверяют frequency/phase, dynamic update, slew,
safe-state и source path `analogWrite -> actuator output`.

### 7.6. IRQ, events и scheduling — P0/P1

- Довести GPIO edge events до source-level interrupt registration.
- Зафиксировать priority, masking, debounce policy, re-entrancy и порядок IRQ.
- Разделить polling, event-driven и periodic tasks.
- Добавить `millis`/`micros`, timer callbacks либо документированный механизм
  periodic wake-up без busy loop.
- Зафиксировать clock policy при paused Unity, изменении `Time.timeScale`,
  большом host tick и пропущенных кадрах.
- Ограничить event queue и диагностировать overflow/lost event.

Критерий: button/encoder/fault input может безопасно запускать обработчик,
а periodic control loop не зависит от случайного числа Unity frames.

Реализовано: source-level `attachInterrupt`/`detachInterrupt` используют
deterministic simulated-time edge queue. Scheduling policy фиксирует clock mode
(`ScaledDeltaTime`, `UnscaledDeltaTime`, `Paused`), maximum host tick,
interrupt priority, mask policy, non-reentrant dispatch, queue capacity и
debounce interval. IRQ queue сортируется по `(timestamp, priority desc,
sequence)`; nested ISR не запускается до `RETURN_IRQ`.

Overflow, masked drop и debounce диагностируются без исключения из input
callback: увеличивается lost-event counter, а в diagnostics/trace записывается
`IRQ_QUEUE_OVERFLOW`, `IRQ_MASKED` или `IRQ_DEBOUNCED`. Existing `delay()`/`WAIT`
и `yield()` являются source-level periodic wake-up без busy loop;
`VirtualPeriodicScheduler` предоставляет deterministic simulated-time callbacks
для backend/control tests. Pause, `Time.timeScale`, large host tick и dropped
frames обрабатываются explicit clock policy и bounded callback budget.

### 7.7. UART и внешняя телеметрия — P1

- Подключить существующий `VirtualUartBank` к scene backend и source API.
- Зафиксировать baud, frame timing, RX/TX FIFO, overflow, empty read и reset.
- Добавить serial telemetry example и observable Unity output endpoint.
- Проверить backpressure и loss policy для входящих/исходящих данных.

Критерий: firmware отправляет telemetry и принимает commands из Unity scene,
а FIFO/overflow/reset behavior покрыты EditMode и PlayMode tests.

### 7.8. Register-level SPI/I2C и virtual devices — P1

Текущие `spiTransfer` и `i2cTransfer` являются byte-level response stubs и
недостаточны для драйверов датчиков. Нужно добавить:

- I2C START/STOP, repeated START, read/write direction, ACK/NACK, address
  presence, clock speed и timeout;
- register address, multi-byte transaction, device state и несколько devices
  на одной шине;
- SPI chip-select/device mapping, transaction boundary, CPOL/CPHA, bit order,
  clock speed и full-duplex buffers;
- `VirtualI2cDevice`/`VirtualSpiDevice` с register map, configurable response,
  fault injection и reset state;
- единый bus backend для standalone и Unity scene.

Критерий: пример драйвера виртуального temperature/IMU/encoder sensor читает
registers через C++ API, а missing device, NACK, timeout и malformed transfer
дают диагностируемый результат.

### 7.9. EEPROM, DMA и периферийные error contracts — P1

- Уточнить persistent storage ownership, save/load/reset и scene serialization
  policy для EEPROM.
- Для DMA завершить contract error, timeout, cancellation, queue full,
  unreleased completion и backend reset; не оставлять firmware в бесконечном
  polling без диагностируемого результата.
- Единообразно определить для всех buses: return status или VM fault,
  retryability, timeout units, cancellation и reset effect.
- Добавить fault injection для disconnected device, invalid data, overflow,
  stalled transfer и conflicting output.

Критерий: каждая ошибка имеет документированный status/message, deterministic
test и безопасное поведение actuator outputs.

### 7.10. End-to-end examples, observability и CI — P0/P1

Добавить реальные прикладные вертикальные срезы:

1. `VirtualAnalogSensor -> ADC -> threshold -> digital relay/LED` — базовый
   source-built ADC/PWM slice уже реализован; bounded trace для VM time, ADC
   sample, PWM output и faults уже доступен; следующий шаг — telemetry transport
   и trace export в CI.
2. `Button/encoder -> IRQ -> debounced state machine -> PWM actuator`.
3. `I2C temperature sensor -> calibration/filter -> fan PWM -> UART telemetry`.
4. `SPI encoder -> position control -> motor driver -> fault input`.

Для каждого среза нужны source `.cpp`, generated `.rsfw`, Unity scene fixture,
EditMode/PlayMode tests, expected event/output trace и negative cases. В CI
должны выполняться builder/validator, native CTest, Unity EditMode и выбранные
PlayMode scenarios в чистом воспроизводимом окружении. Добавить trace/logging
для VM time, GPIO transitions, ADC samples, bus transactions, IRQ dispatch,
UART frames, actuator outputs и faults.

## Критерий готовности sensor/control platform

Платформа считается пригодной для полноценной разработки небольших систем,
когда выполнены все P0-пункты и минимум один end-to-end P1-срез. Пользователь
должен иметь возможность:

1. описать board/pin/device configuration без изменения C# runtime;
2. написать firmware только на поддержанном C++ API;
3. собрать и проверить `.rsfw` profile-aware validator-ом;
4. запустить тот же package в Unity scene и deterministic test fixture;
5. подключить analog/digital/bus sensors и actuators;
6. получить telemetry и диагностируемые ошибки;
7. воспроизвести сценарий по simulated time и output trace.

До выполнения этого критерия текущий MCU следует считать функциональным
прототипом VM/HAL, а не готовой платформой для полноценной sensor/control
разработки.

Source-level `millis()` завершён: declaration, Clang lowering в `HAL_CALL
Millis`, deterministic VM implementation, generated `.rsfw`, Unity PlayMode
acceptance и negative argument test покрыты. `micros()` и unsigned overflow
semantics остаются отдельным следующим расширением.

Source-level `attachInterrupt()`/`detachInterrupt()` также завершён для
`void handler()` ISR: compile-time pin/mode validation, function-entry patching,
`ATTACH_IRQ`/`ENABLE_IRQ`/`DETACH_IRQ` lowering, deterministic `RETURN_IRQ`,
generated firmware, negative builder cases и Unity edge/detach acceptance
покрыты. Общие function pointers и `micros()` остаются вне текущего scope.

Source-level UART v1 завершён для builtin facades `Serial`/`Serial1`/`Serial2`/
`Serial3` ports `0..3`: `begin`, `available`, `read`, `write`, compile-time
baud validation, generated echo and multi-port firmware, FIFO-driven Unity
acceptance, UART trace и invalid preset/custom-object tests покрыты.

UART RX scheduling v1 также завершён: `VirtualUartBank` принимает timestamped
packets с детерминированным FIFO/insertion ordering, VM учитывает UART input
events при ожидании, а `UartReceive` добавлен в trace contract.

Source-level UART RX callbacks завершены для всех builtin facades:
`attachReceive(void handler())`/`detachReceive()` проходят strict named-handler
validation, function-entry patching и `ATTACH_UART_RX`/`DETACH_UART_RX` lowering;
VM dispatch выполняется после FIFO delivery через отдельный non-reentrant
callback state и `RETURN_UART_RX`. Generated callback fixture, trace ordering
`UartReceive -> UartCallback -> UartTransmit` и PlayMode acceptance покрыты.
Packet framing/HITL integration теперь продолжены source-level packet API:
`attachPacketReceive(void handler())`/`detachPacketReceive()` и bounded payload
accessors `packetAvailable()`/`packetRead()`/`packetType()` добавлены для
builtin facades. Только CRC-valid complete HITL frames вызывают packet handler;
отдельные packet opcodes, queue и trace ordering покрыты builder fixture и
PlayMode acceptance.

HITL v1 streaming framing increment также добавлен на transport boundary:
`HitlFrameParser` и `HitlFrameStreamDecoder` принимают fragmented/concatenated
byte chunks, ограничивают внутренний buffer максимальным v1 frame size,
отклоняют CRC/version/type/length failures без остановки потока и
Source-level packet callbacks теперь добавлены для builtin UART facades:
`attachPacketReceive(void handler())`/`detachPacketReceive()` используют
`ATTACH_UART_PACKET_RX`/`DETACH_UART_PACKET_RX`, а payload доступен через
`packetAvailable()`/`packetRead()` и `packetType()`. Только CRC-valid complete
builder fixture и PlayMode acceptance. Дополнительно покрыты fragmented frame,
concatenated FIFO packets, zero-length payload, atomic queue overflow,
malformed callback declarations, reset/re-dispatch и reader HAL operand checks.
Native `HitlTransportIngress` теперь предоставляет явную bounded host/backend
границу: raw transport chunks преобразуются в FIFO valid frames, CRC recovery
остаётся внутри decoder, а overflow/rejection metrics доступны consumer-у.
builder fixture и PlayMode acceptance. Более богатые payload ownership/array
semantics остаются отдельным будущим расширением.

Benchmark baseline для virtual VM зафиксирован локальным Unity EditMode
прогоном `VirtualMcuPerformanceTests` на Unity `6000.3.16f1`, Windows Editor,
Mono, Ryzen 5 3600. Workload использует 8192 операций и 12 измерений после
5 warmup measurements; baseline следует читать как reference для сравнений,
а не как жёсткий CI threshold. В одном чистом прогоне `Advance` дал median
1149.0 us без profiler и 1346.3 us с profiler для compute workload, а для
8192 HAL dispatch — 644.55 us и 520.50 us соответственно. Counters проверены
assertions: compute — 16385 instructions и 0 HAL calls, HAL dispatch — 8193
instructions и 8192 HAL calls; peak value/call/interrupt stack depths также
сохраняются в performance output. Повторный прогон должен выполняться на той
же машине и с теми же workload parameters, поскольку Unity Editor timing
имеет заметный host noise.

Async DMA contract: queue capacity включает все unreleased tickets, включая
`DMA_COMPLETE`; слот освобождается только `dmaRelease`. `ResetDma` является
явной cancellation/reset операцией и инвалидирует active, queued и complete
tickets, одновременно очищая staging buffers. Отдельный transfer timeout и
ошибка устройства пока не моделируются: completion определяется только
simulated time, а invalid arguments/backend absence приводят к VM fault.

## Что не входит в первый релиз

- Полная эмуляция ARM/AVR instruction set.
- Полная бинарная совместимость с vendor STM32 HAL.
- Полная STL, exceptions, threads и unrestricted dynamic allocation.
- Передача каждой VM-инструкции через ZeroMQ или Serial.
- Собственный C++ compiler frontend вместо Clang.

## CI/CD

- Builder и format validator запускаются без Unity.
- C# VM получает unit/integration tests без физической платы.
- Unity PlayMode tests проверяют сцены и периферию.
- HITL job запускается отдельно на аппаратном стенде.
- Все release packages проверяются checksum, target profile и VM version.
