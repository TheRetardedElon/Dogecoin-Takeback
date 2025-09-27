#include "theme_manager.h"
#include "dogecoingui.h"
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDir>

EnhancedThemeManager::EnhancedThemeManager(DogecoinGUI* parent)
    : QObject(parent)
    , m_parent(parent)
    , m_menuBar(nullptr)
    , m_themeIndex(0)
{
    initializeThemes();
}

EnhancedThemeManager::~EnhancedThemeManager()
{
}

void EnhancedThemeManager::initializeThemes()
{
    setupDefaultThemes();
    // Temporarily disable custom menu bar to fix signal issues
    // createCustomMenuBar();
}

void EnhancedThemeManager::setupDefaultThemes()
{
    // Cyberpunk Theme
    ThemeInfo cyberpunk;
    cyberpunk.name = "cyberpunk";
    cyberpunk.displayName = "Cyberpunk";
    cyberpunk.description = "Neon-lit digital underworld";
    cyberpunk.primaryColor = "#00ff00";
    cyberpunk.secondaryColor = "#ff0080";
    cyberpunk.accentColor = "#00ffff";
    cyberpunk.backgroundColor = "#0a0a0a";
    cyberpunk.textColor = "#00ff00";
    cyberpunk.layout = "sidebar";
    cyberpunk.menuItems << "Hack" << "Transfer" << "Receive" << "Logs" << "Console" << "Matrix";
    cyberpunk.hasCustomMenus = true;
    m_themes["cyberpunk"] = cyberpunk;

    // Neon Theme
    ThemeInfo neon;
    neon.name = "neon";
    neon.displayName = "Neon";
    neon.description = "Electric pink and blue vibes";
    neon.primaryColor = "#ff0080";
    neon.secondaryColor = "#00ffff";
    neon.accentColor = "#ffff00";
    neon.backgroundColor = "#0a0a0a";
    neon.textColor = "#ff0080";
    neon.layout = "cards";
    neon.menuItems << "Spark" << "Send" << "Receive" << "History" << "Debug" << "Glow";
    neon.hasCustomMenus = true;
    m_themes["neon"] = neon;

    // Dark Theme
    ThemeInfo dark;
    dark.name = "dark";
    dark.displayName = "Dark";
    dark.description = "Professional dark mode";
    dark.primaryColor = "#ffffff";
    dark.secondaryColor = "#cccccc";
    dark.accentColor = "#0078d4";
    dark.backgroundColor = "#1a1a1a";
    dark.textColor = "#ffffff";
    dark.layout = "sidebar";
    dark.menuItems << "Home" << "Send" << "Receive" << "History" << "Tools" << "Themes";
    dark.hasCustomMenus = true;
    m_themes["dark"] = dark;

    // Light Theme
    ThemeInfo light;
    light.name = "light";
    light.displayName = "Light";
    light.description = "Clean and minimal";
    light.primaryColor = "#333333";
    light.secondaryColor = "#666666";
    light.accentColor = "#0078d4";
    light.backgroundColor = "#ffffff";
    light.textColor = "#333333";
    light.layout = "tabs";
    light.menuItems << "Overview" << "Send" << "Receive" << "History" << "Console" << "Settings";
    light.hasCustomMenus = true;
    m_themes["light"] = light;

    // Futuristic Theme
    ThemeInfo futuristic;
    futuristic.name = "futuristic";
    futuristic.displayName = "Futuristic";
    futuristic.description = "Space-age interface";
    futuristic.primaryColor = "#00d4ff";
    futuristic.secondaryColor = "#ff6b35";
    futuristic.accentColor = "#ffd700";
    futuristic.backgroundColor = "#001122";
    futuristic.textColor = "#00d4ff";
    futuristic.layout = "minimal";
    futuristic.menuItems << "Launch" << "Transmit" << "Receive" << "Archive" << "Terminal" << "Config";
    futuristic.hasCustomMenus = true;
    m_themes["futuristic"] = futuristic;

    // Retro Theme
    ThemeInfo retro;
    retro.name = "retro";
    retro.displayName = "Retro";
    retro.description = "80s computer vibes";
    retro.primaryColor = "#00ff00";
    retro.secondaryColor = "#ffff00";
    retro.accentColor = "#ff00ff";
    retro.backgroundColor = "#000000";
    retro.textColor = "#00ff00";
    retro.layout = "sidebar";
    retro.menuItems << "Boot" << "Xfer" << "Recv" << "Log" << "Debug" << "Mode";
    retro.hasCustomMenus = true;
    m_themes["retro"] = retro;

    // Minimal Theme
    ThemeInfo minimal;
    minimal.name = "minimal";
    minimal.displayName = "Minimal";
    minimal.description = "Ultra-clean interface";
    minimal.primaryColor = "#333333";
    minimal.secondaryColor = "#999999";
    minimal.accentColor = "#000000";
    minimal.backgroundColor = "#f8f8f8";
    minimal.textColor = "#333333";
    minimal.layout = "minimal";
    minimal.menuItems << "•" << "→" << "←" << "≡" << "⚙" << "○";
    minimal.hasCustomMenus = true;
    m_themes["minimal"] = minimal;

    m_currentTheme = "dark";
}

