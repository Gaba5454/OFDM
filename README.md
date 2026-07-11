# OFDM

Проект для передачи и приёма OFDM-кадров через PlutoSDR, для симуляции канала и для визуализации через ImGui/ImPlot.

Главная идея проекта:

- собрать OFDM-кадр из текста;
- промодулировать полезные данные;
- добавить PSS, обучающий OFDM-символ и циклический префикс;
- передать кадр через PlutoSDR или через симулятор канала;
- на приёме найти PSS, оценить CFO, выделить полезные OFDM-символы;
- оценить канал, эквализовать поднесущие и восстановить текст.

Сейчас realtime GUI разделён по ролям:

- backend-потоки работают с SDR, декодером и оценкой канала;
- backend заранее готовит массивы для графиков;
- ImGui/ImPlot только читает готовые массивы и рисует их.

Это сделано специально, чтобы не перегружать процессор постоянными пересчётами в GUI-потоке.

## Что умеет проект

- `simulation`:
  симуляция OFDM-цепочки без SDR, с визуализацией сигнала, PSS, корреляции и созвездия.
- `realtime TX`:
  отдельное окно управления передатчиком.
- `realtime RX`:
  отдельное окно мониторинга приёмника.
- `TX/RX`:
  короткие алиасы для тех же отдельных GUI-окон передатчика и приёмника.

## Зависимости

```bash
sudo apt install libsdl2-dev libgl1-mesa-dev libglew-dev pkg-config fftw3-dev
git submodule update --init --recursive
```

## Сборка

```bash
cmake -S . -B build
cmake --build build
```

## Режимы запуска

```bash
./build/OFDM simulation
./build/OFDM realtime TX <device>
./build/OFDM realtime RX <device>
./build/OFDM TX <device>
./build/OFDM RX <device>
```

Примеры:

```bash
./build/OFDM simulation
./build/OFDM realtime TX usb:1.10.5
./build/OFDM realtime RX usb:1.9.5
./build/OFDM TX usb:1.10.5
./build/OFDM RX usb:1.9.5
```

Текст передачи, модуляция и метод оценки канала задаются только через GUI. Через CLI передаётся только режим и URI PlutoSDR.

## Быстрый путь сигнала

Передача:

```text
text
-> payload_codec
-> modulation_map
-> frame_builder
-> ofdm_symbol
-> cycle_prefix
-> ofdm_radio / SoapySDR TX
```

Приём:

```text
SoapySDR RX
-> PSS correlation + CFO search
-> выделение training/payload символов
-> FFT
-> training_decoder / equalizer
-> payload_codec
-> recovered text
```

Realtime GUI:

```text
RX/TX worker threads
-> prepared backend plot arrays
-> shared state
-> ImGui / ImPlot frontend
```

## Структура проекта

Важно: в этом проекте исторически реализации лежат не только в `src/`, но и в `include/`.  
То есть `include/*.cpp` здесь не ошибка, а реальная часть исходников, которая подключена в `CMakeLists.txt`.

### Корневой уровень

- `CMakeLists.txt`  
  Главный файл сборки. Подключает SDL2, OpenGL, GLEW, FFTW3, SoapySDR, ImGui и ImPlot.

- `README.md`  
  Этот файл. Краткая карта проекта, режимы запуска и назначение модулей.

- `build/`  
  Каталог сборки CMake.

- `docs/`  
  Материалы для рассказа о проекте. Для защиты удобнее всего открыть
  `docs/project_defense_guide_ru.md`.

- `third_party/`  
  Внешние зависимости: `imgui`, `implot`, `SoapySDR`, `libiio`, `libad9361-iio`, `SoapyPlutoSDR`.

- `results/`  
  Отчёты и графики по оценке канала.

### `src/`

- `src/main.cpp`  
  Минимальная точка входа. Разбирает CLI и решает, какой режим запускать:
  `simulation`, `TX/RX`, `realtime TX/RX`.

- `src/gui.cpp`  
  Весь GUI-слой:
  обычная simulation-визуализация, отдельное TX-окно и отдельное RX-окно для realtime-режима.

  Важный момент:
  именно здесь сейчас реализовано разделение на backend и frontend.
  Потоки RX/TX подготавливают `plot`-массивы заранее, а ImGui только рисует.

### `include/`

#### Базовые константы и сервисные типы

