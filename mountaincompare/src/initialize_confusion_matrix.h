/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 3/29/2017
*******************************************************/
#ifndef INITIALIZE_CONFUSION_MATRIX_H
#define INITIALIZE_CONFUSION_MATRIX_H

#include <QThread>
#include <mountainprocessrunner.h>

class Initialize_confusion_matrix : public QThread {
public:
    // input
    QString firings1;
    QString firings2;

    // output
    QString confusion_matrix;
    QString matched_firings;
    QString label_map;
    QString firings2_relabeled;

    void run()
    {
        MountainProcessRunner MPR;
        MPR.setProcessorName("mountainsort.confusion_matrix");
        QVariantMap params;
        params["firings1"] = firings1;
        params["firings2"] = firings2;
        params["match_matching_offset"] = 30;
        params["relabel_firings2"] = "true";
        MPR.setInputParameters(params);
        confusion_matrix = MPR.makeOutputFilePath("confusion_matrix_out");
        matched_firings = MPR.makeOutputFilePath("matched_firings_out");
        label_map = MPR.makeOutputFilePath("label_map_out");
        firings2_relabeled = MPR.makeOutputFilePath("firings2_relabeled_out");
        MPR.runProcess();
    }
};

#endif // INITIALIZE_CONFUSION_MATRIX_H
