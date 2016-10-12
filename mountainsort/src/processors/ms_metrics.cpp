/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 9/13/2016
*******************************************************/

#include "kdtree.h"
#include "ms_metrics.h"

#include <QTime>
#include <diskreadmda.h>
#include <diskreadmda32.h>
#include "mlcommon.h"
#include "extract_clips.h"
#include "synthesize1.h" //for randn
#include "msmisc.h"
#include "compute_templates_0.h"
#include "jsvm.h"
#include "noise_nearest.h"
#include "get_sort_indices.h"

namespace MSMetrics {

struct Metric {
    QList<double> values;
};

double compute_noise_overlap(const DiskReadMda32& X, const QVector<double>& times, ms_metrics_opts opts);
double compute_overlap(const DiskReadMda32& X, const QVector<double>& times1, const QVector<double>& times2, ms_metrics_opts opts);
QSet<QString> get_pairs_to_compare(const DiskReadMda32& X, const DiskReadMda& F, int num_comparisons_per_cluster, ms_metrics_opts opts);

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

    /*
    noise_nearest_opts nn_opts;
    nn_opts.cluster_numbers = opts.cluster_numbers;
    nn_opts.clip_size = opts.clip_size;
    nn_opts.add_noise_level = opts.add_noise_level;
    Mda isolation_matrix = NoiseNearest::compute_isolation_matrix(timeseries, firings, nn_opts);
    */

