#include "lidl_gen_provider.h"
#include "lidl_gen_client.h"  // lidlToPascalCase, lidlTypeToQt
#include "lidl_parser.h"
#include "lidl_validator.h"
#include "c_header_parser.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

// ---------------------------------------------------------------------------
// Type mapping: LIDL → C++ std types
// ---------------------------------------------------------------------------

bool lidlIsStdConvertible(const TypeExpr& te)
{
    if (te.kind == TypeExpr::Primitive) {
        return te.name == "tstr" || te.name == "bstr"
            || te.name == "int" || te.name == "uint"
            || te.name == "float64" || te.name == "bool";
    }
    if (te.kind == TypeExpr::Array && te.elements.size() == 1) {
        const TypeExpr& elem = te.elements[0];
        if (elem.kind == TypeExpr::Primitive) {
            return elem.name == "tstr" || elem.name == "bstr"
                || elem.name == "int" || elem.name == "uint"
                || elem.name == "float64" || elem.name == "bool";
        }
    }
    return false;
}

QString lidlTypeToStd(const TypeExpr& te)
{
    if (te.kind == TypeExpr::Primitive) {
        if (te.name == "tstr")    return "std::string";
        if (te.name == "bstr")    return "std::vector<uint8_t>";
        if (te.name == "int")     return "int64_t";
        if (te.name == "uint")    return "uint64_t";
        if (te.name == "float64") return "double";
        if (te.name == "bool")    return "bool";
        if (te.name == "result")  return "LogosResult";
        if (te.name == "any")     return "QVariant";
        return "QVariant";
    }
    if (te.kind == TypeExpr::Array && te.elements.size() == 1) {
        const TypeExpr& elem = te.elements[0];
        if (elem.kind == TypeExpr::Primitive) {
            if (elem.name == "tstr")    return "std::vector<std::string>";
            if (elem.name == "bstr")    return "std::vector<std::vector<uint8_t>>";
            if (elem.name == "int")     return "std::vector<int64_t>";
            if (elem.name == "uint")    return "std::vector<uint64_t>";
            if (elem.name == "float64") return "std::vector<double>";
            if (elem.name == "bool")    return "std::vector<bool>";
        }
        return "QVariantList";
    }
    if (te.kind == TypeExpr::Map)      return "QVariantMap";
    if (te.kind == TypeExpr::Optional) return "QVariant";
    if (te.kind == TypeExpr::Named)    return "QVariant";
    return "QVariant";
}

// ---------------------------------------------------------------------------
// Conversion helpers: Qt type ↔ std type
// ---------------------------------------------------------------------------

static QString qtParamToStd(const TypeExpr& te, const QString& paramName)
{
    if (!lidlIsStdConvertible(te))
        return paramName;

    if (te.kind == TypeExpr::Primitive) {
        if (te.name == "tstr")    return paramName + ".toStdString()";
        if (te.name == "bstr")    return "std::vector<uint8_t>(" + paramName + ".begin(), " + paramName + ".end())";
        if (te.name == "int")     return "static_cast<int64_t>(" + paramName + ")";
        if (te.name == "uint")    return "static_cast<uint64_t>(" + paramName + ")";
        return paramName;
    }
    if (te.kind == TypeExpr::Array && te.elements.size() == 1) {
        const TypeExpr& elem = te.elements[0];
        if (elem.kind == TypeExpr::Primitive && elem.name == "tstr")
            return "lidlToStdStringVector(" + paramName + ")";
        return "lidlToStdVector_" + elem.name + "(" + paramName + ")";
    }
    return paramName;
}

static QString stdReturnToQt(const TypeExpr& te, const QString& varName)
{
    if (!lidlIsStdConvertible(te))
        return varName;

    if (te.kind == TypeExpr::Primitive) {
        if (te.name == "tstr")    return "QString::fromStdString(" + varName + ")";
        if (te.name == "bstr")    return "QByteArray(reinterpret_cast<const char*>(" + varName + ".data()), static_cast<int>(" + varName + ".size()))";
        if (te.name == "int")     return "static_cast<int>(" + varName + ")";
        if (te.name == "uint")    return "static_cast<int>(" + varName + ")";
        return varName;
    }
    if (te.kind == TypeExpr::Array && te.elements.size() == 1) {
        const TypeExpr& elem = te.elements[0];
        if (elem.kind == TypeExpr::Primitive && elem.name == "tstr")
            return "lidlToQStringList(" + varName + ")";
        return "lidlToQVariantList_" + elem.name + "(" + varName + ")";
    }
    return varName;
}

