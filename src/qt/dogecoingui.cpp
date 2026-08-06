// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021-2023 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/dogecoin-config.h"
#endif

#include "dogecoingui.h"

#include "dogecoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "modaloverlay.h"
#include "networkstyle.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "rpcconsole.h"
#include "utilitydialog.h"
#include "thememanager.h"
#include "modernoverviewpage.h"

#include <QStyle>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QApplication>
#include <QProgressBar>
#include <QStatusBar>
#include <QLabel>

#ifdef ENABLE_WALLET
#include "walletframe.h"
#include "walletmodel.h"
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "chainparams.h"
#include "init.h"
#include "ui_interface.h"
#include "util.h"

#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QFontDatabase>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <QUrlQuery>
#include <QGroupBox>
#include <QComboBox>
#include <QDialog>

const std::string DogecoinGUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif
        ;

#include <boost/bind/bind.hpp>

/** Display name for default wallet name. Uses tilde to avoid name
 * collisions in the future with additional wallets */
const QString DogecoinGUI::DEFAULT_WALLET = "~Default";

DogecoinGUI::DogecoinGUI(const PlatformStyle *_platformStyle, const NetworkStyle *networkStyle, QWidget *parent) :
    QMainWindow(parent),
    enableWallet(false),
    clientModel(0),
    walletFrame(0),
    useModernUI(true),
    m_navButtons(),
    m_currentTheme("dark"),
    m_themeIndex(0),
    unitDisplayControl(0),
    labelWalletEncryptionIcon(0),
    labelWalletHDStatusIcon(0),
    connectionsControl(0),
    labelBlocksIcon(0),
    progressBarLabel(0),
    progressBar(0),
    progressDialog(0),
    appMenuBar(0),
    overviewAction(0),
    historyAction(0),
    quitAction(0),
    sendCoinsAction(0),
    sendCoinsMenuAction(0),
    usedSendingAddressesAction(0),
    usedReceivingAddressesAction(0),
    importPrivateKeyAction(0),
    signMessageAction(0),
    verifyMessageAction(0),
    aboutAction(0),
    receiveCoinsAction(0),
    receiveCoinsMenuAction(0),
    optionsAction(0),
    toggleHideAction(0),
    encryptWalletAction(0),
    backupWalletAction(0),
    changePassphraseAction(0),
    aboutQtAction(0),
    openRPCConsoleAction(0),
    openAction(0),
    showHelpMessageAction(0),
    trayIcon(0),
    trayIconMenu(0),
    notificator(0),
    rpcConsole(0),
    helpMessageDialog(0),
    modalOverlay(0),
    prevBlocks(0),
    spinnerFrame(0),
    platformStyle(_platformStyle)
{
    GUIUtil::restoreWindowGeometry("nWindow", QSize(850, 550), this);

    QString windowTitle = tr(PACKAGE_NAME) + " - ";
#ifdef ENABLE_WALLET
    enableWallet = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    if(enableWallet)
    {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }
    windowTitle += " " + networkStyle->getTitleAddText();
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getTrayAndWindowIcon());
    setWindowIcon(networkStyle->getTrayAndWindowIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif
    setWindowTitle(windowTitle);

    // Temporarily disable borderless window to isolate signal issues
    // setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    // setAttribute(Qt::WA_TranslucentBackground);
    
    // Create custom title bar for borderless window

    rpcConsole = new RPCConsole(_platformStyle, 0);
    helpMessageDialog = new HelpMessageDialog(this, false);
    // Initialize modern UI with actual wallet functionality
    {
        // Create a modern container that will hold both navigation and wallet
        QWidget* modernContainer = new QWidget(this);
        modernContainer->setObjectName("modernContainer");
        QVBoxLayout* mainLayout = new QVBoxLayout(modernContainer);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        // Create horizontal layout for navigation and wallet
        QHBoxLayout* modernLayout = new QHBoxLayout();
        modernLayout->setContentsMargins(0, 0, 0, 0);
        modernLayout->setSpacing(0);
        
        // Create modern navigation sidebar
        QWidget* nav = new QWidget();
        nav->setFixedWidth(250);
        nav->setObjectName("modernNavigation");
        
        QVBoxLayout* navLayout = new QVBoxLayout(nav);
        QLabel* brand = new QLabel(tr("Dogecoin Core Pro"));
        QFont brandFont = brand->font();
        brandFont.setBold(true);
        brandFont.setPointSize(brandFont.pointSize() + 1);
        brand->setFont(brandFont);
        brand->setContentsMargins(8, 12, 8, 12);
        navLayout->addWidget(brand);

        QPushButton* overviewBtn = new QPushButton(tr("Home"));
        QPushButton* sendBtn = new QPushButton(tr("Send"));
        QPushButton* receiveBtn = new QPushButton(tr("Receive"));
        QPushButton* historyBtn = new QPushButton(tr("Transactions"));
        QPushButton* networkBtn = new QPushButton(tr("Network"));
        QPushButton* businessBtn = new QPushButton(tr("Doge Business"));
        QPushButton* memeBtn = new QPushButton(tr("Meme Stream"));
        QPushButton* arcadeBtn = new QPushButton(tr("Arcade"));
        QPushButton* consoleBtn = new QPushButton(tr("Console"));

        // Store buttons for later connection / theme styling
        m_navButtons.clear();
        m_navButtons["overview"] = overviewBtn;
        m_navButtons["send"] = sendBtn;
        m_navButtons["receive"] = receiveBtn;
        m_navButtons["history"] = historyBtn;
        m_navButtons["network"] = networkBtn;
        m_navButtons["business"] = businessBtn;
        m_navButtons["meme"] = memeBtn;
        m_navButtons["arcade"] = arcadeBtn;
        m_navButtons["console"] = consoleBtn;

        navLayout->addWidget(overviewBtn);
        navLayout->addWidget(sendBtn);
        navLayout->addWidget(receiveBtn);
        navLayout->addWidget(historyBtn);
        navLayout->addWidget(networkBtn);
        navLayout->addWidget(businessBtn);
        navLayout->addWidget(memeBtn);
        navLayout->addWidget(arcadeBtn);
        navLayout->addStretch();
        navLayout->addWidget(consoleBtn);

        // Connect navigation buttons to actual functionality with null checks
        connect(overviewBtn, &QPushButton::clicked, [this]() {
            if (walletFrame) {
                gotoOverviewPage();
            }
        });
        connect(sendBtn, &QPushButton::clicked, [this]() {
            if (walletFrame) {
                gotoSendCoinsPage();
            }
        });
        connect(receiveBtn, &QPushButton::clicked, [this]() {
            if (walletFrame) {
                gotoReceiveCoinsPage();
            }
        });
        connect(historyBtn, &QPushButton::clicked, [this]() {
            if (walletFrame) {
                gotoHistoryPage();
            }
        });
        connect(networkBtn, &QPushButton::clicked, [this]() {
            if (walletFrame)
                gotoNetworkPage();
            else
                showDebugWindow();
        });
        connect(businessBtn, &QPushButton::clicked, [this]() {
            if (walletFrame) {
                gotoDogeBusinessPage();
            }
        });
        connect(memeBtn, &QPushButton::clicked, [this]() {
            if (walletFrame) {
                gotoMemeStreamPage();
            }
        });
        connect(arcadeBtn, &QPushButton::clicked, [this]() {
            if (walletFrame) {
                gotoArcadePage();
            }
        });
        connect(consoleBtn, &QPushButton::clicked, [this]() {
            showDebugWindow();
        });
        
        // Create wallet frame for actual functionality
#ifdef ENABLE_WALLET
        if(enableWallet) {
            walletFrame = new WalletFrame(_platformStyle, this);
        } else
#endif
        {
            // When compiled without wallet or -disablewallet is provided
            // the central widget is a rpcconsole widget
            rpcConsole = new RPCConsole(_platformStyle, 0);
            setCentralWidget(rpcConsole);
        }
        
        // Add to layout - navigation on left, wallet on right
        modernLayout->addWidget(nav);
        if (walletFrame) {
            modernLayout->addWidget(walletFrame, 1);
        }
        
        // Add the horizontal layout to the main vertical layout
        mainLayout->addLayout(modernLayout);
        
        setCentralWidget(modernContainer);
        // Shell chrome themed via ThemeManager + applyTheme below.
    }
    
    // Create menu bar and actions for all themes
    createActions();
    createMenuBar();
    createStatusBar();

    // Tray + Notificator (required for non-modal MSG_INFORMATION toasts).
    // Modern-UI refactor previously skipped this; Doge Business invoice create
    // then crashed on notificator->notify(nullptr).
    createTrayIcon(networkStyle);
    
    // Keep the old interface as fallback (commented out for now)
    if (false) {
#ifdef ENABLE_WALLET
    if(enableWallet)
    {
        /** Create wallet frame and make it the central widget */
        walletFrame = new WalletFrame(_platformStyle, this);
        setCentralWidget(walletFrame);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
        }
    }

    // Dogecoin: load fallback font in case Comic Sans is not available on the system
    QFontDatabase::addApplicationFont(":fonts/ComicNeue-Bold");
    QFontDatabase::addApplicationFont(":fonts/ComicNeue-Bold-Oblique");
    QFontDatabase::addApplicationFont(":fonts/ComicNeue-Light");
    QFontDatabase::addApplicationFont(":fonts/ComicNeue-Light-Oblique");
    QFontDatabase::addApplicationFont(":fonts/ComicNeue-Regular");
    QFontDatabase::addApplicationFont(":fonts/ComicNeue-Regular-Oblique");
    QFont::insertSubstitution("Comic Sans MS", "Comic Neue");

    // Dogecoin: load this bundled font for Settings -> Options in case it's not available on the system
    QFontDatabase::addApplicationFont(":fonts/NotoSans-Bold");
    QFontDatabase::addApplicationFont(":fonts/NotoSans-Light");
    QFontDatabase::addApplicationFont(":fonts/NotoSans-Medium");
    QFontDatabase::addApplicationFont(":fonts/NotoSans-Regular");

    // Dogecoin: Specify Comic Sans as new font.
    QFont newFont("Comic Sans MS", 10);

    // Dogecoin: Set new application font
    QApplication::setFont(newFont);

    // Single theme path: ThemeManager owns app + Pro shell styles.
    // Clear any local container stylesheet so global rules cascade.
    ThemeManager* themeManager = ThemeManager::instance();
    connect(themeManager, &ThemeManager::colorsChanged, this, [this]() {
        QWidget* container = findChild<QWidget*>("modernContainer");
        if (container)
            container->setStyleSheet(QString());
        style()->unpolish(this);
        style()->polish(this);
        update();
    });
    // Default Pro chrome: Dark (matches previous shell default)
    themeManager->switchToDark();
    m_currentTheme = "Dark";

    // Accept D&D of URIs
    setAcceptDrops(true);
}

