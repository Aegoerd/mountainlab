#include "matrixview.h"

#include <QMouseEvent>
#include <mvmisc.h>

class MatrixViewPrivate {
public:
    MatrixView *q;
    Mda m_matrix;
    MVRange m_value_range=MVRange(0,1);
    MatrixView::Mode m_mode;
    QVector<int> m_perm_rows;
    QVector<int> m_perm_cols;
    QStringList m_row_labels;
    QStringList m_col_labels;
    bool m_draw_divider_for_final_row=true;
    bool m_draw_divider_for_final_column=true;
    QPoint m_hovered_element=QPoint(-1,-1);
    QPoint m_current_element=QPoint(-1,-1);

    //left,right,top,bottom
    double m_margins[4]={20,0,0,20};

    QRectF get_entry_rect(int m,int n);
    QPointF coord2pix(double m,double n);
    QPointF pix2coord(QPointF pix);
    QColor get_color(double val);
    void draw_string_in_rect(QPainter &painter,QRectF r,QString txt,QColor col,int fontsize=0);
    QColor complementary_color(QColor col);
    int row_map(int m);
    int col_map(int n);
    int row_map_inv(int m);
    int col_map_inv(int n);
    void set_hovered_element(QPoint a);
};

MatrixView::MatrixView()
{
    d=new MatrixViewPrivate;
    d->q=this;

    this->setMouseTracking(true);
}

MatrixView::~MatrixView()
{
    delete d;
}

void MatrixView::setMode(MatrixView::Mode mode)
{
    d->m_mode=mode;
}

void MatrixView::setMatrix(const Mda &A)
{
    d->m_matrix=A;
    update();
}

void MatrixView::setValueRange(double minval, double maxval)
{
    d->m_value_range=MVRange(minval,maxval);
    update();
}

void MatrixView::setIndexPermutations(const QVector<int> &perm_rows, const QVector<int> &perm_cols)
{
    d->m_perm_rows=perm_rows;
    d->m_perm_cols=perm_cols;
    update();
}

void MatrixView::setLabels(const QStringList &row_labels, const QStringList &col_labels)
{
    d->m_row_labels=row_labels;
    d->m_col_labels=col_labels;
    update();
}

void MatrixView::setDrawDividerForFinalRow(bool val)
{
    d->m_draw_divider_for_final_row=val;
    update();
}

void MatrixView::setDrawDividerForFinalColumn(bool val)
{
    d->m_draw_divider_for_final_column=val;
    update();
}

void MatrixView::setCurrentElement(QPoint pt)
{
    if ((pt.x()<0)||(pt.y()<0)) {
        pt=QPoint(-1,-1);
    }
    if (pt==d->m_current_element) return;
    d->m_current_element=pt;
    emit currentElementChanged();
    update();
}

QPoint MatrixView::currentElement() const
{
    return d->m_current_element;
}

