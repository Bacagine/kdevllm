/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *   GPL v2 or later                                                       *
 ***************************************************************************/

#ifndef KDEVLLM_LLMSETTINGSDIALOG_H
#define KDEVLLM_LLMSETTINGSDIALOG_H

#include <QDialog>
#include "llmclient.h"
#include "llmcredentialstore.h"
#include "ui_llmsettingsdialog.h"

class LLMSettingsDialog : public QDialog, private Ui::LLMSettingsDialogBase
{
    Q_OBJECT
public:
    explicit LLMSettingsDialog(LLMCredentialStore* store, QWidget* parent = nullptr);
    ~LLMSettingsDialog() override;

    // Returns config reflecting the current state; saves to wallet/config on OK
    LLMClient::Config currentConfig() const;

private Q_SLOTS:
    // Browser buttons
    void onOpenBrowserOpenAI();
    void onOpenBrowserAnthropic();
    void onOpenBrowserOllama();

    // Connect / disconnect
    void onConnectOpenAI();
    void onDisconnectOpenAI();
    void onConnectAnthropic();
    void onDisconnectAnthropic();
    void onConnectOllama();

    // Show/hide key
    void onToggleShowOpenAI();
    void onToggleShowAnthropic();

    // Test responses
    void onTestResponse(const QString& text);
    void onTestError(const QString& error);

    void onWalletOpened(bool ok);
    void onAccepted();

private:
    void updateStatus(const QString& provider, bool connected);
    void openBrowser(const QString& url);

    LLMCredentialStore* m_store;
    LLMClient*          m_testClient;
    QString             m_testingProvider;
};

#endif // KDEVLLM_LLMSETTINGSDIALOG_H
