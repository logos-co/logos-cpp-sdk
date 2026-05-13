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
        "void","bool","int","double","float","QString","QStringList","QJsonArray","QVariantList","QVariantMap","QVariant"
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
        "bool","int","double","float","QString","QStringList","QJsonArray","QVariantList","QVariantMap","QVariant","LogosResult"
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
    if (type == "QVariantList") return argExpr + ".toList()";
    if (type == "QVariantMap") return argExpr + ".toMap()";
    if (type == "QVariant") return argExpr;
    if (type == "LogosResult") return argExpr + ".value<LogosResult>()";
    return argExpr + ".toString()";
}

// ─── Std (pure-C++) type-mapping helpers ─────────────────────────────────
//
// File-local — not exposed in generator_lib.h. The single public entry
// point is makeHeader / makeSource taking an `ApiStyle` argument; when
// `apiStyle == Std`, those functions internally route through these
// helpers to pick the std type-mapping table. There's only one wrapper
// class per module — `<Module>` — whose signatures depend on apiStyle.
// The std-typed body still goes through the QVariant wire; the Qt↔std
// conversion is inlined at the call site, contained to the generated
// .cpp so callers never include Qt headers.

static QString mapParamTypeStd(const QString& qtType)
{
    const QString base = mapParamType(qtType);
    if (base == "QString")      return "std::string";
    if (base == "QStringList")  return "std::vector<std::string>";
    if (base == "QJsonArray")   return "LogosList";
    if (base == "QVariantList") return "LogosList";
    if (base == "QVariantMap")  return "LogosMap";
    if (base == "QVariant")     return "LogosMap";
    if (base == "int")          return "int64_t";
    return base;
}

static QString mapReturnTypeStd(const QString& qtType)
{
    const QString base = mapReturnType(qtType);
    if (base == "void")         return "void";
    if (base == "QString")      return "std::string";
    if (base == "QStringList")  return "std::vector<std::string>";
    if (base == "QJsonArray")   return "LogosList";
    if (base == "QVariantList") return "LogosList";
    if (base == "QVariantMap")  return "LogosMap";
    if (base == "QVariant")     return "LogosMap";
    if (base == "LogosResult")  return "StdLogosResult";
    if (base == "int")          return "int64_t";
    return base;
}

// Returns a C++ expression that lifts a std-typed parameter into a
// QVariant-side temporary suitable for invokeRemoteMethod. The
// temporaries are rvalues consumed inline at the call site.
static QString stdParamToQVariant(const QString& qtType, const QString& argName)
{
    const QString base = mapParamType(qtType);
    if (base == "QString")
        return "QString::fromStdString(" + argName + ")";
    if (base == "QStringList")
        return "[&]{ QStringList _q; _q.reserve(static_cast<int>(" + argName +
               ".size())); for (const auto& _s : " + argName +
               ") _q.append(QString::fromStdString(_s)); return _q; }()";
    if (base == "QJsonArray")
        return "QJsonDocument::fromJson(QByteArray::fromStdString(" + argName +
               ".dump())).array()";
    if (base == "QVariantList" || base == "QVariantMap" || base == "QVariant")
        return "QJsonDocument::fromJson(QByteArray::fromStdString(" + argName +
               ".dump())).toVariant()" + (base == "QVariantList" ? ".toList()"
                                       : base == "QVariantMap"  ? ".toMap()"
                                       : "");
    if (base == "int")
        // The std signature exposes `int64_t`; widen the QVariant
        // payload to qlonglong so the wire carries the full 64-bit
        // value instead of silently truncating to 32 bits on the way
        // through `static_cast<int>`. (Reported by Copilot review on
        // PR #61 — the std-typed surface and the wire payload were
        // disagreeing for any value outside the int32 range.)
        return "static_cast<qlonglong>(" + argName + ")";
    return argName;
}

