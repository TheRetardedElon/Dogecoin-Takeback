// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "thememanager.h"
#include "guiconstants.h"

#include <QApplication>
#include <QSettings>
#include <QStyleFactory>
#include <QPalette>
#include <QWidget>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QIODevice>

ThemeManager* ThemeManager::m_instance = nullptr;

ThemeManager* ThemeManager::instance()
{
    if (!m_instance) {
        m_instance = new ThemeManager();
    }
    return m_instance;
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_currentTheme(Light)
    , m_useCustomFont(false)
{
    initializeThemes();
    updateColors();
}

void ThemeManager::initializeThemes()
{
    // Initialize built-in themes
    m_builtInThemes[Light] = getLightThemeColors();
    m_builtInThemes[Dark] = getDarkThemeColors();
    m_builtInThemes[Dogecoin] = getDogecoinThemeColors();
    m_builtInThemes[Neon] = getNeonThemeColors();
    m_builtInThemes[Classic] = getClassicThemeColors();
}

ThemeManager::ThemeColors ThemeManager::getLightThemeColors() const
{
    ThemeColors colors;
    
    // Background colors - Light theme
    colors.primaryBackground = QColor(255, 255, 255);      // Pure white
    colors.secondaryBackground = QColor(248, 249, 250);    // Light gray
    colors.tertiaryBackground = QColor(241, 243, 245);    // Slightly darker gray
    
    // Text colors
    colors.primaryText = QColor(33, 37, 41);               // Dark gray
    colors.secondaryText = QColor(108, 117, 125);           // Medium gray
    colors.tertiaryText = QColor(134, 142, 150);            // Light gray
    colors.accentText = QColor(0, 123, 255);                // Blue accent
    
    // Accent colors
    colors.primaryAccent = QColor(0, 123, 255);            // Blue
    colors.secondaryAccent = QColor(108, 117, 125);       // Gray
    colors.successColor = QColor(40, 167, 69);             // Green
    colors.warningColor = QColor(255, 193, 7);             // Yellow
    colors.errorColor = QColor(220, 53, 69);               // Red
    colors.infoColor = QColor(23, 162, 184);               // Cyan
    
    // Border colors
    colors.primaryBorder = QColor(222, 226, 230);          // Light border
    colors.secondaryBorder = QColor(206, 212, 218);       // Medium border
    
    // Shadow colors
    colors.shadowColor = QColor(0, 0, 0, 25);             // Semi-transparent black
    
    // Button colors
    colors.buttonBackground = QColor(0, 123, 255);        // Blue button
    colors.buttonHover = QColor(0, 105, 217);             // Darker blue
    colors.buttonPressed = QColor(0, 86, 179);            // Even darker blue
    colors.buttonText = QColor(255, 255, 255);            // White text
    
    // Input field colors
    colors.inputBackground = QColor(255, 255, 255);       // White
    colors.inputBorder = QColor(206, 212, 218);           // Light border
    colors.inputFocus = QColor(0, 123, 255);              // Blue focus
    
    // Card colors
    colors.cardBackground = QColor(255, 255, 255);        // White
    colors.cardBorder = QColor(222, 226, 230);            // Light border
    colors.cardShadow = QColor(0, 0, 0, 10);             // Very light shadow
    
    return colors;
}

ThemeManager::ThemeColors ThemeManager::getDarkThemeColors() const
{
    ThemeColors colors;
    
    // Background colors - Dark theme
    colors.primaryBackground = QColor(18, 18, 18);        // Very dark gray
    colors.secondaryBackground = QColor(33, 37, 41);      // Dark gray
    colors.tertiaryBackground = QColor(52, 58, 64);        // Medium dark gray
    
    // Text colors
    colors.primaryText = QColor(248, 249, 250);            // Light gray
    colors.secondaryText = QColor(173, 181, 189);          // Medium light gray
    colors.tertiaryText = QColor(134, 142, 150);           // Medium gray
    colors.accentText = QColor(0, 123, 255);               // Blue accent
    
    // Accent colors
    colors.primaryAccent = QColor(0, 123, 255);           // Blue
    colors.secondaryAccent = QColor(173, 181, 189);       // Light gray
    colors.successColor = QColor(40, 167, 69);            // Green
    colors.warningColor = QColor(255, 193, 7);             // Yellow
    colors.errorColor = QColor(220, 53, 69);               // Red
    colors.infoColor = QColor(23, 162, 184);               // Cyan
    
    // Border colors
    colors.primaryBorder = QColor(52, 58, 64);             // Dark border
    colors.secondaryBorder = QColor(73, 80, 87);           // Medium dark border
    
    // Shadow colors
    colors.shadowColor = QColor(0, 0, 0, 50);             // Semi-transparent black
    
    // Button colors
    colors.buttonBackground = QColor(0, 123, 255);        // Blue button
    colors.buttonHover = QColor(0, 105, 217);              // Darker blue
    colors.buttonPressed = QColor(0, 86, 179);             // Even darker blue
    colors.buttonText = QColor(255, 255, 255);            // White text
    
    // Input field colors
    colors.inputBackground = QColor(33, 37, 41);          // Dark background
    colors.inputBorder = QColor(73, 80, 87);              // Dark border
    colors.inputFocus = QColor(0, 123, 255);               // Blue focus
    
    // Card colors
    colors.cardBackground = QColor(33, 37, 41);           // Dark background
    colors.cardBorder = QColor(52, 58, 64);                 // Dark border
    colors.cardShadow = QColor(0, 0, 0, 30);              // Dark shadow
    
    return colors;
}

ThemeManager::ThemeColors ThemeManager::getDogecoinThemeColors() const
{
    ThemeColors colors;
    
    // Background colors - Dogecoin theme (warm, friendly colors)
    colors.primaryBackground = QColor(255, 248, 220);        // Cream/beige
    colors.secondaryBackground = QColor(245, 235, 200);      // Light golden
    colors.tertiaryBackground = QColor(235, 220, 180);       // Medium golden
    
    // Text colors
    colors.primaryText = QColor(139, 69, 19);                // Saddle brown
    colors.secondaryText = QColor(160, 82, 45);              // Sienna
    colors.tertiaryText = QColor(205, 133, 63);              // Peru
    colors.accentText = QColor(255, 140, 0);                 // Dark orange
    
    // Accent colors - Dogecoin orange/gold theme
    colors.primaryAccent = QColor(255, 140, 0);             // Dark orange
    colors.secondaryAccent = QColor(255, 165, 0);           // Orange
    colors.successColor = QColor(50, 205, 50);              // Lime green
    colors.warningColor = QColor(255, 215, 0);              // Gold
    colors.errorColor = QColor(220, 20, 60);                // Crimson
    colors.infoColor = QColor(30, 144, 255);                // Dodger blue
    
    // Border colors
    colors.primaryBorder = QColor(255, 165, 0);             // Orange border
    colors.secondaryBorder = QColor(255, 140, 0);           // Dark orange border
    
    // Shadow colors
    colors.shadowColor = QColor(255, 140, 0, 30);          // Orange shadow
    
    // Button colors
    colors.buttonBackground = QColor(255, 140, 0);         // Dark orange
    colors.buttonHover = QColor(255, 165, 0);              // Orange
    colors.buttonPressed = QColor(255, 69, 0);             // Red orange
    colors.buttonText = QColor(255, 255, 255);             // White text
    
    // Input field colors
    colors.inputBackground = QColor(255, 248, 220);        // Cream
    colors.inputBorder = QColor(255, 165, 0);              // Orange border
    colors.inputFocus = QColor(255, 140, 0);               // Dark orange focus
    
    // Card colors
    colors.cardBackground = QColor(255, 248, 220);         // Cream
    colors.cardBorder = QColor(255, 165, 0);               // Orange border
    colors.cardShadow = QColor(255, 140, 0, 20);          // Orange shadow
    
    return colors;
}

ThemeManager::ThemeColors ThemeManager::getNeonThemeColors() const
{
    ThemeColors colors;
    
    // Background colors - High contrast neon theme
    colors.primaryBackground = QColor(0, 0, 0);              // Pure black
    colors.secondaryBackground = QColor(20, 20, 20);         // Very dark gray
    colors.tertiaryBackground = QColor(40, 40, 40);          // Dark gray
    
    // Text colors
    colors.primaryText = QColor(0, 255, 0);                  // Bright green
    colors.secondaryText = QColor(0, 255, 255);              // Cyan
    colors.tertiaryText = QColor(255, 255, 0);               // Yellow
    colors.accentText = QColor(255, 0, 255);                 // Magenta
    
    // Accent colors - Bright neon colors
    colors.primaryAccent = QColor(0, 255, 0);               // Bright green
    colors.secondaryAccent = QColor(255, 0, 255);           // Magenta
    colors.successColor = QColor(0, 255, 0);                // Green
    colors.warningColor = QColor(255, 255, 0);              // Yellow
    colors.errorColor = QColor(255, 0, 0);                  // Red
    colors.infoColor = QColor(0, 255, 255);                 // Cyan
    
    // Border colors
    colors.primaryBorder = QColor(0, 255, 0);               // Green border
    colors.secondaryBorder = QColor(255, 0, 255);           // Magenta border
    
    // Shadow colors
    colors.shadowColor = QColor(0, 255, 0, 50);            // Green glow
    
    // Button colors
    colors.buttonBackground = QColor(0, 255, 0);           // Green button
    colors.buttonHover = QColor(255, 255, 0);              // Yellow hover
    colors.buttonPressed = QColor(255, 0, 255);            // Magenta pressed
    colors.buttonText = QColor(0, 0, 0);                   // Black text
    
    // Input field colors
    colors.inputBackground = QColor(20, 20, 20);           // Dark background
    colors.inputBorder = QColor(0, 255, 0);                // Green border
    colors.inputFocus = QColor(255, 0, 255);               // Magenta focus
    
    // Card colors
    colors.cardBackground = QColor(20, 20, 20);            // Dark background
    colors.cardBorder = QColor(0, 255, 0);                 // Green border
    colors.cardShadow = QColor(0, 255, 0, 30);             // Green glow
    
    return colors;
}

ThemeManager::ThemeColors ThemeManager::getClassicThemeColors() const
{
    ThemeColors colors;
    
    // Background colors - Classic Bitcoin orange theme
    colors.primaryBackground = QColor(248, 249, 250);       // Light gray
    colors.secondaryBackground = QColor(233, 236, 239);     // Medium light gray
    colors.tertiaryBackground = QColor(206, 212, 218);      // Light gray
    
    // Text colors
    colors.primaryText = QColor(33, 37, 41);                // Dark gray
    colors.secondaryText = QColor(73, 80, 87);              // Medium gray
    colors.tertiaryText = QColor(134, 142, 150);            // Light gray
    colors.accentText = QColor(255, 140, 0);                // Bitcoin orange
    
    // Accent colors - Bitcoin orange theme
    colors.primaryAccent = QColor(255, 140, 0);            // Bitcoin orange
    colors.secondaryAccent = QColor(255, 165, 0);          // Orange
    colors.successColor = QColor(40, 167, 69);             // Green
    colors.warningColor = QColor(255, 193, 7);             // Yellow
    colors.errorColor = QColor(220, 53, 69);               // Red
    colors.infoColor = QColor(23, 162, 184);               // Blue
    
    // Border colors
    colors.primaryBorder = QColor(255, 140, 0);            // Orange border
    colors.secondaryBorder = QColor(255, 165, 0);          // Light orange border
    
    // Shadow colors
    colors.shadowColor = QColor(255, 140, 0, 20);         // Orange shadow
    
    // Button colors
    colors.buttonBackground = QColor(255, 140, 0);        // Orange button
    colors.buttonHover = QColor(255, 165, 0);             // Light orange hover
    colors.buttonPressed = QColor(255, 69, 0);            // Red orange pressed
    colors.buttonText = QColor(255, 255, 255);            // White text
    
    // Input field colors
    colors.inputBackground = QColor(255, 255, 255);       // White background
    colors.inputBorder = QColor(255, 140, 0);             // Orange border
    colors.inputFocus = QColor(255, 165, 0);              // Light orange focus
    
    // Card colors
    colors.cardBackground = QColor(255, 255, 255);        // White background
    colors.cardBorder = QColor(255, 140, 0);              // Orange border
    colors.cardShadow = QColor(255, 140, 0, 15);         // Orange shadow
    
    return colors;
}

void ThemeManager::setTheme(ThemeType theme)
{
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        updateColors();
        Q_EMIT themeChanged(theme);
        Q_EMIT colorsChanged();
    }
}