void EnhancedThemeManager::createCustomMenuBar()
{
    if (m_menuBar) {
        delete m_menuBar;
    }
    
    m_menuBar = new QMenuBar(m_parent);
    m_parent->setMenuBar(m_menuBar);
    
    updateMenuBarForTheme(m_currentTheme);
}

void EnhancedThemeManager::updateMenuBarForTheme(const QString& themeName)
{
    if (!m_menuBar) return;
    
    m_menuBar->clear();
    
    ThemeInfo theme = m_themes[themeName];
    if (!theme.hasCustomMenus) return;
    
    QStringList menuItems = theme.menuItems;
    if (menuItems.size() >= 6) {
        // Create themed menu items
        QMenu* menu1 = m_menuBar->addMenu(menuItems[0]); // Overview/Home/Hack/Spark/etc
        QMenu* menu2 = m_menuBar->addMenu(menuItems[1]); // Send/Transfer/Send/Launch/etc
        QMenu* menu3 = m_menuBar->addMenu(menuItems[2]); // Receive/Receive/Receive/Transmit/etc
        QMenu* menu4 = m_menuBar->addMenu(menuItems[3]); // History/Logs/History/Archive/etc
        QMenu* menu5 = m_menuBar->addMenu(menuItems[4]); // Console/Console/Debug/Terminal/etc
        QMenu* menu6 = m_menuBar->addMenu(menuItems[5]); // Themes/Matrix/Glow/Themes/Config/Mode/etc
        
        // Add actions to each menu
        QAction* action1 = menu1->addAction("Open");
        QAction* action2 = menu2->addAction("New Transaction");
        QAction* action3 = menu3->addAction("New Address");
        QAction* action4 = menu4->addAction("Export");
        QAction* action5 = menu5->addAction("Show Console");
        QAction* action6 = menu6->addAction("Change Theme");
        
        // Connect actions
        connect(action1, &QAction::triggered, [this]() { Q_EMIT menuActionTriggered("overview"); });
        connect(action2, &QAction::triggered, [this]() { Q_EMIT menuActionTriggered("send"); });
        connect(action3, &QAction::triggered, [this]() { Q_EMIT menuActionTriggered("receive"); });
        connect(action4, &QAction::triggered, [this]() { Q_EMIT menuActionTriggered("history"); });
        connect(action5, &QAction::triggered, [this]() { Q_EMIT menuActionTriggered("console"); });
        connect(action6, &QAction::triggered, [this]() { Q_EMIT menuActionTriggered("settings"); });
    }
}

void EnhancedThemeManager::applyTheme(const QString& themeName)
{
    if (!m_themes.contains(themeName)) return;
    
    m_currentTheme = themeName;
    updateMenuBarForTheme(themeName);
    
    // Apply the layout transformation first
    ThemeInfo theme = m_themes[themeName];
    applyLayout(theme.layout);
    
    // Then apply the CSS styling
    QString css = getThemeCSS(themeName);
    qApp->setStyleSheet(css);
    
    // Only emit if there are connected slots to prevent boost signals error
    if (receivers(SIGNAL(themeChanged(QString))) > 0) {
        Q_EMIT themeChanged(themeName);
    }
}

