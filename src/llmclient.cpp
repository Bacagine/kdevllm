/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or    *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "llmclient.h"

#include <QNetworkRequest>
#include <QUrl>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include "debug_kde4.h"
#define PLUGIN_KDEVLLM ""
#else
#include "debug.h"
#endif

LLMClient::LLMClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_reply(nullptr)
{
}

LLMClient::~LLMClient()
{
    cancelRequest();
}

void LLMClient::setConfig(const Config& cfg)
{
    m_config = cfg;
}

const LLMClient::Config& LLMClient::config() const
{
    return m_config;
}

bool LLMClient::isBusy() const
{
    return m_reply != nullptr;
}

void LLMClient::cancelRequest()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void LLMClient::sendMessage(const QString& systemPrompt,
                             const QList<QPair<QString,QString>>& messages)
{
    if (isBusy()) {
        cancelRequest();
    }

    QUrl url;
    QNetworkRequest req;
    QByteArray body;

    switch (m_config.provider) {
    case OpenAI:
        url = QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions"));
        req.setUrl(url);
        req.setRawHeader("Authorization",
            QByteArray("Bearer ") + m_config.apiKey.toUtf8());
        req.setHeader(QNetworkRequest::ContentTypeHeader,
            QByteArray("application/json"));
        body = buildOpenAIRequest(systemPrompt, messages);
        break;

    case Anthropic:
        url = QUrl(QStringLiteral("https://api.anthropic.com/v1/messages"));
        req.setUrl(url);
        req.setRawHeader("x-api-key", m_config.apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
        req.setHeader(QNetworkRequest::ContentTypeHeader,
            QByteArray("application/json"));
        body = buildAnthropicRequest(systemPrompt, messages);
        break;

    case Ollama: {
        QString ep = m_config.endpoint;
        if (!ep.endsWith(QLatin1Char('/')))
            ep += QLatin1Char('/');
        url = QUrl(ep + QStringLiteral("api/chat"));
        req.setUrl(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
            QByteArray("application/json"));
        body = buildOllamaRequest(systemPrompt, messages);
        break;
    }
    }

    qCDebug(PLUGIN_KDEVLLM) << "LLMClient: POST to" << url.toString();

    m_reply = m_nam->post(req, body);
    connect(m_reply, &QNetworkReply::finished,
            this, &LLMClient::onReplyFinished);

    emit requestStarted();
}

void LLMClient::onReplyFinished()
{
    if (!m_reply) return;

    const int httpStatus =
        m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = m_reply->readAll();

    qCDebug(PLUGIN_KDEVLLM) << "LLMClient: HTTP" << httpStatus
                              << ", body size:" << data.size();

    if (m_reply->error() != QNetworkReply::NoError && httpStatus == 0) {
        const QString errMsg = m_reply->errorString();
        m_reply->deleteLater();
        m_reply = nullptr;
        emit errorOccurred(errMsg);
        return;
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    if (httpStatus >= 400) {
        // Surface the error message from the API body if present
        QString apiErr = extractJsonString(data, QStringLiteral("message"));
        if (apiErr.isEmpty())
            apiErr = QString::fromUtf8(data).left(300);
        emit errorOccurred(QString("HTTP %1: %2").arg(httpStatus).arg(apiErr));
        return;
    }

    QString result;
    switch (m_config.provider) {
    case OpenAI:    result = parseOpenAIResponse(data);    break;
    case Anthropic: result = parseAnthropicResponse(data); break;
    case Ollama:    result = parseOllamaResponse(data);    break;
    }

    if (result.isEmpty()) {
        emit errorOccurred(tr("Could not parse response from server."));
    } else {
        emit responseReady(result);
    }
}

// ---------------------------------------------------------------------------
// Request builders
// ---------------------------------------------------------------------------

QString LLMClient::jsonEscape(const QString& s)
{
    QString out;
    out.reserve(s.size() + 16);
    for (int ci = 0; ci < s.length(); ++ci) {
        const QChar c = s.at(ci);
        if (c == QLatin1Char('"'))       out += QStringLiteral("\\\"");
        else if (c == QLatin1Char('\\')) out += QStringLiteral("\\\\");
        else if (c == QLatin1Char('\n')) out += QStringLiteral("\\n");
        else if (c == QLatin1Char('\r')) out += QStringLiteral("\\r");
        else if (c == QLatin1Char('\t')) out += QStringLiteral("\\t");
        else                             out += c;
    }
    return out;
}

QString LLMClient::buildMessagesArray(const QList<QPair<QString,QString>>& messages)
{
    QString arr = QStringLiteral("[");
    for (int i = 0; i < messages.size(); ++i) {
        if (i > 0) arr += QLatin1Char(',');
        arr += QStringLiteral("{\"role\":\"") + jsonEscape(messages[i].first)
             + QStringLiteral("\",\"content\":\"") + jsonEscape(messages[i].second)
             + QStringLiteral("\"}");
    }
    arr += QLatin1Char(']');
    return arr;
}

QByteArray LLMClient::buildOpenAIRequest(const QString& systemPrompt,
                                          const QList<QPair<QString,QString>>& messages) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonArray msgArr;
    if (!systemPrompt.isEmpty()) {
        QJsonObject sys;
        sys["role"]    = QStringLiteral("system");
        sys["content"] = systemPrompt;
        msgArr.append(sys);
    }
    for (const auto& m : messages) {
        QJsonObject msg;
        msg["role"]    = m.first;
        msg["content"] = m.second;
        msgArr.append(msg);
    }
    QJsonObject root;
    root["model"]      = m_config.model;
    root["messages"]   = msgArr;
    root["max_tokens"] = m_config.maxTokens;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
#else
    QString msgs;
    if (!systemPrompt.isEmpty()) {
        msgs += QStringLiteral("{\"role\":\"system\",\"content\":\"")
              + jsonEscape(systemPrompt) + QStringLiteral("\"}");
        if (!messages.isEmpty()) msgs += QLatin1Char(',');
    }
    msgs += buildMessagesArray(messages).mid(1);  // strip leading '['
    msgs.chop(1);  // strip trailing ']' so we can wrap it

    return (QStringLiteral("{\"model\":\"") + jsonEscape(m_config.model)
          + QStringLiteral("\",\"max_tokens\":") + QString::number(m_config.maxTokens)
          + QStringLiteral(",\"messages\":[") + msgs
          + QStringLiteral("]}")).toUtf8();
#endif
}

QByteArray LLMClient::buildAnthropicRequest(const QString& systemPrompt,
                                              const QList<QPair<QString,QString>>& messages) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonArray msgArr;
    for (const auto& m : messages) {
        QJsonObject msg;
        msg["role"]    = m.first;
        msg["content"] = m.second;
        msgArr.append(msg);
    }
    QJsonObject root;
    root["model"]      = m_config.model;
    root["max_tokens"] = m_config.maxTokens;
    root["messages"]   = msgArr;
    if (!systemPrompt.isEmpty())
        root["system"] = systemPrompt;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
#else
    QString body = QStringLiteral("{\"model\":\"") + jsonEscape(m_config.model)
                 + QStringLiteral("\",\"max_tokens\":") + QString::number(m_config.maxTokens)
                 + QStringLiteral(",\"messages\":") + buildMessagesArray(messages);
    if (!systemPrompt.isEmpty())
        body += QStringLiteral(",\"system\":\"") + jsonEscape(systemPrompt) + QLatin1Char('"');
    body += QLatin1Char('}');
    return body.toUtf8();
#endif
}

