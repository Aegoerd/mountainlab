#include <QApplication>
#include <QDebug>
#include <qdatetime.h>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QStringList>
#include "textfile.h"
#include "usagetracking.h"
#include "mda.h"
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDir>
#include <QImageWriter>
#include "commandlineparams.h"
#include "diskarraymodel_new.h"
#include "histogramview.h"
#include "mvlabelcomparewidget.h"
#include "mvoverview2widget.h"
#include "sstimeserieswidget.h"
#include "sstimeseriesview.h"
#include "mvclusterwidget.h"
#include "closemehandler.h"
#include "remotereadmda.h"
#include "taskprogress.h"
#include "mvtimeseriesview.h" //for unit test

#include <QRunnable>
#include <QThreadPool>
#include <QtConcurrentRun>

/// TODO (LOW) option to turn on/off 8-bit quantization per view
/// TODO (MED) update docs
/// TODO (MED) event filter to be computed on client
/// TODO (MED) blobs for populations
/// TODO (LOW) Resort by populations, ampl, etc
/// TODO (LOW) time scale bar for clip view
/// TODO (LOW) electrode view... (firetrack)
/// TODO (LOW) 3D feature plot scale density -- log scale?
/// TODO (0.9.1) Go to timepoint

class TaskProgressViewThread : public QRunnable {
public:
    TaskProgressViewThread(int idx)
        : QRunnable()
        , m_idx(idx)
    {
        setAutoDelete(true);
    }
    void run()
    {
        qsrand(QDateTime::currentDateTime().currentMSecsSinceEpoch());
        QThread::msleep(qrand() % 1000);
        TaskProgress TP1(QString("Test task %1").arg(m_idx));
        if (m_idx % 3 == 0)
            TP1.addTag(TaskProgress::Download);
        else if (m_idx % 3 == 1)
            TP1.addTag(TaskProgress::Calculate);
        TP1.setDescription("The description of the task. This should complete on destruct.");
        for (int i = 0; i <= 100; ++i) {
            TP1.setProgress(i * 1.0 / 100.0);
            TP1.setLabel(QString("Test task %1 (%2)").arg(m_idx).arg(i * 1.0 / 100.0));
            TP1.log(QString("Log #%1").arg(i + 1));
            int rand = 1 + (qrand() % 10);
            QThread::msleep(100 * rand);
        }
    }

private:
    int m_idx;
};

void test_taskprogressview()
{
    int num_jobs = 30; //can increase to test a large number of jobs
    qsrand(QDateTime::currentDateTime().currentMSecsSinceEpoch());
    for (int i = 0; i < num_jobs; ++i) {
        QThreadPool::globalInstance()->releaseThread();
        QThreadPool::globalInstance()->start(new TaskProgressViewThread(i + 1));
        QThread::msleep(qrand() % 10);
    }
    QApplication::instance()->exec();
}
////////////////////////////////////////////////////////////////////////////////

//void run_export_instructions(MVOverview2Widget* W, const QStringList& instructions);

/// TODO (LOW) provide mountainview usage information
/// TODO (LOW) auto correlograms for selected clusters
/// TODO (LOW) figure out what to do when #channels and/or #clusters is huge
/// TODO (0.9.1) make sure to handle merging with other views, such as clips etc. Make elegant way

QColor brighten(QColor col, int amount);
QList<QColor> generate_colors(const QColor& bg, const QColor& fg, int noColors);
QList<QColor> generate_colors_old(const QColor& bg, const QColor& fg, int noColors);

