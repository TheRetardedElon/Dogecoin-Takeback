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
// Dogecoin depends Qt is built with -no-feature-colordialog (QT_NO_COLORDIALOG).
#ifndef QT_NO_COLORDIALOG
#include <QColorDialog>
#endif
#include <QInputDialog>
#include <QLineEdit>
#include <QSignalBlocker>
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
    , m_applyCustomButton(0)
    , m_resetColorsButton(0)
    , m_themeManager(ThemeManager::instance())
    , m_updating(false)
{
    // Build UI with updates blocked so combo init / loadSettings cannot
    // re-apply themes (and polish half-built OptionsDialog) during construction.
    m_updating = true;
    setupUI();
    loadSettings();
    seedColorsFromCurrentTheme();
    m_updating = false;

    // Sync combo when theme changes elsewhere — do not re-apply (would loop).
    connect(m_themeManager, &ThemeManager::themeChanged, this, [this](ThemeManager::ThemeType) {
        if (m_updating || !m_themeCombo)
            return;
        m_updating = true;
        const QString name = m_themeManager->currentThemeName();
        const int idx = m_themeCombo->findText(name);
        if (idx >= 0)
            m_themeCombo->setCurrentIndex(idx);
        seedColorsFromCurrentTheme();
        updatePreview();
        m_updating = false;
    });
    connect(m_themeManager, &ThemeManager::colorsChanged, this, [this]() {
        if (m_updating)
            return;
        seedColorsFromCurrentTheme();
        updatePreview();
    });
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
    m_themeCombo->addItems(m_themeManager->getAvailableThemes());
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ThemeSwitcher::onThemeChanged);
    themeRow->addWidget(m_themeCombo);
    
    m_previewButton = new QPushButton(tr("Preview"));
    m_previewButton->setToolTip(tr("Preview the selected theme"));
    connect(m_previewButton, &QPushButton::clicked, this, &ThemeSwitcher::onPreviewTheme);
    themeRow->addWidget(m_previewButton);
    
    themeRow->addStretch();
    themeLayout->addLayout(themeRow);
    
    // Custom Theme Group — always enabled so swatches + Apply work for any base theme.
    m_customGroup = new QGroupBox(tr("Custom Theme Colors"), this);
    m_customGroup->setToolTip(tr(
        "Swatches follow the active theme. Click a chip to edit, then Apply Custom Theme "
        "to push a custom palette across the whole Core Pro shell."));
    QGridLayout* customLayout = new QGridLayout(m_customGroup);
    
    // Color buttons
    m_primaryBgButton = new QPushButton();
    m_primaryBgButton->setFixedSize(40, 30);
    m_primaryBgButton->setToolTip(tr("Primary background"));
    connect(m_primaryBgButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Primary Background:")), 0, 0);
    customLayout->addWidget(m_primaryBgButton, 0, 1);
    
    m_secondaryBgButton = new QPushButton();
    m_secondaryBgButton->setFixedSize(40, 30);
    m_secondaryBgButton->setToolTip(tr("Secondary background (nav, panels)"));
    connect(m_secondaryBgButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Secondary Background:")), 1, 0);
    customLayout->addWidget(m_secondaryBgButton, 1, 1);
    
    m_textButton = new QPushButton();
    m_textButton->setFixedSize(40, 30);
    m_textButton->setToolTip(tr("Primary text"));
    connect(m_textButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Text Color:")), 2, 0);
    customLayout->addWidget(m_textButton, 2, 1);
    
    m_accentButton = new QPushButton();
    m_accentButton->setFixedSize(40, 30);
    m_accentButton->setToolTip(tr("Accent / buttons"));
    connect(m_accentButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Accent Color:")), 3, 0);
    customLayout->addWidget(m_accentButton, 3, 1);
    
    m_borderButton = new QPushButton();
    m_borderButton->setFixedSize(40, 30);
    m_borderButton->setToolTip(tr("Borders and dividers"));
    connect(m_borderButton, &QPushButton::clicked, this, &ThemeSwitcher::onColorChanged);
    customLayout->addWidget(new QLabel(tr("Border Color:")), 4, 0);
    customLayout->addWidget(m_borderButton, 4, 1);
    
    m_applyCustomButton = new QPushButton(tr("Apply Custom Theme"));
    m_applyCustomButton->setToolTip(tr("Build a Custom theme from the swatches and apply it app-wide."));
    m_applyCustomButton->setEnabled(true);
    connect(m_applyCustomButton, &QPushButton::clicked, this, &ThemeSwitcher::onCustomizeTheme);
    customLayout->addWidget(m_applyCustomButton, 5, 0, 1, 1);

    m_resetColorsButton = new QPushButton(tr("Reset to theme"));
    m_resetColorsButton->setToolTip(tr("Reload swatches from the currently selected named theme."));
    connect(m_resetColorsButton, &QPushButton::clicked, this, &ThemeSwitcher::onResetTheme);
    customLayout->addWidget(m_resetColorsButton, 5, 1, 1, 1);
    
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
    
    // Placeholder until seedColorsFromCurrentTheme / loadSettings runs
    m_customPrimaryBg = QColor(18, 18, 18);
    m_customSecondaryBg = QColor(33, 37, 41);
    m_customText = QColor(248, 249, 250);
    m_customAccent = QColor(0, 123, 255);
    m_customBorder = QColor(52, 58, 64);
    
    updateColorButtons();
    updatePreview();
}

void ThemeSwitcher::loadSettings()
{
    // Never apply a full app theme while loading saved prefs into the form.
    // setCurrentIndex / setChecked fire slots that used to call applyToApplication
    // and hard-crash while OptionsDialog was still under construction.
    const bool wasUpdating = m_updating;
    m_updating = true;

    QSettings settings;

    // Prefer live ThemeManager name (what the shell is using), then saved pref.
    QString themeName = m_themeManager->currentThemeName();
    if (themeName.isEmpty() || themeName == QLatin1String("Unknown") ||
        m_themeCombo->findText(themeName) < 0) {
        themeName = settings.value("theme", "Dark").toString();
    }
    const int index = m_themeCombo->findText(themeName);
    if (index >= 0) {
        QSignalBlocker block(m_themeCombo);
        m_themeCombo->setCurrentIndex(index);
    }

    const bool useCustomFont = settings.value("useCustomFont", false).toBool();
    {
        QSignalBlocker b1(m_useCustomFontCheck);
        QSignalBlocker b2(m_fontFamilyCombo);
        QSignalBlocker b3(m_fontSizeSpin);
        m_useCustomFontCheck->setChecked(useCustomFont);
        m_fontFamilyCombo->setEnabled(useCustomFont);
        m_fontSizeSpin->setEnabled(useCustomFont);

        const QString fontFamily = settings.value("fontFamily", "Segoe UI").toString();
        const int fontIndex = m_fontFamilyCombo->findText(fontFamily);
        if (fontIndex >= 0)
            m_fontFamilyCombo->setCurrentIndex(fontIndex);

        m_fontSizeSpin->setValue(settings.value("fontSize", 9).toInt());
    }

    // If user previously saved custom colors, restore them when theme is Custom;
    // otherwise seed swatches from the active named theme (fixes white chips on Dark).
    if (m_themeManager->currentTheme() == ThemeManager::Custom &&
        settings.contains("customPrimaryBg")) {
        m_customPrimaryBg = settings.value("customPrimaryBg").value<QColor>();
        m_customSecondaryBg = settings.value("customSecondaryBg").value<QColor>();
        m_customText = settings.value("customText").value<QColor>();
        m_customAccent = settings.value("customAccent").value<QColor>();
        m_customBorder = settings.value("customBorder").value<QColor>();
        updateColorButtons();
    } else {
        seedColorsFromCurrentTheme();
    }

    // Preview only — do not push theme to qApp here.
    if (m_previewFrame)
        m_themeManager->applyTheme(m_previewFrame);

    m_updating = wasUpdating;
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

void ThemeSwitcher::applySelectedThemeToApp()
{
    const QString themeName = m_themeCombo->currentText();

    if (themeName == "Light") {
        m_themeManager->switchToLight();
    } else if (themeName == "Dark") {
        m_themeManager->switchToDark();
    } else if (themeName == "Dogecoin") {
        m_themeManager->switchToTheme(ThemeManager::Dogecoin);
    } else if (themeName == "Neon") {
        m_themeManager->switchToTheme(ThemeManager::Neon);
    } else if (themeName == "Classic") {
        m_themeManager->switchToTheme(ThemeManager::Classic);
    } else if (themeName == "Custom") {
        // Custom is applied via onCustomizeTheme (swatches) — do not recurse here.
        return;
    } else {
        // CSS themes from themes folder
        m_themeManager->loadCSSTheme(themeName);
    }
}

void ThemeSwitcher::onThemeChanged()
{
    if (m_updating)
        return;

    m_updating = true;
    applySelectedThemeToApp();
    seedColorsFromCurrentTheme();
    updatePreview();
    m_updating = false;
    Q_EMIT themeChanged();
}

void ThemeSwitcher::onCustomizeTheme()
{
    // Create custom theme with current swatch colors (seeded from active theme)
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
    // Readable button label on light or dark accents
    customColors.buttonText = (m_customAccent.lightness() > 140)
                                  ? QColor(20, 20, 20)
                                  : QColor(255, 255, 255);
    customColors.inputBackground = m_customPrimaryBg;
    customColors.inputBorder = m_customBorder;
    customColors.inputFocus = m_customAccent;
    customColors.cardBackground = m_customSecondaryBg;
    customColors.cardBorder = m_customBorder;
    customColors.cardShadow = QColor(0, 0, 0, 10);
    
    m_themeManager->loadCustomTheme("Custom", customColors);
    m_themeManager->switchToCustom("Custom");
    // switchToCustom already applyToApplication — ensure Pro shell rules are present
    m_themeManager->applyToApplication();

    // Reflect Custom in the combo if listed
    if (m_themeCombo) {
        m_updating = true;
        int idx = m_themeCombo->findText(QStringLiteral("Custom"));
        if (idx < 0) {
            m_themeCombo->addItem(QStringLiteral("Custom"));
            idx = m_themeCombo->findText(QStringLiteral("Custom"));
        }
        if (idx >= 0)
            m_themeCombo->setCurrentIndex(idx);
        m_updating = false;
    }
    
    updatePreview();
    Q_EMIT themeChanged();
}

void ThemeSwitcher::onResetTheme()
{
    // Re-apply named theme from combo, then re-seed swatches from it
    m_updating = true;
    applySelectedThemeToApp();
    seedColorsFromCurrentTheme();
    updatePreview();
    m_updating = false;
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
    
    QColor newColor;
#ifndef QT_NO_COLORDIALOG
    newColor = QColorDialog::getColor(currentColor, this, tr("Select Color"));
#else
    // Minimal depends Qt has no color dialog; accept #RRGGBB / named colors.
    bool ok = false;
    const QString hex = QInputDialog::getText(
        this,
        tr("Select Color"),
        tr("Color (#RRGGBB or name):"),
        QLineEdit::Normal,
        currentColor.name(QColor::HexRgb),
        &ok);
    if (ok)
        newColor = QColor(hex.trimmed());
#endif
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
    // Apply selected theme to whole app (incl. Pro shell) + local preview box
    if (m_updating)
        return;
    if (m_themeCombo && m_themeCombo->currentText() == QLatin1String("Custom")) {
        onCustomizeTheme();
        return;
    }
    m_updating = true;
    applySelectedThemeToApp();
    seedColorsFromCurrentTheme();
    // switchTo* already called applyToApplication; call again for Pro shell safety
    m_themeManager->applyToApplication();
    if (m_previewFrame)
        m_themeManager->applyTheme(m_previewFrame);
    updatePreview();
    m_updating = false;
    Q_EMIT themeChanged();
}

void ThemeSwitcher::updatePreview()
{
    if (!m_previewFrame)
        return;

    // Paint preview from current swatches so chips match the box
    const QString bg = m_customPrimaryBg.name();
    const QString bg2 = m_customSecondaryBg.name();
    const QString text = m_customText.name();
    const QString accent = m_customAccent.name();
    const QString border = m_customBorder.name();
    const QString btnText = (m_customAccent.lightness() > 140)
                                ? QStringLiteral("#141414")
                                : QStringLiteral("#ffffff");

    m_previewFrame->setStyleSheet(
        QStringLiteral(
            "QFrame { background-color: %1; color: %2; border: 1px solid %3; border-radius: 8px; }"
            "QLabel { color: %2; background: transparent; }"
            "QPushButton { background-color: %4; color: %5; border: 1px solid %3; "
            "border-radius: 6px; padding: 8px 14px; font-weight: 600; }"
            "QPushButton:hover { background-color: %6; }")
            .arg(bg, text, border, accent, btnText, m_customAccent.lighter(115).name()));

    if (m_previewLabel) {
        m_previewLabel->setText(tr("Preview: %1\n"
                                   "Primary / secondary / text / accent as shown on the swatches.")
                                    .arg(m_themeCombo ? m_themeCombo->currentText() : QString()));
    }

    updateColorButtons();
}

void ThemeSwitcher::updateColorButtons()
{
    if (!m_primaryBgButton)
        return;

    auto styleChip = [](QPushButton* btn, const QColor& fill, const QColor& border) {
        if (!btn)
            return;
        btn->setStyleSheet(QStringLiteral(
                               "QPushButton { background-color: %1; border: 2px solid %2; border-radius: 4px; }"
                               "QPushButton:hover { border-color: %3; }")
                               .arg(fill.name())
                               .arg(border.name())
                               .arg(border.lighter(130).name()));
        btn->setToolTip(fill.name(QColor::HexRgb));
    };

    styleChip(m_primaryBgButton, m_customPrimaryBg, m_customBorder);
    styleChip(m_secondaryBgButton, m_customSecondaryBg, m_customBorder);
    styleChip(m_textButton, m_customText, m_customBorder);
    styleChip(m_accentButton, m_customAccent, m_customBorder);
    styleChip(m_borderButton, m_customBorder, m_customText);
}

void ThemeSwitcher::seedColorsFromCurrentTheme()
{
    if (!m_themeManager)
        return;
    const ThemeManager::ThemeColors c = m_themeManager->getCurrentColors();
    m_customPrimaryBg = c.primaryBackground;
    m_customSecondaryBg = c.secondaryBackground;
    m_customText = c.primaryText;
    m_customAccent = c.primaryAccent.isValid() ? c.primaryAccent : c.buttonBackground;
    m_customBorder = c.primaryBorder;
    updateColorButtons();
}

void ThemeSwitcher::applyCustomColors()
{
    updatePreview();
}

void ThemeSwitcher::resetToDefaults()
{
    seedColorsFromCurrentTheme();
    updatePreview();
}
