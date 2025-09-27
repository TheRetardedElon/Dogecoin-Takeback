// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "modernoverviewpage.h"
#include "thememanager.h"
#include "dogecoinunits.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "guiconstants.h"

#include <QApplication>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QTimer>
#include <QIcon>
#include <QFont>
#include <QDebug>

ModernOverviewPage::ModernOverviewPage(QWidget* parent)
    : QWidget(parent)
    , m_walletModel(nullptr)
    , m_clientModel(nullptr)
    , m_displayUnit(DogecoinUnits::BTC)
    , m_updateTimer(new QTimer(this))
{
    setupUI();
    
    // Setup update timer
    connect(m_updateTimer, &QTimer::timeout, this, &ModernOverviewPage::updateDisplayUnit);
    m_updateTimer->start(MODEL_UPDATE_DELAY);
    
    // Connect theme changes
    connect(ThemeManager::instance(), &ThemeManager::colorsChanged, this, &ModernOverviewPage::applyTheme);
}

ModernOverviewPage::~ModernOverviewPage()
{
}

void ModernOverviewPage::setWalletModel(WalletModel* walletModel)
{
    m_walletModel = walletModel;
    if (m_walletModel) {
        connect(m_walletModel, &WalletModel::balanceChanged, this, &ModernOverviewPage::updateBalances);
        // connect(m_walletModel, &WalletModel::transactionChanged, this, &ModernOverviewPage::updateTransactionList);
        updateBalances();
        updateTransactionList();
    }
}

void ModernOverviewPage::setClientModel(ClientModel* clientModel)
{
    m_clientModel = clientModel;
    if (m_clientModel) {
        connect(m_clientModel, &ClientModel::numConnectionsChanged, this, &ModernOverviewPage::updateNetworkStatus);
        connect(m_clientModel, &ClientModel::numBlocksChanged, this, &ModernOverviewPage::updateNetworkStatus);
        updateNetworkStatus();
    }
}

void ModernOverviewPage::setupUI()
{
    // Set modern sleek styling
    setStyleSheet(R"(
        ModernOverviewPage {
            background-color: #1a1a1a;
            color: #ffffff;
        }
        QLabel {
            color: #ffffff;
        }
        QPushButton {
            background-color: #3d3d3d;
            border: 1px solid #555555;
            border-radius: 8px;
            padding: 12px 20px;
            color: #ffffff;
            font-weight: 500;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #4d4d4d;
            border-color: #666666;
        }
        QPushButton:pressed {
            background-color: #2d2d2d;
        }
    )");
    
    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(30, 30, 30, 30);
    m_mainLayout->setSpacing(16);
    
    // Scroll area for content
    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_scrollContent = new QWidget();
    QVBoxLayout* scrollLayout = new QVBoxLayout(m_scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(16);
    
    // Setup cards
    setupBalanceCards();
    setupTransactionCard();
    setupNetworkCard();
    setupQuickActions();
    
    // Add content to scroll layout
    scrollLayout->addWidget(m_balanceCard);
    scrollLayout->addWidget(m_pendingCard);
    scrollLayout->addWidget(m_immatureCard);
    scrollLayout->addWidget(m_watchOnlyCard);
    scrollLayout->addWidget(m_transactionCard);
    scrollLayout->addWidget(m_networkCard);
    scrollLayout->addLayout(m_actionsLayout);
    scrollLayout->addStretch();
    
    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea);
    
    applyTheme();
}

