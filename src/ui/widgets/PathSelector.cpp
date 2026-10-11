#include "ui/widgets/PathSelector.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QStyle>

PathSelector::PathSelector(Mode mode, QWidget* parent)
    : QWidget(parent), m_mode(mode)
{
    m_dialogTitle = (mode == SourceDirectory)
    ? tr("Выберите папку-источник (A)")
    : tr("Выберите папку-назначение (Б)");

    m_icon = new QLabel(this);
    m_icon->setFixedSize(20, 20);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(mode == SourceDirectory
                                   ? tr("Путь к исходной папке…")
                                   : tr("Путь к папке для сохранения результатов…"));
    m_edit->setClearButtonEnabled(true);

    auto makeBtn = [this](QStyle::StandardPixmap icon, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        b->setToolTip(tip);
        b->setAutoRaise(true);
        return b;
    };

    m_browseBtn  = makeBtn(QStyle::SP_DirOpenIcon, tr("Выбрать папку…"));
    m_openBtn    = makeBtn(QStyle::SP_DirLinkIcon, tr("Открыть в проводнике"));
    m_refreshBtn = makeBtn(QStyle::SP_BrowserReload, tr("Обновить"));

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->addWidget(m_icon);
    lay->addWidget(m_edit, 1);
    lay->addWidget(m_browseBtn);
    lay->addWidget(m_openBtn);
    lay->addWidget(m_refreshBtn);

    connect(m_browseBtn,  &QToolButton::clicked, this, &PathSelector::onBrowse);
    connect(m_openBtn,    &QToolButton::clicked, this, &PathSelector::onOpen);
    connect(m_refreshBtn, &QToolButton::clicked, this, &PathSelector::onRefresh);
    connect(m_edit, &QLineEdit::textEdited, this, &PathSelector::onTextEdited);

    validate();
}

QString PathSelector::path() const { return m_edit->text().trimmed(); }

void PathSelector::setPath(const QString& p)
{
    m_edit->setText(p);
    validate();
}

void PathSelector::onBrowse()
{
    const QString start = QFileInfo(path()).absolutePath().isEmpty()
    ? QDir::homePath() : QFileInfo(path()).absolutePath();

    const QString dir = QFileDialog::getExistingDirectory(this, m_dialogTitle, start,
                                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    m_edit->setText(QDir::toNativeSeparators(dir));
    validate();
    emit pathChanged(path());
    emit pathSelected(path());
}

void PathSelector::onOpen()
{
    const QString p = path();
    if (QFileInfo(p).exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(p));
}

void PathSelector::onRefresh()
{
    validate();
    emit pathChanged(path());
}

void PathSelector::onTextEdited(const QString&)
{
    validate();
    emit pathChanged(path());
}

void PathSelector::validate()
{
    const QString p = path();
    const bool exists = !p.isEmpty() && QFileInfo(p).isDir();

    QString cssStyle;                    // <-- больше не конфликтует с style()
    QString tip;
    if (p.isEmpty()) {
        // нейтрально
    } else if (!exists) {
        cssStyle = "color: #c0392b;";
        tip = tr("Папка не существует");
    }

    m_edit->setStyleSheet(cssStyle);
    m_edit->setToolTip(tip);

    // QWidget::style() — квалифицируем явно, чтобы никакая локальная
    // переменная не могла это затенение повторить.
    m_icon->setPixmap(QWidget::style()->standardIcon(
                                          exists ? QStyle::SP_DirIcon : QStyle::SP_MessageBoxWarning).pixmap(16, 16));
}
