/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 6/6/2016
*******************************************************/

#include "mvtimeseriesview.h"
#include "spikespywidget.h"
#include <QHBoxLayout>
#include <QSplitter>

class SpikeSpyWidgetPrivate {
public:
    SpikeSpyWidget* q;
    double m_samplerate;
    QList<MVTimeSeriesView*> m_views;

    QSplitter* m_splitter;
};

SpikeSpyWidget::SpikeSpyWidget()
{
    d = new SpikeSpyWidgetPrivate;
    d->q = this;
    d->m_samplerate = 0;

    d->m_splitter = new QSplitter;

    QHBoxLayout* layout = new QHBoxLayout;
    layout->addWidget(d->m_splitter);
    setLayout(layout);
}

SpikeSpyWidget::~SpikeSpyWidget()
{
    delete d;
}

void SpikeSpyWidget::setSampleRate(double samplerate)
{
    d->m_samplerate = samplerate;
    foreach (MVTimeSeriesView* V, d->m_views) {
        V->setSampleRate(samplerate);
    }
}

void SpikeSpyWidget::addView(const SpikeSpyViewData& data)
{
    MVTimeSeriesView* W = new MVTimeSeriesView;
    qDebug() << "DEBUG" << __FUNCTION__ << __FILE__ << __LINE__;
    qDebug() << "@@@@@@@@@@@@@@@@@@@" << data.timeseries.N1() << data.timeseries.N2() << data.firings.N1() << data.firings.N2();
    W->setData(data.timeseries);
    QVector<double> times;
    QVector<int> labels;
    for (long i = 0; i < data.firings.N2(); i++) {
        times << data.firings.value(1, i);
        labels << (int)data.firings.value(2, i);
    }
    W->setTimesLabels(times, labels);
    W->setSampleRate(d->m_samplerate);
    d->m_splitter->addWidget(W);
    d->m_views << W;
}
