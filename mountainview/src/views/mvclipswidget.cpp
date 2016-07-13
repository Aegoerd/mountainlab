/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 4/4/2016
*******************************************************/

#include "mvclipswidget.h"
#include "mvclipsview.h"
#include "taskprogress.h"
#include <QHBoxLayout>
#include "mountainprocessrunner.h"
#include "mlcommon.h"
#include "mlcommon.h"
#include "mvtimeseriesview2.h"
#include "mvmainwindow.h"
#include <QMessageBox>
#include "actionfactory.h"

/// TODO: (HIGH) merge should apply to all widgets
/// TODO (LOW) handle case where there are too many clips to want to download
/// TODO: (MEDIUM) labels in clips widget

class MVClipsWidgetComputer {
public:
    //input
    DiskReadMda firings;
    DiskReadMda timeseries;
    QString mlproxy_url;
    MVEventFilter filter;
    int clip_size;
    QList<int> labels_to_use;

    //output
    DiskReadMda clips;
    QVector<double> times;
    QVector<int> labels;

    void compute();
};

class MVClipsWidgetPrivate {
public:
    MVClipsWidget* q;
    QList<int> m_labels_to_use;
    //MVClipsView* m_view;
    MVTimeSeriesView2* m_view;
    MVContext m_view_context;
    MVClipsWidgetComputer m_computer;
};

MVClipsWidget::MVClipsWidget(MVContext* context)
    : MVAbstractView(context)
{
    d = new MVClipsWidgetPrivate;
    d->q = this;

    //d->m_view = new MVClipsView(context);
    d->m_view = new MVTimeSeriesView2(&d->m_view_context);
    mvtsv_prefs p = d->m_view->prefs();
    p.show_current_timepoint = false;
    //p.show_time_axis = false;
    p.show_time_axis_ticks = false;
    p.show_time_axis_scale = false;
    p.show_time_axis_scale_only_1ms = true;
    d->m_view->setPrefs(p);

    ActionFactory::addToToolbar(ActionFactory::ActionType::ZoomIn, this, d->m_view, SLOT(slot_zoom_in()));
    ActionFactory::addToToolbar(ActionFactory::ActionType::ZoomOut, this, d->m_view, SLOT(slot_zoom_out()));

    QHBoxLayout* hlayout = new QHBoxLayout;
    hlayout->setMargin(0);
    hlayout->setSpacing(0);
    hlayout->addWidget(d->m_view);
    this->setLayout(hlayout);

    this->recalculateOn(context, SIGNAL(currentTimeseriesChanged()));
    this->recalculateOn(context, SIGNAL(filteredFiringsChanged()));
    this->recalculateOnOptionChanged("clip_size");

    recalculate();
}

MVClipsWidget::~MVClipsWidget()
{
    this->stopCalculation();
    delete d;
}

void MVClipsWidget::prepareCalculation()
{
    d->m_computer.mlproxy_url = mvContext()->mlProxyUrl();
    d->m_computer.filter = mvContext()->eventFilter();
    d->m_computer.firings = mvContext()->firings();
    d->m_computer.timeseries = mvContext()->currentTimeseries();
    d->m_computer.labels_to_use = d->m_labels_to_use;
    d->m_computer.clip_size = mvContext()->option("clip_size").toInt();
}

void MVClipsWidget::runCalculation()
{
    d->m_computer.compute();
}

void MVClipsWidget::onCalculationFinished()
{
    TaskProgress task("MVClipsWidget::onCalculationFinished");
    task.log(QString("%1x%2x%3").arg(d->m_computer.clips.N1()).arg(d->m_computer.clips.N2()).arg(d->m_computer.clips.N3()));
    DiskReadMda clips = d->m_computer.clips.reshaped(d->m_computer.clips.N1(), d->m_computer.clips.N2() * d->m_computer.clips.N3());
    d->m_view_context.copySettingsFrom(mvContext());
    d->m_view_context.addTimeseries("clips", clips);
    d->m_view_context.setCurrentTimeseriesName("clips");
    d->m_view_context.setCurrentTimeRange(MVRange(0, clips.N2() - 1));
    d->m_view->setClipSize(d->m_computer.clips.N2());
    d->m_view->recalculate();
}

void MVClipsWidget::setLabelsToUse(const QList<int>& labels)
{
    d->m_labels_to_use = labels;
    this->recalculate();
}

void MVClipsWidget::paintEvent(QPaintEvent* evt)
{
    QPainter painter(this);
    if (isCalculating()) {
        //show that something is computing
        painter.fillRect(QRectF(0, 0, width(), height()), mvContext()->color("calculation-in-progress"));
    }

    QWidget::paintEvent(evt);
}

