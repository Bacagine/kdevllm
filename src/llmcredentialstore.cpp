/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *   GPL v2 or later                                                       *
 ***************************************************************************/

#include "llmcredentialstore.h"

#include <KLocalizedString>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <KGlobal>
#  include <KConfigGroup>
static KSharedConfigPtr kdevllmConfig() { return KGlobal::config(); }
#else
#  include <KSharedConfig>
#  include <KConfigGroup>
static KSharedConfigPtr kdevllmConfig() { return KSharedConfig::openConfig(); }
#endif

LLMCredentialStore::LLMCredentialStore(QObject* parent)
    : QObject(parent)
    , m_wallet(nullptr)
{
}

LLMCredentialStore::~LLMCredentialStore()
{
    delete m_wallet;
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QString LLMCredentialStore::apiKeyUrl(const QString& provider)
{
    if (provider == QLatin1String("OpenAI"))
        return QStringLiteral("https://platform.openai.com/api-keys");
    if (provider == QLatin1String("Anthropic"))
        return QStringLiteral("https://console.anthropic.com/settings/keys");
    return QString();
}

QString LLMCredentialStore::apiKeyHelp(const QString& provider)
{
    if (provider == QLatin1String("OpenAI"))
        return i18n(
            "<b>OpenAI API Key</b><br/>"
            "1. Click <i>Open Browser</i> — it opens platform.openai.com<br/>"
            "2. Log in with your OpenAI / ChatGPT account<br/>"
            "3. Click <i>Create new secret key</i><br/>"
            "4. Copy the key and paste it here<br/>"
            "<small>Free tier available. Keys start with <tt>sk-</tt></small>"
        );
    if (provider == QLatin1String("Anthropic"))
        return i18n(
            "<b>Anthropic API Key</b><br/>"
            "1. Click <i>Open Browser</i> — it opens console.anthropic.com<br/>"
            "2. Log in with your Anthropic / Claude account<br/>"
            "3. Under <i>API Keys</i>, click <i>Create Key</i><br/>"
            "4. Copy the key and paste it here<br/>"
            "<small>Keys start with <tt>sk-ant-</tt></small>"
        );
    if (provider == QLatin1String("Ollama"))
        return i18n(
            "<b>Ollama (local — no account needed)</b><br/>"
            "1. Install Ollama: <tt>curl -fsSL https://ollama.com/install.sh | sh</tt><br/>"
            "2. Pull a model: <tt>ollama pull codellama</tt><br/>"
            "3. Ollama runs at <tt>http://localhost:11434</tt> by default<br/>"
            "<small>No API key required for local use</small>"
        );
    return QString();
}

// ---------------------------------------------------------------------------
// Wallet management
// ---------------------------------------------------------------------------

void LLMCredentialStore::openWallet(WId wid)
{
    if (m_wallet && m_wallet->isOpen()) {
        emit walletOpened(true);
        return;
    }
    delete m_wallet;
    m_wallet = KWallet::Wallet::openWallet(
        KWallet::Wallet::NetworkWallet(), wid,
        KWallet::Wallet::Asynchronous);

    if (m_wallet) {
        connect(m_wallet, SIGNAL(walletOpened(bool)),
                this,     SLOT(onWalletOpened(bool)));
    } else {
        emit walletOpened(false);
    }
}

bool LLMCredentialStore::isWalletOpen() const
{
    return m_wallet && m_wallet->isOpen();
}

void LLMCredentialStore::onWalletOpened(bool opened)
{
    if (opened && m_wallet) {
        if (!m_wallet->hasFolder(walletFolder()))
            m_wallet->createFolder(walletFolder());
        m_wallet->setFolder(walletFolder());
    }
    emit walletOpened(opened);
}

// ---------------------------------------------------------------------------
// Store / retrieve
// ---------------------------------------------------------------------------

bool LLMCredentialStore::saveKey(const QString& provider, const QString& key)
{
    if (m_wallet && m_wallet->isOpen()) {
        m_wallet->setFolder(walletFolder());
        return m_wallet->writePassword(provider, key) == 0;
    }
    // Fallback: store in KConfig (not encrypted but better than nothing)
    KConfigGroup grp(kdevllmConfig(), QStringLiteral("kdevllm_keys"));
    grp.writeEntry(provider, key);
    grp.sync();
    return true;
}

QString LLMCredentialStore::loadKey(const QString& provider) const
{
    if (m_wallet && m_wallet->isOpen()) {
        m_wallet->setFolder(walletFolder());
        QString key;
        m_wallet->readPassword(provider, key);
        return key;
    }
    // Fallback: read from KConfig
    KConfigGroup grp(kdevllmConfig(), QStringLiteral("kdevllm_keys"));
    return grp.readEntry(provider, QString());
}

bool LLMCredentialStore::deleteKey(const QString& provider)
{
    if (m_wallet && m_wallet->isOpen()) {
        m_wallet->setFolder(walletFolder());
        return m_wallet->removeEntry(provider) == 0;
    }
    KConfigGroup grp(kdevllmConfig(), QStringLiteral("kdevllm_keys"));
    grp.deleteEntry(provider);
    grp.sync();
    return true;
}

bool LLMCredentialStore::isConnected(const QString& provider) const
{
    return !loadKey(provider).isEmpty();
}
