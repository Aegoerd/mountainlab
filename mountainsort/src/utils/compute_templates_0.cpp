/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 3/29/2016
*******************************************************/

#include "compute_templates_0.h"
#include "mlcommon.h"
#include "mlcommon.h"
#include <math.h>

Mda32 compute_templates_0(DiskReadMda32& X, Mda64& firings, int clip_size)
{
    QVector<double> times;
    QVector<int> labels;
    long L = firings.N2();
    for (long i = 0; i < L; i++) {
        times << firings.value(1, i);
        labels << (int)firings.value(2, i);
    }
    return compute_templates_0(X, times, labels, clip_size);
}

Mda32 compute_templates_0(DiskReadMda32& X, const QVector<double>& times, const QVector<int>& labels, int clip_size)
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
            float* Xptr = X0.dataPtr();
            float* Tptr = templates.dataPtr(0, 0, k - 1);
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

void compute_templates_stdevs(Mda32& templates, Mda32& stdevs, DiskReadMda32& X, const QVector<double>& times, const QVector<int>& labels, int clip_size)
{
    int M = X.N1();
    int T = clip_size;
    long L = times.count();

    int K = MLCompute::max<int>(labels);

    int Tmid = (int)((T + 1) / 2) - 1;

    Mda64 sums(M, T, K);
    Mda64 sumsqrs(M, T, K);
    QList<long> counts;
    for (int k = 0; k < K; k++)
        counts << k;
    for (long i = 0; i < L; i++) {
        int k = labels[i];
        long t0 = (long)(times[i] + 0.5);
        if (k >= 1) {
            Mda32 X0;
            X.readChunk(X0, 0, t0 - Tmid, M, T);
            float* Xptr = X0.dataPtr();
            double* sum_ptr = sums.dataPtr(0, 0, k - 1);
            double* sumsqr_ptr = sumsqrs.dataPtr(0, 0, k - 1);
            for (int i = 0; i < M * T; i++) {
                sum_ptr[i] += Xptr[i];
                sumsqr_ptr[i] += Xptr[i] * Xptr[i];
            }
            counts[k - 1]++;
        }
    }

    templates.allocate(M, T, K);
    stdevs.allocate(M, T, K);
    for (int k = 0; k < K; k++) {
        for (int t = 0; t < T; t++) {
            for (int m = 0; m < M; m++) {
                if (counts[k] >= 2) {
                    double sum0 = sums.get(m, t, k);
                    double sumsqr0 = sumsqrs.get(m, t, k);
                    templates.set(sum0 / counts[k], m, t, k);
                    stdevs.set(sqrt(sumsqr0 / counts[k] - (sum0 * sum0) / (counts[k] * counts[k])), m, t, k);
                }
            }
        }
    }
}
