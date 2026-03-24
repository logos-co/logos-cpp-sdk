#include "generator_lib.h"

#include <QFile>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

QString toPascalCase(const QString& name)
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

QString normalizeType(QString t)
{
    t = t.trimmed();
    if (t.startsWith("const ")) t = t.mid(6);
    t = t.trimmed();
    // Drop reference and pointer qualifiers
    if (t.endsWith('&') || t.endsWith('*')) t.chop(1);
    t = t.trimmed();
    return t;
}

QString mapParamType(const QString& qtType)
{
    const QString base = normalizeType(qtType);
    static const QSet<QString> known = {
        "void","bool","int","double","float","QString","QStringList","QJsonArray","QVariant"
    };
    if (known.contains(base)) return base;
    // Fallback to QVariant for unknown types
    return QString("QVariant");
}

QString mapReturnType(const QString& qtType)
{
    const QString base = normalizeType(qtType);
    if (base.isEmpty() || base == "void") return QString("void");
    static const QSet<QString> known = {
        "bool","int","double","float","QString","QStringList","QJsonArray","QVariant","LogosResult"
    };
    if (known.contains(base)) return base;
    return QString("QVariant");
}

QString toQVariantConversion(const QString& type, const QString& argExpr)
{
    if (type == "int") return argExpr + ".toInt()";
    if (type == "bool") return argExpr + ".toBool()";
    if (type == "double") return argExpr + ".toDouble()";
    if (type == "float") return argExpr + ".toFloat()";
    if (type == "QString") return argExpr + ".toString()";
    if (type == "QStringList") return argExpr + ".toStringList()";
    if (type == "QJsonArray") return "qvariant_cast<QJsonArray>(" + argExpr + ")";
    if (type == "QVariant") return argExpr;
    if (type == "LogosResult") return argExpr + ".value<LogosResult>()";
    return argExpr + ".toString()";
}

