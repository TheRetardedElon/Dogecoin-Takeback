// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "customthemes.h"
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDateTime>

// Forward declaration
bool copyDirectory(const QString& source, const QString& destination);

CustomThemeManager::CustomThemeManager(QObject* parent)
    : QObject(parent)
{
    setupDirectories();
}

CustomThemeManager::~CustomThemeManager()
{
}

void CustomThemeManager::setupDirectories()
{
    // Set up theme directories
    QString appDir = QCoreApplication::applicationDirPath();
    m_themesRoot = appDir + "/../src/qt/themes";
    m_customThemesDir = m_themesRoot + "/custom";
    m_templatesDir = m_themesRoot + "/templates";
    
    // Create directories if they don't exist
    QDir().mkpath(m_customThemesDir);
    QDir().mkpath(m_templatesDir);
    
    qDebug() << "Theme directories set up:";
    qDebug() << "  Root:" << m_themesRoot;
    qDebug() << "  Custom:" << m_customThemesDir;
    qDebug() << "  Templates:" << m_templatesDir;
}

QStringList CustomThemeManager::discoverThemes()
{
    QStringList allThemes;
    m_themeCache.clear();
    
    // Discover built-in CSS themes
    QStringList cssThemes = scanDirectoryForThemes(m_themesRoot);
    
    // Discover custom themes
    QStringList customThemes = scanDirectoryForThemes(m_customThemesDir);
    
    // Discover template themes
    QStringList templateThemes = scanDirectoryForThemes(m_templatesDir);
    
    allThemes << cssThemes << customThemes << templateThemes;
    allThemes.removeDuplicates();
    
    m_discoveredThemes = allThemes;
    
    qDebug() << "Discovered" << allThemes.size() << "themes:" << allThemes;
    
    Q_EMIT themesDiscovered(allThemes);
    return allThemes;
}

QStringList CustomThemeManager::scanDirectoryForThemes(const QString& directory)
{
    QStringList themes;
    QDir dir(directory);
    
    if (!dir.exists()) {
        return themes;
    }
    
    // Look for theme folders
    QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString& folder : folders) {
        QString themePath = dir.absoluteFilePath(folder);
        QString cssPath = themePath + "/" + folder + ".css";
        
        // Check if this is a valid theme (has CSS file)
        if (QFile::exists(cssPath) && isValidCSSFile(cssPath)) {
            themes << folder;
            
            // Load metadata if available
            QString metadataPath = themePath + "/theme.json";
            if (QFile::exists(metadataPath)) {
                ThemeMetadata metadata = parseMetadataFile(metadataPath);
                metadata.cssPath = cssPath;
                m_themeCache[folder] = metadata;
            } else {
                // Create default metadata
                ThemeMetadata metadata;
                metadata.name = folder;
                metadata.description = QString("Custom theme: %1").arg(folder);
                metadata.author = "Unknown";
                metadata.cssPath = cssPath;
                metadata.type = folder.startsWith("custom_") ? "custom" : "css";
                m_themeCache[folder] = metadata;
            }
        }
    }
    
    return themes;
}

ThemeMetadata CustomThemeManager::getThemeMetadata(const QString& themeName)
{
    if (m_themeCache.contains(themeName)) {
        return m_themeCache[themeName];
    }
    
    // Try to load metadata
    QString metadataPath = QString("%1/%2/theme.json").arg(m_themesRoot, themeName);
    if (QFile::exists(metadataPath)) {
        ThemeMetadata metadata = parseMetadataFile(metadataPath);
        m_themeCache[themeName] = metadata;
        return metadata;
    }
    
    // Return default metadata
    ThemeMetadata metadata;
    metadata.name = themeName;
    metadata.description = QString("Theme: %1").arg(themeName);
    metadata.author = "Unknown";
    metadata.type = "css";
    return metadata;
}

