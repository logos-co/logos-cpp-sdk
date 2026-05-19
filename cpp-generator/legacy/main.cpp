#include <QCoreApplication>
#include <QPluginLoader>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMetaObject>
#include <QMetaMethod>
#include <QByteArray>
#include <QTextStream>
#include <QDir>
#include <QByteArrayList>
#include <QFile>
#include <QSet>
#include <QRegularExpression>
#include <QtGlobal>
#include "logos_provider_object.h"
#include "generator_lib.h"
#include "../experimental/lidl_parser.h"

// Convert a TypeExpr → Qt-typed string name (same surface the
// metaobject-introspection path produces for methods, so generator_lib
// can consume both via one code path).
static QString lidlTypeExprToQtTypeName(const TypeExpr& te)
{
    if (te.kind == TypeExpr::Primitive) {
        if (te.name == "tstr")    return "QString";
        if (te.name == "bstr")    return "QByteArray";
        if (te.name == "int")     return "int";
        if (te.name == "uint")    return "int";       // wire-as-int for now
        if (te.name == "float64") return "double";
        if (te.name == "bool")    return "bool";
        if (te.name == "result")  return "LogosResult";
        if (te.name == "any")     return "QVariant";
        return "QVariant";
    }
    if (te.kind == TypeExpr::Array && te.elements.size() == 1) {
        const TypeExpr& elem = te.elements[0];
        if (elem.kind == TypeExpr::Primitive && elem.name == "tstr")
            return "QStringList";
        return "QVariantList";
    }
    if (te.kind == TypeExpr::Map)      return "QVariantMap";
    if (te.kind == TypeExpr::Optional) return "QVariant";
    if (te.kind == TypeExpr::Named)    return "QVariant";
    return "QVariant";
}

// Load events from a `.lidl` sidecar shipped alongside a module's
// pre-built headers. Returns a JSON array of
//   { name, params: [ { name, type } ] }
// using Qt-typed type names — same shape generator_lib's makeHeader /
// makeSource already consume for methods.
static QJsonArray loadEventsFromLidl(const QString& lidlPath, QTextStream& err)
{
    QJsonArray result;
    QFile f(lidlPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err << "Failed to open events sidecar: " << lidlPath << "\n";
        return result;
    }
    QString source = QString::fromUtf8(f.readAll());
    f.close();

    LidlParseResult pr = lidlParse(source);
    if (pr.hasError()) {
        err << lidlPath << ":" << pr.errorLine << ":" << pr.errorColumn
            << ": " << pr.error << "\n";
        return result;
    }

    for (const EventDecl& ed : pr.module.events) {
        QJsonObject obj;
        obj["name"] = ed.name;
        QJsonArray params;
        for (const ParamDecl& pd : ed.params) {
            QJsonObject p;
            p["name"] = pd.name;
            p["type"] = lidlTypeExprToQtTypeName(pd.type);
            params.append(p);
        }
        obj["params"] = params;
        result.append(obj);
    }
    return result;
}

static QJsonArray enumerateMethods(QObject* moduleInstance)
{
    QJsonArray methodsArray;

    if (!moduleInstance) {
        return methodsArray;
    }

    const QMetaObject* metaObject = moduleInstance->metaObject();

    for (int i = 0; i < metaObject->methodCount(); ++i) {
        QMetaMethod method = metaObject->method(i);

        if (method.enclosingMetaObject() != metaObject) {
            continue;
        }

        QJsonObject methodObj;
        methodObj["signature"] = QString::fromUtf8(method.methodSignature());
        methodObj["name"] = QString::fromUtf8(method.name());
        methodObj["returnType"] = QString::fromUtf8(method.typeName());
        bool isInvokable = method.isValid() && (method.methodType() == QMetaMethod::Method || method.methodType() == QMetaMethod::Slot);
        methodObj["isInvokable"] = isInvokable;

        if (method.parameterCount() > 0) {
            QJsonArray params;
            for (int p = 0; p < method.parameterCount(); ++p) {
                QJsonObject paramObj;
                paramObj["type"] = QString::fromUtf8(method.parameterTypeName(p));
                QByteArrayList paramNames = method.parameterNames();
                if (p < paramNames.size() && !paramNames.at(p).isEmpty()) {
                    paramObj["name"] = QString::fromUtf8(paramNames.at(p));
                } else {
                    paramObj["name"] = QString("param%1").arg(p);
                }
                params.append(paramObj);
            }
            methodObj["parameters"] = params;
        }

        methodsArray.append(methodObj);
    }

    return methodsArray;
}