QString ThemeManager::currentThemeName() const
{
    switch (m_currentTheme) {
        case Light: return "Light";
        case Dark: return "Dark";
        case Dogecoin: return "Dogecoin";
        case Neon: return "Neon";
        case Classic: return "Classic";
        case Custom: return "Custom";
        default: return "Unknown";
    }
}

QColor ThemeManager::color(const QString& colorName) const
{
    if (colorName == "primaryBackground") return m_currentColors.primaryBackground;
    if (colorName == "secondaryBackground") return m_currentColors.secondaryBackground;
    if (colorName == "tertiaryBackground") return m_currentColors.tertiaryBackground;
    if (colorName == "primaryText") return m_currentColors.primaryText;
    if (colorName == "secondaryText") return m_currentColors.secondaryText;
    if (colorName == "tertiaryText") return m_currentColors.tertiaryText;
    if (colorName == "accentText") return m_currentColors.accentText;
    if (colorName == "primaryAccent") return m_currentColors.primaryAccent;
    if (colorName == "secondaryAccent") return m_currentColors.secondaryAccent;
    if (colorName == "successColor") return m_currentColors.successColor;
    if (colorName == "warningColor") return m_currentColors.warningColor;
    if (colorName == "errorColor") return m_currentColors.errorColor;
    if (colorName == "infoColor") return m_currentColors.infoColor;
    if (colorName == "primaryBorder") return m_currentColors.primaryBorder;
    if (colorName == "secondaryBorder") return m_currentColors.secondaryBorder;
    if (colorName == "shadowColor") return m_currentColors.shadowColor;
    if (colorName == "buttonBackground") return m_currentColors.buttonBackground;
    if (colorName == "buttonHover") return m_currentColors.buttonHover;
    if (colorName == "buttonPressed") return m_currentColors.buttonPressed;
    if (colorName == "buttonText") return m_currentColors.buttonText;
    if (colorName == "inputBackground") return m_currentColors.inputBackground;
    if (colorName == "inputBorder") return m_currentColors.inputBorder;
    if (colorName == "inputFocus") return m_currentColors.inputFocus;
    if (colorName == "cardBackground") return m_currentColors.cardBackground;
    if (colorName == "cardBorder") return m_currentColors.cardBorder;
    if (colorName == "cardShadow") return m_currentColors.cardShadow;
    
    return QColor(); // Return invalid color if not found
}

