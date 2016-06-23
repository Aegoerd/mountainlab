/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 3/31/2016
*******************************************************/

#include "mvhistogramgrid.h"
#include "histogramview.h"
#include "mvutils.h"
#include "taskprogress.h"

#include <QAction>
#include <QGridLayout>
#include <QKeyEvent>
#include <QList>
#include <QPainter>
#include <math.h>
#include "msmisc.h"
#include "mvmisc.h"

class MVHistogramGridPrivate {
public:
    MVHistogramGrid* q;

    QGridLayout* m_grid_layout;
    QList<HistogramView*> m_histogram_views;
    int m_num_columns;

    QList<QWidget*> m_child_widgets;

    void do_highlighting();
    int find_view_index_for_k(int k);
    void shift_select_clusters_between(int kA, int kB);
};

MVHistogramGrid::MVHistogramGrid(MVViewAgent* view_agent)
    : MVAbstractView(view_agent)
{
    d = new MVHistogramGridPrivate;
    d->q = this;
    d->m_num_columns = -1;

    QObject::connect(view_agent, SIGNAL(clusterAttributesChanged(int)), this, SLOT(slot_cluster_attributes_changed(int)));
    QObject::connect(view_agent, SIGNAL(currentClusterChanged()), this, SLOT(slot_update_highlighting()));
    QObject::connect(view_agent, SIGNAL(selectedClustersChanged()), this, SLOT(slot_update_highlighting()));

    QGridLayout* GL = new QGridLayout;
    GL->setHorizontalSpacing(12);
    GL->setVerticalSpacing(0);
    GL->setMargin(0);
    this->setLayout(GL);
    d->m_grid_layout = GL;

    this->setFocusPolicy(Qt::StrongFocus);

    {
        QAction* a = new QAction("Export image", this);
        this->addAction(a);
        connect(a, SIGNAL(triggered(bool)), this, SLOT(slot_export_image()));
    }

    this->recalculate();
}

MVHistogramGrid::~MVHistogramGrid()
{
    delete d;
}

QImage MVHistogramGrid::renderImage(int W, int H)
{
    if (!W)
        W = 1800;
    if (!H)
        H = 900;
    int max_row = 0, max_col = 0;
    for (int i = 0; i < d->m_histogram_views.count(); i++) {
        HistogramView* HV = d->m_histogram_views[i];
        int row = HV->property("row").toInt();
        int col = HV->property("col").toInt();
        if (row > max_row)
            max_row = row;
        if (col > max_col)
            max_col = col;
    }
    int NR = max_row + 1, NC = max_col + 1;
    int spacingx = 10;
    int spacingy = 10;
    int W0 = (W - spacingx * (NC + 1)) / NC;
    int H0 = (H - spacingy * (NR + 1)) / NR;

    QImage ret = QImage(W, H, QImage::Format_RGB32);
    QPainter painter(&ret);
    painter.fillRect(0, 0, ret.width(), ret.height(), Qt::white);

    for (int i = 0; i < d->m_histogram_views.count(); i++) {
        HistogramView* HV = d->m_histogram_views[i];
        int row = HV->property("row").toInt();
        int col = HV->property("col").toInt();
        QImage img = HV->renderImage(W0, H0);
        int x0 = spacingx + (W0 + spacingx) * col;
        int y0 = spacingy + (H0 + spacingy) * row;
        painter.drawImage(x0, y0, img);
    }

    return ret;
}

void MVHistogramGrid::paintEvent(QPaintEvent* evt)
{
    QWidget::paintEvent(evt);

    QPainter painter(this);
    if (isCalculating()) {
        //show that something is computing
        painter.fillRect(QRectF(0, 0, width(), height()), viewAgent()->color("calculation-in-progress"));
    }
}

void MVHistogramGrid::keyPressEvent(QKeyEvent* evt)
{
    if ((evt->key() == Qt::Key_A) && (evt->modifiers() & Qt::ControlModifier)) {
        QList<int> ks;
        for (int i = 0; i < d->m_histogram_views.count(); i++) {
            ks << d->m_histogram_views[i]->property("k").toInt();
        }
        viewAgent()->setSelectedClusters(ks);
    }
}

