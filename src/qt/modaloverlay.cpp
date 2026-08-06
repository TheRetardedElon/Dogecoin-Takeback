// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2021-2022 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "modaloverlay.h"
#include "ui_modaloverlay.h"

#include "guiutil.h"

#include "chainparams.h"

#include <QResizeEvent>
#include <QPropertyAnimation>

ModalOverlay::ModalOverlay(QWidget *parent) :
QWidget(parent),
ui(new Ui::ModalOverlay),
bestHeaderHeight(0),
bestHeaderDate(QDateTime()),
lastBlockCount(0),
lastVerificationProgress(0.0),
layerIsVisible(false),
userClosed(false)
{
    ui->setupUi(this);
    connect(ui->closeButton, SIGNAL(clicked()), this, SLOT(closeClicked()));
    if (parent) {
        parent->installEventFilter(this);
        raise();
    }

    blockProcessTime.clear();
    setVisible(false);
    // Clear stale defaults so we never look "stuck at 0%" without labels
    ui->percentageProgress->setText(tr("…"));
    ui->progressBar->setValue(0);
    ui->numberOfBlocksLeft->setText(tr("Starting…"));
    ui->expectedTimeLeft->setText(tr("…"));
    ui->progressIncreasePerH->setText(tr("…"));
}

ModalOverlay::~ModalOverlay()
{
    delete ui;
}

bool ModalOverlay::eventFilter(QObject * obj, QEvent * ev) {
    if (obj == parent()) {
        if (ev->type() == QEvent::Resize) {
            QResizeEvent * rev = static_cast<QResizeEvent*>(ev);
            resize(rev->size());
            if (!layerIsVisible)
                setGeometry(0, height(), width(), height());

        }
        else if (ev->type() == QEvent::ChildAdded) {
            raise();
        }
    }
    return QWidget::eventFilter(obj, ev);
}

//! Tracks parent widget changes
bool ModalOverlay::event(QEvent* ev) {
    if (ev->type() == QEvent::ParentAboutToChange) {
        if (parent()) parent()->removeEventFilter(this);
    }
    else if (ev->type() == QEvent::ParentChange) {
        if (parent()) {
            parent()->installEventFilter(this);
            raise();
        }
    }
    return QWidget::event(ev);
}

void ModalOverlay::setKnownBestHeight(int count, const QDateTime& blockDate)
{
    if (count > bestHeaderHeight) {
        bestHeaderHeight = count;
        bestHeaderDate = blockDate;
    }
    // Header tips move without tipUpdate — keep the modal in sync so progress
    // is not stuck at 0% while the status bar already shows header %.
    updateHeaderSyncLabel(lastBlockCount);
    if (headerSyncProgress() >= 0.0)
        setProgressForHeadersPhase();
}

double ModalOverlay::headerSyncProgress() const
{
    if (!bestHeaderDate.isValid() || bestHeaderHeight <= 0)
        return -1.0;

    const QDateTime currentDate = QDateTime::currentDateTime();
    const int estHeadersLeft =
        bestHeaderDate.secsTo(currentDate) / Params().GetConsensus(bestHeaderHeight).nPowTargetSpacing;
    // Same condition as status-bar "Syncing Headers (x%)..."
    if (estHeadersLeft <= HEADER_HEIGHT_DELTA_SYNC)
        return -1.0; // headers caught up enough; use block verification progress

    const double denom = static_cast<double>(bestHeaderHeight + estHeadersLeft);
    if (denom <= 0.0)
        return -1.0;
    const double p = static_cast<double>(bestHeaderHeight) / denom;
    if (p < 0.0)
        return 0.0;
    if (p > 1.0)
        return 1.0;
    return p;
}

void ModalOverlay::setProgressForHeadersPhase()
{
    const double p = headerSyncProgress();
    if (p < 0.0)
        return;
    ui->percentageProgress->setText(
        tr("%1% (headers)").arg(QString::number(p * 100.0, 'f', 1)));
    ui->progressBar->setValue(static_cast<int>(p * 100.0 + 0.5));
    // Block-validation ETA is meaningless until headers settle
    ui->expectedTimeLeft->setText(tr("Syncing headers first…"));
    if (ui->progressIncreasePerH->text().isEmpty() ||
        ui->progressIncreasePerH->text() == QLatin1String("…") ||
        ui->progressIncreasePerH->text() == QLatin1String("0.00%")) {
        ui->progressIncreasePerH->setText(tr("n/a (headers)"));
    }
}

