/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 4/4/2016
*******************************************************/

#ifndef MVCLIPSWIDGET_H
#define MVCLIPSWIDGET_H

#include "diskreadmda.h"
#include <QWidget>
#include "mvutils.h"
#include "mvabstractview.h"
#include "mvabstractviewfactory.h"

class MVClipsWidgetPrivate;
class MVClipsWidget : public MVAbstractView {
    Q_OBJECT
public:
    friend class MVClipsWidgetPrivate;
    MVClipsWidget(MVContext* context);
    virtual ~MVClipsWidget();

    void prepareCalculation() Q_DECL_OVERRIDE;
    void runCalculation() Q_DECL_OVERRIDE;
    void onCalculationFinished() Q_DECL_OVERRIDE;

    void setLabelsToUse(const QList<int>& labels);

    void paintEvent(QPaintEvent* evt);
signals:
private slots:

private:
    MVClipsWidgetPrivate* d;
};

class MVClipsFactory : public MVAbstractViewFactory {
    Q_OBJECT
public:
    MVClipsFactory(QObject* parent = 0);
    QString id() const Q_DECL_OVERRIDE;
    QString name() const Q_DECL_OVERRIDE;
    QString title() const Q_DECL_OVERRIDE;
    MVAbstractView* createView(MVContext* context) Q_DECL_OVERRIDE;
    int order() const Q_DECL_OVERRIDE;
    bool isEnabled(MVContext* context) const Q_DECL_OVERRIDE;
};

#endif // MVCLIPSWIDGET_H