bool CustomThemeManager::loadTheme(const QString& themeName)
{
    QStringList themes = discoverThemes();
    if (!themes.contains(themeName)) {
        qDebug() << "Theme not found:" << themeName;
        return false;
    }
    
    ThemeMetadata metadata = getThemeMetadata(themeName);
    QString cssPath = metadata.cssPath;
    
    if (cssPath.isEmpty()) {
        cssPath = QString("%1/%2/%2.css").arg(m_themesRoot, themeName);
    }
    
    if (!QFile::exists(cssPath)) {
        qDebug() << "CSS file not found:" << cssPath;
        return false;
    }
    
    // Load and apply CSS
    QFile cssFile(cssPath);
    if (cssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&cssFile);
        QString cssContent = stream.readAll();
        cssFile.close();
        
        // Apply CSS globally
        qApp->setStyleSheet(cssContent);
        
        qDebug() << "Loaded theme:" << themeName << "from" << cssPath;
        Q_EMIT themeLoaded(themeName);
        return true;
    }
    
    qDebug() << "Failed to load theme:" << cssPath;
    return false;
}

bool CustomThemeManager::saveTheme(const ThemeMetadata& metadata, const QString& cssContent)
{
    QString sanitizedName = sanitizeThemeName(metadata.name);
    QString themeDir = QString("%1/%2").arg(m_customThemesDir, sanitizedName);
    
    // Create theme directory
    if (!QDir().mkpath(themeDir)) {
        qDebug() << "Failed to create theme directory:" << themeDir;
        return false;
    }
    
    // Save CSS file
    QString cssPath = QString("%1/%2.css").arg(themeDir, sanitizedName);
    QFile cssFile(cssPath);
    if (!cssFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to create CSS file:" << cssPath;
        return false;
    }
    
    QTextStream stream(&cssFile);
    stream << cssContent;
    cssFile.close();
    
    // Save metadata
    QString metadataPath = QString("%1/theme.json").arg(themeDir);
    if (!writeMetadataFile(metadataPath, metadata)) {
        qDebug() << "Failed to save metadata:" << metadataPath;
        return false;
    }
    
    // Update cache
    ThemeMetadata updatedMetadata = metadata;
    updatedMetadata.cssPath = cssPath;
    m_themeCache[sanitizedName] = updatedMetadata;
    
    qDebug() << "Saved theme:" << sanitizedName << "to" << themeDir;
    Q_EMIT themeSaved(sanitizedName);
    return true;
}

bool CustomThemeManager::exportTheme(const QString& themeName, const QString& exportPath)
{
    ThemeMetadata metadata = getThemeMetadata(themeName);
    QString cssPath = metadata.cssPath;
    
    if (cssPath.isEmpty()) {
        cssPath = QString("%1/%2/%2.css").arg(m_themesRoot, themeName);
    }
    
    if (!QFile::exists(cssPath)) {
        return false;
    }
    
    // Create export directory
    QDir().mkpath(exportPath);
    
    // Copy CSS file
    QString exportCssPath = QString("%1/%2.css").arg(exportPath, themeName);
    if (!QFile::copy(cssPath, exportCssPath)) {
        return false;
    }
    
    // Copy metadata if exists
    QString metadataPath = QString("%1/%2/theme.json").arg(m_themesRoot, themeName);
    if (QFile::exists(metadataPath)) {
        QString exportMetadataPath = QString("%1/theme.json").arg(exportPath);
        QFile::copy(metadataPath, exportMetadataPath);
    }
    
    Q_EMIT themeExported(themeName, exportPath);
    return true;
}