void DogecoinGUI::applyTheme(const QString& themeName)
{
    // Map legacy chrome names → ThemeManager / CSS themes
    ThemeManager* tm = ThemeManager::instance();
    const QString t = themeName.toLower();
    if (t == "light") {
        tm->switchToLight();
    } else if (t == "dark") {
        tm->switchToDark();
    } else if (t == "dogecoin") {
        tm->switchToTheme(ThemeManager::Dogecoin);
    } else if (t == "neon") {
        tm->switchToTheme(ThemeManager::Neon);
    } else if (t == "classic" || t == "basic") {
        tm->switchToTheme(ThemeManager::Classic);
    } else {
        // matrix, cyberpunk, retro, etc. — CSS packs under src/qt/themes/
        tm->loadCSSTheme(themeName);
    }

    // Ensure container does not override the global sheet
    QWidget* container = findChild<QWidget*>("modernContainer");
    if (container)
        container->setStyleSheet(QString());

    m_currentTheme = themeName;
}

void DogecoinGUI::cycleTheme()
{
    QString themes[] = {"dark", "light", "dogecoin", "neon", "matrix", "cyberpunk"};
    m_themeIndex = (m_themeIndex + 1) % 6;
    applyTheme(themes[m_themeIndex]);
}

void DogecoinGUI::applyGlobalTheme(const QString& themeName)
{
    if (themeName == "matrix") {
        QString matrixCSS = R"(
            /* Matrix Theme - Digital Green Terminal Aesthetic */
            QMainWindow {
                background-color: #000000;
                color: #00ff00;
                font-family: 'Courier New', monospace;
            }
            
            QWidget {
                background-color: #000000;
                color: #00ff00;
                border: 1px solid #00ff00;
            }
            
            QPushButton {
                background-color: #001100;
                color: #00ff00;
                border: 2px solid #00ff00;
                border-radius: 3px;
                padding: 8px 16px;
                font-family: 'Courier New', monospace;
                font-weight: bold;
            }
            
            QPushButton:hover {
                background-color: #002200;
                box-shadow: 0 0 10px #00ff00;
            }
            
            QPushButton:pressed {
                background-color: #003300;
            }
            
            QLabel {
                color: #00ff00;
                font-family: 'Courier New', monospace;
                background-color: transparent;
            }
            
            QGroupBox {
                color: #00ff00;
                border: 2px solid #00ff00;
                border-radius: 5px;
                margin-top: 10px;
                font-weight: bold;
                font-family: 'Courier New', monospace;
            }
            
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                color: #00ff00;
                background-color: #000000;
            }
            
            QComboBox {
                background-color: #001100;
                color: #00ff00;
                border: 2px solid #00ff00;
                border-radius: 3px;
                padding: 5px;
                font-family: 'Courier New', monospace;
            }
            
            QComboBox::drop-down {
                border: none;
                background-color: #001100;
            }
            
            QComboBox::down-arrow {
                image: none;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 5px solid #00ff00;
            }
            
            QComboBox QAbstractItemView {
                background-color: #000000;
                color: #00ff00;
                border: 2px solid #00ff00;
                selection-background-color: #002200;
            }
            
            QDialog {
                background-color: #000000;
                color: #00ff00;
            }
            
            QScrollBar:vertical {
                background-color: #001100;
                width: 15px;
                border: 1px solid #00ff00;
            }
            
            QScrollBar::handle:vertical {
                background-color: #00ff00;
                min-height: 20px;
            }
            
            QScrollBar::handle:vertical:hover {
                background-color: #00cc00;
            }
            
            /* Matrix digital rain effect for special elements */
            QFrame[objectName="matrixFrame"] {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                    stop:0 #000000, stop:0.5 #001100, stop:1 #000000);
                border: 2px solid #00ff00;
            }
            
            /* Terminal-style text areas */
            QTextEdit, QPlainTextEdit {
                background-color: #000000;
                color: #00ff00;
                border: 2px solid #00ff00;
                font-family: 'Courier New', monospace;
                selection-background-color: #002200;
            }
            
            /* Matrix-style progress bars */
            QProgressBar {
                background-color: #001100;
                border: 2px solid #00ff00;
                border-radius: 3px;
                text-align: center;
                color: #00ff00;
                font-family: 'Courier New', monospace;
            }
            
            QProgressBar::chunk {
                background-color: #00ff00;
            }
        )";
        
        qApp->setStyleSheet(matrixCSS);
    } else if (themeName == "cyberpunk") {
        QString cyberpunkCSS = R"(
            /* Cyberpunk Theme - Neon Purple/Pink Futuristic */
            QMainWindow {
                background-color: #0a0a0a;
                color: #ff00ff;
                font-family: 'Orbitron', 'Arial', sans-serif;
            }
            
            QWidget {
                background-color: #1a0a1a;
                color: #ff00ff;
                border: 1px solid #ff00ff;
            }
            
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff00ff, stop:1 #800080);
                color: #000000;
                border: 2px solid #ff00ff;
                border-radius: 8px;
                padding: 10px 20px;
                font-weight: bold;
                font-size: 12px;
            }
            
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff44ff, stop:1 #aa00aa);
                box-shadow: 0 0 15px #ff00ff;
            }
            
            QPushButton:pressed {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #cc00cc, stop:1 #660066);
            }
            
            QLabel {
                color: #ff00ff;
                font-weight: bold;
                background-color: transparent;
            }
            
            QGroupBox {
                color: #ff00ff;
                border: 2px solid #ff00ff;
                border-radius: 10px;
                margin-top: 10px;
                font-weight: bold;
                background-color: #1a0a1a;
            }
            
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                color: #ff00ff;
                background-color: #0a0a0a;
                border: 1px solid #ff00ff;
            }
            
            QComboBox {
                background-color: #1a0a1a;
                color: #ff00ff;
                border: 2px solid #ff00ff;
                border-radius: 8px;
                padding: 8px;
                font-weight: bold;
            }
            
            QComboBox::drop-down {
                border: none;
                background-color: #1a0a1a;
            }
            
            QComboBox::down-arrow {
                image: none;
                border-left: 6px solid transparent;
                border-right: 6px solid transparent;
                border-top: 6px solid #ff00ff;
            }
            
            QComboBox QAbstractItemView {
                background-color: #0a0a0a;
                color: #ff00ff;
                border: 2px solid #ff00ff;
                selection-background-color: #2a0a2a;
            }
            
            QDialog {
                background-color: #0a0a0a;
                color: #ff00ff;
                border: 2px solid #ff00ff;
            }
            
            QScrollBar:vertical {
                background-color: #1a0a1a;
                width: 15px;
                border: 1px solid #ff00ff;
            }
            
            QScrollBar::handle:vertical {
                background-color: #ff00ff;
                min-height: 20px;
            }
            
            QScrollBar::handle:vertical:hover {
                background-color: #ff44ff;
            }
        )";
        
        qApp->setStyleSheet(cyberpunkCSS);
    } else if (themeName == "neon") {
        QString neonCSS = R"(
            /* Neon Theme - Bright Electric Colors */
            QMainWindow {
                background-color: #000011;
                color: #00ffff;
                font-family: 'Arial', sans-serif;
            }
            
            QWidget {
                background-color: #001122;
                color: #00ffff;
                border: 1px solid #00ffff;
            }
            
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #00ffff, stop:1 #0088cc);
                color: #000000;
                border: 2px solid #00ffff;
                border-radius: 6px;
                padding: 10px 18px;
                font-weight: bold;
                font-size: 12px;
            }
            
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #44ffff, stop:1 #00aadd);
                box-shadow: 0 0 20px #00ffff;
            }
            
            QPushButton:pressed {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0088aa, stop:1 #006699);
            }
            
            QLabel {
                color: #00ffff;
                font-weight: bold;
                background-color: transparent;
            }
            
            QGroupBox {
                color: #00ffff;
                border: 2px solid #00ffff;
                border-radius: 8px;
                margin-top: 10px;
                font-weight: bold;
                background-color: #001133;
            }
            
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                color: #00ffff;
                background-color: #000011;
                border: 1px solid #00ffff;
            }
            
            QComboBox {
                background-color: #001122;
                color: #00ffff;
                border: 2px solid #00ffff;
                border-radius: 6px;
                padding: 8px;
                font-weight: bold;
            }
            
            QComboBox::drop-down {
                border: none;
                background-color: #001122;
            }
            
            QComboBox::down-arrow {
                image: none;
                border-left: 6px solid transparent;
                border-right: 6px solid transparent;
                border-top: 6px solid #00ffff;
            }
            
            QComboBox QAbstractItemView {
                background-color: #000011;
                color: #00ffff;
                border: 2px solid #00ffff;
                selection-background-color: #002244;
            }
            
            QDialog {
                background-color: #000011;
                color: #00ffff;
                border: 2px solid #00ffff;
            }
            
            QScrollBar:vertical {
                background-color: #001122;
                width: 15px;
                border: 1px solid #00ffff;
            }
            
            QScrollBar::handle:vertical {
                background-color: #00ffff;
                min-height: 20px;
            }
            
            QScrollBar::handle:vertical:hover {
                background-color: #44ffff;
            }
        )";
        
        qApp->setStyleSheet(neonCSS);
    } else if (themeName == "futuristic") {
        QString futuristicCSS = R"(
            /* Futuristic Theme - Space Age Metallic */
            QMainWindow {
                background-color: #0a0f1a;
                color: #00ff88;
                font-family: 'Segoe UI', 'Arial', sans-serif;
            }
            
            QWidget {
                background-color: #1a2a3a;
                color: #00ff88;
                border: 1px solid #00ff88;
            }
            
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #00ff88, stop:1 #008855);
                color: #000000;
                border: 2px solid #00ff88;
                border-radius: 12px;
                padding: 12px 24px;
                font-weight: bold;
                font-size: 13px;
            }
            
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #44ffaa, stop:1 #00aa66);
                box-shadow: 0 0 25px #00ff88;
            }
            
            QPushButton:pressed {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #00cc66, stop:1 #006644);
            }
            
            QLabel {
                color: #00ff88;
                font-weight: bold;
                background-color: transparent;
            }
            
            QGroupBox {
                color: #00ff88;
                border: 2px solid #00ff88;
                border-radius: 15px;
                margin-top: 10px;
                font-weight: bold;
                background-color: #1a2a3a;
            }
            
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                color: #00ff88;
                background-color: #0a0f1a;
                border: 1px solid #00ff88;
            }
            
            QComboBox {
                background-color: #1a2a3a;
                color: #00ff88;
                border: 2px solid #00ff88;
                border-radius: 12px;
                padding: 10px;
                font-weight: bold;
            }
            
            QComboBox::drop-down {
                border: none;
                background-color: #1a2a3a;
            }
            
            QComboBox::down-arrow {
                image: none;
                border-left: 7px solid transparent;
                border-right: 7px solid transparent;
                border-top: 7px solid #00ff88;
            }
            
            QComboBox QAbstractItemView {
                background-color: #0a0f1a;
                color: #00ff88;
                border: 2px solid #00ff88;
                selection-background-color: #2a4a5a;
            }
            
            QDialog {
                background-color: #0a0f1a;
                color: #00ff88;
                border: 2px solid #00ff88;
            }
            
            QScrollBar:vertical {
                background-color: #1a2a3a;
                width: 15px;
                border: 1px solid #00ff88;
            }
            
            QScrollBar::handle:vertical {
                background-color: #00ff88;
                min-height: 20px;
            }
            
            QScrollBar::handle:vertical:hover {
                background-color: #44ffaa;
            }
        )";
        
        qApp->setStyleSheet(futuristicCSS);
    } else if (themeName == "retro") {
        QString retroCSS = R"(
            /* Retro Theme - 80s/90s Computer Aesthetic */
            QMainWindow {
                background-color: #000080;
                color: #00ff00;
                font-family: 'Courier New', monospace;
            }
            
            QWidget {
                background-color: #0000aa;
                color: #00ff00;
                border: 2px solid #00ff00;
            }
            
            QPushButton {
                background-color: #0000ff;
                color: #ffffff;
                border: 3px solid #00ff00;
                border-radius: 0px;
                padding: 8px 16px;
                font-family: 'Courier New', monospace;
                font-weight: bold;
                font-size: 11px;
            }
            
            QPushButton:hover {
                background-color: #4444ff;
                box-shadow: 0 0 10px #00ff00;
            }
            
            QPushButton:pressed {
                background-color: #0000cc;
            }
            
            QLabel {
                color: #00ff00;
                font-family: 'Courier New', monospace;
                font-weight: bold;
                background-color: transparent;
            }
            
            QGroupBox {
                color: #00ff00;
                border: 3px solid #00ff00;
                border-radius: 0px;
                margin-top: 10px;
                font-weight: bold;
                font-family: 'Courier New', monospace;
                background-color: #0000aa;
            }
            
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                color: #00ff00;
                background-color: #000080;
                border: 2px solid #00ff00;
            }
            
            QComboBox {
                background-color: #0000aa;
                color: #00ff00;
                border: 3px solid #00ff00;
                border-radius: 0px;
                padding: 6px;
                font-family: 'Courier New', monospace;
                font-weight: bold;
            }
            
            QComboBox::drop-down {
                border: none;
                background-color: #0000aa;
            }
            
            QComboBox::down-arrow {
                image: none;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 5px solid #00ff00;
            }
            
            QComboBox QAbstractItemView {
                background-color: #000080;
                color: #00ff00;
                border: 3px solid #00ff00;
                selection-background-color: #0000cc;
            }
            
            QDialog {
                background-color: #000080;
                color: #00ff00;
                border: 3px solid #00ff00;
            }
            
            QScrollBar:vertical {
                background-color: #0000aa;
                width: 15px;
                border: 2px solid #00ff00;
            }
            
            QScrollBar::handle:vertical {
                background-color: #00ff00;
                min-height: 20px;
            }
            
            QScrollBar::handle:vertical:hover {
                background-color: #44ff44;
            }
        )";
        
        qApp->setStyleSheet(retroCSS);
    } else if (themeName == "minimal") {
        QString minimalCSS = R"(
            /* Minimal Theme - Clean and Simple */
            QMainWindow {
                background-color: #f8f8f8;
                color: #333333;
                font-family: 'Segoe UI', 'Arial', sans-serif;
            }
            
            QWidget {
                background-color: #ffffff;
                color: #333333;
                border: 1px solid #e0e0e0;
            }
            
            QPushButton {
                background-color: #ffffff;
                color: #333333;
                border: 1px solid #cccccc;
                border-radius: 4px;
                padding: 8px 16px;
                font-weight: normal;
                font-size: 12px;
            }
            
            QPushButton:hover {
                background-color: #f0f0f0;
                border-color: #999999;
            }
            
            QPushButton:pressed {
                background-color: #e8e8e8;
            }
            
            QLabel {
                color: #333333;
                font-weight: normal;
                background-color: transparent;
            }
            
            QGroupBox {
                color: #333333;
                border: 1px solid #e0e0e0;
                border-radius: 6px;
                margin-top: 10px;
                font-weight: normal;
                background-color: #ffffff;
            }
            
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                color: #333333;
                background-color: #f8f8f8;
                border: 1px solid #e0e0e0;
            }
            
            QComboBox {
                background-color: #ffffff;
                color: #333333;
                border: 1px solid #cccccc;
                border-radius: 4px;
                padding: 6px;
                font-weight: normal;
            }
            
            QComboBox::drop-down {
                border: none;
                background-color: #ffffff;
            }
            
            QComboBox::down-arrow {
                image: none;
                border-left: 4px solid transparent;
                border-right: 4px solid transparent;
                border-top: 4px solid #666666;
            }
            
            QComboBox QAbstractItemView {
                background-color: #ffffff;
                color: #333333;
                border: 1px solid #cccccc;
                selection-background-color: #e8e8e8;
            }
            
            QDialog {
                background-color: #ffffff;
                color: #333333;
                border: 1px solid #cccccc;
            }
            
            QScrollBar:vertical {
                background-color: #f0f0f0;
                width: 12px;
                border: none;
            }
            
            QScrollBar::handle:vertical {
                background-color: #cccccc;
                min-height: 20px;
                border-radius: 6px;
            }
            
            QScrollBar::handle:vertical:hover {
                background-color: #999999;
            }
        )";
        
        qApp->setStyleSheet(minimalCSS);
    } else if (themeName == "light") {
        QString lightCSS = R"(
            /* Light Theme - Bright and Professional */
            QMainWindow {
                background-color: #ffffff;
                color: #2c3e50;
                font-family: 'Segoe UI', 'Arial', sans-serif;
            }
            
            QWidget {
                background-color: #ffffff;
                color: #2c3e50;
                border: 1px solid #bdc3c7;
            }
            
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3498db, stop:1 #2980b9);
                color: #ffffff;
                border: 1px solid #2980b9;
                border-radius: 6px;
                padding: 10px 20px;
                font-weight: bold;
                font-size: 12px;
            }
            
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5dade2, stop:1 #3498db);
                box-shadow: 0 2px 4px rgba(52,152,219,0.3);
            }
            
            QPushButton:pressed {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2980b9, stop:1 #1f618d);
            }
            
            QLabel {
                color: #2c3e50;
                font-weight: normal;
                background-color: transparent;
            }
            
            QGroupBox {
                color: #2c3e50;
                border: 2px solid #bdc3c7;
                border-radius: 8px;
                margin-top: 10px;
                font-weight: bold;
                background-color: #f8f9fa;
            }
            
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                color: #2c3e50;
                background-color: #ffffff;
                border: 1px solid #bdc3c7;
            }
            
            QComboBox {
                background-color: #ffffff;
                color: #2c3e50;
                border: 2px solid #bdc3c7;
                border-radius: 6px;
                padding: 8px;
                font-weight: normal;
            }
            
            QComboBox::drop-down {
                border: none;
                background-color: #ffffff;
            }
            
            QComboBox::down-arrow {
                image: none;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 5px solid #2c3e50;
            }
            
            QComboBox QAbstractItemView {
                background-color: #ffffff;
                color: #2c3e50;
                border: 2px solid #bdc3c7;
                selection-background-color: #e3f2fd;
            }
            
            QDialog {
                background-color: #ffffff;
                color: #2c3e50;
                border: 2px solid #bdc3c7;
            }
            
            QScrollBar:vertical {
                background-color: #ecf0f1;
                width: 12px;
                border: 1px solid #bdc3c7;
            }
            
            QScrollBar::handle:vertical {
                background-color: #3498db;
                min-height: 20px;
                border-radius: 6px;
            }
            
            QScrollBar::handle:vertical:hover {
                background-color: #2980b9;
            }
        )";
        
        qApp->setStyleSheet(lightCSS);
    } else if (themeName == "basic") {
        // Basic theme - simple and clean
        qApp->setStyleSheet("");
    } else if (themeName == "dark") {
        // Dark theme
        QString darkCSS = R"(
            QMainWindow, QWidget {
                background-color: #2b2b2b;
                color: #ffffff;
            }
            QPushButton {
                background-color: #404040;
                color: #ffffff;
                border: 1px solid #666666;
                border-radius: 5px;
                padding: 8px;
            }
            QPushButton:hover {
                background-color: #505050;
            }
        )";
        qApp->setStyleSheet(darkCSS);
    }
}