// toPascalCase, normalizeType, mapParamType, mapReturnType -> generator_lib.h/cpp

// makeHeader -> generator_lib.h/cpp

// makeSource -> generator_lib.h/cpp

static bool writeUmbrellaHeader(const QString& genDirPath, QTextStream& err)
{
    // Generate logos_sdk.h: include every per-module wrapper header in
    // the gen dir and aggregate them into a flat `LogosModules` struct.
    // The wrappers may be Qt-typed or Std-typed depending on the
    // --api-style picked for this build; the umbrella shape doesn't
    // change because either flavor produces the same accessor name
    // (`<dep>`) on the same class name (`<Dep>`).
    //
    // `core_manager_api.h` (if present in the gen dir from an older
    // run) is intentionally filtered out — universal modules access
    // only the deps they explicitly declared in `metadata.json#
    // dependencies`. Apps that need to manage the core use the C API
    // in liblogos directly, not the typed `LogosModules` aggregate.
    QDir genDir(genDirPath);
    QStringList headers = genDir.entryList(QStringList() << "*_api.h", QDir::Files | QDir::Readable);
    headers.removeAll(QStringLiteral("core_manager_api.h"));

    QString content;
    QTextStream s(&content);
    s << "#pragma once\n";
    s << "#include \"logos_api.h\"\n";
    s << "#include \"logos_api_client.h\"\n\n";
    for (const QString& h : headers) s << "#include \"" << h << "\"\n";
    s << "\n";

    s << "struct LogosModules {\n";
    s << "    explicit LogosModules(LogosAPI* api) : api(api)";
    for (const QString& h : headers) {
        QString base = h;
        base.chop(QString("_api.h").size());
        s << ", \n        " << base << "(api)";
    }
    s << " {}\n";
    s << "    LogosAPI* api;\n";
    for (const QString& h : headers) {
        QString base = h;
        base.chop(QString("_api.h").size());
        QString className = toPascalCase(base);
        s << "    " << className << " " << base << ";\n";
    }
    s << "};\n";

    QFile outFile(genDir.filePath("logos_sdk.h"));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write umbrella header: " << outFile.fileName() << "\n";
        return false;
    }
    outFile.write(content.toUtf8());
    outFile.close();
    return true;
}

static bool writeUmbrellaHeaderFromDeps(const QString& genDirPath, const QJsonArray& deps, QTextStream& err)
{
    // Generate logos_sdk.h from metadata.json's dependencies list. The
    // shape doesn't depend on apiStyle — each dep emits a single
    // `<name>_api.h` whose class signature shape was already decided
    // at codegen time. The umbrella just `#include`s and aggregates
    // each wrapper into the flat `LogosModules` struct.
    //
    // Only the modules explicitly listed in `metadata.json#
    // dependencies` are exposed. Apps that need to manage the core
    // (basecamp, logoscore) use liblogos' C API directly rather than
    // the typed `LogosModules` aggregate.
    QDir genDir(genDirPath);
    QString content;
    QTextStream s(&content);
    s << "#pragma once\n";
    s << "#include \"logos_api.h\"\n";
    s << "#include \"logos_api_client.h\"\n\n";
    for (const QJsonValue& v : deps) {
        if (!v.isString()) continue;
        QString depName = v.toString();
        s << "#include \"" << depName << "_api.h\"\n";
    }
    s << "\n";

    s << "struct LogosModules {\n";
    s << "    explicit LogosModules(LogosAPI* api) : api(api)";
    for (const QJsonValue& v : deps) {
        if (!v.isString()) continue;
        QString depName = v.toString();
        s << ", \n        " << depName << "(api)";
    }
    s << " {}\n";
    s << "    LogosAPI* api;\n";
    for (const QJsonValue& v : deps) {
        if (!v.isString()) continue;
        QString depName = v.toString();
        QString className = toPascalCase(depName);
        s << "    " << className << " " << depName << ";\n";
    }
    s << "};\n";

    QFile outFile(genDir.filePath("logos_sdk.h"));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write umbrella header: " << outFile.fileName() << "\n";
        return false;
    }
    outFile.write(content.toUtf8());
    outFile.close();
    return true;
}

