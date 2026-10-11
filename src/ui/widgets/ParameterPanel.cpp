#include "ui/widgets/ParameterPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

namespace {

// Соответствие индексов и enum OutputFormat.
//constexpr int kFormatSameAsSource = 0;

// Соответствие индексов и enum ConflictPolicy.
// constexpr int kConflictOverwrite = 0;
// constexpr int kConflictSkip      = 1;
// constexpr int kConflictRename    = 2;
// constexpr int kConflictAsk       = 3;

} // namespace

ParameterPanel::ParameterPanel(QWidget* parent) : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    root->addWidget(createSpeedGroup());
    root->addWidget(createFormatGroup());
    root->addWidget(createAdvancedGroup());

    connect(m_speedSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ParameterPanel::onAnyChanged);
    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_suppressSignals) return;
        m_suppressSignals = true;
        m_speedSpin->setValue(v / 100.0);
        m_suppressSignals = false;
        onAnyChanged();
    });
    connect(m_speedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
                if (m_suppressSignals) return;
                m_suppressSignals = true;
                m_speedSlider->setValue(int(v * 100.0));
                m_suppressSignals = false;
            });

    connect(m_formatCombo,    &QComboBox::currentIndexChanged, this, &ParameterPanel::onAnyChanged);
    connect(m_sampleRateCombo,&QComboBox::currentIndexChanged, this, &ParameterPanel::onAnyChanged);
    connect(m_bitDepthCombo,  &QComboBox::currentIndexChanged, this, &ParameterPanel::onAnyChanged);
    connect(m_bitrateCombo,   &QComboBox::currentIndexChanged, this, &ParameterPanel::onAnyChanged);
    connect(m_conflictCombo,  &QComboBox::currentIndexChanged, this, &ParameterPanel::onAnyChanged);

    connect(m_copyMetaCheck,  &QCheckBox::toggled, this, &ParameterPanel::onAnyChanged);
    connect(m_normalizeCheck, &QCheckBox::toggled, this, &ParameterPanel::onAnyChanged);
    connect(m_engineCombo,    &QComboBox::currentIndexChanged, this, &ParameterPanel::onAnyChanged);
    connect(m_threadsSpin,    QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ParameterPanel::onAnyChanged);
}

QGroupBox* ParameterPanel::createSpeedGroup()
{
    auto* box = new QGroupBox(tr("Скорость"), this);
    auto* lay = new QGridLayout(box);

    m_speedSpin = new QDoubleSpinBox(box);
    m_speedSpin->setRange(0.25, 4.00);
    m_speedSpin->setSingleStep(0.05);
    m_speedSpin->setDecimals(2);
    m_speedSpin->setSuffix("×");
    m_speedSpin->setValue(1.0);

    m_speedSlider = new QSlider(Qt::Horizontal, box);
    m_speedSlider->setRange(25, 400);
    m_speedSlider->setValue(100);

    auto* presetsWidget = new QWidget(box);
    auto* presetsLay = new QHBoxLayout(presetsWidget);
    presetsLay->setContentsMargins(0, 0, 0, 0);
    presetsLay->setSpacing(4);

    const QList<double> presets = { 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };
    for (double p : presets) {
        auto* b = new QPushButton(QString("%1×").arg(p, 0, 'g', 4), presetsWidget);
        b->setProperty("presetValue", p);
        b->setAutoDefault(false);
        connect(b, &QPushButton::clicked, this, &ParameterPanel::applyPreset);
        presetsLay->addWidget(b);
        m_presetButtons.push_back(b);
    }
    presetsLay->addStretch(1);

    auto* pitchCheck = new QCheckBox(tr("Сохранять питч (обязательно)"), box);
    pitchCheck->setChecked(true);
    pitchCheck->setEnabled(false);
    pitchCheck->setToolTip(tr("Приложение всегда сохраняет высоту тона. "
                              "Опция отключена для совместимости."));

    lay->addWidget(new QLabel(tr("Коэффициент:"), box), 0, 0);
    lay->addWidget(m_speedSpin, 0, 1);
    lay->addWidget(m_speedSlider, 1, 0, 1, 2);
    lay->addWidget(presetsWidget, 2, 0, 1, 2);
    lay->addWidget(pitchCheck, 3, 0, 1, 2);
    lay->setColumnStretch(1, 1);
    return box;
}

