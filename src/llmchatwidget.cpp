/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *   GPL v2 or later                                                       *
 ***************************************************************************/

#include "llmchatwidget.h"
#include "llmsettingsdialog.h"
#include "kdevllmplugin.h"

#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QPushButton>
#include <QLabel>
#include <QKeyEvent>
#include <QScrollBar>

#include <KLocalizedString>

#include <interfaces/icore.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/idocument.h>
#include <interfaces/iplugin.h>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <ktexteditor/document.h>
#  include <ktexteditor/view.h>
#  include <ktexteditor/selectioninterface.h>
#else
#  include <KTextEditor/Document>
#  include <KTextEditor/View>
#endif


LLMChatWidget::LLMChatWidget(KDevelop::IPlugin* plugin, QWidget* parent)
    : QWidget(parent)
    , m_plugin(plugin)
    , m_client(nullptr)
{
    setupUi(this);

    connect(btnSend, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(btnSendFile, SIGNAL(clicked()), this, SLOT(onSendFileClicked()));
    connect(btnSendSelection, SIGNAL(clicked()), this, SLOT(onSendSelectionClicked()));
    connect(btnApply, SIGNAL(clicked()), this, SLOT(onApplyToEditorClicked()));
    connect(btnClear, SIGNAL(clicked()), this, SLOT(onClearClicked()));
    connect(btnSettings, SIGNAL(clicked()), this, SLOT(onSettingsClicked()));

}

LLMChatWidget::~LLMChatWidget()
{
}

void LLMChatWidget::setClient(LLMClient* client)
{
    if (m_client) {
        disconnect(m_client, nullptr, this, nullptr);
    }
    m_client = client;
    if (m_client) {
        connect(m_client, SIGNAL(responseReady(QString)),
                this, SLOT(onResponseReady(QString)));
        connect(m_client, SIGNAL(errorOccurred(QString)),
                this, SLOT(onErrorOccurred(QString)));
        connect(m_client, SIGNAL(requestStarted()),
                this, SLOT(onRequestStarted()));
    }
    updateStatusLabel();
}

void LLMChatWidget::updateStatusLabel()
{
    if (!m_client) {
        labelStatus->setText(i18n("No provider configured. Click Settings."));
        return;
    }
    const LLMClient::Config& cfg = m_client->config();
    QString provName;
    switch (cfg.provider) {
    case LLMClient::OpenAI:    provName = QStringLiteral("OpenAI");    break;
    case LLMClient::Anthropic: provName = QStringLiteral("Anthropic"); break;
    case LLMClient::Ollama:    provName = QStringLiteral("Ollama");    break;
    }
    labelStatus->setText(
        i18n("%1 / %2").arg(provName).arg(cfg.model));
}

void LLMChatWidget::onConfigChanged()
{
    updateStatusLabel();
}

// ---------------------------------------------------------------------------
// Sending messages
// ---------------------------------------------------------------------------

static const QString SYSTEM_PROMPT =
    QStringLiteral(
        "You are an expert software engineering assistant integrated into the "
        "KDevelop IDE. When asked to refactor or rewrite code, output ONLY the "
        "complete new code inside a single markdown code block (```). "
        "Do not include explanations outside the code block unless asked."
    );

void LLMChatWidget::onSendClicked()
{
    const QString text = inputEdit->toPlainText().trimmed();
    if (text.isEmpty() || !m_client || m_client->isBusy()) return;

    inputEdit->clear();
    appendUserMessage(text);
    m_history.append(qMakePair(QStringLiteral("user"), text));
    m_client->sendMessage(SYSTEM_PROMPT, m_history);
}

void LLMChatWidget::onSendFileClicked()
{
    const QString content = getCurrentFileContent();
    if (content.isEmpty()) {
        appendSystemMessage(i18n("No active file open in the editor."));
        return;
    }
    const QString msg = i18n("Here is the current file content:\n\n```\n%1\n```\n\nWhat would you like to do with it?").arg(content);
    inputEdit->setPlainText(msg);
    inputEdit->setFocus();
}

void LLMChatWidget::onSendSelectionClicked()
{
    const QString sel = getSelectedText();
    if (sel.isEmpty()) {
        appendSystemMessage(i18n("No text selected in the editor."));
        return;
    }
    const QString msg = i18n("Here is the selected code:\n\n```\n%1\n```\n\nPlease refactor this code.").arg(sel);
    inputEdit->setPlainText(msg);
    inputEdit->setFocus();
}

void LLMChatWidget::onApplyToEditorClicked()
{
    if (m_lastResponse.isEmpty()) return;
    const QString code = extractCodeBlock(m_lastResponse);
    applyToEditor(code.isEmpty() ? m_lastResponse : code);
}

void LLMChatWidget::onClearClicked()
{
    m_history.clear();
    m_lastResponse.clear();
    chatHistory->clear();
    btnApply->setEnabled(false);
}

void LLMChatWidget::onSettingsClicked()
{
    // The plugin owns the credential store; get it via the plugin
    KDevLLMPlugin* plugin = qobject_cast<KDevLLMPlugin*>(m_plugin);
    if (!plugin) return;

    LLMSettingsDialog dlg(plugin->credentialStore(), this);
    if (dlg.exec() == QDialog::Accepted && m_client) {
        m_client->setConfig(dlg.currentConfig());
        updateStatusLabel();
    }
}

void LLMChatWidget::onRequestStarted()
{
    btnSend->setEnabled(false);
    appendSystemMessage(i18n("Thinking…"));
}

void LLMChatWidget::onResponseReady(const QString& text)
{
    btnSend->setEnabled(true);
    m_lastResponse = text;
    m_history.append(qMakePair(QStringLiteral("assistant"), text));
    appendAssistantMessage(text);
    btnApply->setEnabled(true);
}

void LLMChatWidget::onErrorOccurred(const QString& error)
{
    btnSend->setEnabled(true);
    appendSystemMessage(i18n("<font color='red'>Error: %1</font>").arg(error));
}

// ---------------------------------------------------------------------------
// UI helpers
// ---------------------------------------------------------------------------

void LLMChatWidget::appendHtml(const QString& html)
{
    chatHistory->append(html);
    // Scroll to bottom
    QScrollBar* sb = chatHistory->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

void LLMChatWidget::appendUserMessage(const QString& text)
{
    appendHtml(
        QStringLiteral("<p><b style='color:#2255aa'>You:</b> ")
        + text.toHtmlEscaped()
              .replace(QStringLiteral("\n"), QStringLiteral("<br/>"))
        + QStringLiteral("</p>")
    );
}

void LLMChatWidget::appendAssistantMessage(const QString& text)
{
    // Simple markdown: convert ```...``` code blocks to <pre><code>
    QString html = text.toHtmlEscaped();

    // Replace code blocks first (triple backtick)
    int pos = 0;
    while ((pos = html.indexOf(QStringLiteral("```"), pos)) != -1) {
        // Find language tag if any (up to newline)
        int nlPos = html.indexOf(QLatin1Char('\n'), pos + 3);
        if (nlPos == -1) break;
        int closePos = html.indexOf(QStringLiteral("```"), nlPos);
        if (closePos == -1) break;
        const QString codeContent = html.mid(nlPos + 1, closePos - nlPos - 1);
        const QString replacement =
            QStringLiteral("<pre style='background:#f4f4f4;padding:4px;'><code>")
            + codeContent
            + QStringLiteral("</code></pre>");
        html.replace(pos, closePos - pos + 3, replacement);
        pos += replacement.length();
    }

    // Convert remaining newlines
    html.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));

    appendHtml(
        QStringLiteral("<p><b style='color:#228822'>Assistant:</b><br/>")
        + html + QStringLiteral("</p>")
    );
}

