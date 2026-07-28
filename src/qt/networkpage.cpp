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
#include "peertablemodel.h"
#include "platformstyle.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalMapper>
#include <QSplitter>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

NetworkPage::NetworkPage(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      clientModel(0),
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

    QLabel* sub = new QLabel(tr("Live peer and sync status from this Core node. "
                                "Right-click a peer to disconnect or ban; right-click a ban to unban. "
                                "This is pure Dogecoin P2P — not an app-chain network."));
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

    warningsLabel = new QLabel();
    warningsLabel->setObjectName(QStringLiteral("mutedLabel"));
    warningsLabel->setWordWrap(true);
    root->addWidget(warningsLabel);

    QSplitter* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);

    // Peers
    QWidget* peersBox = new QWidget();
    QVBoxLayout* peersLay = new QVBoxLayout(peersBox);
    peersLay->setContentsMargins(0, 0, 0, 0);
    QLabel* peersTitle = new QLabel(tr("Connected peers"));
    QFont pf = peersTitle->font();
    pf.setBold(true);
    peersTitle->setFont(pf);
    peersLay->addWidget(peersTitle);
    peerView = new QTableView(peersBox);
    peerView->setObjectName(QStringLiteral("networkPeerView"));
    peerView->verticalHeader()->hide();
    peerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    peerView->setSelectionBehavior(QAbstractItemView::SelectRows);
    peerView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    peerView->setAlternatingRowColors(true);
    peerView->horizontalHeader()->setStretchLastSection(true);
    peerView->setSortingEnabled(true);
    peerView->setContextMenuPolicy(Qt::CustomContextMenu);
    peersLay->addWidget(peerView, 1);
    split->addWidget(peersBox);

    // Bans
    QWidget* banBox = new QWidget();
    QVBoxLayout* banLay = new QVBoxLayout(banBox);
    banLay->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout* banHead = new QHBoxLayout();
    QLabel* banTitle = new QLabel(tr("Banned peers"));
    QFont bf = banTitle->font();
    bf.setBold(true);
    banTitle->setFont(bf);
    banHead->addWidget(banTitle);
    banHead->addStretch();
    QLabel* banHint = new QLabel(tr("Right-click → Unban"));
    banHint->setObjectName(QStringLiteral("mutedLabel"));
    banHead->addWidget(banHint);
    banLay->addLayout(banHead);
    banView = new QTableView(banBox);
    banView->setObjectName(QStringLiteral("networkBanView"));
    banView->verticalHeader()->hide();
    banView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    banView->setSelectionBehavior(QAbstractItemView::SelectRows);
    banView->setSelectionMode(QAbstractItemView::SingleSelection);
    banView->setAlternatingRowColors(true);
    banView->horizontalHeader()->setStretchLastSection(true);
    banView->setSortingEnabled(true);
    banView->setMaximumHeight(160);
    banView->setContextMenuPolicy(Qt::CustomContextMenu);
    banLay->addWidget(banView);
    split->addWidget(banBox);

    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    root->addWidget(split, 1);

    connect(consoleBtn, SIGNAL(clicked()), this, SLOT(onOpenConsole()));
    connect(networkActiveCheck, SIGNAL(toggled(bool)), this, SLOT(onNetworkActiveToggled(bool)));

    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateStats()));
    pollTimer->start(2000);
}

void NetworkPage::setupContextMenus()
{
    // Peer: disconnect + ban durations (same as RPCConsole)
    QAction* disconnectAction = new QAction(tr("&Disconnect"), this);
    QAction* banAction1h = new QAction(tr("Ban for") + QStringLiteral(" ") + tr("1 &hour"), this);
    QAction* banAction24h = new QAction(tr("Ban for") + QStringLiteral(" ") + tr("1 &day"), this);
    QAction* banAction7d = new QAction(tr("Ban for") + QStringLiteral(" ") + tr("1 &week"), this);
    QAction* banAction365d = new QAction(tr("Ban for") + QStringLiteral(" ") + tr("1 &year"), this);

    peersContextMenu = new QMenu(this);
    peersContextMenu->addAction(disconnectAction);
    peersContextMenu->addSeparator();
    peersContextMenu->addAction(banAction1h);
    peersContextMenu->addAction(banAction24h);
    peersContextMenu->addAction(banAction7d);
    peersContextMenu->addAction(banAction365d);

    QSignalMapper* banMapper = new QSignalMapper(this);
    banMapper->setMapping(banAction1h, 60 * 60);
    banMapper->setMapping(banAction24h, 60 * 60 * 24);
    banMapper->setMapping(banAction7d, 60 * 60 * 24 * 7);
    banMapper->setMapping(banAction365d, 60 * 60 * 24 * 365);
    connect(banAction1h, SIGNAL(triggered()), banMapper, SLOT(map()));
    connect(banAction24h, SIGNAL(triggered()), banMapper, SLOT(map()));
    connect(banAction7d, SIGNAL(triggered()), banMapper, SLOT(map()));
    connect(banAction365d, SIGNAL(triggered()), banMapper, SLOT(map()));
    connect(banMapper, SIGNAL(mapped(int)), this, SLOT(banSelectedPeer(int)));
    connect(disconnectAction, SIGNAL(triggered()), this, SLOT(disconnectSelectedPeer()));
    connect(peerView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showPeersContextMenu(QPoint)));

    // Ban list: unban
    QAction* unbanAction = new QAction(tr("&Unban"), this);
    banContextMenu = new QMenu(this);
    banContextMenu->addAction(unbanAction);
    connect(unbanAction, SIGNAL(triggered()), this, SLOT(unbanSelectedPeer()));
    connect(banView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showBanContextMenu(QPoint)));
}