QGroupBox* ParameterPanel::createFormatGroup()
{
    auto* box = new QGroupBox(tr("Формат вывода"), this);
    auto* lay = new QGridLayout(box);

    m_formatCombo = new QComboBox(box);
    m_formatCombo->addItem(tr("Как в источнике"),  int(OutputFormat::SameAsSource));
    m_formatCombo->addItem("WAV",                  int(OutputFormat::Wav));
    m_formatCombo->addItem("FLAC",                 int(OutputFormat::Flac));
    m_formatCombo->addItem("MP3",                  int(OutputFormat::Mp3));
    m_formatCombo->addItem("OGG",                  int(OutputFormat::Ogg));
    m_formatCombo->addItem("M4A",                  int(OutputFormat::M4a));

    m_sampleRateCombo = new QComboBox(box);
    m_sampleRateCombo->addItem(tr("Как в источнике"), 0);
    m_sampleRateCombo->addItem("22050 Hz", 22050);
    m_sampleRateCombo->addItem("44100 Hz", 44100);
    m_sampleRateCombo->addItem("48000 Hz", 48000);
    m_sampleRateCombo->addItem("96000 Hz", 96000);

    m_bitDepthCombo = new QComboBox(box);
    m_bitDepthCombo->addItem(tr("Как в источнике"), 0);
    m_bitDepthCombo->addItem("16 бит",  16);
    m_bitDepthCombo->addItem("24 бит",  24);
    m_bitDepthCombo->addItem("32 float",32);

    m_bitrateCombo = new QComboBox(box);
    m_bitrateCombo->addItem("96 kbps",  96);
    m_bitrateCombo->addItem("128 kbps", 128);
    m_bitrateCombo->addItem("192 kbps", 192);
    m_bitrateCombo->addItem("256 kbps", 256);
    m_bitrateCombo->addItem("320 kbps", 320);
    m_bitrateCombo->setCurrentIndex(2);

    m_conflictCombo = new QComboBox(box);
    m_conflictCombo->addItem(tr("Перезаписать"),   int(ConflictPolicy::Overwrite));
    m_conflictCombo->addItem(tr("Пропустить"),     int(ConflictPolicy::Skip));
    m_conflictCombo->addItem(tr("Переименовать"),  int(ConflictPolicy::Rename));
    m_conflictCombo->addItem(tr("Спросить"),       int(ConflictPolicy::Ask));
    m_conflictCombo->setCurrentIndex(2);

    int row = 0;
    lay->addWidget(new QLabel(tr("Формат:"), box), row, 0);       lay->addWidget(m_formatCombo,     row++, 1);
    lay->addWidget(new QLabel(tr("Частота дискретизации:"), box), row, 0);
    lay->addWidget(m_sampleRateCombo, row++, 1);
    lay->addWidget(new QLabel(tr("Глубина (lossless):"), box), row, 0);
    lay->addWidget(m_bitDepthCombo, row++, 1);
    lay->addWidget(new QLabel(tr("Битрейт (lossy):"), box), row, 0);
    lay->addWidget(m_bitrateCombo, row++, 1);
    lay->addWidget(new QLabel(tr("При совпадении:"), box), row, 0);
    lay->addWidget(m_conflictCombo, row++, 1);
    lay->setColumnStretch(1, 1);

    connect(m_formatCombo, &QComboBox::currentIndexChanged,
            this, &ParameterPanel::updateFormatWidgets);
    updateFormatWidgets();

    return box;
}

