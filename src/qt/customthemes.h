// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_CUSTOMTHEMES_H
#define BITCOIN_QT_CUSTOMTHEMES_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QStandardPaths>

struct ThemeMetadata {
    QString name;
    QString description;
    QString author;
    QString version;
    QString type; // "css", "builtin", "template"
    QString previewPath;
    QStringList tags;
    QString compatibility;
    QString cssPath;
    QJsonObject customProperties;
    
    ThemeMetadata() : version("1.0"), type("css"), compatibility("1.14.99+") {}
};

class CustomThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit CustomThemeManager(QObject* parent = nullptr);
    ~CustomThemeManager();

    // Theme discovery and management
    QStringList discoverThemes();
    ThemeMetadata getThemeMetadata(const QString& themeName);
    bool loadTheme(const QString& themeName);
    bool saveTheme(const ThemeMetadata& metadata, const QString& cssContent);
    bool exportTheme(const QString& themeName, const QString& exportPath);
    bool importTheme(const QString& importPath);
    
    // Theme creation helpers
    QStringList getAvailableTemplates();
    ThemeMetadata createThemeFromTemplate(const QString& templateName, const QString& newName);
    QString generateCSSFromMetadata(const ThemeMetadata& metadata);
    
    // File system operations
    QString getThemesDirectory() const;
    QString getCustomThemesDirectory() const;
    QString getTemplatesDirectory() const;
    bool createThemeDirectory(const QString& themeName);
    
    // Validation
    bool validateTheme(const QString& themeName);
    QStringList getThemeValidationErrors(const QString& themeName);

Q_SIGNALS:
    void themesDiscovered(const QStringList& themeNames);
    void themeLoaded(const QString& themeName);
    void themeSaved(const QString& themeName);
    void themeExported(const QString& themeName, const QString& exportPath);
    void themeImported(const QString& themeName);

private:
    void setupDirectories();
    ThemeMetadata parseMetadataFile(const QString& metadataPath);
    bool writeMetadataFile(const QString& metadataPath, const ThemeMetadata& metadata);
    QStringList scanDirectoryForThemes(const QString& directory);
    bool isValidCSSFile(const QString& cssPath);
    QString sanitizeThemeName(const QString& name);
    
    QString m_themesRoot;
    QString m_customThemesDir;
    QString m_templatesDir;
    QStringList m_discoveredThemes;
    QMap<QString, ThemeMetadata> m_themeCache;
};

#endif // BITCOIN_QT_CUSTOMTHEMES_H