static QString variantToQtArg(const TypeExpr& te, int argIdx)
{
    QString a = "args.at(" + QString::number(argIdx) + ")";
    QString qt = lidlTypeToQt(te);
    if (qt == "QString")     return a + ".toString()";
    if (qt == "QByteArray")  return a + ".toByteArray()";
    if (qt == "int")         return a + ".toInt()";
    if (qt == "double")      return a + ".toDouble()";
    if (qt == "bool")        return a + ".toBool()";
    if (qt == "QStringList") return a + ".toStringList()";
    if (qt == "QVariantList") return a + ".toList()";
    if (qt == "QVariantMap") return a + ".toMap()";
    if (qt == "LogosResult") return a + ".value<LogosResult>()";
    return a;
}

// ---------------------------------------------------------------------------
// Provider header generation
// ---------------------------------------------------------------------------

QString lidlMakeProviderHeader(const ModuleDecl& module,
                               const QString& implClass,
                               const QString& implHeader)
{
    QString className = lidlToPascalCase(module.name);
    QString providerObjectClass = className + "ProviderObject";
    QString pluginClass = className + "Plugin";
    QString h;
    QTextStream s(&h);

    s << "// AUTO-GENERATED by logos-cpp-generator -- do not edit\n";
    s << "#pragma once\n\n";

    s << "#include <QObject>\n";
    s << "#include <QString>\n";
    s << "#include <QVariant>\n";
    s << "#include <QStringList>\n";
    s << "#include <QVariantList>\n";
    s << "#include <QVariantMap>\n";
    s << "#include <QByteArray>\n";
    s << "#include <QJsonArray>\n";
    s << "#include <string>\n";
    s << "#include <vector>\n";
    s << "#include <cstdint>\n";
    s << "#include <functional>\n\n";

    s << "#include \"logos_provider_object.h\"\n";
    s << "#include \"interface.h\"\n";
    s << "#include \"logos_types.h\"\n\n";

    s << "#include \"" << implHeader << "\"\n\n";

    // Conversion helpers (only if needed)
    bool needsStringVecHelper = false;
    for (const MethodDecl& md : module.methods) {
        for (const ParamDecl& pd : md.params) {
            if (pd.type.kind == TypeExpr::Array && pd.type.elements.size() == 1
                && pd.type.elements[0].kind == TypeExpr::Primitive
                && pd.type.elements[0].name == "tstr")
                needsStringVecHelper = true;
        }
        if (md.returnType.kind == TypeExpr::Array && md.returnType.elements.size() == 1
            && md.returnType.elements[0].kind == TypeExpr::Primitive
            && md.returnType.elements[0].name == "tstr")
            needsStringVecHelper = true;
    }

    if (needsStringVecHelper) {
        s << "namespace {\n";
        s << "inline QStringList lidlToQStringList(const std::vector<std::string>& v) {\n";
        s << "    QStringList result;\n";
        s << "    result.reserve(static_cast<int>(v.size()));\n";
        s << "    for (const auto& s : v)\n";
        s << "        result.append(QString::fromStdString(s));\n";
        s << "    return result;\n";
        s << "}\n\n";
        s << "inline std::vector<std::string> lidlToStdStringVector(const QStringList& list) {\n";
        s << "    std::vector<std::string> result;\n";
        s << "    result.reserve(static_cast<size_t>(list.size()));\n";
        s << "    for (const auto& s : list)\n";
        s << "        result.push_back(s.toStdString());\n";
        s << "    return result;\n";
        s << "}\n";
        s << "} // anonymous namespace\n\n";
    }

    // Emit nlohmannToQVariant helper if any method returns LogosMap / LogosList / StdLogosResult
    bool needsNlohmannHelper = false;
    bool needsResultHelper = false;
    for (const MethodDecl& md : module.methods) {
        if (md.jsonReturn)   needsNlohmannHelper = true;
        if (md.resultReturn) { needsNlohmannHelper = true; needsResultHelper = true; }
    }
    if (needsNlohmannHelper) {
        s << "#include <nlohmann/json.hpp>\n\n";
        s << "namespace {\n";
        s << "inline QVariant nlohmannToQVariant(const nlohmann::json& j) {\n";
        s << "    if (j.is_null())    return QVariant();\n";
        s << "    if (j.is_boolean()) return QVariant(j.get<bool>());\n";
        s << "    if (j.is_number_integer()) return QVariant(static_cast<qlonglong>(j.get<int64_t>()));\n";
        s << "    if (j.is_number_unsigned()) return QVariant(static_cast<qulonglong>(j.get<uint64_t>()));\n";
        s << "    if (j.is_number_float()) return QVariant(j.get<double>());\n";
        s << "    if (j.is_string()) return QVariant(QString::fromStdString(j.get<std::string>()));\n";
        s << "    if (j.is_array()) {\n";
        s << "        QVariantList list;\n";
        s << "        list.reserve(static_cast<int>(j.size()));\n";
        s << "        for (const auto& elem : j)\n";
        s << "            list.append(nlohmannToQVariant(elem));\n";
        s << "        return QVariant(list);\n";
        s << "    }\n";
        s << "    if (j.is_object()) {\n";
        s << "        QVariantMap map;\n";
        s << "        for (auto it = j.begin(); it != j.end(); ++it)\n";
        s << "            map.insert(QString::fromStdString(it.key()), nlohmannToQVariant(it.value()));\n";
        s << "        return QVariant(map);\n";
        s << "    }\n";
        s << "    return QVariant();\n";
        s << "}\n";
        if (needsResultHelper) {
            s << "\n";
            s << "#include \"logos_result.h\"\n";
            s << "inline LogosResult stdResultToQt(const StdLogosResult& r) {\n";
            s << "    LogosResult qr;\n";
            s << "    qr.success = r.success;\n";
            s << "    qr.value = nlohmannToQVariant(r.value);\n";
            s << "    qr.error = r.error.empty() ? QVariant() : QVariant(QString::fromStdString(r.error));\n";
            s << "    return qr;\n";
            s << "}\n";
        }
        s << "} // anonymous namespace\n\n";
    }

    // --- ProviderObject class ---
    s << "class " << providerObjectClass << " : public LogosProviderBase {\n";
    s << "    LOGOS_PROVIDER(" << providerObjectClass << ", \""
      << module.name << "\", \"" << (module.version.isEmpty() ? "0.0.0" : module.version) << "\")\n\n";
    s << "public:\n";

    // Wire m_impl.emitEvent → LogosProviderBase::emitEvent when the impl
    // declares a public emitEvent callback (detected from header) or when
    // events are declared in metadata.json (legacy path).
    if (module.hasEmitEvent || !module.events.isEmpty()) {
        s << "    " << providerObjectClass << "() {\n";
        s << "        m_impl.emitEvent = [this](const std::string& name, const std::string& data) {\n";
        s << "            QVariantList args;\n";
        s << "            if (!data.empty()) args << QString::fromStdString(data);\n";
        s << "            emitEvent(QString::fromStdString(name), args);\n";
        s << "        };\n";
        s << "    }\n\n";
    }

    for (const MethodDecl& md : module.methods) {
        QString qtRet = lidlTypeToQt(md.returnType);
        bool retConvertible = lidlIsStdConvertible(md.returnType);

        s << "    " << qtRet << " " << md.name << "(";
        for (int i = 0; i < md.params.size(); ++i) {
            QString qt = lidlTypeToQt(md.params[i].type);
            if (qt == "QString" || qt == "QByteArray" || qt == "QStringList"
                || qt == "QVariantList" || qt == "QVariantMap" || qt == "LogosResult")
                s << "const " << qt << "& " << md.params[i].name;
            else
                s << qt << " " << md.params[i].name;
            if (i + 1 < md.params.size()) s << ", ";
        }
        s << ") {\n";

        if (qtRet == "void") {
            s << "        m_impl." << md.name << "(";
            for (int i = 0; i < md.params.size(); ++i) {
                s << qtParamToStd(md.params[i].type, md.params[i].name);
                if (i + 1 < md.params.size()) s << ", ";
            }
            s << ");\n";
        } else if (md.jsonReturn) {
            // LogosMap / LogosList: impl returns nlohmann::json, convert to Qt type
            s << "        auto _result = m_impl." << md.name << "(";
            for (int i = 0; i < md.params.size(); ++i) {
                s << qtParamToStd(md.params[i].type, md.params[i].name);
                if (i + 1 < md.params.size()) s << ", ";
            }
            s << ");\n";
            if (qtRet == "QVariantMap")
                s << "        return nlohmannToQVariant(_result).toMap();\n";
            else
                s << "        return nlohmannToQVariant(_result).toList();\n";
        } else if (md.resultReturn) {
            // StdLogosResult: impl returns pure-C++ result, convert to Qt LogosResult
            s << "        auto _result = m_impl." << md.name << "(";
            for (int i = 0; i < md.params.size(); ++i) {
                s << qtParamToStd(md.params[i].type, md.params[i].name);
                if (i + 1 < md.params.size()) s << ", ";
            }
            s << ");\n";
            s << "        return stdResultToQt(_result);\n";
        } else if (retConvertible) {
            s << "        auto _result = m_impl." << md.name << "(";
            for (int i = 0; i < md.params.size(); ++i) {
                s << qtParamToStd(md.params[i].type, md.params[i].name);
                if (i + 1 < md.params.size()) s << ", ";
            }
            s << ");\n";
            s << "        return " << stdReturnToQt(md.returnType, "_result") << ";\n";
        } else {
            s << "        return m_impl." << md.name << "(";
            for (int i = 0; i < md.params.size(); ++i) {
                s << qtParamToStd(md.params[i].type, md.params[i].name);
                if (i + 1 < md.params.size()) s << ", ";
            }
            s << ");\n";
        }

        s << "    }\n\n";
    }

    if (!module.events.isEmpty()) {
        s << "protected:\n";
        for (const EventDecl& ed : module.events) {
            QString methodName = "emit" + lidlToPascalCase(ed.name);
            s << "    void " << methodName << "(";
            for (int i = 0; i < ed.params.size(); ++i) {
                QString qt = lidlTypeToQt(ed.params[i].type);
                if (qt == "QString" || qt == "QByteArray" || qt == "QStringList"
                    || qt == "QVariantList" || qt == "QVariantMap" || qt == "LogosResult")
                    s << "const " << qt << "& " << ed.params[i].name;
                else
                    s << qt << " " << ed.params[i].name;
                if (i + 1 < ed.params.size()) s << ", ";
            }
            s << ") {\n";
            s << "        emitEvent(\"" << ed.name << "\", QVariantList{";
            for (int i = 0; i < ed.params.size(); ++i) {
                s << "QVariant::fromValue(" << ed.params[i].name << ")";
                if (i + 1 < ed.params.size()) s << ", ";
            }
            s << "});\n";
            s << "    }\n\n";
        }
    }

    s << "private:\n";
    s << "    " << implClass << " m_impl;\n";
    s << "};\n\n";

    // --- Plugin/Loader class ---
    s << "class " << pluginClass << " : public QObject, public PluginInterface, public LogosProviderPlugin {\n";
    s << "    Q_OBJECT\n";
    s << "    Q_PLUGIN_METADATA(IID LogosProviderPlugin_iid FILE \"metadata.json\")\n";
    s << "    Q_INTERFACES(PluginInterface LogosProviderPlugin)\n\n";
    s << "public:\n";
    s << "    QString name() const override { return QStringLiteral(\"" << module.name << "\"); }\n";
    s << "    QString version() const override { return QStringLiteral(\""
      << (module.version.isEmpty() ? "0.0.0" : module.version) << "\"); }\n";
    s << "    LogosProviderObject* createProviderObject() override {\n";
    s << "        return new " << providerObjectClass << "();\n";
    s << "    }\n";
    s << "};\n";

    return h;
}

