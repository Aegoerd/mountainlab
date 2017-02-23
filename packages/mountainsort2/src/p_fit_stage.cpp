#include "p_fit_stage.h"

#include <diskreadmda.h>
#include <diskreadmda32.h>
#include <mda.h>
#include <mda32.h>
#include "get_sort_indices.h"
#include "mlcommon.h"

namespace P_fit_stage {
Mda sort_firings_by_time(const Mda& firings);
Mda32 compute_templates(const DiskReadMda32& X, const QVector<double>& times, const QVector<int>& labels, int clip_size);
QList<long> fit_stage_kernel(Mda32& X, Mda32& templates, QVector<double>& times, QVector<int>& labels, const Fit_stage_opts& opts);
}

bool p_fit_stage(QString timeseries_path, QString firings_path, QString firings_out_path, Fit_stage_opts opts)
{
    //The timeseries data and the dimensions
    DiskReadMda32 X(timeseries_path);
    long M = X.N1();
    long N = X.N2();
    int T = opts.clip_size;

    //Read in the firings array
    Mda firingsA;
    firingsA.read(firings_path);
    Mda firings = P_fit_stage::sort_firings_by_time(firingsA);

    //L is the number of events. Accumulate vectors of times and labels for convenience
    long L = firings.N2();
    QVector<double> times;
    QVector<int> labels;
    for (long j = 0; j < L; j++) {
        times << firings.value(1, j);
        labels << (int)firings.value(2, j); //these are labels of clusters
    }

    //These are the templates corresponding to the clusters
    Mda32 templates = P_fit_stage::compute_templates(X, times, labels, T); //MxTxK

    long processing_chunk_size = 1e6;
    long processing_chunk_overlap_size = 1e4;

    //Now we do the processing in chunks
    long chunk_size = processing_chunk_size;
    long overlap_size = processing_chunk_overlap_size;
    if (N < processing_chunk_size) {
        chunk_size = N;
        overlap_size = 0;
    }

    QList<long> inds_to_use;

    printf("Starting fit stage\n");
    {
        long num_timepoints_handled = 0;
#pragma omp parallel for
        for (long timepoint = 0; timepoint < N; timepoint += chunk_size) {
            QMap<QString, long> elapsed_times_local;
            Mda32 chunk; //this will be the chunk we are working on
            Mda32 local_templates; //just a local copy of the templates
            QVector<double> local_times; //the times that fall in this time range
            QVector<int> local_labels; //the corresponding labels
            QList<long> local_inds; //the corresponding event indices
            Fit_stage_opts local_opts;
#pragma omp critical(lock1)
            {
                //build the variables above
                local_templates = templates;
                local_opts = opts;
                X.readChunk(chunk, 0, timepoint - overlap_size, M, chunk_size + 2 * overlap_size);
                for (long jj = 0; jj < L; jj++) {
                    if ((timepoint - overlap_size <= times[jj]) && (times[jj] < timepoint - overlap_size + chunk_size + 2 * overlap_size)) {
                        local_times << times[jj] - (timepoint - overlap_size);
                        local_labels << labels[jj];
                        local_inds << jj;
                    }
                }
            }
            //Our real task is to decide which of these events to keep. Those will be stored in local_inds_to_use
            //"Local" means this chunk in this thread
            QList<long> local_inds_to_use;
            {
                //This is the main kernel operation!!
                local_inds_to_use = P_fit_stage::fit_stage_kernel(chunk, local_templates, local_times, local_labels, local_opts);
            }
#pragma omp critical(lock1)
            {
                {
                    for (long ii = 0; ii < local_inds_to_use.count(); ii++) {
                        long ind0 = local_inds[local_inds_to_use[ii]];
                        double t0 = times[ind0];
                        if ((timepoint <= t0) && (t0 < timepoint + chunk_size)) {
                            inds_to_use << ind0;
                        }
                    }
                }

                num_timepoints_handled += qMin(chunk_size, N - timepoint);
            }
        }
    }

    qSort(inds_to_use);
    long num_to_use = inds_to_use.count();
    if (times.count()) {
        printf("using %ld/%ld events (%g%%)\n", num_to_use, (long)times.count(), num_to_use * 100.0 / times.count());
    }
    Mda firings_out(firings.N1(), num_to_use);
    for (long i = 0; i < num_to_use; i++) {
        for (int j = 0; j < firings.N1(); j++) {
            firings_out.set(firings.get(j, inds_to_use[i]), j, i);
        }
    }

    return firings_out.write64(firings_out_path);
}

