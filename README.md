# Audio Speed Changer

[🇷🇺 Русский](#-русский) | [🇬🇧 English](#-english)

---

## 🇷🇺 Русский

**Audio Speed Changer** — настольное приложение на Qt (Widgets) для пакетного изменения скорости аудиофайлов **без изменения высоты тона (без эффекта питча)**. Декодирование и кодирование выполняет FFmpeg, растяжение по времени — SoundTouch или Rubber Band.

### Возможности

- Пакетная обработка папок, в том числе с вложенными каталогами.
- Фильтр расширений входных файлов (по умолчанию `*.wav;*.mp3;*.flac;*.ogg;*.m4a;*.aac;*.aiff`).
- Изменение скорости с сохранением питча (коэффициент `2.0` — вдвое быстрее, `0.5` — вдвое медленнее).
- Выбор движка растяжения: **SoundTouch** или **Rubber Band**.
- Выходные форматы определяются по расширению: WAV, FLAC, MP3, OGG, M4A/AAC, AIFF, OPUS.
- Настраиваемые параметры: частота дискретизации, разрядность, битрейт, политика при конфликте имён, копирование метаданных, нормализация громкости.
- Многопоточная обработка (пул воркеров), отмена и пауза очереди.
- Панель прогресса, журнал событий, светлая и тёмная темы.
- Настройки сохраняются между запусками.

### Технологии

| Компонент | Назначение |
|---|---|
| Qt 5/6 (Widgets), qmake, C++17 | интерфейс и сборка |
| FFmpeg (shared: avformat, avcodec, avutil, swresample) | декодирование и кодирование |
| SoundTouch (static) | растяжение по времени |
| Rubber Band (static) | растяжение по времени |

### Структура проекта

```text
src/
├── app/         пути приложения и настройки (QSettings)
├── audio/       обёртки над FFmpeg, SoundTouch, Rubber Band
├── core/        общие типы: ProcessingParameters и др.
├── threading/   воркеры, пул потоков, токен отмены
├── ui/          главное окно, диалоги, виджеты, модели списка файлов
├── utils/       логгер, работа с файлами, форматирование времени
└── main.cpp
third_party/     зависимости и сгенерированный deps.pri
setup_audio_deps.ps1
audio_speed_changer.pro
```

### Сборка (Windows)

1. Установите Qt и компилятор (проект использовался с llvm-mingw).
2. Откройте `audio_speed_changer.pro` в Qt Creator.
3. Запустите qmake. Если файла `third_party/deps.pri` нет, `.pro` автоматически запустит `setup_audio_deps.ps1`, который скачает и подготовит FFmpeg, SoundTouch и Rubber Band и создаст `deps.pri`.
4. Соберите проект.

После линковки копирует DLL FFmpeg и среды выполнения компилятора рядом с `.exe` и в корень каталога сборки.

> Для кодирования в MP3 и OPUS сборка FFmpeg должна включать соответствующие энкодеры (например, libmp3lame и libopus).

> Заголовки в `third_party/ffmpeg/include` и DLL в `third_party/ffmpeg/bin` должны быть из одной и той же сборки FFmpeg.

### Использование

1. Укажите исходную и целевую папки.
2. Задайте коэффициент скорости, формат и остальные параметры.
3. Выберите файлы в списке и запустите обработку.
4. Следите за прогрессом и сообщениями в журнале.

### Файлы настроек и журналы

- Настройки: `settings.ini` в каталоге `AppConfigLocation`.
- Журнал: `logs/app.log` в каталоге `AppDataLocation`.

### Известные нюансы

- Для planar-форматов кодеков (AAC, MP3, Vorbis) в `swr_convert` нужно передавать `frame->extended_data`, а не указатель на `frame->data[0]`. Иначе возможно падение внутри `swresample` уже после начала обработки.

### Лицензии

Проект использует сторонние библиотеки со своими лицензиями (FFmpeg — LGPL/GPL в зависимости от сборки, SoundTouch — LGPL, Rubber Band — GPL или коммерческая). Перед распространением проверьте условия каждой из них. Лицензия самого проекта: коммерческое использование без разрешения автора запрещено.

---

## 🇬🇧 English

**Audio Speed Changer** is a Qt (Widgets) desktop application for batch-changing the speed of audio files **without altering pitch**. FFmpeg handles decoding and encoding; time stretching is done by SoundTouch or Rubber Band.

### Features

- Batch processing of folders, including subdirectories.
- Input extension filter (default: `*.wav;*.mp3;*.flac;*.ogg;*.m4a;*.aac;*.aiff`).
- Pitch-preserving speed change (factor `2.0` = twice as fast, `0.5` = half speed).
- Selectable stretching engine: **SoundTouch** or **Rubber Band**.
- Output format is chosen by file extension: WAV, FLAC, MP3, OGG, M4A/AAC, AIFF, OPUS.
- Configurable parameters: sample rate, bit depth, bitrate, filename conflict policy, metadata copying, volume normalization.
- Multithreaded processing (worker pool) with cancellation and queue pause.
- Progress panel, event log, light and dark themes.
- Settings persist between sessions.

### Tech stack

| Component | Purpose |
|---|---|
| Qt 5/6 (Widgets), qmake, C++17 | UI and build system |
| FFmpeg (shared: avformat, avcodec, avutil, swresample) | decoding and encoding |
| SoundTouch (static) | time stretching |
| Rubber Band (static) | time stretching |

### Project layout

```text
src/
├── app/         application paths and settings (QSettings)
├── audio/       wrappers around FFmpeg, SoundTouch, Rubber Band
├── core/        shared types: ProcessingParameters, etc.
├── threading/   workers, thread pool, cancellation token
├── ui/          main window, dialogs, widgets, file list models
├── utils/       logger, file helpers, duration formatting
└── main.cpp
third_party/     dependencies and the generated deps.pri
setup_audio_deps.ps1
audio_speed_changer.pro
```

### Building (Windows)

1. Install Qt and a compiler (the project has been used with llvm-mingw).
2. Open `audio_speed_changer.pro` in Qt Creator.
3. Run qmake. If `third_party/deps.pri` is missing, the `.pro` file automatically runs `setup_audio_deps.ps1`, which fetches and prepares FFmpeg, SoundTouch and Rubber Band and generates `deps.pri`.
4. Build the project.

After linking copies the FFmpeg and compiler runtime DLLs next to the `.exe` and into the build root.

> Encoding to MP3 and OPUS requires an FFmpeg build that includes the corresponding encoders (e.g. libmp3lame and libopus).

> The headers in `third_party/ffmpeg/include` and the DLLs in `third_party/ffmpeg/bin` must come from the same FFmpeg build.

### Usage

1. Choose the source and target folders.
2. Set the speed factor, output format and other parameters.
3. Select files in the list and start processing.
4. Watch progress and log messages.

### Settings and logs

- Settings: `settings.ini` in `AppConfigLocation`.
- Log: `logs/app.log` in `AppDataLocation`.

### Known pitfalls

- For planar codec sample formats (AAC, MP3, Vorbis), `swr_convert` must receive `frame->extended_data`, not a pointer to `frame->data[0]`. Otherwise the app may crash inside `swresample` shortly after processing starts.

### Licenses

This project uses third-party libraries under their own licenses (FFmpeg — LGPL/GPL depending on the build, SoundTouch — LGPL, Rubber Band — GPL or commercial). Review each license before distributing. License of this project: commercial use without permission of the author is prohibited.