QByteArray LLMClient::buildOllamaRequest(const QString& systemPrompt,
                                          const QList<QPair<QString,QString>>& messages) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonArray msgArr;
    if (!systemPrompt.isEmpty()) {
        QJsonObject sys;
        sys["role"]    = QStringLiteral("system");
        sys["content"] = systemPrompt;
        msgArr.append(sys);
    }
    for (const auto& m : messages) {
        QJsonObject msg;
        msg["role"]    = m.first;
        msg["content"] = m.second;
        msgArr.append(msg);
    }
    QJsonObject root;
    root["model"]    = m_config.model;
    root["messages"] = msgArr;
    root["stream"]   = false;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
#else
    QString msgs;
    if (!systemPrompt.isEmpty()) {
        msgs += QStringLiteral("{\"role\":\"system\",\"content\":\"")
              + jsonEscape(systemPrompt) + QStringLiteral("\"}");
        if (!messages.isEmpty()) msgs += QLatin1Char(',');
    }
    msgs += buildMessagesArray(messages).mid(1);
    msgs.chop(1);

    return (QStringLiteral("{\"model\":\"") + jsonEscape(m_config.model)
          + QStringLiteral("\",\"stream\":false,\"messages\":[") + msgs
          + QStringLiteral("]}")).toUtf8();
