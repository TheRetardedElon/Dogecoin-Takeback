// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2020-2022 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/dogecoin-config.h"
#endif

#include "splashscreen.h"

#include "networkstyle.h"

#include "clientversion.h"
#include "init.h"
#include "util.h"
#include "ui_interface.h"
#include "version.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QPainter>
#include <QRadialGradient>
#include <QDebug>

#include <boost/bind/bind.hpp>

SplashScreen::SplashScreen(Qt::WindowFlags f, const NetworkStyle *networkStyle) :
    QWidget(0, f), curAlignment(0)
{
    // set reference point, paddings for action bubble positioning
    int bubblePaddingBottom    = 152;  // Distance from bottom for action bubble (move ALL text down 8px)
    int bubblePaddingSides     = 50;  // Side padding for bubble text
    int titleVersionVSpace     = 18;  // Space between title and version (moved down 4px)
    int versionCopyrightVSpace = 16;  // Space between version and copyright (moved down 8px)

    float fontFactor           = 1.0;
    float devicePixelRatio     = 1.0;
#if QT_VERSION > 0x050100
    devicePixelRatio = ((QGuiApplication*)QCoreApplication::instance())->devicePixelRatio();
#endif

    // define text to place
    QString titleText       = tr(PACKAGE_NAME);
    QString versionText     = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightYears  = QString::fromUtf8(strprintf("\xc2\xA9 %u-%u", 2009, COPYRIGHT_YEAR).c_str());
    QString copyrightDevelopers = QString::fromUtf8("The Dogecoin Core developers");
    QString titleAddText    = networkStyle->getTitleAddText();

    QString font            = "Comic Sans MS";

    // create a bitmap according to device pixelratio - make it square for the icon
    QSize splashSize(480*devicePixelRatio,480*devicePixelRatio);
    pixmap = QPixmap(splashSize);
    pixmap.fill(Qt::transparent); // Start with transparent background

#if QT_VERSION > 0x050100
    // change to HiDPI if it makes sense
    pixmap.setDevicePixelRatio(devicePixelRatio);
#endif

    QPainter pixPaint(&pixmap);

    // Draw the dogecoin icon centered
    QRect rectIcon(QPoint(0,0), QSize(splashSize.width()/devicePixelRatio, splashSize.height()/devicePixelRatio));
    const QSize requiredSize(1024,1024);
    QPixmap icon(networkStyle->getAppIcon().pixmap(requiredSize));
    
    // Debug: Check if icon loaded properly
    if (icon.isNull()) {
        qDebug() << "SplashScreen: Icon is null! Trying to load from resource directly...";
        // Try loading directly from resource as fallback
        icon = QPixmap(":/icons/dogecoin");
        if (icon.isNull()) {
            qDebug() << "SplashScreen: Direct resource load also failed!";
        } else {
            qDebug() << "SplashScreen: Direct resource load succeeded, size:" << icon.size();
        }
    } else {
        qDebug() << "SplashScreen: Icon loaded successfully from NetworkStyle, size:" << icon.size();
    }
    
    if (!icon.isNull()) {
        pixPaint.drawPixmap(rectIcon, icon);
    } else {
        qDebug() << "SplashScreen: Cannot draw icon - it's null!";
    }

    // Position text in the action bubble area (bottom center) - the bubble is already in the PNG
    int bubbleTextY = splashSize.height()/devicePixelRatio - bubblePaddingBottom;
    
    // Set pen color for text (the bubble background is already in the PNG)
    pixPaint.setPen(QColor(255,255,255)); // White text for good contrast
    
    // Draw title in action bubble (slightly larger)
    QFont titleFont(font, 20*fontFactor);
    titleFont.setBold(true);
    pixPaint.setFont(titleFont);
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth = fm.width(titleText);
    
    // Center the title text
    int titleX = (splashSize.width()/devicePixelRatio - titleTextWidth) / 2;
    pixPaint.drawText(titleX, bubbleTextY, titleText);

    // Draw version text below title (bold, slightly larger)
    QFont versionFont(font, 10*fontFactor);
    versionFont.setBold(true);
    pixPaint.setFont(versionFont);
    fm = pixPaint.fontMetrics();
    int versionTextWidth = fm.width(versionText);
    
    // Center the version text
    int versionX = (splashSize.width()/devicePixelRatio - versionTextWidth) / 2;
    pixPaint.drawText(versionX, bubbleTextY + titleVersionVSpace, versionText);

    // Draw copyright years below version (bold, slightly larger)
    QFont copyrightFont(font, 8*fontFactor);
    copyrightFont.setBold(true);
    pixPaint.setFont(copyrightFont);
    
    // Draw copyright years (centered)
    fm = pixPaint.fontMetrics();
    int copyrightYearsWidth = fm.width(copyrightYears);
    int copyrightYearsX = (splashSize.width()/devicePixelRatio - copyrightYearsWidth) / 2;
    int copyrightYearsY = bubbleTextY + titleVersionVSpace + versionCopyrightVSpace;
    pixPaint.drawText(copyrightYearsX, copyrightYearsY, copyrightYears);

    // Draw copyright developers below years
    fm = pixPaint.fontMetrics();
    int copyrightDevelopersWidth = fm.width(copyrightDevelopers);
    int copyrightDevelopersX = (splashSize.width()/devicePixelRatio - copyrightDevelopersWidth) / 2;
    int copyrightDevelopersY = copyrightYearsY + versionCopyrightVSpace;
    pixPaint.drawText(copyrightDevelopersX, copyrightDevelopersY, copyrightDevelopers);

    // Draw additional text if special network (small, in top area)
    if(!titleAddText.isEmpty()) {
        QFont boldFont = QFont(font, 12*fontFactor);
        boldFont.setWeight(QFont::Bold);
        pixPaint.setFont(boldFont);
        pixPaint.setPen(QColor(100,100,100)); // Gray for network text
        fm = pixPaint.fontMetrics();
        int titleAddTextWidth = fm.width(titleAddText);
        pixPaint.drawText((splashSize.width()/devicePixelRatio - titleAddTextWidth) / 2, 30, titleAddText);
    }

    pixPaint.end();

    // Set window title
    setWindowTitle(titleText + " " + titleAddText);

    // Make window transparent and frameless for floating effect
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    
    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), QSize(pixmap.size().width()/devicePixelRatio,pixmap.size().height()/devicePixelRatio));
    resize(r.size());
    setFixedSize(r.size());
    move(QApplication::desktop()->screenGeometry().center() - r.center());

    subscribeToCoreSignals();
}