void DogecoinGUI::showSettingsDialog()
{
    // Create a revolutionary settings dialog with theme selection
    QDialog* settingsDialog = new QDialog(this);
    settingsDialog->setWindowTitle("⚙️ Advanced Settings");
    settingsDialog->setFixedSize(800, 700);
    settingsDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    settingsDialog->setAttribute(Qt::WA_TranslucentBackground);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(settingsDialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Custom title bar for dialog
    QWidget* dialogTitleBar = new QWidget();
    dialogTitleBar->setObjectName("dialogTitleBar");
    dialogTitleBar->setFixedHeight(40);
    dialogTitleBar->setStyleSheet(R"(
        QWidget#dialogTitleBar {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2d2d2d, stop:1 #404040);
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
        }
    )");
    
    QHBoxLayout* dialogTitleLayout = new QHBoxLayout(dialogTitleBar);
    dialogTitleLayout->setContentsMargins(15, 0, 10, 0);
    
    QLabel* dialogTitle = new QLabel("Settings");
    dialogTitle->setStyleSheet("color: #ffffff; font-weight: 600; font-size: 14px;");
    
    QPushButton* closeDialogBtn = new QPushButton("×");
    closeDialogBtn->setFixedSize(30, 25);
    closeDialogBtn->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #ffffff;
            border: none;
            font-size: 18px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #ff4444;
            border-radius: 3px;
        }
    )");
    
    dialogTitleLayout->addWidget(dialogTitle);
    dialogTitleLayout->addStretch();
    dialogTitleLayout->addWidget(closeDialogBtn);
    
    connect(closeDialogBtn, &QPushButton::clicked, settingsDialog, &QDialog::close);
    
    // Revolutionary main content with tabs
    QWidget* content = new QWidget();
    content->setObjectName("settingsContent");
    content->setStyleSheet(R"(
        QWidget#settingsContent {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1a1a1a, stop:1 #2d2d2d);
            border-bottom-left-radius: 15px;
            border-bottom-right-radius: 15px;
            border: 2px solid #404040;
        }
    )");
    
    QVBoxLayout* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(30, 30, 30, 30);
    
    // Revolutionary theme selection section
    QGroupBox* themeGroup = new QGroupBox("🎨 Advanced Theme System");
    themeGroup->setStyleSheet(R"(
        QGroupBox {
            color: #ffffff;
            font-weight: 700;
            font-size: 18px;
            border: 3px solid #0078d4;
            border-radius: 15px;
            margin-top: 15px;
            padding-top: 15px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(0,120,212,0.1), stop:1 rgba(0,120,212,0.05));
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 15px 0 15px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0078d4, stop:1 #106ebe);
            border-radius: 8px;
            color: #ffffff;
        }
    )");
    
    QVBoxLayout* themeLayout = new QVBoxLayout(themeGroup);
    
    // Current theme display with enhanced styling
    QString currentTheme = m_currentTheme.isEmpty() ? QString("dark") : m_currentTheme;
    QLabel* currentThemeLabel = new QLabel("🚀 Active Theme: " + currentTheme);
    currentThemeLabel->setStyleSheet(R"(
        color: #00ff00;
        font-size: 16px;
        font-weight: 600;
        margin: 15px;
        padding: 10px;
        background: rgba(0,255,0,0.1);
        border: 1px solid #00ff00;
        border-radius: 8px;
        text-shadow: 0 0 10px #00ff00;
    )");
    themeLayout->addWidget(currentThemeLabel);
    
    // Theme selection buttons
    QHBoxLayout* themeButtonsLayout = new QHBoxLayout();
    
    QPushButton* cycleThemeBtn = new QPushButton("🔄 Cycle Through All Themes");
    cycleThemeBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff0080, stop:0.5 #0078d4, stop:1 #00ff00);
            color: #ffffff;
            border: 2px solid #ffffff;
            border-radius: 12px;
            padding: 15px 25px;
            font-weight: 700;
            font-size: 16px;
            text-shadow: 0 0 10px #ffffff;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff4080, stop:0.5 #106ebe, stop:1 #40ff40);
            box-shadow: 0 0 20px rgba(255,255,255,0.5);
            transform: scale(1.05);
        }
        QPushButton:pressed {
            transform: scale(0.95);
        }
    )");
    
    QComboBox* themeDropdown = new QComboBox();
    themeDropdown->setStyleSheet(R"(
        QComboBox {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #404040, stop:1 #2d2d2d);
            color: #ffffff;
            border: 2px solid #0078d4;
            border-radius: 10px;
            padding: 12px 20px;
            font-size: 16px;
            font-weight: 600;
            min-width: 200px;
        }
        QComboBox:hover {
            border: 2px solid #00ff00;
            box-shadow: 0 0 15px rgba(0,255,0,0.3);
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 8px solid transparent;
            border-right: 8px solid transparent;
            border-top: 8px solid #ffffff;
            margin-right: 15px;
        }
        QComboBox QAbstractItemView {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #404040, stop:1 #2d2d2d);
            color: #ffffff;
            border: 2px solid #0078d4;
            selection-background-color: #0078d4;
            padding: 5px;
        }
        QComboBox QAbstractItemView::item {
            padding: 10px;
            border-radius: 5px;
            margin: 2px;
        }
        QComboBox QAbstractItemView::item:selected {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff0080, stop:1 #0078d4);
            color: #ffffff;
        }
    )");
    
    // Add all available themes including the new Matrix theme
    QStringList allThemes = QStringList() << "basic" << "dark" << "light" << "cyberpunk" << "neon" << "futuristic" << "retro" << "minimal" << "matrix";
    themeDropdown->addItems(allThemes);
    themeDropdown->setCurrentText(currentTheme);
    
    themeButtonsLayout->addWidget(cycleThemeBtn);
    themeButtonsLayout->addWidget(themeDropdown);
    themeButtonsLayout->addStretch();
    
    themeLayout->addLayout(themeButtonsLayout);
    
    // Connect theme selection with simple cycling (uses m_themeIndex member)
    QStringList availableThemes = QStringList() << "basic" << "dark" << "light" << "cyberpunk" << "neon" << "futuristic" << "retro" << "minimal" << "matrix";
    
    connect(cycleThemeBtn, &QPushButton::clicked, [currentThemeLabel, themeDropdown, availableThemes, this]() {
        m_themeIndex = (m_themeIndex + 1) % availableThemes.size();
        QString newTheme = availableThemes[m_themeIndex];
        applyGlobalTheme(newTheme);
        currentThemeLabel->setText("🚀 Active Theme: " + newTheme);
        themeDropdown->setCurrentText(newTheme);
    });
    
    connect(themeDropdown, QOverload<const QString&>::of(&QComboBox::currentTextChanged), 
            [currentThemeLabel, this](const QString& theme) {
        // Apply the selected theme
        applyGlobalTheme(theme);
        currentThemeLabel->setText("🚀 Active Theme: " + theme);
    });
    
    contentLayout->addWidget(themeGroup);
    
    // Add revolutionary new features section
    QGroupBox* featuresGroup = new QGroupBox("🚀 Revolutionary Features");
    featuresGroup->setStyleSheet(R"(
        QGroupBox {
            color: #ffffff;
            font-weight: 700;
            font-size: 18px;
            border: 3px solid #ff0080;
            border-radius: 15px;
            margin-top: 20px;
            padding-top: 15px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(255,0,128,0.1), stop:1 rgba(255,0,128,0.05));
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 15px 0 15px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ff0080, stop:1 #ff4080);
            border-radius: 8px;
            color: #ffffff;
        }
    )");
    
    QVBoxLayout* featuresLayout = new QVBoxLayout(featuresGroup);
    
    // Feature buttons
    QHBoxLayout* featureButtonsLayout = new QHBoxLayout();
    
    QPushButton* resetLayoutBtn = new QPushButton("🎯 Reset Layout");
    resetLayoutBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff6b35, stop:1 #f7931e);
            color: #ffffff;
            border: 2px solid #ffffff;
            border-radius: 10px;
            padding: 12px 20px;
            font-weight: 600;
            font-size: 14px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ff8b55, stop:1 #ffa33e);
            box-shadow: 0 0 15px rgba(255,107,53,0.5);
        }
    )");
    
    // Removed random theme button as requested
    
    QPushButton* saveThemeBtn = new QPushButton("💾 Save Theme");
    saveThemeBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4caf50, stop:1 #2e7d32);
            color: #ffffff;
            border: 2px solid #ffffff;
            border-radius: 10px;
            padding: 12px 20px;
            font-weight: 600;
            font-size: 14px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #66bb6a, stop:1 #388e3c);
            box-shadow: 0 0 15px rgba(76,175,80,0.5);
        }
    )");
    
    featureButtonsLayout->addWidget(resetLayoutBtn);
    featureButtonsLayout->addWidget(saveThemeBtn);
    featureButtonsLayout->addStretch();
    
    featuresLayout->addLayout(featureButtonsLayout);
    
    // Connect feature buttons (temporarily disabled)
    // Random theme button removed as requested
    
    contentLayout->addWidget(featuresGroup);
    contentLayout->addStretch();
    
    // Enhanced close button
    QPushButton* closeBtn = new QPushButton("✨ Close Settings");
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #666666, stop:1 #404040);
            color: #ffffff;
            border: 2px solid #888888;
            border-radius: 12px;
            padding: 15px 40px;
            font-weight: 700;
            font-size: 16px;
            text-shadow: 0 0 8px #ffffff;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #888888, stop:1 #666666);
            border: 2px solid #ffffff;
            box-shadow: 0 0 20px rgba(255,255,255,0.3);
            transform: scale(1.05);
        }
        QPushButton:pressed {
            transform: scale(0.95);
        }
    )");
    connect(closeBtn, &QPushButton::clicked, settingsDialog, &QDialog::close);
    
    contentLayout->addWidget(closeBtn);
    
    mainLayout->addWidget(dialogTitleBar);
    mainLayout->addWidget(content);
    
    settingsDialog->exec();
    settingsDialog->deleteLater();
}