ThemeManager::ThemeColors ThemeManager::getCurrentColors() const
{
    return m_currentColors;
}

void ThemeManager::switchToLight()
{
    setTheme(Light);
}

void ThemeManager::switchToDark()
{
    setTheme(Dark);
}

void ThemeManager::switchToAuto()
{
    setTheme(Light); // Fallback to Light theme since Auto was removed
}

void ThemeManager::switchToTheme(ThemeType theme)
{
    setTheme(theme);
    updateColors();
    
    // Apply the theme globally to the entire application
    if (qApp) {
        qApp->setStyleSheet(getStylesheet());
    }
    
    // Don't emit signal to prevent infinite recursion with ThemeSwitcher
    // Q_EMIT themeChanged(theme);
}

void ThemeManager::switchToCustom(const QString& themeName)
{
    if (m_customThemes.contains(themeName)) {
        setTheme(Custom);
        m_currentColors = m_customThemes[themeName];
        Q_EMIT colorsChanged();
    }
}

void ThemeManager::loadCustomTheme(const QString& themeName, const ThemeColors& colors)
{
    m_customThemes[themeName] = colors;
}

void ThemeManager::saveCustomTheme(const QString& themeName, const ThemeColors& colors)
{
    m_customThemes[themeName] = colors;
    // TODO: Save to settings
}

