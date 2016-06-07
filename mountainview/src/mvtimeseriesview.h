/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 5/27/2016
*******************************************************/

#ifndef MVTIMESERIESVIEW_H
#define MVTIMESERIESVIEW_H

#include <QWidget>
#include <diskreadmda.h>
#include "mvviewagent.h"

/// TODO on first load, multiscale file is created on server, the process is detached. Provide feedback to the user somehow

/// Witold is there a Qt struct that captures this?
struct MVRange {
    MVRange(double min0 = 0, double max0 = 1)
    {
        min = min0;
        max = max0;
    }
    bool operator==(const MVRange& other);
    MVRange operator+(double offset);
    MVRange operator*(double scale);
    double min, max;
};

class MVTimeSeriesViewPrivate;
class MVTimeSeriesView : public QWidget {
    Q_OBJECT
public:
    friend class MVTimeSeriesViewPrivate;
    MVTimeSeriesView(MVViewAgent* view_agent);
    virtual ~MVTimeSeriesView();

    void setSampleRate(double samplerate);
    void setTimeseries(const DiskReadMda& X);
    void setMLProxyUrl(const QString& url);
    void setTimesLabels(const QVector<double>& times, const QVector<int>& labels);
    void setChannelColors(const QList<QColor>& colors);

    void setTimeRange(MVRange);
    void setCurrentTimepoint(double t);
    void setSelectedTimeRange(MVRange range);
    void setAmplitudeFactor(double factor); // display range will be between -1/factor and 1/factor, but not clipped (thus channel plots may overlap)
    void autoSetAmplitudeFactor();
    void autoSetAmplitudeFactorWithinTimeRange();

    double currentTimepoint() const;
    MVRange timeRange() const;
    double amplitudeFactor() const;
    DiskReadMda timeseries();

    void resizeEvent(QResizeEvent* evt);
    void paintEvent(QPaintEvent* evt);
    void mousePressEvent(QMouseEvent* evt);
    void mouseReleaseEvent(QMouseEvent* evt);
    void mouseMoveEvent(QMouseEvent* evt);
    void wheelEvent(QWheelEvent* evt);
    void keyPressEvent(QKeyEvent* evt);

    static void unit_test();

private:
    MVTimeSeriesViewPrivate* d;
};

#endif // MVTIMESERIESVIEW_H
