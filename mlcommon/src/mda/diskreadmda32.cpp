#include "diskreadmda32.h"
#include <stdio.h>
#include "mdaio.h"
#include <math.h>
#include <QFile>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include "cachemanager.h"
#include "mlcommon.h"
#include <icounter.h>
#include <objectregistry.h>

#define MAX_PATH_LEN 10000
#define DEFAULT_CHUNK_SIZE 1e6

/// TODO (LOW) make tmp directory with different name on server, so we can really test if it is doing the computation in the right place

class DiskReadMda32Private {
public:
    DiskReadMda32* q;
    FILE* m_file;
    bool m_file_open_failed;
    bool m_header_read;
    MDAIO_HEADER m_header;
    bool m_reshaped;
    long m_mda_header_total_size;
    Mda32 m_internal_chunk;
    long m_current_internal_chunk_index;
    Mda32 m_memory_mda;
    bool m_use_memory_mda;
    bool m_use_concat=false;
    int m_concat_dimension=2;
    QList<DiskReadMda32> m_concat_list;

    QString m_path;
    QJsonObject m_prv_object;

    IIntCounter* allocatedCounter = nullptr;
    IIntCounter* freedCounter = nullptr;
    IIntCounter* bytesReadCounter = nullptr;
    IIntCounter* bytesWrittenCounter = nullptr;

    void construct_and_clear();
    bool read_header_if_needed();
    bool open_file_if_needed();
    void copy_from(const DiskReadMda32& other);
    long total_size();
};

DiskReadMda32::DiskReadMda32(const QString& path)
{
    d = new DiskReadMda32Private;
    d->q = this;
    ICounterManager* manager = ObjectRegistry::getObject<ICounterManager>();
    if (manager) {
        d->allocatedCounter = static_cast<IIntCounter*>(manager->counter("allocated_bytes"));
        d->freedCounter = static_cast<IIntCounter*>(manager->counter("freed_bytes"));
        d->bytesReadCounter = static_cast<IIntCounter*>(manager->counter("bytes_read"));
        d->bytesWrittenCounter = static_cast<IIntCounter*>(manager->counter("bytes_written"));
    }
    d->construct_and_clear();
    if (!path.isEmpty()) {
        this->setPath(path);
    }
}

DiskReadMda32::DiskReadMda32(const DiskReadMda32& other)
{
    d = new DiskReadMda32Private;
    d->q = this;
    d->construct_and_clear();
    d->copy_from(other);
}

DiskReadMda32::DiskReadMda32(const Mda32& X)
{
    d = new DiskReadMda32Private;
    d->q = this;
    d->construct_and_clear();
    d->m_use_memory_mda = true;
    d->m_memory_mda = X;
}

DiskReadMda32::DiskReadMda32(const QJsonObject& prv_object)
{
    d = new DiskReadMda32Private;
    d->q = this;
    d->construct_and_clear();

    //do not allow downloads or processing because now this is handled in a separate gui, as it should!!!
    //bool allow_downloads = false;
    //bool allow_processing = false;

    this->setPrvObject(prv_object); //important to do this anyway (even if resolve fails) so we can retrieve it later with toPrvObject()
    //QString path0 = resolve_prv_object(prv_object, allow_downloads, allow_processing);
    QString path0 = locate_prv(prv_object);
    if (path0.isEmpty()) {
        qWarning() << "Unable to construct DiskReadMda32 from prv_object. Unable to resolve. Original path = " << prv_object["original_path"].toString();
        return;
    }
    this->setPath(path0);
}

DiskReadMda32::DiskReadMda32(int concat_dimension, const QList<DiskReadMda32> &arrays)
{
    d = new DiskReadMda32Private;
    d->q = this;
    d->construct_and_clear();

    if (concat_dimension!=0) {
        qCritical() << "For now concat_dimension must be 2!";
        return;
    }

    d->m_use_concat=true;
    d->m_concat_dimension=concat_dimension;
    d->m_concat_list=arrays;
}

DiskReadMda32::~DiskReadMda32()
{
    if (d->m_file) {
        fclose(d->m_file);
    }
    delete d;
}

void DiskReadMda32::operator=(const DiskReadMda32& other)
{
    d->copy_from(other);
}

