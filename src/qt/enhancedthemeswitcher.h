// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ENHANCEDTHEMESWITCHER_H
#define BITCOIN_QT_ENHANCEDTHEMESWITCHER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QTabWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>

#include "customthemes.h"

class EnhancedThemeSwitcher : public QWidget
{
    Q_OBJECT

public:
    explicit EnhancedThemeSwitcher(QWidget* parent = nullptr);
    ~EnhancedThemeSwitcher();

    void loadSettings();
    void saveSettings();
    void refreshThemes();

Q_SIGNALS:
    void themeChanged();

private Q_SLOTS:
    void onThemeChanged();
    void onCreateTheme();
    void onImportTheme();
    void onExportTheme();
    void onDeleteTheme();
    void onRefreshThemes();
    void onThemeSelected();
    void onPreviewTheme();

private:
    void setupUI();
    void setupThemeTab();
    void setupCustomTab();
    void setupTemplatesTab();
    void setupPreviewTab();
    void updateThemeList();
    void updatePreview();
    void applyTheme(const QString& themeName);
    void showThemeInfo(const QString& themeName);

    // UI Components
    QVBoxLayout* m_mainLayout;
    QTabWidget* m_tabWidget;
    
    // Theme Tab
    QWidget* m_themeTab;
    QVBoxLayout* m_themeLayout;
    QComboBox* m_themeCombo;
    QPushButton* m_previewButton;
    QPushButton* m_applyButton;
    QTextEdit* m_themeDescription;
    QLabel* m_themeInfo;
    
    // Custom Tab
    QWidget* m_customTab;
    QVBoxLayout* m_customLayout;
    QPushButton* m_createThemeButton;
    QPushButton* m_importThemeButton;
    QPushButton* m_exportThemeButton;
    QPushButton* m_deleteThemeButton;
    QPushButton* m_refreshButton;
    QListWidget* m_customThemeList;
    QTextEdit* m_customThemeInfo;
    
    // Templates Tab
    QWidget* m_templatesTab;
    QVBoxLayout* m_templatesLayout;
    QListWidget* m_templateList;
    QPushButton* m_useTemplateButton;
    QTextEdit* m_templateInfo;
    
    // Preview Tab
    QWidget* m_previewTab;
    QVBoxLayout* m_previewLayout;
    QFrame* m_previewFrame;
    QLabel* m_previewLabel;
    QPushButton* m_previewButtonWidget;
    QLineEdit* m_previewInput;
    
    // Data
    CustomThemeManager* m_themeManager;
    QStringList m_availableThemes;
    QString m_currentTheme;
};

#endif // BITCOIN_QT_ENHANCEDTHEMESWITCHER_H