static bool writeUmbrellaSource(const QString& genDirPath, QTextStream& err)
{
    // Generate logos_sdk.cpp: one #include per per-module wrapper
    // `.cpp` in the gen dir. There's now exactly one wrapper file per
    // module (Qt or std, picked at generation time), so no de-dup or
    // twin-file filtering is needed.
    //
    // `core_manager_api.cpp` (if present from an older run) is
    // filtered out — the umbrella header no longer declares
    // `CoreManager core_manager;` so including its definitions would
    // produce dead code.
    QDir genDir(genDirPath);
    QStringList sources = genDir.entryList(QStringList() << "*_api.cpp", QDir::Files | QDir::Readable);
    sources.removeAll(QStringLiteral("core_manager_api.cpp"));

    QString content;
    QTextStream s(&content);
    s << "#include \"logos_sdk.h\"\n\n";
    for (const QString& c : sources) s << "#include \"" << c << "\"\n";
    s << "\n";

    QFile outFile(genDir.filePath("logos_sdk.cpp"));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write umbrella source: " << outFile.fileName() << "\n";
        return false;
    }
    outFile.write(content.toUtf8());
    outFile.close();
    return true;
}

static bool writeUmbrellaSourceFromDeps(const QString& genDirPath, const QJsonArray& deps, QTextStream& err)
{
    // Generate logos_sdk.cpp from metadata.json's dependencies list.
    // Each dep emits one wrapper `.cpp` (Qt or std — decided at codegen
    // time, file name is the same either way), `#include`'d here.
    QDir genDir(genDirPath);
    QString content;
    QTextStream s(&content);
    s << "#include \"logos_sdk.h\"\n\n";
    for (const QJsonValue& v : deps) {
        if (!v.isString()) continue;
        QString depName = v.toString();
        s << "#include \"" << depName << "_api.cpp\"\n";
    }
    s << "\n";

    QFile outFile(genDir.filePath("logos_sdk.cpp"));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write umbrella source: " << outFile.fileName() << "\n";
        return false;
    }
    outFile.write(content.toUtf8());
    outFile.close();
    return true;
}

// ── Provider-header mode: scan LOGOS_METHOD markers and generate dispatch ────
// ParsedMethod, parseProviderHeader, toQVariantConversion -> generator_lib.h/cpp