QStringList ThemeManager::availableThemes() const
{
    QStringList themes;
    themes << "Dogecoin" << "Neon" << "Classic";
    
    // Multiple fallback paths for finding themes directory
    QStringList possiblePaths;
    
    // 1. Relative to application directory
    QString appDir = QCoreApplication::applicationDirPath();
    possiblePaths << appDir + "/../src/qt/themes";
    
    // 2. Relative to current working directory
    possiblePaths << "src/qt/themes";
    
    // 3. Relative to executable location (for development)
    possiblePaths << appDir + "/../../src/qt/themes";
    
    // 4. Absolute path fallback
    possiblePaths << "/mnt/d/dogecoin-master/src/qt/themes";
    
    qDebug() << "Application directory:" << appDir;
    qDebug() << "Current working directory:" << QDir::currentPath();
    
    bool themesFound = false;
    for (const QString& themesPath : possiblePaths) {
        QDir themesDir(themesPath);
        qDebug() << "Checking themes path:" << themesPath << "exists:" << themesDir.exists();
        
        if (themesDir.exists()) {
            QStringList themeDirs = themesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            qDebug() << "Found theme directories:" << themeDirs;
            
            for (const QString& dir : themeDirs) {
                QString cssFile = themesDir.absoluteFilePath(dir + "/" + dir + ".css");
                if (QFile::exists(cssFile)) {
                    themes << dir;
                    qDebug() << "Found CSS theme:" << dir << "at" << cssFile;
                    themesFound = true;
                } else {
                    qDebug() << "CSS file missing for theme:" << dir << "expected at:" << cssFile;
                }
            }
            break; // Use first valid path
        }
    }
    
    if (!themesFound) {
        qDebug() << "No themes directory found in any of the checked paths";
    }
    
    themes << m_customThemes.keys();
    qDebug() << "Final available themes:" << themes;
    return themes;
}

