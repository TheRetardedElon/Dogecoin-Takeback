// Simple test to verify our modern UI components compile correctly
// This is a minimal test to check for basic compilation issues

#include "src/qt/thememanager.h"
#include "src/qt/themeswitcher.h"
#include "src/qt/moderncard.h"
#include "src/qt/modernoverviewpage.h"
#include "src/qt/modernnavigation.h"
#include "src/qt/modernmainwindow.h"

#include <QApplication>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Test basic instantiation
    ThemeManager* themeManager = ThemeManager::instance();
    Q_UNUSED(themeManager);
    
    // Test theme switching
    themeManager->switchToLight();
    themeManager->switchToDark();
    
    // Test color access
    QColor primaryBg = themeManager->color("primaryBackground");
    Q_UNUSED(primaryBg);
    
    return 0;
}
