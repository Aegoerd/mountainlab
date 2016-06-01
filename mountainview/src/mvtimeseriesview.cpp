/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 5/27/2016
*******************************************************/

#include "mvtimeseriesview.h"
#include "multiscaletimeseries.h"
#include "mvtimeseriesrendermanager.h"
#include <math.h>

#include <QMouseEvent>
#include <QPainter>

struct mvtsv_coord {
    mvtsv_coord(long channel0 = 0, double t0 = 0, double y0 = 0)
    {
        channel = channel0;
        t = t0;
        y = y0;
    }
    long channel;
    double t;
    double y;
    static mvtsv_coord from_t(double t)
    {
        return mvtsv_coord(0, t, 0);
    }
};

struct mvtsv_channel {
    long channel;
    QString label;
    QRectF geometry;
};

class MVTimeSeriesViewPrivate {
public:
    MVTimeSeriesView* q;
    MultiScaleTimeSeries m_ts;
    DiskReadMda m_data;
    double m_view_t1, m_view_t2;
    double m_amplitude_factor;
    QList<mvtsv_channel> m_channels;
    double m_current_t;
    MVRange m_selected_t_range;
    bool m_activated;

    MVTimeSeriesRenderManagerPrefs m_prefs;

    bool m_layout_needed;

    MVTimeSeriesRenderManager m_render_manager;

    QPointF m_left_click_anchor_pix;
    mvtsv_coord m_left_click_anchor_coord;
    MVRange m_left_click_anchor_t_range;
    bool m_left_click_dragging;

    QList<mvtsv_channel> make_channel_layout(double W, double H, long M);
    void paint_cursor(QPainter* painter, double W, double H);
    QPointF coord2pix(mvtsv_coord C);
    mvtsv_coord pix2coord(long channel, QPointF pix);

    void zoom_out(mvtsv_coord about_coord, double frac = 0.8);
    void zoom_in(mvtsv_coord about_coord, double frac = 0.8);
};

MVTimeSeriesView::MVTimeSeriesView()
{
    d = new MVTimeSeriesViewPrivate;
    d->q = this;
    d->m_current_t = 0;
    d->m_selected_t_range = MVRange(-1, -1);
    d->m_activated = true; /// TODO set activated only when window is active (like in sstimeseriesview, I think)
    d->m_amplitude_factor = 1;
    d->m_left_click_anchor_pix = QPointF(-1, -1);
    d->m_left_click_dragging = false;
    d->m_layout_needed = true;
    this->setMouseTracking(true);
    d->m_render_manager.setMultiScaleTimeSeries(&d->m_ts);
    d->m_render_manager.setPrefs(d->m_prefs);
}

MVTimeSeriesView::~MVTimeSeriesView()
{
    delete d;
}

void MVTimeSeriesView::setData(const DiskReadMda& X)
{
    d->m_data = X;
    d->m_ts.setData(X);
    d->m_layout_needed = true;
    update();
}

MVRange MVTimeSeriesView::timeRange() const
{
    return MVRange(d->m_view_t1, d->m_view_t2);
}

void MVTimeSeriesView::resizeEvent(QResizeEvent* evt)
{
    d->m_layout_needed = true;
    QWidget::resizeEvent(evt);
}

void MVTimeSeriesView::setTimeRange(MVRange range)
{
    d->m_view_t1 = range.min;
    d->m_view_t2 = range.max;
    update();
}

void MVTimeSeriesView::setCurrentTimepoint(double t)
{
    if (t == d->m_current_t)
        return;
    d->m_current_t = t;
    update();
}

void MVTimeSeriesView::setSelectedTimeRange(MVRange range)
{
    if (range == d->m_selected_t_range)
        return;
    d->m_selected_t_range = range;
    update();
}

double MVTimeSeriesView::currentTimepoint() const
{
    return d->m_current_t;
}

