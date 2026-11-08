/*
 * Lightning Desk -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Schomp Technologies LLC
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "net/License.h"

#include "common/Settings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

namespace deskflow {

namespace {

// Ed25519 public key of the Schomp Technologies license signer. The private half lives in the
// Lightning vault (key:lightning-desk:license-signing). Replacing this constant invalidates every
// issued license, so treat it as part of the protocol.
constexpr char kLicensePublicKeyPem[] = "-----BEGIN PUBLIC KEY-----\n"
                                        "MCowBQYDK2VwAyEAqEWEdb/4mJ+pfAOxITMlXGnbDNFmOEjOn58S0GC5BMk=\n"
                                        "-----END PUBLIC KEY-----\n";

// The signed payload is the fields joined by '|' in this order, so clients and the issuing
// tool never disagree about JSON whitespace or key order.
QByteArray payloadFor(const QJsonObject &o)
{
  return QStringLiteral("%1|%2|%3|%4|%5")
      .arg(
          o.value(QStringLiteral("email")).toString(), o.value(QStringLiteral("tier")).toString(),
          QString::number(o.value(QStringLiteral("max_extra")).toInt()), o.value(QStringLiteral("exp")).toString(),
          o.value(QStringLiteral("issued")).toString()
      )
      .toUtf8();
}

} // namespace

QString License::licensePath()
{
  return QStringLiteral("%1/license.json").arg(Settings::settingsPath());
}

const License &License::current(bool reload)
{
  static License s_license;
  static bool s_loaded = false;
  if (!s_loaded || reload) {
    s_loaded = true;
    QFile f(licensePath());
    if (f.open(QIODevice::ReadOnly)) {
      s_license = fromJson(f.readAll());
    } else {
      s_license = License();
    }
  }
  return s_license;
}

License License::fromJson(const QByteArray &json)
{
  License lic;
  QJsonParseError err;
  const auto doc = QJsonDocument::fromJson(json, &err);
  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    lic.m_problem = QStringLiteral("license.json is not valid JSON");
    return lic;
  }
  const auto o = doc.object();
  lic.m_email = o.value(QStringLiteral("email")).toString();
  lic.m_tier = o.value(QStringLiteral("tier")).toString();
  lic.m_maxExtra = o.value(QStringLiteral("max_extra")).toInt(kFreeExtraComputers);
  const auto exp = o.value(QStringLiteral("exp")).toString();
  if (!exp.isEmpty()) {
    lic.m_expires = QDateTime::fromString(exp, Qt::ISODate);
  }
  const auto sig = QByteArray::fromBase64(o.value(QStringLiteral("sig")).toString().toLatin1());

  if (lic.m_email.isEmpty() || lic.m_tier.isEmpty() || sig.isEmpty()) {
    lic.m_problem = QStringLiteral("license.json is missing fields");
    return lic;
  }
  if (!verify(payloadFor(o), sig)) {
    lic.m_problem = QStringLiteral("license.json signature does not match");
    return lic;
  }
  if (lic.m_expires.isValid() && lic.m_expires < QDateTime::currentDateTimeUtc()) {
    lic.m_problem = QStringLiteral("license expired on %1").arg(lic.m_expires.toString(Qt::ISODate));
    return lic;
  }
  lic.m_maxExtra = qBound(0, lic.m_maxExtra, 64);
  lic.m_valid = true;
  return lic;
}

bool License::verify(const QByteArray &payload, const QByteArray &signature)
{
  BIO *bio = BIO_new_mem_buf(kLicensePublicKeyPem, -1);
  if (bio == nullptr) {
    return false;
  }
  EVP_PKEY *key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (key == nullptr) {
    return false;
  }
  bool ok = false;
  if (EVP_MD_CTX *ctx = EVP_MD_CTX_new(); ctx != nullptr) {
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1) {
      ok = EVP_DigestVerify(
               ctx, reinterpret_cast<const unsigned char *>(signature.constData()), signature.size(),
               reinterpret_cast<const unsigned char *>(payload.constData()), payload.size()
           ) == 1;
    }
    EVP_MD_CTX_free(ctx);
  }
  EVP_PKEY_free(key);
  return ok;
}

} // namespace deskflow
