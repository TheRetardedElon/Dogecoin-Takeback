#ifndef ENHANCED_THEME_MANAGER_H
#define ENHANCED_THEME_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QWidget>

class BitcoinGUI;

struct ThemeInfo {
    QString name;
    QString displayName;
    QString description;
    QString primaryColor;
    QString secondaryColor;
    QString accentColor;
    QString backgroundColor;
    QString textColor;
    QString layout; // "sidebar", "tabs", "cards", "minimal"
    QStringList menuItems;
    bool hasCustomMenus;
};

class EnhancedThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit EnhancedThemeManager(BitcoinGUI* parent);
    ~EnhancedThemeManager();

    void initializeThemes();
    void applyTheme(const QString& themeName);
    void cycleTheme();
    QStringList getAvailableThemes() const;
    ThemeInfo getThemeInfo(const QString& themeName) const;
    QString getCurrentTheme() const;
    
    // Menu system
    void createCustomMenuBar();
    void updateMenuBarForTheme(const QString& themeName);
    QMenuBar* getMenuBar() const { return m_menuBar; }

Q_SIGNALS:
    void themeChanged(const QString& themeName);
    void menuActionTriggered(const QString& actionName);

private Q_SLOTS:
    void onMenuAction();

private:
    BitcoinGUI* m_parent;
    QMenuBar* m_menuBar;
    QMap<QString, ThemeInfo> m_themes;
    QString m_currentTheme;
    int m_themeIndex;
    
    void setupDefaultThemes();
    void createCyberpunkMenus();
    void createNeonMenus();
    void createDarkMenus();
    void createLightMenus();
    void createFuturisticMenus();
    void createRetroMenus();
    void createMinimalMenus();
    
    QString getThemeCSS(const QString& themeName) const;
    void applyLayout(const QString& layoutType);
    
    // Layout creation methods
    void createCardLayout();
    void createMinimalLayout();
    void createTabLayout();
    void createSidebarLayout();
    
    // Revolutionary new layout methods
    void createFloatingCardLayout();
    void createSpaceAgeLayout();
    void createDashboardLayout();
    void createMatrixLayout();
    void createHolographicLayout();
    void createEnhancedSidebarLayout();
};

#endif // ENHANCED_THEME_MANAGER_H