namespace P_fit_stage {

Mda sort_firings_by_time(const Mda& firings)
{
    QVector<double> times;
    for (long i = 0; i < firings.N2(); i++) {
        times << firings.value(1, i);
    }
    QList<long> sort_inds = get_sort_indices(times);

    Mda F(firings.N1(), firings.N2());
    for (long i = 0; i < firings.N2(); i++) {
        for (int j = 0; j < firings.N1(); j++) {
            F.setValue(firings.value(j, sort_inds[i]), j, i);
        }
    }

    return F;
}

Mda32 compute_templates(const DiskReadMda32& X, const QVector<double>& times, const QVector<int>& labels, int clip_size)
{
    int M = X.N1();
    int T = clip_size;
    long L = times.count();

    int K = MLCompute::max<int>(labels);

    int Tmid = (int)((T + 1) / 2) - 1;

    Mda32 templates(M, T, K);
    QList<long> counts;
    for (int k = 0; k < K; k++)
        counts << k;
    for (long i = 0; i < L; i++) {
        int k = labels[i];
        long t0 = (long)(times[i] + 0.5);
        if (k >= 1) {
            Mda32 X0;
            X.readChunk(X0, 0, t0 - Tmid, M, T);
            dtype32* Xptr = X0.dataPtr();
            dtype32* Tptr = templates.dataPtr(0, 0, k - 1);
            for (int i = 0; i < M * T; i++) {
                Tptr[i] += Xptr[i];
            }
            counts[k - 1]++;
        }
    }
    for (int k = 0; k < K; k++) {
        for (int t = 0; t < T; t++) {
            for (int m = 0; m < M; m++) {
                if (counts[k]) {
                    templates.set(templates.get(m, t, k) / counts[k], m, t, k);
                }
            }
        }
    }

    return templates;
}

typedef QList<int> IntList;

QList<int> get_channel_mask(Mda32 template0, int num)
{
    int M = template0.N1();
    int T = template0.N2();
    QVector<double> maxabs;
    for (int m = 0; m < M; m++) {
        double val = 0;
        for (int t = 0; t < T; t++) {
            val = qMax(val, 1.0 * qAbs(template0.value(m, t)));
        }
        maxabs << val;
    }
    QList<long> inds = get_sort_indices(maxabs);
    QList<int> ret;
    for (int i = 0; i < num; i++) {
        if (i < inds.count()) {
            ret << inds[inds.count() - 1 - i];
        }
    }
    return ret;
}

bool is_dirty(const Mda& dirty, long t0, const QList<int>& chmask)
{
    for (int i = 0; i < chmask.count(); i++) {
        if (dirty.value(chmask[i], t0))
            return true;
    }
    return false;
}

double compute_score(long N, float* X, float* template0)
{
    Mda resid(1, N);
    double* resid_ptr = resid.dataPtr();
    for (long i = 0; i < N; i++)
        resid_ptr[i] = X[i] - template0[i];
    double norm1 = MLCompute::norm(N, X);
    double norm2 = MLCompute::norm(N, resid_ptr);
    return norm1 * norm1 - norm2 * norm2;
}

double compute_score(int M, int T, float* X, float* template0, const QList<int>& chmask)
{
    double before_sumsqr = 0;
    double after_sumsqr = 0;
    for (int t = 0; t < T; t++) {
        for (int i = 0; i < chmask.count(); i++) {
            int m = chmask[i];
            double val = X[m + t * M];
            before_sumsqr += val * val;
            val -= template0[m + t * M];
            after_sumsqr += val * val;
        }
    }

    return before_sumsqr - after_sumsqr;
}

void subtract_scaled_template(long N, double* X, double* template0)
{
    double S12 = 0, S22 = 0;
    for (long i = 0; i < N; i++) {
        S22 += template0[i] * template0[i];
        S12 += X[i] * template0[i];
    }
    double alpha = 1;
    if (S22)
        alpha = S12 / S22;
    for (long i = 0; i < N; i++) {
        X[i] -= alpha * template0[i];
    }
}

void subtract_scaled_template(int M, int T, float* X, float* template0, const QList<int>& chmask)
{
    double S12 = 0, S22 = 0;
    for (int t = 0; t < T; t++) {
        for (int j = 0; j < chmask.count(); j++) {
            int m = chmask[j];
            int i = m + M * t;
            S22 += template0[i] * template0[i];
            S12 += X[i] * template0[i];
        }
    }
    double alpha = 1;
    if (S22)
        alpha = S12 / S22;
    for (int t = 0; t < T; t++) {
        for (int j = 0; j < chmask.count(); j++) {
            int m = chmask[j];
            int i = m + M * t;
            X[i] -= alpha * template0[i];
        }
    }
}

QVector<int> find_events_to_use(const QVector<double>& times, const QVector<double>& scores, const Fit_stage_opts& opts)
{
    QVector<int> to_use;
    long L = times.count();
    for (long i = 0; i < L; i++)
        to_use << 0; //start out not using any
    for (long i = 0; i < L; i++) {
        if (scores[i] > 0) { //score has to at least be positive
            to_use[i] = 1; //for now we say we are using it
            {
                // but let's check nearby events that may have a larger score
                long j = i;
                while ((j >= 0) && (times[j] >= times[i] - opts.clip_size)) {
                    if ((i != j) && (scores[j] >= scores[i]))
                        to_use[i] = 0; //actually not using it because there is something bigger to the left
                    j--;
                }
            }
            {
                long j = i;
                while ((j < times.count()) && (times[j] <= times[i] + opts.clip_size)) {
                    if (scores[j] > scores[i])
                        to_use[i] = 0; //actually not using it because there is something bigger to the right
                    j++;
                }
            }
        }
    }
    return to_use;
}

QList<long> fit_stage_kernel(Mda32& X, Mda32& templates, QVector<double>& times, QVector<int>& labels, const Fit_stage_opts& opts)
{
    int M = X.N1(); //the number of dimensions
    int T = opts.clip_size; //the clip size
    int Tmid = (int)((T + 1) / 2) - 1; //the center timepoint in a clip (zero-indexed)
    long L = times.count(); //number of events we are looking at
    int K = MLCompute::max<int>(labels); //the maximum label number

    //compute the L2-norms of the templates ahead of time
    QVector<double> template_norms;
    template_norms << 0;
    for (int k = 1; k <= K; k++) {
        template_norms << MLCompute::norm(M * T, templates.dataPtr(0, 0, k - 1));
    }

    //keep passing through the data until nothing changes anymore
    bool something_changed = true;
    QVector<int> all_to_use; //a vector of 0's and 1's telling which events should be used
    for (long i = 0; i < L; i++)
        all_to_use << 0; //start out using none
    int num_passes = 0;
    //while ((something_changed)&&(num_passes<2)) {

    QVector<double> scores(L);
    Mda dirty(X.N1(), X.N2()); //timepoints/channels for which we need to recompute the score if a clip was centered here
    for (long ii = 0; ii < dirty.totalSize(); ii++) {
        dirty.setValue(1, ii);
    }

    QList<IntList> channel_mask;
    for (int i = 0; i < K; i++) {
        Mda32 template0;
        templates.getChunk(template0, 0, 0, i, M, T, 1);
        channel_mask << get_channel_mask(template0, 8); //use only the 8 channels with highest maxval
    }

    while (something_changed) {
        num_passes++;
        QVector<double> scores_to_try;
        QVector<double> times_to_try;
        QVector<int> labels_to_try;
        QList<long> inds_to_try; //indices of the events to try on this pass
        //QVector<double> template_norms_to_try;
        for (long i = 0; i < L; i++) { //loop through the events
            if (all_to_use[i] == 0) { //if we are not yet using it...
                double t0 = times[i];
                int k0 = labels[i];
                if (k0 > 0) { //make sure we have a positive label (don't know why we wouldn't)
                    long tt = (long)(t0 - Tmid + 0.5); //start time of clip
                    double score0 = 0;
                    IntList chmask = channel_mask[k0 - 1];
                    if (!is_dirty(dirty, t0, chmask)) {
                        // we don't need to recompute the score
                        score0 = scores[i];
                    }
                    else {
                        //we do need to recompute it.
                        if ((tt >= 0) && (tt + T <= X.N2())) { //make sure we are in range
                            //The score will be how much something like the L2-norm is decreased
                            score0 = compute_score(M, T, X.dataPtr(0, tt), templates.dataPtr(0, 0, k0 - 1), chmask);
                        }
                        /*
                        if (score0 < template_norms[k0] * template_norms[k0] * 0.1)
                            score0 = 0; //the norm of the improvement needs to be at least 0.5 times the norm of the template
                            */
                    }

                    //the score needs to be at least as large as neglogprior in order to accept the spike
                    //double neglogprior = 30;
                    double neglogprior = 0;
                    if (score0 > neglogprior) {
                        //we are not committing to using this event yet... see below for next step
                        scores_to_try << score0;
                        times_to_try << t0;
                        labels_to_try << k0;
                        inds_to_try << i;
                    }
                    else {
                        //means we definitely aren't using it (so we will never get here again)
                        all_to_use[i] = -1; //signals not to try again
                    }
                }
            }
        }
        //Look at those events to try and see if we should use them
        QVector<int> to_use = find_events_to_use(times_to_try, scores_to_try, opts);

        //at this point, nothing is dirty
        for (long i = 0; i < dirty.totalSize(); i++) {
            dirty.set(0, i);
        }

        //for all those we are going to "use", we want to subtract out the corresponding templates from the timeseries data
        something_changed = false;
        long num_added = 0;
        for (long i = 0; i < to_use.count(); i++) {
            if (to_use[i] == 1) {
                IntList chmask = channel_mask[labels_to_try[i] - 1];
                something_changed = true;
                num_added++;
                long tt = (long)(times_to_try[i] - Tmid + 0.5);
                subtract_scaled_template(M, T, X.dataPtr(0, tt), templates.dataPtr(0, 0, labels_to_try[i] - 1), chmask);
                for (int aa = tt - T / 2 - 1; aa <= tt + T + T / 2 + 1; aa++) {
                    if ((aa >= 0) && (aa < X.N2())) {
                        for (int k = 0; k < chmask.count(); k++) {
                            dirty.setValue(1, chmask[k], aa);
                        }
                    }
                }
                all_to_use[inds_to_try[i]] = 1;
            }
        }
    }

    QList<long> inds_to_use;
    for (long i = 0; i < L; i++) {
        if (all_to_use[i] == 1)
            inds_to_use << i;
    }

    return inds_to_use;
}
}
