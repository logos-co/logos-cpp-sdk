#include "lidl_gen_client.h"
#include "lidl_parser.h"
#include "lidl_validator.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString lidlToPascalCase(const QString& name)
{
    QString out;
    bool cap = true;
    for (QChar c : name) {
        if (!c.isLetterOrNumber()) { cap = true; continue; }
        if (cap) { out.append(c.toUpper()); cap = false; }
        else { out.append(c.toLower()); }
    }
    if (out.isEmpty()) return QString("Module");
    return out;
}

QString lidlTypeToQt(const TypeExpr& te)
{
    switch (te.kind) {
    case TypeExpr::Primitive:
        if (te.name == "void")    return "void";
        if (te.name == "tstr")    return "QString";
        if (te.name == "bstr")    return "QByteArray";
        if (te.name == "int")     return "int";
        if (te.name == "uint")    return "int";
        if (te.name == "float64") return "double";
        if (te.name == "bool")    return "bool";
        if (te.name == "result")  return "LogosResult";
        if (te.name == "any")     return "QVariant";
        return "QVariant";
    case TypeExpr::Array:
        if (te.elements.size() == 1
            && te.elements[0].kind == TypeExpr::Primitive
            && te.elements[0].name == "tstr") {
            return "QStringList";
        }
        return "QVariantList";
    case TypeExpr::Map:
        return "QVariantMap";
    case TypeExpr::Optional:
        return "QVariant";
    case TypeExpr::Named:
        return "QVariant";
    }
    return "QVariant";
}

static bool isRefType(const QString& qt)
{
    return qt == "QString" || qt == "QStringList" || qt == "QJsonArray"
        || qt == "QVariantList" || qt == "QVariantMap" || qt == "QByteArray";
}

static void emitParam(QTextStream& s, const QString& qtType, const QString& name)
{
    if (isRefType(qtType))
        s << "const " << qtType << "& " << name;
    else
        s << qtType << " " << name;
}

static QString returnConversion(const QString& qt)
{
    if (qt == "bool")        return "return _result.toBool();";
    if (qt == "int")         return "return _result.toInt();";
    if (qt == "double")      return "return _result.toDouble();";
    if (qt == "float")       return "return _result.toFloat();";
    if (qt == "QString")     return "return _result.toString();";
    if (qt == "QStringList") return "return _result.toStringList();";
    if (qt == "QJsonArray")  return "return qvariant_cast<QJsonArray>(_result);";
    if (qt == "QVariantList") return "return _result.toList();";
    if (qt == "QVariantMap") return "return _result.toMap();";
    if (qt == "LogosResult") return "return _result.value<LogosResult>();";
    return "return _result;";
}

static QString asyncDefaultVal(const QString& qt)
{
    if (qt == "bool")        return "false";
    if (qt == "int" || qt == "double" || qt == "float") return "0";
    if (qt == "QString")     return "QString()";
    if (qt == "QStringList") return "QStringList()";
    if (qt == "QJsonArray")  return "QJsonArray()";
    if (qt == "QVariantList") return "QVariantList()";
    if (qt == "QVariantMap") return "QVariantMap()";
    return qt + "{}";
}

// ---------------------------------------------------------------------------
// Header generation
// ---------------------------------------------------------------------------

