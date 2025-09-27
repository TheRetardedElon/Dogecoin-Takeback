// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MODERNMAINWINDOW_H
#define BITCOIN_QT_MODERNMAINWINDOW_H

#include <QMainWindow>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QActionGroup>

#include "modernnavigation.h"
#include "modernoverviewpage.h"
#include "thememanager.h"

class ClientModel;
class WalletModel;

class ModernMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ModernMainWindow(QWidget* parent = nullptr);
    ~ModernMainWindow();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);

Q_SIGNALS:
    void sendCoins();
    void receiveCoins();
    void showHistory();

private Q_SLOTS:
    void onNavigationItemClicked(ModernNavigation::NavItem item);
    void onSendCoins();
    void onReceiveCoins();
    void onShowHistory();

private:
    void setupUI();
    void setupNavigation();
    void setupContent();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupPageContent(QWidget* page, const QString& title, const QString& icon, const QString& description);
    void applyTheme();
    void setCurrentPage(ModernNavigation::NavItem item);
    void updateLayout();
    void onNavigationToggle();
    void onThemeChanged();

    // Main layout
    QSplitter* m_mainSplitter;
    QWidget* m_centralWidget;
    QHBoxLayout* m_mainLayout;
    
    // Navigation
    ModernNavigation* m_navigation;
    
    // Content area
    QStackedWidget* m_contentStack;
    ModernOverviewPage* m_overviewPage;
    QWidget* m_sendPage;
    QWidget* m_receivePage;
    QWidget* m_historyPage;
    QWidget* m_addressBookPage;
    QWidget* m_consolePage;
    QWidget* m_settingsPage;
    
    // Models
    ClientModel* m_clientModel;
    WalletModel* m_walletModel;
    
    // Theme management
    ThemeManager* m_themeManager;
    bool m_navigationCollapsed;
    
    // Actions
    QAction* m_sendAction;
    QAction* m_receiveAction;
    QAction* m_historyAction;
    QAction* m_addressBookAction;
    QAction* m_consoleAction;
    QAction* m_quitAction;
    QAction* m_aboutAction;
    QAction* m_optionsAction;
};

#endif // BITCOIN_QT_MODERNMAINWINDOW_H
