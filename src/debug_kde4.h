#ifndef KDEVLLM_DEBUG_KDE4_H
#define KDEVLLM_DEBUG_KDE4_H

// Qt4/KDE4 fallback — uses kDebug() instead of qCDebug()
#include <KDebug>
#define qCDebug(cat)  kDebug()
#define qCWarning(cat) kWarning()

#endif
