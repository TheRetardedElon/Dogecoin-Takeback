// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "modernmainwindow.h"
#include "clientmodel.h"
#include "walletmodel.h"

#include <QApplication>
#include <QSplitter>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QWidget>
#include <QDebug>

ModernMainWindow::ModernMainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_clientModel(nullptr)
    , m_walletModel(nullptr)
    , m_themeManager(ThemeManager::instance())
    , m_navigationCollapsed(false)
{
    setupUI();
    applyTheme();
    
    // Connect theme changes
    connect(m_themeManager, &ThemeManager::colorsChanged, this, &ModernMainWindow::applyTheme);
}

ModernMainWindow::~ModernMainWindow()
{
}

void ModernMainWindow::setClientModel(ClientModel* clientModel)
{
    m_clientModel = clientModel;
    if (m_overviewPage) {
        m_overviewPage->setClientModel(clientModel);
    }
}

void ModernMainWindow::setWalletModel(WalletModel* walletModel)
{
    m_walletModel = walletModel;
    if (m_overviewPage) {
        m_overviewPage->setWalletModel(walletModel);
    }
}

void ModernMainWindow::setCurrentPage(ModernNavigation::NavItem item)
{
    if (m_navigation) {
        m_navigation->setCurrentItem(item);
    }
    
    if (m_contentStack) {
        int index = static_cast<int>(item);
        m_contentStack->setCurrentIndex(index);
    }
}

void ModernMainWindow::setupUI()
{
    // Set window properties with proper flags for window controls
    setWindowTitle("Dogecoin Core - Modern UI");
    setMinimumSize(1000, 700);
    resize(1200, 800);
    
    // Ensure proper window flags for dragging and controls
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | 
                   Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint | Qt::WindowContextHelpButtonHint);
    
    // Enable window dragging and proper event handling
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_NoMouseReplay, false);
    setAttribute(Qt::WA_MouseTracking, true);
    
    // Create main splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    setCentralWidget(m_mainSplitter);
    
    setupNavigation();
    setupContent();
    setupStatusBar();
    
    // Set initial layout
    updateLayout();
}

void ModernMainWindow::setupNavigation()
{
    m_navigation = new ModernNavigation();
    m_navigation->setFixedWidth(200);
    
    // Connect navigation signals
    connect(m_navigation, &ModernNavigation::itemClicked, 
            this, &ModernMainWindow::onNavigationItemClicked);
    connect(m_navigation, &ModernNavigation::toggleCompact, 
            this, &ModernMainWindow::onNavigationToggle);
    
    m_mainSplitter->addWidget(m_navigation);
}

void ModernMainWindow::setupContent()
{
    // Create content stack
    m_contentStack = new QStackedWidget();
    m_mainSplitter->addWidget(m_contentStack);
    
    // Create pages
    m_overviewPage = new ModernOverviewPage();
    m_sendPage = new QWidget();
    m_receivePage = new QWidget();
    m_historyPage = new QWidget();
    m_addressBookPage = new QWidget();
    m_consolePage = new QWidget();
    m_settingsPage = new QWidget();
    
    // Add pages to stack
    m_contentStack->addWidget(m_overviewPage);
    m_contentStack->addWidget(m_sendPage);
    m_contentStack->addWidget(m_receivePage);
    m_contentStack->addWidget(m_historyPage);
    m_contentStack->addWidget(m_addressBookPage);
    m_contentStack->addWidget(m_consolePage);
    m_contentStack->addWidget(m_settingsPage);
    
    // Setup page content
    setupPageContent(m_sendPage, "Send Coins", "📤", "Send DOGE to other addresses");
    setupPageContent(m_receivePage, "Receive Coins", "📥", "Generate addresses to receive DOGE");
    setupPageContent(m_historyPage, "Transaction History", "📋", "View your transaction history");
    setupPageContent(m_addressBookPage, "Address Book", "👥", "Manage your saved addresses");
    setupPageContent(m_consolePage, "Debug Console", "💻", "Advanced debugging and RPC console");
    setupPageContent(m_settingsPage, "Settings", "⚙️", "Configure Dogecoin Core settings");
    
    // Connect overview page signals
    connect(m_overviewPage, &ModernOverviewPage::sendCoins, this, &ModernMainWindow::sendCoins);
    connect(m_overviewPage, &ModernOverviewPage::receiveCoins, this, &ModernMainWindow::receiveCoins);
    connect(m_overviewPage, &ModernOverviewPage::showHistory, this, &ModernMainWindow::showHistory);
}