// ---------------------------------------------------------------------------
// Dispatch source generation
// ---------------------------------------------------------------------------

QString lidlMakeProviderDispatch(const ModuleDecl& module)
{
    QString className = lidlToPascalCase(module.name);
    QString providerObjectClass = className + "ProviderObject";
    QString c;
    QTextStream s(&c);

    s << "// AUTO-GENERATED by logos-cpp-generator -- do not edit\n";
    s << "#include \"" << module.name << "_qt_glue.h\"\n";
    s << "#include <QJsonArray>\n";
    s << "#include <QJsonObject>\n";
    s << "#include <QVariant>\n";
    s << "#include <QString>\n";
    s << "#include \"logos_types.h\"\n\n";

    // --- callMethod ---
    s << "QVariant " << providerObjectClass
      << "::callMethod(const QString& methodName, const QVariantList& args)\n{\n";

    for (const MethodDecl& md : module.methods) {
        QString qtRet = lidlTypeToQt(md.returnType);
        s << "    if (methodName == \"" << md.name << "\") {\n";

        if (qtRet == "void") {
            s << "        " << md.name << "(";
            for (int i = 0; i < md.params.size(); ++i) {
                s << variantToQtArg(md.params[i].type, i);
                if (i + 1 < md.params.size()) s << ", ";
            }
            s << ");\n";
            s << "        return QVariant(true);\n";
        } else {
            s << "        return QVariant::fromValue(" << md.name << "(";
            for (int i = 0; i < md.params.size(); ++i) {
                s << variantToQtArg(md.params[i].type, i);
                if (i + 1 < md.params.size()) s << ", ";
            }
            s << "));\n";
        }
        s << "    }\n";
    }

    s << "    qWarning() << \"" << providerObjectClass
      << "::callMethod: unknown method:\" << methodName;\n";
    s << "    return QVariant();\n";
    s << "}\n\n";

    // --- getMethods ---
    s << "QJsonArray " << providerObjectClass << "::getMethods()\n{\n";
    s << "    QJsonArray methods;\n";

    for (const MethodDecl& md : module.methods) {
        QString qtRet = lidlTypeToQt(md.returnType);
        s << "    {\n";
        s << "        QJsonObject obj;\n";
        s << "        obj[\"name\"] = QStringLiteral(\"" << md.name << "\");\n";
        s << "        obj[\"returnType\"] = QStringLiteral(\"" << qtRet << "\");\n";
        s << "        obj[\"isInvokable\"] = true;\n";

        QString sig = md.name + "(";
        for (int i = 0; i < md.params.size(); ++i) {
            sig += lidlTypeToQt(md.params[i].type);
            if (i + 1 < md.params.size()) sig += ",";
        }
        sig += ")";
        s << "        obj[\"signature\"] = QStringLiteral(\"" << sig << "\");\n";

        if (!md.params.isEmpty()) {
            s << "        QJsonArray params;\n";
            for (int i = 0; i < md.params.size(); ++i) {
                s << "        params.append(QJsonObject{{\"type\", QStringLiteral(\""
                  << lidlTypeToQt(md.params[i].type) << "\")}, {\"name\", QStringLiteral(\""
                  << md.params[i].name << "\")}});\n";
            }
            s << "        obj[\"parameters\"] = params;\n";
        }

        s << "        methods.append(obj);\n";
        s << "    }\n";
    }

    s << "    return methods;\n";
    s << "}\n";

    return c;
}

