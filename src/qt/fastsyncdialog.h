// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_FASTSYNCDIALOG_H
#define DOGECOIN_QT_FASTSYNCDIALOG_H

#include <QDialog>
#include <QAtomicInt>
#include <QThread>

class QLabel;
class QProgressBar;
class QPushButton;
class QTextEdit;

/**
 * Product P1.7: first-run / on-demand Fast Sync from CDN.
 * Resolves latest.json, stream-hashes the multi-GB artifact (WinHTTP on PE),
 * fail-closed digest check, then loadtxoutset + activate when attested.
 */
class FastSyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FastSyncDialog(QWidget* parent = 0);
    ~FastSyncDialog();

    /** True if load+activate completed successfully this session. */
    bool succeeded() const { return m_succeeded; }

public Q_SLOTS:
    void startFetch();
    void abortFetch();
    void skipToP2P();

private Q_SLOTS:
    void onWorkerProgress(qint64 bytes, qint64 expected);
    void onWorkerStatus(const QString& text);
    void onWorkerFinished(bool ok, const QString& message);

private:
    void setBusy(bool busy);
    void clearPendingOfferFlag();

    QLabel* m_title;
    QLabel* m_body;
    QLabel* m_status;
    QProgressBar* m_bar;
    QPushButton* m_startBtn;
    QPushButton* m_skipBtn;
    QPushButton* m_abortBtn;
    QPushButton* m_closeBtn;

    QThread* m_thread;
    QAtomicInt m_abort;
    bool m_succeeded;
    bool m_running;
};

/** Worker lives in m_thread; communicates via signals. */
class FastSyncWorker : public QObject
{
    Q_OBJECT
public:
    explicit FastSyncWorker(QAtomicInt* abortFlag, QObject* parent = 0);

public Q_SLOTS:
    void process();

Q_SIGNALS:
    void progress(qint64 bytes, qint64 expected);
    void status(const QString& text);
    void finished(bool ok, const QString& message);

private:
    QAtomicInt* m_abort;
    static bool ProgressThunk(uint64_t bytes_done, int64_t expected_bytes, void* ctx);
};

#endif // DOGECOIN_QT_FASTSYNCDIALOG_H