void MatrixView::paintEvent(QPaintEvent *evt)
{
    Q_UNUSED(evt)

    double left=d->m_margins[0];
    //double right=d->m_margins[1];
    //double top=d->m_margins[2];
    double bottom=d->m_margins[3];

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    int M=d->m_matrix.N1();
    int N=d->m_matrix.N2();
    for (int n=0; n<N; n++) {
        for (int m=0; m<M; m++) {
            QRectF r=d->get_entry_rect(m,n);
            double val=d->m_matrix.value(m,n);
            QColor col=d->get_color(val);
            painter.fillRect(r,col);
            QString str;
            if (d->m_mode==PercentMode) {
                str=QString("%1%").arg((int)(val*100));
                if (val==0) str="";
            }
            else if (d->m_mode==CountsMode) {
                str=QString("%1").arg(val);
                if (val==0) str="";
            }
            d->draw_string_in_rect(painter,r,str,d->complementary_color(col));
        }
    }

    for (int m=0; m<M; m++) {
        int m2=d->row_map(m);
        QPointF pt1=d->coord2pix(m2,0);
        QPointF pt2=d->coord2pix(m2+1,0);
        QRectF r=QRectF(pt1.x()-left,pt1.y(),left,pt2.y()-pt1.y());
        int fontsize=qMin(16.0,pt2.y()-pt1.y());
        d->draw_string_in_rect(painter,r,d->m_row_labels.value(m),Qt::black,fontsize);
    }
    for (int n=0; n<N; n++) {
        int n2=d->col_map(n);
        QPointF pt1=d->coord2pix(M,n2);
        QPointF pt2=d->coord2pix(M,n2+1);
        QRectF r=QRectF(pt1.x(),pt1.y(),pt2.x()-pt1.x(),bottom);
        int fontsize=qMin(16.0,(pt2.x()-pt1.x())/2);
        d->draw_string_in_rect(painter,r,d->m_col_labels.value(n),Qt::black,fontsize);
    }

    if (d->m_draw_divider_for_final_row) {
        QPointF pt1=d->coord2pix(M-1,0);
        QPointF pt2=d->coord2pix(M-1,N-1);
        QPen pen(QBrush(Qt::gray),2);
        painter.setPen(pen);
        painter.drawLine(pt1,pt2);
    }
    if (d->m_draw_divider_for_final_column) {
        QPointF pt1=d->coord2pix(0,N-1);
        QPointF pt2=d->coord2pix(M-1,N-1);
        QPen pen(QBrush(Qt::gray),2);
        painter.setPen(pen);
        painter.drawLine(pt1,pt2);
    }

    QColor col(255,255,220,60);
    if (d->m_hovered_element.x()>=0) {
        int m2=d->row_map(d->m_hovered_element.x());
        QPointF pt1=d->coord2pix(m2,0);
        QPointF pt2=d->coord2pix(m2+1,N);
        QRectF r=QRectF(pt1-QPointF(left,0),pt2);
        painter.fillRect(r,col);
    }
    if (d->m_hovered_element.y()>=0) {
        int n2=d->col_map(d->m_hovered_element.y());
        int m2=d->row_map(d->m_hovered_element.x());
        QPointF pt1=d->coord2pix(0,n2);
        QPointF pt2=d->coord2pix(m2,n2+1);
        QPointF pt3=d->coord2pix(m2+1,n2);
        QPointF pt4=d->coord2pix(M,n2+1);
        QRectF r1=QRectF(pt1,pt2);
        QRectF r2=QRectF(pt3,pt4+QPointF(0,bottom));

        painter.fillRect(r1,col);
        painter.fillRect(r2,col);
    }

    if ((d->m_current_element.x()>=0)&&(d->m_current_element.y()>=0)) {
        QRectF r=d->get_entry_rect(d->m_current_element.x(),d->m_current_element.y());
        QPen pen(QBrush(QColor(255,200,180)),3);
        painter.setPen(pen);
        painter.drawRect(r);
    }
}

void MatrixView::mouseMoveEvent(QMouseEvent *evt)
{
    int M=d->m_matrix.N1();
    int N=d->m_matrix.N2();

    QPointF pos=evt->pos();
    QPointF coord=d->pix2coord(pos);
    int m=(int)coord.x();
    int n=(int)coord.y();
    if ((0<=m)&&(m<M)&&(0<=n)&&(n<N)) {
        int m2=d->row_map_inv(m);
        int n2=d->col_map_inv(n);
        d->set_hovered_element(QPoint(m2,n2));
    }
    else {
        d->set_hovered_element(QPoint(-1,-1));
    }

}

void MatrixView::leaveEvent(QEvent *)
{
    d->set_hovered_element(QPoint(-1,-1));
    update();
}

void MatrixView::mousePressEvent(QMouseEvent *evt)
{
    int M=d->m_matrix.N1();
    int N=d->m_matrix.N2();

    QPointF pos=evt->pos();
    QPointF coord=d->pix2coord(pos);
    int m=(int)coord.x();
    int n=(int)coord.y();
    if ((0<=m)&&(m<M)&&(0<=n)&&(n<N)) {
        int m2=d->row_map_inv(m);
        int n2=d->col_map_inv(n);
        this->setCurrentElement(QPoint(m2,n2));
    }
    else {
        this->setCurrentElement(QPoint(-1,-1));
    }
}

QRectF MatrixViewPrivate::get_entry_rect(int m, int n)
{
    m=row_map(m);
    n=col_map(n);
    QPointF pt1=coord2pix(m,n);
    QPointF pt2=coord2pix(m+1,n+1)+QPointF(1,1);
    return QRectF(pt1,pt2);
}

