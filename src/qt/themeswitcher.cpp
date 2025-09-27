// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "themeswitcher.h"
#include "thememanager.h"
#include <QFontDatabase>

#include <QApplication>
#include <QFontDatabase>
#include <QSettings>
#include <QMessageBox>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QFrame>
#include <QDebug>

ThemeSwitcher::ThemeSwitcher(QWidget* parent)
    : QWidget(parent)
    , m_themeManager(ThemeManager::instance())
    , m_updating(false)
{
    setupUI();
    loadSettings();
    
    // Connect theme manager signals
    connect(m_themeManager, &ThemeManager::themeChanged, this, &ThemeSwitcher::onThemeChanged);
    connect(m_themeManager, &ThemeManager::colorsChanged, this, &ThemeSwitcher::updatePreview);
}

ThemeSwitcher::~ThemeSwitcher()
{
}

void ThemeSwitcher::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Theme Selection Group
    QGroupBox* themeGroup = new QGroupBox(tr("Theme Selection"), this);
    QVBoxLayout* themeLayout = new QVBoxLayout(themeGroup);
    
    QHBoxLayout* themeRow = new QHBoxLayout();
    themeRow->addWidget(new QLabel(tr("Theme:")));
    
    m_themeCombo = new QComboBox();
    m_themeCombo->addItems(m_themeManager->availableThemes());
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ThemeSwitcher::onThemeChanged);
    themeRow->addWidget(m_themeCombo);
    
    m_previewButton = new QPushButton(tr("Preview"));
    m_previewButton->setToolTip(tr("Preview the selected theme"));
    connect(m_previewButton, &QPushButton::clicked, this, &ThemeSwitcher::onPreviewTheme);
    themeRow->addWidget(m_previewButton);
    
    themeRow->addStretch();
    themeLayout->addLayout(themeRow);
    
    // Custom Theme Group
    m_customGroup = new QGroupBox(tr("Custom Theme Colors"), this);
    m_customGroup->setEnabled(false);
    QGridLayout* customLayout = new QGridLayout(m_customGroup);
    
    // Color buttons
    m_primaryBgButton = new QPushButton();
    m_primaryBgButton->setFixedSize(40, 30);
    connect(m_primaryBgButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Primary Background:")), 0, 0);
    customLayout->addWidget(m_primaryBgButton, 0, 1);
    
    m_secondaryBgButton = new QPushButton();
    m_secondaryBgButton->setFixedSize(40, 30);
    connect(m_secondaryBgButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Secondary Background:")), 1, 0);
    customLayout->addWidget(m_secondaryBgButton, 1, 1);
    
    m_textButton = new QPushButton();
    m_textButton->setFixedSize(40, 30);
    connect(m_textButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Text Color:")), 2, 0);
    customLayout->addWidget(m_textButton, 2, 1);
    
    m_accentButton = new QPushButton();
    m_accentButton->setFixedSize(40, 30);
    connect(m_accentButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Accent Color:")), 3, 0);
    customLayout->addWidget(m_accentButton, 3, 1);
    
    m_borderButton = new QPushButton();
    m_borderButton->setFixedSize(40, 30);
    connect(m_borderButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Border Color:")), 4, 0);
    customLayout->addWidget(m_borderButton, 4, 1);
    
    QPushButton* applyCustomButton = new QPushButton(tr("Apply Custom Theme"));
    connect(applyCustomButton, &QPushButton::clicked, this, &ThemeSwitcher::onCustomizeTheme);
    customLayout->addWidget(applyCustomButton, 5, 0, 1, 2);
    
    // Font Group
    m_fontGroup = new QGroupBox(tr("Font Settings"), this);
    QGridLayout* fontLayout = new QGridLayout(m_fontGroup);
    
    m_useCustomFontCheck = new QCheckBox(tr("Use custom font"));
    connect(m_useCustomFontCheck, &QCheckBox::toggled, this, &ThemeSwitcher::onFontFamilyChanged);
    fontLayout->addWidget(m_useCustomFontCheck, 0, 0, 1, 2);
    
    fontLayout->addWidget(new QLabel(tr("Font Family:")), 1, 0);
    m_fontFamilyCombo = new QComboBox();
    QFontDatabase db;
    m_fontFamilyCombo->addItems(db.families());
    m_fontFamilyCombo->setEnabled(false);
    connect(m_fontFamilyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ThemeSwitcher::onFontFamilyChanged);
    fontLayout->addWidget(m_fontFamilyCombo, 1, 1);
    
    fontLayout->addWidget(new QLabel(tr("Font Size:")), 2, 0);
    m_fontSizeSpin = new QSpinBox();
    m_fontSizeSpin->setRange(8, 24);
    m_fontSizeSpin->setValue(9);
    m_fontSizeSpin->setEnabled(false);
    connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &ThemeSwitcher::onFontSizeChanged);
    fontLayout->addWidget(m_fontSizeSpin, 2, 1);
    
    // Preview Area
    QGroupBox* previewGroup = new QGroupBox(tr("Preview"), this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewFrame = new QFrame();
    m_previewFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    m_previewFrame->setMinimumHeight(100);
    
    QVBoxLayout* previewFrameLayout = new QVBoxLayout(m_previewFrame);
    
    m_previewLabel = new QLabel(tr("This is a preview of the selected theme.\n"
                                  "The colors and fonts will be applied to the entire application."));
    m_previewLabel->setWordWrap(true);
    previewFrameLayout->addWidget(m_previewLabel);
    
    m_previewButtonWidget = new QPushButton(tr("Sample Button"));
    previewFrameLayout->addWidget(m_previewButtonWidget);
    
    previewLayout->addWidget(m_previewFrame);
    
    // Add all groups to main layout
    mainLayout->addWidget(themeGroup);
    mainLayout->addWidget(m_customGroup);
    mainLayout->addWidget(m_fontGroup);
    mainLayout->addWidget(previewGroup);
    mainLayout->addStretch();
    
    // Initialize custom colors
    m_customPrimaryBg = QColor(255, 255, 255);
    m_customSecondaryBg = QColor(248, 249, 250);
    m_customText = QColor(33, 37, 41);
    m_customAccent = QColor(0, 123, 255);
    m_customBorder = QColor(222, 226, 230);
    
    updatePreview();
}

void ThemeSwitcher::loadSettings()
{
    QSettings settings;
    
    // Load theme
    QString themeName = settings.value("theme", "Light").toString();
    int index = m_themeCombo->findText(themeName);
    if (index >= 0) {
        m_themeCombo->setCurrentIndex(index);
    }
    
    // Load font settings
    bool useCustomFont = settings.value("useCustomFont", false).toBool();
    m_useCustomFontCheck->setChecked(useCustomFont);
    m_fontFamilyCombo->setEnabled(useCustomFont);
    m_fontSizeSpin->setEnabled(useCustomFont);
    
    QString fontFamily = settings.value("fontFamily", "Segoe UI").toString();
    int fontIndex = m_fontFamilyCombo->findText(fontFamily);
    if (fontIndex >= 0) {
        m_fontFamilyCombo->setCurrentIndex(fontIndex);
    }
    
    int fontSize = settings.value("fontSize", 9).toInt();
    m_fontSizeSpin->setValue(fontSize);
    
    // Load custom colors
    m_customPrimaryBg = settings.value("customPrimaryBg", QColor(255, 255, 255)).value<QColor>();
    m_customSecondaryBg = settings.value("customSecondaryBg", QColor(248, 249, 250)).value<QColor>();
    m_customText = settings.value("customText", QColor(33, 37, 41)).value<QColor>();
    m_customAccent = settings.value("customAccent", QColor(0, 123, 255)).value<QColor>();
    m_customBorder = settings.value("customBorder", QColor(222, 226, 230)).value<QColor>();
    
    updateColorButtons();
}

void ThemeSwitcher::saveSettings()
{
    QSettings settings;
    
    // Save theme
    settings.setValue("theme", m_themeCombo->currentText());
    
    // Save font settings
    settings.setValue("useCustomFont", m_useCustomFontCheck->isChecked());
    settings.setValue("fontFamily", m_fontFamilyCombo->currentText());
    settings.setValue("fontSize", m_fontSizeSpin->value());
    
    // Save custom colors
    settings.setValue("customPrimaryBg", m_customPrimaryBg);
    settings.setValue("customSecondaryBg", m_customSecondaryBg);
    settings.setValue("customText", m_customText);
    settings.setValue("customAccent", m_customAccent);
    settings.setValue("customBorder", m_customBorder);
}

void ThemeSwitcher::onThemeChanged()
{
    if (m_updating) return;
    
    QString themeName = m_themeCombo->currentText();
    
    if (themeName == "Light") {
        m_themeManager->switchToLight();
        m_customGroup->setEnabled(false);
    } else if (themeName == "Dark") {
        m_themeManager->switchToDark();
        m_customGroup->setEnabled(false);
    } else if (themeName == "Auto") {
        m_themeManager->switchToAuto();
        m_customGroup->setEnabled(false);
    } else if (themeName == "Dogecoin" || themeName == "Neon" || themeName == "Classic") {
        // Built-in themes
        if (themeName == "Dogecoin") {
            m_themeManager->switchToTheme(ThemeManager::Dogecoin);
        } else if (themeName == "Neon") {
            m_themeManager->switchToTheme(ThemeManager::Neon);
        } else if (themeName == "Classic") {
            m_themeManager->switchToTheme(ThemeManager::Classic);
        }
        m_customGroup->setEnabled(false);
    } else {
        // CSS themes from themes folder or custom themes
        m_themeManager->loadCSSTheme(themeName);
        m_customGroup->setEnabled(false);
    }
    
    updatePreview();
    Q_EMIT themeChanged();
}

void ThemeSwitcher::onCustomizeTheme()
{
    // Create custom theme with current colors
    ThemeManager::ThemeColors customColors;
    customColors.primaryBackground = m_customPrimaryBg;
    customColors.secondaryBackground = m_customSecondaryBg;
    customColors.primaryText = m_customText;
    customColors.primaryAccent = m_customAccent;
    customColors.primaryBorder = m_customBorder;
    
    // Fill in other colors based on primary colors
    customColors.tertiaryBackground = m_customSecondaryBg.darker(110);
    customColors.secondaryText = m_customText.lighter(150);
    customColors.tertiaryText = m_customText.lighter(200);
    customColors.accentText = m_customAccent;
    customColors.secondaryAccent = m_customAccent.lighter(120);
    customColors.successColor = QColor(40, 167, 69);
    customColors.warningColor = QColor(255, 193, 7);
    customColors.errorColor = QColor(220, 53, 69);
    customColors.infoColor = QColor(23, 162, 184);
    customColors.secondaryBorder = m_customBorder.darker(110);
    customColors.shadowColor = QColor(0, 0, 0, 25);
    customColors.buttonBackground = m_customAccent;
    customColors.buttonHover = m_customAccent.darker(110);
    customColors.buttonPressed = m_customAccent.darker(120);
    customColors.buttonText = QColor(255, 255, 255);
    customColors.inputBackground = m_customPrimaryBg;
    customColors.inputBorder = m_customBorder;
    customColors.inputFocus = m_customAccent;
    customColors.cardBackground = m_customPrimaryBg;
    customColors.cardBorder = m_customBorder;
    customColors.cardShadow = QColor(0, 0, 0, 10);
    
    m_themeManager->loadCustomTheme("Custom", customColors);
    m_themeManager->switchToCustom("Custom");
    
    updatePreview();
    Q_EMIT themeChanged();
}

void ThemeSwitcher::onResetTheme()
{
    resetToDefaults();
}

void ThemeSwitcher::onColorChanged()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QColor currentColor;
    if (button == m_primaryBgButton) currentColor = m_customPrimaryBg;
    else if (button == m_secondaryBgButton) currentColor = m_customSecondaryBg;
    else if (button == m_textButton) currentColor = m_customText;
    else if (button == m_accentButton) currentColor = m_customAccent;
    else if (button == m_borderButton) currentColor = m_customBorder;
    
    QColor newColor = QColorDialog::getColor(currentColor, this, tr("Select Color"));
    if (newColor.isValid()) {
        if (button == m_primaryBgButton) m_customPrimaryBg = newColor;
        else if (button == m_secondaryBgButton) m_customSecondaryBg = newColor;
        else if (button == m_textButton) m_customText = newColor;
        else if (button == m_accentButton) m_customAccent = newColor;
        else if (button == m_borderButton) m_customBorder = newColor;
        
        updateColorButtons();
        updatePreview();
    }
}

void ThemeSwitcher::onFontSizeChanged()
{
    if (m_useCustomFontCheck->isChecked()) {
        m_themeManager->setFontSize(m_fontSizeSpin->value());
        updatePreview();
    }
}

void ThemeSwitcher::onFontFamilyChanged()
{
    if (m_useCustomFontCheck->isChecked()) {
        m_themeManager->setFontFamily(m_fontFamilyCombo->currentText());
        updatePreview();
    }
}

void ThemeSwitcher::onPreviewTheme()
{
    // Apply theme to preview area
    m_themeManager->applyTheme(m_previewFrame);
    updatePreview();
}

void ThemeSwitcher::updatePreview()
{
    // Update preview frame with current theme
    m_themeManager->applyTheme(m_previewFrame);
    
    // Update color buttons
    updateColorButtons();
}

void ThemeSwitcher::updateColorButtons()
{
    m_primaryBgButton->setStyleSheet(QString("background-color: %1; border: 1px solid %2;")
                                     .arg(m_customPrimaryBg.name())
                                     .arg(m_customBorder.name()));
    
    m_secondaryBgButton->setStyleSheet(QString("background-color: %1; border: 1px solid %2;")
                                       .arg(m_customSecondaryBg.name())
                                       .arg(m_customBorder.name()));
    
    m_textButton->setStyleSheet(QString("background-color: %1; border: 1px solid %2;")
                                .arg(m_customText.name())
                                .arg(m_customBorder.name()));
    
    m_accentButton->setStyleSheet(QString("background-color: %1; border: 1px solid %2;")
                                  .arg(m_customAccent.name())
                                  .arg(m_customBorder.name()));
    
    m_borderButton->setStyleSheet(QString("background-color: %1; border: 1px solid %2;")
                                  .arg(m_customBorder.name())
                                  .arg(m_customBorder.name()));
}

void ThemeSwitcher::applyCustomColors()
{
    // This method can be used to apply custom colors to the current theme
    updatePreview();
}

void ThemeSwitcher::resetToDefaults()
{
    m_customPrimaryBg = QColor(255, 255, 255);
    m_customSecondaryBg = QColor(248, 249, 250);
    m_customText = QColor(33, 37, 41);
    m_customAccent = QColor(0, 123, 255);
    m_customBorder = QColor(222, 226, 230);
    
    updateColorButtons();
    updatePreview();
}