void DiskReadMda32::setPath(const QString& file_path)
{
    if (d->m_file) {
        fclose(d->m_file);
        d->m_file = 0;
    }
    d->construct_and_clear();

    if ((file_path.endsWith(".txt")) || (file_path.endsWith(".csv"))) {
        Mda32 X(file_path);
        (*this) = X;
        return;
    }
    else if (file_path.endsWith(".prv")) {
        //do not allow downloads or processing because now this is handled in a separate gui, as it should!!!
        //bool allow_downloads = false;
        //bool allow_processing = false;

        //important to do this first so that we can retrieve it with toPrvObject()
        QJsonObject obj = QJsonDocument::fromJson(TextFile::read(file_path).toUtf8()).object();
        this->setPrvObject(obj);

        //QString file_path_2 = resolve_prv_file(file_path, allow_downloads, allow_processing);
        QString file_path_2 = locate_prv(obj);
        if (!file_path_2.isEmpty()) {
            this->setPath(file_path_2);
            return;
        }
        else {
            qWarning() << "Unable to load DiskReadMda32 because .prv file could not be resolved.";
            return;
        }
    }
    else if (file_path.startsWith("\"")) {
        QStringList list=file_path.split(";");
        for (int i=0; i<list.count(); i++) {
            list[i]=list[i].mid(1,list[i].count()-2); //remove quotes
        }
        if (list.count()==1) {
            d->m_path=list[0];
        }
        else {
            setConcatPaths(2,list);
        }
    }
    else {
        d->m_path = file_path;
    }
}

void DiskReadMda32::setPrvObject(const QJsonObject& prv_object)
{
    d->m_prv_object = prv_object;
}

void DiskReadMda32::setConcatPaths(int concat_dimension, const QStringList &paths)
{
    if (concat_dimension!=2) {
        qWarning() << "For now, the concat_dimension must be 2";
        return;
    }
    d->m_use_concat=true;
    d->m_concat_dimension=concat_dimension;
    d->m_concat_list.clear();
    foreach (QString path0,paths) {
        d->m_concat_list << DiskReadMda32(path0);
    }
}

QString compute_memory_checksum32(long nbytes, void* ptr)
{
    QByteArray X((char*)ptr, nbytes);
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(X);
    return QString(hash.result().toHex());
}

QString compute_mda_checksum(Mda32& X)
{
    QString ret = compute_memory_checksum32(X.totalSize() * sizeof(dtype32), X.dataPtr());
    ret += "-";
    for (int i = 0; i < X.ndims(); i++) {
        if (i > 0)
            ret += "x";
        ret += QString("%1").arg(X.size(i));
    }
    return ret;
}

QString DiskReadMda32::makePath() const
{
    if (d->m_use_memory_mda) {
        QString checksum = compute_mda_checksum(d->m_memory_mda);
        QString fname = CacheManager::globalInstance()->makeLocalFile(checksum + ".makePath.mda", CacheManager::ShortTerm);
        if (QFile::exists(fname))
            return fname;
        if (d->m_memory_mda.write64(fname + ".tmp")) {
            if (QFile::rename(fname + ".tmp", fname)) {
                return fname;
            }
            else {
                QFile::remove(fname + ".tmp");
                return "";
            }
        }
        else {
            QFile::remove(fname);
            QFile::remove(fname + ".tmp");
            return "";
        }
    }
    else if (d->m_use_concat) {
        QStringList list;
        for (int i=0; i<d->m_concat_list.count(); i++) {
            list << "\""+d->m_concat_list[i].makePath()+"\"";
        }
        return list.join(";");
    }
    else {
        return d->m_path;
    }
}

QJsonObject DiskReadMda32::toPrvObject() const
{
    if (d->m_prv_object.isEmpty()) {
        QJsonObject ret;
        QString path0 = this->makePath();
        ret["original_size"] = QFileInfo(path0).size();
        ret["original_checksum"] = MLUtil::computeSha1SumOfFile(d->m_path);
        ret["original_fcs"] = "head1000-" + MLUtil::computeSha1SumOfFileHead(d->m_path, 1000);
        ret["original_path"] = path0;
        return ret;
    }
    else
        return d->m_prv_object;
}

long DiskReadMda32::N1() const
{
    if (d->m_use_memory_mda) {
        return d->m_memory_mda.N1();
    }
    if (!d->read_header_if_needed()) {
        return 0;
    }
    return d->m_header.dims[0];
}