// Returns a C++ expression that converts a QVariant return value into
// the std-typed return type. `varExpr` is the source QVariant.
static QString qVariantToStdReturn(const QString& qtType, const QString& varExpr)
{
    const QString base = mapReturnType(qtType);
    if (base == "void")
        return QString();
    if (base == "bool")
        return varExpr + ".toBool()";
    if (base == "int")
        return "static_cast<int64_t>(" + varExpr + ".toInt())";
    if (base == "double" || base == "float")
        return varExpr + ".toDouble()";
    if (base == "QString")
        return varExpr + ".toString().toStdString()";
    if (base == "QStringList")
        return "[&]{ std::vector<std::string> _v; const QStringList _q = " +
               varExpr + ".toStringList(); _v.reserve(static_cast<size_t>(_q.size())); "
               "for (const QString& _s : _q) _v.push_back(_s.toStdString()); return _v; }()";
    if (base == "QJsonArray" || base == "QVariantList")
        return "LogosList::parse(QJsonDocument(QJsonArray::fromVariantList(" +
               varExpr + ".toList())).toJson(QJsonDocument::Compact).toStdString())";
    if (base == "QVariantMap" || base == "QVariant")
        return "LogosMap::parse(QJsonDocument(QJsonObject::fromVariantMap(" +
               varExpr + ".toMap())).toJson(QJsonDocument::Compact).toStdString())";
    if (base == "LogosResult")
        return "[&]{ StdLogosResult _r; const LogosResult _q = " + varExpr +
               ".value<LogosResult>(); _r.success = _q.success; "
               "if (_q.value.isValid()) _r.value = LogosMap::parse("
               "QJsonDocument(QJsonObject::fromVariantMap(_q.value.toMap())).toJson(QJsonDocument::Compact).toStdString()); "
               "_r.error = _q.error.toString().toStdString(); return _r; }()";
    return varExpr + ".toString().toStdString()";
}

// Param-type predicate: passed by const-ref?
static bool isStdRefType(const QString& t)
{
    return t == "std::string" || t.startsWith("std::vector")
        || t == "LogosMap" || t == "LogosList";
}

static bool isQtRefType(const QString& t)
{
    // Matches the pre-refactor Qt-style by-ref set exactly. `QByteArray`
    // and `LogosResult` are intentionally NOT included — the original
    // generator emitted those parameter types by value, and the goal
    // of routing existing Qt-style wrappers through this predicate is
    // to keep the generated signatures bit-for-bit unchanged. Adding
    // them to the set would have broken downstream code that took
    // the address-of, overloaded on the parameter type, or relied on
    // the by-value signature in shipped headers. (Reported by Copilot
    // review on PR #61.)
    return t == "QString" || t == "QStringList"
        || t == "QJsonArray" || t == "QVariantList" || t == "QVariantMap";
}

