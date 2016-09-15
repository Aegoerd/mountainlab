/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 7/6/2016
*******************************************************/

#include "mlcommon.h"
#include "cachemanager/cachemanager.h"
#include "taskprogress/taskprogress.h"

#include <QFile>
#include <QTextStream>
#include <QTime>
#include <QThread>
#include <QCoreApplication>
#include <QUrl>
#include <QDir>
#include <QCryptographicHash>
#include <math.h>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QJsonArray>
#include <QSettings>

#ifdef QT_GUI_LIB
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QMessageBox>
#endif

QString TextFile::read(const QString& fname, QTextCodec* codec)
{
    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream ts(&file);
    if (codec != 0)
        ts.setCodec(codec);
    QString ret = ts.readAll();
    file.close();
    return ret;
}

bool TextFile::write(const QString& fname, const QString& txt, QTextCodec* codec)
{
    /*
     * Modification on 5/23/16 by jfm
     * We don't want an program to try to read this while we have only partially completed writing the file.
     * Therefore we now create a temporary file and then copy it over
     */

    QString tmp_fname = fname + ".tf." + MLUtil::makeRandomId(6) + ".tmp";

    //if a file with this name already exists, we need to remove it
    //(should we really do this before testing whether writing is successful? I think yes)
    if (QFile::exists(fname)) {
        if (!QFile::remove(fname)) {
            qWarning() << "Problem in TextFile::write. Could not remove file even though it exists" << fname;
            return false;
        }
    }

    //write text to temporary file
    QFile file(tmp_fname);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Problem in TextFile::write. Could not open for writing... " << tmp_fname;
        return false;
    }
    QTextStream ts(&file);
    if (codec != 0) {
        ts.setAutoDetectUnicode(false);
        ts.setCodec(codec);
    }
    ts << txt;
    ts.flush();
    file.close();

    //check the contents of the file (is this overkill?)
    QString txt_test = TextFile::read(tmp_fname, codec);
    if (txt_test != txt) {
        QFile::remove(tmp_fname);
        qWarning() << "Problem in TextFile::write. The contents of the file do not match what was expected." << fname;
        return false;
    }

    //finally, rename the file
    if (!QFile::rename(tmp_fname, fname)) {
        qWarning() << "Problem in TextFile::write. Unable to rename file at the end of the write command" << fname;
        return false;
    }

    return true;
}

QChar make_random_alphanumeric()
{
    static int val = 0;
    val++;
    QTime time = QTime::currentTime();
    QString code = time.toString("hh:mm:ss:zzz");
    code += QString::number(qrand() + val);
    code += QString::number(QCoreApplication::applicationPid());
    code += QString::number((long)QThread::currentThreadId());
    int num = qHash(code);
    if (num < 0)
        num = -num;
    num = num % 36;
    if (num < 26)
        return QChar('A' + num);
    else
        return QChar('0' + num - 26);
}
QString make_random_id_22(int numchars)
{
    QString ret;
    for (int i = 0; i < numchars; i++) {
        ret.append(make_random_alphanumeric());
    }
    return ret;
}

QString MLUtil::makeRandomId(int numchars)
{
    return make_random_id_22(numchars);
}

bool MLUtil::threadInterruptRequested()
{
    return QThread::currentThread()->isInterruptionRequested();
}

bool MLUtil::inGuiThread()
{
    return (QThread::currentThread() == QCoreApplication::instance()->thread());
}

QString find_ancestor_path_with_name(QString path, QString name)
{
    if (name.isEmpty())
        return "";
    while (QFileInfo(path).fileName() != name) {
        path = QFileInfo(path).path();
        if (!path.contains(name))
            return ""; //guarantees that we eventually exit the while loop
    }
    return path; //the directory name must equal the name argument
}

QString MLUtil::mountainlabBasePath()
{
    return find_ancestor_path_with_name(qApp->applicationDirPath(), "mountainlab");
}

void mkdir_if_doesnt_exist(const QString& path)
{
    if (!QDir(path).exists()) {
        QDir(QFileInfo(path).path()).mkdir(QFileInfo(path).fileName());
    }
}

QString MLUtil::mlLogPath()
{
    QString ret = mountainlabBasePath() + "/log";
    mkdir_if_doesnt_exist(ret);
    return ret;
}

QString MLUtil::resolvePath(const QString& basepath, const QString& path)
{
    if (QFileInfo(path).isRelative()) {
        return basepath + "/" + path;
    }
    else
        return path;
}