long DiskReadMda32::N2() const
{
    if (d->m_use_memory_mda)
        return d->m_memory_mda.N2();
    if (!d->read_header_if_needed())
        return 0;
    return d->m_header.dims[1];
}

long DiskReadMda32::N3() const
{
    if (d->m_use_memory_mda)
        return d->m_memory_mda.N3();
    if (!d->read_header_if_needed())
        return 0;
    return d->m_header.dims[2];
}

long DiskReadMda32::N4() const
{
    if (d->m_use_memory_mda)
        return d->m_memory_mda.N4();
    if (!d->read_header_if_needed())
        return 0;
    return d->m_header.dims[3];
}

long DiskReadMda32::N5() const
{
    if (d->m_use_memory_mda)
        return d->m_memory_mda.N5();
    if (!d->read_header_if_needed())
        return 0;
    return d->m_header.dims[4];
}

long DiskReadMda32::N6() const
{
    if (d->m_use_memory_mda)
        return d->m_memory_mda.N6();
    if (!d->read_header_if_needed())
        return 0;
    return d->m_header.dims[5];
}

long DiskReadMda32::N(int dim) const
{
    d->read_header_if_needed();
    if (dim == 0)
        return 0; //should be 1-based
    if (dim == 1)
        return N1();
    else if (dim == 2)
        return N2();
    else if (dim == 3)
        return N3();
    else if (dim == 4)
        return N4();
    else if (dim == 5)
        return N5();
    else if (dim == 6)
        return N6();
    else
        return 1;
}

long DiskReadMda32::totalSize() const
{
    d->read_header_if_needed();
    return d->total_size();
}

MDAIO_HEADER DiskReadMda32::mdaioHeader() const
{
    d->read_header_if_needed();
    return d->m_header;
}

bool DiskReadMda32::reshape(long N1b, long N2b, long N3b, long N4b, long N5b, long N6b)
{
    long size_b = N1b * N2b * N3b * N4b * N5b * N6b;
    if (size_b != this->totalSize()) {
        qWarning() << "Cannot reshape because sizes do not match" << this->totalSize() << N1b << N2b << N3b << N4b << N5b << N6b;
        return false;
    }
    if (d->m_use_memory_mda) {
        if (d->m_memory_mda.reshape(N1b, N2b, N3b, N4b, N5b, N6b)) {
            d->m_reshaped = true;
            return true;
        }
        else
            return false;
    }
    if (!d->read_header_if_needed()) {
        return false;
    }
    d->m_header.dims[0] = N1b;
    d->m_header.dims[1] = N2b;
    d->m_header.dims[2] = N3b;
    d->m_header.dims[3] = N4b;
    d->m_header.dims[4] = N5b;
    d->m_header.dims[5] = N6b;
    d->m_reshaped = true;
    return true;
}

DiskReadMda32 DiskReadMda32::reshaped(long N1b, long N2b, long N3b, long N4b, long N5b, long N6b)
{
    long size_b = N1b * N2b * N3b * N4b * N5b * N6b;
    if (size_b != this->totalSize()) {
        qWarning() << "Cannot reshape because sizes do not match" << this->totalSize() << N1b << N2b << N3b << N4b << N5b << N6b;
        return (*this);
    }
    DiskReadMda32 ret = (*this);
    ret.reshape(N1b, N2b, N3b, N4b, N5b, N6b);
    return ret;
}