bool CustomThemeManager::importTheme(const QString& importPath)
{
    QFileInfo importInfo(importPath);
    if (!importInfo.exists()) {
        return false;
    }
    
    if (importInfo.isDir()) {
        // Import theme folder
        QString themeName = importInfo.baseName();
        QString targetDir = QString("%1/%2").arg(m_customThemesDir, themeName);
        
        if (QDir().exists(targetDir)) {
            // Remove existing theme
            QDir(targetDir).removeRecursively();
        }
        
        // Copy theme folder
        return QDir().mkpath(targetDir) && 
               copyDirectory(importPath, targetDir);
    } else if (importInfo.suffix() == "css") {
        // Import single CSS file
        QString themeName = importInfo.baseName();
        QString targetDir = QString("%1/%2").arg(m_customThemesDir, themeName);
        
        QDir().mkpath(targetDir);
        QString targetCss = QString("%1/%2.css").arg(targetDir, themeName);
        
        return QFile::copy(importPath, targetCss);
    }
    
    return false;
}

QStringList CustomThemeManager::getAvailableTemplates()
{
    QStringList templates;
    QDir templatesDir(m_templatesDir);
    
    if (templatesDir.exists()) {
        QStringList folders = templatesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& folder : folders) {
            QString cssPath = templatesDir.absoluteFilePath(folder + "/" + folder + ".css");
            if (QFile::exists(cssPath)) {
                templates << folder;
            }
        }
    }
    
    return templates;
}

ThemeMetadata CustomThemeManager::createThemeFromTemplate(const QString& templateName, const QString& newName)
{
    QString templatePath = QString("%1/%2").arg(m_templatesDir, templateName);
    QString templateCssPath = QString("%1/%2.css").arg(templatePath, templateName);
    QString templateMetadataPath = QString("%1/theme.json").arg(templatePath);
    
    ThemeMetadata metadata;
    metadata.name = newName;
    metadata.description = QString("Custom theme based on %1").arg(templateName);
    metadata.author = "User";
    metadata.type = "custom";
    
    if (QFile::exists(templateMetadataPath)) {
        ThemeMetadata templateMetadata = parseMetadataFile(templateMetadataPath);
        metadata.description = templateMetadata.description + " (Customized)";
        metadata.tags = templateMetadata.tags;
    }
    
    return metadata;
}

QString CustomThemeManager::generateCSSFromMetadata(const ThemeMetadata& metadata)
{
    // This would generate CSS from metadata properties
    // For now, return a basic template
    return QString(R"(
/* %1 - %2 */
/* Created by %3 */

QMainWindow {
    background-color: #ffffff;
    color: #000000;
}

QWidget {
    background-color: #ffffff;
    color: #000000;
}

QPushButton {
    background-color: #007bff;
    color: #ffffff;
    border: 1px solid #007bff;
    border-radius: 4px;
    padding: 8px 16px;
}

QPushButton:hover {
    background-color: #0056b3;
}

QPushButton:pressed {
    background-color: #004085;
}
)").arg(metadata.name, metadata.description, metadata.author);
}

QString CustomThemeManager::getThemesDirectory() const
{
    return m_themesRoot;
}

QString CustomThemeManager::getCustomThemesDirectory() const
{
    return m_customThemesDir;
}

QString CustomThemeManager::getTemplatesDirectory() const
{
    return m_templatesDir;
}

bool CustomThemeManager::createThemeDirectory(const QString& themeName)
{
    QString sanitizedName = sanitizeThemeName(themeName);
    QString themeDir = QString("%1/%2").arg(m_customThemesDir, sanitizedName);
    return QDir().mkpath(themeDir);
}

bool CustomThemeManager::validateTheme(const QString& themeName)
{
    QStringList errors = getThemeValidationErrors(themeName);
    return errors.isEmpty();
}

QStringList CustomThemeManager::getThemeValidationErrors(const QString& themeName)
{
    QStringList errors;
    
    ThemeMetadata metadata = getThemeMetadata(themeName);
    
    if (metadata.cssPath.isEmpty() || !QFile::exists(metadata.cssPath)) {
        errors << "CSS file not found or invalid";
    }
    
    if (metadata.name.isEmpty()) {
        errors << "Theme name is required";
    }
    
    if (!isValidCSSFile(metadata.cssPath)) {
        errors << "CSS file is invalid or corrupted";
    }
    
    return errors;
}

