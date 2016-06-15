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
#include "mvviewagent.h"

class MVClipsWidgetPrivate;
class MVClipsWidget : public QWidget {
    Q_OBJECT
public:
    friend class MVClipsWidgetPrivate;
    MVClipsWidget(MVViewAgent* view_agent);
    virtual ~MVClipsWidget();
    void setMLProxyUrl(const QString& url);
    void setLabelsToUse(const QList<int>& labels);

    int currentClipIndex();
signals:
    void currentEventChanged();
private slots:
    void slot_computation_finished();
    void slot_restart_calculation();
    void slot_view_agent_option_changed(QString name);

private:
    MVClipsWidgetPrivate* d;
};

#endif // MVCLIPSWIDGET_H