void MVTimeSeriesView::paintEvent(QPaintEvent* evt)
{
    Q_UNUSED(evt)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    double W0 = this->width();
    double H0 = this->height();
    long M = d->m_data.N1();
    if (!M)
        return;

    if (d->m_layout_needed) {
        d->m_layout_needed = false;
        d->m_channels = d->make_channel_layout(W0, H0, M);
        for (long m = 0; m < M; m++) {
            mvtsv_channel* CH = &d->m_channels[m];
            CH->channel = m;
            CH->label = QString("%1").arg(m + 1);
        }
    }

    d->paint_cursor(&painter, W0, H0);

    long num_t = d->m_view_t2 - d->m_view_t1;
    long panel_size = 100;
    while (num_t / panel_size > 10) {
        panel_size *= 10;
    }
    long panel_index_1 = d->m_view_t1 / panel_size;
    long panel_index_2 = d->m_view_t2 / panel_size;
    for (long panel_index = panel_index_1; panel_index <= panel_index_2; panel_index++) {
        double t1 = panel_index * panel_size;
        double t2 = t1 + panel_size;
        QPointF pix1 = d->coord2pix(mvtsv_coord::from_t(t1));
        QPointF pix2 = d->coord2pix(mvtsv_coord::from_t(t2));
        double xx = pix2.x();
        double WW = pix2.x() - pix1.x();
        QImage img = d->m_render_manager.getImage(t1, t2, d->m_amplitude_factor, WW, H0);
        painter.drawImage(xx, 0, img);
    }
}

void MVTimeSeriesView::mousePressEvent(QMouseEvent* evt)
{
    if (evt->button() == Qt::LeftButton) {
        d->m_left_click_anchor_pix = evt->pos();
        d->m_left_click_anchor_t_range = this->timeRange();
        d->m_left_click_anchor_coord = d->pix2coord(0, evt->pos());
        d->m_left_click_dragging = false;
    }
    //update_cursor();
}

void MVTimeSeriesView::mouseReleaseEvent(QMouseEvent* evt)
{
    d->m_left_click_anchor_pix = QPointF(-1, -1);
    if (evt->button() == Qt::LeftButton) {
        if (!d->m_left_click_dragging) {
            mvtsv_coord coord = d->pix2coord(0, evt->pos());
            this->setCurrentTimepoint(coord.t);
        }
        d->m_left_click_dragging = false;
    }
    //update_cursor();
}

void MVTimeSeriesView::mouseMoveEvent(QMouseEvent* evt)
{
    if (!d->m_left_click_dragging) {
        if (d->m_left_click_anchor_pix.x() >= 0) {
            double dist = qAbs(d->m_left_click_anchor_pix.x() - evt->pos().x());
            if (dist >= 5) {
                d->m_left_click_dragging = true;
            }
        }
    }

    if (d->m_left_click_dragging) {
        // we want the evt->pos() to correspond to d->m_left_click_anchor_coord.t
        double t0 = d->pix2coord(0, evt->pos()).t;
        double dt = t0 - d->m_left_click_anchor_coord.t;
        this->setTimeRange(this->timeRange() + (-dt));
    }

    /// TODO set the mouse pointer cursor
    //set_cursor();
}

void MVTimeSeriesView::wheelEvent(QWheelEvent* evt)
{
    int delta = evt->delta();
    if (!(evt->modifiers() & Qt::ControlModifier)) {
        if (delta < 0) {
            d->zoom_out(mvtsv_coord::from_t(this->currentTimepoint()));
        } else if (delta > 0) {
            d->zoom_in(mvtsv_coord::from_t(this->currentTimepoint()));
        }
    } else {
        //This used to allow zooming at hover position -- probably not needed
        /*
        float frac = 1;
        if (delta < 0)
            frac = 1 / 0.8;
        else if (delta > 0)
            frac = 0.8;
        Vec2 coord = this->plot()->pixToCoord(vec2(evt->pos().x(), evt->pos().y()));
        d->do_zoom(coord.x, frac);
        */
    }
}

void MVTimeSeriesView::unit_test()
{
    long M = 4;
    long N = 100000;
    Mda X(M, N);
    for (long n = 0; n < N; n++) {
        for (long m = 0; m < M; m++) {
            double period = (m + 1) * 105.8;
            double val = sin(n * 2 * M_PI / period);
            val *= (N - n) * 1.0 / N;
            val += (qrand() % 10000) * 1.0 / 10000 * 0.4;
            X.setValue(val, m, n);
        }
    }
    DiskReadMda X0(X);
    MVTimeSeriesView* W = new MVTimeSeriesView;
    W->setData(X0);
    W->setTimeRange(MVRange(0, 1000));
    W->show();
}

QList<mvtsv_channel> MVTimeSeriesViewPrivate::make_channel_layout(double W, double H, long M)
{
    QList<mvtsv_channel> channels;
    if (!M)
        return channels;
    MVTimeSeriesRenderManagerPrefs P = m_prefs;
    double mleft = P.margins[0];
    double mright = P.margins[1];
    double mtop = P.margins[2];
    double mbottom = P.margins[3];
    double space = P.space_between_channels;
    double channel_height = (H - mbottom - mtop - (M - 1) * space) / M;
    double y0 = mtop;
    for (int m = 0; m < M; m++) {
        mvtsv_channel X;
        X.geometry = QRectF(mleft, y0, W - mleft - mright, channel_height);
        channels << X;
        y0 += channel_height + space;
    }
    return channels;
}