ThemeMetadata CustomThemeManager::parseMetadataFile(const QString& metadataPath)
{
    ThemeMetadata metadata;
    
    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return metadata;
    }
    
    QByteArray data = metadataFile.readAll();
    metadataFile.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "Failed to parse metadata:" << error.errorString();
        return metadata;
    }
    
    QJsonObject obj = doc.object();
    metadata.name = obj["name"].toString();
    metadata.description = obj["description"].toString();
    metadata.author = obj["author"].toString();
    metadata.version = obj["version"].toString();
    metadata.type = obj["type"].toString();
    metadata.previewPath = obj["preview"].toString();
    metadata.compatibility = obj["compatibility"].toString();
    
    // Parse tags array
    QJsonArray tagsArray = obj["tags"].toArray();
    for (const QJsonValue& tag : tagsArray) {
        metadata.tags << tag.toString();
    }
    
    // Parse custom properties
    metadata.customProperties = obj["customProperties"].toObject();
    
    return metadata;
}

bool CustomThemeManager::writeMetadataFile(const QString& metadataPath, const ThemeMetadata& metadata)
{
    QJsonObject obj;
    obj["name"] = metadata.name;
    obj["description"] = metadata.description;
    obj["author"] = metadata.author;
    obj["version"] = metadata.version;
    obj["type"] = metadata.type;
    obj["preview"] = metadata.previewPath;
    obj["compatibility"] = metadata.compatibility;
    
    QJsonArray tagsArray;
    for (const QString& tag : metadata.tags) {
        tagsArray.append(tag);
    }
    obj["tags"] = tagsArray;
    
    obj["customProperties"] = metadata.customProperties;
    
    QJsonDocument doc(obj);
    
    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    metadataFile.write(doc.toJson());
    metadataFile.close();
    
    return true;
}

bool CustomThemeManager::isValidCSSFile(const QString& cssPath)
{
    if (!QFile::exists(cssPath)) {
        return false;
    }
    
    QFile cssFile(cssPath);
    if (!cssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&cssFile);
    QString content = stream.readAll();
    cssFile.close();
    
    // Basic CSS validation - check for common CSS syntax
    return content.contains("QMainWindow") || content.contains("QWidget") || content.contains("QPushButton");
}

QString CustomThemeManager::sanitizeThemeName(const QString& name)
{
    // Remove invalid characters and create valid folder name
    QString sanitized = name;
    sanitized = sanitized.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    sanitized = sanitized.replace(QRegularExpression("_{2,}"), "_");
    sanitized = sanitized.toLower();
    
    if (sanitized.startsWith("_")) {
        sanitized = sanitized.mid(1);
    }
    
    if (sanitized.endsWith("_")) {
        sanitized = sanitized.left(sanitized.length() - 1);
    }
    
    if (sanitized.isEmpty()) {
        sanitized = "custom_theme";
    }
    
    return sanitized;
}

// Helper function for directory copying
bool copyDirectory(const QString& source, const QString& destination)
{
    QDir sourceDir(source);
    if (!sourceDir.exists()) {
        return false;
    }
    
    QDir destDir(destination);
    if (!destDir.exists()) {
        destDir.mkpath(".");
    }
    
    QStringList files = sourceDir.entryList(QDir::Files);
    for (const QString& file : files) {
        QString srcPath = sourceDir.absoluteFilePath(file);
        QString destPath = destDir.absoluteFilePath(file);
        if (!QFile::copy(srcPath, destPath)) {
            return false;
        }
    }
    
    QStringList dirs = sourceDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& dir : dirs) {
        QString srcPath = sourceDir.absoluteFilePath(dir);
        QString destPath = destDir.absoluteFilePath(dir);
        if (!copyDirectory(srcPath, destPath)) {
            return false;
        }
    }
    
    return true;
}