QString makeHeader(const QString& moduleName, const QString& className, const QJsonArray& methods)
{
    QString h;
    QTextStream s(&h);
    s << "#pragma once\n";
    s << "#include <QString>\n";
    s << "#include <QVariant>\n";
    s << "#include <QStringList>\n";
    s << "#include <QJsonArray>\n";
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
    // Methods
    for (const QJsonValue& v : methods) {
        const QJsonObject o = v.toObject();
        const bool invokable = o.value("isInvokable").toBool();
        if (!invokable) continue;
        const QString name = o.value("name").toString();
        const QString ret = mapReturnType(o.value("returnType").toString());
        s << "    " << ret << " " << name << "(";
        QJsonArray params = o.value("parameters").toArray();
        for (int i = 0; i < params.size(); ++i) {
            QJsonObject p = params.at(i).toObject();
            QString pt = mapParamType(p.value("type").toString());
            QString pn = p.value("name").toString();
            if (pt == "QString" || pt == "QStringList" || pt == "QJsonArray") {
                s << "const " << pt << "& " << pn;
            } else {
                s << pt << " " << pn;
            }
            if (i + 1 < params.size()) s << ", ";
        }
        s << ");\n";
        // Async overload: same params + callback + optional Timeout
        QString asyncCallbackType = (ret == "void") ? QString("std::function<void()>") : QString("std::function<void(") + ret + ")>";
        s << "    void " << name << "Async(";
        for (int i = 0; i < params.size(); ++i) {
            QJsonObject p = params.at(i).toObject();
            QString pt = mapParamType(p.value("type").toString());
            QString pn = p.value("name").toString();
            if (pt == "QString" || pt == "QStringList" || pt == "QJsonArray") {
                s << "const " << pt << "& " << pn;
            } else {
                s << pt << " " << pn;
            }
            if (i + 1 < params.size()) s << ", ";
        }
        if (params.size() > 0) s << ", ";
        s << asyncCallbackType << " callback, Timeout timeout = Timeout());\n";
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

QString makeSource(const QString& moduleName, const QString& className, const QString& headerBaseName, const QJsonArray& methods)
{
    QString c;
    QTextStream s(&c);
    s << "#include \"" << headerBaseName << "\"\n\n";
    s << "#include <QDebug>\n\n";
    s << className << "::" << className << "(LogosAPI* api) : m_api(api), m_client(api->getClient(\"" << moduleName << "\")), m_moduleName(QStringLiteral(\"" << moduleName << "\")) {}\n\n";
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
    s << "    if (!callback) {\n";
    s << "        qWarning() << \"" << className << ": ignoring empty event callback for\" << eventName;\n";
    s << "        return false;\n";
    s << "    }\n";
    s << "    LogosObject* origin = ensureReplica();\n";
    s << "    if (!origin) {\n";
    s << "        return false;\n";
    s << "    }\n";
    s << "    m_client->onEvent(origin, eventName, callback);\n";
    s << "    return true;\n";
    s << "}\n\n";
    s << "bool " << className << "::on(const QString& eventName, EventCallback callback) {\n";
    s << "    if (!callback) {\n";
    s << "        qWarning() << \"" << className << ": ignoring empty event callback for\" << eventName;\n";
    s << "        return false;\n";
    s << "    }\n";
    s << "    return on(eventName, [callback](const QString&, const QVariantList& data) {\n";
    s << "        callback(data);\n";
    s << "    });\n";
    s << "}\n\n";
    s << "void " << className << "::setEventSource(LogosObject* source) {\n";
    s << "    m_eventSource = source;\n";
    s << "}\n\n";
    s << "LogosObject* " << className << "::eventSource() const {\n";
    s << "    return m_eventSource;\n";
    s << "}\n\n";
    s << "void " << className << "::trigger(const QString& eventName) {\n";
    s << "    trigger(eventName, QVariantList{});\n";
    s << "}\n\n";
    s << "void " << className << "::trigger(const QString& eventName, const QVariantList& data) {\n";
    s << "    if (!m_eventSource) {\n";
    s << "        qWarning() << \"" << className << ": no event source set for trigger\" << eventName;\n";
    s << "        return;\n";
    s << "    }\n";
    s << "    m_client->onEventResponse(m_eventSource, eventName, data);\n";
    s << "}\n\n";
    s << "void " << className << "::trigger(const QString& eventName, LogosObject* source, const QVariantList& data) {\n";
    s << "    if (!source) {\n";
    s << "        qWarning() << \"" << className << ": cannot trigger\" << eventName << \"with null source\";\n";
    s << "        return;\n";
    s << "    }\n";
    s << "    m_client->onEventResponse(source, eventName, data);\n";
    s << "}\n\n";
    for (const QJsonValue& v : methods) {
        const QJsonObject o = v.toObject();
        const bool invokable = o.value("isInvokable").toBool();
        if (!invokable) continue;
        const QString name = o.value("name").toString();
        const QString ret = mapReturnType(o.value("returnType").toString());
        QJsonArray params = o.value("parameters").toArray();
        // Signature
        s << ret << " " << className << "::" << name << "(";
        for (int i = 0; i < params.size(); ++i) {
            QJsonObject p = params.at(i).toObject();
            QString pt = mapParamType(p.value("type").toString());
            QString pn = p.value("name").toString();
            if (pt == "QString" || pt == "QStringList" || pt == "QJsonArray") {
                s << "const " << pt << "& " << pn;
            } else {
                s << pt << " " << pn;
            }
            if (i + 1 < params.size()) s << ", ";
        }
        s << ") {\n";
        // Body: perform call
        if (ret != "void") {
            s << "    QVariant _result = ";
        } else {
            s << "    ";
        }
        if (params.size() == 0) {
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\");\n";
        } else if (params.size() == 1) {
            QJsonObject p = params.at(0).toObject();
            QString pn = p.value("name").toString();
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\", " << pn << ");\n";
        } else if (params.size() == 2) {
            QString p0 = params.at(0).toObject().value("name").toString();
            QString p1 = params.at(1).toObject().value("name").toString();
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\", " << p0 << ", " << p1 << ");\n";
        } else if (params.size() == 3) {
            QString p0 = params.at(0).toObject().value("name").toString();
            QString p1 = params.at(1).toObject().value("name").toString();
            QString p2 = params.at(2).toObject().value("name").toString();
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\", " << p0 << ", " << p1 << ", " << p2 << ");\n";
        } else if (params.size() == 4) {
            QString p0 = params.at(0).toObject().value("name").toString();
            QString p1 = params.at(1).toObject().value("name").toString();
            QString p2 = params.at(2).toObject().value("name").toString();
            QString p3 = params.at(3).toObject().value("name").toString();
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\", " << p0 << ", " << p1 << ", " << p2 << ", " << p3 << ");\n";
        } else if (params.size() == 5) {
            QString p0 = params.at(0).toObject().value("name").toString();
            QString p1 = params.at(1).toObject().value("name").toString();
            QString p2 = params.at(2).toObject().value("name").toString();
            QString p3 = params.at(3).toObject().value("name").toString();
            QString p4 = params.at(4).toObject().value("name").toString();
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\", " << p0 << ", " << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ");\n";
        } else {
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\", QVariantList{";
            for (int i = 0; i < params.size(); ++i) {
                QString pn = params.at(i).toObject().value("name").toString();
                s << pn;
                if (i + 1 < params.size()) s << ", ";
            }
            s << "});\n";
        }
        // Return conversion
        if (ret == "void") {
            // nothing
        } else if (ret == "bool") {
            s << "    return _result.toBool();\n";
        } else if (ret == "int") {
            s << "    return _result.toInt();\n";
        } else if (ret == "double") {
            s << "    return _result.toDouble();\n";
        } else if (ret == "float") {
            s << "    return _result.toFloat();\n";
        } else if (ret == "QString") {
            s << "    return _result.toString();\n";
        } else if (ret == "QStringList") {
            s << "    return _result.toStringList();\n";
        } else if (ret == "QJsonArray") {
            s << "    return qvariant_cast<QJsonArray>(_result);\n";
        }else if (ret == "LogosResult") {
            s << "    return _result.value<LogosResult>();\n";
        } else { // QVariant
            s << "    return _result;\n";
        }
        s << "}\n\n";
        // Async implementation
        s << "void " << className << "::" << name << "Async(";
        for (int i = 0; i < params.size(); ++i) {
            QJsonObject p = params.at(i).toObject();
            QString pt = mapParamType(p.value("type").toString());
            QString pn = p.value("name").toString();
            if (pt == "QString" || pt == "QStringList" || pt == "QJsonArray") {
                s << "const " << pt << "& " << pn;
            } else {
                s << pt << " " << pn;
            }
            if (i + 1 < params.size()) s << ", ";
        }
        if (params.size() > 0) s << ", ";
        s << "std::function<void(" << (ret == "void" ? "void" : ret) << ")> callback, Timeout timeout) {\n";
        s << "    if (!callback) return;\n";
        s << "    m_client->invokeRemoteMethodAsync(\"" << moduleName << "\", \"" << name << "\", ";
        if (params.size() == 0) {
            s << "QVariantList()";
        } else if (params.size() == 1) {
            s << "QVariantList() << " << params.at(0).toObject().value("name").toString();
        } else {
            s << "QVariantList{";
            for (int i = 0; i < params.size(); ++i) {
                s << params.at(i).toObject().value("name").toString();
                if (i + 1 < params.size()) s << ", ";
            }
            s << "}";
        }
        s << ", [callback](QVariant v) {\n";
        if (ret == "void") {
            s << "        callback();\n";
        } else {
            QString defaultVal;
            if (ret == "bool") defaultVal = "false";
            else if (ret == "int" || ret == "double" || ret == "float") defaultVal = "0";
            else if (ret == "QString") defaultVal = "QString()";
            else if (ret == "QStringList") defaultVal = "QStringList()";
            else if (ret == "QJsonArray") defaultVal = "QJsonArray()";
            else defaultVal = ret + "{}";
            if (ret == "QVariant") {
                s << "        callback(v);\n";
            } else {
                s << "        callback(v.isValid() ? qvariant_cast<" << ret << ">(v) : " << defaultVal << ");\n";
            }
        }
        s << "    }, timeout);\n";
        s << "}\n\n";
    }
    return c;
}

QVector<ParsedMethod> parseProviderHeader(const QString& headerPath, QTextStream& err)
{
    QVector<ParsedMethod> methods;

    QFile file(headerPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err << "Cannot open header file: " << headerPath << "\n";
        return methods;
    }

    QTextStream in(&file);
    QRegularExpression re(
        R"(^\s*LOGOS_METHOD\s+(.+?)\s+(\w+)\s*\(([^)]*)\)\s*;)"
    );

    while (!in.atEnd()) {
        QString line = in.readLine();
        auto match = re.match(line);
        if (!match.hasMatch()) continue;

        ParsedMethod m;
        m.returnType = normalizeType(match.captured(1));
        m.name = match.captured(2);

        QString paramStr = match.captured(3).trimmed();
        if (!paramStr.isEmpty()) {
            QStringList paramParts = paramStr.split(',');
            for (const QString& part : paramParts) {
                QString trimmed = part.trimmed();
                int eqIdx = trimmed.indexOf('=');
                if (eqIdx > 0) trimmed = trimmed.left(eqIdx).trimmed();
                int lastSpace = trimmed.lastIndexOf(' ');
                int lastAmp = trimmed.lastIndexOf('&');
                int splitAt = qMax(lastSpace, lastAmp);
                if (splitAt > 0) {
                    QString type = normalizeType(trimmed.left(splitAt + 1));
                    QString pname = trimmed.mid(splitAt + 1).trimmed();
                    m.params.append({type, pname});
                } else {
                    m.params.append({normalizeType(trimmed), QString("arg%1").arg(m.params.size())});
                }
            }
        }

        methods.append(m);
    }

    file.close();
    return methods;
}
