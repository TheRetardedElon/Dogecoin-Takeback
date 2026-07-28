// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_THEMEBUILDER_H
#define BITCOIN_QT_THEMEBUILDER_H

#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QTabWidget>
#include <QSplitter>
#include <QFrame>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QSlider>
#include <QScrollArea>

#include "customthemes.h"

class ColorPickerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPickerWidget(const QString& label, QWidget* parent = nullptr);
    QColor getColor() const;
    void setColor(const QColor& color);
    void setLabel(const QString& label);

Q_SIGNALS:
    void colorChanged(const QColor& color);

private Q_SLOTS:
    void onColorButtonClicked();

private:
    QLabel* m_label;
    QPushButton* m_colorButton;
    QColor m_color;
};

class ThemePreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ThemePreviewWidget(QWidget* parent = nullptr);
    void updatePreview(const ThemeMetadata& metadata, const QString& cssContent);

private:
    void setupPreview();
    
    QWidget* m_previewContainer;
    QLabel* m_previewTitle;
    QPushButton* m_previewButton;
    QLineEdit* m_previewInput;
    QFrame* m_previewCard;
};

class ThemeBuilder : public QDialog
{
    Q_OBJECT

public:
    explicit ThemeBuilder(QWidget* parent = nullptr);
    ~ThemeBuilder();

    void loadTheme(const QString& themeName);
    void loadTemplate(const QString& templateName);

Q_SIGNALS:
    void themeCreated(const QString& themeName);
    void themeUpdated(const QString& themeName);

private Q_SLOTS:
    void onSaveTheme();
    void onExportTheme();
    void onImportTheme();
    void onLoadTemplate();
    void onPreviewTheme();
    void onResetTheme();
    void onColorChanged();
    void onThemeNameChanged();
    void onGenerateCSS();

private:
    void setupUI();
    void setupColorTab();
    void setupMetadataTab();
    void setupPreviewTab();
    void setupActions();
    void updatePreview();
    void loadCurrentTheme();
    ThemeMetadata createMetadata();
    QString generateCSSContent();
    bool validateTheme();
    void showValidationErrors();

    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_contentLayout;
    QSplitter* m_splitter;
    
    // Left side - Controls
    QWidget* m_controlsWidget;
    QVBoxLayout* m_controlsLayout;
    QTabWidget* m_tabWidget;
    
    // Color Tab
    QWidget* m_colorTab;
    QVBoxLayout* m_colorLayout;
    QScrollArea* m_colorScrollArea;
    QWidget* m_colorWidget;
    QGridLayout* m_colorGridLayout;
    
    // Metadata Tab
    QWidget* m_metadataTab;
    QVBoxLayout* m_metadataLayout;
    QLineEdit* m_nameEdit;
    QLineEdit* m_authorEdit;
    QTextEdit* m_descriptionEdit;
    QComboBox* m_typeCombo;
    QLineEdit* m_versionEdit;
    QLineEdit* m_compatibilityEdit;
    QTextEdit* m_tagsEdit;
    
    // Preview Tab
    QWidget* m_previewTab;
    QVBoxLayout* m_previewLayout;
    ThemePreviewWidget* m_previewWidget;
    
    // Right side - Preview
    QWidget* m_previewContainer;
    QVBoxLayout* m_previewContainerLayout;
    QLabel* m_previewLabel;
    QFrame* m_previewFrame;
    
    // Bottom - Actions
    QHBoxLayout* m_actionsLayout;
    QPushButton* m_saveButton;
    QPushButton* m_exportButton;
    QPushButton* m_importButton;
    QPushButton* m_loadTemplateButton;
    QPushButton* m_previewButton;
    QPushButton* m_resetButton;
    QPushButton* m_closeButton;
    
    // Color pickers
    ColorPickerWidget* m_primaryBgPicker;
    ColorPickerWidget* m_secondaryBgPicker;
    ColorPickerWidget* m_tertiaryBgPicker;
    ColorPickerWidget* m_primaryTextPicker;
    ColorPickerWidget* m_secondaryTextPicker;
    ColorPickerWidget* m_tertiaryTextPicker;
    ColorPickerWidget* m_accentTextPicker;
    ColorPickerWidget* m_primaryAccentPicker;
    ColorPickerWidget* m_secondaryAccentPicker;
    ColorPickerWidget* m_successColorPicker;
    ColorPickerWidget* m_warningColorPicker;
    ColorPickerWidget* m_errorColorPicker;
    ColorPickerWidget* m_infoColorPicker;
    ColorPickerWidget* m_primaryBorderPicker;
    ColorPickerWidget* m_secondaryBorderPicker;
    ColorPickerWidget* m_shadowColorPicker;
    ColorPickerWidget* m_buttonBgPicker;
    ColorPickerWidget* m_buttonHoverPicker;
    ColorPickerWidget* m_buttonPressedPicker;
    ColorPickerWidget* m_buttonTextPicker;
    ColorPickerWidget* m_inputBgPicker;
    ColorPickerWidget* m_inputBorderPicker;
    ColorPickerWidget* m_inputFocusPicker;
    ColorPickerWidget* m_cardBgPicker;
    ColorPickerWidget* m_cardBorderPicker;
    ColorPickerWidget* m_cardShadowPicker;
    
    // Data
    CustomThemeManager* m_themeManager;
    ThemeMetadata m_currentMetadata;
    QString m_currentCSS;
    bool m_isLoading;
};

#endif // BITCOIN_QT_THEMEBUILDER_H
