QT       += core gui widgets

CONFIG   += c++17
CONFIG   -= app_bundle

TARGET    = audio-speed-changer
TEMPLATE  = app

DEFINES  += QT_DEPRECATED_WARNINGS

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    \
    src/app/AppPaths.cpp \
    src/app/AppSettings.cpp \
    \
    src/core/Core.cpp \
    \
    src/audio/AudioInterfaces.cpp \
    src/audio/AudioBackends.cpp \
    \
    src/threading/ProcessingWorker.cpp \
    src/threading/WorkerPool.cpp \
    \
    src/ui/MainWindow.cpp \
    src/ui/SettingsDialog.cpp \
    src/ui/AboutDialog.cpp \
    \
    src/ui/widgets/PathSelector.cpp \
    src/ui/widgets/ParameterPanel.cpp \
    src/ui/widgets/ProgressPanel.cpp \
    src/ui/widgets/LogWidget.cpp \
    \
    src/ui/models/FileListModel.cpp \
    src/ui/models/FileListProxyModel.cpp \
    \
    src/utils/Logger.cpp \
    src/utils/FileUtils.cpp \
    src/utils/DurationFormat.cpp

HEADERS += \
    src/app/AppPaths.h \
    src/app/AppSettings.h \
    \
    src/core/Core.h \
    \
    src/audio/AudioInterfaces.h \
    src/audio/AudioBackends.h \
    \
    src/threading/CancellationToken.h \
    src/threading/ProcessingWorker.h \
    src/threading/WorkerPool.h \
    \
    src/ui/MainWindow.h \
    src/ui/SettingsDialog.h \
    src/ui/AboutDialog.h \
    \
    src/ui/widgets/PathSelector.h \
    src/ui/widgets/ParameterPanel.h \
    src/ui/widgets/ProgressPanel.h \
    src/ui/widgets/LogWidget.h \
    \
    src/ui/models/FileListModel.h \
    src/ui/models/FileListProxyModel.h \
    \
    src/utils/Result.h \
    src/utils/Logger.h \
    src/utils/FileUtils.h \
    src/utils/DurationFormat.h

# =====================================================================
# Сторонние библиотеки подключаются ТОЛЬКО через deps.pri.
# deps.pri создаётся скриптом setup_audio_deps.ps1.
# =====================================================================
win32 {
    DEPS_SCRIPT = $$shell_path($$PWD/setup_audio_deps.ps1)
    DEPS_PRI    = $$PWD/third_party/deps.pri

    !exists($$DEPS_SCRIPT) {
        error("setup_audio_deps.ps1 not found: $$DEPS_SCRIPT")
    }

    !exists($$DEPS_PRI) {
        message("Running setup_audio_deps.ps1 ...")
        system("powershell -ExecutionPolicy Bypass -NoProfile -File $$quote($$DEPS_SCRIPT)")
    }

    exists($$DEPS_PRI) {
        include($$DEPS_PRI)
    } else {
        warning("deps.pri not found. Run qmake again after dependencies are built.")
    }
}

unix {
    # На Linux аналогичный .pri, если понадобится — оставьте на будущее.
    exists($$PWD/third_party/deps.pri): include($$PWD/third_party/deps.pri)
}

DISTFILES += \
    setup_audio_deps.ps1

# =====================================================================
# Post-link: копируем runtime-DLL рядом с .exe И в корень сборки.
# Пути FFMPEG_BIN_DIR и COMPILER_BIN_DIR приходят из deps.pri.
# =====================================================================
win32 {
    CONFIG(debug, debug|release): DLL_SUBDIR = debug
    else:                         DLL_SUBDIR = release

    DLL_EXE_DIR  = $$shell_path($$OUT_PWD/$$DLL_SUBDIR)
    DLL_ROOT_DIR = $$shell_path($$OUT_PWD)
    DLL_BAT      = $$OUT_PWD/copy_runtime_dlls.bat

    BAT_LINES  = "@echo off"
    BAT_LINES += "setlocal enableextensions"

    !isEmpty(FFMPEG_BIN_DIR) {
        FF_BIN = $$shell_path($$FFMPEG_BIN_DIR)
        BAT_LINES += "if not exist \"$$DLL_EXE_DIR\"  mkdir \"$$DLL_EXE_DIR\""
        BAT_LINES += "xcopy /Y /I /Q \"$$FF_BIN\\*.dll\" \"$$DLL_EXE_DIR\"  >NUL 2>&1"
        BAT_LINES += "xcopy /Y /I /Q \"$$FF_BIN\\*.dll\" \"$$DLL_ROOT_DIR\" >NUL 2>&1"
    }

    !isEmpty(COMPILER_BIN_DIR) {
        CC_BIN = $$shell_path($$COMPILER_BIN_DIR)

        # Список в переменную: так 'libc++.dll' не парсится как выражение с '++'.
        RUNTIME_DLLS  = libc++.dll
        RUNTIME_DLLS += libunwind.dll
        RUNTIME_DLLS += libstdc++-6.dll
        RUNTIME_DLLS += libgcc_s_seh-1.dll
        RUNTIME_DLLS += libwinpthread-1.dll

        for(dll, RUNTIME_DLLS) {
            BAT_LINES += "if exist \"$$CC_BIN\\$$dll\" copy /Y \"$$CC_BIN\\$$dll\" \"$$DLL_EXE_DIR\"  >NUL 2>&1"
            BAT_LINES += "if exist \"$$CC_BIN\\$$dll\" copy /Y \"$$CC_BIN\\$$dll\" \"$$DLL_ROOT_DIR\" >NUL 2>&1"
        }
    }

    BAT_LINES += "exit /b 0"

    write_file($$DLL_BAT, BAT_LINES)

    QMAKE_POST_LINK += $$shell_path($$DLL_BAT)
}
