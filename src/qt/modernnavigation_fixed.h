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

Q_SIGNALS:
    void itemClicked(NavItem item);

private Q_SLOTS:
    void onItemClicked();

private:
    void setupUI();
    void setupButton(QPushButton* button, NavItem item, const QString& text, const QString& icon);
    void updateButtonStyle(QPushButton* button, bool active);
    void animateButton(QPushButton* button);

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
    
    NavItem m_currentItem;
    QPropertyAnimation* m_buttonAnimation;
};

#endif // BITCOIN_QT_MODERNAVIGATION_H
