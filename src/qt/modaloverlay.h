// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_MODALOVERLAY_H
#define DOGECOIN_QT_MODALOVERLAY_H

#include <QDateTime>
#include <QWidget>

//! The required delta of headers to the estimated number of available headers until we show the IBD progress
static constexpr int HEADER_HEIGHT_DELTA_SYNC = 24;

namespace Ui {
    class ModalOverlay;
}

/** Modal overlay to display information about the chain-sync state */
class ModalOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ModalOverlay(QWidget *parent);
    ~ModalOverlay();

public Q_SLOTS:
    void tipUpdate(int count, const QDateTime& blockDate, double nVerificationProgress);
    void setKnownBestHeight(int count, const QDateTime& blockDate);
    /** Header-sync progress 0.0–1.0 (estimated); -1 if not in header-sync phase. */
    double headerSyncProgress() const;

    void toggleVisibility();
    // will show or hide the modal layer
    void showHide(bool hide = false, bool userRequested = false);
    void closeClicked();
    bool isLayerVisible() { return layerIsVisible; }

protected:
    bool eventFilter(QObject * obj, QEvent * ev);
    bool event(QEvent* ev);

private:
    /** Refresh blocks-left / header phase labels from bestHeaderHeight. */
    void updateHeaderSyncLabel(int blockCount);
    /** When headers are still far ahead, drive the progress bar from header estimate. */
    void setProgressForHeadersPhase();

    Ui::ModalOverlay *ui;
    int bestHeaderHeight; //best known height (based on the headers)
    QDateTime bestHeaderDate;
    int lastBlockCount; // last validated tip height from tipUpdate
    double lastVerificationProgress;
    QVector<QPair<qint64, double> > blockProcessTime;
    bool layerIsVisible;
    bool userClosed;
};

#endif // DOGECOIN_QT_MODALOVERLAY_H