void EnhancedThemeManager::cycleTheme()
{
    QStringList themeNames = m_themes.keys();
    m_themeIndex = (m_themeIndex + 1) % themeNames.size();
    applyTheme(themeNames[m_themeIndex]);
}

QStringList EnhancedThemeManager::getAvailableThemes() const
{
    return m_themes.keys();
}

ThemeInfo EnhancedThemeManager::getThemeInfo(const QString& themeName) const
{
    return m_themes.value(themeName);
}

QString EnhancedThemeManager::getCurrentTheme() const
{
    return m_currentTheme;
}

QString EnhancedThemeManager::getThemeCSS(const QString& themeName) const
{
    ThemeInfo theme = m_themes[themeName];
    QString css;
    
    if (themeName == "cyberpunk") {
        css = QString(R"(
            QWidget#modernContainer {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0a0a0a, stop:1 #1a1a1a);
                color: %1;
                font-family: 'Courier New', monospace;
            }
            QWidget#modernNavigation {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0d1117, stop:1 #161b22);
                border-right: 2px solid %1;
            }
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 #00cc00);
                border: 1px solid %1;
                border-radius: 6px;
                padding: 12px 20px;
                color: #000000;
                font-weight: 600;
                font-size: 14px;
                font-family: 'Courier New', monospace;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %1);
                box-shadow: 0 0 10px %1;
            }
            QMenuBar {
                background-color: #0a0a0a;
                color: %1;
                border-bottom: 1px solid %1;
                font-family: 'Courier New', monospace;
            }
            QMenuBar::item {
                background-color: transparent;
                padding: 8px 16px;
                color: %1;
            }
            QMenuBar::item:selected {
                background-color: %1;
                color: #000000;
            }
            QMenu {
                background-color: #0a0a0a;
                color: %1;
                border: 1px solid %1;
                font-family: 'Courier New', monospace;
            }
            QMenu::item {
                padding: 8px 16px;
                color: %1;
            }
            QMenu::item:selected {
                background-color: %1;
                color: #000000;
            }
            QWidget#customTitleBar {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2);
                border-top-left-radius: 10px;
                border-top-right-radius: 10px;
            }
            QPushButton#titleBarButton {
                background-color: rgba(255, 255, 255, 0.1);
                color: #ffffff;
                border: none;
                border-radius: 3px;
                font-weight: bold;
                font-size: 14px;
            }
            QPushButton#titleBarButton:hover {
                background-color: rgba(255, 255, 255, 0.2);
            }
            QPushButton#titleBarButton:pressed {
                background-color: rgba(255, 255, 255, 0.3);
            }
        )").arg(theme.primaryColor).arg(theme.secondaryColor);
    }
    else if (themeName == "neon") {
        css = QString(R"(
            QWidget#modernContainer {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0a0a0a, stop:1 #1a0a2e);
                color: %1;
            }
            QWidget#modernNavigation {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #16213e, stop:1 #0f3460);
                border-right: 2px solid %1;
            }
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 #e60073);
                border: 1px solid %1;
                border-radius: 8px;
                padding: 12px 20px;
                color: #ffffff;
                font-weight: 600;
                font-size: 14px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %1);
                box-shadow: 0 0 15px %1;
            }
            QMenuBar {
                background-color: #0a0a0a;
                color: %1;
                border-bottom: 1px solid %1;
            }
            QMenuBar::item {
                background-color: transparent;
                padding: 8px 16px;
                color: %1;
            }
            QMenuBar::item:selected {
                background-color: %1;
                color: #000000;
            }
            QMenu {
                background-color: #0a0a0a;
                color: %1;
                border: 1px solid %1;
            }
            QMenu::item {
                padding: 8px 16px;
                color: %1;
            }
            QMenu::item:selected {
                background-color: %1;
                color: #000000;
            }
        )").arg(theme.primaryColor);
    }
    else if (themeName == "futuristic") {
        css = QString(R"(
            QWidget#modernContainer {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #001122, stop:1 #003366);
                color: %1;
                font-family: 'Arial', sans-serif;
            }
            QWidget#modernNavigation {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #002244, stop:1 #004488);
                border-right: 2px solid %1;
            }
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 #0099cc);
                border: 1px solid %1;
                border-radius: 4px;
                padding: 10px 18px;
                color: #ffffff;
                font-weight: 500;
                font-size: 13px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %1);
                box-shadow: 0 0 8px %1;
            }
            QMenuBar {
                background-color: #001122;
                color: %1;
                border-bottom: 1px solid %1;
            }
            QMenuBar::item {
                background-color: transparent;
                padding: 8px 16px;
                color: %1;
            }
            QMenuBar::item:selected {
                background-color: %1;
                color: #000000;
            }
            QMenu {
                background-color: #001122;
                color: %1;
                border: 1px solid %1;
            }
            QMenu::item {
                padding: 8px 16px;
                color: %1;
            }
            QMenu::item:selected {
                background-color: %1;
                color: #000000;
            }
        )").arg(theme.primaryColor);
    }
    else {
        // Default styling for other themes
        css = QString(R"(
            QWidget#modernContainer {
                background-color: %1;
                color: %2;
            }
            QWidget#modernNavigation {
                background-color: %3;
                border-right: 1px solid %4;
            }
            QPushButton {
                background-color: transparent;
                border: none;
                padding: 15px 20px;
                text-align: left;
                color: %2;
                font-size: 14px;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: %4;
                color: %2;
            }
            QMenuBar {
                background-color: %1;
                color: %2;
                border-bottom: 1px solid %4;
            }
            QMenuBar::item {
                background-color: transparent;
                padding: 8px 16px;
                color: %2;
            }
            QMenuBar::item:selected {
                background-color: %4;
                color: %2;
            }
            QMenu {
                background-color: %1;
                color: %2;
                border: 1px solid %4;
            }
            QMenu::item {
                padding: 8px 16px;
                color: %2;
            }
            QMenu::item:selected {
                background-color: %4;
                color: %2;
            }
        )").arg(theme.backgroundColor).arg(theme.textColor).arg(theme.secondaryColor).arg(theme.accentColor);
    }
    
    return css;
}

