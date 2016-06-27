/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 3/31/2016
*******************************************************/

#include "mvcrosscorrelogramswidget3.h"
#include "histogramview.h"
#include "mvutils.h"
#include "taskprogress.h"
#include "mvmainwindow.h" //to disappear
#include "tabber.h" //to disappear

#include <QAction>
#include <QGridLayout>
#include <QKeyEvent>
#include <QList>
#include <QPainter>
#include <math.h>
#include "msmisc.h"
#include "mvmisc.h"

/// TODO: (MEDIUM) make abstract histogram view that encompasses both cross-correlograms and amplitude histograms

struct Correlogram3 {
    int k1 = 0, k2 = 0;
    QList<double> data;
};

class MVCrossCorrelogramsWidget3Computer {
public:
    //input
    QString mlproxy_url;
    DiskReadMda firings;
    MVEventFilter event_filter;
    CrossCorrelogramOptions3 options;
    int max_dt;
    ClusterMerge cluster_merge;

    //output
    QList<Correlogram3> correlograms;

    void compute();
};

class MVCrossCorrelogramsWidget3Private {
public:
    MVCrossCorrelogramsWidget3* q;
    MVCrossCorrelogramsWidget3Computer m_computer;
    QList<Correlogram3> m_correlograms;

    QGridLayout* m_grid_layout;

    CrossCorrelogramOptions3 m_options;
};

MVCrossCorrelogramsWidget3::MVCrossCorrelogramsWidget3(MVViewAgent* view_agent)
    : MVHistogramGrid(view_agent)
{
    d = new MVCrossCorrelogramsWidget3Private;
    d->q = this;

    this->recalculateOn(view_agent, SIGNAL(filteredFiringsChanged()));
    this->recalculateOn(view_agent, SIGNAL(clusterMergeChanged()), false);
    this->recalculateOn(view_agent, SIGNAL(clusterVisibilityChanged()), false);
    this->recalculateOnOptionChanged("cc_max_dt_msec");

    this->recalculate();
}

MVCrossCorrelogramsWidget3::~MVCrossCorrelogramsWidget3()
{
    delete d;
}

void MVCrossCorrelogramsWidget3::prepareCalculation()
{
    d->m_computer.mlproxy_url = viewAgent()->mlProxyUrl();
    d->m_computer.firings = viewAgent()->firings();
    d->m_computer.event_filter = viewAgent()->eventFilter();
    d->m_computer.options = d->m_options;
    d->m_computer.max_dt = viewAgent()->option("cc_max_dt_msec", 100).toDouble() / 1000 * viewAgent()->sampleRate();
    d->m_computer.cluster_merge = viewAgent()->clusterMerge();
}

void MVCrossCorrelogramsWidget3::runCalculation()
{
    d->m_computer.compute();
}

double compute_max2(const QList<Correlogram3>& data0)
{
    double ret = 0;
    for (int i = 0; i < data0.count(); i++) {
        QList<double> tmp = data0[i].data;
        for (int j = 0; j < tmp.count(); j++) {
            if (tmp[j] > ret)
                ret = tmp[j];
        }
    }
    return ret;
}

void MVCrossCorrelogramsWidget3::onCalculationFinished()
{
    d->m_correlograms = d->m_computer.correlograms;

    double bin_max = compute_max2(d->m_correlograms);
    double bin_min = -bin_max;
    //int num_bins=100;
    int bin_size = 20;
    int num_bins = (bin_max - bin_min) / bin_size;
    if (num_bins < 100)
        num_bins = 100;
    if (num_bins > 2000)
        num_bins = 2000;
    double sample_freq = viewAgent()->sampleRate();
    double time_width = (bin_max - bin_min) / sample_freq * 1000;
    HorizontalScaleAxisData X;
    X.use_it = true;
    X.label = QString("%1 ms").arg((int)(time_width / 2));
    this->setHorizontalScaleAxis(X);

    QList<HistogramView*> histogram_views;
    for (int ii = 0; ii < d->m_correlograms.count(); ii++) {
        int k1 = d->m_correlograms[ii].k1;
        int k2 = d->m_correlograms[ii].k2;
        if ((viewAgent()->clusterIsVisible(k1)) && (viewAgent()->clusterIsVisible(k2))) {
            HistogramView* HV = new HistogramView;
            HV->setData(d->m_correlograms[ii].data);
            HV->setColors(viewAgent()->colors());
            HV->setBins(bin_min, bin_max, num_bins);
            QString title0 = QString("%1/%2").arg(d->m_correlograms[ii].k1).arg(d->m_correlograms[ii].k2);
            HV->setTitle(title0);
            HV->setProperty("k1", d->m_correlograms[ii].k1);
            HV->setProperty("k2", d->m_correlograms[ii].k2);
            HV->setProperty("k", d->m_correlograms[ii].k2);
            histogram_views << HV;
        }
    }
    this->setHistogramViews(histogram_views);
}