void ModernMainWindow::setupPageContent(QWidget* page, const QString& title, const QString& icon, const QString& description)
{
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(20);
    
    // Title
    QLabel* titleLabel = new QLabel(icon + " " + title);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: 600; color: #007bff;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    // Description
    QLabel* descLabel = new QLabel(description);
    descLabel->setStyleSheet("font-size: 16px; color: #6c757d;");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);
    
    // Placeholder content
    QLabel* placeholderLabel = new QLabel("This page is under construction.\nModern UI components will be implemented here.");
    placeholderLabel->setStyleSheet("font-size: 14px; color: #adb5bd;");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setWordWrap(true);
    layout->addWidget(placeholderLabel);
    
    layout->addStretch();
}

void ModernMainWindow::setupStatusBar()
{
    // Create status bar
    QStatusBar* statusBar = this->statusBar();
    
    // Connection status
    QLabel* connectionLabel = new QLabel("Connections: 0");
    connectionLabel->setObjectName("connectionLabel");
    statusBar->addWidget(connectionLabel);
    
    // Block count
    QLabel* blockLabel = new QLabel("Blocks: 0");
    blockLabel->setObjectName("blockLabel");
    statusBar->addWidget(blockLabel);
    
    // Progress bar
    QProgressBar* progressBar = new QProgressBar();
    progressBar->setObjectName("progressBar");
    progressBar->setVisible(false);
    statusBar->addPermanentWidget(progressBar);
    
    // Status message
    QLabel* statusLabel = new QLabel("Synchronizing...");
    statusLabel->setObjectName("statusLabel");
    statusBar->addPermanentWidget(statusLabel);
}

void ModernMainWindow::onNavigationItemClicked(ModernNavigation::NavItem item)
{
    setCurrentPage(item);
    
    // Emit specific signals for certain items
    switch (item) {
        case ModernNavigation::Send:
            Q_EMIT sendCoins();
            break;
        case ModernNavigation::Receive:
            Q_EMIT receiveCoins();
            break;
        case ModernNavigation::History:
            Q_EMIT showHistory();
            break;
        case ModernNavigation::AddressBook:
            Q_EMIT showHistory(); // Placeholder for address book
            break;
        case ModernNavigation::Console:
            Q_EMIT showHistory(); // Placeholder for console
            break;
        // Settings case removed - not in enum
        default:
            break;
    }
}

void ModernMainWindow::onNavigationToggle()
{
    m_navigationCollapsed = !m_navigationCollapsed;
    updateLayout();
}

void ModernMainWindow::onThemeChanged()
{
    applyTheme();
}

void ModernMainWindow::applyTheme()
{
    // Apply theme to the main window
    m_themeManager->applyTheme(this);
    
    // Apply theme to all pages
    if (m_overviewPage) {
        m_themeManager->applyTheme(m_overviewPage);
    }
    
    // Update status bar
    QStatusBar* statusBar = this->statusBar();
    if (statusBar) {
        statusBar->setStyleSheet(
            "QStatusBar {"
            "    background-color: " + m_themeManager->color("secondaryBackground").name() + ";"
            "    border-top: 1px solid " + m_themeManager->color("primaryBorder").name() + ";"
            "    color: " + m_themeManager->color("primaryText").name() + ";"
            "}"
        );
    }
}

void ModernMainWindow::updateLayout()
{
    if (m_navigationCollapsed) {
        m_navigation->setFixedWidth(60);
        m_navigation->setCompactMode(true);
    } else {
        m_navigation->setFixedWidth(200);
        m_navigation->setCompactMode(false);
    }
}

void ModernMainWindow::onSendCoins()
{
    Q_EMIT sendCoins();
}

void ModernMainWindow::onReceiveCoins()
{
    Q_EMIT receiveCoins();
}

void ModernMainWindow::onShowHistory()
{
    Q_EMIT showHistory();
}