void EnhancedThemeManager::applyLayout(const QString& layoutType)
{
    // Apply different layout structures based on theme
    if (layoutType == "cards") {
        // Create floating card-based layout
        createFloatingCardLayout();
    } else if (layoutType == "minimal") {
        // Create ultra-minimal space-age layout
        createSpaceAgeLayout();
    } else if (layoutType == "tabs") {
        // Create modern dashboard layout
        createDashboardLayout();
    } else if (layoutType == "cyberpunk") {
        // Create matrix-style terminal layout
        createMatrixLayout();
    } else if (layoutType == "neon") {
        // Create holographic interface
        createHolographicLayout();
    } else {
        // Default enhanced sidebar layout
        createEnhancedSidebarLayout();
    }
}

void EnhancedThemeManager::createCardLayout()
{
    // Card-based futuristic layout
    if (!m_parent) return;
    
    // Find the main container
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    // Apply card-based styling
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernContainer {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0a0a0a, stop:0.5 #1a1a2e, stop:1 #16213e);
        }
        QWidget#modernNavigation {
            background: rgba(255, 0, 128, 0.1);
            border: 2px solid #ff0080;
            border-radius: 15px;
            margin: 10px;
        }
        QPushButton {
            background: rgba(255, 0, 128, 0.2);
            border: 1px solid #ff0080;
            border-radius: 10px;
            padding: 15px 20px;
            margin: 5px;
            color: #ff0080;
            font-weight: 600;
            font-size: 14px;
        }
        QPushButton:hover {
            background: rgba(255, 0, 128, 0.4);
            box-shadow: 0 0 20px #ff0080;
            transform: scale(1.05);
        }
    )");
}

void EnhancedThemeManager::createMinimalLayout()
{
    // Ultra-minimal futuristic layout
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernContainer {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #001122, stop:1 #003366);
        }
        QWidget#modernNavigation {
            background: transparent;
            border: none;
            margin: 5px;
        }
        QPushButton {
            background: transparent;
            border: 1px solid #00d4ff;
            border-radius: 3px;
            padding: 8px 12px;
            margin: 2px;
            color: #00d4ff;
            font-size: 12px;
            font-weight: 300;
        }
        QPushButton:hover {
            background: rgba(0, 212, 255, 0.1);
            box-shadow: 0 0 10px #00d4ff;
        }
    )");
}

