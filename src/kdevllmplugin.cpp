/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *   GPL v2 or later                                                       *
 ***************************************************************************/

#include "kdevllmplugin.h"
#include "llmclient.h"
#include "llmchatwidget.h"
#include "llmsettingsdialog.h"
#include "llmcredentialstore.h"

#include <QAction>
#include <QMenu>
#include <QUrl>
#include <KActionCollection>

#include <KLocalizedString>
#include <KPluginFactory>

#include <interfaces/icore.h>
#include <interfaces/iuicontroller.h>
#include <interfaces/context.h>
#include <interfaces/contextmenuextension.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/idocument.h>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <ktexteditor/document.h>
#  include <ktexteditor/view.h>
#else
#  include <KTextEditor/Document>
#  include <KTextEditor/View>
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <KGlobal>
#  include <KConfigGroup>
static KSharedConfigPtr kdevllmConfig() { return KGlobal::config(); }
#else
#  include <KSharedConfig>
#  include <KConfigGroup>
static KSharedConfigPtr kdevllmConfig() { return KSharedConfig::openConfig(); }
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include "debug_kde4.h"
#define PLUGIN_KDEVLLM ""
// KDE4: use factory + exported plugin descriptor
K_PLUGIN_FACTORY(KDevLLMFactory, registerPlugin<KDevLLMPlugin>();)
K_EXPORT_PLUGIN(KDevLLMFactory(KAboutData(
    "kdevllm", "kdevllm",
    ki18n("LLM Integration"),
    "0.1",
    ki18n("AI-powered code assistant (OpenAI, Anthropic, Ollama)"),
    KAboutData::License_GPL
)))
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "debug.h"
K_PLUGIN_FACTORY_WITH_JSON(KDevLLMFactory, "kdevllm.json",
    registerPlugin<KDevLLMPlugin>();)
#else
#include "debug.h"
K_PLUGIN_FACTORY_WITH_JSON(KDevLLMFactory, "kdevllm.json",
    registerPlugin<KDevLLMPlugin>();)
#endif

// ---------------------------------------------------------------------------
// Tool-view factory
// ---------------------------------------------------------------------------

class KDevLLMViewFactory : public KDevelop::IToolViewFactory
{
public:
    explicit KDevLLMViewFactory(KDevLLMPlugin* plugin)
        : m_plugin(plugin) {}

    QWidget* create(QWidget* parent = nullptr) override {
        LLMChatWidget* w = new LLMChatWidget(m_plugin, parent);
        w->setClient(m_plugin->client());
        return w;
    }

    Qt::DockWidgetArea defaultPosition() const override {
        return Qt::RightDockWidgetArea;
    }

    QString id() const override {
        return QStringLiteral("org.kdevelop.LLMView");
    }

private:
    KDevLLMPlugin* m_plugin;
};

// ---------------------------------------------------------------------------
// Plugin constructor / destructor
// ---------------------------------------------------------------------------

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
KDevLLMPlugin::KDevLLMPlugin(QObject* parent, const QStringList& args)
    : KDevelop::IPlugin(KDevLLMFactory::componentData(), parent)
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
KDevLLMPlugin::KDevLLMPlugin(QObject* parent, const QVariantList& args)
    : KDevelop::IPlugin(QStringLiteral("kdevllm"), parent)
#else
KDevLLMPlugin::KDevLLMPlugin(QObject* parent, const KPluginMetaData& metaData,
                              const QVariantList& args)
    : KDevelop::IPlugin(QStringLiteral("kdevllm"), parent, metaData)
#endif
    , m_client(new LLMClient(this))
    , m_credStore(new LLMCredentialStore(this))
    , m_viewFactory(new KDevLLMViewFactory(this))
{
    Q_UNUSED(args);

    loadConfig();
    core()->uiController()->addToolView(i18n("AI Assistant"), m_viewFactory);
    setupActions();

    qCDebug(PLUGIN_KDEVLLM) << "KDevLLM plugin loaded";
}

KDevLLMPlugin::~KDevLLMPlugin()
{
}

void KDevLLMPlugin::unload()
{
    core()->uiController()->removeToolView(m_viewFactory);
}

LLMClient* KDevLLMPlugin::client() const
{
    return m_client;
}

