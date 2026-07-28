// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "networkpage.h"

#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "peertablemodel.h"
#include "platformstyle.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

NetworkPage::NetworkPage(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      clientModel(0),
      pollTimer(0)
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
    consoleBtn = new QPushButton(tr("Open Debug Console"));
    consoleBtn->setObjectName(QStringLiteral("homeQuickButton"));
    consoleBtn->setToolTip(tr("Peers detail, bans, console, and traffic graph"));
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

    QLabel* peersTitle = new QLabel(tr("Connected peers"));
    QFont pf = peersTitle->font();
    pf.setBold(true);
    peersTitle->setFont(pf);
    root->addWidget(peersTitle);

    peerView = new QTableView(this);
    peerView->setObjectName(QStringLiteral("networkPeerView"));
    peerView->verticalHeader()->hide();
    peerView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    peerView->setSelectionBehavior(QAbstractItemView::SelectRows);
    peerView->setSelectionMode(QAbstractItemView::SingleSelection);
    peerView->setAlternatingRowColors(true);
    peerView->horizontalHeader()->setStretchLastSection(true);
    peerView->setSortingEnabled(true);
    root->addWidget(peerView, 1);

    connect(consoleBtn, SIGNAL(clicked()), this, SLOT(onOpenConsole()));

    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateStats()));
    pollTimer->start(2000);
}

void NetworkPage::setClientModel(ClientModel* model)
{
    clientModel = model;
    if (clientModel && clientModel->getPeerTableModel()) {
        peerView->setModel(clientModel->getPeerTableModel());
        peerView->setColumnWidth(PeerTableModel::Address, 200);
        peerView->setColumnWidth(PeerTableModel::Subversion, 150);
        peerView->setColumnWidth(PeerTableModel::Ping, 80);
        clientModel->getPeerTableModel()->startAutoRefresh();
    } else {
        peerView->setModel(0);
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
}

void NetworkPage::onOpenConsole()
{
    Q_EMIT openDebugConsole();
}
