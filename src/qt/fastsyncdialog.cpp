// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/dogecoin-config.h"
#endif

#include "fastsyncdialog.h"

#include "chainparams.h"
#include "guiutil.h"
#include "net.h"
#include "node/chainstate.h"
#include "node/snapshot_fetch.h"
#include "node/utxo_snapshot.h"
#include "sync.h"
#include "util.h"
#include "validation.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <QVBoxLayout>

#include <string>

// 24h — multi-GB CDN download over residential links
static const int CDN_FETCH_TIMEOUT_SEC = 86400;

// ---------- worker ----------

FastSyncWorker::FastSyncWorker(QAtomicInt* abortFlag, QObject* parent)
    : QObject(parent), m_abort(abortFlag)
{
}

bool FastSyncWorker::ProgressThunk(uint64_t bytes_done, int64_t expected_bytes, void* ctx)
{
    FastSyncWorker* self = static_cast<FastSyncWorker*>(ctx);
    if (!self || !self->m_abort)
        return true;
    if (self->m_abort->load() != 0)
        return false;
    Q_EMIT self->progress(static_cast<qint64>(bytes_done), static_cast<qint64>(expected_bytes));
    return true;
}

void FastSyncWorker::process()
{
    // RAII: re-enable P2P if we quieted the network around load/activate.
    struct NetworkQuietGuard {
        bool quieted;
        bool prevActive;
        NetworkQuietGuard() : quieted(false), prevActive(true) {}
        void Quiet()
        {
            if (!g_connman || quieted)
                return;
            prevActive = g_connman->GetNetworkActive();
            if (prevActive) {
                g_connman->SetNetworkActive(false);
                quieted = true;
            }
        }
        void Restore()
        {
            if (quieted && g_connman) {
                g_connman->SetNetworkActive(prevActive);
                quieted = false;
            }
        }
        ~NetworkQuietGuard() { Restore(); }
    } netGuard;

    try {
        // Mid-IBD safety gate: Fast Sync + prune mid-sync is the crash path
        // (partial blocks, wallet locator beyond pruned data). Prefer fresh datadir.
        {
            LOCK(cs_main);
            const int tipH = chainActive.Height();
            if (IsSnapshotChainstateActive()) {
                Q_EMIT finished(false, tr("A Fast Sync snapshot is already active on this node."));
                return;
            }
            // Deep mid-sync with pruning: refuse and tell the user how to recover.
            // Low tip (<10k) is still OK — first-run headers / early IBD.
            if (fPruneMode && tipH > 10000) {
                Q_EMIT finished(false,
                    tr("Fast Sync refused: this node is mid-sync at height %1 with pruning on.\n\n"
                       "That combination breaks after snapshot activate (missing block bodies + "
                       "wallet rescan beyond pruned data).\n\n"
                       "What to do:\n"
                       "1. Close Core Pro fully.\n"
                       "2. Use a fresh data directory (Settings → Options → Reset, or a new -datadir), "
                       "OR delete only blocks/ + chainstate/ if you have no funds to keep.\n"
                       "3. Restart and run Fast Sync from height 0.\n\n"
                       "No manual pause button — the node automates quiet-network load when the "
                       "datadir is ready.")
                        .arg(tipH));
                return;
            }
        }

        Q_EMIT status(tr("Resolving CDN manifest…"));
        std::string manifest_src = GetArg("-snapshotmanifest", DEFAULT_SNAPSHOT_MANIFEST_URL);
        if (manifest_src.empty())
            manifest_src = DEFAULT_SNAPSHOT_MANIFEST_URL;

        // Custom direct URL path (Options SoftSet)
        const std::string direct_url = GetArg("-snapshoturl", "");
        const std::string direct_sha = GetArg("-snapshotsha256", "");

        SnapshotArtifactManifest m;
        std::string error;
        std::string artifact_url;
        uint256 expected;
        int64_t size_hint = -1;
        int height_hint = -1;

        const bool direct_dat = !direct_url.empty() && !direct_sha.empty() &&
                                direct_url.find("latest.json") == std::string::npos;

        if (direct_dat) {
            artifact_url = direct_url;
            if (!ParseSha256Hex(direct_sha, expected, error)) {
                Q_EMIT finished(false, QString::fromStdString(error));
                return;
            }
            // Custom direct .dat: still check disk if size unknown (skip size gate)
            m.size_bytes = -1;
            m.height = -1;
        } else {
            if (!direct_url.empty() && direct_url.find("latest.json") != std::string::npos)
                manifest_src = direct_url;
            if (!ResolveSnapshotFromManifest(manifest_src, m, error, 120)) {
                Q_EMIT finished(false, QString::fromStdString(error));
                return;
            }
            if (!ParseSha256Hex(m.artifact_sha256_hex, expected, error)) {
                Q_EMIT finished(false, QString::fromStdString(error));
                return;
            }
            artifact_url = m.url;
            size_hint = m.size_bytes;
            height_hint = m.height;
        }

        if (m_abort->load() != 0) {
            Q_EMIT finished(false, tr("Aborted."));
            return;
        }

        // Preflight: attestation + disk + CDN probe — before multi-GB download
        Q_EMIT status(tr("Checking snapshot compatibility, disk space, and CDN…"));
        {
            const bool require_attested = !direct_dat; // custom .dat may be advanced
            if (!PreValidateSnapshotForFastSync(m, artifact_url, error, require_attested)) {
                Q_EMIT finished(false, QString::fromStdString(error));
                return;
            }
        }

        QString sizeNote;
        if (size_hint > 0) {
            const double gib = size_hint / (1024.0 * 1024.0 * 1024.0);
            sizeNote = tr(" (~%1 GiB)").arg(QString::number(gib, 'f', 1));
        }

        // Mesh M2: candidate list from manifest urls[] (primary first); single direct URL otherwise
        std::vector<std::string> candidates;
        if (direct_dat) {
            candidates.push_back(artifact_url);
        } else {
            candidates = m.CandidateUrls();
            if (candidates.empty() && !artifact_url.empty())
                candidates.push_back(artifact_url);
        }

        QStringList tryList;
        for (size_t i = 0; i < candidates.size(); ++i)
            tryList << QString::fromStdString(candidates[i]);
        Q_EMIT status(tr("Downloading attested snapshot%1…\nTrying %2 source(s):\n%3")
                          .arg(sizeNote)
                          .arg(candidates.size())
                          .arg(tryList.join(QLatin1String("\n"))));
        if (size_hint > 0)
            Q_EMIT progress(0, size_hint);

        fs::path dest = GetDataDir() / "snapshots" / "utxo_fetch.dat";
        {
            boost::system::error_code ec;
            fs::create_directories(dest.parent_path(), ec);
        }

        uint64_t bytes = 0;
        std::string used_source;
        if (!FetchSnapshotArtifactFromCandidates(candidates, dest, expected, bytes, error,
                                                 &used_source, CDN_FETCH_TIMEOUT_SEC,
                                                 ProgressThunk, this)) {
            Q_EMIT finished(false, QString::fromStdString(error));
            return;
        }
        if (!used_source.empty())
            artifact_url = used_source;

        if (m_abort->load() != 0) {
            Q_EMIT finished(false, tr("Aborted after download."));
            return;
        }

        Q_EMIT status(tr("Download verified. Quieting P2P, then loading UTXO snapshot…"));
        Q_EMIT progress(static_cast<qint64>(bytes), static_cast<qint64>(bytes));

        if (!HasBackgroundChainstate()) {
            Q_EMIT finished(false, tr("Background chainstate not initialized (node not ready)."));
            return;
        }

        // Automate the critical section: pause peer block chatter so ActivateBestChain
        // does not race DisconnectTip/ConnectTip on a body-less snapshot tip.
        netGuard.Quiet();
        Q_EMIT status(tr("Network quiet. Loading UTXO snapshot into chainstate…"));

        uint64_t coins_loaded = 0;
        uint256 base_hash;
        int base_height = -1;
        if (!LoadUTXOSnapshot(dest, coins_loaded, base_hash, base_height, error)) {
            netGuard.Restore();
            Q_EMIT finished(false, QString::fromStdString(error));
            return;
        }

        Q_EMIT status(tr("Snapshot loaded (height %1, %2 coins). Activating tip…")
                          .arg(base_height)
                          .arg(QString::number(coins_loaded)));

        if (!ActivateLoadedSnapshot(error)) {
            netGuard.Restore();
            // Still useful if load succeeded but activate needs more headers — report load OK
            Q_EMIT finished(false,
                            tr("Loaded snapshot but activate failed: %1\n"
                               "You can retry activatesnapshot from the console after headers sync.")
                                .arg(QString::fromStdString(error)));
            return;
        }

        // Brief settle, then re-enable P2P so background history + tip advance resume.
        netGuard.Restore();
        Q_EMIT status(tr("Snapshot active. Network restored — background validation continues…"));

        const AssumeutxoData* att = Params().AssumeutxoForHeight(base_height);
        QString attNote = att ? tr("Attested height — background history proof will continue.")
                              : tr("Height not in mapAssumeutxo — treat as advanced/dev path.");

        Q_EMIT finished(true,
                        tr("Fast Sync complete.\n"
                           "Active tip height: %1\n"
                           "Coins loaded: %2\n"
                           "%3\n\n"
                           "Wallet becomes usable while historical blocks validate in the background.\n"
                           "P2P was automatically quieted during load/activate and restored afterward.")
                            .arg(base_height)
                            .arg(QString::number(coins_loaded))
                            .arg(attNote));
        (void)height_hint;
    } catch (const std::exception& e) {
        Q_EMIT finished(false, QString::fromUtf8(e.what()));
    } catch (...) {
        Q_EMIT finished(false, tr("Unknown error during Fast Sync."));
    }
}

