#ifndef KITEMMODELS_DEBUG_H
#define KITEMMODELS_DEBUG_H

#include <QDebug>
#include <QLoggingCategory>

Q_GLOBAL_STATIC_WITH_ARGS(QLoggingCategory, LOG_CAT, ("kitemmodels.library"));
static QLoggingCategory& KITEMMODELS_LOG = *LOG_CAT;

#endif // KITEMMODELS_DEBUG_H