void LLMChatWidget::appendSystemMessage(const QString& html)
{
    appendHtml(
        QStringLiteral("<p><i style='color:#888888'>") + html
        + QStringLiteral("</i></p>")
    );
}

// ---------------------------------------------------------------------------
// Editor access
// ---------------------------------------------------------------------------

QString LLMChatWidget::getCurrentFileContent() const
{
    KDevelop::IDocument* doc =
        KDevelop::ICore::self()->documentController()->activeDocument();
    if (!doc) return QString();
    KTextEditor::Document* tdoc = doc->textDocument();
    if (!tdoc) return QString();
    return tdoc->text();
}

QString LLMChatWidget::getSelectedText() const
{
    KDevelop::IDocument* doc =
        KDevelop::ICore::self()->documentController()->activeDocument();
    if (!doc) return QString();
    KTextEditor::View* view = doc->activeTextView();
    if (!view || !view->selection()) return QString();
    return view->selectionText();
}

void LLMChatWidget::applyToEditor(const QString& text)
{
    KDevelop::IDocument* doc =
        KDevelop::ICore::self()->documentController()->activeDocument();
    if (!doc) {
        appendSystemMessage(i18n("No active document to apply changes to."));
        return;
    }

    KTextEditor::View* view = doc->activeTextView();
    KTextEditor::Document* tdoc = doc->textDocument();

    if (!tdoc) {
        appendSystemMessage(i18n("Active document has no editable text."));
        return;
    }

    if (view && view->selection()) {
        KTextEditor::Range range = view->selectionRange();
        tdoc->replaceText(range, text);
        view->removeSelection();
        appendSystemMessage(i18n("Applied to selection."));
    } else {
        tdoc->setText(text);
        appendSystemMessage(i18n("Applied to entire file."));
    }
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

QString LLMChatWidget::extractCodeBlock(const QString& response)
{
    const int start = response.indexOf(QStringLiteral("```"));
    if (start == -1) return QString();

    // Skip optional language identifier on the same line
    int contentStart = response.indexOf(QLatin1Char('\n'), start + 3);
    if (contentStart == -1) return QString();
    contentStart++; // skip the newline itself

    const int end = response.indexOf(QStringLiteral("```"), contentStart);
    if (end == -1) return response.mid(contentStart);

    return response.mid(contentStart, end - contentStart);
}
