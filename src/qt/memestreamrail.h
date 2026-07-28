// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_MEMESTREAMRAIL_H
#define DOGECOIN_QT_MEMESTREAMRAIL_H

#include "memestreamclient.h"

#include <QWidget>
#include <QList>

class WalletModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QTimer;
QT_END_NAMESPACE

/**
 * Compact Meme Stream column for the Home page (right rail).
 * Shows a few live cards + Tip; "Open full feed" jumps to MemeStreamPage.
 */
class MemeStreamRail : public QWidget
{
    Q_OBJECT

public:
    explicit MemeStreamRail(const PlatformStyle* platformStyle, QWidget* parent = 0);

    void setWalletModel(WalletModel* walletModel);
    /** Fetch feed (call when Home is shown). */
    void refresh();

Q_SIGNALS:
    void tipRequested(const QString& address);
    void openFullPage();

public Q_SLOTS:
    void onFeedReceived(const QList<MemeStreamItem>& items);
    void onFeedFailed(const QString& error);
    void onRefreshClicked();
    void onOpenFullClicked();

private:
    void setupUi();
    void rebuild(const QList<MemeStreamItem>& items);
    QWidget* buildCard(const MemeStreamItem& item);
    QString currentAuthorAddress() const;

    const PlatformStyle* platformStyle;
    WalletModel* walletModel;
    MemeStreamClient* client;

    QLabel* headerStatus;
    QScrollArea* scroll;
    QWidget* container;
    QVBoxLayout* feedLayout;
    QPushButton* refreshBtn;
    QPushButton* openFullBtn;
    QTimer* autoRefresh;
    QList<MemeStreamItem> lastItems;
};

#endif // DOGECOIN_QT_MEMESTREAMRAIL_H