static int generateProviderDispatch(const QString& headerPath, const QString& outputDir, QTextStream& out, QTextStream& err)
{
    QFileInfo fi(headerPath);
    if (!fi.exists()) {
        err << "Header file does not exist: " << headerPath << "\n";
        return 2;
    }

    QVector<ParsedMethod> methods = parseProviderHeader(headerPath, err);
    if (methods.isEmpty()) {
        err << "No LOGOS_METHOD markers found in: " << headerPath << "\n";
        return 3;
    }

    // Derive the class name from the header: parse for ": public LogosProviderBase"
    QString className;
    {
        QFile f(headerPath);
        f.open(QIODevice::ReadOnly | QIODevice::Text);
        QTextStream ts(&f);
        QRegularExpression classRe(R"(class\s+(\w+)\s*:\s*public\s+LogosProviderBase)");
        while (!ts.atEnd()) {
            QString line = ts.readLine();
            auto m = classRe.match(line);
            if (m.hasMatch()) {
                className = m.captured(1);
                break;
            }
        }
        f.close();
    }

    if (className.isEmpty()) {
        err << "Could not find class inheriting LogosProviderBase in: " << headerPath << "\n";
        return 4;
    }

    QString headerBaseName = fi.fileName();

    QString genDirPath = outputDir.isEmpty() ? fi.absolutePath() : outputDir;
    QDir().mkpath(genDirPath);

    // Generate logos_provider_dispatch.cpp
    QString content;
    QTextStream s(&content);

    s << "// AUTO-GENERATED by logos-cpp-generator -- do not edit\n";
    s << "#include \"" << headerBaseName << "\"\n";
    s << "#include <QJsonArray>\n";
    s << "#include <QJsonObject>\n";
    s << "#include <QVariant>\n";
    s << "#include <QString>\n";
    s << "#include \"logos_types.h\"\n\n";

    // callMethod() — group by name to support overloaded methods
    QMap<QString, QVector<const ParsedMethod*>> methodsByName;
    for (const ParsedMethod& m : methods) {
        methodsByName[m.name].append(&m);
    }

    s << "QVariant " << className << "::callMethod(const QString& methodName, const QVariantList& args)\n";
    s << "{\n";
    for (auto it = methodsByName.constBegin(); it != methodsByName.constEnd(); ++it) {
        const QString& name = it.key();
        const QVector<const ParsedMethod*>& overloads = it.value();
        s << "    if (methodName == \"" << name << "\") {\n";
        bool needArgsSizeCheck = overloads.size() > 1;
        for (const ParsedMethod* m : overloads) {
            if (needArgsSizeCheck) {
                s << "        if (args.size() == " << m->params.size() << ") {\n";
                s << "    ";
            }
            if (m->returnType == "void" || m->returnType.isEmpty()) {
                s << "        " << m->name << "(";
                for (int i = 0; i < m->params.size(); ++i) {
                    s << toQVariantConversion(m->params[i].first, QString("args.at(%1)").arg(i));
                    if (i + 1 < m->params.size()) s << ", ";
                }
                s << ");\n";
                if (needArgsSizeCheck) s << "    ";
                s << "        return QVariant(true);\n";
            } else {
                s << "        return QVariant::fromValue(" << m->name << "(";
                for (int i = 0; i < m->params.size(); ++i) {
                    s << toQVariantConversion(m->params[i].first, QString("args.at(%1)").arg(i));
                    if (i + 1 < m->params.size()) s << ", ";
                }
                s << "));\n";
            }
            if (needArgsSizeCheck) {
                s << "        }\n";
            }
        }
        s << "    }\n";
    }
    s << "    qWarning() << \"" << className << "::callMethod: unknown method:\" << methodName;\n";
    s << "    return QVariant();\n";
    s << "}\n\n";

    // getMethods()
    s << "QJsonArray " << className << "::getMethods()\n";
    s << "{\n";
    s << "    QJsonArray methods;\n";
    for (const ParsedMethod& m : methods) {
        s << "    {\n";
        s << "        QJsonObject obj;\n";
        s << "        obj[\"name\"] = QStringLiteral(\"" << m.name << "\");\n";
        s << "        obj[\"returnType\"] = QStringLiteral(\"" << m.returnType << "\");\n";
        s << "        obj[\"isInvokable\"] = true;\n";
        QString sig = m.name + "(";
        for (int i = 0; i < m.params.size(); ++i) {
            sig += m.params[i].first;
            if (i + 1 < m.params.size()) sig += ",";
        }
        sig += ")";
        s << "        obj[\"signature\"] = QStringLiteral(\"" << sig << "\");\n";
        if (!m.params.isEmpty()) {
            s << "        QJsonArray params;\n";
            for (int i = 0; i < m.params.size(); ++i) {
                s << "        params.append(QJsonObject{{\"type\", QStringLiteral(\"" << m.params[i].first << "\")}, {\"name\", QStringLiteral(\"" << m.params[i].second << "\")}});\n";
            }
            s << "        obj[\"parameters\"] = params;\n";
        }
        s << "        methods.append(obj);\n";
        s << "    }\n";
    }
    s << "    return methods;\n";
    s << "}\n";

    QString outputPath = QDir(genDirPath).filePath("logos_provider_dispatch.cpp");
    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write dispatch file: " << outputPath << "\n";
        return 5;
    }
    outFile.write(content.toUtf8());
    outFile.close();

    out << "Generated provider dispatch: " << outputPath << " (" << methods.size() << " methods from " << className << ")\n";
    out.flush();
    return 0;
}