#endif
}

// ---------------------------------------------------------------------------
// Response parsers
// ---------------------------------------------------------------------------

// Extract the first occurrence of "key": "..." from JSON data.
// Handles basic escape sequences. Works in Qt4 and Qt5/6.
QString LLMClient::extractJsonString(const QByteArray& rawData, const QString& key)
{
    const QString data = QString::fromUtf8(rawData);
    // Try both: "key": "value" and "key":"value"
    const QString patterns[2] = {
        QLatin1Char('"') + key + QStringLiteral("\": \""),
        QLatin1Char('"') + key + QStringLiteral("\":\"")
    };

    int pos = -1;
    int prefixLen = 0;
    for (int p = 0; p < 2; ++p) {
        pos = data.indexOf(patterns[p]);
        if (pos != -1) { prefixLen = patterns[p].length(); break; }
    }
    if (pos == -1) return QString();
    pos += prefixLen;

    QString result;
    bool escaped = false;
    for (int i = pos; i < data.length(); ++i) {
        const QChar c = data.at(i);
        if (escaped) {
            if      (c == QLatin1Char('n'))  result += QLatin1Char('\n');
            else if (c == QLatin1Char('t'))  result += QLatin1Char('\t');
            else if (c == QLatin1Char('r'))  result += QLatin1Char('\r');
            else if (c == QLatin1Char('\\')) result += QLatin1Char('\\');
            else if (c == QLatin1Char('"'))  result += QLatin1Char('"');
            else if (c == QLatin1Char('/'))  result += QLatin1Char('/');
            else { result += QLatin1Char('\\'); result += c; }
            escaped = false;
        } else if (c == QLatin1Char('\\')) {
            escaped = true;
        } else if (c == QLatin1Char('"')) {
            break;
        } else {
            result += c;
        }
    }
    return result;
}

QString LLMClient::parseOpenAIResponse(const QByteArray& data) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return QString();
    return doc.object()
              .value(QStringLiteral("choices")).toArray()
              .at(0).toObject()
              .value(QStringLiteral("message")).toObject()
              .value(QStringLiteral("content")).toString();
#else
    return extractJsonString(data, QStringLiteral("content"));
#endif
}

QString LLMClient::parseAnthropicResponse(const QByteArray& data) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return QString();
    return doc.object()
              .value(QStringLiteral("content")).toArray()
              .at(0).toObject()
              .value(QStringLiteral("text")).toString();
#else
    return extractJsonString(data, QStringLiteral("text"));
#endif
}

QString LLMClient::parseOllamaResponse(const QByteArray& data) const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return QString();
    return doc.object()
              .value(QStringLiteral("message")).toObject()
              .value(QStringLiteral("content")).toString();
#else
    return extractJsonString(data, QStringLiteral("content"));
#endif
}