// ---------- dialog ----------

FastSyncDialog::FastSyncDialog(QWidget* parent)
    : QDialog(parent),
      m_thread(0),
      m_succeeded(false),
      m_running(false)
{
    setWindowTitle(tr("Fast Sync — attested UTXO snapshot"));
    setModal(true);
    setMinimumWidth(520);

    m_title = new QLabel(tr("<h2>Fast Sync (recommended)</h2>"), this);
    m_body = new QLabel(this);
    m_body->setWordWrap(true);
    m_body->setTextFormat(Qt::RichText);
    m_body->setText(tr(
        "<p>Download an <b>attested UTXO snapshot</b> from the public GPE CDN "
        "(<code>sync.doge.gopastearth.com</code>), verify the file SHA-256 "
        "(fail closed), load it, and activate the tip.</p>"
        "<p>Typical size is multi‑GB and may take a while on home internet. "
        "Your live wallet data always stays on local disk — cloud is only a dumb download pipe.</p>"
        "<p>After activation, the wallet can be usable while historical blocks "
        "are proven in the background. You can also skip and use normal P2P sync.</p>"
        "<p><b>Automated safety:</b> mid-sync+prune is blocked, P2P is quieted "
        "during load/activate, and missing block bodies no longer crash the node.</p>"));

    m_status = new QLabel(tr("Ready."), this);
    m_status->setWordWrap(true);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 1000);
    m_bar->setValue(0);
    m_bar->setTextVisible(true);
    m_bar->setFormat(tr("Idle"));

    m_startBtn = new QPushButton(tr("Download & activate snapshot"), this);
    m_skipBtn = new QPushButton(tr("Continue with normal sync (P2P)"), this);
    m_abortBtn = new QPushButton(tr("Abort download"), this);
    m_abortBtn->setEnabled(false);
    m_closeBtn = new QPushButton(tr("Close"), this);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addWidget(m_startBtn);
    buttons->addWidget(m_skipBtn);
    buttons->addStretch();
    buttons->addWidget(m_abortBtn);
    buttons->addWidget(m_closeBtn);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_title);
    layout->addWidget(m_body);
    layout->addWidget(m_status);
    layout->addWidget(m_bar);
    layout->addLayout(buttons);

    connect(m_startBtn, SIGNAL(clicked()), this, SLOT(startFetch()));
    connect(m_skipBtn, SIGNAL(clicked()), this, SLOT(skipToP2P()));
    connect(m_abortBtn, SIGNAL(clicked()), this, SLOT(abortFetch()));
    connect(m_closeBtn, SIGNAL(clicked()), this, SLOT(reject()));
}

