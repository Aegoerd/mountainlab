/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
*******************************************************/

#ifndef MVCLIPSVIEW_H
#define MVCLIPSVIEW_H

#include "sstimeseriesview.h"
#include "mvutils.h"

/** \class MVClipsView
 *  \brief View a set of clips. Usually each clip contains a single spike.
 */

class MVClipsView : public SSTimeSeriesView {
    Q_OBJECT
public:
    ///Create a new instance
    static MVClipsView* newInstance();
    virtual ~MVClipsView()
    {
    }

    ///Set MxTxL array of clips to view
    virtual void setClips(const Mda& clips) = 0;

    ///Set the associated times, for purpose of currentEvent()
    virtual void setTimes(const QList<double>& times) = 0;

    ///Set the associated labels, for purpose of currentEvent()
    virtual void setLabels(const QList<int>& labels) = 0;

    ///The index of the currently selected clip
    virtual int currentClipIndex() = 0;

    ///The currently selected event -- this has the time/label
    virtual MVEvent currentEvent() = 0;

    ///Set the current event, which searches times/labels to move the cursor to the corresponding clip
    virtual void setCurrentEvent(MVEvent evt) = 0;

signals:

    ///The user has selected a new current clip, which affects the outputs of currentClipIndex() and currentEvent()
    void currentEventChanged();
};

#endif // MVCLIPSVIEW_H
