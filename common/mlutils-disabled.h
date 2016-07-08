/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 5/11/2016
*******************************************************/

#ifndef MLUTILS_H
#define MLUTILS_H

#include <QString>
#include <QDebug>

QString cfp(const QString& path); //cannonical file path
QString compute_checksum_of_file(const QString& path);
QString mountainlabBasePath();
QString MLUtil::tempPath();
QString mlLogPath();
QString mlConfigPath();
void mkdir_if_doesnt_exist(const QString& path);
QString resolve_path(QString basepath, QString path);
bool in_gui_thread();
bool MLUtil::threadInterruptRequested();
QString MLNetwork::httpGetText_curl(const QString& url);
QString MLNetwork::httpGetText(const QString& url);
QString MLNetwork::httpGetBinaryFile(const QString& url);
//QString get_path_query(const QString &path);
//QString get_path_without_query(const QString &path);
//QMap<QString,QString> parse_query(const QString &query);
//QMap<QString,QString> parse_path_query(const QString &path);

#endif // MLUTILS_H