QGroupBox* ParameterPanel::createAdvancedGroup()
{
    auto* box = new QGroupBox(tr("Дополнительно"), this);
    auto* lay = new QGridLayout(box);

    m_copyMetaCheck  = new QCheckBox(tr("Копировать метаданные"), box);
    m_copyMetaCheck->setChecked(true);

    m_normalizeCheck = new QCheckBox(tr("Нормализовать громкость"), box);

    m_engineCombo = new QComboBox(box);
    m_engineCombo->addItem("SoundTouch", int(AudioEngine::SoundTouch));
    m_engineCombo->addItem("Rubber Band", int(AudioEngine::RubberBand));

    m_threadsSpin = new QSpinBox(box);
    m_threadsSpin->setRange(1, 64);
    m_threadsSpin->setValue(4);

    int row = 0;
    lay->addWidget(m_copyMetaCheck,  row++, 0, 1, 2);
    lay->addWidget(m_normalizeCheck, row++, 0, 1, 2);
    lay->addWidget(new QLabel(tr("DSP-движок:"), box), row, 0); lay->addWidget(m_engineCombo, row++, 1);
    lay->addWidget(new QLabel(tr("Потоков:"), box),    row, 0); lay->addWidget(m_threadsSpin, row++, 1);
    lay->setColumnStretch(1, 1);

    return box;
}

ProcessingParameters ParameterPanel::parameters() const
{
    ProcessingParameters p;
    p.speedFactor     = m_speedSpin->value();
    p.format          = static_cast<OutputFormat>(m_formatCombo->currentData().toInt());
    p.sampleRate      = m_sampleRateCombo->currentData().toInt();
    p.bitDepth        = m_bitDepthCombo->currentData().toInt();
    p.bitrateKbps     = m_bitrateCombo->currentData().toInt();
    p.conflict        = static_cast<ConflictPolicy>(m_conflictCombo->currentData().toInt());
    p.copyMetadata    = m_copyMetaCheck->isChecked();
    p.normalizeVolume = m_normalizeCheck->isChecked();
    p.engine          = static_cast<AudioEngine>(m_engineCombo->currentData().toInt());
    p.threadCount     = m_threadsSpin->value();
    return p;
}

void ParameterPanel::setParameters(const ProcessingParameters& p)
{
    m_suppressSignals = true;
    m_speedSpin->setValue(p.speedFactor);
    m_speedSlider->setValue(int(p.speedFactor * 100.0));

    auto setCombo = [](QComboBox* c, int value) {
        const int idx = c->findData(value);
        if (idx >= 0) c->setCurrentIndex(idx);
    };
    setCombo(m_formatCombo,     int(p.format));
    setCombo(m_sampleRateCombo, p.sampleRate);
    setCombo(m_bitDepthCombo,   p.bitDepth);
    setCombo(m_bitrateCombo,    p.bitrateKbps);
    setCombo(m_conflictCombo,   int(p.conflict));
    setCombo(m_engineCombo,     int(p.engine));

    m_copyMetaCheck->setChecked(p.copyMetadata);
    m_normalizeCheck->setChecked(p.normalizeVolume);
    m_threadsSpin->setValue(p.threadCount);
    m_suppressSignals = false;

    updateFormatWidgets();
}

void ParameterPanel::onAnyChanged()
{
    if (m_suppressSignals) return;
    emit parametersChanged(parameters());
}

void ParameterPanel::applyPreset()
{
    auto* b = qobject_cast<QPushButton*>(sender());
    if (!b) return;
    const double v = b->property("presetValue").toDouble();
    m_speedSpin->setValue(v);
    m_speedSlider->setValue(int(v * 100.0));
    onAnyChanged();
}

void ParameterPanel::updateFormatWidgets()
{
    const OutputFormat f = static_cast<OutputFormat>(m_formatCombo->currentData().toInt());
    const bool isLossy = (f == OutputFormat::Mp3 ||
                          f == OutputFormat::Ogg ||
                          f == OutputFormat::M4a);
    const bool isLosslessExplicit = (f == OutputFormat::Wav || f == OutputFormat::Flac);

    m_bitrateCombo->setEnabled(isLossy);
    m_bitDepthCombo->setEnabled(isLosslessExplicit);
}