DogecoinGUI::~DogecoinGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    GUIUtil::saveWindowGeometry("nWindow", this);
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif

    delete rpcConsole;
}

void DogecoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(platformStyle->SingleColorIcon(":/icons/overview"), tr("&Wow"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/send"), tr("&Such Send"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a Dogecoin address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    sendCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/send"), sendCoinsAction->text(), this);
    sendCoinsMenuAction->setStatusTip(sendCoinsAction->statusTip());
    sendCoinsMenuAction->setToolTip(sendCoinsMenuAction->statusTip());

    receiveCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/receiving_addresses"), tr("&Much Receive"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and dogecoin: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    receiveCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/receiving_addresses"), receiveCoinsAction->text(), this);
    receiveCoinsMenuAction->setStatusTip(receiveCoinsAction->statusTip());
    receiveCoinsMenuAction->setToolTip(receiveCoinsMenuAction->statusTip());

    historyAction = new QAction(platformStyle->SingleColorIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

#ifdef ENABLE_WALLET
    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
#endif // ENABLE_WALLET

    quitAction = new QAction(platformStyle->TextColorIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(platformStyle->TextColorIcon(":/icons/about"), tr("&About %1").arg(tr(PACKAGE_NAME)), this);
    aboutAction->setStatusTip(tr("Show information about %1").arg(tr(PACKAGE_NAME)));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setEnabled(false);
    aboutQtAction = new QAction(platformStyle->TextColorIcon(":/icons/about_qt"), tr("About &Qt"), this);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(platformStyle->TextColorIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for %1").arg(tr(PACKAGE_NAME)));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    optionsAction->setEnabled(false);
    toggleHideAction = new QAction(platformStyle->TextColorIcon(":/icons/about"), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(platformStyle->TextColorIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    signMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your Dogecoin addresses to prove you own them"));
    verifyMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/verify"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified Dogecoin addresses"));
    paperWalletAction = new QAction(QIcon(":/icons/print"), tr("&Print paper wallets"), this);
    paperWalletAction->setStatusTip(tr("Print paper wallets"));

    openRPCConsoleAction = new QAction(platformStyle->TextColorIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));
    // initially disable the debug window menu item
    openRPCConsoleAction->setEnabled(false);

    usedSendingAddressesAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Such sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Much receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    openAction = new QAction(platformStyle->TextColorIcon(":/icons/open"), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a dogecoin: URI or payment request"));

    importPrivateKeyAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Import Private Key..."), this);
    importPrivateKeyAction->setStatusTip(tr("Import a Dogecoin private key"));

    showHelpMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/info"), tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the %1 help message to get a list with possible Dogecoin command-line options").arg(tr(PACKAGE_NAME)));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showDebugWindow()));
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