QString lidlMakeHeader(const ModuleDecl& module)
{
    QString className = lidlToPascalCase(module.name);
    QString h;
    QTextStream s(&h);

    s << "#pragma once\n";
    s << "#include <QString>\n";
    s << "#include <QVariant>\n";
    s << "#include <QStringList>\n";
    s << "#include <QJsonArray>\n";
    s << "#include <QVariantList>\n";
    s << "#include <QVariantMap>\n";
    s << "#include <functional>\n";
    s << "#include <utility>\n";
    s << "#include \"logos_types.h\"\n";
    s << "#include \"logos_api.h\"\n";
    s << "#include \"logos_api_client.h\"\n";
    s << "#include \"logos_object.h\"\n\n";

    s << "class " << className << " {\n";
    s << "public:\n";
    s << "    explicit " << className << "(LogosAPI* api);\n\n";

    s << "    using RawEventCallback = std::function<void(const QString&, const QVariantList&)>;\n";
    s << "    using EventCallback = std::function<void(const QVariantList&)>;\n\n";
    s << "    bool on(const QString& eventName, RawEventCallback callback);\n";
    s << "    bool on(const QString& eventName, EventCallback callback);\n";
    s << "    void setEventSource(LogosObject* source);\n";
    s << "    LogosObject* eventSource() const;\n";
    s << "    void trigger(const QString& eventName);\n";
    s << "    void trigger(const QString& eventName, const QVariantList& data);\n";
    s << "    template<typename... Args>\n";
    s << "    void trigger(const QString& eventName, Args&&... args) {\n";
    s << "        trigger(eventName, packVariantList(std::forward<Args>(args)...));\n";
    s << "    }\n";
    s << "    void trigger(const QString& eventName, LogosObject* source, const QVariantList& data);\n";
    s << "    template<typename... Args>\n";
    s << "    void trigger(const QString& eventName, LogosObject* source, Args&&... args) {\n";
    s << "        trigger(eventName, source, packVariantList(std::forward<Args>(args)...));\n";
    s << "    }\n\n";

    for (const MethodDecl& md : module.methods) {
        QString ret = lidlTypeToQt(md.returnType);
        s << "    " << ret << " " << md.name << "(";
        for (int i = 0; i < md.params.size(); ++i) {
            emitParam(s, lidlTypeToQt(md.params[i].type), md.params[i].name);
            if (i + 1 < md.params.size()) s << ", ";
        }
        s << ");\n";
        QString asyncCb = (ret == "void")
            ? QString("std::function<void()>")
            : QString("std::function<void(") + ret + ")>";
        s << "    void " << md.name << "Async(";
        for (int i = 0; i < md.params.size(); ++i) {
            emitParam(s, lidlTypeToQt(md.params[i].type), md.params[i].name);
            if (i + 1 < md.params.size()) s << ", ";
        }
        if (!md.params.isEmpty()) s << ", ";
        s << asyncCb << " callback, Timeout timeout = Timeout());\n";
    }

    s << "\nprivate:\n";
    s << "    LogosObject* ensureReplica();\n";
    s << "    template<typename... Args>\n";
    s << "    static QVariantList packVariantList(Args&&... args) {\n";
    s << "        QVariantList list;\n";
    s << "        list.reserve(sizeof...(Args));\n";
    s << "        using Expander = int[];\n";
    s << "        (void)Expander{0, (list.append(QVariant::fromValue(std::forward<Args>(args))), 0)...};\n";
    s << "        return list;\n";
    s << "    }\n";
    s << "    LogosAPI* m_api;\n";
    s << "    LogosAPIClient* m_client;\n";
    s << "    QString m_moduleName;\n";
    s << "    LogosObject* m_eventReplica = nullptr;\n";
    s << "    LogosObject* m_eventSource = nullptr;\n";
    s << "};\n";

    return h;
}

// ---------------------------------------------------------------------------
// Source generation
// ---------------------------------------------------------------------------

