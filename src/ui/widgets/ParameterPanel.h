#pragma once

#include <QWidget>

#include "core/Core.h"

class QDoubleSpinBox;
class QSlider;
class QPushButton;
class QCheckBox;
class QComboBox;
class QSpinBox;
class QGroupBox;

class ParameterPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ParameterPanel(QWidget* parent = nullptr);

    ProcessingParameters parameters() const;
    void setParameters(const ProcessingParameters& p);

signals:
    void parametersChanged(const ProcessingParameters& p);

private slots:
    void onAnyChanged();
    void applyPreset();

private:
    QGroupBox* createSpeedGroup();
    QGroupBox* createFormatGroup();
    QGroupBox* createAdvancedGroup();
    void       updateFormatWidgets();

    // speed
    QDoubleSpinBox* m_speedSpin = nullptr;
    QSlider*        m_speedSlider = nullptr;
    QList<QPushButton*> m_presetButtons;

    // format
    QComboBox* m_formatCombo    = nullptr;
    QComboBox* m_sampleRateCombo= nullptr;
    QComboBox* m_bitDepthCombo  = nullptr;
    QComboBox* m_bitrateCombo   = nullptr;
    QComboBox* m_conflictCombo  = nullptr;

    // advanced
    QCheckBox* m_copyMetaCheck  = nullptr;
    QCheckBox* m_normalizeCheck = nullptr;
    QComboBox* m_engineCombo    = nullptr;
    QSpinBox*  m_threadsSpin    = nullptr;

    bool m_suppressSignals = false;
};