void ThemeManager::setFontFamily(const QString& family)
{
    m_customFont.setFamily(family);
    m_useCustomFont = true;
}

void ThemeManager::setFontSize(int size)
{
    m_customFont.setPointSize(size);
    m_useCustomFont = true;
}

QFont ThemeManager::getFont() const
{
    if (m_useCustomFont) {
        return m_customFont;
    }
    return QApplication::font();
}

void ThemeManager::updateColors()
{
    switch (m_currentTheme) {
        case Light:
            m_currentColors = m_builtInThemes[Light];
            break;
        case Dark:
            m_currentColors = m_builtInThemes[Dark];
            break;
        case Dogecoin:
            m_currentColors = m_builtInThemes[Dogecoin];
            break;
        case Neon:
            m_currentColors = m_builtInThemes[Neon];
            break;
        case Classic:
            m_currentColors = m_builtInThemes[Classic];
            break;
        case Custom:
            // Custom colors should already be set
            break;
    }
}

void ThemeManager::applyTheme(QApplication* app)
{
    if (!app) return;
    
    // Apply font
    if (m_useCustomFont) {
        app->setFont(m_customFont);
    }
    
    // Apply stylesheet
    app->setStyleSheet(getStylesheet());
}

void ThemeManager::applyTheme(QWidget* widget)
{
    if (!widget) return;
    
    widget->setStyleSheet(getStylesheet());
}

