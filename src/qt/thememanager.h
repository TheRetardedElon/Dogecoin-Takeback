// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_THEMEMANAGER_H
#define BITCOIN_QT_THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QPalette>
#include <QString>
#include <QMap>
#include <QFont>
#include <QApplication>
#include <QStyle>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum ThemeType {
        Light,
        Dark,
        Dogecoin, // Dogecoin-themed colors
        Neon,     // High-contrast neon theme
        Classic,  // Classic Bitcoin-style theme
        Auto,     // Follows system theme
        Custom
    };

    struct ThemeColors {
        // Background colors
        QColor primaryBackground;
        QColor secondaryBackground;
        QColor tertiaryBackground;
        
        // Text colors
        QColor primaryText;
        QColor secondaryText;
        QColor tertiaryText;
        QColor accentText;
        
        // Accent colors
        QColor primaryAccent;
        QColor secondaryAccent;
        QColor successColor;
        QColor warningColor;
        QColor errorColor;
        QColor infoColor;
        
        // Border colors
        QColor primaryBorder;
        QColor secondaryBorder;
        
        // Shadow colors
        QColor shadowColor;
        
        // Button colors
        QColor buttonBackground;
        QColor buttonHover;
        QColor buttonPressed;
        QColor buttonText;
        
        // Input field colors
        QColor inputBackground;
        QColor inputBorder;
        QColor inputFocus;
        
        // Card colors
        QColor cardBackground;
        QColor cardBorder;
        QColor cardShadow;
    };

    static ThemeManager* instance();
    
    // Theme management
    void setTheme(ThemeType theme);
    ThemeType currentTheme() const { return m_currentTheme; }
    QString currentThemeName() const;
    
    // Color access
    QColor color(const QString& colorName) const;
    ThemeColors getCurrentColors() const;
    
    // Theme switching
    void switchToLight();
    void switchToDark();
    void switchToAuto();
    void switchToTheme(ThemeType theme);
    void switchToCustom(const QString& themeName);
    
    // Custom theme management
    void loadCustomTheme(const QString& themeName, const ThemeColors& colors);
    void saveCustomTheme(const QString& themeName, const ThemeColors& colors);
    QStringList availableThemes() const;
    
    // Font management
    void setFontFamily(const QString& family);
    void setFontSize(int size);
    QFont getFont() const;
    
    // Style application
    void applyTheme(QApplication* app);
    void applyTheme(QWidget* widget);
    QString getStylesheet() const;
    void loadCSSTheme(const QString& themeName);

Q_SIGNALS:
    void themeChanged(ThemeType newTheme);
    void colorsChanged();

private:
    explicit ThemeManager(QObject* parent = nullptr);
    static ThemeManager* m_instance;
    
    ThemeType m_currentTheme;
    ThemeColors m_currentColors;
    QFont m_customFont;
    bool m_useCustomFont;
    
    void initializeThemes();
    void updateColors();
    ThemeColors getLightThemeColors() const;
    ThemeColors getDarkThemeColors() const;
    ThemeColors getDogecoinThemeColors() const;
    ThemeColors getNeonThemeColors() const;
    ThemeColors getClassicThemeColors() const;
    ThemeColors getCustomThemeColors(const QString& themeName) const;
    
    QMap<QString, ThemeColors> m_customThemes;
    QMap<ThemeType, ThemeColors> m_builtInThemes;
};

#endif // BITCOIN_QT_THEMEMANAGER_H
