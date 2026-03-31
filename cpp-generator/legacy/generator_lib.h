#ifndef GENERATOR_LIB_H
#define GENERATOR_LIB_H

#include <QString>
#include <QJsonArray>
#include <QTextStream>
#include <QVector>
#include <QPair>

struct ParsedMethod {
    QString returnType;
    QString name;
    QVector<QPair<QString, QString>> params; // (type, name)
};

QString toPascalCase(const QString& name);
QString normalizeType(QString t);
QString mapParamType(const QString& qtType);
QString mapReturnType(const QString& qtType);
QString toQVariantConversion(const QString& type, const QString& argExpr);
QString makeHeader(const QString& moduleName, const QString& className, const QJsonArray& methods);
QString makeSource(const QString& moduleName, const QString& className, const QString& headerBaseName, const QJsonArray& methods);
QVector<ParsedMethod> parseProviderHeader(const QString& headerPath, QTextStream& err);

#endif // GENERATOR_LIB_H
