/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *   GPL v2 or later                                                       *
 ***************************************************************************/

#ifndef KDEVLLM_LLMCHATWIDGET_H
#define KDEVLLM_LLMCHATWIDGET_H

#include <QWidget>
#include <QList>
#include <QPair>
#include <QString>
#include "llmclient.h"
#include "ui_llmchatwidget.h"

namespace KDevelop { class IPlugin; }

class LLMChatWidget : public QWidget, private Ui::LLMChatWidgetBase
{
    Q_OBJECT
public:
    explicit LLMChatWidget(KDevelop::IPlugin* plugin, QWidget* parent = nullptr);
    ~LLMChatWidget() override;

    void setClient(LLMClient* client);
    void updateStatusLabel();

public Q_SLOTS:
    void onConfigChanged();

private Q_SLOTS:
    void onSendClicked();
    void onSendFileClicked();
    void onSendSelectionClicked();
    void onApplyToEditorClicked();
    void onClearClicked();
    void onSettingsClicked();
    void onResponseReady(const QString& text);
    void onErrorOccurred(const QString& error);
    void onRequestStarted();

private:
    void appendHtml(const QString& html);
    void appendUserMessage(const QString& text);
    void appendAssistantMessage(const QString& text);
    void appendSystemMessage(const QString& text);

    QString getCurrentFileContent() const;
    QString getSelectedText() const;
    void applyToEditor(const QString& text);

    // Extract the last code block from a markdown response
    static QString extractCodeBlock(const QString& response);

    KDevelop::IPlugin*             m_plugin;
    LLMClient*                     m_client;
    QList<QPair<QString,QString>>  m_history;
    QString                        m_lastResponse;
};

#endif // KDEVLLM_LLMCHATWIDGET_H