SplashScreen::~SplashScreen()
{
    unsubscribeFromCoreSignals();
}

void SplashScreen::slotFinish(QWidget *mainWin)
{
    Q_UNUSED(mainWin);

    /* If the window is minimized, hide() will be ignored. */
    /* Make sure we de-minimize the splashscreen window before hiding */
    if (isMinimized())
        showNormal();
    hide();
    deleteLater(); // No more need for this
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignBottom|Qt::AlignHCenter),
        Q_ARG(QColor, QColor(55,55,55)));
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress)
{
    InitMessage(splash, title + strprintf("%d", nProgress) + "%");
}

#ifdef ENABLE_WALLET
void SplashScreen::ConnectWallet(CWallet* wallet)
{
    wallet->ShowProgress.connect(boost::bind(ShowProgress, this,
                                             boost::placeholders::_1,
                                             boost::placeholders::_2));
    connectedWallets.push_back(wallet);
}
#endif

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.InitMessage.connect(boost::bind(InitMessage, this,
                                                boost::placeholders::_1));
    uiInterface.ShowProgress.connect(boost::bind(ShowProgress, this,
                                                 boost::placeholders::_1,boost::placeholders::_2));
#ifdef ENABLE_WALLET
    uiInterface.LoadWallet.connect(boost::bind(&SplashScreen::ConnectWallet, this,
                                               boost::placeholders::_1));
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.InitMessage.disconnect(boost::bind(InitMessage, this,
                                                   boost::placeholders::_1));
    uiInterface.ShowProgress.disconnect(boost::bind(ShowProgress, this,
                                                    boost::placeholders::_1,
                                                    boost::placeholders::_2));
#ifdef ENABLE_WALLET
    Q_FOREACH(CWallet* const & pwallet, connectedWallets) {
        pwallet->ShowProgress.disconnect(boost::bind(ShowProgress, this,
                                                     boost::placeholders::_1,
                                                     boost::placeholders::_2));
    }
#endif
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    
    // Draw status message higher up and in white
    if (!curMessage.isEmpty()) {
        QRect r = rect().adjusted(5, 5, -5, -5);
        // Move the text up by reducing the bottom margin
        r.setBottom(r.bottom() - 62); // Move up an additional 12 pixels (total 62)
        painter.setPen(QColor(255, 255, 255)); // White color
        painter.drawText(r, curAlignment, curMessage);
    }
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    StartShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