LLMCredentialStore* KDevLLMPlugin::credentialStore() const
{
    return m_credStore;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void KDevLLMPlugin::loadConfig()
{
    KConfigGroup grp(kdevllmConfig(), QStringLiteral("kdevllm"));
    LLMClient::Config cfg;
    cfg.provider  = static_cast<LLMClient::Provider>(grp.readEntry("provider", 0));
    cfg.maxTokens = 4096;

    // Model and endpoint come from KConfig (non-sensitive)
    switch (cfg.provider) {
    case LLMClient::OpenAI:
        cfg.model = grp.readEntry("modelOpenAI", QStringLiteral("gpt-4o"));
        // API key loaded from wallet when needed; also try KConfig fallback
        cfg.apiKey = m_credStore->loadKey(QStringLiteral("OpenAI"));
        break;
    case LLMClient::Anthropic:
        cfg.model  = grp.readEntry("modelAnthropic",
                        QStringLiteral("claude-3-5-sonnet-20241022"));
        cfg.apiKey = m_credStore->loadKey(QStringLiteral("Anthropic"));
        break;
    case LLMClient::Ollama:
        cfg.endpoint = grp.readEntry("endpointOllama",
                           QStringLiteral("http://localhost:11434"));
        cfg.model    = grp.readEntry("modelOllama", QStringLiteral("codellama"));
        break;
    }
    m_client->setConfig(cfg);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void KDevLLMPlugin::setupActions()
{
    // Global menu actions (non-context)
    QAction* actSettings = actionCollection()->addAction(
        QStringLiteral("llm_settings"));
    actSettings->setText(i18n("AI Assistant Settings..."));
    connect(actSettings, SIGNAL(triggered()), this, SLOT(onSettingsAction()));
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

KDevelop::ContextMenuExtension KDevLLMPlugin::contextMenuExtension(
    KDevelop::Context* context, QWidget* parent)
{
    KDevelop::ContextMenuExtension ext;

    // Only show on editor context
    if (context->type() != KDevelop::Context::EditorContext)
        return ext;

    QMenu* menu = new QMenu(i18n("AI Assistant"), parent);

    QAction* explainAct = new QAction(i18n("Explain Code"), menu);
    connect(explainAct, SIGNAL(triggered()), this, SLOT(ctxExplain()));
    menu->addAction(explainAct);

    QAction* refactorAct = new QAction(i18n("Refactor with AI"), menu);
    connect(refactorAct, SIGNAL(triggered()), this, SLOT(ctxRefactor()));
    menu->addAction(refactorAct);

    QAction* commentsAct = new QAction(i18n("Add Comments"), menu);
    connect(commentsAct, SIGNAL(triggered()), this, SLOT(ctxAddComments()));
    menu->addAction(commentsAct);

    menu->addSeparator();

    QAction* customAct = new QAction(i18n("Ask AI..."), menu);
    connect(customAct, SIGNAL(triggered()), this, SLOT(ctxAskCustom()));
    menu->addAction(customAct);

    ext.addAction(KDevelop::ContextMenuExtension::ExtensionGroup,
                  menu->menuAction());
    return ext;
}

// ---------------------------------------------------------------------------
// Context menu handlers — send code to chat widget
// ---------------------------------------------------------------------------

static QString activeSelection()
{
    KDevelop::IDocument* doc =
        KDevelop::ICore::self()->documentController()->activeDocument();
    if (!doc) return QString();
    KTextEditor::View* view = doc->activeTextView();
    if (view && view->selection())
        return view->selectionText();
    KTextEditor::Document* tdoc = doc->textDocument();
    return tdoc ? tdoc->text() : QString();
}

static void sendPromptToChat(KDevLLMPlugin* plugin, const QString& userMsg)
{
    if (!plugin->client() || userMsg.isEmpty()) return;
    QList<QPair<QString,QString>> history;
    history.append(qMakePair(QStringLiteral("user"), userMsg));
    const QString sys =
        QStringLiteral("You are an expert software engineer. Answer concisely.");
    plugin->client()->sendMessage(sys, history);
    // The chat widget's signal connections will update the UI
}

void KDevLLMPlugin::ctxExplain()
{
    const QString code = activeSelection();
    if (code.isEmpty()) return;
    sendPromptToChat(this,
        i18n("Explain the following code:\n\n```\n%1\n```").arg(code));
}

void KDevLLMPlugin::ctxRefactor()
{
    const QString code = activeSelection();
    if (code.isEmpty()) return;
    sendPromptToChat(this,
        i18n("Refactor the following code, improving clarity and efficiency. "
             "Output only the refactored code in a code block:\n\n```\n%1\n```").arg(code));
}

void KDevLLMPlugin::ctxAddComments()
{
    const QString code = activeSelection();
    if (code.isEmpty()) return;
    sendPromptToChat(this,
        i18n("Add clear and concise documentation comments to the following code. "
             "Output only the commented code in a code block:\n\n```\n%1\n```").arg(code));
}

void KDevLLMPlugin::ctxAskCustom()
{
    // The chat widget is in the tool view — user types their question there
}

void KDevLLMPlugin::onSettingsAction()
{
    LLMSettingsDialog dlg(m_credStore, nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        m_client->setConfig(dlg.currentConfig());
    }
}

#include "kdevllmplugin.moc"