void ModernOverviewPage::setupBalanceCards()
{
    // Main balance card
    m_balanceCard = new ModernCard(this, ModernCard::Primary);
    m_balanceCard->setTitle(tr("Available Balance"));
    m_balanceCard->setIcon(QIcon(":/icons/wallet"));
    
    QVBoxLayout* balanceLayout = new QVBoxLayout();
    m_balanceLabel = new QLabel("0.00000000 DOGE");
    m_balanceLabel->setStyleSheet("font-size: 24px; font-weight: 600; color: #007bff;");
    m_balanceLabel->setAlignment(Qt::AlignCenter);
    balanceLayout->addWidget(m_balanceLabel);
    
    QLabel* balanceDesc = new QLabel(tr("Your current spendable balance"));
    balanceDesc->setStyleSheet("font-size: 12px; color: #6c757d;");
    balanceDesc->setAlignment(Qt::AlignCenter);
    balanceLayout->addWidget(balanceDesc);
    
    m_balanceCard->setContent(new QWidget());
    static_cast<QVBoxLayout*>(m_balanceCard->layout())->addLayout(balanceLayout);
    
    // Pending balance card
    m_pendingCard = new ModernCard(this, ModernCard::Warning);
    m_pendingCard->setTitle(tr("Pending"));
    m_pendingCard->setIcon(QIcon(":/icons/clock"));
    
    QVBoxLayout* pendingLayout = new QVBoxLayout();
    m_pendingLabel = new QLabel("0.00000000 DOGE");
    m_pendingLabel->setStyleSheet("font-size: 18px; font-weight: 600;");
    m_pendingLabel->setAlignment(Qt::AlignCenter);
    pendingLayout->addWidget(m_pendingLabel);
    
    QLabel* pendingDesc = new QLabel(tr("Unconfirmed transactions"));
    pendingDesc->setStyleSheet("font-size: 12px; color: #6c757d;");
    pendingDesc->setAlignment(Qt::AlignCenter);
    pendingLayout->addWidget(pendingDesc);
    
    m_pendingCard->setContent(new QWidget());
    static_cast<QVBoxLayout*>(m_pendingCard->layout())->addLayout(pendingLayout);
    
    // Immature balance card
    m_immatureCard = new ModernCard(this, ModernCard::Info);
    m_immatureCard->setTitle(tr("Immature"));
    m_immatureCard->setIcon(QIcon(":/icons/mining"));
    
    QVBoxLayout* immatureLayout = new QVBoxLayout();
    m_immatureLabel = new QLabel("0.00000000 DOGE");
    m_immatureLabel->setStyleSheet("font-size: 18px; font-weight: 600;");
    m_immatureLabel->setAlignment(Qt::AlignCenter);
    immatureLayout->addWidget(m_immatureLabel);
    
    QLabel* immatureDesc = new QLabel(tr("Mined balance not yet matured"));
    immatureDesc->setStyleSheet("font-size: 12px; color: #6c757d;");
    immatureDesc->setAlignment(Qt::AlignCenter);
    immatureLayout->addWidget(immatureDesc);
    
    m_immatureCard->setContent(new QWidget());
    static_cast<QVBoxLayout*>(m_immatureCard->layout())->addLayout(immatureLayout);
    
    // Watch-only balance card
    m_watchOnlyCard = new ModernCard(this, ModernCard::Default);
    m_watchOnlyCard->setTitle(tr("Watch-only"));
    m_watchOnlyCard->setIcon(QIcon(":/icons/eye"));
    
    QVBoxLayout* watchOnlyLayout = new QVBoxLayout();
    m_watchOnlyLabel = new QLabel("0.00000000 DOGE");
    m_watchOnlyLabel->setStyleSheet("font-size: 18px; font-weight: 600;");
    m_watchOnlyLabel->setAlignment(Qt::AlignCenter);
    watchOnlyLayout->addWidget(m_watchOnlyLabel);
    
    QLabel* watchOnlyDesc = new QLabel(tr("Watch-only addresses"));
    watchOnlyDesc->setStyleSheet("font-size: 12px; color: #6c757d;");
    watchOnlyDesc->setAlignment(Qt::AlignCenter);
    watchOnlyLayout->addWidget(watchOnlyDesc);
    
    m_watchOnlyCard->setContent(new QWidget());
    static_cast<QVBoxLayout*>(m_watchOnlyCard->layout())->addLayout(watchOnlyLayout);
}

