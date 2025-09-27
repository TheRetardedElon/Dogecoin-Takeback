// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MODERNOVERVIEWPAGE_H
#define BITCOIN_QT_MODERNOVERVIEWPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QScrollArea>
#include <QFrame>
#include <QProgressBar>
#include <QTextEdit>
#include <QListWidget>
#include <QListWidgetItem>

#include "moderncard.h"
#include "thememanager.h"

class ClientModel;
class WalletModel;
class DogecoinUnits;

class ModernOverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit ModernOverviewPage(QWidget* parent = nullptr);
    ~ModernOverviewPage();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);

Q_SIGNALS:
    void sendCoins();
    void receiveCoins();
    void showHistory();

private Q_SLOTS:
    void updateDisplayUnit();
    void onSendClicked();
    void onReceiveClicked();
    void onHistoryClicked();
    void updateTransactionList();
    void updateNetworkDisplay();

private:
    void setupUI();
    void setupBalanceCards();
    void setupTransactionCard();
    void setupNetworkCard();
    void setupQuickActions();
    void applyTheme();
    void updateBalanceDisplay();
    void updateTransactionDisplay();
    void updateNetworkStatus();

    // Main layout
    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QVBoxLayout* m_contentLayout;
    
    // Balance cards
    ModernCard* m_balanceCard;
    ModernCard* m_pendingCard;
    ModernCard* m_immatureCard;
    ModernCard* m_watchOnlyCard;
    
    // Transaction card
    ModernCard* m_transactionCard;
    QListWidget* m_transactionList;
    
    // Network card
    ModernCard* m_networkCard;
    QLabel* m_networkStatusLabel;
    QLabel* m_peerCountLabel;
    QLabel* m_blockHeightLabel;
    QProgressBar* m_syncProgress;
    
    // Quick actions
    QHBoxLayout* m_actionsLayout;
    QPushButton* m_sendButton;
    QPushButton* m_receiveButton;
    QPushButton* m_historyButton;
    
    // Models
    ClientModel* m_clientModel;
    WalletModel* m_walletModel;
    
    // Update timer
    QTimer* m_updateTimer;
    DogecoinUnits::Unit m_displayUnit;
};

#endif // BITCOIN_QT_MODERNOVERVIEWPAGE_H