QPointF MatrixViewPrivate::coord2pix(double m, double n)
{
    double left=m_margins[0];
    double right=m_margins[1];
    double top=m_margins[2];
    double bottom=m_margins[3];

    int W0=q->width()-left-right;
    int H0=q->height() -top-bottom;
    if (!(W0*H0)) return QPointF(-1,-1);
    int M=m_matrix.N1();
    int N=m_matrix.N2();
    if (!(M*N)) return QPointF(0,0);

    return QPointF(left+n/N*W0,top+m/M*H0);
}

QPointF MatrixViewPrivate::pix2coord(QPointF pix)
{
    double left=m_margins[0];
    double right=m_margins[1];
    double top=m_margins[2];
    double bottom=m_margins[3];

    int W0=q->width()-left-right;
    int H0=q->height() -top-bottom;
    if (!(W0*H0)) return QPointF(-1,-1);
    int M=m_matrix.N1();
    int N=m_matrix.N2();
    if (!(M*N)) return QPointF(0,0);

    return QPointF((pix.y()-top)*M/H0,(pix.x()-left)*N/W0);
}

QColor MatrixViewPrivate::get_color(double val)
{
    if (m_value_range.max<=m_value_range.min) return QColor(0,0,0);
    double tmp=(val-m_value_range.min)/m_value_range.range();
    tmp=qMax(0.0,qMin(1.0,tmp));
    //tmp=1-tmp;
    int tmp2=(int)(tmp*255);
    return QColor(tmp2/2,tmp2,tmp2);
}

void MatrixViewPrivate::draw_string_in_rect(QPainter &painter, QRectF r, QString txt, QColor col, int fontsize)
{
    painter.save();

    QPen pen=painter.pen();
    pen.setColor(col);
    painter.setPen(pen);

    QFont font=painter.font();
    if (fontsize) {
        font.setPixelSize(fontsize);
    }
    else {
        //determine automatically
        int pix=16;
        font.setPixelSize(pix);
        while ((pix>6)&&(QFontMetrics(font).width(txt)>=r.width())) {
            pix--;
            font.setPixelSize(pix);
        }
    }
    painter.setFont(font);

    painter.setClipRect(r);
    painter.drawText(r,Qt::AlignCenter|Qt::AlignVCenter,txt);

    painter.restore();
}

QColor MatrixViewPrivate::complementary_color(QColor col)
{

    double r=col.red()*1.0/255;
    double g=col.green()*1.0/255;
    double b=col.blue()*1.0/255;
    double r2,g2,b2;

    //(0,0,0) -> (0.5,0.5,0.5)
    //(0.5,0.5,0.5) -> (0.7,0.7,1)
    //(1,1,1) -> (0,0,0.4)
    if (r<0.5) {
        r2=0.5+r/0.5*0.2;
    }
    else {
        r2=0.7-(r-0.5)/0.5*0.7;
    }
    if (g<0.5) {
        g2=0.5+g/0.5*0.2;
    }
    else {
        g2=0.7-(g-0.5)/0.5*0.7;
    }
    if (b<0.5) {
        b2=0.5+b/0.5*0.5;
    }
    else {
        b2=1-(b-0.5)/0.5*0.6;
    }

    return QColor((int)(r2*255),(int)(g2*255),(int)(b2*255));
}

int MatrixViewPrivate::row_map(int m)
{
    if (m<m_perm_rows.count()) return m_perm_rows[m];
    else return m;
}

int MatrixViewPrivate::col_map(int n)
{
    if (n<m_perm_cols.count()) return m_perm_cols[n];
    else return n;
}

int MatrixViewPrivate::row_map_inv(int m)
{
    int m2=m_perm_rows.indexOf(m);
    if (m2<0) return m;
    else return m2;
}

int MatrixViewPrivate::col_map_inv(int n)
{
    int n2=m_perm_cols.indexOf(n);
    if (n2<0) return n;
    else return n2;
}

void MatrixViewPrivate::set_hovered_element(QPoint a)
{
    if (m_hovered_element==a) return;
    m_hovered_element=a;
    q->update();
}