void MVTimeSeriesViewPrivate::paint_cursor(QPainter* painter, double W, double H)
{
    Q_UNUSED(W)
    Q_UNUSED(H)


    MVTimeSeriesRenderManagerPrefs P = m_prefs;
    double mtop = P.margins[2];
    double mbottom = P.margins[3];

    if (m_selected_t_range.min < 0) {
        QPointF p0 = coord2pix(mvtsv_coord(0, m_current_t, 0));
        QPointF p1 = coord2pix(mvtsv_coord(0, m_current_t, 0));
        p0.setY(mtop);
        p1.setY(H - mbottom);

        for (int pass = 1; pass <= 2; pass++) {
            QPainterPath path;
            QPointF pp = p0;
            int sign = -1;
            if (pass == 2) {
                pp = p1;
                sign = 1;
            }
            path.moveTo(pp.x(), pp.y() - 10 * sign);
            path.lineTo(pp.x() - 8, pp.y() - 2 * sign);
            path.lineTo(pp.x() + 8, pp.y() - 2 * sign);
            path.lineTo(pp.x(), pp.y() - 10 * sign);
            QColor col = QColor(60, 80, 60);
            if (m_activated) {
                //col=Qt::gray;
                col = QColor(50, 50, 220);
            }
            painter->fillPath(path, QBrush(col));
        }

        QPainterPath path2;
        path2.moveTo(p0.x(), p0.y() + 10);
        path2.lineTo(p1.x(), p1.y() - 10);
        painter->setPen(QPen(QBrush(QColor(50, 50, 220, 60)), 0));
        painter->drawPath(path2);

        //painter->setPen(QPen(QBrush(QColor(50,50,220,180)),1));
        //painter->drawPath(path2);
    }

    if (m_selected_t_range.min >= 0) {
        QPointF p0 = coord2pix(mvtsv_coord(0, m_selected_t_range.min, 0));
        QPointF p1 = coord2pix(mvtsv_coord(0, m_selected_t_range.max, 0));
        p0.setY(mtop);
        p1.setY(H - mbottom);

        QPainterPath path;
        path.moveTo(p0.x(), p0.y());
        path.lineTo(p1.x(), p0.y());
        path.lineTo(p1.x(), p1.y());
        path.lineTo(p0.x(), p1.y());
        path.lineTo(p0.x(), p0.y());

        int pen_width = 6;
        QColor pen_color = QColor(150, 150, 150);
        painter->setPen(QPen(QBrush(pen_color), pen_width));
        painter->drawPath(path);
    }
}

QPointF MVTimeSeriesViewPrivate::coord2pix(mvtsv_coord C)
{
    if (C.channel >= m_channels.count())
        return QPointF(0, 0);

    mvtsv_channel* CH = &m_channels[C.channel];

    double xpct = (C.t - m_view_t1) / (m_view_t2 - m_view_t1);
    double px = CH->geometry.x() + xpct * CH->geometry.width();
    double py = CH->geometry.y() + CH->geometry.height() / 2 + CH->geometry.height() / 2 * (C.y * m_amplitude_factor);
    return QPointF(px, py);
}

mvtsv_coord MVTimeSeriesViewPrivate::pix2coord(long channel, QPointF pix)
{
    if (channel >= m_channels.count())
        return mvtsv_coord(0, 0, 0);

    mvtsv_channel* CH = &m_channels[channel];

    mvtsv_coord C;
    double xpct = 0;
    if (CH->geometry.width()) {
        xpct = (pix.x() - CH->geometry.x()) / (CH->geometry.width());
    }
    C.t = m_view_t1 + xpct * (m_view_t2 - m_view_t1);
    if (m_amplitude_factor) {
        C.y = (pix.y() - (CH->geometry.y() + CH->geometry.height() / 2)) / m_amplitude_factor / (CH->geometry.height() / 2);
    }
    return C;
}

void MVTimeSeriesViewPrivate::zoom_out(mvtsv_coord about_coord, double frac)
{
    QPointF about_pix = coord2pix(about_coord);
    q->setTimeRange(q->timeRange() * (1 / frac));
    mvtsv_coord new_coord = pix2coord(0, about_pix);
    double dt = about_coord.t - new_coord.t;
    q->setTimeRange(q->timeRange() + (dt));
}

void MVTimeSeriesViewPrivate::zoom_in(mvtsv_coord about_coord, double frac)
{
    zoom_out(about_coord, 1 / frac);
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
