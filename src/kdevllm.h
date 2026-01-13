/**
 * @file kdevllm.h
 *
 * @brief Integrates LLM with KDevelop
 *
 * @author Gustavo Bacagine <gustavo.bacagine@protonmail.com>
 *
 * @date Jan 2026
 */

#ifndef _KDEVLLM_H_
#define _KDEVLLM_H_

#include <interfaces/iplugin.h>

class kdevllm : public KDevelop::IPlugin {
  Q_OBJECT

public:
  // KPluginFactory-based plugin wants constructor with this signature
  kdevllm(QObject* parent, const KPluginMetaData& metaData, const QVariantList& args);
};

#endif // KDEVLLM_H