void MVCrossCorrelogramsWidget3::setOptions(CrossCorrelogramOptions3 opts)
{
    d->m_options = opts;
    this->recalculate();
}

QList<double> compute_cc_data3(const QList<double>& times1_in, const QList<double>& times2_in, int max_dt, bool exclude_matches)
{
    QList<double> ret;
    QList<double> times1 = times1_in;
    QList<double> times2 = times2_in;
    qSort(times1);
    qSort(times2);

    if ((times1.isEmpty()) || (times2.isEmpty()))
        return ret;

    long i1 = 0;
    for (long i2 = 0; i2 < times2.count(); i2++) {
        while ((i1 + 1 < times1.count()) && (times1[i1] < times2[i2] - max_dt))
            i1++;
        long j1 = i1;
        while ((j1 < times1.count()) && (times1[j1] <= times2[i2] + max_dt)) {
            bool ok = true;
            if ((exclude_matches) && (j1 == i2) && (times1[j1] == times2[i2]))
                ok = false;
            if (ok) {
                ret << times1[j1] - times2[i2];
            }
            j1++;
        }
    }
    return ret;
}

typedef QList<double> DoubleList;
typedef QList<int> IntList;
void MVCrossCorrelogramsWidget3Computer::compute()
{
    TaskProgress task(TaskProgress::Calculate, QString("Cross Correlograms (%1)").arg(options.mode));

    correlograms.clear();

    firings = compute_filtered_firings_locally(firings, event_filter);

    QList<double> times;
    QList<int> labels;
    long L = firings.N2();

    //assemble the times and labels arrays
    task.setProgress(0.2);
    for (long n = 0; n < L; n++) {
        times << firings.value(1, n);
        labels << (int)firings.value(2, n);
    }

    //compute K (the maximum label)
    int K = compute_max(labels);

    //handle the merge
    QMap<int, int> label_map = cluster_merge.labelMap(K);
    for (long n = 0; n < L; n++) {
        labels[n] = label_map[labels[n]];
    }

    //Assemble the correlogram objects depending on mode
    if (options.mode == All_Auto_Correlograms3) {
        for (int k = 1; k <= K; k++) {
            Correlogram3 CC;
            CC.k1 = k;
            CC.k2 = k;
            this->correlograms << CC;
        }
    }
    else if (options.mode == Cross_Correlograms3) {
        int k0 = options.ks.value(0);
        for (int k = 1; k <= K; k++) {
            Correlogram3 CC;
            CC.k1 = k0;
            CC.k2 = k;
            this->correlograms << CC;
        }
    }
    else if (options.mode == Matrix_Of_Cross_Correlograms3) {
        for (int i = 0; i < options.ks.count(); i++) {
            for (int j = 0; j < options.ks.count(); j++) {
                Correlogram3 CC;
                CC.k1 = options.ks[i];
                CC.k2 = options.ks[j];
                this->correlograms << CC;
            }
        }
    }

    //assemble the times organized by k
    QList<DoubleList> the_times;
    for (int k = 0; k <= K; k++) {
        the_times << DoubleList();
    }
    for (long ii = 0; ii < labels.count(); ii++) {
        int k = labels[ii];
        if (k <= the_times.count()) {
            the_times[k] << times[ii];
        }
    }

    //compute the cross-correlograms
    task.setProgress(0.7);
    for (int j = 0; j < correlograms.count(); j++) {
        if (thread_interrupt_requested()) {
            return;
        }
        int k1 = correlograms[j].k1;
        int k2 = correlograms[j].k2;
        correlograms[j].data = compute_cc_data3(the_times.value(k1), the_times.value(k2), max_dt, (k1 == k2));
    }
}

MVAutoCorrelogramsFactory::MVAutoCorrelogramsFactory(MVViewAgent* context, QObject* parent)
    : MVAbstractViewFactory(context, parent)
{
}