void MVClipsWidgetComputer::compute()
{
    TaskProgress task(TaskProgress::Calculate, "Clips Widget Computer");

    /// TODO (LOW) think about how to automatically assemble a pipeline that gets sent to server in one shot

    //need to compute remotely because of subsequent steps
    firings = compute_filtered_firings_remotely(mlproxy_url, firings, filter);
    //firings = compute_filtered_firings_locally(firings, filter);

    QString firings_out_path;
    {
        QString labels_str;
        foreach (int x, labels_to_use) {
            if (!labels_str.isEmpty())
                labels_str += ",";
            labels_str += QString("%1").arg(x);
        }

        MountainProcessRunner MT;
        QString processor_name = "mv_subfirings";
        MT.setProcessorName(processor_name);

        QMap<QString, QVariant> params;
        params["firings"] = firings.makePath();
        params["labels"] = labels_str;
        MT.setInputParameters(params);
        //MT.setMscmdServerUrl(mscmdserver_url);
        MT.setMLProxyUrl(mlproxy_url);

        firings_out_path = MT.makeOutputFilePath("firings_out");

        MT.runProcess();
        if (MLUtil::threadInterruptRequested()) {
            task.error(QString("Halted while running process: " + processor_name));
            return;
        }
    }

    QString clips_path;
    {
        MountainProcessRunner MT;
        QString processor_name = "extract_clips";
        MT.setProcessorName(processor_name);

        QMap<QString, QVariant> params;
        params["timeseries"] = timeseries.makePath();
        params["firings"] = firings_out_path;
        params["clip_size"] = clip_size;
        MT.setInputParameters(params);
        //MT.setMscmdServerUrl(mscmdserver_url);
        MT.setMLProxyUrl(mlproxy_url);

        clips_path = MT.makeOutputFilePath("clips");

        MT.runProcess();
        if (MLUtil::threadInterruptRequested()) {
            task.error(QString("Halted while running process: " + processor_name));
            return;
        }
    }
    task.log("Reading: " + firings_out_path);
    DiskReadMda firings_out(firings_out_path);
    task.log(QString("firings_out: %1 x %2").arg(firings_out.N1()).arg(firings_out.N2()));
    times.clear();
    for (long j = 0; j < firings_out.N2(); j++) {
        times << firings_out.value(1, j);
        labels << firings_out.value(2, j);
    }
    task.log("Reading: " + clips_path);
    DiskReadMda CC(clips_path);
    CC.setRemoteDataType("float32_q8"); //to save download time
    task.log(QString("CC: %1 x %2 x %3, clip_size=%4").arg(CC.N1()).arg(CC.N2()).arg(CC.N3()).arg(clip_size));
    clips = CC;
    /*
    CC.readChunk(clips, 0, 0, 0, CC.N1(), CC.N2(), CC.N3());
    if (MLUtil::threadInterruptRequested()) {
        task.error(QString("Halted while reading chunk from: " + clips_path));
        return;
    }
    */
}

MVClipsFactory::MVClipsFactory(MVContext* context, QObject* parent)
    : MVAbstractViewFactory(context, parent)
{
    connect(context, SIGNAL(selectedClustersChanged()),
        this, SLOT(updateEnabled()));
    updateEnabled();
}

QString MVClipsFactory::id() const
{
    return QStringLiteral("open-clips");
}

QString MVClipsFactory::name() const
{
    return tr("Clips");
}

QString MVClipsFactory::title() const
{
    return tr("Clips");
}

MVAbstractView* MVClipsFactory::createView(QWidget* parent)
{
    Q_UNUSED(parent)
    QList<int> ks = mvContext()->selectedClusters();
    qSort(ks);
    if (ks.count() == 0) {
        QMessageBox::information(0, "Unable to open clips", "You must select at least one cluster.");
        return Q_NULLPTR;
    }
    MVClipsWidget* X = new MVClipsWidget(mvContext());
    X->setProperty("widget_type", "clips");
    X->setLabelsToUse(ks);
    /// TODO, pass this in a method
    //X->setProperty("ks", int_list_to_string_list(ks));
    /// TODO (LOW) more descriptive tab title in case of more than one

    //    QString tab_title = "Clips";
    //    if (ks.count() == 1) {
    //        int kk = ks[0];
    //        tab_title = QString("Clips %1").arg(kk);
    //    }
    return X;
}

int MVClipsFactory::order() const
{
    return 0;
}

void MVClipsFactory::updateEnabled()
{
    setEnabled(!mvContext()->selectedClusters().isEmpty());
}