void ModernOverviewPage::setupTransactionCard()
{
    m_transactionCard = new ModernCard(this, ModernCard::Default);
    m_transactionCard->setTitle(tr("Recent Transactions"));
    m_transactionCard->setIcon(QIcon(":/icons/transaction"));
    
    QVBoxLayout* transactionLayout = new QVBoxLayout();
    m_transactionList = new QListWidget();
    m_transactionList->addItem(tr("No recent transactions"));
    transactionLayout->addWidget(m_transactionList);
    
    m_transactionCard->setContent(new QWidget());
    static_cast<QVBoxLayout*>(m_transactionCard->layout())->addLayout(transactionLayout);
}

void ModernOverviewPage::setupNetworkCard()
{
    m_networkCard = new ModernCard(this, ModernCard::Info);
    m_networkCard->setTitle(tr("Network Status"));
    m_networkCard->setIcon(QIcon(":/icons/network"));
    
    QVBoxLayout* networkLayout = new QVBoxLayout();
    
    m_connectionsLabel = new QLabel(tr("Connections: 0"));
    m_connectionsLabel->setStyleSheet("font-size: 14px;");
    networkLayout->addWidget(m_connectionsLabel);
    
    m_blocksLabel = new QLabel(tr("Blocks: 0"));
    m_blocksLabel->setStyleSheet("font-size: 14px;");
    networkLayout->addWidget(m_blocksLabel);
    
    m_syncLabel = new QLabel(tr("Synchronizing..."));
    m_syncLabel->setStyleSheet("font-size: 14px; color: #ffc107;");
    networkLayout->addWidget(m_syncLabel);
    
    m_networkCard->setContent(new QWidget());
    static_cast<QVBoxLayout*>(m_networkCard->layout())->addLayout(networkLayout);
}

void ModernOverviewPage::setupQuickActions()
{
    m_actionsLayout = new QHBoxLayout();
    m_actionsLayout->setSpacing(12);
    
    // Send button
    m_sendButton = new QPushButton(tr("Send"));
    m_sendButton->setIcon(QIcon(":/icons/send"));
    m_sendButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #007bff;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 12px 24px;"
        "    font-weight: 600;"
        "    font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #0056b3;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #004085;"
        "}"
    );
    connect(m_sendButton, &QPushButton::clicked, this, &ModernOverviewPage::onSendClicked);
    m_actionsLayout->addWidget(m_sendButton);
    
    // Receive button
    m_receiveButton = new QPushButton(tr("Receive"));
    m_receiveButton->setIcon(QIcon(":/icons/receive"));
    m_receiveButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #28a745;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 12px 24px;"
        "    font-weight: 600;"
        "    font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1e7e34;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #155724;"
        "}"
    );
    connect(m_receiveButton, &QPushButton::clicked, this, &ModernOverviewPage::onReceiveClicked);
    m_actionsLayout->addWidget(m_receiveButton);
    
    // History button
    m_historyButton = new QPushButton(tr("History"));
    m_historyButton->setIcon(QIcon(":/icons/history"));
    m_historyButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #6c757d;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 12px 24px;"
        "    font-weight: 600;"
        "    font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #545b62;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #3d4043;"
        "}"
    );
    connect(m_historyButton, &QPushButton::clicked, this, &ModernOverviewPage::onHistoryClicked);
    m_actionsLayout->addWidget(m_historyButton);
    
    // Refresh button
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setIcon(QIcon(":/icons/refresh"));
    m_refreshButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #17a2b8;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 12px 24px;"
        "    font-weight: 600;"
        "    font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #138496;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0f6674;"
        "}"
    );
    connect(m_refreshButton, &QPushButton::clicked, this, &ModernOverviewPage::onRefreshClicked);
    m_actionsLayout->addWidget(m_refreshButton);
    
    m_actionsLayout->addStretch();
}

