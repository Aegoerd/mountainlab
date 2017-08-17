#include "handle_request.h"
#include "processmanager.h"
#include "mlcommon.h"

#include <QJsonDocument>
#include <cachemanager.h>
#include <QCoreApplication>
#include <QFile>
#include <QCryptographicHash>

QJsonObject handle_request_run_process(QString processor_name,const QJsonObject &inputs,const QJsonObject &outputs,const QJsonObject &parameters);

QJsonObject handle_request(const QJsonObject &request)
{
    QString action=request["action"].toString();

    QJsonObject response;
    response["success"]=false; //assume the worst

    if (action=="run_process") {
        QString processor_name=request["processor_name"].toString();
        if (processor_name.isEmpty()) {
            response["error"]="Processor name is empty";
            return response;
        }

        response=handle_request_run_process(processor_name,request["inputs"].toObject(),request["outputs"].toObject(),request["parameters"].toObject());
        return response;
    }
    else {
        response["error"]="Unknown action: "+action;
        return response;
    }

}

bool create_prv(QString fname_in,QString prv_fname_out) {
    QFile::remove(prv_fname_out);
    QString exe="prv";
    QStringList args; args << "create" << fname_in << prv_fname_out;
    QProcess pp;
    pp.execute(exe,args);
    return QFile::exists(prv_fname_out);
}

QString compute_unique_object_code(QJsonObject obj)
{
    QByteArray json = QJsonDocument(obj).toJson();
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(json);
    return QString(hash.result().toHex());
}


QJsonObject handle_request_run_process(QString processor_name,const QJsonObject &inputs,const QJsonObject &outputs,const QJsonObject &parameters) {
    QJsonObject response;
    response["success"]=false; //assume the worst

    ProcessManager *PM=ProcessManager::globalInstance();
    MLProcessor PP=PM->processor(processor_name);
    if (PP.name != processor_name) { //rather use PP.isNull()
        response["error"]="Unable to find processor: "+processor_name;
        return response;
    }

    QStringList args;
    args << "run-process" << processor_name;

    QStringList ikeys=inputs.keys();
    QStringList pkeys=parameters.keys();
    QStringList okeys=outputs.keys();
    foreach (QString key,PP.inputs.keys()) {
        if (!PP.inputs[key].optional) {
            if (!ikeys.contains(key)) {
                response["error"]="Missing input: "+key;
                return response;
            }
        }
    }
    foreach (QString key,PP.outputs.keys()) {
        if (!PP.outputs[key].optional) {
            if (!okeys.contains(key)) {
                response["error"]="Missing output: "+key;
                return response;
            }
        }
    }
    foreach (QString key,PP.parameters.keys()) {
        if (!PP.parameters[key].optional) {
            if (!pkeys.contains(key)) {
                response["error"]="Missing parameter: "+key;
                return response;
            }
        }
    }

    foreach (QString key,ikeys) {
        if (PP.inputs.contains(key)) {
            QJsonObject prv_object=inputs[key].toObject();
            QString path0=locate_prv(prv_object);
            if (path0.isEmpty()) {
                response["error"]="Unable to locate prv: "+key;
                return response;
            }
            args << QString("--%1=%2").arg(key).arg(path0);
        }
        else {
            response["error"]="Unexpected input: "+key;
            return response;
        }
    }

    foreach (QString key,pkeys) {
        if (PP.parameters.contains(key)) {
            args << QString("--%1=%2").arg(key).arg(parameters[key].toVariant().toString());
        }
        else {
            response["error"]="Unexpected parameter: "+key;
            return response;
        }
    }

    QJsonObject code_object;
    code_object["processor_name"]=processor_name;
    code_object["inputs"]=inputs;
    code_object["parameters"]=parameters;
    QString code = compute_unique_object_code(code_object).mid(0,10);

    QMap<QString,QString> output_files;
    foreach (QString key,okeys) {
        if (PP.outputs.contains(key)) {
            if (outputs[key].toBool()) {
                // TODO: make this file name depend on inputs so we don't need to recompute
                QString fname=CacheManager::globalInstance()->makeExpiringFile("output_%1_%2",60*60).arg(key).arg(code);
                args << QString("--%1=%2").arg(key).arg(fname);
                output_files[key]=fname;
            }
        }
        else {
            response["error"]="Unexpected output: "+key;
            return response;
        }
    }

    QString exe=qApp->applicationDirPath()+"/mountainprocess";

    QProcess pp;
    qDebug().noquote() << "Running: "+exe+" "+args.join(" ");
    int ret=pp.execute(exe,args);
    if (ret!=0) {
        response["error"]="Error running processor.";
        return response;
    }

    QJsonObject outputs0;
    foreach (QString key,okeys) {
        if (outputs[key].toBool()) {
            QString fname=output_files[key];
            if (!QFile::exists(fname)) {
                response["error"]="Output file does not exist: "+fname;
                return response;
            }
            QString tmp_prv=fname+".prv";
            if (!create_prv(fname,tmp_prv)) {
                response["error"]="Unable to create prv object for: "+fname;
                return response;
            }
            QString prv_json=TextFile::read(tmp_prv);
            if (prv_json.isEmpty()) {
                response["error"]="Problem creating prv object for: "+fname;
                return response;
            }
            QFile::remove(tmp_prv);
            outputs0[key]=QJsonDocument::fromJson(prv_json.toUtf8()).object();
        }
    }

    response["outputs"]=outputs0;
    response["success"]=true;
    return response;

}