void MLUtil::mkdirIfNeeded(const QString& path)
{
    mkdir_if_doesnt_exist(path);
}

#include "sumit.h"
QString MLUtil::computeSha1SumOfFile(const QString& path)
{
    return sumit(path);
    /*
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return "";
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    file.close();
    QString ret = QString(hash.result().toHex());
    return ret;
    */
}

static QString s_temp_path = "";
QString MLUtil::tempPath()
{
    if (!s_temp_path.isEmpty())
        return s_temp_path;

    QString tmp = MLUtil::configResolvedPath("", "temporary_path");
    if (!QDir(tmp).mkpath("mountainlab")) {
        qCritical() << "Unable to create temporary directory: " + tmp + "/mountainlab";
        abort();
    }

    s_temp_path = tmp + "/mountainlab";
    return s_temp_path;
}

QVariant clp_string_to_variant(const QString& str);

CLParams::CLParams(int argc, char* argv[])
{
    this->success = true; //let's be optimistic!

    //find the named and unnamed parameters checking for errors along the way
    for (int i = 1; i < argc; i++) {
        QString str = QString(argv[i]);
        if (str.startsWith("--")) {
            int ind2 = str.indexOf("=");
            QString name = str.mid(2, ind2 - 2);
            QString val = "";
            if (ind2 >= 0)
                val = str.mid(ind2 + 1);
            if (name.isEmpty()) {
                this->success = false;
                this->error_message = "Problem with parameter: " + str;
                return;
            }
            this->named_parameters[name] = clp_string_to_variant(val);
        }
        else {
            this->unnamed_parameters << str;
        }
    }
}

bool clp_is_int(const QString& str)
{
    bool ok;
    str.toInt(&ok);
    return ok;
}

bool clp_is_float(const QString& str)
{
    bool ok;
    str.toFloat(&ok);
    return ok;
}

QVariant clp_string_to_variant(const QString& str)
{
    if (clp_is_int(str))
        return str.toInt();
    if (clp_is_float(str))
        return str.toFloat();
    return str;
}

double MLCompute::min(const QVector<double>& X)
{
    return *std::min_element(X.constBegin(), X.constEnd());
}

double MLCompute::max(const QVector<double>& X)
{
    return *std::max_element(X.constBegin(), X.constEnd());
}

double MLCompute::sum(const QVector<double>& X)
{
    return std::accumulate(X.constBegin(), X.constEnd(), 0.0);
}

double MLCompute::mean(const QVector<double>& X)
{
    if (X.isEmpty())
        return 0;
    double s = sum(X);
    return s / X.count();
}

double MLCompute::stdev(const QVector<double>& X)
{
    double sumsqr = std::inner_product(X.constBegin(), X.constEnd(), X.constBegin(), 0.0);
    double sum = std::accumulate(X.constBegin(), X.constEnd(), 0.0);
    int ct = X.count();
    if (ct >= 2) {
        return sqrt((sumsqr - sum * sum / ct) / (ct - 1));
    }
    else
        return 0;
}

double MLCompute::dotProduct(const QVector<double>& X1, const QVector<double>& X2)
{
    if (X1.count() != X2.count())
        return 0;
    return std::inner_product(X1.constBegin(), X1.constEnd(), X2.constBegin(), 0.0);
}

double MLCompute::norm(const QVector<double>& X)
{
    return sqrt(dotProduct(X, X));
}

double MLCompute::correlation(const QVector<double>& X1, const QVector<double>& X2)
{
    if (X1.count() != X2.count())
        return 0;
    long N = X1.count();
    double mean1 = mean(X1);
    double stdev1 = stdev(X1);
    double mean2 = mean(X2);
    double stdev2 = stdev(X2);
    if ((stdev1 == 0) || (stdev2 == 0))
        return 0;
    QVector<double> Y1(N);
    QVector<double> Y2(N);
    for (long i = 0; i < N; i++) {
        Y1[i] = (X1[i] - mean1) / stdev1;
        Y2[i] = (X2[i] - mean2) / stdev2;
    }
    return dotProduct(Y1, Y2);
}

double MLCompute::norm(long N, const double* X)
{
    return sqrt(dotProduct(N, X, X));
}

double MLCompute::dotProduct(long N, const double* X1, const double* X2)
{
    return std::inner_product(X1, X1 + N, X2, 0.0);
}

