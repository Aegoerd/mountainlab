/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 3/31/2016
*******************************************************/

#ifndef MVHISTOGRAMGRID_H
#define MVHISTOGRAMGRID_H

#include "mvabstractview.h"
#include "histogramview.h"

struct HorizontalScaleAxisData {
    bool use_it = false;
    QString label;
};

class MVHistogramGridPrivate;
class MVHistogramGrid : public MVAbstractView {
    Q_OBJECT
public:
    friend class MVHistogramGridPrivate;
    MVHistogramGrid(MVContext* context);
    virtual ~MVHistogramGrid();

    QImage renderImage(int W = 0, int H = 0);

    void paintEvent(QPaintEvent* evt);
    void keyPressEvent(QKeyEvent* evt);
signals:

protected:
    void setHorizontalScaleAxis(HorizontalScaleAxisData X);
    void setHistogramViews(const QList<HistogramView*> views);
    QList<HistogramView*> histogramViews();
    void prepareMimeData(QMimeData& mimeData, const QPoint& pos);
    void setPairMode(bool val);
    bool pairMode() const;
private slots:
    void slot_histogram_view_clicked(Qt::KeyboardModifiers modifiers);
    void slot_export_image();
    void slot_cluster_attributes_changed(int cluster_number);
    void slot_update_highlighting();
    void slot_context_menu(const QPoint& pt);

private:
    MVHistogramGridPrivate* d;
};

#endif // MVHISTOGRAMGRID_H