QString lidlMakeSource(const ModuleDecl& module)
{
    QString className = lidlToPascalCase(module.name);
    QString headerRel = module.name + "_api.h";
    QString c;
    QTextStream s(&c);

    s << "#include \"" << headerRel << "\"\n\n";
    s << "#include <QDebug>\n\n";

    s << className << "::" << className << "(LogosAPI* api) : m_api(api), m_client(api->getClient(\""
      << module.name << "\")), m_moduleName(QStringLiteral(\"" << module.name << "\")) {}\n\n";

    s << "LogosObject* " << className << "::ensureReplica() {\n";
    s << "    if (!m_eventReplica) {\n";
    s << "        LogosObject* replica = m_client->requestObject(m_moduleName);\n";
    s << "        if (!replica) {\n";
    s << "            qWarning() << \"" << className << ": failed to acquire remote object for events on\" << m_moduleName;\n";
    s << "            return nullptr;\n";
    s << "        }\n";
    s << "        m_eventReplica = replica;\n";
    s << "    }\n";
    s << "    return m_eventReplica;\n";
    s << "}\n\n";

    s << "bool " << className << "::on(const QString& eventName, RawEventCallback callback) {\n";
    s << "    if (!callback) { qWarning() << \"" << className << ": ignoring empty event callback for\" << eventName; return false; }\n";
    s << "    LogosObject* origin = ensureReplica();\n";
    s << "    if (!origin) return false;\n";
    s << "    m_client->onEvent(origin, eventName, callback);\n";
    s << "    return true;\n";
    s << "}\n\n";

    s << "bool " << className << "::on(const QString& eventName, EventCallback callback) {\n";
    s << "    if (!callback) { qWarning() << \"" << className << ": ignoring empty event callback for\" << eventName; return false; }\n";
    s << "    return on(eventName, [callback](const QString&, const QVariantList& data) { callback(data); });\n";
    s << "}\n\n";

    s << "void " << className << "::setEventSource(LogosObject* source) { m_eventSource = source; }\n\n";
    s << "LogosObject* " << className << "::eventSource() const { return m_eventSource; }\n\n";
    s << "void " << className << "::trigger(const QString& eventName) { trigger(eventName, QVariantList{}); }\n\n";

    s << "void " << className << "::trigger(const QString& eventName, const QVariantList& data) {\n";
    s << "    if (!m_eventSource) { qWarning() << \"" << className << ": no event source set for trigger\" << eventName; return; }\n";
    s << "    m_client->onEventResponse(m_eventSource, eventName, data);\n";
    s << "}\n\n";

    s << "void " << className << "::trigger(const QString& eventName, LogosObject* source, const QVariantList& data) {\n";
    s << "    if (!source) { qWarning() << \"" << className << ": cannot trigger\" << eventName << \"with null source\"; return; }\n";
    s << "    m_client->onEventResponse(source, eventName, data);\n";
    s << "}\n\n";

    for (const MethodDecl& md : module.methods) {
        QString ret = lidlTypeToQt(md.returnType);
        int nParams = md.params.size();

        s << ret << " " << className << "::" << md.name << "(";
        for (int i = 0; i < nParams; ++i) {
            emitParam(s, lidlTypeToQt(md.params[i].type), md.params[i].name);
            if (i + 1 < nParams) s << ", ";
        }
        s << ") {\n";

        if (ret != "void") s << "    QVariant _result = ";
        else                s << "    ";

        if (nParams <= 5) {
            s << "m_client->invokeRemoteMethod(\"" << module.name << "\", \"" << md.name << "\"";
            for (int i = 0; i < nParams; ++i)
                s << ", " << md.params[i].name;
            s << ");\n";
        } else {
            s << "m_client->invokeRemoteMethod(\"" << module.name << "\", \"" << md.name << "\", QVariantList{";
            for (int i = 0; i < nParams; ++i) {
                s << md.params[i].name;
                if (i + 1 < nParams) s << ", ";
            }
            s << "});\n";
        }

        if (ret != "void")
            s << "    " << returnConversion(ret) << "\n";
        s << "}\n\n";

        s << "void " << className << "::" << md.name << "Async(";
        for (int i = 0; i < nParams; ++i) {
            emitParam(s, lidlTypeToQt(md.params[i].type), md.params[i].name);
            if (i + 1 < nParams) s << ", ";
        }
        if (nParams > 0) s << ", ";
        s << "std::function<void(" << (ret == "void" ? "void" : ret) << ")> callback, Timeout timeout) {\n";
        s << "    if (!callback) return;\n";
        s << "    m_client->invokeRemoteMethodAsync(\"" << module.name << "\", \"" << md.name << "\", ";
        if (nParams == 0) {
            s << "QVariantList()";
        } else if (nParams == 1) {
            s << "QVariantList() << " << md.params[0].name;
        } else {
            s << "QVariantList{";
            for (int i = 0; i < nParams; ++i) {
                s << md.params[i].name;
                if (i + 1 < nParams) s << ", ";
            }
            s << "}";
        }
        s << ", [callback](QVariant v) {\n";
        if (ret == "void") {
            s << "        callback();\n";
        } else if (ret == "QVariant") {
            s << "        callback(v);\n";
        } else {
            s << "        callback(v.isValid() ? qvariant_cast<" << ret << ">(v) : " << asyncDefaultVal(ret) << ");\n";
        }
        s << "    }, timeout);\n";
        s << "}\n\n";
    }

    return c;
}

// ---------------------------------------------------------------------------
// metadata.json
// ---------------------------------------------------------------------------