double MLCompute::dotProduct(long N, const float* X1, const float* X2)
{

    return std::inner_product(X1, X1 + N, X2, 0.0);
}

QString MLUtil::computeSha1SumOfString(const QString& str)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(str.toLatin1());
    return QString(hash.result().toHex());
}

double MLCompute::sum(long N, const double* X)
{
    return std::accumulate(X, X + N, 0.0);
}

double MLCompute::mean(long N, const double* X)
{
    if (!N)
        return 0;
    return sum(N, X) / N;
}

double MLCompute::max(long N, const double* X)
{
    return N ? *std::max_element(X, X + N) : 0;
}

double MLCompute::min(long N, const double* X)
{
    return N ? *std::min_element(X, X + N) : 0;
}

QList<int> MLUtil::stringListToIntList(const QStringList& list)
{
    QList<int> ret;
    ret.reserve(list.size());
    foreach (QString str, list) {
        ret << str.toInt();
    }
    return ret;
}

QStringList MLUtil::intListToStringList(const QList<int>& list)
{
    QStringList ret;
    ret.reserve(list.size());
    foreach (int a, list) {
        ret << QString::number(a);
    }
    return ret;
}

void MLUtil::fromJsonValue(QByteArray& X, const QJsonValue& val)
{
    X = QByteArray::fromBase64(val.toString().toLatin1());
}

void MLUtil::fromJsonValue(QList<int>& X, const QJsonValue& val)
{
    X.clear();
    QByteArray ba;
    MLUtil::fromJsonValue(ba, val);
    QDataStream ds(&ba, QIODevice::ReadOnly);
    while (!ds.atEnd()) {
        int val;
        ds >> val;
        X << val;
    }
}

void MLUtil::fromJsonValue(QVector<int>& X, const QJsonValue& val)
{
    X.clear();
    QByteArray ba;
    MLUtil::fromJsonValue(ba, val);
    QDataStream ds(&ba, QIODevice::ReadOnly);
    while (!ds.atEnd()) {
        int val;
        ds >> val;
        X << val;
    }
}

void MLUtil::fromJsonValue(QVector<double>& X, const QJsonValue& val)
{
    X.clear();
    QByteArray ba;
    MLUtil::fromJsonValue(ba, val);
    QDataStream ds(&ba, QIODevice::ReadOnly);
    while (!ds.atEnd()) {
        double val;
        ds >> val;
        X << val;
    }
}

QJsonValue MLUtil::toJsonValue(const QByteArray& X)
{
    return QString(X.toBase64());
}

QJsonValue MLUtil::toJsonValue(const QList<int>& X)
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    for (int i = 0; i < X.count(); i++) {
        ds << X[i];
    }
    return toJsonValue(ba);
}

QJsonValue MLUtil::toJsonValue(const QVector<int>& X)
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    for (int i = 0; i < X.count(); i++) {
        ds << X[i];
    }
    return toJsonValue(ba);
}

QJsonValue MLUtil::toJsonValue(const QVector<double>& X)
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    for (int i = 0; i < X.count(); i++) {
        ds << X[i];
    }
    return toJsonValue(ba);
}

QByteArray MLUtil::readByteArray(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    QByteArray ret = file.readAll();
    file.close();
    return ret;
}

bool MLUtil::writeByteArray(const QString& path, const QByteArray& X)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to open file for writing byte array: " + path;
        return false;
    }
    if (file.write(X) != X.count()) {
        qWarning() << "Problem writing byte array: " + path;
        return false;
    }
    file.close();
    return true;
}

double MLCompute::min(long N, const float* X)
{
    return N ? *std::min_element(X, X + N) : 0;
}

double MLCompute::max(long N, const float* X)
{
    return N ? *std::max_element(X, X + N) : 0;
}

double MLCompute::sum(long N, const float* X)
{
    return std::accumulate(X, X + N, 0.0);
}

double MLCompute::mean(long N, const float* X)
{
    if (!N)
        return 0;
    return sum(N, X) / N;
}

double MLCompute::norm(long N, const float* X)
{
    return sqrt(dotProduct(N, X, X));
}

double MLCompute::median(const QVector<double>& X)
{
    if (X.isEmpty())
        return 0;
    QVector<double> Y = X;
    qSort(Y);
    if (Y.count() % 2 == 0) {
        return (Y[Y.count() / 2 - 1] + Y[Y.count() / 2]) / 2;
    }
    else {
        return Y[Y.count() / 2];
    }
}

