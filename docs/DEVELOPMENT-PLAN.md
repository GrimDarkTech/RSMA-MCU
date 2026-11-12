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

Критерий: `Blink -> Button -> Interrupt -> PWM -> UART` работает в VM с трассировкой событий.

## Этап 5. HITL

- Реализовать serial framing, CRC, timestamps и sequence numbers.
- Реализовать heartbeat, timeout, reconnect и safe state.
- Сделать bridge для физического STM32/Arduino adapter firmware.
- Добавить conformance tests с диапазонами задержки и частоты.

Критерий: один набор функциональных сценариев выполняется с EmulatorBackend и HitlBackend.

## Этап 6. Расширение SDK

- ADC.
- SPI и I2C.
- DMA в ограниченном виде.
- EEPROM-подобное persistent storage.
- Дополнительные Arduino и STM32 board profiles.
- Профилирование и threaded dispatch только после измерений.

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
