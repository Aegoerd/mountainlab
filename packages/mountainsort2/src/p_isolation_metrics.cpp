#include "p_isolation_metrics.h"
#include "get_sort_indices.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTime>
#include <QVector>
#include <diskreadmda.h>
#include <diskreadmda32.h>
#include <mda.h>
#include <mda32.h>
#include "pca.h"
#include "kdtree.h"
#include "compute_templates_0.h"

namespace P_isolation_metrics {
Mda32 extract_clips(const DiskReadMda32 &X,QVector<double> &times,int clip_size);
Mda32 compute_mean_clip(const Mda32 &clips);
QJsonObject get_cluster_metrics(const DiskReadMda32 &X,const QVector<double> &times,P_isolation_metrics_opts opts);
QJsonObject get_pair_metrics(const DiskReadMda32 &X,const QVector<double> &times_k1,const QVector<double> &times_k2,P_isolation_metrics_opts opts);
QSet<QString> get_pairs_to_compare(const DiskReadMda32& X, const DiskReadMda& F, bigint num_comparisons_per_cluster, P_isolation_metrics_opts opts);
double compute_overlap(const DiskReadMda32& X, const QVector<double>& times1, const QVector<double>& times2, P_isolation_metrics_opts opts);
struct ClusterData {
    QVector<double> times;
    QJsonObject cluster_metrics;
};
}

bool p_isolation_metrics(QString timeseries_path,QString firings_path,QString metrics_out_path,QString pair_metrics_out_path,P_isolation_metrics_opts opts) {
    DiskReadMda32 X(timeseries_path);
    Mda firings(firings_path);

    QMap<int,P_isolation_metrics::ClusterData> cluster_data;

    QVector<double> times(firings.N2());
    QVector<int> labels(firings.N2());
    for (bigint i = 0; i < firings.N2(); i++) {
        times[i] = firings.value(1, i);
        labels[i] = firings.value(2, i);
    }

    QSet<int> used_labels_set;
    for (bigint i = 0; i < labels.count(); i++) {
        used_labels_set.insert(labels[i]);
    }
    QList<int> used_labels = used_labels_set.toList();
    qSort(used_labels);

    foreach (int k, used_labels) {
        QVector<double> times_k;
        for (bigint i=0; i<labels.count(); i++) {
            if (labels[i]==k) times_k << times[i];
        }
        QJsonObject tmp = P_isolation_metrics::get_cluster_metrics(X, times, opts);
        P_isolation_metrics::ClusterData CD;
        CD.times=times_k;
        CD.cluster_metrics=tmp;
        cluster_data[k]=CD;
    }

    QJsonArray clusters;
    foreach (int k,used_labels) {
        QJsonObject tmp;
        tmp["label"]=k;
        tmp["metrics"]=cluster_data[k].cluster_metrics;
        clusters.push_back(tmp);
    }

    QJsonArray cluster_pairs;
    int num_comparisons_per_cluster=10;
    QSet<QString> pairs_to_compare=P_isolation_metrics::get_pairs_to_compare(X,firings,num_comparisons_per_cluster,opts);
    QList<QString> pairs_to_compare_list=pairs_to_compare.toList();
    qSort(pairs_to_compare_list);
    foreach (QString pairstr,pairs_to_compare_list) {
        QStringList vals = pairstr.split("-");
        int k1 = vals[0].toInt();
        int k2 = vals[1].toInt();
        if ((cluster_data.contains(k1))&&(cluster_data.contains(k2))) {
            QVector<double> times_k1=cluster_data[k1].times;
            QVector<double> times_k2=cluster_data[k2].times;
            QJsonObject pair_metrics=P_isolation_metrics::get_pair_metrics(X,times_k1,times_k2,opts);
            QJsonObject tmp;
            tmp["label"]=QString("%1,%2").arg(k1).arg(k2);
            tmp["metrics"]=pair_metrics;
            cluster_pairs.push_back(tmp);
        }
    }

    {
        QJsonObject obj;
        obj["clusters"] = clusters;
        QString json = QJsonDocument(obj).toJson(QJsonDocument::Indented);
        if (!TextFile::write(metrics_out_path, json))
            return false;
    }

    {
        QJsonObject obj;
        obj["cluster_pairs"] = clusters;
        QString json = QJsonDocument(obj).toJson(QJsonDocument::Indented);
        if (!TextFile::write(pair_metrics_out_path, json))
            return false;
    }

    return true;

}