FastSyncDialog::~FastSyncDialog()
{
    abortFetch();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
        delete m_thread;
        m_thread = 0;
    }
}

void FastSyncDialog::setBusy(bool busy)
{
    m_running = busy;
    m_startBtn->setEnabled(!busy);
    m_skipBtn->setEnabled(!busy);
    m_abortBtn->setEnabled(busy);
    m_closeBtn->setEnabled(!busy);
}

void FastSyncDialog::clearPendingOfferFlag()
{
    QSettings settings;
    settings.setValue("fPendingFastSyncOffer", false);
}

void FastSyncDialog::startFetch()
{
    if (m_running)
        return;
    m_abort.store(0);
    setBusy(true);
    m_bar->setValue(0);
    m_bar->setFormat(tr("Starting…"));
    m_status->setText(tr("Starting Fast Sync worker…"));

    if (m_thread) {
        m_thread->quit();
        m_thread->wait(1000);
        delete m_thread;
        m_thread = 0;
    }

    m_thread = new QThread(this);
    FastSyncWorker* worker = new FastSyncWorker(&m_abort);
    worker->moveToThread(m_thread);
    connect(m_thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, SIGNAL(progress(qint64,qint64)), this, SLOT(onWorkerProgress(qint64,qint64)));
    connect(worker, SIGNAL(status(QString)), this, SLOT(onWorkerStatus(QString)));
    connect(worker, SIGNAL(finished(bool,QString)), this, SLOT(onWorkerFinished(bool,QString)));
    connect(worker, SIGNAL(finished(bool,QString)), worker, SLOT(deleteLater()));
    connect(worker, SIGNAL(finished(bool,QString)), m_thread, SLOT(quit()));
    m_thread->start();
}

