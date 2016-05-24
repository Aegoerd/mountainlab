/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 5/23/2016
*******************************************************/

#ifndef CLUSTERMERGE
#define CLUSTERMERGE

#include <QSet>

class ClusterMergePrivate;
class ClusterMerge {
public:
    friend class ClusterMergePrivate;
    ClusterMerge();
    ClusterMerge(const ClusterMerge& other);
    virtual ~ClusterMerge();
    void operator=(const ClusterMerge& other);
    void merge(int label1, int label2);
    void merge(const QSet<int>& labels);
    void merge(const QList<int>& labels);
    void unmerge(int label);
    void unmerge(const QSet<int>& labels);
    void unmerge(const QList<int>& labels);

    int representativeLabel(int label) const;
    QList<int> representativeLabels() const;
    QList<int> getMergeGroup(int label) const;
    QString toJson() const;

    static ClusterMerge fromJson(const QString& json);

private:
    ClusterMergePrivate* d;
};

#endif // CLUSTERMERGE