#include "multiscaletimeseries.h"
#include "spikespywidget.h"
#include "taskprogressview.h"
int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    // make sure task progress monitor is instantiated in the main thread
    TaskManager::TaskProgressMonitor* monitor = TaskManager::TaskProgressMonitor::globalInstance();
    Q_UNUSED(monitor);
    CloseMeHandler::start();

    setbuf(stdout, 0);

    /// Witold I don't want to do this here! It should be in the taskprogress.h. What can I do?
    qRegisterMetaType<TaskInfo>();

    CLParams CLP = commandlineparams(argc, argv);

    QString mv_fname;
    if (CLP.unnamed_parameters.value(0).endsWith(".mv")) {
        mv_fname = CLP.unnamed_parameters.value(0);
    }

    QList<QColor> channel_colors;
    QStringList color_strings;
    color_strings
        << "#282828"
        << "#402020"
        << "#204020"
        << "#202070";
    for (int i = 0; i < color_strings.count(); i++)
        channel_colors << QColor(brighten(color_strings[i], 80));

    int num1 = 7;
    int num2 = 32;
    QList<QColor> colors00 = generate_colors(Qt::gray, Qt::white, num2);
    QList<QColor> label_colors;
    for (int j = 0; j < colors00.count(); j++) {
        //label_colors << brighten(colors00.value((j * num1) % num2),2);
        label_colors << colors00.value((j * num1) % num2);
    }
    printf("-------------------------------\n");
    for (int j=0; j<label_colors.count(); j++) {
        QColor col=label_colors[j];
        double r=col.red()*1.0/255;
        double g=col.green()*1.0/255;
        double b=col.blue()*1.0/255;
        printf("%.3f %.3f %.3f\n",r,g,b);
    }
    printf("-------------------------------\n");

    if (CLP.unnamed_parameters.value(0) == "unit_test") {
        QString arg2 = CLP.unnamed_parameters.value(1);
        if (arg2 == "remotereadmda") {
            unit_test_remote_read_mda();
            return 0;
        } else if (arg2 == "remotereadmda2") {
            QString arg3 = CLP.unnamed_parameters.value(2, "http://localhost:8000/firings.mda");
            unit_test_remote_read_mda_2(arg3);
            return 0;
        } else if (arg2 == "taskprogressview") {
            MVOverview2Widget* W = new MVOverview2Widget(new MVViewAgent); //not that the view agent does not get deleted. :(
            W->show();
            W->move(QApplication::desktop()->screen()->rect().topLeft() + QPoint(200, 200));
            int W0 = 1400, H0 = 1000;
            QRect geom = QApplication::desktop()->geometry();
            if ((geom.width() - 100 < W0) || (geom.height() - 100 < H0)) {
                //W->showMaximized();
                W->resize(geom.width() - 100, geom.height() - 100);
            } else {
                W->resize(W0, H0);
            }
            test_taskprogressview();
            qWarning() << "No such unit test: " + arg2;
            return 0;
        } else if (arg2 == "multiscaletimeseries") {
            MultiScaleTimeSeries::unit_test(3, 10);
            return 0;
        } else if (arg2 == "mvtimeseriesview") {
            MVTimeSeriesView::unit_test();
        }
    }

    QString mode = CLP.named_parameters.value("mode", "overview2").toString();
    if (mode == "overview2") {
        printf("overview2...\n");
        QString raw_path = CLP.named_parameters["raw"].toString();
        QString pre_path = CLP.named_parameters["pre"].toString();
        QString filt_path = CLP.named_parameters["filt"].toString();
        QString firings_path = CLP.named_parameters["firings"].toString();
        double samplerate = CLP.named_parameters.value("samplerate", 20000).toDouble();
        QString epochs_path = CLP.named_parameters["epochs"].toString();
        QString window_title = CLP.named_parameters["window_title"].toString();
        MVOverview2Widget* W = new MVOverview2Widget(new MVViewAgent); //not that the view agent does not get deleted. :(
        W->setChannelColors(channel_colors);
        W->setLabelColors(label_colors);
        W->setMLProxyUrl(CLP.named_parameters.value("mlproxy_url", "").toString());
        {
            W->setWindowTitle(window_title);
            W->show();
            //W->move(QApplication::desktop()->screen()->rect().topLeft() + QPoint(200, 200));

            int W0 = 1400, H0 = 1000;
            QRect geom = QApplication::desktop()->geometry();
            if ((geom.width() - 100 < W0) || (geom.height() - 100 < H0)) {
                //W->showMaximized();
                W->resize(geom.width() - 100, geom.height() - 100);
            } else {
                W->resize(W0, H0);
            }

            W->move(QApplication::desktop()->screen()->rect().bottomRight() - QPoint(W0, H0));

            qApp->processEvents();
        }

        if (!mv_fname.isEmpty()) {
            W->loadMVFile(mv_fname);
        }

        if (!pre_path.isEmpty()) {
            W->addTimeseriesPath("Preprocessed Data", pre_path);
        }
        if (!filt_path.isEmpty()) {
            W->addTimeseriesPath("Filtered Data", filt_path);
        }
        if (!raw_path.isEmpty()) {
            W->addTimeseriesPath("Raw Data", raw_path);
        }

        if (!epochs_path.isEmpty()) {
            QList<Epoch> epochs = read_epochs(epochs_path);
            W->setEpochs(epochs);
        }
        if (window_title.isEmpty())
            window_title = pre_path;
        if (window_title.isEmpty())
            window_title = filt_path;
        if (window_title.isEmpty())
            window_title = raw_path;
        if (!firings_path.isEmpty()) {
            W->setFiringsPath(firings_path);
        }
        if (samplerate) {
            W->setSampleRate(samplerate);
        }

        W->setDefaultInitialization();
    } else if (mode == "spikespy") {
        printf("spikespy...\n");
        QStringList timeseries_paths = CLP.named_parameters["timeseries"].toString().split(",");
        QStringList firings_paths = CLP.named_parameters["firings"].toString().split(",");
        double samplerate = CLP.named_parameters["samplerate"].toDouble();

        SpikeSpyWidget* W = new SpikeSpyWidget(new MVViewAgent); //not that the view agent will not get deleted. :(
        W->setChannelColors(channel_colors);
        W->setLabelColors(label_colors);
        W->setSampleRate(samplerate);
        for (int i = 0; i < timeseries_paths.count(); i++) {
            QString tsp = timeseries_paths.value(i);
            if (tsp.isEmpty())
                tsp = timeseries_paths.value(0);
            QString fp = firings_paths.value(i);
            if (fp.isEmpty())
                fp = firings_paths.value(0);
            SpikeSpyViewData view;
            view.timeseries = DiskReadMda(tsp);
            view.firings = DiskReadMda(fp);
            W->addView(view);
        }
        W->show();
        W->move(QApplication::desktop()->screen()->rect().topLeft() + QPoint(200, 200));
        W->resize(1800, 1200);

        /*
        SSTimeSeriesWidget* W = new SSTimeSeriesWidget;
        SSTimeSeriesView* V = new SSTimeSeriesView;
        V->setSampleRate(samplerate);
        DiskArrayModel_New* DAM = new DiskArrayModel_New;
        DAM->setPath(timeseries_path);
        V->setData(DAM, true);
        Mda firings;
        firings.read(firings_path);
        QList<long> times, labels;
        for (int i = 0; i < firings.N2(); i++) {
            times << (long)firings.value(1, i);
            labels << (long)firings.value(2, i);
        }
        V->setTimesLabels(times, labels);
        W->addView(V);
        W->show();
        W->move(QApplication::desktop()->screen()->rect().topLeft() + QPoint(200, 200));
        W->resize(1800, 1200);
        */
    }

    int ret = a.exec();

    printf("Number of files open: %d, number of unfreed mallocs: %d, number of unfreed megabytes: %g\n", jnumfilesopen(), jmalloccount(), (int)jbytesallocated() * 1.0 / 1000000);

    return ret;
}