- `include/const.h`  
  Общие константы проекта:
  длина FFT, длина CP, тип комплексного семпла, частота дискретизации, известный пилот и другие базовые параметры.

- `include/fftw_guard.h`  
  Общий mutex для безопасного создания FFTW-планов.

- `include/cli_utils.h`, `include/cli_utils.cpp`  
  Печать usage.

- `include/gui.h`  
  Публичные объявления GUI-функций и структуры `GuiPlotData` для simulation-режима.

#### Формирование OFDM-кадра

- `include/frame_layout.h`, `include/frame_layout.cpp`  
  Геометрия кадра в частотной области:
  где стоят пилоты, какие поднесущие являются информационными, сколько байт влезает в OFDM-символ, сколько OFDM-символов нужно для текста.

  Если нужно менять:
  пилоты, активные поднесущие, плотность полезных данных, тренировочный шаблон, смотреть надо сюда в первую очередь.

- `include/payload_codec.h`, `include/payload_codec.cpp`  
  Упаковка и распаковка полезной нагрузки:
  байты, длина текста, CRC, перевод байтов в биты и обратно.

- `include/modulation_map.h`, `include/modulation_map.cpp`  
  Описание поддерживаемых модуляций и их созвездий.
  Здесь лежат:
  карта символов, Gray-кодирование, демодуляция, hard-decision.

  Если нужно добавить новую модуляцию, основной модуль для правок именно этот.

- `include/ofdm_symbol.h`, `include/ofdm_symbol.cpp`  
  Построение одного OFDM-символа:
  раскладка нулей, пилотов, полезных поднесущих и переход в временную область через IFFT.

- `include/cycle_prefix.h`, `include/cycle_prefix.cpp`  
  Добавление циклического префикса.

- `include/pss_generator.h`, `include/pss_generator.cpp`  
  Генерация PSS. Используется для синхронизации кадра на приёме.

- `include/frame_builder.h`, `include/frame_builder.cpp`  
  Сборка полного TX-кадра.
  Формирует:
  PSS с CP, training-символ, полезные OFDM-символы, итоговый массив семплов для передачи.

  Это главный вход в тракт передачи на логическом уровне.

#### Приём, синхронизация и декодирование

- `include/receive.h`, `include/receive.cpp`  
  Низкоуровневый RX/TX-тракт через SoapySDR:
  настройка потока, чтение семплов, запись семплов, формат данных.

- `include/cfo_functions.h`, `include/cfo_functions.cpp`  
  Оценка и компенсация частотного смещения.

- `include/corellations.h`, `include/corellations.cpp`  
  Вспомогательная логика для анализа карт корреляции и поиска максимумов.

- `include/equalizer.h`, `include/equalizer.cpp`  
  Базовая эквализация по оценке канала.
  Здесь находится:
  `Pilot-LS + linear interpolation`.

- `include/training_decoder.h`, `include/training_decoder.cpp`  
  Центральный модуль оценки канала и декодирования payload в частотной области.

  Здесь реализованы методы:

  - `Training LS`
  - `Pilot LS Linear`
  - `Pilot LMMSE`
  - `DFT LS`
  - `Decision Directed`

  Также здесь лежат:

  - FFT без CP;
  - сглаживание оценки в временной области через `DFT-LS`;
  - эквализация символов;
  - unpack полезной нагрузки и проверка CRC.

- `include/ofdm_radio.h`, `include/ofdm_radio.cpp`  
  Верхний уровень радио-логики.
  Это один из самых важных файлов проекта.

  Он отвечает за:

  - открытие PlutoSDR;
  - поиск начала кадра по PSS;
  - грубую и тонкую компенсацию CFO;
  - вызов декодера;
  - live-мониторинг OFDM-структуры в realtime RX.

  Если нужно понять весь рабочий путь реального эфира, начинать чтение стоит отсюда.

#### Симуляция канала и offline-визуализация

- `include/channel_simulate.h`, `include/channel_simulate.cpp`  
  Модель канала и добавление шума.

- `include/translate.h`, `include/translate.cpp`  
  Построение offline-представления для simulation-режима:
  из переданного текста в набор графиков и результатов декодирования.

- `include/simulation.h`, `include/simulation.cpp`  
  Вход в simulation-режим.

### `results/`

- `results/channel_estimation_report.md`  
  Отчёт по сравнению методов оценки канала.

- `results/generate_channel_estimation_report.py`  
  Скрипт, который генерирует отчёт и графики.

