
#include "mvfile.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include "textfile.h"
#include "mlutils.h"

class MVFilePrivate {
public:
    MVFile* q;

    QJsonObject m_obj;
    QString m_path;
};

MVFile::MVFile()
{
    d = new MVFilePrivate;
    d->q = this;
    d->m_obj["mvfile_version"] = "0.1";
    d->m_obj=other.d->m_obj;
    d->m_path=other.d->m_path;
}

MVFile::MVFile(const MVFile &other)
{
    d = new MVFilePrivate;
    d->q = this;
}

MVFile::~MVFile()
{
    delete d;
}

void MVFile::operator=(const MVFile &other)
{
    d->m_obj=other.d->m_obj;
    d->m_path=other.d->m_path;
}

bool MVFile::read(const QString& path)
{
    QString json = read_text_file(path);
    if (json.isEmpty())
        return false;
    bool ret = this->setJson(json);
    d->m_path = path;
    emit resultsChanged();
    return ret;
}

bool MVFile::write(const QString& path)
{
    return write_text_file(path, this->json());
}

bool MVFile::load(const QString& url)
{
    QString json = http_get_text_curl(url);
    bool ret = setJson(json);
    d->m_path = url;
    emit resultsChanged();
    return ret;
}

QString MVFile::path() const
{
    return d->m_path;
}

bool MVFile::setJson(const QString& json)
{
    QJsonParseError err;
    d->m_obj = QJsonDocument::fromJson(json.toLatin1(), &err).object();
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "Problem parsing json in OFile::setJson";
        return false;
    }
    return true;
}

QString MVFile::json() const
{
    return QJsonDocument(d->m_obj).toJson();
}

double MVFile::sampleRate() const
{
    return d->m_obj["samplerate"].toDouble();
}

QString MVFile::mlproxyUrl() const
{
    return d->m_obj["mlproxy_url"].toString();
}

QString MVFile::basePath() const
{
    return d->m_obj["basepath"].toString();
}

QString MVFile::firingsPath() const
{
    return d->m_obj["firings"].toString();
}

QStringList MVFile::timeseriesNames() const
{
    QStringList ret;
    QJsonArray ts = d->m_obj["timeseries"].toArray();
    for (int i = 0; i < ts.count(); i++) {
        QJsonObject tsobj = ts[i].toObject();
        QString name = tsobj["name"].toString();
        ret << name;
    }
    return ret;
}

QString MVFile::timeseriesPath(const QString& name) const
{
    QJsonArray ts = d->m_obj["timeseries"].toArray();
    for (int i = 0; i < ts.count(); i++) {
        QJsonObject tsobj = ts[i].toObject();
        QString name0 = tsobj["name"].toString();
        if (name0 == name) {
            QString path = tsobj["path"].toString();
            return path;
        }
    }
    return "";
}

QJsonObject MVFile::viewOptions() const
{
    return d->m_obj["view_options"].toObject();
}

QJsonObject MVFile::eventFilter() const
{
    return d->m_obj["event_filter"].toObject();
}

QJsonObject MVFile::annotations() const
{
    return d->m_obj["annotations"].toObject();
}

void MVFile::setFiringsPath(QString path)
{
    d->m_obj["firings"] = path;
}

void MVFile::setSampleRate(double rate)
{
    d->m_obj["samplerate"] = rate;
}

void MVFile::setMlproxyUrl(QString url)
{
    d->m_obj["mlproxy_url"] = url;
}

void MVFile::addTimeseriesPath(const QString& name, const QString& path)
{
    QJsonArray ts = d->m_obj["timeseries"].toArray();
    QJsonObject obj;
    obj["name"] = name;
    obj["path"] = path;
    ts.append(obj);
    d->m_obj["timeseries"] = ts;
}

void MVFile::setViewOptions(QJsonObject obj)
{
    d->m_obj["view_options"] = obj;
}

void MVFile::setEventFilter(QJsonObject obj)
{
    d->m_obj["event_filter"] = obj;
}

void MVFile::setAnnotations(QJsonObject obj)
{
    d->m_obj["annotations"] = obj;
}

QString MVFile::mvfile_version() const
{
    return d->m_obj["mvfile_version"].toString();
}
