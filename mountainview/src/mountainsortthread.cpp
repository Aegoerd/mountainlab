/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 4/4/2016
*******************************************************/

#include "mountainsortthread.h"
#include <QCoreApplication>
#include <QMap>
#include <QProcess>
#include <QVariant>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "msmisc.h"
#include "cachemanager.h"
#include "mlutils.h"
#include "taskprogress.h"

class MountainsortThreadPrivate {
public:
    MountainsortThread* q;
    QString m_processor_name;
    QMap<QString, QVariant> m_parameters;
    //QString m_mscmdserver_url;
    QString m_mlproxy_url;

    QString create_temporary_output_file_name(const QString& remote_url, const QString& processor_name, const QMap<QString, QVariant>& params, const QString& parameter_name);
};

MountainsortThread::MountainsortThread()
{
    d = new MountainsortThreadPrivate;
    d->q = this;
}

MountainsortThread::~MountainsortThread()
{
    delete d;
}

void MountainsortThread::setProcessorName(const QString& pname)
{
    d->m_processor_name = pname;
}

QString MountainsortThread::makeOutputFilePath(const QString& pname)
{
    QString ret = d->create_temporary_output_file_name(d->m_mlproxy_url, d->m_processor_name, d->m_parameters, pname);
    d->m_parameters[pname] = ret;
    return ret;
}

void MountainsortThread::setInputParameters(const QMap<QString, QVariant>& parameters)
{
    d->m_parameters = parameters;
}

void MountainsortThread::setMLProxyUrl(const QString& url)
{
    d->m_mlproxy_url = url;
}

QJsonObject variantmap_to_json_obj(QVariantMap map)
{
    QJsonObject ret;
    QStringList keys = map.keys();
    // Use fromVariantMap
    foreach(QString key, keys)
    {
        ret[key] = QJsonValue::fromVariant(map[key]);
    }
    return ret;
}

QVariantMap json_obj_to_variantmap(QJsonObject obj)
{
    QVariantMap ret;
    QStringList keys = obj.keys();
    foreach(QString key, keys)
    {
        ret[key] = obj[key].toVariant();
    }
    return ret;
}

QJsonObject http_post(QString url, QJsonObject req)
{
    QTime timer;
    timer.start();
    QNetworkAccessManager manager;
    QNetworkRequest request;
    request.setUrl(url);
    QByteArray json_data = QJsonDocument(req).toJson();
    QNetworkReply* reply = manager.post(request, json_data);
    QEventLoop loop;
    QString ret;
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        ret+=reply->readAll();
    });
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    printf("RECEIVED TEXT (%d ms, %d bytes) from POST %s\n", timer.elapsed(), ret.count(), url.toLatin1().data());
    QString str = ret.mid(0, 5000) + "...";
    str.replace("\\n", "\n");
    printf("%s\n", (str.toLatin1().data()));
    QJsonObject obj = QJsonDocument::fromJson(ret.toLatin1()).object();
    return obj;
}

void MountainsortThread::compute()
{

    TaskProgress task("MS: " + d->m_processor_name);

    //if (d->m_mscmdserver_url.isEmpty()) {
    if (d->m_mlproxy_url.isEmpty()) {
        QString mountainsort_exe = mountainlabBasePath() + "/mountainsort/bin/mountainsort";
        QStringList args;
        args << d->m_processor_name;
        QStringList keys = d->m_parameters.keys();
        foreach(QString key, keys)
        {
            args << QString("--%1=%2").arg(key).arg(d->m_parameters.value(key).toString());
        }
        task.log(QString("Executing locally: %1").arg(mountainsort_exe));
        foreach(QString key, keys)
        {
            QString val = d->m_parameters[key].toString();
            task.log(QString("%1 = %2").arg(key).arg(val));
            if (val.startsWith("http")) {
                task.error("Executing locally, but parameter starts with http. Probably mpserver url has not been set.");
                return;
            }
        }

        if (QProcess::execute(mountainsort_exe, args) != 0) {
            qWarning() << "Problem running mountainsort" << mountainsort_exe << args;
            task.error("Problem running mountainsort");
        }
    } else {
        /*
        QString url = d->m_mscmdserver_url + "/?";
        url += "processor=" + d->m_processor_name + "&";
        QStringList keys = d->m_parameters.keys();
        foreach(QString key, keys)
        {
            url += QString("%1=%2&").arg(key).arg(d->m_parameters.value(key).toString());
        }
        this->setStatus("Remote " + d->m_processor_name, "http_get_text: " + url, 0.5);
        http_get_text(url);
        this->setStatus("", "", 1);
        */

        task.log("Setting up pp_process");
        QJsonObject pp_process;
        pp_process["processor_name"] = d->m_processor_name;
        pp_process["parameters"] = variantmap_to_json_obj(d->m_parameters);
        QJsonArray pp_processes;
        pp_processes.append(pp_process);
        QJsonObject pp;
        pp["processes"] = pp_processes;
        QString pipeline_json = QJsonDocument(pp).toJson(QJsonDocument::Compact);

        task.log(pipeline_json);

        QString script;
        script += QString("function main(params) {\n");
        script += QString("  MP.runPipeline('%1');\n").arg(pipeline_json);
        script += QString("}\n");

        QJsonObject req;
        req["action"] = "queueScript";
        req["script"] = script;
        QString url = d->m_mlproxy_url+"/mpserver";
        task.log("POSTING: " + url);
        task.log(QJsonDocument(req).toJson());
        QJsonObject resp = http_post(url, req);
        task.log("GOT RESPONSE: ");
        task.log(QJsonDocument(resp).toJson());
    }
}

QString MountainsortThreadPrivate::create_temporary_output_file_name(const QString& mlproxy_url, const QString& processor_name, const QMap<QString, QVariant>& params, const QString& parameter_name)
{
    QString str = processor_name + ":";
    QStringList keys = params.keys();
    qSort(keys);
    foreach(QString key, keys)
    {
        str += key + "=" + params.value(key).toString() + "&";
    }

    QString file_name = QString("%1_%2.tmp").arg(compute_hash(str)).arg(parameter_name);
    qDebug() << "************************************************ mlproxy_url = " << mlproxy_url;
    QString ret = CacheManager::globalInstance()->makeRemoteFile(mlproxy_url, file_name, CacheManager::LongTerm);
    return ret;
}