void EnhancedThemeManager::createTabLayout()
{
    // Tabbed interface layout
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernContainer {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ffffff, stop:1 #f5f5f5);
        }
        QWidget#modernNavigation {
            background: #e8e8e8;
            border-bottom: 2px solid #0078d4;
            border-radius: 0px;
            margin: 0px;
        }
        QPushButton {
            background: transparent;
            border: none;
            border-bottom: 3px solid transparent;
            padding: 12px 20px;
            margin: 0px;
            color: #333333;
            font-size: 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: #d0d0d0;
            border-bottom: 3px solid #0078d4;
        }
    )");
}

void EnhancedThemeManager::createSidebarLayout()
{
    // Default sidebar layout with enhanced styling
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    // Apply enhanced sidebar styling based on current theme
    QString themeName = getCurrentTheme();
    if (themeName == "cyberpunk") {
        container->setStyleSheet(container->styleSheet() + R"(
            QWidget#modernNavigation {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0d1117, stop:1 #161b22);
                border-right: 3px solid #00ff00;
                border-radius: 0px;
            }
            QPushButton {
                background: rgba(0, 255, 0, 0.1);
                border: 1px solid #00ff00;
                border-radius: 8px;
                padding: 12px 20px;
                margin: 3px;
                color: #00ff00;
                font-family: 'Courier New', monospace;
                font-weight: 600;
                font-size: 13px;
            }
            QPushButton:hover {
                background: rgba(0, 255, 0, 0.3);
                box-shadow: 0 0 15px #00ff00;
                transform: translateX(5px);
            }
        )");
    }
}

void EnhancedThemeManager::createFloatingCardLayout()
{
    // Revolutionary floating card layout for Neon theme
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    // Apply floating card styling with radical transformations
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernContainer {
            background: qradialgradient(cx:0.5, cy:0.5, radius:1, stop:0 #0a0a0a, stop:0.7 #1a1a2e, stop:1 #16213e);
            border-radius: 20px;
        }
        QWidget#modernNavigation {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(255,0,128,0.2), stop:1 rgba(0,255,255,0.2));
            border: 3px solid #ff0080;
            border-radius: 25px;
            margin: 15px;
            box-shadow: 0 0 30px rgba(255,0,128,0.5);
        }
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,0,128,0.3), stop:1 rgba(0,255,255,0.3));
            border: 2px solid #ff0080;
            border-radius: 15px;
            padding: 18px 25px;
            margin: 8px;
            color: #ff0080;
            font-weight: 700;
            font-size: 16px;
            text-shadow: 0 0 10px #ff0080;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,0,128,0.6), stop:1 rgba(0,255,255,0.6));
            box-shadow: 0 0 25px #ff0080, 0 0 50px rgba(255,0,128,0.3);
            transform: scale(1.1) translateY(-5px);
        }
        QPushButton:pressed {
            transform: scale(0.95);
        }
    )");
}

void EnhancedThemeManager::createSpaceAgeLayout()
{
    // Ultra-minimal space-age layout for Futuristic theme
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernContainer {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #001122, stop:0.5 #002244, stop:1 #003366);
            border: 1px solid #00d4ff;
        }
        QWidget#modernNavigation {
            background: transparent;
            border: 1px solid #00d4ff;
            border-radius: 0px;
            margin: 0px;
        }
        QPushButton {
            background: transparent;
            border: 1px solid #00d4ff;
            border-radius: 0px;
            padding: 12px 20px;
            margin: 1px;
            color: #00d4ff;
            font-size: 11px;
            font-weight: 300;
            font-family: 'Courier New', monospace;
            text-transform: uppercase;
            letter-spacing: 2px;
        }
        QPushButton:hover {
            background: rgba(0, 212, 255, 0.1);
            box-shadow: 0 0 15px #00d4ff;
            border-left: 3px solid #00d4ff;
        }
        QPushButton:pressed {
            background: rgba(0, 212, 255, 0.2);
        }
    )");
}