#ifdef ENABLE_WALLET
    if(walletFrame)
    {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
        connect(paperWalletAction, SIGNAL(triggered()), walletFrame, SLOT(printPaperWallets()));
        connect(importPrivateKeyAction, SIGNAL(triggered()), walletFrame, SLOT(importPrivateKey()));
    }
#endif // ENABLE_WALLET

    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this, SLOT(showDebugWindowActivateConsole()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this, SLOT(showDebugWindow()));
}

void DogecoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    if(walletFrame)
    {
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addAction(paperWalletAction);
        file->addSeparator();
        file->addAction(importPrivateKeyAction);
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    if(walletFrame)
    {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addSeparator();
    }
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    if(walletFrame)
    {
        help->addAction(openRPCConsoleAction);
    }
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void DogecoinGUI::createToolBars()
{
    if(walletFrame)
    {
        QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
        toolbar->setMovable(false);
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addAction(overviewAction);
        toolbar->addAction(sendCoinsAction);
        toolbar->addAction(receiveCoinsAction);
        toolbar->addAction(historyAction);
        overviewAction->setChecked(true);
    }
}

void DogecoinGUI::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
    
    // Pass to modern UI if enabled
    
    if(_clientModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        qWarning("DogecoinGUI::setClientModel: tray menu");
        createTrayIconMenu();

        // Keep up to date with client
        qWarning("DogecoinGUI::setClientModel: updateNetworkState");
        updateNetworkState();
        connect(_clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));
        connect(_clientModel, SIGNAL(networkActiveChanged(bool)), this, SLOT(setNetworkActive(bool)));

        if (modalOverlay) {
        modalOverlay->setKnownBestHeight(_clientModel->getHeaderTipHeight(), QDateTime::fromTime_t(_clientModel->getHeaderTipTime()));
        }
        qWarning("DogecoinGUI::setClientModel: setNumBlocks");
        setNumBlocks(_clientModel->getNumBlocks(), _clientModel->getLastBlockDate(), _clientModel->getVerificationProgress(NULL), false);
        connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(setNumBlocks(int,QDateTime,double,bool)));

        // Receive and report messages from client model
        connect(_clientModel, SIGNAL(message(QString,QString,unsigned int)), this, SLOT(message(QString,QString,unsigned int)));

        // Show progress dialog
        connect(_clientModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)));

        qWarning("DogecoinGUI::setClientModel: rpcConsole");
        rpcConsole->setClientModel(_clientModel);
