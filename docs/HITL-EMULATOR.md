# Emulator и HITL

## 1. Общая граница

Virtual MCU и физическая плата подключаются к RSMA через общий I/O boundary:

```text
IMcuIoBackend
    EmulatorBackend  -> C# VM / in-process runtime
    HitlSerialBackend -> Serial/USB bridge
    HitlNetworkBackend -> Ethernet bridge
```

RSMA LED, motor, encoder и sensor не должны зависеть от конкретной реализации backend.

## 2. События

Событие boundary содержит:

- session/device ID;
- monotonic timestamp;
- sequence number;
- peripheral ID;
- event type;
- typed payload;
- flags;
- CRC для внешнего транспорта.

Минимальные события: GPIO configuration, GPIO level change, PWM update, ADC sample, UART RX/TX, interrupt request, reset и heartbeat.

## 3. EmulatorBackend

Внутри Unity backend вызывает VM и обновляет модель RSMA в пределах одного согласованного шага времени. Быстрые сигналы, включая PWM, должны передаваться как параметры сигнала или накопленные transition events, а не только как редкие snapshots.

## 4. HitlBackend

HITL Bridge обеспечивает:

- framing;
- CRC;
- sequence numbers;
- heartbeat;
- timeout;
- reconnect;
- safe output state;
- диагностику latency и dropped frames.

Потеря связи должна переводить устройство в явное состояние `Degraded` или `Lost` и применять заранее заданное безопасное состояние выводов.

## 5. Сценарий соответствия

Один тест должен запускаться с двумя backend:

1. Blink на виртуальном PC13.
2. Нажатие виртуальной кнопки на PA0.
3. EXTI callback.
4. PWM на TIM2.
5. UART round-trip.
6. Reset и восстановление safe state.

Для VM проверяется детерминированный порядок событий. Для HITL задаются допуски задержки, частоты и потерь.