QString lidlGenerateMetadataJson(const ModuleDecl& module)
{
    QJsonObject obj;
    obj["name"] = module.name;
    obj["version"] = module.version.isEmpty() ? "0.0.0" : module.version;
    obj["type"] = "core";
    obj["category"] = module.category.isEmpty() ? "general" : module.category;
    obj["description"] = module.description;
    obj["main"] = module.name + "_plugin";
    QJsonArray deps;
    for (const QString& d : module.depends)
        deps.append(d);
    obj["dependencies"] = deps;
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Indented);
}

// ---------------------------------------------------------------------------
// Full pipeline (from .lidl file)
// ---------------------------------------------------------------------------

int lidlGenerateClientStubs(const QString& lidlPath, const QString& outputDir,
                            bool moduleOnly, QTextStream& out, QTextStream& err)
{
    QFileInfo fi(lidlPath);
    if (!fi.exists()) { err << "LIDL file does not exist: " << lidlPath << "\n"; return 2; }
    QFile file(fi.canonicalFilePath().isEmpty() ? fi.absoluteFilePath() : fi.canonicalFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { err << "Failed to open LIDL file: " << lidlPath << "\n"; return 3; }
    QString source = QString::fromUtf8(file.readAll());
    file.close();

    LidlParseResult pr = lidlParse(source);
    if (pr.hasError()) { err << lidlPath << ":" << pr.errorLine << ":" << pr.errorColumn << ": " << pr.error << "\n"; return 4; }

    LidlValidationResult vr = lidlValidate(pr.module);
    if (vr.hasErrors()) { for (const QString& e : vr.errors) err << lidlPath << ": " << e << "\n"; return 5; }

    const ModuleDecl& mod = pr.module;
    QString genDirPath = outputDir.isEmpty() ? QDir::current().filePath("logos-cpp-sdk/cpp/generated") : outputDir;
    QDir().mkpath(genDirPath);

    QString headerAbs = QDir(genDirPath).filePath(mod.name + "_api.h");
    QString sourceAbs = QDir(genDirPath).filePath(mod.name + "_api.cpp");

    { QFile f(headerAbs); if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { err << "Failed to write: " << headerAbs << "\n"; return 6; } f.write(lidlMakeHeader(mod).toUtf8()); }
    { QFile f(sourceAbs); if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { err << "Failed to write: " << sourceAbs << "\n"; return 7; } f.write(lidlMakeSource(mod).toUtf8()); }
    { QString metaPath = QDir(genDirPath).filePath("metadata.json"); QFile f(metaPath); if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { err << "Failed to write: " << metaPath << "\n"; return 8; } f.write(lidlGenerateMetadataJson(mod).toUtf8()); }

    out << "Generated: " << headerAbs << " and " << sourceAbs << "\n";

    if (!moduleOnly) {
        QDir genDir(genDirPath);
        QStringList headers = genDir.entryList(QStringList() << "*_api.h", QDir::Files | QDir::Readable);
        { QString content; QTextStream ss(&content);
          ss << "#pragma once\n#include \"logos_api.h\"\n#include \"logos_api_client.h\"\n\n";
          for (const QString& h : headers) ss << "#include \"" << h << "\"\n";
          ss << "\nstruct LogosModules {\n    explicit LogosModules(LogosAPI* api) : api(api)";
          for (const QString& h : headers) { QString base = h; base.chop(6); ss << ", \n        " << base << "(api)"; }
          ss << " {}\n    LogosAPI* api;\n";
          for (const QString& h : headers) { QString base = h; base.chop(6); ss << "    " << lidlToPascalCase(base) << " " << base << ";\n"; }
          ss << "};\n";
          QFile f(genDir.filePath("logos_sdk.h")); if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { err << "Failed to write umbrella\n"; return 9; } f.write(content.toUtf8()); }
        { QStringList sources = genDir.entryList(QStringList() << "*_api.cpp", QDir::Files | QDir::Readable);
          QString content; QTextStream ss(&content); ss << "#include \"logos_sdk.h\"\n\n";
          for (const QString& c : sources) ss << "#include \"" << c << "\"\n"; ss << "\n";
          QFile f(genDir.filePath("logos_sdk.cpp")); if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { err << "Failed to write umbrella\n"; return 10; } f.write(content.toUtf8()); }
        out << "Generated: logos_sdk.h and logos_sdk.cpp\n";
    }
    out << "Generated: metadata.json\n";
    out.flush();
    return 0;
}