    printf("Cluster metrics...\n");
    QMap<QString, Metric> cluster_metrics;
    QTime timer;
    timer.start();
    for (int i = 0; i < opts.cluster_numbers.count(); i++) {
        if (timer.elapsed() > 5000) {
            qDebug() << QString("Cluster %1 of %2").arg(i + 1).arg(opts.cluster_numbers.count());
            timer.restart();
        }
        int k = opts.cluster_numbers[i];
        QVector<double> times_k;
        for (long i = 0; i < times.count(); i++) {
            if (labels[i] == k) {
                times_k << times[i];
            }
        }
        Mda32 clips_k = extract_clips(X, times_k, opts.clip_size);
        Mda32 template_k = compute_mean_clip(clips_k);
        Mda32 stdev_k = compute_stdev_clip(clips_k);
        {
            double min0 = template_k.minimum();
            double max0 = template_k.maximum();
            cluster_metrics["peak_amp"].values << qMax(qAbs(min0), qAbs(max0));
        }
        {
            double min0 = stdev_k.minimum();
            double max0 = stdev_k.maximum();
            cluster_metrics["peak_noise"].values << qMax(qAbs(min0), qAbs(max0));
        }
        {
            cluster_metrics["noise_overlap"].values << compute_noise_overlap(X, times_k, opts);
        }
        /*
        {
            double numer = isolation_matrix.value(i, opts.cluster_numbers.count());
            double denom = times_k.count();
            if (!denom)
                denom = 1;
            double val = numer / denom;
            cluster_metrics["noise_overlap"].values << val;
        }
        {
            double numer = isolation_matrix.value(i, i);
            double denom = times_k.count();
            if (!denom)
                denom = 1;
            double val = numer / denom;
            cluster_metrics["isolation"].values << val;
        }
        */
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

    printf("get pairs to compare...\n");
    QSet<QString> pairs_to_compare = get_pairs_to_compare(X, F, 5, opts);

    //////////////////////////////////////////////////////////////
    printf("Cluster pair metrics...\n");
    QMap<QString, Metric> cluster_pair_metrics;
    QTime timer1;
    timer1.start();
    for (int i1 = 0; i1 < opts.cluster_numbers.count(); i1++) {
        if (timer1.elapsed() > 5000) {
            qDebug() << QString("Cluster pair metrics %1 of %2").arg(i1 + 1).arg(opts.cluster_numbers.count());
            timer1.restart();
        }
        for (int i2 = 0; i2 < opts.cluster_numbers.count(); i2++) {
            int k1 = opts.cluster_numbers[i1];
            int k2 = opts.cluster_numbers[i2];

            if (pairs_to_compare.contains(QString("%1-%2").arg(k1).arg(k2))) {
                QVector<double> times_k1;
                for (long i = 0; i < times.count(); i++) {
                    if (labels[i] == k1) {
                        times_k1 << times[i];
                    }
                }
                QVector<double> times_k2;
                for (long i = 0; i < times.count(); i++) {
                    if (labels[i] == k2) {
                        times_k2 << times[i];
                    }
                }
                double val = compute_overlap(X, times_k1, times_k2, opts);
                if (val >= 0.01) { //to save some space in the file
                    cluster_pair_metrics["overlap"].values << val;
                }
                else {
                    cluster_pair_metrics["overlap"].values << 0;
                }
            }
            else {
                cluster_pair_metrics["overlap"].values << 0;
            }
        }
    }

    QStringList cluster_pair_metric_names = cluster_pair_metrics.keys();
    QString cluster_pair_metric_txt = "cluster1,cluster2," + cluster_pair_metric_names.join(",") + "\n";
    int jj = 0;
    for (int i1 = 0; i1 < opts.cluster_numbers.count(); i1++) {
        for (int i2 = 0; i2 < opts.cluster_numbers.count(); i2++) {
            bool has_something_non_zero = false;
            foreach (QString name, cluster_pair_metric_names) {
                if (cluster_pair_metrics[name].values[jj])
                    has_something_non_zero = true;
            }
            if ((has_something_non_zero) && (i1 != i2)) {
                QString line = QString("%1,%2").arg(opts.cluster_numbers[i1]).arg(opts.cluster_numbers[i2]);
                foreach (QString name, cluster_pair_metric_names) {
                    line += QString(",%1").arg(cluster_pair_metrics[name].values[jj]);
                }
                cluster_pair_metric_txt += line + "\n";
            }
            jj++;
        }
    }
    if (!TextFile::write(cluster_pair_metrics_path, cluster_pair_metric_txt)) {
        qWarning() << "Problem writing output file: " + cluster_pair_metrics_path;
        return false;
    }

    return true;
}

long random_time(long N, int clip_size)
{
    if (N <= clip_size * 2)
        return N / 2;
    return clip_size + (qrand() % (N - clip_size * 2));
}

QVector<double> sample(const QVector<double>& times, long num)
{
    QVector<double> random_values(times.count());
    for (long i = 0; i < times.count(); i++) {
        random_values[i] = sin(i * 12 + i * i);
    }
    QList<long> inds = get_sort_indices(random_values);
    QVector<double> ret;
    for (long i = 0; (i < num) && (i < inds.count()); i++) {
        ret << times[inds[i]];
    }
    return ret;
}

double compute_noise_overlap(const DiskReadMda32& X, const QVector<double>& times, ms_metrics_opts opts)
{
    int num_to_use = qMin(opts.max_num_to_use, times.count());
    QVector<double> times_subset = sample(times, num_to_use);

    QVector<double> all_times = times_subset;
    QVector<int> all_labels; //0 and 1

    for (long i = 0; i < times_subset.count(); i++) {
        all_labels << 1;
    }
    //equal amount of random clips
    for (long i = 0; i < times_subset.count(); i++) {
        all_times << random_time(X.N2(), opts.clip_size);
        all_labels << 0;
    }

    Mda32 all_clips = extract_clips(X, all_times, opts.clip_size);

    Mda32 all_clips_reshaped(all_clips.N1() * all_clips.N2(), all_clips.N3());
    long NNN = all_clips.totalSize();
    for (long iii = 0; iii < NNN; iii++) {
        all_clips_reshaped.set(all_clips.get(iii), iii);
    }

    bool subtract_mean = false;
    Mda32 FF;
    Mda32 CC, sigma;
    pca(CC, FF, sigma, all_clips_reshaped, opts.num_features, subtract_mean);

    KdTree tree;
    tree.create(FF);
    double num_correct = 0;
    double num_total = 0;
    for (long i = 0; i < FF.N2(); i++) {
        QVector<float> p;
        for (int j = 0; j < FF.N1(); j++) {
            p << FF.value(j, i);
        }
        QList<long> indices = tree.findApproxKNearestNeighbors(FF, p, opts.K_nearest, opts.exhaustive_search_num);
        for (int a = 0; a < indices.count(); a++) {
            if (indices[a] != i) {
                if (all_labels[indices[a]] == all_labels[i])
                    num_correct++;
                num_total++;
            }
        }
    }
    if (!num_total)
        return 0;
    return 1 - (num_correct * 1.0 / num_total);
}

double compute_overlap(const DiskReadMda32& X, const QVector<double>& times1, const QVector<double>& times2, ms_metrics_opts opts)
{
    int num_to_use = qMin(qMin(opts.max_num_to_use, times1.count()), times2.count());
    if (num_to_use < opts.min_num_to_use)
        return 0;
    QVector<double> times1_subset = sample(times1, num_to_use);
    QVector<double> times2_subset = sample(times2, num_to_use);

    QVector<double> all_times;
    QVector<int> all_labels; //1 and 2

    for (long i = 0; i < times1_subset.count(); i++) {
        all_times << times1_subset[i];
        all_labels << 1;
    }
    for (long i = 0; i < times2_subset.count(); i++) {
        all_times << times2_subset[i];
        all_labels << 2;
    }

    Mda32 all_clips = extract_clips(X, all_times, opts.clip_size);

    Mda32 all_clips_reshaped(all_clips.N1() * all_clips.N2(), all_clips.N3());
    long NNN = all_clips.totalSize();
    for (long iii = 0; iii < NNN; iii++) {
        all_clips_reshaped.set(all_clips.get(iii), iii);
    }

    bool subtract_mean = false;
    Mda32 FF;
    Mda32 CC, sigma;
    pca(CC, FF, sigma, all_clips_reshaped, opts.num_features, subtract_mean);

    KdTree tree;
    tree.create(FF);
    double num_correct = 0;
    double num_total = 0;
    for (long i = 0; i < all_times.count(); i++) {
        QVector<float> p;
        for (int j = 0; j < FF.N1(); j++) {
            p << FF.value(j, i);
        }
        QList<long> indices = tree.findApproxKNearestNeighbors(FF, p, opts.K_nearest, opts.exhaustive_search_num);
        for (int a = 0; a < indices.count(); a++) {
            if (indices[a] != i) {
                if (all_labels[indices[a]] == all_labels[i])
                    num_correct++;
                num_total++;
            }
        }
    }
    if (!num_total)
        return 0;
    return 1 - (num_correct * 1.0 / num_total);
}

QVector<long> find_label_inds(const QVector<int>& labels, int k)
{
    QVector<long> ret;
    for (int i = 0; i < labels.count(); i++) {
        if (labels[i] == k)
            ret << i;
    }
    return ret;
}

double distsqr_between_templates(const Mda32& X, const Mda32& Y)
{
    double ret = 0;
    for (long i = 0; i < X.totalSize(); i++) {
        double tmp = X.get(i) - Y.get(i);
        ret += tmp * tmp;
    }
    return ret;
}

QSet<QString> get_pairs_to_compare(const DiskReadMda32& X, const DiskReadMda& F, int num_comparisons_per_cluster, ms_metrics_opts opts)
{
    QSet<QString> ret;

    QSet<int> cluster_numbers_set;
    for (int i = 0; i < opts.cluster_numbers.count(); i++) {
        cluster_numbers_set.insert(opts.cluster_numbers[i]);
    }

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

    Mda32 templates0 = compute_templates_0(X, times, labels, opts.clip_size);

    for (int i1 = 0; i1 < opts.cluster_numbers.count(); i1++) {
        int k1 = opts.cluster_numbers[i1];
        Mda32 template1;
        templates0.getChunk(template1, 0, 0, k1 - 1, template1.N1(), template1.N2(), 1);
        QVector<double> dists;
        for (int i2 = 0; i2 < opts.cluster_numbers.count(); i2++) {
            Mda32 template2;
            int k2 = opts.cluster_numbers[i2];
            templates0.getChunk(template2, 0, 0, k2 - 1, template2.N1(), template2.N2(), 1);
            dists << distsqr_between_templates(template1, template2);
        }
        QList<long> inds = get_sort_indices(dists);
        for (int a = 0; (a < inds.count()) && (a < num_comparisons_per_cluster); a++) {
            ret.insert(QString("%1-%2").arg(k1).arg(opts.cluster_numbers[inds[a]]));
        }
    }

    return ret;
}
}