static int generateFromPlugin(const QString& pluginInputPath, const QString& outputDir, bool moduleOnly, ApiStyle apiStyle, const QJsonArray& events, QTextStream& out, QTextStream& err)
{
    QFileInfo fi(pluginInputPath);
    if (!fi.exists()) {
        err << "Plugin file does not exist: " << pluginInputPath << "\n";
        return 2;
    }

    QString resolvedPath = fi.canonicalFilePath();
    if (resolvedPath.isEmpty()) {
        resolvedPath = fi.absoluteFilePath();
    }

    QString genDirPath = outputDir.isEmpty() ? QDir::current().filePath("logos-cpp-sdk/cpp/generated") : outputDir;
    QDir().mkpath(genDirPath);

    QPluginLoader loader(resolvedPath);
    if (!loader.load()) {
        err << "Failed to load plugin at " << resolvedPath << ": " << loader.errorString() << "\n";
        return 3;
    }
    QObject* instance = loader.instance();
    if (!instance) {
        err << "Plugin loaded but no instance could be created for " << resolvedPath << "\n";
        loader.unload();
        return 4;
    }

    QString moduleName;
    {
        QJsonObject md = loader.metaData();
        QJsonObject meta = md.value("MetaData").toObject();
        moduleName = meta.value("name").toString();
        if (moduleName.isEmpty()) {
            moduleName = QFileInfo(resolvedPath).baseName();
        }
    }

    QJsonArray methods;
    LogosProviderPlugin* providerPlugin = qobject_cast<LogosProviderPlugin*>(instance);
    if (providerPlugin) {
        LogosProviderObject* provider = providerPlugin->createProviderObject();
        if (provider) {
            methods = provider->getMethods();
            out << "Detected new-API plugin (LogosProviderPlugin), using getMethods() — "
                << methods.size() << " methods\n";
            delete provider;
        } else {
            err << "LogosProviderPlugin::createProviderObject() returned null\n";
        }
    } else {
        methods = enumerateMethods(instance);
    }

    QString className = toPascalCase(moduleName);
    QString headerRel = QString("%1_api.h").arg(moduleName);
    QString sourceRel = QString("%1_api.cpp").arg(moduleName);
    QString headerAbs = QDir(genDirPath).filePath(headerRel);
    QString sourceAbs = QDir(genDirPath).filePath(sourceRel);

    // Single per-module wrapper file pair. apiStyle decides the
    // signature shape: Qt-typed for legacy / handcrafted callers
    // (default), std-typed when the consuming module's build passed
    // --api-style=std (typically because it's `interface: "universal"`).
    // Both produce the same filename and class name, so the umbrella
    // doesn't need to know which style was picked. `events` (loaded
    // from a sibling `.lidl` sidecar via --events-from) adds typed
    // `on<EventName>(callback)` accessors next to the existing methods.
    QString header = makeHeader(moduleName, className, methods, apiStyle, events);
    QString source = makeSource(moduleName, className, headerRel, methods, apiStyle, events);

    {
        QFile f(headerAbs);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            err << "Failed to write header: " << headerAbs << "\n";
            loader.unload();
            return 5;
        }
        f.write(header.toUtf8());
        f.close();
    }
    {
        QFile f(sourceAbs);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            err << "Failed to write source: " << sourceAbs << "\n";
            loader.unload();
            return 6;
        }
        f.write(source.toUtf8());
        f.close();
    }

    if (!moduleOnly) {
        if (!writeUmbrellaHeader(genDirPath, err)) {
            loader.unload();
            return 7;
        }
        if (!writeUmbrellaSource(genDirPath, err)) {
            loader.unload();
            return 8;
        }
    }

    QJsonDocument doc(methods);
    // out << doc.toJson(QJsonDocument::Indented) << "\n";
    out << "Generated: " << QDir(genDirPath).filePath(headerRel) << " and " << QDir(genDirPath).filePath(sourceRel) << "\n";
    out.flush();

    loader.unload();
    return 0;
}