#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            qWarning("DogecoinGUI::setClientModel: walletFrame");
            walletFrame->setClientModel(_clientModel);
        }
#endif // ENABLE_WALLET
        if (unitDisplayControl) {
        unitDisplayControl->setOptionsModel(_clientModel->getOptionsModel());
        }
        
        OptionsModel* optionsModel = _clientModel->getOptionsModel();
        if(optionsModel)
        {
            // be aware of the tray icon disable state change reported by the OptionsModel object.
            connect(optionsModel,SIGNAL(hideTrayIconChanged(bool)),this,SLOT(setTrayIconVisible(bool)));
        
            // initialize the disable state of the tray icon with the current value in the model.
            setTrayIconVisible(optionsModel->getHideTrayIcon());
        }
        qWarning("DogecoinGUI::setClientModel: done");
    } else {
        // Disable possibility to show main window via action
        if (toggleHideAction) {
        toggleHideAction->setEnabled(false);
        }
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
        // Propagate cleared model to child objects
        rpcConsole->setClientModel(nullptr);
#ifdef ENABLE_WALLET
        if (walletFrame)
        {
            walletFrame->setClientModel(nullptr);
        }
#endif // ENABLE_WALLET
        if (unitDisplayControl) {
            unitDisplayControl->setOptionsModel(nullptr);
        }
    }
}

#ifdef ENABLE_WALLET
bool DogecoinGUI::addWallet(const QString& name, WalletModel *walletModel)
{
    
    
    if(!walletFrame)
        return false;
    setWalletActionsEnabled(true);
    return walletFrame->addWallet(name, walletModel);
}

