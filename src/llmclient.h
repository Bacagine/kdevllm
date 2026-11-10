/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or    *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef KDEVLLM_LLMCLIENT_H
#define KDEVLLM_LLMCLIENT_H

#include <QObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class LLMClient : public QObject
{
    Q_OBJECT
public:
    enum Provider {
        OpenAI,
        Anthropic,
        Ollama
    };

    struct Config {
        Provider provider;
        QString  apiKey;
        QString  endpoint;   // used by Ollama; defaults to http://localhost:11434
        QString  model;
        int      maxTokens;

        Config()
            : provider(OpenAI)
            , endpoint(QStringLiteral("http://localhost:11434"))
            , model(QStringLiteral("gpt-4o"))
            , maxTokens(4096)
        {}
    };

    explicit LLMClient(QObject* parent = nullptr);
    ~LLMClient() override;

    void setConfig(const Config& cfg);
    const Config& config() const;

    // messages: list of (role, content) pairs — role is "user" or "assistant"
    void sendMessage(const QString& systemPrompt,
                     const QList<QPair<QString,QString>>& messages);

    void cancelRequest();
    bool isBusy() const;

Q_SIGNALS:
    void requestStarted();
    void responseReady(const QString& text);
    void errorOccurred(const QString& error);

private Q_SLOTS:
    void onReplyFinished();

private:
    QByteArray buildOpenAIRequest(const QString& systemPrompt,
                                  const QList<QPair<QString,QString>>& messages) const;
    QByteArray buildAnthropicRequest(const QString& systemPrompt,
                                     const QList<QPair<QString,QString>>& messages) const;
    QByteArray buildOllamaRequest(const QString& systemPrompt,
                                  const QList<QPair<QString,QString>>& messages) const;

    QString parseOpenAIResponse(const QByteArray& data) const;
    QString parseAnthropicResponse(const QByteArray& data) const;
    QString parseOllamaResponse(const QByteArray& data) const;

    // Generic: extract first string value for a given JSON key
    static QString extractJsonString(const QByteArray& data, const QString& key);
    // Escape a string for embedding in a JSON literal
    static QString jsonEscape(const QString& str);
    // Build a JSON messages array from (role,content) pairs
    static QString buildMessagesArray(const QList<QPair<QString,QString>>& messages);

    QNetworkAccessManager* m_nam;
    QNetworkReply*         m_reply;
    Config                 m_config;
};

#endif // KDEVLLM_LLMCLIENT_H
