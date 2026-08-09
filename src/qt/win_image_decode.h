// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_WIN_IMAGE_DECODE_H
#define DOGECOIN_QT_WIN_IMAGE_DECODE_H

#include <QByteArray>
#include <QImage>

/**
 * Decode image bytes (JPEG/PNG/GIF/…) on Windows via WIC when Qt was built
 * without JPEG plugins (-no-libjpeg / -no-feature-imageformat_jpeg in depends).
 * Returns a null QImage on failure or non-Windows.
 */
QImage DecodeImageBytesWin(const QByteArray& bytes);

#endif // DOGECOIN_QT_WIN_IMAGE_DECODE_H