void EnhancedThemeManager::createDashboardLayout()
{
    // Modern dashboard layout for Light theme
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernContainer {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ffffff, stop:0.5 #f8f9fa, stop:1 #e9ecef);
            border-radius: 0px;
        }
        QWidget#modernNavigation {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:1 #f1f3f4);
            border-right: 3px solid #0078d4;
            border-radius: 0px;
            margin: 0px;
        }
        QPushButton {
            background: transparent;
            border: none;
            border-bottom: 2px solid transparent;
            padding: 15px 25px;
            margin: 0px;
            color: #333333;
            font-size: 15px;
            font-weight: 500;
            text-align: left;
        }
        QPushButton:hover {
            background: rgba(0, 120, 212, 0.1);
            border-bottom: 2px solid #0078d4;
            color: #0078d4;
        }
        QPushButton:pressed {
            background: rgba(0, 120, 212, 0.2);
        }
    )");
}

void EnhancedThemeManager::createMatrixLayout()
{
    // Matrix-style terminal layout for Cyberpunk theme
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernContainer {
            background: #000000;
            border: 2px solid #00ff00;
            border-radius: 0px;
        }
        QWidget#modernNavigation {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #001100, stop:1 #002200);
            border-right: 3px solid #00ff00;
            border-radius: 0px;
            margin: 0px;
        }
        QPushButton {
            background: rgba(0, 255, 0, 0.1);
            border: 1px solid #00ff00;
            border-radius: 0px;
            padding: 12px 20px;
            margin: 2px;
            color: #00ff00;
            font-family: 'Courier New', monospace;
            font-weight: 600;
            font-size: 12px;
            text-shadow: 0 0 5px #00ff00;
        }
        QPushButton:hover {
            background: rgba(0, 255, 0, 0.3);
            box-shadow: 0 0 20px #00ff00, inset 0 0 20px rgba(0,255,0,0.1);
            text-shadow: 0 0 10px #00ff00;
        }
        QPushButton:pressed {
            background: rgba(0, 255, 0, 0.5);
            text-shadow: 0 0 15px #00ff00;
        }
    )");
}

void EnhancedThemeManager::createHolographicLayout()
{
    // Holographic interface layout for special effects
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernContainer {
            background: qradialgradient(cx:0.5, cy:0.5, radius:1, stop:0 #0a0a0a, stop:0.5 #1a0a2e, stop:1 #16213e);
            border: 2px solid #ff0080;
            border-radius: 30px;
        }
        QWidget#modernNavigation {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(255,0,128,0.3), stop:1 rgba(0,255,255,0.3));
            border: 2px solid #ff0080;
            border-radius: 20px;
            margin: 10px;
        }
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,0,128,0.4), stop:1 rgba(0,255,255,0.4));
            border: 1px solid #ff0080;
            border-radius: 12px;
            padding: 15px 20px;
            margin: 5px;
            color: #ffffff;
            font-weight: 600;
            font-size: 14px;
            text-shadow: 0 0 8px #ff0080;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255,0,128,0.7), stop:1 rgba(0,255,255,0.7));
            box-shadow: 0 0 20px #ff0080, 0 0 40px rgba(255,0,128,0.5);
            transform: scale(1.05);
        }
    )");
}

void EnhancedThemeManager::createEnhancedSidebarLayout()
{
    // Enhanced sidebar layout with advanced styling
    if (!m_parent) return;
    
    QWidget* container = m_parent->findChild<QWidget*>("modernContainer");
    if (!container) return;
    
    container->setStyleSheet(container->styleSheet() + R"(
        QWidget#modernNavigation {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2d2d2d, stop:1 #404040);
            border-right: 2px solid #666666;
            border-radius: 0px;
        }
        QPushButton {
            background: transparent;
            border: none;
            border-left: 3px solid transparent;
            padding: 15px 20px;
            margin: 0px;
            color: #cccccc;
            font-size: 14px;
            font-weight: 500;
            text-align: left;
        }
        QPushButton:hover {
            background: rgba(255, 255, 255, 0.1);
            border-left: 3px solid #0078d4;
            color: #ffffff;
        }
        QPushButton:pressed {
            background: rgba(255, 255, 255, 0.2);
            border-left: 3px solid #106ebe;
        }
    )");
}

void EnhancedThemeManager::onMenuAction()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action && receivers(SIGNAL(menuActionTriggered(QString))) > 0) {
        Q_EMIT menuActionTriggered(action->text());
    }
}

// MOC will be generated automatically
