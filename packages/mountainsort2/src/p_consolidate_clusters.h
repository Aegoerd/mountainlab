#ifndef P_CONSOLIDATE_CLUSTERS_H
#define P_CONSOLIDATE_CLUSTERS_H

#include <QString>


struct Consolidate_clusters_opts {
    int central_channel=1;
    double consolidation_factor=0.9;
};

bool p_consolidate_clusters(QString clips,QString labels,QString labels_out,Consolidate_clusters_opts opts);

#endif // P_CONSOLIDATE_CLUSTERS_H
