/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 5/23/2016
*******************************************************/

#include "mvviewagent.h"
#include <QDebug>

struct TimeseriesStruct {
    QString name;
    DiskReadMda data;
};

class MVViewAgentPrivate {
public:
    MVViewAgent* q;
    ClusterMerge m_cluster_merge;
    QMap<int, QJsonObject> m_cluster_attributes;
    MVEvent m_current_event;
    int m_current_cluster;
    QList<int> m_selected_clusters;
    double m_current_timepoint;
    MVRange m_current_time_range;
    QList<QColor> m_cluster_colors;
    QList<QColor> m_channel_colors;
    QMap<QString, TimeseriesStruct> m_timeseries;
    QString m_current_timeseries_name;
    DiskReadMda m_firings;
    double m_sample_rate;
    QMap<QString, QVariant> m_options;
};

MVViewAgent::MVViewAgent()
{
    d = new MVViewAgentPrivate;
    d->q = this;
    d->m_current_cluster = 0;
    d->m_current_timepoint = 0;
}

MVViewAgent::~MVViewAgent()
{
    delete d;
}

QMap<int, QJsonObject> MVViewAgent::clusterAttributes() const
{
    return d->m_cluster_attributes;
}

MVEvent MVViewAgent::currentEvent() const
{
    return d->m_current_event;
}

int MVViewAgent::currentCluster() const
{
    return d->m_current_cluster;
}

QList<int> MVViewAgent::selectedClusters() const
{
    return d->m_selected_clusters;
}

double MVViewAgent::currentTimepoint() const
{
    return d->m_current_timepoint;
}

MVRange MVViewAgent::currentTimeRange() const
{
    return d->m_current_time_range;
}

QList<QColor> MVViewAgent::channelColors() const
{
    return d->m_channel_colors;
}

QList<QColor> MVViewAgent::clusterColors() const
{
    return d->m_cluster_colors;
}

DiskReadMda MVViewAgent::currentTimeseries()
{
    return d->m_timeseries.value(d->m_current_timeseries_name).data;
}

QString MVViewAgent::currentTimeseriesName()
{
    return d->m_current_timeseries_name;
}

QColor MVViewAgent::clusterColor(int k) const
{
    if (k <= 0)
        return Qt::black;
    if (d->m_cluster_colors.isEmpty())
        return Qt::black;
    return d->m_cluster_colors[(k - 1) % d->m_cluster_colors.count()];
}

QColor MVViewAgent::channelColor(int m) const
{
    if (m < 0)
        return Qt::black;
    if (d->m_channel_colors.isEmpty())
        return Qt::black;
    return d->m_channel_colors[m % d->m_channel_colors.count()];
}

QStringList MVViewAgent::timeseriesNames() const
{
    return d->m_timeseries.keys();
}

void MVViewAgent::addTimeseries(QString name, DiskReadMda timeseries)
{
    TimeseriesStruct X;
    X.data = timeseries;
    X.name = name;
    d->m_timeseries[name] = X;
    emit this->timeseriesNamesChanged();
}

DiskReadMda MVViewAgent::firings()
{
    return d->m_firings;
}

double MVViewAgent::sampleRate() const
{
    return d->m_sample_rate;
}

QVariant MVViewAgent::option(QString name, QVariant default_val)
{
    return d->m_options.value(name, default_val);
}

void MVViewAgent::setCurrentTimeseriesName(QString name)
{
    if (d->m_current_timeseries_name == name)
        return;
    d->m_current_timeseries_name = name;
    emit this->currentTimeseriesChanged();
}

void MVViewAgent::setFirings(const DiskReadMda& F)
{
    d->m_firings = F;
    emit firingsChanged();
}

void MVViewAgent::setSampleRate(double sample_rate)
{
    d->m_sample_rate = sample_rate;
}

ClusterMerge MVViewAgent::clusterMerge() const
{
    return d->m_cluster_merge;
}

void MVViewAgent::setClusterMerge(const ClusterMerge& CM)
{
    if (d->m_cluster_merge == CM)
        return;
    d->m_cluster_merge = CM;
    emit this->clusterMergeChanged();
}

void MVViewAgent::setClusterAttributes(const QMap<int, QJsonObject>& A)
{
    if (d->m_cluster_attributes == A)
        return;
    d->m_cluster_attributes = A;
    emit this->clusterAttributesChanged();
}

void MVViewAgent::setCurrentEvent(const MVEvent& evt)
{
    if (evt == d->m_current_event)
        return;
    d->m_current_event = evt;
    emit currentEventChanged();
    this->setCurrentTimepoint(evt.time);
}

void MVViewAgent::setCurrentCluster(int k)
{
    if (k == d->m_current_cluster)
        return;
    d->m_current_cluster = k;

    QList<int> tmp = selectedClusters();
    if (!tmp.contains(k)) {
        tmp << k;
        this->setSelectedClusters(tmp); //think about this
    }
    emit currentClusterChanged();
}

void MVViewAgent::setSelectedClusters(const QList<int>& ks)
{
    QList<int> ks2 = QList<int>::fromSet(ks.toSet()); //remove duplicates and -1
    ks2.removeAll(-1);
    qSort(ks2);
    if (d->m_selected_clusters == ks2)
        return;
    d->m_selected_clusters = ks2;
    if (!d->m_selected_clusters.contains(d->m_current_cluster)) {
        this->setCurrentCluster(-1);
    }
    emit selectedClustersChanged();
}

void MVViewAgent::setCurrentTimepoint(double tp)
{
    if (d->m_current_timepoint == tp)
        return;
    d->m_current_timepoint = tp;
    emit currentTimepointChanged();
}

void MVViewAgent::setCurrentTimeRange(const MVRange& range)
{
    if (d->m_current_time_range == range)
        return;
    d->m_current_time_range = range;
    emit currentTimeRangeChanged();
}

void MVViewAgent::setClusterColors(const QList<QColor>& colors)
{
    d->m_cluster_colors = colors;
}

void MVViewAgent::setChannelColors(const QList<QColor>& colors)
{
    d->m_channel_colors = colors;
}

void MVViewAgent::setOption(QString name, QVariant value)
{
    if (d->m_options[name] == value)
        return;
    d->m_options[name] = value;
    emit optionChanged(name);
}

void MVViewAgent::clickCluster(int k, Qt::KeyboardModifiers modifiers)
{
    /// TODO handle shift modifier
    if (modifiers & Qt::ControlModifier) {
        if (k < 0)
            return;
        if (d->m_selected_clusters.contains(k)) {
            QList<int> tmp = d->m_selected_clusters;
            tmp.removeAll(k);
            this->setSelectedClusters(tmp);
        }
        else {
            if (k >= 0) {
                QList<int> tmp = d->m_selected_clusters;
                tmp << k;
                this->setSelectedClusters(tmp);
            }
        }
    }
    else {
        this->setSelectedClusters(QList<int>());
        this->setCurrentCluster(k);
    }
}

bool MVRange::operator==(const MVRange& other)
{
    return ((other.min == min) && (other.max == max));
}

MVRange MVRange::operator+(double offset)
{
    return MVRange(min + offset, max + offset);
}

MVRange MVRange::operator*(double scale)
{
    double center = (min + max) / 2;
    double span = (max - min);
    return MVRange(center - span / 2 * scale, center + span / 2 * scale);
}
