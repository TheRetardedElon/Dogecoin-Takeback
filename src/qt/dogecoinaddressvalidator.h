// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_DOGECOINADDRESSVALIDATOR_H
#define DOGECOIN_QT_DOGECOINADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class DogecoinAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit DogecoinAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

/** Bitcoin address widget validator, checks for a valid bitcoin address.
 */
class DogecoinAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit DogecoinAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const;
};

#endif // DOGECOIN_QT_DOGECOINADDRESSVALIDATOR_H
