/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 9/13/2016
*******************************************************/

#include "ms_metrics.h"

#include <diskreadmda.h>
#include <diskreadmda32.h>
#include "mlcommon.h"
#include "extract_clips.h"
#include "synthesize1.h" //for randn
#include "msmisc.h"
#include "compute_templates_0.h"
#include "jsvm.h"

namespace MSMetrics {

struct Metric {
    QList<double> values;
};

bool ms_metrics(QString timeseries, QString firings, QString cluster_metrics_path, QString cluster_pair_metrics_path, ms_metrics_opts opts)
{
    DiskReadMda32 X(timeseries);
    DiskReadMda F(firings);

    //define opts.cluster_numbers in case it is empty
    QVector<int> labels0;
    for (long i = 0; i < F.N2(); i++) {
        labels0 << (int)F.value(2, i);
    }
    int K = MLCompute::max(labels0);
    if (opts.cluster_numbers.isEmpty()) {
        for (int k = 1; k <= K; k++) {
            opts.cluster_numbers << k;
        }
    }
    QSet<int> cluster_numbers_set;
    for (int i = 0; i < opts.cluster_numbers.count(); i++) {
        cluster_numbers_set.insert(opts.cluster_numbers[i]);
    }
    qDebug() << "Using cluster numbers:" << opts.cluster_numbers;

    printf("Extracting times and labels...\n");
    //QVector<long> inds;
    QVector<double> times;
    QVector<int> labels;
    for (long i = 0; i < F.N2(); i++) {
        int label0 = (int)F.value(2, i);
        if (cluster_numbers_set.contains(label0)) {
            //inds << i;
            times << F.value(1, i);
            labels << label0;
        }
    }

    QMap<QString, Metric> cluster_metrics;
    for (int i = 0; i < opts.cluster_numbers.count(); i++) {
        int k = opts.cluster_numbers[i];
        QVector<double> times_k;
        for (long i = 0; i < times.count(); i++) {
            if (labels[i] == k) {
                times_k << times[i];
            }
        }
        Mda32 clips_k = extract_clips(X, times_k, opts.clip_size);
        Mda32 template_k = compute_mean_clip(clips_k);
        double min0 = template_k.minimum();
        double max0 = template_k.maximum();
        cluster_metrics["abspeak"].values << qMax(qAbs(min0), qAbs(max0));
    }

    QStringList cluster_metric_names = cluster_metrics.keys();
    QString cluster_metric_txt = "cluster," + cluster_metric_names.join(",") + "\n";
    for (int i = 0; i < opts.cluster_numbers.count(); i++) {
        QString line = QString("%1").arg(opts.cluster_numbers[i]);
        foreach (QString name, cluster_metric_names) {
            line += QString(",%1").arg(cluster_metrics[name].values[i]);
        }
        cluster_metric_txt += line + "\n";
    }
    if (!TextFile::write(cluster_metrics_path, cluster_metric_txt)) {
        qWarning() << "Problem writing output file: " + cluster_metrics_path;
        return false;
    }

    TextFile::write(cluster_pair_metrics_path, "");

    return true;
}
}
