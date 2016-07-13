/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
*******************************************************/

#ifndef GET_SORT_INDICES_H
#define GET_SORT_INDICES_H

#include <QList>

QList<long> get_sort_indices(const QList<long>& X);
QList<long> get_sort_indices(const QVector<double>& X);

#endif // GET_SORT_INDICES_H