// ---------------------------------------------------------------------------
// Full pipeline (from .lidl file)
// ---------------------------------------------------------------------------

int lidlGenerateProviderGlue(const QString& lidlPath,
                              const QString& implClass,
                              const QString& implHeader,
                              const QString& outputDir,
                              QTextStream& out, QTextStream& err)
{
    QFileInfo fi(lidlPath);
    if (!fi.exists()) {
        err << "LIDL file does not exist: " << lidlPath << "\n";
        return 2;
    }
    QFile file(fi.canonicalFilePath().isEmpty() ? fi.absoluteFilePath() : fi.canonicalFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err << "Failed to open LIDL file: " << lidlPath << "\n";
        return 3;
    }
    QString source = QString::fromUtf8(file.readAll());
    file.close();

    LidlParseResult pr = lidlParse(source);
    if (pr.hasError()) {
        err << lidlPath << ":" << pr.errorLine << ":" << pr.errorColumn
            << ": " << pr.error << "\n";
        return 4;
    }

    LidlValidationResult vr = lidlValidate(pr.module);
    if (vr.hasErrors()) {
        for (const QString& e : vr.errors)
            err << lidlPath << ": " << e << "\n";
        return 5;
    }

    const ModuleDecl& mod = pr.module;
    QString genDirPath = outputDir.isEmpty()
        ? QDir::current().filePath("generated")
        : outputDir;
    QDir().mkpath(genDirPath);

    QString glueHeaderAbs = QDir(genDirPath).filePath(mod.name + "_qt_glue.h");
    {
        QFile f(glueHeaderAbs);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            err << "Failed to write glue header: " << glueHeaderAbs << "\n";
            return 6;
        }
        f.write(lidlMakeProviderHeader(mod, implClass, implHeader).toUtf8());
    }

    QString dispatchAbs = QDir(genDirPath).filePath(mod.name + "_dispatch.cpp");
    {
        QFile f(dispatchAbs);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            err << "Failed to write dispatch source: " << dispatchAbs << "\n";
            return 7;
        }
        f.write(lidlMakeProviderDispatch(mod).toUtf8());
    }

    out << "Generated: " << glueHeaderAbs << "\n";
    out << "Generated: " << dispatchAbs << "\n";
    out.flush();
    return 0;
}