QColor brighten(QColor col, int amount)
{
    int r = col.red() + amount;
    int g = col.green() + amount;
    int b = col.blue() + amount;
    if (r > 255)
        r = 255;
    if (r < 0)
        r = 0;
    if (g > 255)
        g = 255;
    if (g < 0)
        g = 0;
    if (b > 255)
        b = 255;
    if (b < 0)
        b = 0;
    return QColor(r, g, b, col.alpha());
}

QList<QColor> generate_colors_ahb(int n_in)
{
    int n = n_in;
    float c[n][3], t, x;
    float grey = 0.6, sat = 0.65, bri = 0.7; // adj params
    //float dummy;

    c[n - 1][0] = grey;
    c[n - 1][1] = grey;
    c[n - 1][2] = grey;
    c[n - 2][0] = 1.0;
    c[n - 2][1] = 1.0;
    c[n - 2][2] = 1.0;

    n -= 2;
    for (int i = 0; i < n; ++i) {
        t = 6.0 * i / n;
        t = t + 0.2 * sinf(2.0 * M_PI / 3.0 * (t - 2.5));
        for (int j = 0; j < 3; ++j) {
            while (t<0) t+=6.0;
            while (t>6.0) t-=6.0;
            //t = 6.0 * modff(t / 6.0, &dummy); // does mod by 6
            x = 2.0 - fabsf(t - 3.0);
            if (x < 0)
                x = 0;
            if (x > 1)
                x = 1;
            c[i][j] = x;
            t = t - 2.0;
        }
        if (i % 2)
            for (int j = 0; j < 3; ++j)
                c[i][j] = sat * c[i][j] + (1.0 - sat);
        if (i % 4 == 1)
            for (int j = 0; j < 3; ++j)
                c[i][j] *= bri;
    }

    QList<QColor> ret;
    for (int i = 0; i < n_in; i++) {
        ret << QColor(c[i][0] * 255, c[i][1] * 255, c[i][2] * 255);
    }
    return ret;
}

QList<QColor> generate_colors(const QColor& bg, const QColor& fg, int noColors)
{
    Q_UNUSED(bg)
    Q_UNUSED(fg)
    return generate_colors_ahb(noColors);
}

// generate_colors() is adapted from code by...
/*
 * Copyright (c) 2008 Helder Correia
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
*/

QList<QColor> generate_colors_old(const QColor& bg, const QColor& fg, int noColors)
{
    QList<QColor> colors;
    const int HUE_BASE = (bg.hue() == -1) ? 90 : bg.hue();
    int h, s, v;

    for (int i = 0; i < noColors; i++) {
        h = int(HUE_BASE + (360.0 / noColors * i)) % 360;
        s = 240;
        v = int(qMax(bg.value(), fg.value()) * 0.85);

        // take care of corner cases
        const int M = 35;
        if ((h < bg.hue() + M && h > bg.hue() - M)
            || (h < fg.hue() + M && h > fg.hue() - M)) {
            h = ((bg.hue() + fg.hue()) / (i + 1)) % 360;
            s = ((bg.saturation() + fg.saturation() + 2 * i) / 2) % 256;
            v = ((bg.value() + fg.value() + 2 * i) / 2) % 256;
        }

        colors.append(QColor::fromHsv(h, s, v));
    }

    return colors;
}
