/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 4/2/2016
*******************************************************/

#ifndef REMOTEREADMDA_H
#define REMOTEREADMDA_H

#include <QString>
#include "mda.h"

class RemoteReadMdaPrivate;
/**
 * @brief Do not use this class directly -- it is used by DiskReadMda
 */
class RemoteReadMda {
public:
    friend class RemoteReadMdaPrivate;
    RemoteReadMda(const QString& path = "");
    RemoteReadMda(const RemoteReadMda& other);
    void operator=(const RemoteReadMda& other);
    virtual ~RemoteReadMda();

    void setPath(const QString& path);
    QString path() const;

    int N1();
    int N2();
    int N3();
    QDateTime fileLastModified();

    ///Retrieve a chunk of the vectorized data of size 1xN starting at position i
    bool readChunk(Mda& X, int i, int size) const;

private:
    RemoteReadMdaPrivate* d;
};

void unit_test_remote_read_mda_2(const QString& path);
void unit_test_remote_read_mda();

#endif // REMOTEREADMDA_H
