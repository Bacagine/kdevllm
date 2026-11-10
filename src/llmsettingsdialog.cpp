/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *   GPL v2 or later                                                       *
 ***************************************************************************/

#include "llmsettingsdialog.h"

#include <QDesktopServices>
#include <QUrl>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTabWidget>
#include <QDialogButtonBox>

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

LLMSettingsDialog::LLMSettingsDialog(LLMCredentialStore* store, QWidget* parent)
    : QDialog(parent)
    , m_store(store)
    , m_testClient(new LLMClient(this))
{
    setupUi(this);

    // Browser buttons
    connect(btnOpenBrowserOpenAI,   SIGNAL(clicked()), this, SLOT(onOpenBrowserOpenAI()));
    connect(btnOpenBrowserAnthropic,SIGNAL(clicked()), this, SLOT(onOpenBrowserAnthropic()));
    connect(btnOpenBrowserOllama,   SIGNAL(clicked()), this, SLOT(onOpenBrowserOllama()));

    // Connect/disconnect
    connect(btnConnectOpenAI,    SIGNAL(clicked()), this, SLOT(onConnectOpenAI()));
    connect(btnDisconnectOpenAI, SIGNAL(clicked()), this, SLOT(onDisconnectOpenAI()));
    connect(btnConnectAnthropic,    SIGNAL(clicked()), this, SLOT(onConnectAnthropic()));
    connect(btnDisconnectAnthropic, SIGNAL(clicked()), this, SLOT(onDisconnectAnthropic()));
    connect(btnConnectOllama, SIGNAL(clicked()), this, SLOT(onConnectOllama()));

    // Show/hide keys
    connect(btnShowOpenAI,   SIGNAL(clicked()), this, SLOT(onToggleShowOpenAI()));
    connect(btnShowAnthropic,SIGNAL(clicked()), this, SLOT(onToggleShowAnthropic()));

    // Test client
    connect(m_testClient, SIGNAL(responseReady(QString)), this, SLOT(onTestResponse(QString)));
    connect(m_testClient, SIGNAL(errorOccurred(QString)), this, SLOT(onTestError(QString)));

    // Dialog buttons
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(onAccepted()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Wallet
    connect(m_store, SIGNAL(walletOpened(bool)), this, SLOT(onWalletOpened(bool)));

    // Load saved model names from KConfig (non-sensitive)
    KConfigGroup grp(kdevllmConfig(), QStringLiteral("kdevllm"));
    editModelOpenAI->setText(grp.readEntry("modelOpenAI",
        QStringLiteral("gpt-4o")));
    editModelAnthropic->setText(grp.readEntry("modelAnthropic",
        QStringLiteral("claude-3-5-sonnet-20241022")));
    editModelOllama->setText(grp.readEntry("modelOllama",
        QStringLiteral("codellama")));
    editEndpointOllama->setText(grp.readEntry("endpointOllama",
        QStringLiteral("http://localhost:11434")));

    // Active provider
    comboActiveProvider->setCurrentIndex(grp.readEntry("provider", 0));

    // Open wallet to check existing connections
    m_store->openWallet(winId());
}

LLMSettingsDialog::~LLMSettingsDialog()
{
}

void LLMSettingsDialog::onWalletOpened(bool ok)
{
    if (!ok) return;
    // Refresh status indicators
    updateStatus(QStringLiteral("OpenAI"),
                 m_store->isConnected(QStringLiteral("OpenAI")));
    updateStatus(QStringLiteral("Anthropic"),
                 m_store->isConnected(QStringLiteral("Anthropic")));
    // Populate key fields (masked) so user knows they're set
    const QString openAiKey = m_store->loadKey(QStringLiteral("OpenAI"));
    if (!openAiKey.isEmpty())
        editKeyOpenAI->setText(openAiKey);
    const QString anthropicKey = m_store->loadKey(QStringLiteral("Anthropic"));
    if (!anthropicKey.isEmpty())
        editKeyAnthropic->setText(anthropicKey);
}

LLMClient::Config LLMSettingsDialog::currentConfig() const
{
    LLMClient::Config cfg;
    cfg.provider  = static_cast<LLMClient::Provider>(comboActiveProvider->currentIndex());
    cfg.maxTokens = 4096;

    switch (cfg.provider) {
    case LLMClient::OpenAI:
        cfg.apiKey = m_store->loadKey(QStringLiteral("OpenAI"));
        cfg.model  = editModelOpenAI->text().trimmed();
        break;
    case LLMClient::Anthropic:
        cfg.apiKey = m_store->loadKey(QStringLiteral("Anthropic"));
        cfg.model  = editModelAnthropic->text().trimmed();
        break;
    case LLMClient::Ollama:
        cfg.endpoint = editEndpointOllama->text().trimmed();
        cfg.model    = editModelOllama->text().trimmed();
        break;
    }
    return cfg;
}

void LLMSettingsDialog::onAccepted()
{
    // Save non-sensitive config
    KConfigGroup grp(kdevllmConfig(), QStringLiteral("kdevllm"));
    grp.writeEntry("provider",       comboActiveProvider->currentIndex());
    grp.writeEntry("modelOpenAI",    editModelOpenAI->text().trimmed());
    grp.writeEntry("modelAnthropic", editModelAnthropic->text().trimmed());
    grp.writeEntry("modelOllama",    editModelOllama->text().trimmed());
    grp.writeEntry("endpointOllama", editEndpointOllama->text().trimmed());
    grp.sync();
    accept();
}

// ---------------------------------------------------------------------------
// Browser
// ---------------------------------------------------------------------------

void LLMSettingsDialog::openBrowser(const QString& url)
{
    QDesktopServices::openUrl(QUrl(url));
}

void LLMSettingsDialog::onOpenBrowserOpenAI()
{
    openBrowser(LLMCredentialStore::apiKeyUrl(QStringLiteral("OpenAI")));
}

void LLMSettingsDialog::onOpenBrowserAnthropic()
{
    openBrowser(LLMCredentialStore::apiKeyUrl(QStringLiteral("Anthropic")));
}

void LLMSettingsDialog::onOpenBrowserOllama()
{
    openBrowser(QStringLiteral("https://ollama.com/download"));
}

// ---------------------------------------------------------------------------
// Connect / Disconnect
// ---------------------------------------------------------------------------

void LLMSettingsDialog::onConnectOpenAI()
{
    const QString key = editKeyOpenAI->text().trimmed();
    if (key.isEmpty()) {
        labelStatusOpenAI->setText(
            i18n("<font color='red'>Paste your API key first</font>"));
        return;
    }
    // Test before saving
    m_testingProvider = QStringLiteral("OpenAI");
    LLMClient::Config cfg;
    cfg.provider  = LLMClient::OpenAI;
    cfg.apiKey    = key;
    cfg.model     = editModelOpenAI->text().trimmed();
    cfg.maxTokens = 16;
    m_testClient->setConfig(cfg);
    QList<QPair<QString,QString>> msgs;
    msgs.append(qMakePair(QStringLiteral("user"), QStringLiteral("Say only: OK")));
    labelStatusOpenAI->setText(i18n("Testing…"));
    btnConnectOpenAI->setEnabled(false);
    m_testClient->sendMessage(QString(), msgs);
}

void LLMSettingsDialog::onDisconnectOpenAI()
{
    m_store->deleteKey(QStringLiteral("OpenAI"));
    editKeyOpenAI->clear();
    updateStatus(QStringLiteral("OpenAI"), false);
}

void LLMSettingsDialog::onConnectAnthropic()
{
    const QString key = editKeyAnthropic->text().trimmed();
    if (key.isEmpty()) {
        labelStatusAnthropic->setText(
            i18n("<font color='red'>Paste your API key first</font>"));
        return;
    }
    m_testingProvider = QStringLiteral("Anthropic");
    LLMClient::Config cfg;
    cfg.provider  = LLMClient::Anthropic;
    cfg.apiKey    = key;
    cfg.model     = editModelAnthropic->text().trimmed();
    cfg.maxTokens = 16;
    m_testClient->setConfig(cfg);
    QList<QPair<QString,QString>> msgs;
    msgs.append(qMakePair(QStringLiteral("user"), QStringLiteral("Say only: OK")));
    labelStatusAnthropic->setText(i18n("Testing…"));
    btnConnectAnthropic->setEnabled(false);
    m_testClient->sendMessage(QString(), msgs);
}

void LLMSettingsDialog::onDisconnectAnthropic()
{
    m_store->deleteKey(QStringLiteral("Anthropic"));
    editKeyAnthropic->clear();
    updateStatus(QStringLiteral("Anthropic"), false);
}

void LLMSettingsDialog::onConnectOllama()
{
    m_testingProvider = QStringLiteral("Ollama");
    LLMClient::Config cfg;
    cfg.provider  = LLMClient::Ollama;
    cfg.endpoint  = editEndpointOllama->text().trimmed();
    cfg.model     = editModelOllama->text().trimmed();
    cfg.maxTokens = 16;
    m_testClient->setConfig(cfg);
    QList<QPair<QString,QString>> msgs;
    msgs.append(qMakePair(QStringLiteral("user"), QStringLiteral("Say only: OK")));
    labelStatusOllama->setText(i18n("Testing…"));
    btnConnectOllama->setEnabled(false);
    m_testClient->sendMessage(QString(), msgs);
}

// ---------------------------------------------------------------------------
// Test responses
// ---------------------------------------------------------------------------

void LLMSettingsDialog::onTestResponse(const QString&)
{
    if (m_testingProvider == QLatin1String("OpenAI")) {
        m_store->saveKey(QStringLiteral("OpenAI"), editKeyOpenAI->text().trimmed());
        updateStatus(QStringLiteral("OpenAI"), true);
        btnConnectOpenAI->setEnabled(true);
    } else if (m_testingProvider == QLatin1String("Anthropic")) {
        m_store->saveKey(QStringLiteral("Anthropic"), editKeyAnthropic->text().trimmed());
        updateStatus(QStringLiteral("Anthropic"), true);
        btnConnectAnthropic->setEnabled(true);
    } else if (m_testingProvider == QLatin1String("Ollama")) {
        labelStatusOllama->setText(
            i18n("<font color='green'>Connected ✓</font>"));
        btnConnectOllama->setEnabled(true);
    }
}

void LLMSettingsDialog::onTestError(const QString& error)
{
    const QString msg =
        i18n("<font color='red'>Error: %1</font>").arg(error);
    if (m_testingProvider == QLatin1String("OpenAI")) {
        labelStatusOpenAI->setText(msg);
        btnConnectOpenAI->setEnabled(true);
    } else if (m_testingProvider == QLatin1String("Anthropic")) {
        labelStatusAnthropic->setText(msg);
        btnConnectAnthropic->setEnabled(true);
    } else {
        labelStatusOllama->setText(msg);
        btnConnectOllama->setEnabled(true);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void LLMSettingsDialog::updateStatus(const QString& provider, bool connected)
{
    if (provider == QLatin1String("OpenAI")) {
        labelStatusOpenAI->setText(connected
            ? i18n("<font color='green'>Connected ✓</font>")
            : i18n("Not connected"));
        btnDisconnectOpenAI->setEnabled(connected);
    } else if (provider == QLatin1String("Anthropic")) {
        labelStatusAnthropic->setText(connected
            ? i18n("<font color='green'>Connected ✓</font>")
            : i18n("Not connected"));
        btnDisconnectAnthropic->setEnabled(connected);
    }
}

void LLMSettingsDialog::onToggleShowOpenAI()
{
    editKeyOpenAI->setEchoMode(
        btnShowOpenAI->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
    btnShowOpenAI->setText(btnShowOpenAI->isChecked()
        ? i18n("Hide") : i18n("Show"));
}

void LLMSettingsDialog::onToggleShowAnthropic()
{
    editKeyAnthropic->setEchoMode(
        btnShowAnthropic->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
    btnShowAnthropic->setText(btnShowAnthropic->isChecked()
        ? i18n("Hide") : i18n("Show"));
}