namespace P_isolation_metrics {
bigint random_time(bigint N, bigint clip_size)
{
    if (N <= clip_size * 2)
        return N / 2;
    return clip_size + (qrand() % (N - clip_size * 2));
}

QVector<double> sample(const QVector<double>& times, bigint num)
{
    QVector<double> random_values(times.count());
    for (bigint i = 0; i < times.count(); i++) {
        random_values[i] = sin(i * 12 + i * i);
    }
    QList<bigint> inds = get_sort_indices_bigint(random_values);
    QVector<double> ret;
    for (bigint i = 0; (i < num) && (i < inds.count()); i++) {
        ret << times[inds[i]];
    }
    return ret;
}

Mda32 compute_noise_shape(const Mda32& noise_clips, const Mda32& template0)
{
    bigint peak_channel = 0;
    bigint peak_timepoint = 0;
    double best_val = 0;
    for (bigint t = 0; t < template0.N2(); t++) {
        for (bigint m = 0; m < template0.N1(); m++) {
            double val = qAbs(template0.value(m, t));
            if (val > best_val) {
                best_val = val;
                peak_channel = m;
                peak_timepoint = t;
            }
        }
    }
    Mda ret(template0.N1(), template0.N2());
    for (bigint i = 0; i < noise_clips.N3(); i++) {
        double weight = noise_clips.value(peak_channel, peak_timepoint, i) / noise_clips.N3();
        for (bigint t = 0; t < template0.N2(); t++) {
            for (bigint m = 0; m < template0.N1(); m++) {
                ret.set(ret.get(m, t) + noise_clips.get(m, t, i) * weight, m, t);
            }
        }
    }
    Mda32 ret2(ret.N1(), ret.N2());
    for (bigint i = 0; i < ret.totalSize(); i++) {
        ret2.set(ret.get(i), i);
    }
    return ret2;
}

Mda32 compute_noise_shape_2(const DiskReadMda32& X, const Mda32& template0, P_isolation_metrics_opts opts)
{
    bigint num_noise_times = 1000;
    QVector<double> noise_times;
    for (bigint i = 0; i < num_noise_times; i++) {
        noise_times << random_time(X.N2(), opts.clip_size);
    }

    Mda32 clips = extract_clips(X, noise_times, opts.clip_size);
    return compute_noise_shape(clips, template0);
}

void regress_out_noise_shape(Mda32& clips, const Mda32& shape)
{
    double shape_norm = MLCompute::norm(shape.totalSize(), shape.constDataPtr());
    for (bigint i = 0; i < clips.N3(); i++) {
        Mda32 clip;
        clips.getChunk(clip, 0, 0, i, clips.N1(), clips.N2(), 1);
        double inner_product = MLCompute::dotProduct(clip.totalSize(), clip.constDataPtr(), shape.constDataPtr());
        if (shape_norm) {
            double coef = inner_product / (shape_norm * shape_norm);
            for (bigint j = 0; j < clip.totalSize(); j++) {
                clip.set(clip.get(j) - coef * shape.get(j), j);
            }
        }
        clips.setChunk(clip, 0, 0, i);
    }
}

double compute_noise_overlap(const DiskReadMda32& X, const QVector<double>& times, P_isolation_metrics_opts opts, bool debug)
{
    QTime timer;
    timer.start();

    QList<bigint> elapsed_times;

    bigint num_to_use = qMin(opts.max_num_to_use, times.count());
    QVector<double> times_subset = sample(times, num_to_use);

    QVector<bigint> labels_subset;
    for (bigint i = 0; i < times_subset.count(); i++) {
        labels_subset << 1;
    }
    //equal amount of random clips
    QVector<double> noise_times;
    QVector<bigint> noise_labels;
    for (bigint i = 0; i < times_subset.count(); i++) {
        noise_times << random_time(X.N2(), opts.clip_size);
        noise_labels << 0;
    }

    elapsed_times << timer.restart();

    QVector<double> all_times = times_subset;
    QVector<bigint> all_labels = labels_subset; //0 and 1

    all_times.append(noise_times);
    all_labels.append(noise_labels);

    Mda32 clips = extract_clips(X, times_subset, opts.clip_size);
    //Mda32 noise_clips = extract_clips(X, noise_times, opts.clip_size);
    Mda32 all_clips = extract_clips(X, all_times, opts.clip_size);

    elapsed_times << timer.restart();

    //Mda32 noise_shape = compute_noise_shape(noise_clips, compute_mean_clip(clips));
    Mda32 noise_shape = compute_noise_shape_2(X, compute_mean_clip(clips), opts);
    elapsed_times << timer.restart();
    if (debug)
        noise_shape.write32("/tmp/noise_shape.mda");
    if (debug)
        all_clips.write32("/tmp/all_clips_before.mda");
    regress_out_noise_shape(all_clips, noise_shape);
    elapsed_times << timer.restart();
    if (debug)
        all_clips.write32("/tmp/all_clips_after.mda");

    elapsed_times << timer.restart();

    Mda32 all_clips_reshaped(all_clips.N1() * all_clips.N2(), all_clips.N3());
    bigint NNN = all_clips.totalSize();
    for (bigint iii = 0; iii < NNN; iii++) {
        all_clips_reshaped.set(all_clips.get(iii), iii);
    }

    elapsed_times << timer.restart();

    bool subtract_mean = false;
    Mda32 FF;
    Mda32 CC, sigma;
    pca(CC, FF, sigma, all_clips_reshaped, opts.num_features, subtract_mean);

    elapsed_times << timer.restart();

    KdTree tree;
    tree.create(FF);
    double num_correct = 0;
    double num_total = 0;
    for (bigint i = 0; i < FF.N2(); i++) {
        QVector<float> p;
        for (bigint j = 0; j < FF.N1(); j++) {
            p << FF.value(j, i);
        }
        QList<int> indices = tree.findApproxKNearestNeighbors(FF, p, opts.K_nearest, opts.exhaustive_search_num);
        for (bigint a = 0; a < indices.count(); a++) {
            if (indices[a] != i) {
                if (all_labels[indices[a]] == all_labels[i])
                    num_correct++;
                num_total++;
            }
        }
    }

    elapsed_times << timer.restart();

    if (false)
        qDebug().noquote() << "TIMES:" << elapsed_times;

    if (!num_total)
        return 0;
    return 1 - (num_correct * 1.0 / num_total);
}

double compute_overlap(const DiskReadMda32& X, const QVector<double>& times1, const QVector<double>& times2, P_isolation_metrics_opts opts)
{
    bigint num_to_use = qMin(qMin(opts.max_num_to_use, times1.count()), times2.count());
    if (num_to_use < opts.min_num_to_use)
        return 0;
    QVector<double> times1_subset = sample(times1, num_to_use);
    QVector<double> times2_subset = sample(times2, num_to_use);

    QVector<double> all_times;
    QVector<bigint> all_labels; //1 and 2

    for (bigint i = 0; i < times1_subset.count(); i++) {
        all_times << times1_subset[i];
        all_labels << 1;
    }
    for (bigint i = 0; i < times2_subset.count(); i++) {
        all_times << times2_subset[i];
        all_labels << 2;
    }

    Mda32 all_clips = extract_clips(X, all_times, opts.clip_size);

    Mda32 all_clips_reshaped(all_clips.N1() * all_clips.N2(), all_clips.N3());
    bigint NNN = all_clips.totalSize();
    for (bigint iii = 0; iii < NNN; iii++) {
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
    for (bigint i = 0; i < all_times.count(); i++) {
        QVector<float> p;
        for (bigint j = 0; j < FF.N1(); j++) {
            p << FF.value(j, i);
        }
        QList<int> indices = tree.findApproxKNearestNeighbors(FF, p, opts.K_nearest, opts.exhaustive_search_num);
        for (bigint a = 0; a < indices.count(); a++) {
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

QVector<bigint> find_label_inds(const QVector<bigint>& labels, bigint k)
{
    QVector<bigint> ret;
    for (bigint i = 0; i < labels.count(); i++) {
        if (labels[i] == k)
            ret << i;
    }
    return ret;
}

double distsqr_between_templates(const Mda32& X, const Mda32& Y)
{
    double ret = 0;
    for (bigint i = 0; i < X.totalSize(); i++) {
        double tmp = X.get(i) - Y.get(i);
        ret += tmp * tmp;
    }
    return ret;
}

QSet<QString> get_pairs_to_compare(const DiskReadMda32& X, const DiskReadMda& F, bigint num_comparisons_per_cluster, P_isolation_metrics_opts opts)
{
    QSet<QString> ret;

    QSet<bigint> cluster_numbers_set;
    for (bigint i = 0; i < opts.cluster_numbers.count(); i++) {
        cluster_numbers_set.insert(opts.cluster_numbers[i]);
    }

    QVector<double> times;
    QVector<int> labels;
    for (bigint i = 0; i < F.N2(); i++) {
        bigint label0 = (bigint)F.value(2, i);
        if (cluster_numbers_set.contains(label0)) {
            //inds << i;
            times << F.value(1, i);
            labels << label0;
        }
    }

    Mda32 templates0 = compute_templates_0(X, times, labels, opts.clip_size);

    for (bigint i1 = 0; i1 < opts.cluster_numbers.count(); i1++) {
        bigint k1 = opts.cluster_numbers[i1];
        Mda32 template1;
        templates0.getChunk(template1, 0, 0, k1 - 1, template1.N1(), template1.N2(), 1);
        QVector<double> dists;
        for (bigint i2 = 0; i2 < opts.cluster_numbers.count(); i2++) {
            Mda32 template2;
            bigint k2 = opts.cluster_numbers[i2];
            templates0.getChunk(template2, 0, 0, k2 - 1, template2.N1(), template2.N2(), 1);
            dists << distsqr_between_templates(template1, template2);
        }
        QList<bigint> inds = get_sort_indices_bigint(dists);
        for (bigint a = 0; (a < inds.count()) && (a < num_comparisons_per_cluster); a++) {
            ret.insert(QString("%1-%2").arg(k1).arg(opts.cluster_numbers[inds[a]]));
        }
    }

    return ret;
}

Mda32 extract_clips(const DiskReadMda32 &X,QVector<double> &times,int clip_size)
{
    int M=X.N1();
    int T=clip_size;
    int Tmid = (bigint)((T + 1) / 2) - 1;
    bigint L=times.count();
    Mda32 clips(M, T, L);
    for (bigint i = 0; i < L; i++) {
        bigint t1 = times.value(i) - Tmid;
        //bigint t2 = t1 + T - 1;
        Mda32 tmp;
        X.readChunk(tmp, 0, t1, M, T);
        clips.setChunk(tmp, 0, 0, i);
    }
    return clips;
}
Mda32 compute_mean_clip(const Mda32& clips)
{
    int M = clips.N1();
    int T = clips.N2();
    int L = clips.N3();
    Mda32 ret;
    ret.allocate(M, T);
    int aaa = 0;
    for (int i = 0; i < L; i++) {
        int bbb = 0;
        for (int t = 0; t < T; t++) {
            for (int m = 0; m < M; m++) {
                ret.set(ret.get(bbb) + clips.get(aaa), bbb);
                aaa++;
                bbb++;
            }
        }
    }
    if (L) {
        for (int t = 0; t < T; t++) {
            for (int m = 0; m < M; m++) {
                ret.set(ret.get(m, t) / L, m, t);
            }
        }
    }
    return ret;
}
QJsonObject get_cluster_metrics(const DiskReadMda32 &X,const QVector<double> &times,P_isolation_metrics_opts opts) {

}
QJsonObject get_pair_metrics(const DiskReadMda32 &X,const QVector<double> &times_k1,const QVector<double> &times_k2,P_isolation_metrics_opts opts) {
    QJsonObject pair_metrics;
    double overlap=P_isolation_metrics::compute_overlap(X,times_k1,times_k2,opts);
    pair_metrics["overlap"]=overlap;
    return pair_metrics;
}
}
