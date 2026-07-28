// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_NETWORKPAGE_H
#define DOGECOIN_QT_NETWORKPAGE_H

#include <QWidget>

class ClientModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QLabel;
class QTableView;
class QPushButton;
class QTimer;
QT_END_NAMESPACE

/**
 * Network status page for Core Pro shell: connections, sync, peer table.
 * Uses the same PeerTableModel as the debug console.
 */
class NetworkPage : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkPage(const PlatformStyle* platformStyle, QWidget* parent = 0);

    void setClientModel(ClientModel* clientModel);
    void refresh();

Q_SIGNALS:
    void openDebugConsole();

private Q_SLOTS:
    void updateStats();
    void onOpenConsole();

private:
    void setupUi();

    const PlatformStyle* platformStyle;
    ClientModel* clientModel;

    QLabel* connectionsLabel;
    QLabel* blocksLabel;
    QLabel* headersLabel;
    QLabel* ibdLabel;
    QLabel* trafficLabel;
    QLabel* warningsLabel;
    QTableView* peerView;
    QPushButton* consoleBtn;
    QTimer* pollTimer;
};

#endif // DOGECOIN_QT_NETWORKPAGE_H
