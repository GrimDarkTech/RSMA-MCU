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

Значения частоты и задержки должны иметь фиксированные единицы и явно документированную точность. Сетевой backend не должен передавать каждую VM-инструкцию; границей обмена являются периферийные события.

## 5. Ошибки

HAL должен возвращать диагностируемый статус для:

- неизвестного pin/port;
- неподдержанного режима;
- обращения к отключенному тактированием периферийному блоку;
- переполнения UART buffer;
- конфликтующих драйверов линии;
- отсутствующего board capability.