void ThemeManager::loadCSSTheme(const QString& themeName)
{
    // Multiple fallback paths for finding CSS file
    QStringList possiblePaths;
    
    // Handle template themes (remove "template_" prefix)
    QString actualThemeName = themeName;
    QString themeDir = themeName;
    if (themeName.startsWith("template_")) {
        actualThemeName = themeName.mid(9); // Remove "template_" prefix
        themeDir = actualThemeName;
    }
    
    QString appDir = QCoreApplication::applicationDirPath();
    
    // 1. Custom themes directory
    possiblePaths << appDir + "/themes/custom/" + themeDir + "/" + actualThemeName + ".css";
    
    // 2. Template themes directory  
    possiblePaths << appDir + "/themes/templates/" + themeDir + "/" + actualThemeName + ".css";
    
    // 3. Development paths (relative to source)
    possiblePaths << QString("src/qt/themes/custom/%1/%2.css").arg(themeDir).arg(actualThemeName);
    possiblePaths << QString("src/qt/themes/templates/%1/%2.css").arg(themeDir).arg(actualThemeName);
    
    // 4. Existing CSS themes (Matrix, Cyberpunk, etc.)
    possiblePaths << QString("src/qt/themes/%1/%1.css").arg(actualThemeName);
    possiblePaths << appDir + "/themes/" + actualThemeName + "/" + actualThemeName + ".css";
    
    // 5. Absolute path fallbacks for development
    possiblePaths << QString("/mnt/d/dogecoin-master/src/qt/themes/custom/%1/%2.css").arg(themeDir).arg(actualThemeName);
    possiblePaths << QString("/mnt/d/dogecoin-master/src/qt/themes/templates/%1/%2.css").arg(themeDir).arg(actualThemeName);
    possiblePaths << QString("/mnt/d/dogecoin-master/src/qt/themes/%1/%1.css").arg(actualThemeName);
    
    QString cssPath;
    QFile cssFile;
    
    // Try each path until we find the CSS file
    for (const QString& path : possiblePaths) {
        cssFile.setFileName(path);
        qDebug() << "Checking CSS path:" << path;
        if (cssFile.exists()) {
            cssPath = path;
            qDebug() << "Found CSS file at:" << path;
            break;
        }
    }
    
    if (cssPath.isEmpty()) {
        qDebug() << "Failed to find CSS theme:" << themeName << "in any of the checked paths";
        qDebug() << "Checked paths:" << possiblePaths;
        return;
    }
    
    if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&cssFile);
        QString cssContent = stream.readAll();
        cssFile.close();
        
        // Apply CSS theme globally to the application
        qApp->setStyleSheet(cssContent);
        
        // Set theme type to Custom (don't emit signal to prevent crash during initialization)
        m_currentTheme = Custom;
        // Q_EMIT themeChanged(Custom); // Commented out to prevent boost::signals2::no_slots_error
        
        qDebug() << "Successfully loaded CSS theme:" << themeName << "from" << cssPath;
    } else {
        qDebug() << "Failed to open CSS theme file:" << cssPath;
    }
}

QString ThemeManager::getStylesheet() const
{
    QString style = QString(
        "/* Modern Dogecoin Theme Stylesheet */\n"
        "QWidget {\n"
        "    background-color: %1;\n"
        "    color: %2;\n"
        "    font-family: 'Segoe UI', 'Helvetica Neue', Arial, sans-serif;\n"
        "}\n"
        "\n"
        "/* Main Window */\n"
        "QMainWindow {\n"
        "    background-color: %1;\n"
        "    border: none;\n"
        "}\n"
        "\n"
        "/* Cards and Panels */\n"
        "QFrame[frameShape=\"4\"] {\n"  // StyledPanel
        "    background-color: %3;\n"
        "    border: 1px solid %4;\n"
        "    border-radius: 8px;\n"
        "    padding: 12px;\n"
        "}\n"
        "\n"
        "/* Buttons */\n"
        "QPushButton {\n"
        "    background-color: %5;\n"
        "    color: %6;\n"
        "    border: none;\n"
        "    border-radius: 6px;\n"
        "    padding: 8px 16px;\n"
        "    font-weight: 500;\n"
        "    min-height: 20px;\n"
        "}\n"
        "\n"
        "QPushButton:hover {\n"
        "    background-color: %7;\n"
        "}\n"
        "\n"
        "QPushButton:pressed {\n"
        "    background-color: %8;\n"
        "}\n"
        "\n"
        "QPushButton:disabled {\n"
        "    background-color: %9;\n"
        "    color: %10;\n"
        "}\n"
        "\n"
        "/* Input Fields */\n"
        "QLineEdit, QTextEdit, QPlainTextEdit {\n"
        "    background-color: %11;\n"
        "    border: 1px solid %12;\n"
        "    border-radius: 4px;\n"
        "    padding: 6px 8px;\n"
        "    selection-background-color: %13;\n"
        "}\n"
        "\n"
        "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {\n"
        "    border-color: %13;\n"
        "    border-width: 2px;\n"
        "}\n"
        "\n"
        "/* Labels */\n"
        "QLabel {\n"
        "    color: %2;\n"
        "    background: transparent;\n"
        "}\n"
        "\n"
        "/* Status Bar */\n"
        "QStatusBar {\n"
        "    background-color: %14;\n"
        "    border-top: 1px solid %4;\n"
        "    padding: 4px;\n"
        "}\n"
        "\n"
        "/* Menu Bar */\n"
        "QMenuBar {\n"
        "    background-color: %1;\n"
        "    border-bottom: 1px solid %4;\n"
        "    padding: 4px;\n"
        "}\n"
        "\n"
        "QMenuBar::item {\n"
        "    background-color: transparent;\n"
        "    padding: 4px 8px;\n"
        "    border-radius: 4px;\n"
        "}\n"
        "\n"
        "QMenuBar::item:selected {\n"
        "    background-color: %15;\n"
        "}\n"
        "\n"
        "/* Scroll Bars */\n"
        "QScrollBar:vertical {\n"
        "    background-color: %14;\n"
        "    width: 12px;\n"
        "    border-radius: 6px;\n"
        "}\n"
        "\n"
        "QScrollBar::handle:vertical {\n"
        "    background-color: %16;\n"
        "    border-radius: 6px;\n"
        "    min-height: 20px;\n"
        "}\n"
        "\n"
        "QScrollBar::handle:vertical:hover {\n"
        "    background-color: %17;\n"
        "}\n"
        "\n"
        "/* Progress Bar */\n"
        "QProgressBar {\n"
        "    background-color: %14;\n"
        "    border: 1px solid %4;\n"
        "    border-radius: 4px;\n"
        "    text-align: center;\n"
        "}\n"
        "\n"
        "QProgressBar::chunk {\n"
        "    background-color: %5;\n"
        "    border-radius: 3px;\n"
        "}\n"
    ).arg(m_currentColors.primaryBackground.name())
     .arg(m_currentColors.primaryText.name())
     .arg(m_currentColors.cardBackground.name())
     .arg(m_currentColors.primaryBorder.name())
     .arg(m_currentColors.buttonBackground.name())
     .arg(m_currentColors.buttonText.name())
     .arg(m_currentColors.buttonHover.name())
     .arg(m_currentColors.buttonPressed.name())
     .arg(m_currentColors.tertiaryBackground.name())
     .arg(m_currentColors.tertiaryText.name())
     .arg(m_currentColors.inputBackground.name())
     .arg(m_currentColors.inputBorder.name())
     .arg(m_currentColors.inputFocus.name())
     .arg(m_currentColors.secondaryBackground.name())
     .arg(m_currentColors.primaryAccent.name())
     .arg(m_currentColors.secondaryText.name())
     .arg(m_currentColors.primaryText.name());
     
    return style;
}

