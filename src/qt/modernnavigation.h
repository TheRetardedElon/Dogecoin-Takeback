// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MODERNAVIGATION_H
#define BITCOIN_QT_MODERNAVIGATION_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>

class ModernNavigation : public QWidget
{
    Q_OBJECT

public:
    enum NavItem {
        Overview,
        Send,
        Receive,
        History,
        AddressBook,
        Console
    };

    explicit ModernNavigation(QWidget* parent = nullptr);
    ~ModernNavigation();

    void setCurrentItem(NavItem item);
    NavItem currentItem() const { return m_currentItem; }
    void setCompactMode(bool compact);

Q_SIGNALS:
    void itemClicked(NavItem item);
    void toggleCompact();

private Q_SLOTS:
    void onItemClicked();

private:
    void setupUI();
    void setupButton(QPushButton* button, NavItem item, const QString& text, const QString& icon);
    void updateButtonStyle(QPushButton* button, bool active);
    void animateButton(QPushButton* button);
    void updateItemStyles();
    void animateItemChange(NavItem newItem);
    void applyTheme();
    void onToggleClicked();

    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_headerLayout;
    QLabel* m_titleLabel;
    QLabel* m_logoLabel;
    
    QPushButton* m_overviewButton;
    QPushButton* m_sendButton;
    QPushButton* m_receiveButton;
    QPushButton* m_historyButton;
    QPushButton* m_addressBookButton;
    QPushButton* m_consoleButton;
    QPushButton* m_toggleButton;
    QPushButton* m_settingsButton;
    
    QFrame* m_navFrame;
    QVBoxLayout* m_navLayout;
    QFrame* m_statusFrame;
    QVBoxLayout* m_statusLayout;
    QLabel* m_balanceLabel;
    QLabel* m_statusLabel;
    
    QMap<NavItem, QPushButton*> m_navButtons;
    
    NavItem m_currentItem;
    QPropertyAnimation* m_buttonAnimation;
    bool m_compactMode;
    QPropertyAnimation* m_animation;
};

#endif // BITCOIN_QT_MODERNAVIGATION_H