bool DogecoinGUI::setCurrentWallet(const QString& name)
{
    if(!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

void DogecoinGUI::removeAllWallets()
{
    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void DogecoinGUI::setWalletActionsEnabled(bool enabled)
{
    // Null-check every action: modern shell may construct GUI before all actions exist.
    if (overviewAction) overviewAction->setEnabled(enabled);
    if (sendCoinsAction) sendCoinsAction->setEnabled(enabled);
    if (sendCoinsMenuAction) sendCoinsMenuAction->setEnabled(enabled);
    if (receiveCoinsAction) receiveCoinsAction->setEnabled(enabled);
    if (receiveCoinsMenuAction) receiveCoinsMenuAction->setEnabled(enabled);
    if (historyAction) historyAction->setEnabled(enabled);
    if (encryptWalletAction) encryptWalletAction->setEnabled(enabled);
    if (backupWalletAction) backupWalletAction->setEnabled(enabled);
    if (changePassphraseAction) changePassphraseAction->setEnabled(enabled);
    if (signMessageAction) signMessageAction->setEnabled(enabled);
    if (verifyMessageAction) verifyMessageAction->setEnabled(enabled);
    if (usedSendingAddressesAction) usedSendingAddressesAction->setEnabled(enabled);
    if (usedReceivingAddressesAction) usedReceivingAddressesAction->setEnabled(enabled);
    if (openAction) openAction->setEnabled(enabled);
    if (paperWalletAction) paperWalletAction->setEnabled(enabled);
    if (importPrivateKeyAction) importPrivateKeyAction->setEnabled(enabled);
}

void DogecoinGUI::createTrayIcon(const NetworkStyle *networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("%1 client").arg(tr(PACKAGE_NAME)) + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getTrayAndWindowIcon());
    trayIcon->hide();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void DogecoinGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsMenuAction);
    trayIconMenu->addAction(receiveCoinsMenuAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void DogecoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

void DogecoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void DogecoinGUI::aboutClicked()
{
    if(!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();
}

void DogecoinGUI::showDebugWindow()
{
    if (rpcConsole) {
    rpcConsole->showNormal();
    rpcConsole->show();
    rpcConsole->raise();
    rpcConsole->activateWindow();
    }
}

void DogecoinGUI::showDebugWindowActivateConsole()
{
    if (rpcConsole) {
    rpcConsole->setTabFocus(RPCConsole::TAB_CONSOLE);
    showDebugWindow();
    }
}

void DogecoinGUI::showHelpMessageClicked()
{
    if (helpMessageDialog) {
    helpMessageDialog->show();
    }
}

#ifdef ENABLE_WALLET
void DogecoinGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void DogecoinGUI::gotoOverviewPage()
{
    if (overviewAction) {
    overviewAction->setChecked(true);
    }
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void DogecoinGUI::gotoHistoryPage()
{
    if (historyAction) {
    historyAction->setChecked(true);
    }
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void DogecoinGUI::gotoReceiveCoinsPage()
{
    if (receiveCoinsAction) {
    receiveCoinsAction->setChecked(true);
    }
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void DogecoinGUI::gotoSendCoinsPage(QString addr)
{
    if (sendCoinsAction) {
    sendCoinsAction->setChecked(true);
    }
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void DogecoinGUI::gotoMemeStreamPage()
{
    if (walletFrame) walletFrame->gotoMemeStreamPage();
}

void DogecoinGUI::gotoDogeBusinessPage(int tab)
{
    if (walletFrame) walletFrame->gotoDogeBusinessPage(tab);
}

void DogecoinGUI::gotoNetworkPage()
{
    if (walletFrame) walletFrame->gotoNetworkPage();
}

void DogecoinGUI::gotoArcadePage()
{
    if (walletFrame) walletFrame->gotoArcadePage();
}

void DogecoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void DogecoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}
#endif // ENABLE_WALLET

void DogecoinGUI::updateNetworkState()
{
    int count = clientModel->getNumConnections();
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }

    QString tooltip;

    if (clientModel->getNetworkActive()) {
        tooltip = tr("%n active connection(s) to Dogecoin network", "", count) + QString(".<br>") + tr("Click to disable network activity.");
    } else {
        tooltip = tr("Network activity disabled.") + QString("<br>") + tr("Click to enable network activity again.");
        icon = ":/icons/network_disabled";
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    // Check if connectionsControl exists before using it
    if (connectionsControl) {
        connectionsControl->setToolTip(tooltip);
    connectionsControl->setPixmap(platformStyle->SingleColorIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    }
}

void DogecoinGUI::setNumConnections(int count)
{
    updateNetworkState();
}

void DogecoinGUI::setNetworkActive(bool networkActive)
{
    updateNetworkState();
}

void DogecoinGUI::updateHeadersSyncProgressLabel()
{
    if (!clientModel)
        return;
    int64_t headersTipTime = clientModel->getHeaderTipTime();
    int headersTipHeight = clientModel->getHeaderTipHeight();
    if (headersTipHeight < 0)
        return;
    int estHeadersLeft = (GetTime() - headersTipTime) / Params().GetConsensus(headersTipHeight).nPowTargetSpacing;
    if (estHeadersLeft > HEADER_HEIGHT_DELTA_SYNC) {
        const double headerPct =
            100.0 * static_cast<double>(headersTipHeight) /
            static_cast<double>(headersTipHeight + estHeadersLeft);
        if (progressBarLabel) {
            progressBarLabel->setText(tr("Syncing Headers (%1%)...")
                                          .arg(QString::number(headerPct, 'f', 1)));
            progressBarLabel->setVisible(true);
        }
        // Status bar used to leave the bar at block-verification % (~0) while the
        // label already said headers 1.9% — keep both in lockstep.
        if (progressBar) {
            progressBar->setFormat(tr("%1% headers").arg(QString::number(headerPct, 'f', 1)));
            progressBar->setMaximum(1000);
            progressBar->setValue(static_cast<int>(headerPct * 10.0 + 0.5));
            progressBar->setVisible(true);
        }
    }
}

void DogecoinGUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool header)
{
    if (modalOverlay)
    {
        if (header) {
            modalOverlay->setKnownBestHeight(count, blockDate);
        } else {
            modalOverlay->tipUpdate(count, blockDate, nVerificationProgress);
        }
    }
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BLOCK_SOURCE_NETWORK:
            if (header) {
                updateHeadersSyncProgressLabel();
                // Still show out-of-sync modal during header phase
#ifdef ENABLE_WALLET
                if (walletFrame && modalOverlay) {
                    walletFrame->showOutOfSyncWarning(true);
                    modalOverlay->showHide();
                }
#endif
                return;
            }
            if (progressBarLabel) {
            progressBarLabel->setText(tr("Synchronizing with network..."));
            }
            updateHeadersSyncProgressLabel();
            break;
        case BLOCK_SOURCE_DISK:
            if (header) {
                if (progressBarLabel) {
                progressBarLabel->setText(tr("Indexing blocks on disk..."));
                }
            } else {
                if (progressBarLabel) {
                progressBarLabel->setText(tr("Processing blocks on disk..."));
                }
            }
            break;
        case BLOCK_SOURCE_REINDEX:
            if (progressBarLabel) {
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            }
            break;
        case BLOCK_SOURCE_NONE:
            if (header) {
                return;
            }
            if (progressBarLabel) {
            progressBarLabel->setText(tr("Connecting to peers..."));
            }
            break;
    }

    QString tooltip;

    QDateTime currentDate = QDateTime::currentDateTime();
    qint64 secs = blockDate.secsTo(currentDate);

    tooltip = tr("Processed %n block(s) of transaction history.", "", count);

    // AssumeUTXO Phase D: tip may be "current" while background still proves history.
    if (clientModel->isAssumeUtxoActive() && !clientModel->isAssumeUtxoValidated()) {
        const QString assumeLabel = clientModel->getAssumeUtxoStatusLabel();
        if (progressBarLabel) {
            progressBarLabel->setText(assumeLabel);
            progressBarLabel->setVisible(true);
        }
        if (progressBar) {
            const double p = clientModel->getAssumeUtxoValidationProgress();
            progressBar->setFormat(tr("%1% — validating snapshot history").arg(QString::number(100.0 * p, 'f', 1)));
            progressBar->setMaximum(1000);
            progressBar->setValue(static_cast<int>(p * 1000.0));
            progressBar->setVisible(true);
        }
        tooltip = assumeLabel + QString("<br>") + tooltip;
        if (labelBlocksIcon) {
            labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        }
#ifdef ENABLE_WALLET
        if (walletFrame) {
            walletFrame->showOutOfSyncWarning(false);
        }
#endif
        if (labelBlocksIcon) {
            labelBlocksIcon->setToolTip(tooltip);
        }
        return;
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        if (labelBlocksIcon) {
        labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        }

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(false);
            if (modalOverlay) {
            modalOverlay->showHide(true, true);
            }
        }
#endif // ENABLE_WALLET

        if (progressBarLabel) {
        progressBarLabel->setVisible(false);
        }
        if (progressBar) {
        progressBar->setVisible(false);
        }
    }
    else
    {
        QString timeBehindText = GUIUtil::formatNiceTimeOffset(secs);

        if (progressBarLabel) {
        progressBarLabel->setVisible(true);
        }
        if (progressBar) {
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        }
        if (progressBar) {
        progressBar->setValue(nVerificationProgress * 1000000000.0 + 0.5);
        progressBar->setVisible(true);
        }

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if(count != prevBlocks)
        {
            if (labelBlocksIcon) {
            labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(QString(
                ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            }
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(true);
            if (modalOverlay) {
            modalOverlay->showHide();
            }
        }
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    if (labelBlocksIcon) {
    labelBlocksIcon->setToolTip(tooltip);
    }
    if (progressBarLabel) {
    progressBarLabel->setToolTip(tooltip);
    }
    if (progressBar) {
    progressBar->setToolTip(tooltip);
    }
}

void DogecoinGUI::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    QString strTitle = tr("Dogecoin"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    }
    else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }
    // Append title to "Dogecoin - "
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    }
    else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons, this);
        int r = mBox.exec();
        if (ret != NULL)
            *ret = r == QMessageBox::Ok;
    } else if (notificator) {
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
    } else {
        // Fallback if tray/notificator was not initialized
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, QMessageBox::Ok, this);
        mBox.exec();
        if (ret != NULL)
            *ret = true;
    }
}

void DogecoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void DogecoinGUI::closeEvent(QCloseEvent *event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            // close rpcConsole in case it was open to make some space for the shutdown window
            rpcConsole->close();

            QApplication::quit();
        }
        else
        {
            QMainWindow::showMinimized();
            event->ignore();
        }
    }
#else
    QMainWindow::closeEvent(event);
#endif
}

