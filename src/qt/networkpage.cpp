// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "networkpage.h"

#include "bantablemodel.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "peertablemodel.h"
#include "platformstyle.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

NetworkPage::NetworkPage(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      clientModel(0),
      pollTimer(0),
      updatingNetworkToggle(false)
{
    setupUi();
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
    peerView->setSelectionMode(QAbstractItemView::SingleSelection);
    peerView->setAlternatingRowColors(true);
    peerView->horizontalHeader()->setStretchLastSection(true);
    peerView->setSortingEnabled(true);
    peersLay->addWidget(peerView, 1);
    split->addWidget(peersBox);

    // Bans
    QWidget* banBox = new QWidget();
    QVBoxLayout* banLay = new QVBoxLayout(banBox);
    banLay->setContentsMargins(0, 0, 0, 0);
    QLabel* banTitle = new QLabel(tr("Banned peers"));
    QFont bf = banTitle->font();
    bf.setBold(true);
    banTitle->setFont(bf);
    banLay->addWidget(banTitle);
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
            // BanTableModel has refresh() only (no auto timer like peers)
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