void MVHistogramGrid::setHistogramViews(const QList<HistogramView *> views)
{
    qDeleteAll(d->m_histogram_views);
    d->m_histogram_views=views;

    int NUM = views.count();
    int num_rows = (int)sqrt(NUM);
    if (num_rows < 1)
        num_rows = 1;
    int num_cols = (NUM + num_rows - 1) / num_rows;
    d->m_num_columns = num_cols;

    QGridLayout *GL=d->m_grid_layout;
    for (int jj = 0; jj < views.count(); jj++) {
        HistogramView* HV = views[jj];
        int row0 = (jj) / num_cols;
        int col0 = (jj) % num_cols;
        GL->addWidget(HV, row0, col0);
        HV->setProperty("row", row0);
        HV->setProperty("col", col0);
        HV->setProperty("view_index", jj);
        connect(HV, SIGNAL(clicked(Qt::KeyboardModifiers)), this, SLOT(slot_histogram_view_clicked(Qt::KeyboardModifiers)));
        connect(HV, SIGNAL(activated()), this, SLOT(slot_histogram_view_activated()));
        connect(HV, SIGNAL(signalExportHistogramMatrixImage()), this, SLOT(slot_export_image()));
    }
}

void MVHistogramGrid::slot_histogram_view_clicked(Qt::KeyboardModifiers modifiers)
{
    int k=sender()->property("k").toInt();

    if (modifiers & Qt::ControlModifier) {
        viewAgent()->clickCluster(k, Qt::ControlModifier);
    }
    else if (modifiers & Qt::ShiftModifier) {
        int k0 = viewAgent()->currentCluster();
        d->shift_select_clusters_between(k0, k);
    }
    else {
        viewAgent()->clickCluster(k, Qt::NoModifier);
    }
}

void MVHistogramGrid::slot_histogram_view_activated()
{
    emit histogramActivated();
}

void MVHistogramGrid::slot_export_image()
{
    QImage img = this->renderImage();
    user_save_image(img);
}

void MVHistogramGrid::slot_cluster_attributes_changed(int cluster_number)
{
    Q_UNUSED(cluster_number)
    // not implemented for now
}

void MVHistogramGrid::slot_update_highlighting()
{
    d->do_highlighting();
}

void MVHistogramGridPrivate::do_highlighting()
{
    QList<int> selected_clusters = q->viewAgent()->selectedClusters();
    for (int i = 0; i < m_histogram_views.count(); i++) {
        HistogramView* HV = m_histogram_views[i];
        int k = HV->property("k").toInt();
        if (k == q->viewAgent()->currentCluster()) {
            HV->setCurrent(true);
        }
        else {
            HV->setCurrent(false);
        }
        if (selected_clusters.contains(k)) {
            HV->setSelected(true);
        }
        else {
            HV->setSelected(false);
        }
    }
}

void MVHistogramGridPrivate::shift_select_clusters_between(int kA, int kB)
{
    QSet<int> selected_clusters = q->viewAgent()->selectedClusters().toSet();
    int ind1 = find_view_index_for_k(kA);
    int ind2 = find_view_index_for_k(kB);
    if ((ind1 >= 0) && (ind2 >= 0)) {
        for (int ii = qMin(ind1, ind2); ii <= qMax(ind1, ind2); ii++) {
            selected_clusters.insert(m_histogram_views[ii]->property("k2").toInt());
        }
    }
    else if (ind1 >= 0) {
        selected_clusters.insert(m_histogram_views[ind1]->property("k2").toInt());
    }
    else if (ind2 >= 0) {
        selected_clusters.insert(m_histogram_views[ind2]->property("k2").toInt());
    }
    q->viewAgent()->setSelectedClusters(QList<int>::fromSet(selected_clusters));
}

int MVHistogramGridPrivate::find_view_index_for_k(int k)
{
    for (int i = 0; i < m_histogram_views.count(); i++) {
        if (m_histogram_views[i]->property("k").toInt() == k)
            return i;
    }
    return -1;
}
