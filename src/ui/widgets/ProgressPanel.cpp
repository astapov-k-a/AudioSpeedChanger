#include "ui/widgets/ProgressPanel.h"
#include "utils/DurationFormat.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

ProgressPanel::ProgressPanel(QWidget* parent) : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    auto* overallRow = new QHBoxLayout();
    m_overallLbl = new QLabel(tr("Общий:"), this);
    m_overallLbl->setMinimumWidth(80);
    m_overall = new QProgressBar(this);
    m_overall->setRange(0, 100);
    m_overall->setTextVisible(true);
    m_overall->setFormat(tr("%p% (%v/%m)"));
    overallRow->addWidget(m_overallLbl);
    overallRow->addWidget(m_overall, 1);

    auto* currentRow = new QHBoxLayout();
    m_currentLbl = new QLabel(tr("Текущий:"), this);
    m_currentLbl->setMinimumWidth(80);
    m_current = new QProgressBar(this);
    m_current->setRange(0, 100);
    m_current->setTextVisible(true);
    m_etaLbl = new QLabel(QStringLiteral("—"), this);
    m_etaLbl->setMinimumWidth(80);
    m_etaLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    currentRow->addWidget(m_currentLbl);
    currentRow->addWidget(m_current, 1);
    currentRow->addWidget(new QLabel(tr("ETA:"), this));
    currentRow->addWidget(m_etaLbl);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    m_pauseBtn  = new QPushButton(tr("Пауза"), this);
    m_cancelBtn = new QPushButton(tr("Отмена"), this);
    btnRow->addWidget(m_pauseBtn);
    btnRow->addWidget(m_cancelBtn);

    root->addLayout(overallRow);
    root->addLayout(currentRow);
    root->addLayout(btnRow);

    connect(m_pauseBtn,  &QPushButton::clicked, this, &ProgressPanel::onPauseClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ProgressPanel::cancelRequested);

    reset();
    setRunning(false);
}

void ProgressPanel::reset()
{
    m_overall->setRange(0, 100);
    m_overall->setValue(0);
    m_current->setRange(0, 100);
    m_current->setValue(0);
    m_currentLbl->setText(tr("Текущий:"));
    m_etaLbl->setText(QStringLiteral("—"));
    m_paused = false;
    if (m_pauseBtn) m_pauseBtn->setText(tr("Пауза"));
}

void ProgressPanel::setOverall(int done, int total)
{
    total = qMax(0, total);
    m_overall->setRange(0, qMax(1, total));
    m_overall->setValue(done);
    m_overall->setFormat(tr("%p% (%1/%2)").arg(done).arg(total));
}

void ProgressPanel::setCurrentFile(const QString& name)
{
    m_currentLbl->setText(name.isEmpty() ? tr("Текущий:") : name);
}

void ProgressPanel::setCurrentProgress(int percent)
{
    m_current->setValue(qBound(0, percent, 100));
}

void ProgressPanel::setEta(double seconds)
{
    m_etaLbl->setText(DurationFormat::etaFromSeconds(seconds));
}

void ProgressPanel::setRunning(bool running)
{
    m_pauseBtn->setEnabled(running);
    m_cancelBtn->setEnabled(running);
}

void ProgressPanel::setPaused(bool paused)
{
    m_paused = paused;
    m_pauseBtn->setText(paused ? tr("Продолжить") : tr("Пауза"));
}

void ProgressPanel::onPauseClicked()
{
    if (m_paused) emit resumeRequested();
    else          emit pauseRequested();
}
