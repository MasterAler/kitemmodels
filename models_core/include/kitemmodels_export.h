#ifndef KITEMMODELS_EXPORT_H
#define KITEMMODELS_EXPORT_H

#include <QtCore/qglobal.h>

#if defined(KITEMMODELS_LIBRARY)
#  define KITEMMODELS_EXPORT Q_DECL_EXPORT
#else
#  define KITEMMODELS_EXPORT Q_DECL_IMPORT
#endif

#endif // KITEMMODELS_EXPORT_H