#include "p_combine_firings.h"

#include <mda.h>
#include "get_sort_indices.h"

bool p_combine_firings(QStringList firings_list, QString firings_out)
{
    QVector<int> all_central_channels;
    QVector<double> all_times;
    QVector<int> all_labels;

    int max_label=0;
    int label_offset=0;
    for (long i=0; i<firings_list.count(); i++) {
        Mda F(firings_list[i]);
        for (long j=0; j<F.N2(); j++) {
            all_central_channels << F.value(0,j);
            all_times << F.value(1,j);

            int label0=label_offset+F.value(2,j);
            if (label0>0) {
                all_labels << label0;
                if (label0>max_label) max_label=label0;
            }
        }
        label_offset=max_label;
    }

    long L=all_times.count();
    QList<long> sort_inds=get_sort_indices(all_times);

    Mda ret(3,L);
    for (long j=0; j<L; j++) {
        ret.setValue(all_central_channels[sort_inds[j]],0,j);
        ret.setValue(all_times[sort_inds[j]],1,j);
        ret.setValue(all_labels[sort_inds[j]],2,j);
    }
    return ret.write64(firings_out);
}
