/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *   GPL v2 or later                                                       *
 ***************************************************************************/

#ifndef KDEVLLM_LLMCREDENTIALSTORE_H
#define KDEVLLM_LLMCREDENTIALSTORE_H

#include <QObject>
#include <QString>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <KDE/KWallet/KWallet>
#else
#  include <KWallet/KWallet>
#endif

/**
 * Wraps KWallet for storing AI provider API keys.
 * Falls back to KConfig (obfuscated) when KWallet is unavailable.
 */
class LLMCredentialStore : public QObject
{
    Q_OBJECT
public:
    explicit LLMCredentialStore(QObject* parent = nullptr);
    ~LLMCredentialStore() override;

    // Returns the browser URL for the user to create/find their API key
    static QString apiKeyUrl(const QString& provider);
    static QString apiKeyHelp(const QString& provider);

    // Async: open wallet then emit walletOpened(success)
    void openWallet(WId wid = 0);
    bool isWalletOpen() const;

    // Synchronous store/retrieve (only call after walletOpened(true))
    bool saveKey(const QString& provider, const QString& key);
    QString loadKey(const QString& provider) const;
    bool deleteKey(const QString& provider);

    bool isConnected(const QString& provider) const;

Q_SIGNALS:
    void walletOpened(bool success);

private Q_SLOTS:
    void onWalletOpened(bool opened);

private:
    static QString walletFolder() { return QStringLiteral("kdevllm"); }

    KWallet::Wallet* m_wallet;
};

#endif // KDEVLLM_LLMCREDENTIALSTORE_H