void NetworkPage::setClientModel(ClientModel* model)
{
    clientModel = model;
    if (clientModel) {
        if (clientModel->getPeerTableModel()) {
            peerView->setModel(clientModel->getPeerTableModel());
            peerView->setColumnWidth(PeerTableModel::Address, 200);
            peerView->setColumnWidth(PeerTableModel::Subversion, 150);
            peerView->setColumnWidth(PeerTableModel::Ping, 80);
            clientModel->getPeerTableModel()->startAutoRefresh();
        }
        if (clientModel->getBanTableModel()) {
            banView->setModel(clientModel->getBanTableModel());
            banView->setColumnWidth(BanTableModel::Address, 220);
            clientModel->getBanTableModel()->refresh();
        }
    } else {
        peerView->setModel(0);
        banView->setModel(0);
    }
    updateStats();
}

void NetworkPage::refresh()
{
    updateStats();
}

void NetworkPage::updateStats()
{
    if (!clientModel) {
        connectionsLabel->setText(QStringLiteral("—"));
        blocksLabel->setText(QStringLiteral("—"));
        headersLabel->setText(QStringLiteral("—"));
        ibdLabel->setText(tr("No client model"));
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
    } else {
        ibdLabel->setText(tr("Synced"));
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

    if (clientModel->getBanTableModel())
        clientModel->getBanTableModel()->refresh();
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
    if (!peerView->indexAt(point).isValid() || !peersContextMenu)
        return;
    peersContextMenu->exec(peerView->viewport()->mapToGlobal(point));
}

void NetworkPage::showBanContextMenu(const QPoint& point)
{
    if (!banView->indexAt(point).isValid() || !banContextMenu)
        return;
    banContextMenu->exec(banView->viewport()->mapToGlobal(point));
}

void NetworkPage::disconnectSelectedPeer()
{
    if (!g_connman)
        return;
    const QList<QModelIndex> nodes = GUIUtil::getEntryData(peerView, PeerTableModel::NetNodeId);
    for (int i = 0; i < nodes.count(); ++i) {
        const NodeId id = nodes.at(i).data().toLongLong();
        g_connman->DisconnectNode(id);
    }
    updateStats();
}

void NetworkPage::banSelectedPeer(int bantime)
{
    if (!clientModel || !g_connman)
        return;

    const QList<QModelIndex> nodes = GUIUtil::getEntryData(peerView, PeerTableModel::NetNodeId);
    PeerTableModel* peerModel = clientModel->getPeerTableModel();
    if (!peerModel)
        return;

    for (int i = 0; i < nodes.count(); ++i) {
        const NodeId id = nodes.at(i).data().toLongLong();
        const int row = peerModel->getRowByNodeId(id);
        if (row < 0)
            continue;
        const CNodeCombinedStats* stats = peerModel->getNodeStats(row);
        if (stats)
            g_connman->Ban(stats->nodeStats.addr, BanReasonManuallyAdded, bantime);
    }

    if (clientModel->getBanTableModel())
        clientModel->getBanTableModel()->refresh();
    updateStats();
}

void NetworkPage::unbanSelectedPeer()
{
    if (!clientModel || !g_connman)
        return;

    const QList<QModelIndex> nodes = GUIUtil::getEntryData(banView, BanTableModel::Address);
    for (int i = 0; i < nodes.count(); ++i) {
        const QString strNode = nodes.at(i).data().toString();
        CSubNet possibleSubnet;
        LookupSubNet(strNode.toStdString(), possibleSubnet);
        if (possibleSubnet.IsValid())
            g_connman->Unban(possibleSubnet);
    }

    if (clientModel->getBanTableModel())
        clientModel->getBanTableModel()->refresh();
    updateStats();
}
