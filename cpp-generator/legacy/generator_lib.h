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

// Which type surface to expose on the generated per-module wrapper.
// Each module's build picks ONE — there's no composite output. Default
// is Qt for backward compatibility; `interface: "universal"` modules
// flip to Std via the -DLOGOS_API_STYLE=std CMake flag the module
// builder threads through.
enum class ApiStyle { Qt, Std };

QString toPascalCase(const QString& name);
QString normalizeType(QString t);
QString mapParamType(const QString& qtType);
QString mapReturnType(const QString& qtType);
QString toQVariantConversion(const QString& type, const QString& argExpr);

// makeHeader / makeSource emit the single `<Class>` wrapper for a
// module. When `apiStyle == Std`, parameter / return types come from
// the std-typed mapping table (std::string / std::vector<std::string>
// / LogosMap / LogosList / int64_t / StdLogosResult) and the .cpp
// body wraps the QVariant wire with inline Qt↔std conversions —
// callers never include Qt headers. When `apiStyle == Qt`, the output
// matches the legacy Qt-typed surface (QString / QStringList /
// QVariantList / QVariantMap / int / LogosResult). The class name is
// always `<Module>` either way; the two styles are mutually exclusive.
//
// `events` carries typed event prototypes loaded from a `.lidl`
// sidecar via --events-from. Each entry is
//   { "name": "<event>", "params": [ { "name": "...", "type": "<QtTypeName>" } ] }
// (Qt-typed names — same surface methods come through). When non-empty,
// the wrapper also gets one `on<EventName>(callback)` accessor per
// event next to the existing generic `onEvent(name, callback)` channel.
// The accessor signature uses the apiStyle's type surface for the
// callback's argument types.
QString makeHeader(const QString& moduleName, const QString& className, const QJsonArray& methods, ApiStyle apiStyle = ApiStyle::Qt, const QJsonArray& events = {});
QString makeSource(const QString& moduleName, const QString& className, const QString& headerBaseName, const QJsonArray& methods, ApiStyle apiStyle = ApiStyle::Qt, const QJsonArray& events = {});
QVector<ParsedMethod> parseProviderHeader(const QString& headerPath, QTextStream& err);

#endif // GENERATOR_LIB_H