// ---------------------------------------------------------------------------
// C-FFI provider header generation
// Generates a Qt glue header that calls C functions directly — no C++ impl class.
// ---------------------------------------------------------------------------

QString lidlMakeProviderHeaderCFFI(const CHeaderParseResult& cpr)
{
    const ModuleDecl& module  = cpr.module;
    const QString freeFunc    = cpr.freeStringFunc;
    const QString cInc        = cpr.cHeaderInclude;

    QString className         = lidlToPascalCase(module.name);
    QString providerObjectClass = className + "ProviderObject";
    QString pluginClass       = className + "Plugin";

    QString h;
    QTextStream s(&h);

    s << "// AUTO-GENERATED by logos-cpp-generator (c-ffi mode) -- do not edit\n";
    s << "#pragma once\n\n";

    s << "#include <QObject>\n";
    s << "#include <QString>\n";
    s << "#include <QVariant>\n";
    s << "#include <QStringList>\n";
    s << "#include <QVariantList>\n";
    s << "#include <QVariantMap>\n";
    s << "#include <QByteArray>\n";
    s << "#include <QJsonArray>\n";
    s << "#include <cstdint>\n\n";

    s << "#include \"logos_provider_object.h\"\n";
    s << "#include \"interface.h\"\n";
    s << "#include \"logos_types.h\"\n\n";

    // Include the C header
    s << "extern \"C\" {\n";
    s << "#include \"" << cInc << "\"\n";
    s << "}\n\n";

    // --- ProviderObject class ---
    s << "class " << providerObjectClass << " : public LogosProviderBase {\n";
    s << "    LOGOS_PROVIDER(" << providerObjectClass << ", \""
      << module.name << "\", \"" << (module.version.isEmpty() ? "0.0.0" : module.version) << "\")\n\n";
    s << "public:\n";

    for (const CHeaderMethod& cm : cpr.methods) {
        const MethodDecl& md = cm.decl;
        QString qtRet        = lidlTypeToQt(md.returnType);

        // Emit method signature
        s << "    " << qtRet << " " << md.name << "(";
        for (int i = 0; i < md.params.size(); ++i) {
            QString qt = lidlTypeToQt(md.params[i].type);
            if (qt == "QString" || qt == "QByteArray" || qt == "QStringList"
                || qt == "QVariantList" || qt == "QVariantMap" || qt == "LogosResult")
                s << "const " << qt << "& " << md.params[i].name;
            else
                s << qt << " " << md.params[i].name;
            if (i + 1 < md.params.size()) s << ", ";
        }
        s << ") {\n";

        // Build the C call expression with type conversions for Qt → C
        // For each param: QString → const char* via QByteArray; int → int64_t; bool → bool; etc.
        //
        // We need to pre-declare QByteArray temporaries for QString params so the
        // constData() pointer stays alive across the call.
        bool hasStringParams = false;
        for (const ParamDecl& pd : md.params) {
            if (pd.type.kind == TypeExpr::Primitive && pd.type.name == "tstr") {
                hasStringParams = true;
                break;
            }
        }

        if (hasStringParams) {
            for (const ParamDecl& pd : md.params) {
                if (pd.type.kind == TypeExpr::Primitive && pd.type.name == "tstr") {
                    s << "        QByteArray _" << pd.name << "_utf8 = " << pd.name << ".toUtf8();\n";
                }
            }
        }

        // Build argument list for C call
        auto cArg = [](const ParamDecl& pd) -> QString {
            if (pd.type.kind == TypeExpr::Primitive) {
                if (pd.type.name == "tstr")    return "_" + pd.name + "_utf8.constData()";
                if (pd.type.name == "int")     return "static_cast<int64_t>(" + pd.name + ")";
                if (pd.type.name == "uint")    return "static_cast<uint64_t>(" + pd.name + ")";
                if (pd.type.name == "float64") return "static_cast<double>(" + pd.name + ")";
            }
            return pd.name;
        };

        QString callExpr = cm.cFunctionName + "(";
        for (int i = 0; i < md.params.size(); ++i) {
            callExpr += cArg(md.params[i]);
            if (i + 1 < md.params.size()) callExpr += ", ";
        }
        callExpr += ")";

        if (qtRet == "void") {
            s << "        " << callExpr << ";\n";
        } else if (md.returnType.kind == TypeExpr::Primitive && md.returnType.name == "tstr") {
            // String return: C function returns char* or const char*
            if (cm.returnsHeapString && !freeFunc.isEmpty()) {
                // Heap-allocated: copy into QString, then free
                s << "        char* _result = " << callExpr << ";\n";
                s << "        QString _ret = _result ? QString::fromUtf8(_result) : QString();\n";
                s << "        " << freeFunc << "(_result);\n";
                s << "        return _ret;\n";
            } else {
                // Static/borrowed: just wrap, no free
                s << "        const char* _result = " << callExpr << ";\n";
                s << "        return _result ? QString::fromUtf8(_result) : QString();\n";
            }
        } else if (md.returnType.kind == TypeExpr::Primitive && md.returnType.name == "int") {
            s << "        return static_cast<int>(" << callExpr << ");\n";
        } else if (md.returnType.kind == TypeExpr::Primitive && md.returnType.name == "uint") {
            s << "        return static_cast<int>(" << callExpr << ");\n";
        } else if (md.returnType.kind == TypeExpr::Primitive && md.returnType.name == "float64") {
            s << "        return static_cast<double>(" << callExpr << ");\n";
        } else {
            s << "        return " << callExpr << ";\n";
        }

        s << "    }\n\n";
    }

    s << "private:\n";
    s << "};\n\n";

    // --- Plugin/Loader class ---
    s << "class " << pluginClass << " : public QObject, public PluginInterface, public LogosProviderPlugin {\n";
    s << "    Q_OBJECT\n";
    s << "    Q_PLUGIN_METADATA(IID LogosProviderPlugin_iid FILE \"metadata.json\")\n";
    s << "    Q_INTERFACES(PluginInterface LogosProviderPlugin)\n\n";
    s << "public:\n";
    s << "    QString name() const override { return QStringLiteral(\"" << module.name << "\"); }\n";
    s << "    QString version() const override { return QStringLiteral(\""
      << (module.version.isEmpty() ? "0.0.0" : module.version) << "\"); }\n";
    s << "    LogosProviderObject* createProviderObject() override {\n";
    s << "        return new " << providerObjectClass << "();\n";
    s << "    }\n";
    s << "};\n";

    return h;
}

// ---------------------------------------------------------------------------
// C-FFI dispatch source generation (callMethod + getMethods)
// Reuses the same logic as the standard dispatch — methods/types are identical.
// ---------------------------------------------------------------------------

QString lidlMakeProviderDispatchCFFI(const CHeaderParseResult& cpr)
{
    // The dispatch code only depends on method names and Qt types —
    // exactly the same as the standard dispatch.  Delegate directly.
    return lidlMakeProviderDispatch(cpr.module);
}