void FastSyncDialog::abortFetch()
{
    m_abort.store(1);
    if (m_running)
        m_status->setText(tr("Abort requested… finishing current I/O."));
}

void FastSyncDialog::skipToP2P()
{
    clearPendingOfferFlag();
    QSettings settings;
    // Keep prefer-fast prune benefits; user just declined CDN snapshot now
    m_status->setText(tr("Continuing with normal P2P sync."));
    reject();
}

void FastSyncDialog::onWorkerProgress(qint64 bytes, qint64 expected)
{
    if (expected > 0) {
        int v = static_cast<int>((bytes * 1000) / expected);
        if (v > 1000)
            v = 1000;
        m_bar->setValue(v);
        const double doneGiB = bytes / (1024.0 * 1024.0 * 1024.0);
        const double totalGiB = expected / (1024.0 * 1024.0 * 1024.0);
        m_bar->setFormat(tr("%p% — %1 / %2 GiB")
                             .arg(QString::number(doneGiB, 'f', 2))
                             .arg(QString::number(totalGiB, 'f', 2)));
    } else {
        m_bar->setRange(0, 0); // busy indicator
        const double doneGiB = bytes / (1024.0 * 1024.0 * 1024.0);
        m_bar->setFormat(tr("%1 GiB downloaded…").arg(QString::number(doneGiB, 'f', 2)));
    }
}

void FastSyncDialog::onWorkerStatus(const QString& text)
{
    m_status->setText(text);
}

void FastSyncDialog::onWorkerFinished(bool ok, const QString& message)
{
    setBusy(false);
    m_bar->setRange(0, 1000);
    if (ok) {
        m_succeeded = true;
        m_bar->setValue(1000);
        m_bar->setFormat(tr("Done"));
        m_status->setText(message);
        clearPendingOfferFlag();
        QMessageBox::information(this, tr("Fast Sync"), message);
        accept();
    } else {
        m_bar->setFormat(tr("Failed / aborted"));
        m_status->setText(message);
        QMessageBox::warning(this, tr("Fast Sync"), message);
    }
}
