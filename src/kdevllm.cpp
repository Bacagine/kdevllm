/**
 * @file kdevllm.cpp
 *
 * @brief Integrates LLM with KDevelop
 *
 * @author Gustavo Bacagine <gustavo.bacagine@protonmail.com>
 *
 * @date Jan 2026
 */

#include "kdevllm.h"
#include <debug.h>
#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(kdevllmFactory, "kdevllm.json", registerPlugin<kdevllm>(); )

kdevllm::kdevllm(QObject* parent, const KPluginMetaData& metaData, const QVariantList& args)
  : KDevelop::IPlugin(QStringLiteral("kdevllm"), parent, metaData) {
  Q_UNUSED(args);
  qCDebug(PLUGIN_KDEVLLM) << "Hello world, my plugin is loaded!";
}

// needed for QObject class created from K_PLUGIN_FACTORY_WITH_JSON
#include "kdevllm.moc"
#include "moc_kdevllm.cpp"