int legacy_main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QTextStream err(stderr);
    QTextStream out(stdout);

    const QStringList args = app.arguments();
    
    // Parse --output-dir option
    QString outputDir;
    const int outDirIdx = args.indexOf("--output-dir");
    if (outDirIdx != -1 && outDirIdx + 1 < args.size()) {
        outputDir = args.at(outDirIdx + 1);
        if (outputDir.startsWith('@')) {
            outputDir.remove(0, 1);
        }
    }

    // Parse --module-only option
    bool moduleOnly = args.contains("--module-only");

    // Parse --general-only option
    bool generalOnly = args.contains("--general-only");

    // Parse --api-style option (qt | std). Picks which type surface
    // the generated `<Module>` wrapper exposes. Default is qt for
    // backward compatibility — every existing module that doesn't
    // declare `interface: "universal"` in its metadata.json keeps
    // its Qt-typed LogosModules surface. Universal modules get
    // -DLOGOS_API_STYLE=std threaded through by mkLogosModule.nix /
    // LogosModule.cmake, which becomes `--api-style=std` here.
    // Both forms accepted: `--api-style std` and `--api-style=std`.
    ApiStyle apiStyle = ApiStyle::Qt;
    {
        QString apiVal;
        for (int i = 0; i < args.size(); ++i) {
            const QString& a = args.at(i);
            if (a == "--api-style") {
                if (i + 1 < args.size()) apiVal = args.at(i + 1);
                break;
            }
            if (a.startsWith("--api-style=")) {
                apiVal = a.section('=', 1);
                break;
            }
        }
        if (apiVal == "std") apiStyle = ApiStyle::Std;
        else if (!apiVal.isEmpty() && apiVal != "qt") {
            err << "Unknown --api-style value: " << apiVal
                << " (expected 'qt' or 'std')\n";
            return 1;
        }
    }

    // Support: extract dependencies from a metadata.json file
    {
        const int metaIdx = args.indexOf("--metadata");
        if (metaIdx != -1) {
            if (metaIdx + 1 >= args.size()) {
                err << "Usage: " << QFileInfo(app.applicationFilePath()).fileName() << " --metadata /absolute/path/to/metadata.json [--output-dir /path/to/output] [--module-only] [--general-only]\n";
                return 1;
            }
            QString metaPathArg = args.at(metaIdx + 1);
            if (metaPathArg.startsWith('@')) {
                metaPathArg.remove(0, 1);
            }
            QFileInfo mfi(metaPathArg);
            if (!mfi.exists()) {
                err << "Metadata file does not exist: " << metaPathArg << "\n";
                return 2;
            }
            QString metaResolvedPath = mfi.canonicalFilePath();
            if (metaResolvedPath.isEmpty()) {
                metaResolvedPath = mfi.absoluteFilePath();
            }
            QFile mf(metaResolvedPath);
            if (!mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                err << "Failed to open metadata file: " << metaResolvedPath << "\n";
                return 3;
            }
            const QByteArray jsonData = mf.readAll();
            mf.close();
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                err << "Invalid metadata JSON in " << metaResolvedPath << ": " << parseError.errorString() << "\n";
                return 4;
            }
            const QJsonObject obj = doc.object();
            const QJsonArray deps = obj.value("dependencies").toArray();

            // If --general-only provided, generate only the umbrella files.
            // `LogosModules` exposes ONLY the modules listed in
            // `metadata.json#dependencies` — apps that need to manage the
            // core use liblogos' C API directly.
            if (generalOnly) {
                QString genDirPath = outputDir.isEmpty() ? QDir::current().filePath("logos-cpp-sdk/cpp/generated") : outputDir;
                QDir().mkpath(genDirPath);

                // Generate umbrella headers based on dependencies from metadata
                if (!writeUmbrellaHeaderFromDeps(genDirPath, deps, err)) {
                    return 7;
                }
                if (!writeUmbrellaSourceFromDeps(genDirPath, deps, err)) {
                    return 8;
                }

                out << "Generated logos_sdk.h and logos_sdk.cpp\n";
                out.flush();
                return 0;
            }

            // If --module-dir provided, generate for each dependency; else print deps
            const int modDirIdx = args.indexOf("--module-dir");
            if (modDirIdx != -1) {
                if (modDirIdx + 1 >= args.size()) {
                    err << "Usage: " << QFileInfo(app.applicationFilePath()).fileName() << " --metadata /path/to/metadata.json --module-dir /path/to/modules_dir [--output-dir /path/to/output] [--module-only] [--general-only]\n";
                    return 1;
                }
                QString moduleDirArg = args.at(modDirIdx + 1);
                if (moduleDirArg.startsWith('@')) {
                    moduleDirArg.remove(0, 1);
                }
                QDir moduleDir(moduleDirArg);
                if (!moduleDir.exists()) {
                    err << "Module directory does not exist: " << moduleDirArg << "\n";
                    return 2;
                }

                QString genDirPath = outputDir.isEmpty() ? QDir::current().filePath("logos-cpp-sdk/cpp/generated") : outputDir;
                QDir().mkpath(genDirPath);

                QString suffix;
#if defined(Q_OS_MACOS)
                suffix = ".dylib";
#elif defined(Q_OS_LINUX)
                suffix = ".so";
#elif defined(Q_OS_WIN)
                suffix = ".dll";
#else
                suffix = "";
#endif

                int overallStatus = 0;
                for (const QJsonValue& v : deps) {
                    if (!v.isString()) continue;
                    const QString depName = v.toString();
                    const QString pluginFileName = depName + "_plugin" + suffix;
                    const QString pluginPath = moduleDir.filePath(pluginFileName);
                    if (!QFileInfo::exists(pluginPath)) {
                        err << "Skipping: plugin not found for dependency '" << depName << "' at " << pluginPath << "\n";
                        continue;
                    }
                    out << "Running generator for dependency plugin: " << pluginPath << "\n";
                    // No --events-from sidecar in the multi-dep iteration
                    // path (each dep would need its own sidecar — out of
                    // scope here; --events-from is consumed by the
                    // per-plugin path below, invoked from buildHeaders.nix).
                    const int st = generateFromPlugin(pluginPath, outputDir, moduleOnly, apiStyle, QJsonArray(), out, err);
                    if (st != 0) {
                        overallStatus = st; // remember last non-zero
                    }
                }
                if (overallStatus == 0 && !moduleOnly) {
                    if (!writeUmbrellaHeader(genDirPath, err)) {
                        overallStatus = 7;
                    } else if (!writeUmbrellaSource(genDirPath, err)) {
                        overallStatus = 8;
                    }
                }
                return overallStatus;
            } else {
                for (const QJsonValue& v : deps) {
                    if (v.isString()) {
                        out << v.toString() << "\n";
                    }
                }
                out.flush();
                return 0;
            }
        }
    }

    // --provider-header mode: scan LOGOS_METHOD markers and generate dispatch code
    {
        const int phIdx = args.indexOf("--provider-header");
        if (phIdx != -1) {
            if (phIdx + 1 >= args.size()) {
                err << "Usage: " << QFileInfo(app.applicationFilePath()).fileName() << " --provider-header /path/to/impl.h [--output-dir /path/to/output]\n";
                return 1;
            }
            QString headerArg = args.at(phIdx + 1);
            if (headerArg.startsWith('@')) headerArg.remove(0, 1);
            return generateProviderDispatch(headerArg, outputDir, out, err);
        }
    }

    if (args.size() < 2) {
        err << "Usage: " << QFileInfo(app.applicationFilePath()).fileName() << " /absolute/path/to/plugin [--output-dir /path/to/output] [--module-only] [--events-from /path/to/<name>.lidl]\n";
        err << "   or:  " << QFileInfo(app.applicationFilePath()).fileName() << " --metadata /absolute/path/to/metadata.json [--output-dir /path/to/output] [--module-only] [--general-only]\n";
        err << "   or:  " << QFileInfo(app.applicationFilePath()).fileName() << " --metadata /absolute/path/to/metadata.json --general-only [--output-dir /path/to/output]\n";
        err << "   or:  " << QFileInfo(app.applicationFilePath()).fileName() << " --provider-header /path/to/impl.h [--output-dir /path/to/output]\n";
        return 1;
    }

    // --events-from <path>: load typed event prototypes from a LIDL
    // sidecar shipped alongside a dep's pre-built headers. When set,
    // the consumer wrapper (<name>_api.{h,cpp}) gains typed
    // `on<EventName>(callback)` accessors next to the existing
    // generic `onEvent(name, callback)` channel.
    QJsonArray eventsFromSidecar;
    {
        const int evIdx = args.indexOf("--events-from");
        QString evPath;
        if (evIdx != -1 && evIdx + 1 < args.size()) {
            evPath = args.at(evIdx + 1);
        } else {
            for (const QString& a : args) {
                if (a.startsWith("--events-from=")) {
                    evPath = a.section('=', 1);
                    break;
                }
            }
        }
        if (!evPath.isEmpty() && QFileInfo(evPath).exists()) {
            eventsFromSidecar = loadEventsFromLidl(evPath, err);
        }
    }

    QString argPath = args.at(1);
    return generateFromPlugin(argPath, outputDir, moduleOnly, apiStyle, eventsFromSidecar, out, err);
}