bool read_chunk_from_concat_list(Mda32 &chunk,const QList<DiskReadMda32> &list,long i0,long size0,int concat_dimension) {
    if (list.count()==0) {
        qWarning() << "Problem in read_chunk_from concat_list: list is empty";
        return false;
    }
    long N1=list[0].N1();
    if ((concat_dimension!=2)||(i0%N1!=0)||(size0%N1!=0)) {
        qWarning() << "For now the concat_dimension must be 2 and the i and size in readChunk must be a multiples of N1";
        return false;
    }
    long pos1=i0/N1;
    long pos2=pos1+size0/N1-1;

    QVector<long> sizes;
    for (int j=0; j<list.count(); j++) {
        if (list[j].totalSize()%N1!=0) {
            qWarning() << "For now the concat_dimension must be 2 and the individual arrays must have total size a multiple of N1" << N1 << list[j].totalSize();
            return false;
        }
        sizes << list[j].totalSize()/N1;
    }
    QVector<long> start_points, end_points;
    start_points << 0;
    end_points << sizes.value(0) - 1;
    for (int i = 1; i < sizes.count(); i++) {
        start_points << end_points[i - 1] + 1;
        end_points << start_points[i] + sizes[i] - 1;
    }

    long ii1 = 0;
    while ((ii1 < list.count()) && (pos1 > end_points[ii1])) {
        ii1++;
    }
    long ii2 = ii1;
    while ((ii2 + 1 < list.count()) && (start_points[ii2 + 1] <= pos2)) {
        ii2++;
    }

    chunk.allocate(1, N1*(pos2 - pos1 + 1));
    for (int ii = ii1; ii <= ii2; ii++) {
        Mda32 chunk_ii;

        //ttA,ttB, the range to read from Y
        //ssA, the position to write it in "out"
        long ttA, ttB, ssA;
        if (ii == ii1) {
            long offset = pos1 - start_points[ii];
            ssA = 0;
            if (ii1 < ii2) {
                ttA = offset;
                ttB = sizes[ii] - 1;
            }
            else {
                ttA = offset;
                ttB = ttA + (pos2 - pos1 + 1) - 1;
            }
        }
        else if (ii == ii2) {
            ttA = 0;
            ttB = pos2 - start_points[ii];
            ssA = start_points[ii] - pos1;
        }
        else {
            ttA = 0;
            ttB = sizes[ii] - 1;
            ssA = start_points[ii] - pos1;
        }

        list[ii].readChunk(chunk_ii, N1*ttA, N1*(ttB - ttA + 1));
        chunk.setChunk(chunk_ii, N1*ssA);
    }
    return true;
}

bool DiskReadMda32::readChunk(Mda32& X, long i, long size) const
{
    if (d->m_use_memory_mda) {
        d->m_memory_mda.getChunk(X, i, size);
        return true;
    }
    else if (d->m_use_concat) {
        return read_chunk_from_concat_list(X,d->m_concat_list,i,size,d->m_concat_dimension);
    }
    if (!d->open_file_if_needed())
        return false;
    X.allocate(size, 1);
    long jA = qMax(i, 0L);
    long jB = qMin(i + size - 1, d->total_size() - 1);
    long size_to_read = jB - jA + 1;
    if (size_to_read > 0) {
        fseek(d->m_file, d->m_header.header_size + d->m_header.num_bytes_per_entry * (jA), SEEK_SET);
        long bytes_read = mda_read_float32(&X.dataPtr()[jA - i], &d->m_header, size_to_read, d->m_file);
        if (d->bytesReadCounter)
            d->bytesReadCounter->add(bytes_read);
        if (bytes_read != size_to_read) {
            printf("Warning problem reading chunk in DiskReadMda32: %ld<>%ld\n", bytes_read, size_to_read);
            return false;
        }
    }
    return true;
}

bool DiskReadMda32::readChunk(Mda32& X, long i1, long i2, long size1, long size2) const
{
    if (size2 == 0) {
        return readChunk(X, i1, size1);
    }
    if (d->m_use_memory_mda) {
        d->m_memory_mda.getChunk(X, i1, i2, size1, size2);
        return true;
    }
    else if (d->m_use_concat) {
        if (!readChunk(X,i1+N1()*i2,size1*size2))
            return false;
        return X.reshape(size1,size2);
    }
    if (!d->open_file_if_needed())
        return false;
    if ((size1 == N1()) && (i1 == 0)) {
        //easy case
        X.allocate(size1, size2);
        long jA = qMax(i2, 0L);
        long jB = qMin(i2 + size2 - 1, N2() - 1);
        long size2_to_read = jB - jA + 1;
        if (size2_to_read > 0) {
            fseek(d->m_file, d->m_header.header_size + d->m_header.num_bytes_per_entry * (i1 + N1() * jA), SEEK_SET);
            long bytes_read = mda_read_float32(&X.dataPtr()[(jA - i2) * size1], &d->m_header, size1 * size2_to_read, d->m_file);
            if (d->bytesReadCounter)
                d->bytesReadCounter->add(bytes_read);
            if (bytes_read != size1 * size2_to_read) {
                printf("Warning problem reading 2d chunk in DiskReadMda32: %ld<>%ld\n", bytes_read, size1 * size2);
                return false;
            }
        }
        return true;
    }
    else {
        printf("Warning: This case not yet supported (DiskReadMda32::readchunk 2d).\n");
        return false;
    }
}