/////////////////////////////////////////////////////////////////////////////

QString find_file_with_checksum(QString dirpath, QString checksum, long size_bytes, bool recursive)
{
    QStringList list = QDir(dirpath).entryList(QStringList("*"), QDir::Files, QDir::Name);
    foreach (QString fname, list) {
        QString path0 = dirpath + "/" + fname;
        if (QFileInfo(path0).size() == size_bytes) {
            QString checksum1 = MLUtil::computeSha1SumOfFile(path0);
            if (checksum1 == checksum) {
                return path0;
            }
        }
    }
    if (recursive) {
        QStringList list2 = QDir(dirpath).entryList(QStringList("*"), QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        foreach (QString foldername, list2) {
            QString tmp = find_file_with_checksum(dirpath + "/" + foldername, checksum, size_bytes, recursive);
            if (!tmp.isEmpty())
                return tmp;
        }
    }
    return "";
}

QString find_file_with_checksum(const QString& checksum, long size_bytes)
{
    QStringList bigfile_paths = MLUtil::configResolvedPathList("mountainprocess", "bigfile_paths");

    //first search in the big files
    foreach (QString bp, bigfile_paths) {
        QString path = find_file_with_checksum(bp, checksum, size_bytes, true);
        if (!path.isEmpty())
            return path;
    }

    //next search in the current directory (recursively)
    {
        QString path = find_file_with_checksum(".", checksum, size_bytes, true);
        if (!path.isEmpty())
            return path;
    }

    //next try in the temporary directories
    {
        QString path = find_file_with_checksum(CacheManager::globalInstance()->localTempPath() + "/tmp_long_term", checksum, size_bytes, false);
        if (!path.isEmpty())
            return path;
    }

    {
        QString path = find_file_with_checksum(CacheManager::globalInstance()->localTempPath() + "/tmp_short_term", checksum, size_bytes, false);
        if (!path.isEmpty())
            return path;
    }

    return "";
}

QString make_temporary_output_file_name(QString processor_name, QMap<QString, QVariant> args_inputs, QMap<QString, QVariant> args_parameters, QString output_pname)
{
    QJsonObject tmp;
    tmp["processor_name"] = processor_name;
    tmp["inputs"] = QJsonObject::fromVariantMap(args_inputs);
    tmp["parameters"] = QJsonObject::fromVariantMap(args_parameters);
    tmp["output_pname"] = output_pname;
    QString tmp_json = QJsonDocument(tmp).toJson();
    QString code = MLUtil::computeSha1SumOfString(tmp_json);
    return CacheManager::globalInstance()->makeLocalFile(code + "-prv-" + output_pname);
}

void run_process(QString processor_name, QMap<QString, QVariant> inputs, QMap<QString, QVariant> outputs, QMap<QString, QVariant> parameters, bool force_run)
{
    QStringList args;
    args << "run-process" << processor_name;
    {
        QStringList keys = inputs.keys();
        foreach (QString key, keys) {
            args << QString("--%1=%2").arg(key).arg(inputs[key].toString());
        }
    }
    {
        QStringList keys = outputs.keys();
        foreach (QString key, keys) {
            args << QString("--%1=%2").arg(key).arg(outputs[key].toString());
        }
    }
    {
        QStringList keys = parameters.keys();
        foreach (QString key, keys) {
            args << QString("--%1=%2").arg(key).arg(parameters[key].toString());
        }
    }
    if (force_run) {
        args << "--_force_run";
    }
    QString exe = MLUtil::mountainlabBasePath() + "/mountainprocess/bin/mountainprocess";
    qDebug() << "Running process:" << args.join(" ");
    QProcess::execute(exe, args);
}

QString create_file_from_prv(QString output_name, QString checksum0, long size0, const QJsonArray& processes)
{
    printf("Creating output corresponding to %s\n", output_name.toLatin1().data());
    QString path1 = find_file_with_checksum(checksum0, size0);
    if (!path1.isEmpty()) {
        printf("Found file with checksum %s\n", checksum0.toLatin1().data());
        return path1;
    }
    for (int i = 0; i < processes.count(); i++) {
        QJsonObject process0 = processes[i].toObject();

        QString processor_name = process0["processor_name"].toString();
        QJsonObject outputs = process0["outputs"].toObject();
        QJsonObject inputs = process0["inputs"].toObject();
        QJsonObject parameters = process0["parameters"].toObject();
        QStringList input_pnames = inputs.keys();
        QStringList output_pnames = outputs.keys();
        QStringList parameter_pnames = parameters.keys();

        foreach (QString opname, output_pnames) {
            if (outputs[opname].toObject()["original_path"].toString() == output_name) {
                QMap<QString, QVariant> args_inputs;
                QMap<QString, QVariant> args_outputs;
                QMap<QString, QVariant> args_parameters;

                foreach (QString ipname, input_pnames) {
                    QJsonObject input0 = inputs[ipname].toObject();
                    QString name0 = input0["original_path"].toString();
                    QString checksum0 = input0["original_checksum"].toString();
                    long size0 = input0["original_size"].toVariant().toLongLong();
                    QString path0 = create_file_from_prv(name0, checksum0, size0, processes);
                    if (path0.isEmpty())
                        return "";
                    args_inputs[ipname] = path0;
                }

                foreach (QString ppname, parameter_pnames) {
                    args_parameters[ppname] = parameters[ppname].toVariant(); //important to do it this way instead of toString
                }

                foreach (QString opname2, output_pnames) {
                    args_outputs[opname2] = make_temporary_output_file_name(processor_name, args_inputs, args_parameters, opname2);
                }

                //should we always force_run here? (I guess so)
                run_process(processor_name, args_inputs, args_outputs, args_parameters, true);
                QString output_path = args_outputs[opname].toString();
                if (!QFile::exists(output_path)) {
                    qWarning() << "Output file does not exist after running process: " + output_path;
                    return "";
                }
                return output_path;
            }
        }
    }
    return "";
}

QString resolve_prv_file(const QString& prv_fname)
{
    QString json = TextFile::read(prv_fname);
    QJsonParseError err;
    QJsonObject obj = QJsonDocument::fromJson(json.toLatin1(), &err).object();
    if (err.error != QJsonParseError::NoError) {
        qDebug() << json;
        qWarning() << "Error parsing json." << err.errorString();
        return "";
    }
    QString path0 = obj["original_path"].toString();
    QString checksum0 = obj["original_checksum"].toString();
    long size0 = obj["original_size"].toVariant().toLongLong();
    QString path2 = create_file_from_prv(path0, checksum0, size0, obj["processes"].toArray());
    if (!path2.isEmpty()) {
        return path2;
    }
    qWarning() << "Unable to resolve: " + prv_fname;
    return "";
}

bool resolve_prv_files(QMap<QString, QVariant>& command_line_params)
{
    QStringList keys = command_line_params.keys();
    foreach (QString key, keys) {
        QVariant val = command_line_params[key];
        if ((!QFile::exists(val.toString())) && (QFile::exists(val.toString() + ".prv"))) {
            val = val.toString() + ".prv";
            command_line_params[key] = val;
        }
        if (val.toString().endsWith(".prv")) {
            QString fname = val.toString();
            val = resolve_prv_file(fname);
            if (val.toString().isEmpty()) {
                qWarning() << "Error resolving .prv file: " + fname;
                return false;
            }
            else {
                printf("Resolved: %s --> %s\n", fname.toLatin1().data(), val.toString().toLatin1().data());
            }
            command_line_params[key] = val;
        }
    }
    return true;
}

QVariant MLUtil::configValue(const QString& group, const QString& key)
{
    QSettings settings(MLUtil::mountainlabBasePath() + "/mountainlab.ini", QSettings::IniFormat);
    QSettings settings_default(MLUtil::mountainlabBasePath() + "/mountainlab.ini.default", QSettings::IniFormat);
    if (!group.isEmpty()) {
        settings.beginGroup(group);
        settings_default.beginGroup(group);
    }
    if (settings.contains(key))
        return settings.value(key);
    else
        return settings_default.value(key);
}

QString MLUtil::configResolvedPath(const QString& group, const QString& key)
{
    QString ret = MLUtil::configValue(group, key).toString();
    return QDir(MLUtil::mountainlabBasePath()).filePath(ret);
}

QStringList MLUtil::configResolvedPathList(const QString& group, const QString& key)
{
    QStringList vals = MLUtil::configValue(group, key).toString().split(";", QString::SkipEmptyParts);
    QStringList ret;
    foreach (QString val, vals) {
        ret << QDir(MLUtil::mountainlabBasePath()).filePath(val);
    }
    return ret;
}
