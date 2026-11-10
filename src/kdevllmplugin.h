/***************************************************************************
 *   Copyright 2024-2025 Gustavo Bacagine <gustavo.bacagine@protonmail.com> *
 *   GPL v2 or later                                                       *
 ***************************************************************************/

#ifndef KDEVLLM_KDEVLLMPLUGIN_H
#define KDEVLLM_KDEVLLMPLUGIN_H

#include <interfaces/iplugin.h>
#include <interfaces/contextmenuextension.h>
#include "llmcredentialstore.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#  include <KPluginFactory>
#  include <KAboutData>
#else
#  include <KPluginFactory>
#  if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#    include <KPluginMetaData>
#  endif
#endif

class LLMClient;
class LLMChatWidget;
class KDevLLMViewFactory;

class KDevLLMPlugin : public KDevelop::IPlugin
{
    Q_OBJECT
public:
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    KDevLLMPlugin(QObject* parent, const QStringList& args);
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    KDevLLMPlugin(QObject* parent, const QVariantList& args = QVariantList());
#else
    KDevLLMPlugin(QObject* parent, const KPluginMetaData& metaData,
                  const QVariantList& args = QVariantList());
#endif
    ~KDevLLMPlugin() override;

    void unload() override;

    KDevelop::ContextMenuExtension contextMenuExtension(
        KDevelop::Context* context, QWidget* parent) override;

    LLMClient* client() const;
    LLMCredentialStore* credentialStore() const;

private Q_SLOTS:
    void ctxExplain();
    void ctxRefactor();
    void ctxAddComments();
    void ctxAskCustom();
    void onSettingsAction();

private:
    void setupActions();
    void loadConfig();

    LLMClient*           m_client;
    LLMCredentialStore*  m_credStore;
    KDevLLMViewFactory*  m_viewFactory;
    QList<QUrl>          m_contextUrls;
};

#endif // KDEVLLM_KDEVLLMPLUGIN_H