bool DiskReadMda32::readChunk(Mda32& X, long i1, long i2, long i3, long size1, long size2, long size3) const
{
    if (size3 == 0) {
        if (size2 == 0) {
            return readChunk(X, i1, size1);
        }
        else {
            return readChunk(X, i1, i2, size1, size2);
        }
    }
    if (d->m_use_memory_mda) {
        d->m_memory_mda.getChunk(X, i1, i2, i3, size1, size2, size3);
        return true;
    }
    else if (d->m_use_concat) {
        if (!readChunk(X,i1+N1()*i2+N1()*N2()*i3,size1*size2*size3))
            return false;
        return X.reshape(size1,size2,size3);
    }
    if (!d->open_file_if_needed())
        return false;
    if ((size1 == N1()) && (size2 == N2())) {
        //easy case
        X.allocate(size1, size2, size3);
        long jA = qMax(i3, 0L);
        long jB = qMin(i3 + size3 - 1, N3() - 1);
        long size3_to_read = jB - jA + 1;
        if (size3_to_read > 0) {
            fseek(d->m_file, d->m_header.header_size + d->m_header.num_bytes_per_entry * (i1 + N1() * i2 + N1() * N2() * jA), SEEK_SET);
            long bytes_read = mda_read_float32(&X.dataPtr()[(jA - i3) * size1 * size2], &d->m_header, size1 * size2 * size3_to_read, d->m_file);
            if (d->bytesReadCounter)
                d->bytesReadCounter->add(bytes_read);
            if (bytes_read != size1 * size2 * size3_to_read) {
                printf("Warning problem reading 3d chunk in DiskReadMda32: %ld<>%ld\n", bytes_read, size1 * size2 * size3_to_read);
                return false;
            }
        }
        return true;
    }
    else {
        printf("Warning: This case not yet supported (DiskReadMda32::readchunk 3d).\n");
        return false;
    }
}

dtype32 DiskReadMda32::value(long i) const
{
    if (d->m_use_memory_mda)
        return d->m_memory_mda.value(i);
    if ((i < 0) || (i >= d->total_size()))
        return 0;
    long chunk_index = i / DEFAULT_CHUNK_SIZE;
    long offset = i - DEFAULT_CHUNK_SIZE * chunk_index;
    if (d->m_current_internal_chunk_index != chunk_index) {
        long size_to_read = DEFAULT_CHUNK_SIZE;
        if (chunk_index * DEFAULT_CHUNK_SIZE + size_to_read > d->total_size())
            size_to_read = d->total_size() - chunk_index * DEFAULT_CHUNK_SIZE;
        if (size_to_read) {
            this->readChunk(d->m_internal_chunk, chunk_index * DEFAULT_CHUNK_SIZE, size_to_read);
        }
        d->m_current_internal_chunk_index = chunk_index;
    }
    return d->m_internal_chunk.value(offset);
}

dtype32 DiskReadMda32::value(long i1, long i2) const
{
    if (d->m_use_memory_mda)
        return d->m_memory_mda.value(i1, i2);
    if ((i1 < 0) || (i1 >= N1()))
        return 0;
    if ((i2 < 0) || (i2 >= N2()))
        return 0;
    return value(i1 + N1() * i2);
}

dtype32 DiskReadMda32::value(long i1, long i2, long i3) const
{
    if (d->m_use_memory_mda)
        return d->m_memory_mda.value(i1, i2, i3);
    if ((i1 < 0) || (i1 >= N1()))
        return 0;
    if ((i2 < 0) || (i2 >= N2()))
        return 0;
    if ((i3 < 0) || (i3 >= N3()))
        return 0;
    return value(i1 + N1() * i2 + N1() * N2() * i3);
}

void DiskReadMda32Private::construct_and_clear()
{
    m_file_open_failed = false;
    m_file = 0;
    m_current_internal_chunk_index = -1;
    m_use_memory_mda = false;
    m_use_concat = false;
    m_header_read = false;
    m_reshaped = false;
    this->m_internal_chunk = Mda32();
    this->m_mda_header_total_size = 0;
    this->m_memory_mda = Mda32();
    this->m_path = "";
}

