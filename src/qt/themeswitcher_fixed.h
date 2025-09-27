// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_THEMESWITCHER_H
#define BITCOIN_QT_THEMESWITCHER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QColorDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QFrame>

class ThemeManager;

class ThemeSwitcher : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeSwitcher(QWidget* parent = nullptr);
    ~ThemeSwitcher();

    void loadSettings();
    void saveSettings();

Q_SIGNALS:
    void themeChanged();

private Q_SLOTS:
    void onThemeChanged();
    void onCustomizeTheme();
    void onResetTheme();
    void onColorChanged();
    void onFontSizeChanged();
    void onFontFamilyChanged();
    void onPreviewTheme();

private:
    void setupUI();
    void updatePreview();
    void applyCustomColors();
    void resetToDefaults();
    void updateColorButtons();

    // Theme selection
    QComboBox* m_themeCombo;
    QPushButton* m_previewButton;
    
    // Custom theme controls
    QGroupBox* m_customGroup;
    QPushButton* m_primaryBgButton;
    QPushButton* m_secondaryBgButton;
    QPushButton* m_textButton;
    QPushButton* m_accentButton;
    QPushButton* m_borderButton;
    
    // Font controls
    QGroupBox* m_fontGroup;
    QComboBox* m_fontFamilyCombo;
    QSpinBox* m_fontSizeSpin;
    QCheckBox* m_useCustomFontCheck;
    
    // Preview area
    QFrame* m_previewFrame;
    QLabel* m_previewLabel;
    QPushButton* m_previewButtonWidget;
    
    // Current custom colors
    QColor m_customPrimaryBg;
    QColor m_customSecondaryBg;
    QColor m_customText;
    QColor m_customAccent;
    QColor m_customBorder;
    
    ThemeManager* m_themeManager;
    bool m_updating;
};

#endif // BITCOIN_QT_THEMESWITCHER_H