QStringList ThemeManager::discoverCustomThemes()
{
    QStringList themes;
    
    // Check custom themes directory
    QString customThemesDir = QCoreApplication::applicationDirPath() + "/themes/custom";
    QDir dir(customThemesDir);
    
    if (dir.exists()) {
        QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& folder : folders) {
            QString cssPath = dir.absoluteFilePath(folder + "/" + folder + ".css");
            if (QFile::exists(cssPath)) {
                themes << folder;
            }
        }
    }
    
    // Check templates directory
    QString templatesDir = QCoreApplication::applicationDirPath() + "/themes/templates";
    QDir templatesDirObj(templatesDir);
    
    if (templatesDirObj.exists()) {
        QStringList templateFolders = templatesDirObj.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& folder : templateFolders) {
            QString cssPath = templatesDirObj.absoluteFilePath(folder + "/" + folder + ".css");
            if (QFile::exists(cssPath)) {
                themes << "template_" + folder;
            }
        }
    }
    
    return themes;
}

QStringList ThemeManager::getAvailableThemes()
{
    QStringList themes;
    
    // Add built-in themes
    themes << "Light" << "Dark" << "Dogecoin" << "Neon" << "Classic";
    
    // Add existing CSS themes
    themes << "Matrix" << "Cyberpunk" << "Futuristic" << "Minimal" << "Retro" << "Woodgrain";
    
    // Add discovered custom themes
    themes << discoverCustomThemes();
    
    return themes;
}

bool ThemeManager::isCustomTheme(const QString& themeName)
{
    QStringList customThemes = discoverCustomThemes();
    return customThemes.contains(themeName) || themeName.startsWith("template_");
}

void ThemeManager::refreshThemes()
{
    // Force rediscovery of themes
    // This could be called when new themes are added
    qDebug() << "Refreshing theme list...";
    QStringList availableThemes = getAvailableThemes();
    qDebug() << "Available themes:" << availableThemes;
}