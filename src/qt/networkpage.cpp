// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "networkpage.h"

#include "bantablemodel.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "net.h"
#include "netbase.h"
#include "peermapwidget.h"
#include "peertablemodel.h"
#include "platformstyle.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

NetworkPage::NetworkPage(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      clientModel(0),
      connectionsLabel(0),
      blocksLabel(0),
      headersLabel(0),
      ibdLabel(0),
      ibdDetailLabel(0),
      trafficLabel(0),
      warningsLabel(0),
      peerMap(0),
      mapHostLayout(0),
      pollTimer(0),
      updatingNetworkToggle(false),
      peersContextMenu(0),
      banContextMenu(0)
{
    setupUi();
    setupContextMenus();
}

void NetworkPage::setupUi()
{
    setObjectName(QStringLiteral("networkPage"));
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    QHBoxLayout* head = new QHBoxLayout();
    QLabel* title = new QLabel(tr("Network"));
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 4);
    tf.setBold(true);
    title->setFont(tf);
    head->addWidget(title);
    head->addStretch();

    networkActiveCheck = new QCheckBox(tr("Network activity enabled"));
    networkActiveCheck->setToolTip(tr("When off, this node stops connecting and accepting peers (local-only mode)."));
    head->addWidget(networkActiveCheck);

    consoleBtn = new QPushButton(tr("Open Debug Console"));
    consoleBtn->setObjectName(QStringLiteral("homeQuickButton"));
    consoleBtn->setToolTip(tr("Full console, traffic graph, and advanced peer tools"));
    head->addWidget(consoleBtn);
    root->addLayout(head);

    QLabel* sub = new QLabel(tr("Live map of your connected peers around the world. "
                                "Click a cyan/magenta dot for node details. "
                                "Disconnect/ban tools are in the Debug Console. "
                                "Pure Dogecoin P2P — locations are approximate IP geolocation."));
    sub->setObjectName(QStringLiteral("mutedLabel"));
    sub->setWordWrap(true);
    root->addWidget(sub);

    // Stats row
    QHBoxLayout* stats = new QHBoxLayout();
    auto makeStat = [](const QString& caption, QLabel** out) {
        QVBoxLayout* v = new QVBoxLayout();
        QLabel* c = new QLabel(caption);
        c->setObjectName(QStringLiteral("mutedLabel"));
        *out = new QLabel(QStringLiteral("—"));
        QFont f = (*out)->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 1);
        (*out)->setFont(f);
        v->addWidget(c);
        v->addWidget(*out);
        return v;
    };
    stats->addLayout(makeStat(tr("Connections"), &connectionsLabel));
    stats->addLayout(makeStat(tr("Blocks"), &blocksLabel));
    stats->addLayout(makeStat(tr("Headers"), &headersLabel));
    stats->addLayout(makeStat(tr("Sync"), &ibdLabel));
    stats->addLayout(makeStat(tr("Traffic"), &trafficLabel));
    stats->addStretch();
    root->addLayout(stats);

    ibdDetailLabel = new QLabel();
    ibdDetailLabel->setObjectName(QStringLiteral("mutedLabel"));
    ibdDetailLabel->setWordWrap(true);
    ibdDetailLabel->setToolTip(tr("IBD peer-delivery telemetry (see getibdinfo RPC). "
                                  "stall = peers disconnected for blocking download; "
                                  "rescue = blocks fetched via non-preferred peers; "
                                  "dl-to = per-block download timeouts; "
                                  "flush = chainstate flushes."));
    root->addWidget(ibdDetailLabel);

    warningsLabel = new QLabel();
    warningsLabel->setObjectName(QStringLiteral("mutedLabel"));
    warningsLabel->setWordWrap(true);
    root->addWidget(warningsLabel);

    // Peer map is created on first show (ensurePeerMap) so wallet open does not
    // construct QNetworkAccessManager / geo timers during startup.
    mapHostLayout = new QVBoxLayout();
    mapHostLayout->setContentsMargins(0, 0, 0, 0);
    root->addLayout(mapHostLayout, 1);
    peerMap = 0;

    // Tables kept hidden for optional future use; disconnect/ban via Debug Console
    peerView = 0;
    banView = 0;

    connect(consoleBtn, SIGNAL(clicked()), this, SLOT(onOpenConsole()));
    connect(networkActiveCheck, SIGNAL(toggled(bool)), this, SLOT(onNetworkActiveToggled(bool)));

    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateStats()));
    // Only start when the page is shown (see showEvent). Eager start left the
    // timer running into ClientModel teardown → Windows heap corruption on exit.
    pollTimer->setInterval(2000);
}