void DogecoinGUI::showEvent(QShowEvent *event)
{
    // enable the debug window when the main window shows up
    if (openRPCConsoleAction) {
    openRPCConsoleAction->setEnabled(true);
    }
    if (aboutAction) {
    aboutAction->setEnabled(true);
    }
    if (optionsAction) {
    optionsAction->setEnabled(true);
    }
}

#ifdef ENABLE_WALLET
void DogecoinGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label)
{
    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(DogecoinUnits::formatWithUnit(unit, amount, true)) +
                  tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             msg, CClientUIInterface::MSG_INFORMATION);
}
#endif // ENABLE_WALLET

void DogecoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void DogecoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        Q_FOREACH(const QUrl &uri, event->mimeData()->urls())
        {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool DogecoinGUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if ((progressBarLabel && progressBarLabel->isVisible()) || (progressBar && progressBar->isVisible()))
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool DogecoinGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void DogecoinGUI::setHDStatus(int hdEnabled)
{
    if (labelWalletHDStatusIcon) {
    labelWalletHDStatusIcon->setPixmap(platformStyle->SingleColorIcon(hdEnabled ? ":/icons/hd_enabled" : ":/icons/hd_disabled").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelWalletHDStatusIcon->setToolTip(hdEnabled ? tr("HD key generation is <b>enabled</b>") : tr("HD key generation is <b>disabled</b>"));
    labelWalletHDStatusIcon->setEnabled(hdEnabled);
    }
}

void DogecoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        if (labelWalletEncryptionIcon) labelWalletEncryptionIcon->hide();
        if (encryptWalletAction) encryptWalletAction->setChecked(false);
        if (changePassphraseAction) changePassphraseAction->setEnabled(false);
        if (encryptWalletAction) encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        if (labelWalletEncryptionIcon) {
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        }
        if (encryptWalletAction) encryptWalletAction->setChecked(true);
        if (changePassphraseAction) changePassphraseAction->setEnabled(true);
        if (encryptWalletAction) encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        if (labelWalletEncryptionIcon) {
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        }
        if (encryptWalletAction) encryptWalletAction->setChecked(true);
        if (changePassphraseAction) changePassphraseAction->setEnabled(true);
        if (encryptWalletAction) encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}
#endif // ENABLE_WALLET

void DogecoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void DogecoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void DogecoinGUI::detectShutdown()
{
    if (ShutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void DogecoinGUI::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog)
        progressDialog->setValue(nProgress);
}

void DogecoinGUI::setTrayIconVisible(bool fHideTrayIcon)
{
    if (trayIcon)
    {
        trayIcon->setVisible(!fHideTrayIcon);
    }
}

void DogecoinGUI::showModalOverlay()
{
    if (modalOverlay && (progressBar->isVisible() || modalOverlay->isLayerVisible()))
        modalOverlay->toggleVisibility();
}

static bool ThreadSafeMessageBox(DogecoinGUI *gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(caption)),
                               Q_ARG(QString, QString::fromStdString(message)),
                               Q_ARG(unsigned int, style),
                               Q_ARG(bool*, &ret));
    return ret;
}

void DogecoinGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this,
                                                         boost::placeholders::_1,
                                                         boost::placeholders::_2,
                                                         boost::placeholders::_3));
    uiInterface.ThreadSafeQuestion.connect(boost::bind(ThreadSafeMessageBox, this,
                                                       boost::placeholders::_1,
                                                       boost::placeholders::_3,
                                                       boost::placeholders::_4));
}

void DogecoinGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this,
                                                            boost::placeholders::_1,
                                                            boost::placeholders::_2,
                                                            boost::placeholders::_3));
    uiInterface.ThreadSafeQuestion.disconnect(boost::bind(ThreadSafeMessageBox, this,
                                                          boost::placeholders::_1,
                                                          boost::placeholders::_3,
                                                          boost::placeholders::_4));
}

void DogecoinGUI::toggleNetworkActive()
{
    if (clientModel) {
        clientModel->setNetworkActive(!clientModel->getNetworkActive());
    }
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl(const PlatformStyle *platformStyle) :
    optionsModel(0),
    menu(0)
{
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
    QList<DogecoinUnits::Unit> units = DogecoinUnits::availableUnits();
    int max_width = 0;
    const QFontMetrics fm(font());
    Q_FOREACH (const DogecoinUnits::Unit unit, units)
    {
        max_width = qMax(max_width, fm.width(DogecoinUnits::name(unit)));
    }
    setMinimumSize(max_width, 0);
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setStyleSheet(QString("QLabel { color : %1 }").arg(platformStyle->SingleColor().name()));
}

/** So that it responds to button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent *event)
{
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu()
{
    menu = new QMenu(this);
    Q_FOREACH(DogecoinUnits::Unit u, DogecoinUnits::availableUnits())
    {
        QAction *menuAction = new QAction(QString(DogecoinUnits::name(u)), this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu,SIGNAL(triggered(QAction*)),this,SLOT(onMenuSelection(QAction*)));
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel *_optionsModel)
{
    if (_optionsModel)
    {
        this->optionsModel = _optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(_optionsModel,SIGNAL(displayUnitChanged(int)),this,SLOT(updateDisplayUnit(int)));

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(_optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits)
{
    setText(DogecoinUnits::name(newUnits));
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action)
{
    if (action)
    {
        optionsModel->setDisplayUnit(action->data());
    }
}

void DogecoinGUI::createStatusBar()
{
    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    unitDisplayControl = new UnitDisplayStatusBarControl(platformStyle);
    labelWalletEncryptionIcon = new QLabel();
    labelWalletHDStatusIcon = new QLabel();
    connectionsControl = new GUIUtil::ClickableLabel();
    labelBlocksIcon = new GUIUtil::ClickableLabel();
    if(enableWallet)
    {
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(unitDisplayControl);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelWalletEncryptionIcon);
        frameBlocksLayout->addWidget(labelWalletHDStatusIcon);
    }
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(connectionsControl);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

    connect(connectionsControl, SIGNAL(clicked(QPoint)), this, SLOT(toggleNetworkActive()));

    modalOverlay = new ModalOverlay(this->centralWidget());
#ifdef ENABLE_WALLET
    if(enableWallet) {
        connect(walletFrame, SIGNAL(requestedSyncWarningInfo()), this, SLOT(showModalOverlay()));
        connect(labelBlocksIcon, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
        connect(progressBar, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
    }
#endif
}

