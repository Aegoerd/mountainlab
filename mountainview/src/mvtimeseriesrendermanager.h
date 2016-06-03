/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 6/1/2016
*******************************************************/

#ifndef MVTIMESERIESRENDERMANAGER_H
#define MVTIMESERIESRENDERMANAGER_H

#include "multiscaletimeseries.h"
#include "mlutils.h"

#include <QColor>
#include <QImage>
#include <QRunnable>
#include <QThread>

class MVTimeSeriesRenderManagerPrivate;
class MVTimeSeriesRenderManager : public QObject {
    Q_OBJECT
public:
    friend class MVTimeSeriesRenderManagerPrivate;
    MVTimeSeriesRenderManager();
    virtual ~MVTimeSeriesRenderManager();

    void setMultiScaleTimeSeries(MultiScaleTimeSeries* ts);
    void setChannelColors(const QList<QColor>& colors);

    QImage getImage(double t1, double t2, double amp_factor, double W, double H);

signals:
    void updated();

private
slots:
    void slot_thread_finished();

private:
    MVTimeSeriesRenderManagerPrivate* d;
};

class MVTimeSeriesRenderManagerThread : public QThread {
    Q_OBJECT
public:
    //input
    double amp_factor;
    long ds_factor;
    long index;
    QList<QColor> channel_colors;
    MultiScaleTimeSeries* ts;
    QColor get_channel_color(long m);

    //output
    QImage image;

    void run();
};

class ThreadManager : public QObject {
    Q_OBJECT
public:
    ThreadManager();
    void start(QString id, QThread* thread);
    void stop(QString id);
private
slots:
    void slot_timer();
    void slot_thread_finished();

private:
    QMap<QString, QThread*> m_queued_threads;
    QMap<QString, QThread*> m_running_threads;
};

#endif // MVTIMESERIESRENDERMANAGER_H