void ModernOverviewPage::updateDisplayUnit()
{
    // This method is called by the timer to update the display unit
    // Implementation would depend on the specific requirements
}

void ModernOverviewPage::updateBalances()
{
    if (!m_walletModel) return;
    
    // Update balance displays
    updateBalanceDisplay();
}

void ModernOverviewPage::updateTransactionList()
{
    if (!m_walletModel) return;
    
    // Update transaction list
    updateTransactionDisplay();
}

void ModernOverviewPage::updateNetworkStatus()
{
    if (!m_clientModel) return;
    
    // Update network status
    updateNetworkDisplay();
}

void ModernOverviewPage::onSendClicked()
{
    // Emit signal to open send dialog
    Q_EMIT sendCoins();
}

void ModernOverviewPage::onReceiveClicked()
{
    // Emit signal to open receive dialog
    Q_EMIT receiveCoins();
}

void ModernOverviewPage::onHistoryClicked()
{
    // Emit signal to open history
    Q_EMIT showHistory();
}

void ModernOverviewPage::onRefreshClicked()
{
    // Refresh all data
    updateBalances();
    updateTransactionList();
    updateNetworkStatus();
}

void ModernOverviewPage::updateBalanceDisplay()
{
    if (!m_walletModel) return;
    
    // Get balances from wallet model
    CAmount balance = m_walletModel->getBalance();
    CAmount unconfirmedBalance = m_walletModel->getUnconfirmedBalance();
    CAmount immatureBalance = m_walletModel->getImmatureBalance();
    CAmount watchOnlyBalance = m_walletModel->getWatchBalance();
    
    // Update labels
    m_balanceLabel->setText(DogecoinUnits::formatWithUnit(m_displayUnit, balance, false, DogecoinUnits::separatorAlways));
    m_pendingLabel->setText(DogecoinUnits::formatWithUnit(m_displayUnit, unconfirmedBalance, false, DogecoinUnits::separatorAlways));
    m_immatureLabel->setText(DogecoinUnits::formatWithUnit(m_displayUnit, immatureBalance, false, DogecoinUnits::separatorAlways));
    m_watchOnlyLabel->setText(DogecoinUnits::formatWithUnit(m_displayUnit, watchOnlyBalance, false, DogecoinUnits::separatorAlways));
}

void ModernOverviewPage::updateTransactionDisplay()
{
    if (!m_walletModel) return;
    
    // Get recent transactions and display them
    // This is a simplified implementation
    m_transactionList->clear();
    m_transactionList->addItem(tr("Recent transactions will be displayed here"));
}

void ModernOverviewPage::updateNetworkDisplay()
{
    if (!m_clientModel) return;
    
    // Update network status labels
    int connections = m_clientModel->getNumConnections();
    int blocks = m_clientModel->getNumBlocks();
    
    m_connectionsLabel->setText(tr("Connections: %1").arg(connections));
    m_blocksLabel->setText(tr("Blocks: %1").arg(blocks));
    
    // Update sync status
    if (m_clientModel->inInitialBlockDownload()) {
        m_syncLabel->setText(tr("Synchronizing..."));
        m_syncLabel->setStyleSheet("font-size: 14px; color: #ffc107;");
    } else {
        m_syncLabel->setText(tr("Synchronized"));
        m_syncLabel->setStyleSheet("font-size: 14px; color: #28a745;");
    }
}

void ModernOverviewPage::applyTheme()
{
    // Apply theme to all components
    ThemeManager::instance()->applyTheme(this);
    
    // Update card styles
    if (m_balanceCard) m_balanceCard->updateStyle();
    if (m_pendingCard) m_pendingCard->updateStyle();
    if (m_immatureCard) m_immatureCard->updateStyle();
    if (m_watchOnlyCard) m_watchOnlyCard->updateStyle();
    if (m_transactionCard) m_transactionCard->updateStyle();
    if (m_networkCard) m_networkCard->updateStyle();
}