- `results/plots/channel_estimation_nmse.png`  
  Сравнение NMSE оценок канала.

- `results/plots/channel_estimation_ber.png`  
  Сравнение BER после эквализации.

- `results/plots/channel_estimation_example.png`  
  Пример истинного и оценённых каналов для одной реализации.

## Ключевые структуры данных

- `TxFrameData`  
  Готовый кадр на передачу:
  биты, модулированные символы, training-символ, PSS, итоговый массив семплов.

- `GuiPlotData`  
  Подготовленный simulation-снимок для offline-визуализации.

- `SyncInfo`  
  Результат поиска начала кадра по PSS:
  индекс пика, старт PSS, старт training-символа, старт payload и оценка CFO.

- `DecodedResult`  
  Результат декодирования полезной нагрузки:
  восстановленный текст, CRC, constellation points, channel estimate, raw bytes.

- `CaptureInfo`  
  Полный результат обработки RX-захвата:
  корреляционная карта, sync, CFO, декодированный payload и флаг наличия кадра.

- `LiveMonitorInfo`  
  Упрощённая realtime-информация по текущему OFDM-срезу:
  CP-метрика, эквализованный символ, созвездие и оценка канала.

## Realtime архитектура

Realtime-режимы специально собраны так, чтобы GUI не занимался тяжёлой математикой.

### Что делает backend

- `tx_worker`  
  Держит PlutoSDR в режиме передачи и постоянно отправляет уже собранный кадр.

- `rx_stream_worker`  
  Постоянно читает эфир, поддерживает окно последних IQ-семплов, считает live-monitor и готовит массивы для временного графика, live-созвездия и live-оценки канала.

- `rx_decode_worker`  
  Берёт накопленное RX-окно, пытается найти полноценный кадр, выполняет PSS sync, CFO, FFT, channel estimation, эквализацию и формирует готовое представление декодированного кадра.

### Что делает frontend

- GUI-поток не читает SDR напрямую.
- GUI-поток не строит созвездие из сырых `vector<CF>` на каждом кадре.
- GUI-поток не пересчитывает канал и не запускает FFT.

Вместо этого backend готовит структуры вида:

- `LinePlotData`
- `ScatterPlotData`
- `ChannelPlotData`
- `TxViewData`
- `LiveViewData`
- `FrameViewData`

Дальше GUI только берёт готовые массивы и отрисовывает их через ImPlot.

## Оценка канала

В проекте реализовано несколько оценок канала, и все они доступны из realtime RX GUI:

- `Training LS`
- `Pilot LS Linear`
- `Pilot LMMSE`
- `DFT LS`
- `Decision Directed`

Почему сейчас по умолчанию используется `DFT LS`:

- он уже реализован в проекте;
- он устойчивее обычного сырого `Training LS`;
- он подавляет шум за счёт ограничения длины импульсной характеристики длиной CP;
- в `results/channel_estimation_report.md` он показывает лучший практический баланс по NMSE и BER среди уже встроенных методов.

Если захочешь дальше развивать именно приёмник, то логичный порядок такой:

1. оставить `DFT LS` как рабочую базу;
2. улучшать `Pilot LMMSE` под конкретную статистику канала;
3. затем аккуратно дорабатывать `Decision Directed`.

## Что править под конкретную задачу

- Хочешь добавить модуляцию:
  `include/modulation_map.cpp`

- Хочешь поменять пилоты, нули, активные поднесущие:
  `include/frame_layout.cpp`

- Хочешь изменить состав OFDM-кадра:
  `include/frame_builder.cpp`

- Хочешь менять PSS:
  `include/pss_generator.cpp`

- Хочешь менять декодирование и оценку канала:
  `include/training_decoder.cpp`

- Хочешь менять логику поиска кадра и работы с PlutoSDR:
  `include/ofdm_radio.cpp`

- Хочешь менять окна и realtime-визуализацию:
  `src/gui.cpp`

## Что уже оптимизировано

- realtime GUI переведён на backend-подготовку графиков;
- GUI больше не пересобирает основные realtime-графики из сырых IQ-векторов каждый кадр;
- snapshot GUI теперь хранит указатели на готовые frontend-кэши вместо тяжёлых копий массивов;
- в горячем пути FFTW убрано постоянное пересоздание планов для live RX/decode операций.

Итог:
нагрузка теперь снимается не только с ImGui, но и с частотной обработки.