bool DiskReadMda32Private::read_header_if_needed()
{
    if (m_header_read)
        return true;
    if (m_use_memory_mda) {
        m_header_read = true;
        return true;
    }
    else if (m_use_concat) {
        m_header_read=true;
        if (m_concat_list.count()==0) {
            qWarning() << "Cannot read header of concat array because list is empty";
            return false;
        }
        m_header=m_concat_list[0].mdaioHeader();
        long N2=0;
        for (int i=0; i<m_concat_list.count(); i++) {
            if (m_concat_list[i].N1()!=m_concat_list[0].N1()) {
                qWarning() << "dimension mismatch in concat list";
                return false;
            }
            if (m_concat_list[i].N3()!=m_concat_list[0].N3()) {
                qWarning() << "dimension mismatch in concat list";
                return false;
            }
            if (m_concat_list[i].N4()!=m_concat_list[0].N4()) {
                qWarning() << "dimension mismatch in concat list";
                return false;
            }
            N2+=m_concat_list[i].N2();
        }
        m_header.dims[1]=N2;
        m_mda_header_total_size = 1;
        for (int i = 0; i < MDAIO_MAX_DIMS; i++)
            m_mda_header_total_size *= m_header.dims[i];
        return true;
    }
    bool file_was_open = (m_file != 0); //so we can restore to previous state (we don't want too many files open unnecessarily)
    if (!open_file_if_needed()) //if successful, it will read the header
        return false;
    if (!m_file)
        return false; //should never happen
    if (!file_was_open) {
        fclose(m_file);
        m_file = 0;
    }
    return true;
}

bool DiskReadMda32Private::open_file_if_needed()
{
    if (m_use_memory_mda)
        return true;
    if (m_use_concat) {
        read_header_if_needed();
        return true;
    }
    if (m_file)
        return true;
    if (m_file_open_failed)
        return false;
    if (m_path.isEmpty())
        return false;
    m_file = fopen(m_path.toLatin1().data(), "rb");
    if (m_file) {
        if (!m_header_read) {
            //important not to read it again in case we have reshaped the array
            mda_read_header(&m_header, m_file);
            m_mda_header_total_size = 1;
            for (int i = 0; i < MDAIO_MAX_DIMS; i++)
                m_mda_header_total_size *= m_header.dims[i];
            m_header_read = true;
        }
    }
    else {
        qWarning() << ":::: Failed to open DiskReadMda32 file: " + m_path;
        if (QFile::exists(m_path)) {
            printf("Even though this file does exist.\n");
        }
        else {
            qWarning() << "File does not exist: " + m_path;
        }
        m_file_open_failed = true; //we don't want to try this more than once
        return false;
    }
    return true;
}

void DiskReadMda32Private::copy_from(const DiskReadMda32& other)
{
    /// TODO (LOW) think about copying over additional information such as internal chunks

    if (this->m_file) {
        fclose(this->m_file);
        this->m_file = 0;
    }
    this->allocatedCounter = other.d->allocatedCounter;
    this->freedCounter = other.d->freedCounter;
    this->bytesReadCounter = other.d->bytesReadCounter;
    this->bytesWrittenCounter = other.d->bytesWrittenCounter;
    this->construct_and_clear();
    this->m_current_internal_chunk_index = -1;
    this->m_file_open_failed = other.d->m_file_open_failed;
    this->m_header = other.d->m_header;
    this->m_header_read = other.d->m_header_read;
    this->m_mda_header_total_size = other.d->m_mda_header_total_size;
    this->m_memory_mda = other.d->m_memory_mda;
    this->m_path = other.d->m_path;
    this->m_prv_object = other.d->m_prv_object;
    this->m_reshaped = other.d->m_reshaped;
    this->m_use_memory_mda = other.d->m_use_memory_mda;
    this->m_use_concat = other.d->m_use_concat;
    this->m_concat_dimension = other.d->m_concat_dimension;
    this->m_concat_list = other.d->m_concat_list;
}

long DiskReadMda32Private::total_size()
{
    if (m_use_memory_mda)
        return m_memory_mda.totalSize();
    if (!read_header_if_needed())
        return 0;
    return m_mda_header_total_size;
}