QString makeHeader(const QString& moduleName, const QString& className, const QJsonArray& methods, ApiStyle apiStyle, const QJsonArray& events)
{
    QString h;
    QTextStream s(&h);
    s << "#pragma once\n";
    if (apiStyle == ApiStyle::Std) {
        // Pure-C++ surface. Qt is still pulled in transitively by
        // logos_api.h (LogosAPI is a QObject), but the wrapper's
        // signatures are entirely std types so callers never have to
        // type a Qt name themselves.
        s << "#include <cstdint>\n";
        s << "#include <string>\n";
        s << "#include <vector>\n";
        s << "#include <functional>\n";
        s << "#include \"logos_types.h\"\n";
        s << "#include \"logos_json.h\"\n";
        s << "#include \"logos_result.h\"\n";
        s << "#include \"logos_api.h\"\n";
        s << "#include \"logos_api_client.h\"\n";
        // Needed for the m_eventReplica member when the module declares
        // any events. Cheap to include unconditionally — keeps the
        // header symmetric with the Qt-style branch.
        if (!events.isEmpty()) s << "#include \"logos_object.h\"\n";
        s << "\n";
    } else {
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
    }
    s << "class " << className << " {\n";
    s << "public:\n";
    s << "    explicit " << className << "(LogosAPI* api);\n\n";
    if (apiStyle == ApiStyle::Qt) {
        // Event subscription / trigger surface — Qt-typed.
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
    }
    // Typed event subscribers — generated from the `.lidl` sidecar shipped
    // with the dep's pre-built headers (via --events-from). One typed
    // adapter per declared event, callback-arg types follow apiStyle.
    // The generic `on(name, cb)` channel above stays available; for
    // std-style consumers it's not exposed but the typed accessors are.
    for (const QJsonValue& ev : events) {
        const QJsonObject eo = ev.toObject();
        const QString evName = eo.value("name").toString();
        if (evName.isEmpty()) continue;
        // `on` + capitalized event name. `evName` is the verbatim name
        // the impl declared in its `logos_events:` block (typically
        // camelCase, e.g. `userLoggedIn`), so we just uppercase its
        // first letter — `toPascalCase` would clobber the internal
        // camelCase boundaries (snake_case input is its target).
        QString cap = evName;
        if (!cap.isEmpty()) cap[0] = cap[0].toUpper();
        const QString accessorName = QString("on") + cap;
        const QJsonArray evParams = eo.value("params").toArray();

        // Build the callback's parameter list using apiStyle's type table.
        QString cbParams;
        for (int i = 0; i < evParams.size(); ++i) {
            const QJsonObject p = evParams.at(i).toObject();
            QString qtPt = p.value("type").toString();
            QString pt = (apiStyle == ApiStyle::Std)
                ? mapParamTypeStd(qtPt) : mapParamType(qtPt);
            bool byRef = (apiStyle == ApiStyle::Std) ? isStdRefType(pt) : isQtRefType(pt);
            if (byRef) cbParams += "const " + pt + "& ";
            else       cbParams += pt + " ";
            cbParams += p.value("name").toString();
            if (i + 1 < evParams.size()) cbParams += ", ";
        }
        s << "    bool " << accessorName
          << "(std::function<void(" << cbParams << ")> callback);\n";
    }
    if (!events.isEmpty()) s << "\n";
    // Methods
    for (const QJsonValue& v : methods) {
        const QJsonObject o = v.toObject();
        const bool invokable = o.value("isInvokable").toBool();
        if (!invokable) continue;
        const QString name = o.value("name").toString();
        const QString qtRet = o.value("returnType").toString();
        const QString ret = (apiStyle == ApiStyle::Std)
            ? mapReturnTypeStd(qtRet) : mapReturnType(qtRet);
        s << "    " << ret << " " << name << "(";
        QJsonArray params = o.value("parameters").toArray();
        for (int i = 0; i < params.size(); ++i) {
            QJsonObject p = params.at(i).toObject();
            QString qtPt = p.value("type").toString();
            QString pt = (apiStyle == ApiStyle::Std)
                ? mapParamTypeStd(qtPt) : mapParamType(qtPt);
            QString pn = p.value("name").toString();
            bool byRef = (apiStyle == ApiStyle::Std) ? isStdRefType(pt) : isQtRefType(pt);
            if (byRef) s << "const " << pt << "& " << pn;
            else       s << pt << " " << pn;
            if (i + 1 < params.size()) s << ", ";
        }
        s << ");\n";
        // Async overload: same params + callback + optional Timeout
        QString asyncCallbackType = (ret == "void")
            ? QString("std::function<void()>")
            : QString("std::function<void(") + ret + ")>";
        s << "    void " << name << "Async(";
        for (int i = 0; i < params.size(); ++i) {
            QJsonObject p = params.at(i).toObject();
            QString qtPt = p.value("type").toString();
            QString pt = (apiStyle == ApiStyle::Std)
                ? mapParamTypeStd(qtPt) : mapParamType(qtPt);
            QString pn = p.value("name").toString();
            bool byRef = (apiStyle == ApiStyle::Std) ? isStdRefType(pt) : isQtRefType(pt);
            if (byRef) s << "const " << pt << "& " << pn;
            else       s << pt << " " << pn;
            if (i + 1 < params.size()) s << ", ";
        }
        if (params.size() > 0) s << ", ";
        s << asyncCallbackType << " callback, Timeout timeout = Timeout());\n";
    }
    s << "\nprivate:\n";
    // ensureReplica() is needed whenever the wrapper subscribes to
    // events — in Qt mode that's always (the generic `on(...)` channel
    // is exposed); in std mode it's gated on at least one declared
    // event in the LIDL sidecar.
    if (apiStyle == ApiStyle::Qt || !events.isEmpty()) {
        s << "    LogosObject* ensureReplica();\n";
    }
    if (apiStyle == ApiStyle::Qt) {
        s << "    template<typename... Args>\n";
        s << "    static QVariantList packVariantList(Args&&... args) {\n";
        s << "        QVariantList list;\n";
        s << "        list.reserve(sizeof...(Args));\n";
        s << "        using Expander = int[];\n";
        s << "        (void)Expander{0, (list.append(QVariant::fromValue(std::forward<Args>(args))), 0)...};\n";
        s << "        return list;\n";
        s << "    }\n";
    }
    s << "    LogosAPI* m_api;\n";
    s << "    LogosAPIClient* m_client;\n";
    s << "    QString m_moduleName;\n";
    if (apiStyle == ApiStyle::Qt) {
        s << "    LogosObject* m_eventReplica = nullptr;\n";
        s << "    LogosObject* m_eventSource = nullptr;\n";
    } else if (!events.isEmpty()) {
        // std-style consumer: only the receive-side replica is needed
        // (no `trigger(...)` API on the std wrapper, so no eventSource).
        s << "    LogosObject* m_eventReplica = nullptr;\n";
    }
    s << "};\n";
    return h;
}