void NetworkPage::setupContextMenus()
{
    // Peer table UI removed — map only. Disconnect/ban via Debug Console.
    peersContextMenu = 0;
    banContextMenu = 0;
}

void NetworkPage::setClientModel(ClientModel* model)
{
    clientModel = model;
    // If map already exists (user visited Network before), re-bind.
    if (peerMap)
        peerMap->setClientModel(model);
    if (!model) {
        if (pollTimer)
            pollTimer->stop();
    } else if (isVisible() && pollTimer && !pollTimer->isActive()) {
        pollTimer->start();
    }
    // Do not start PeerTableModel auto-refresh here — only when map is live.
    updateStats();
}

void NetworkPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    ensurePeerMap();
    if (pollTimer && !pollTimer->isActive())
        pollTimer->start();
    updateStats();
}

void NetworkPage::ensurePeerMap()
{
    if (peerMap || !mapHostLayout)
        return;
    peerMap = new PeerMapWidget(this);
    peerMap->setMinimumHeight(320);
    mapHostLayout->addWidget(peerMap, 1);
    if (clientModel)
        peerMap->setClientModel(clientModel);
}

void NetworkPage::refresh()
{
    updateStats();
    if (peerMap)
        peerMap->refreshFromPeers();
}

void NetworkPage::updateStats()
{
    if (!clientModel) {
        connectionsLabel->setText(QStringLiteral("—"));
        blocksLabel->setText(QStringLiteral("—"));
        headersLabel->setText(QStringLiteral("—"));
        ibdLabel->setText(tr("No client model"));
        if (ibdDetailLabel)
            ibdDetailLabel->clear();
        trafficLabel->setText(QStringLiteral("—"));
        warningsLabel->clear();
        return;
    }

    const int in = clientModel->getNumConnections(CONNECTIONS_IN);
    const int out = clientModel->getNumConnections(CONNECTIONS_OUT);
    connectionsLabel->setText(tr("%1 total (%2 in / %3 out)").arg(in + out).arg(in).arg(out));
    blocksLabel->setText(QString::number(clientModel->getNumBlocks()));
    headersLabel->setText(QString::number(clientModel->getHeaderTipHeight()));

    if (clientModel->inInitialBlockDownload()) {
        const double prog = clientModel->getVerificationProgress(0) * 100.0;
        ibdLabel->setText(tr("Syncing (~%1%)").arg(prog, 0, 'f', 1));
        if (ibdDetailLabel)
            ibdDetailLabel->setText(tr("IBD telemetry: %1 — raise -dbcache if flushes dominate; see getibdinfo")
                                        .arg(clientModel->getIbdStatsSummary()));
    } else {
        ibdLabel->setText(tr("Synced"));
        if (ibdDetailLabel) {
            // Still show lifetime counters — useful after a long sync.
            ibdDetailLabel->setText(tr("Session delivery: %1").arg(clientModel->getIbdStatsSummary()));
        }
    }

    const quint64 recv = clientModel->getTotalBytesRecv();
    const quint64 sent = clientModel->getTotalBytesSent();
    trafficLabel->setText(tr("↓ %1  ↑ %2")
                              .arg(GUIUtil::formatBytes(recv))
                              .arg(GUIUtil::formatBytes(sent)));

    const QString warn = clientModel->getStatusBarWarnings();
    warningsLabel->setText(warn.isEmpty() ? tr("No network warnings.") : warn);

    updatingNetworkToggle = true;
    networkActiveCheck->setChecked(clientModel->getNetworkActive());
    updatingNetworkToggle = false;

}

void NetworkPage::onOpenConsole()
{
    Q_EMIT openDebugConsole();
}

void NetworkPage::onNetworkActiveToggled(bool checked)
{
    if (updatingNetworkToggle || !clientModel)
        return;
    clientModel->setNetworkActive(checked);
    updateStats();
}

void NetworkPage::showPeersContextMenu(const QPoint& point)
{
    Q_UNUSED(point);
}

void NetworkPage::showBanContextMenu(const QPoint& point)
{
    Q_UNUSED(point);
}

void NetworkPage::disconnectSelectedPeer()
{
}

void NetworkPage::banSelectedPeer(int bantime)
{
    Q_UNUSED(bantime);
}

void NetworkPage::unbanSelectedPeer()
{
}
