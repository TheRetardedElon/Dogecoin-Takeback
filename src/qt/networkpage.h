// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_NETWORKPAGE_H
#define DOGECOIN_QT_NETWORKPAGE_H

#include <QWidget>

class ClientModel;
class PlatformStyle;
class PeerMapWidget;

QT_BEGIN_NAMESPACE
class QLabel;
class QTableView;
class QPushButton;
class QTimer;
class QCheckBox;
class QMenu;
class QPoint;
class QVBoxLayout;
class QShowEvent;
QT_END_NAMESPACE

/**
 * Network status page for Core Pro shell: live world map of peers, stats, tables, bans.
 * Peer map is exclusive to this page (not the debug console).
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
    void onNetworkActiveToggled(bool checked);
    void showPeersContextMenu(const QPoint& point);
    void showBanContextMenu(const QPoint& point);
    void disconnectSelectedPeer();
    void banSelectedPeer(int bantime);
    void unbanSelectedPeer();

protected:
    void showEvent(QShowEvent* event);

private:
    void setupUi();
    void setupContextMenus();
    /** Create PeerMap on first show — avoids QNetwork/geo init during wallet open. */
    void ensurePeerMap();

    const PlatformStyle* platformStyle;
    ClientModel* clientModel;

    QLabel* connectionsLabel;
    QLabel* blocksLabel;
    QLabel* headersLabel;
    QLabel* ibdLabel;
    QLabel* ibdDetailLabel;
    QLabel* trafficLabel;
    QLabel* warningsLabel;
    PeerMapWidget* peerMap;
    QVBoxLayout* mapHostLayout;
    QTableView* peerView;
    QTableView* banView;
    QPushButton* consoleBtn;
    QCheckBox* networkActiveCheck;
    QTimer* pollTimer;
    bool updatingNetworkToggle;

    QMenu* peersContextMenu;
    QMenu* banContextMenu;
};

#endif // DOGECOIN_QT_NETWORKPAGE_H
