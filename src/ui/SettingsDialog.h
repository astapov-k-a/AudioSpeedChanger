#pragma once

#include <QDialog>

class AppSettings;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QComboBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(AppSettings* settings, QWidget* parent = nullptr);

signals:
    void settingsChanged();

private slots:
    void onAccepted();

private:
    void loadFromSettings();
    void saveToSettings();

    AppSettings* m_settings = nullptr;

    QCheckBox* m_darkTheme       = nullptr;
    QCheckBox* m_includeSubdirs  = nullptr;
    QLineEdit* m_extFilter       = nullptr;
    QSpinBox*  m_threads         = nullptr;
    QComboBox* m_engineCombo     = nullptr;
};
