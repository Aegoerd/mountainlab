/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 5/2/2016
*******************************************************/

#ifndef MPDAEMON_H
#define MPDAEMON_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QJsonObject>
#include <QProcess>
#include <QFile>
#include "processmanager.h" //for RequestProcessResources

struct ProcessResources {
    double num_threads = 0;
    double memory_gb = 0;
    double num_processes = 0;
};

class MPDaemonPrivate;
class MPDaemon : public QObject {
    Q_OBJECT
public:
    friend class MPDaemonPrivate;
    MPDaemon();
    virtual ~MPDaemon();
    void setTotalResourcesAvailable(ProcessResources PR);
    void setLogPath(const QString& path);
    bool run();
    void clearProcessing();

    static QString daemonPath();
    static QString makeTimestamp(const QDateTime& dt = QDateTime::currentDateTime());
    static QDateTime parseTimestamp(const QString& timestamp);
    static bool waitForFileToAppear(QString fname, qint64 timeout_ms = -1, bool remove_on_appear = false, qint64 parent_pid = 0, QString stdout_fname = "");
    static void wait(qint64 msec);
    static bool pidExists(qint64 pid);
    static bool waitForFinishedAndWriteOutput(QProcess* P);

private slots:
    void slot_pript_qprocess_finished();
    void slot_qprocess_output();
    void iterate();

private:
    MPDaemonPrivate* d;
};

struct ProcessRuntimeOpts {
    ProcessRuntimeOpts()
    {
        num_threads_allotted = 1;
        memory_gb_allotted = 1;
    }
    double num_threads_allotted = 1;
    double memory_gb_allotted = 0;
};

bool is_at_most(ProcessResources needed, ProcessResources available, ProcessResources total_allocated);

enum PriptType {
    ScriptType,
    ProcessType
};

struct MPDaemonPript {
    //Represents a process or a script
    PriptType prtype = ScriptType;
    QString id;
    QString output_fname;
    QString stdout_fname;
    QVariantMap parameters;
    bool is_running = false;
    bool is_finished = false;
    bool success = false;
    QString error;
    QJsonObject runtime_results;
    qint64 parent_pid = 0;
    bool force_run;
    QString daemon_id;
    QString working_path;
    QDateTime timestamp_queued;
    QDateTime timestamp_started;
    QDateTime timestamp_finished;
    QProcess* qprocess = 0;
    QFile* stdout_file = 0;

    //For a script:
    QStringList script_paths;
    QStringList script_path_checksums; //to ensure that scripts have not changed at time of running

    //For a process:
    QString processor_name;
    //double num_threads_requested = 1;
    //double memory_gb_requested = 0;
    RequestProcessResources RPR;
    ProcessRuntimeOpts runtime_opts; //defined at run time
};

enum RecordType {
    AbbreviatedRecord,
    FullRecord,
    RuntimeRecord
};

QJsonObject pript_struct_to_obj(MPDaemonPript S, RecordType rt);
MPDaemonPript pript_obj_to_struct(QJsonObject obj);
QJsonArray stringlist_to_json_array(QStringList list);
QStringList json_array_to_stringlist(QJsonArray X);
QJsonObject variantmap_to_json_obj(QVariantMap map);
QVariantMap json_obj_to_variantmap(QJsonObject obj);
QJsonObject runtime_opts_struct_to_obj(ProcessRuntimeOpts opts);

// this has to be a POD -> no pointers, no dynamic memory allocation
struct MountainProcessDescriptor {
    uint32_t version = 1;
    pid_t pid;
};

#endif // MPDAEMON_H
