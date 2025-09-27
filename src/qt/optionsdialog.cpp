// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "bitcoingui.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "thememanager.h"
#include "themeswitcher.h"

#include "validation.h" // for DEFAULT_SCRIPTCHECK_THREADS and MAX_SCRIPTCHECK_THREADS
#include "netbase.h"
#include "txdb.h" // for -dbcache defaults

#ifdef ENABLE_WALLET
#include "wallet/wallet.h" // for CWallet::GetRequiredFee()
#endif

#include <QDataWidgetMapper>
#include <QDir>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>

OptionsDialog::OptionsDialog(QWidget *parent, bool enableWallet) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    model(0),
    mapper(0)
{
    ui->setupUi(this);

    // Initialize theme switcher
    if (ui->themeSwitcher) {
        ui->themeSwitcher->loadSettings();
        connect(ui->themeSwitcher, &ThemeSwitcher::themeChanged, this, [this]() {
            // Apply theme changes immediately
            ThemeManager::instance()->applyTheme(this);
        });
    }

    /* Main elements init - minimal version for compilation */
    // Most UI elements commented out due to UI changes

    /* Network elements init - minimal version */
    // Most network elements commented out

    /* Window elements init - minimal version */
    // Most window elements commented out

    /* Display elements init - minimal version */
    // Most display elements commented out

    /* Widget-to-option mapper */
    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    /* setup/change UI elements when wallet is disabled/disabled */
    setWalletEnabled(enableWallet);
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setModel(OptionsModel *model)
{
    this->model = model;

    if(model)
    {
        connect(model, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        mapper->setModel(model);
        setMapper();
        mapper->toFirst();
    }

    /* update the display unit, to not use the default ("DOGE") */
    updateDisplayUnit();

    /* warn when one of the following options changes by user action (preserve the name of the signal):
     *
     * - Wallet (disable wallet functionality)
     * - Language (change the language)
     * - Proxy (use a proxy for IP-addresses)
     * - Port (connect to a different port)
     * - Separate (separate port for Tor)
     * - Connect (connect through SOCKS5 proxy)
     * - Min (minimize to the system tray)
     * - Minimize (minimize on close)
     * - if one of the following options changes by user action (preserve the name of the signal):
     *
     * - Language (change the language)
     * - Proxy (use a proxy for IP-addresses)
     * - Port (connect to a different port)
     * - Separate (separate port for Tor)
     * - Connect (connect through SOCKS5 proxy)
     * - Min (minimize to the system tray)
     * - Minimize (minimize on close)
     */

    /* disable apply button after settings are loaded as there is nothing to save */
    disableApplyButton();
}

void OptionsDialog::setMapper()
{
    /* Main */
    // mapper->addMapping(ui->bitcoinAtStartup, OptionsModel::StartAtStartup);
    // mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    // mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);
    // mapper->addMapping(ui->prune, OptionsModel::Prune);
    // mapper->addMapping(ui->pruneSize, OptionsModel::PruneSize);

    /* Wallet */
    // mapper->addMapping(ui->spendZeroConfChange, OptionsModel::SpendZeroConfChange);
    // mapper->addMapping(ui->coinControlFeatures, OptionsModel::CoinControlFeatures);

    /* Network */
    // mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
    // mapper->addMapping(ui->allowIncoming, OptionsModel::Listen);

    // mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    // mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    // mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

    /* Window */
    // mapper->addMapping(ui->hideTrayIcon, OptionsModel::HideTrayIcon);
    // mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    // mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);

    /* Display */
    // mapper->addMapping(ui->lang, OptionsModel::Language);
    // mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    // mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);
}

void OptionsDialog::enableApplyButton()
{
    // ui->applyButton->setEnabled(true); // Commented out - button not in UI
}

void OptionsDialog::disableApplyButton()
{
    // ui->applyButton->setEnabled(false); // Commented out - button not in UI
}

void OptionsDialog::enableSaveButtons()
{
    /* prevent enabling of the save buttons when data modified, if there is an invalid content */
    // if(fInvalidIpPort) // Commented out - variable not defined
    //     return;

    // ui->applyButton->setEnabled(true); // Commented out - button not in UI
    // ui->okButton->setEnabled(true); // Commented out - button not in UI
}

void OptionsDialog::disableSaveButtons()
{
    // ui->applyButton->setEnabled(false); // Commented out - button not in UI
    // ui->okButton->setEnabled(false); // Commented out - button not in UI
}

void OptionsDialog::setSaveButtonState(bool fState)
{
    if(fState)
        enableSaveButtons();
    else
        disableSaveButtons();
}

void OptionsDialog::on_okButton_clicked()
{
    mapper->submit();
    
    // Save theme settings
    if (ui->themeSwitcher) {
        ui->themeSwitcher->saveSettings();
    }
    
    accept();
    updateDefaultProxyNets();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_applyButton_clicked()
{
    mapper->submit();
    disableApplyButton();
}

void OptionsDialog::showRestartWarning(bool fPersistent)
{
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");

    if(fPersistent)
    {
        ui->statusLabel->setText(tr("Client restart required to activate changes."));
    }
    else
    {
        ui->statusLabel->setText(tr("This change would require a client restart."));
    }
}

// void OptionsDialog::clearStatusLabel()
// {
//     ui->statusLabel->clear();
// }

void OptionsDialog::updateDisplayUnit()
{
    if(model)
    {
        /* Update transactionFee with the current unit */
        // ui->transactionFee->setDisplayUnit(model->getDisplayUnit());
    }
}

void OptionsDialog::doRestart()
{
    // QSettings settings; // Commented out - not needed for minimal version
    // settings.setValue("fRestartRequired", true);
    // QApplication::quit();
}

void OptionsDialog::updateProxyValidationState()
{
    QValidatedLineEdit *pUiProxyIp = nullptr; // ui->proxyIp;
    int nProxyPort = 0; // ui->proxyPort->text().toInt();
    if (pUiProxyIp && (pUiProxyIp->isValid() && (!nProxyPort || nProxyPort > 0))) {
        // clearStatusLabel(); // Commented out - method not implemented
    }
}

void OptionsDialog::updateDefaultProxyNets()
{
    // Update default proxy networks
}

void OptionsDialog::setWalletEnabled(bool fHaveWallet)
{
    // ui->walletFrame->setVisible(fHaveWallet); // Commented out - frame not in UI
    if (fHaveWallet) {
        // ui->walletFrame->setEnabled(true);
    } else {
        // ui->walletFrame->setEnabled(false);
    }
}

void OptionsDialog::on_hideTrayIcon_stateChanged(int fState)
{
    if(fState)
    {
        // ui->minimizeToTray->setChecked(false);
        // ui->minimizeToTray->setEnabled(false);
    }
    else
    {
        // ui->minimizeToTray->setEnabled(true);
    }
}

void OptionsDialog::togglePruneWarning(bool fEnabled)
{
    // ui->pruneWarning->setVisible(!ui->pruneWarning->isVisible());
}

void OptionsDialog::setOkButtonState(bool fState)
{
    // ui->okButton->setEnabled(fState); // Commented out - button not in UI
}

void OptionsDialog::on_resetButton_clicked()
{
    if(model)
    {
        // confirmation dialog
        QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr("Confirm options reset"),
            tr("Client restart required to activate changes.") + "<br><br>" + tr("Client will be shut down. Do you want to proceed?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if(btnRetVal == QMessageBox::Cancel)
            return;

        /* reset all options and close GUI */
        model->Reset();
        QApplication::quit();
    }
}

void OptionsDialog::clearStatusLabel()
{
    // ui->statusLabel->clear(); // Commented out - label not in UI
}

// ProxyAddressValidator implementation
ProxyAddressValidator::ProxyAddressValidator(QObject *parent) :
QValidator(parent)
{
}

QValidator::State ProxyAddressValidator::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);
    // Validate the proxy
    CService serv(LookupNumeric(input.toStdString(), 9050));
    proxyType addrProxy = proxyType(serv, true);
    if (addrProxy.IsValid())
        return QValidator::Acceptable;

    return QValidator::Invalid;
}
