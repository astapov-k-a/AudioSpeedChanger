#include "ui/AboutDialog.h"
#include <QApplication>

void AboutDialog::show(QWidget* parent)
{
    QMessageBox::about(parent,
                       QObject::tr("О приложении"),
                       QObject::tr(
                           "<h3>Audio Speed Changer</h3>"
                           "<p>Версия %1</p>"
                           "<p>Пакетное изменение скорости аудиофайлов<br>"
                           "с сохранением высоты тона.</p>"
                           "<p>DSP: SoundTouch / Rubber Band.<br>"
                           "Декодирование/кодирование: FFmpeg.</p>"
                           "<p>Qt %2</p>")
                           .arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion())));
}