void ModalOverlay::updateHeaderSyncLabel(int blockCount)
{
    if (!bestHeaderDate.isValid())
        return;

    const QDateTime currentDate = QDateTime::currentDateTime();
    const int estimateNumHeadersLeft =
        bestHeaderDate.secsTo(currentDate) / Params().GetConsensus(bestHeaderHeight).nPowTargetSpacing;
    const bool hasBestHeader = bestHeaderHeight >= blockCount;

    if (estimateNumHeadersLeft < HEADER_HEIGHT_DELTA_SYNC && hasBestHeader) {
        ui->numberOfBlocksLeft->setText(QString::number(bestHeaderHeight - blockCount));
    } else {
        ui->numberOfBlocksLeft->setText(
            tr("Unknown. Syncing Headers (%1)…").arg(bestHeaderHeight));
        ui->expectedTimeLeft->setText(tr("Unknown…"));
    }
}

void ModalOverlay::tipUpdate(int count, const QDateTime& blockDate, double nVerificationProgress)
{
    QDateTime currentDate = QDateTime::currentDateTime();
    lastBlockCount = count;
    lastVerificationProgress = nVerificationProgress;

    // keep a vector of samples of verification progress at height
    blockProcessTime.push_front(qMakePair(currentDate.toMSecsSinceEpoch(), nVerificationProgress));

    // show progress speed if we have more then one sample
    if (blockProcessTime.size() >= 2)
    {
        double progressStart = blockProcessTime[0].second;
        double progressDelta = 0;
        double progressPerHour = 0;
        qint64 timeDelta = 0;
        qint64 remainingMSecs = 0;
        double remainingProgress = 1.0 - nVerificationProgress;
        for (int i = 1; i < blockProcessTime.size(); i++)
        {
            QPair<qint64, double> sample = blockProcessTime[i];

            // take first sample after 500 seconds or last available one
            if (sample.first < (currentDate.toMSecsSinceEpoch() - 500 * 1000) || i == blockProcessTime.size() - 1) {
                progressDelta = progressStart-sample.second;
                timeDelta = blockProcessTime[0].first - sample.first;
                progressPerHour = progressDelta/(double)timeDelta*1000*3600;
                remainingMSecs = remainingProgress / progressDelta * timeDelta;
                break;
            }
        }
        // show progress increase per hour
        ui->progressIncreasePerH->setText(QString::number(progressPerHour > 0 ? progressPerHour*100 : 0, 'f', 2)+"%");

        // show expected remaining time, if we have a sample
        if (remainingMSecs > 0)
            ui->expectedTimeLeft->setText(GUIUtil::formatNiceTimeOffset(remainingMSecs/1000.0));

        static const int MAX_SAMPLES = 5000;
        if (blockProcessTime.count() > MAX_SAMPLES)
            blockProcessTime.remove(MAX_SAMPLES, blockProcessTime.count()-MAX_SAMPLES);
    }

    // show the last block date
    ui->newestBlockDate->setText(blockDate.toString());

    // Progress bar: during header sync, nVerificationProgress stays ~0% even while
    // the status bar correctly reports header %. Prefer header estimate in that phase.
    const double headerP = headerSyncProgress();
    if (headerP >= 0.0) {
        setProgressForHeadersPhase();
    } else {
        ui->percentageProgress->setText(
            tr("%1% (blocks)").arg(QString::number(nVerificationProgress * 100.0, 'f', 2)));
        ui->progressBar->setValue(static_cast<int>(nVerificationProgress * 100.0 + 0.5));
    }

    updateHeaderSyncLabel(count);
}

void ModalOverlay::toggleVisibility()
{
    showHide(layerIsVisible, true);
    if (!layerIsVisible)
        userClosed = true;
}

void ModalOverlay::showHide(bool hide, bool userRequested)
{
    if ( (layerIsVisible && !hide) || (!layerIsVisible && hide) || (!hide && userClosed && !userRequested))
        return;

    if (!isVisible() && !hide)
        setVisible(true);

    setGeometry(0, hide ? 0 : height(), width(), height());

    QPropertyAnimation* animation = new QPropertyAnimation(this, "pos");
    animation->setDuration(300);
    animation->setStartValue(QPoint(0, hide ? 0 : this->height()));
    animation->setEndValue(QPoint(0, hide ? this->height() : 0));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
    layerIsVisible = !hide;
}

void ModalOverlay::closeClicked()
{
    showHide(true);
    userClosed = true;
}
