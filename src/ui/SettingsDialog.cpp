#include "ui/SettingsDialog.h"
#include "app/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QTabWidget>

SettingsDialog::SettingsDialog(AppSettings* settings, QWidget* parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("Настройки"));
    resize(520, 380);

    auto* tabs = new QTabWidget(this);

    // --- Общие ---
    {
        auto* page = new QWidget(this);
        auto* form = new QFormLayout(page);

        m_darkTheme = new QCheckBox(tr("Тёмная тема"), page);
        form->addRow(QString(), m_darkTheme);

        auto* threadsRow = new QWidget(page);
        auto* hl = new QHBoxLayout(threadsRow);
        hl->setContentsMargins(0, 0, 0, 0);
        m_threads = new QSpinBox(threadsRow);
        m_threads->setRange(1, 64);
        hl->addWidget(m_threads);
        hl->addWidget(new QLabel(tr("потоков"), threadsRow));
        hl->addStretch(1);
        form->addRow(tr("Одновременная обработка:"), threadsRow);

        tabs->addTab(page, tr("Общие"));
    }

    // --- Сканирование ---
    {
        auto* page = new QWidget(this);
        auto* form = new QFormLayout(page);

        m_includeSubdirs = new QCheckBox(tr("Включая подпапки"), page);
        form->addRow(QString(), m_includeSubdirs);

        m_extFilter = new QLineEdit(page);
        m_extFilter->setPlaceholderText("*.wav;*.mp3;*.flac;…");
        form->addRow(tr("Фильтр расширений:"), m_extFilter);

        tabs->addTab(page, tr("Сканирование"));
    }

    // --- Аудио ---
    {
        auto* page = new QWidget(this);
        auto* form = new QFormLayout(page);
        m_engineCombo = new QComboBox(page);
        m_engineCombo->addItem("SoundTouch",  int(AudioEngine::SoundTouch));
        m_engineCombo->addItem("Rubber Band", int(AudioEngine::RubberBand));
        form->addRow(tr("DSP-движок:"), m_engineCombo);
        tabs->addTab(page, tr("Аудио"));
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* root = new QVBoxLayout(this);
    root->addWidget(tabs, 1);
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadFromSettings();
}

void SettingsDialog::loadFromSettings()
{
    m_darkTheme->setChecked(m_settings->darkTheme());
    m_includeSubdirs->setChecked(m_settings->includeSubdirs());
    m_extFilter->setText(m_settings->extensionFilter());
    m_threads->setValue(m_settings->parameters().threadCount);

    const auto p = m_settings->parameters();
    const int idx = m_engineCombo->findData(int(p.engine));
    if (idx >= 0) m_engineCombo->setCurrentIndex(idx);
}

void SettingsDialog::saveToSettings()
{
    m_settings->setDarkTheme(m_darkTheme->isChecked());
    m_settings->setIncludeSubdirs(m_includeSubdirs->isChecked());
    m_settings->setExtensionFilter(m_extFilter->text());

    auto p = m_settings->parameters();
    p.threadCount = m_threads->value();
    p.engine = static_cast<AudioEngine>(m_engineCombo->currentData().toInt());
    m_settings->setParameters(p);
}

void SettingsDialog::onAccepted()
{
    saveToSettings();
    emit settingsChanged();
    accept();
}