QString MVAutoCorrelogramsFactory::id() const
{
    return QStringLiteral("open-auto-correlograms");
}

QString MVAutoCorrelogramsFactory::name() const
{
    return tr("Auto-Correlograms");
}

QString MVAutoCorrelogramsFactory::title() const
{
    return tr("Auto-Correlograms");
}

MVAbstractView* MVAutoCorrelogramsFactory::createView(MVViewAgent* agent, QWidget* parent)
{
    MVCrossCorrelogramsWidget3* X = new MVCrossCorrelogramsWidget3(agent);
    CrossCorrelogramOptions3 opts;
    opts.mode = All_Auto_Correlograms3;
    X->setOptions(opts);
    //QObject::connect(X, SIGNAL(histogramActivated()), this, SLOT(slot_auto_correlogram_activated()));
    return X;
}

/*
void MVAutoCorrelogramsFactory::slot_auto_correlogram_activated()
{
    MVAbstractView* view = qobject_cast<MVAbstractView*>(sender());
    if (!view)
        return;
    MVMainWindow* mw = MVMainWindow::instance();
    int k = mw->viewAgent()->currentCluster();
    TabberTabWidget* TW = mw->tabWidget(view);
    mw->tabber()->setCurrentContainer(TW);
    mw->tabber()->switchCurrentContainer();
    /// TODO: d->open_cross_correlograms(k);
    /// mw->openView("cross-correlograms", k);
}
*/

MVCrossCorrelogramsFactory::MVCrossCorrelogramsFactory(MVViewAgent* context, QObject* parent)
    : MVAbstractViewFactory(context, parent)
{
    connect(MVMainWindow::instance()->viewAgent(), SIGNAL(selectedClustersChanged()),
        this, SLOT(updateEnabled()));
    updateEnabled();
}

QString MVCrossCorrelogramsFactory::id() const
{
    return QStringLiteral("open-cross-correlograms");
}

QString MVCrossCorrelogramsFactory::name() const
{
    return tr("Cross-Correlograms");
}

QString MVCrossCorrelogramsFactory::title() const
{
    return tr("Cross-Correlograms");
}

MVAbstractView* MVCrossCorrelogramsFactory::createView(MVViewAgent* agent, QWidget* parent)
{
    MVCrossCorrelogramsWidget3* X = new MVCrossCorrelogramsWidget3(agent);
    QList<int> ks = agent->selectedClusters();
    if (ks.count() != 1)
        return X;

    CrossCorrelogramOptions3 opts;
    opts.mode = Cross_Correlograms3;
    opts.ks = ks;

    X->setOptions(opts);
    return X;
}

void MVCrossCorrelogramsFactory::updateEnabled()
{
    setEnabled(MVMainWindow::instance()->viewAgent()->selectedClusters().count() == 1);
}

MVMatrixOfCrossCorrelogramsFactory::MVMatrixOfCrossCorrelogramsFactory(MVViewAgent* context, QObject* parent)
    : MVAbstractViewFactory(context, parent)
{
    connect(MVMainWindow::instance()->viewAgent(), SIGNAL(selectedClustersChanged()),
        this, SLOT(updateEnabled()));
    updateEnabled();
}

QString MVMatrixOfCrossCorrelogramsFactory::id() const
{
    return QStringLiteral("open-matrix-of-cross-correlograms");
}

QString MVMatrixOfCrossCorrelogramsFactory::name() const
{
    return tr("Matrix of Cross-Correlograms");
}

QString MVMatrixOfCrossCorrelogramsFactory::title() const
{
    return tr("CC Matrix");
}

MVAbstractView* MVMatrixOfCrossCorrelogramsFactory::createView(MVViewAgent* agent, QWidget* parent)
{
    MVCrossCorrelogramsWidget3* X = new MVCrossCorrelogramsWidget3(agent);
    QList<int> ks = agent->selectedClusters();
    qSort(ks);
    if (ks.isEmpty())
        return X;
    CrossCorrelogramOptions3 opts;
    opts.mode = Matrix_Of_Cross_Correlograms3;
    opts.ks = ks;
    X->setOptions(opts);
    return X;
}

void MVMatrixOfCrossCorrelogramsFactory::updateEnabled()
{
    setEnabled(!MVMainWindow::instance()->viewAgent()->selectedClusters().isEmpty());
}