QString makeSource(const QString& moduleName, const QString& className, const QString& headerBaseName, const QJsonArray& methods, ApiStyle apiStyle, const QJsonArray& events)
{
    QString c;
    QTextStream s(&c);
    s << "#include \"" << headerBaseName << "\"\n\n";
    s << "#include <QDebug>\n";
    if (apiStyle == ApiStyle::Std) {
        // Conversion bridges between std types and QVariant — confined
        // to this .cpp so the caller's translation unit doesn't need
        // any Qt headers itself.
        s << "#include <QJsonDocument>\n";
        s << "#include <QJsonArray>\n";
        s << "#include <QJsonObject>\n";
        s << "#include <QByteArray>\n";
        s << "#include <QStringList>\n";
        s << "#include <QVariantList>\n";
        s << "#include <QVariantMap>\n";
        // logos_object.h is the type of LogosObject* used by typed event
        // accessors when the module declares events. Always include in
        // std mode when events are present.
        if (!events.isEmpty()) s << "#include \"logos_object.h\"\n";
    }
    s << "\n";
    s << className << "::" << className << "(LogosAPI* api) : m_api(api), m_client(api->getClient(\"" << moduleName << "\")), m_moduleName(QStringLiteral(\"" << moduleName << "\")) {}\n\n";

    // ensureReplica() — generated for std mode too when events are
    // declared. The body is identical to the Qt version; pulled up
    // here so both branches share it.
    if (apiStyle == ApiStyle::Std && !events.isEmpty()) {
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
    }
    if (apiStyle == ApiStyle::Qt) {
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
    }

    // Typed event adapters — one per declared event. The callback type
    // uses the apiStyle's type surface; the body unmarshals from the
    // wire's QVariantList into typed args and invokes the user's
    // callback. Subscription uses the same `m_client->onEvent` channel
    // the generic `on(...)` uses.
    for (const QJsonValue& ev : events) {
        const QJsonObject eo = ev.toObject();
        const QString evName = eo.value("name").toString();
        if (evName.isEmpty()) continue;
        // `on` + capitalized event name. `evName` is the verbatim name
        // the impl declared in its `logos_events:` block (typically
        // camelCase, e.g. `userLoggedIn`), so we just uppercase its
        // first letter — `toPascalCase` would clobber the internal
        // camelCase boundaries (snake_case input is its target).
        QString cap = evName;
        if (!cap.isEmpty()) cap[0] = cap[0].toUpper();
        const QString accessorName = QString("on") + cap;
        const QJsonArray evParams = eo.value("params").toArray();

        // Callback signature
        QString cbParams;
        for (int i = 0; i < evParams.size(); ++i) {
            const QJsonObject p = evParams.at(i).toObject();
            QString qtPt = p.value("type").toString();
            QString pt = (apiStyle == ApiStyle::Std)
                ? mapParamTypeStd(qtPt) : mapParamType(qtPt);
            bool byRef = (apiStyle == ApiStyle::Std) ? isStdRefType(pt) : isQtRefType(pt);
            if (byRef) cbParams += "const " + pt + "& ";
            else       cbParams += pt + " ";
            cbParams += p.value("name").toString();
            if (i + 1 < evParams.size()) cbParams += ", ";
        }
        s << "bool " << className << "::" << accessorName
          << "(std::function<void(" << cbParams << ")> callback) {\n";
        s << "    if (!callback) {\n";
        s << "        qWarning() << \"" << className << ": ignoring empty event callback for\" "
          << "<< QStringLiteral(\"" << evName << "\");\n";
        s << "        return false;\n";
        s << "    }\n";
        s << "    LogosObject* origin = ensureReplica();\n";
        s << "    if (!origin) return false;\n";
        s << "    m_client->onEvent(origin, QStringLiteral(\"" << evName << "\"), "
          << "[callback](const QString&, const QVariantList& _args) {\n";
        s << "        if (_args.size() < " << evParams.size() << ") return;\n";
        s << "        callback(";
        for (int i = 0; i < evParams.size(); ++i) {
            const QJsonObject p = evParams.at(i).toObject();
            QString qtPt = p.value("type").toString();
            // Build the QVariant → typed-arg conversion expression.
            const QString argExpr = QString("_args.at(%1)").arg(i);
            QString conv;
            if (apiStyle == ApiStyle::Std) {
                conv = qVariantToStdReturn(qtPt, argExpr);
            } else {
                conv = toQVariantConversion(mapParamType(qtPt), argExpr);
            }
            s << conv;
            if (i + 1 < evParams.size()) s << ", ";
        }
        s << ");\n";
        s << "    });\n";
        s << "    return true;\n";
        s << "}\n\n";
    }

    for (const QJsonValue& v : methods) {
        const QJsonObject o = v.toObject();
        const bool invokable = o.value("isInvokable").toBool();
        if (!invokable) continue;
        const QString name = o.value("name").toString();
        const QString qtRet = o.value("returnType").toString();
        const QString ret = (apiStyle == ApiStyle::Std)
            ? mapReturnTypeStd(qtRet) : mapReturnType(qtRet);
        QJsonArray params = o.value("parameters").toArray();

        // Helper closures kept inline so the two branches don't get
        // pulled apart visually — the per-arg / per-return shape is
        // the only thing that varies between Qt and Std modes.
        auto emitParam = [&](const QJsonObject& p, bool& byRefOut) {
            QString qtPt = p.value("type").toString();
            QString pt = (apiStyle == ApiStyle::Std)
                ? mapParamTypeStd(qtPt) : mapParamType(qtPt);
            QString pn = p.value("name").toString();
            byRefOut = (apiStyle == ApiStyle::Std) ? isStdRefType(pt) : isQtRefType(pt);
            if (byRefOut) s << "const " << pt << "& " << pn;
            else          s << pt << " " << pn;
        };
        auto wireArg = [&](const QJsonObject& p) -> QString {
            QString qtPt = p.value("type").toString();
            QString pn = p.value("name").toString();
            return (apiStyle == ApiStyle::Std) ? stdParamToQVariant(qtPt, pn) : pn;
        };

        // Signature
        s << ret << " " << className << "::" << name << "(";
        for (int i = 0; i < params.size(); ++i) {
            bool byRef;
            emitParam(params.at(i).toObject(), byRef);
            if (i + 1 < params.size()) s << ", ";
        }
        s << ") {\n";

        // Body: perform call
        if (ret != "void") s << "    QVariant _result = ";
        else               s << "    ";

        if (params.size() == 0) {
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\");\n";
        } else if (params.size() <= 5) {
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\"";
            for (int i = 0; i < params.size(); ++i) {
                s << ", " << wireArg(params.at(i).toObject());
            }
            s << ");\n";
        } else {
            s << "m_client->invokeRemoteMethod(\"" << moduleName << "\", \"" << name << "\", QVariantList{";
            for (int i = 0; i < params.size(); ++i) {
                s << wireArg(params.at(i).toObject());
                if (i + 1 < params.size()) s << ", ";
            }
            s << "});\n";
        }

        // Return conversion
        if (ret == "void") {
            // nothing
        } else if (apiStyle == ApiStyle::Std) {
            s << "    return " << qVariantToStdReturn(qtRet, "_result") << ";\n";
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
        } else if (ret == "QVariantList") {
            s << "    return _result.toList();\n";
        } else if (ret == "QVariantMap") {
            s << "    return _result.toMap();\n";
        } else if (ret == "LogosResult") {
            s << "    return _result.value<LogosResult>();\n";
        } else { // QVariant
            s << "    return _result;\n";
        }
        s << "}\n\n";

        // Async implementation
        s << "void " << className << "::" << name << "Async(";
        for (int i = 0; i < params.size(); ++i) {
            bool byRef;
            emitParam(params.at(i).toObject(), byRef);
            if (i + 1 < params.size()) s << ", ";
        }
        if (params.size() > 0) s << ", ";
        s << "std::function<void(" << (ret == "void" ? "void" : ret) << ")> callback, Timeout timeout) {\n";
        s << "    if (!callback) return;\n";
        s << "    m_client->invokeRemoteMethodAsync(\"" << moduleName << "\", \"" << name << "\", ";
        if (params.size() == 0) {
            s << "QVariantList()";
        } else {
            s << "QVariantList{";
            for (int i = 0; i < params.size(); ++i) {
                s << wireArg(params.at(i).toObject());
                if (i + 1 < params.size()) s << ", ";
            }
            s << "}";
        }
        s << ", [callback](QVariant v) {\n";
        if (ret == "void") {
            s << "        (void)v; callback();\n";
        } else if (apiStyle == ApiStyle::Std) {
            // Default-construct on dispatch failure, matching the
            // existing Qt code path which falls back to a zero / empty
            // value when the QVariant is invalid.
            QString defaultVal;
            if (ret == "bool")                       defaultVal = "false";
            else if (ret == "int64_t")               defaultVal = "0";
            else if (ret == "double")                defaultVal = "0.0";
            else if (ret == "std::string")           defaultVal = "std::string()";
            else if (ret.startsWith("std::vector"))  defaultVal = ret + "()";
            else if (ret == "LogosMap")              defaultVal = "LogosMap::object()";
            else if (ret == "LogosList")             defaultVal = "LogosList::array()";
            else if (ret == "StdLogosResult")        defaultVal = "StdLogosResult{}";
            else                                     defaultVal = ret + "{}";
            s << "        if (!v.isValid()) { callback(" << defaultVal << "); return; }\n";
            s << "        callback(" << qVariantToStdReturn(qtRet, "v") << ");\n";
        } else {
            QString defaultVal;
            if (ret == "bool") defaultVal = "false";
            else if (ret == "int" || ret == "double" || ret == "float") defaultVal = "0";
            else if (ret == "QString") defaultVal = "QString()";
            else if (ret == "QStringList") defaultVal = "QStringList()";
            else if (ret == "QJsonArray") defaultVal = "QJsonArray()";
            else if (ret == "QVariantList") defaultVal = "QVariantList()";
            else if (ret == "QVariantMap") defaultVal = "QVariantMap()";
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

