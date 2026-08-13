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
#include "logos_provider_interface.h"
#include "generator_lib.h"
#include "metadata_dependencies.h"
#include "../experimental/lidl_compat.h"
#include "../experimental/impl_header_parser.h"
#include "lidl_to_json.h"   // ModuleDecl -> the JSON surface generator_lib consumes

// Escape a string for safe embedding inside a generated C++ string literal.
static QString cppStringEscape(const QString& s)
{
    QString out = s;
    out.replace('\\', "\\\\");
    out.replace('"', "\\\"");
    out.replace('\n', "\\n");
    return out;
}

// Load events from a `.lidl` sidecar shipped alongside a module's
// pre-built headers. Returns a JSON array of
//   { name, params: [ { name, type } ] }
// using Qt-typed type names — same shape generator_lib's makeHeader /
// makeSource already consume for methods.
static QJsonArray loadEventsFromLidl(const QString& lidlPath, QTextStream& err,
                                     QJsonArray* outRecords = nullptr)
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

    noteOptionalPositionalSlots(pr.module, lidlPath, err);
    if (outRecords) *outRecords = moduleRecordsToJson(pr.module);
    return moduleEventsToJson(pr.module);
}

// ── Dependency interfaces ───────────────────────────────────────────────────
//
// An "interface dependency" is a method/event contract a consumer declares
// (in `metadata.json#interface_dependencies`) decoupled from any concrete
// module. The definition file is either a `.lidl` or a pure-C++ `.h` (the
// module's own language). The generator emits a BOUND wrapper class — the
// target module name is a runtime ctor argument, not baked in — so one
// interface can be bound to any module that satisfies it.

// A single interface to generate a bound wrapper for. `path` is already
// resolved (nix resolves local `${src}/file` and remote `${input}/file`
// store paths and passes them via --interface; the generator never touches
// flake inputs). `implClass` is required for `.h` files, empty for `.lidl`.
struct InterfaceSpec {
    QString name;       // interface identifier → class/file name + bind_<name>
    QString path;       // resolved path to the .lidl / .h definition
    QString implClass;  // class inside a .h whose API defines the interface
};

// Parse all `<flag> <name>=<path>[=<impl_class>]` (or `<flag>=<name>=...`)
// occurrences. Names and store paths contain no '=', so splitting on the
// first two '=' is unambiguous. Used for both `--interface` (runtime-bound
// wrappers) and `--dep` (name-baked wrappers generated from a dep's LIDL).
static QVector<InterfaceSpec> parseSpecFlags(const QStringList& args, const QString& flag)
{
    const QString flagEq = flag + "=";
    QVector<InterfaceSpec> specs;
    for (int i = 0; i < args.size(); ++i) {
        QString value;
        if (args.at(i) == flag && i + 1 < args.size()) {
            value = args.at(i + 1);
        } else if (args.at(i).startsWith(flagEq)) {
            value = args.at(i).section('=', 1);
        } else {
            continue;
        }
        const int firstEq = value.indexOf('=');
        if (firstEq <= 0) continue;  // need at least name=path
        InterfaceSpec spec;
        spec.name = value.left(firstEq);
        const int secondEq = value.indexOf('=', firstEq + 1);
        if (secondEq < 0) {
            spec.path = value.mid(firstEq + 1);
        } else {
            spec.path = value.mid(firstEq + 1, secondEq - firstEq - 1);
            spec.implClass = value.mid(secondEq + 1);
        }
        specs.append(spec);
    }
    return specs;
}

// Parse an interface definition file into a ModuleDecl. `.lidl` parses
// directly; `.h`/`.hpp` go through the impl-header parser, which needs a
// metadata.json — we feed it a synthetic one carrying only the interface
// name so the consumer's identity and events are NOT pulled in (the
// interface's events come solely from the file's own `logos_events:` block).
static bool parseInterfaceFile(const InterfaceSpec& spec, const QString& genDirPath,
                               ModuleDecl& outMod, QTextStream& err)
{
    QFileInfo fi(spec.path);
    if (!fi.exists()) {
        err << "Interface file not found for '" << spec.name << "': " << spec.path << "\n";
        return false;
    }
    const QString ext = fi.suffix().toLower();
    if (ext == "lidl") {
        QFile f(spec.path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            err << "Failed to open interface file: " << spec.path << "\n";
            return false;
        }
        const QString src = QString::fromUtf8(f.readAll());
        f.close();
        LidlParseResult pr = lidlParse(src);
        if (pr.hasError()) {
            err << spec.path << ":" << pr.errorLine << ":" << pr.errorColumn
                << ": " << pr.error << "\n";
            return false;
        }
        outMod = pr.module;
        return true;
    }
    if (ext == "h" || ext == "hpp") {
        if (spec.implClass.isEmpty()) {
            err << "Interface '" << spec.name << "' is a C++ header but no impl_class was given "
                << "(metadata.json interface_dependencies entry needs \"impl_class\")\n";
            return false;
        }
        // Synthetic minimal metadata: name only, no events — keeps the
        // consumer's identity/events out of the interface.
        const QString synthMeta = QDir(genDirPath).filePath("." + spec.name + "_iface_meta.json");
        {
            QFile mf(synthMeta);
            if (!mf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                err << "Failed to write temporary interface metadata: " << synthMeta << "\n";
                return false;
            }
            mf.write(QString("{\"name\":\"%1\"}").arg(spec.name).toUtf8());
            mf.close();
        }
        ImplParseResult pr = parseImplHeader(spec.path, spec.implClass, synthMeta, err);
        QFile::remove(synthMeta);
        if (pr.hasError()) {
            err << "Error parsing interface header " << spec.path << ": " << pr.error << "\n";
            return false;
        }
        outMod = pr.module;
        return true;
    }
    err << "Unsupported interface file type for '" << spec.name << "': " << spec.path
        << " (expected .lidl or .h)\n";
    return false;
}

// Generate a wrapper (`<name>_api.{h,cpp}`) per spec from its definition file.
// The wrapper class is named from the spec `name` (PascalCase), NOT the
// definition file's internal module name, so it matches the `#include` the
// umbrella header emits. `bindMode` picks the wrapper flavour:
//   Bound  — interface dependency: ctor takes a runtime module name; exposed
//            via a `bind_<name>(...)` factory on the umbrella.
//   Static — concrete dependency: the module name is baked in; exposed as a
//            `<name>` member on the umbrella (byte-identical to the wrapper the
//            dep's prebuilt headers used to ship).
static bool generateInterfaceWrappers(const QVector<InterfaceSpec>& ifaces,
                                      const QString& genDirPath, ApiStyle apiStyle,
                                      QTextStream& out, QTextStream& err,
                                      BindMode bindMode = BindMode::Bound)
{
    for (const InterfaceSpec& spec : ifaces) {
        ModuleDecl mod;
        if (!parseInterfaceFile(spec, genDirPath, mod, err)) return false;

        {
            QString recErr;
            if (!lidlCheckRecords(mod, &recErr)) {
                err << spec.path << ": " << recErr << "\n";
                return false;
            }
        }

        noteOptionalPositionalSlots(mod, spec.path, err);

        const QString className = toPascalCase(spec.name);
        const QJsonArray methods = moduleMethodsToJson(mod);
        const QJsonArray events  = moduleEventsToJson(mod);
        const QJsonArray records = moduleRecordsToJson(mod);
        const QString headerRel = spec.name + "_api.h";
        const QString sourceRel = spec.name + "_api.cpp";

        const QString header = makeHeader(spec.name, className, methods, apiStyle, events, bindMode, records);
        const QString source = makeSource(spec.name, className, headerRel, methods, apiStyle, events, bindMode, records);

        {
            QFile f(QDir(genDirPath).filePath(headerRel));
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                err << "Failed to write wrapper header: " << headerRel << "\n";
                return false;
            }
            f.write(header.toUtf8());
        }
        {
            QFile f(QDir(genDirPath).filePath(sourceRel));
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                err << "Failed to write wrapper source: " << sourceRel << "\n";
                return false;
            }
            f.write(source.toUtf8());
        }
        out << "Generated " << (bindMode == BindMode::Bound ? "bound interface" : "dependency")
            << " wrapper: " << headerRel << " (class " << className << ", "
            << methods.size() << " methods, " << events.size() << " events)\n";
    }
    out.flush();
    return true;
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
    // The wrappers may be Qt-typed or std-typed (lp) depending on the
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

static bool writeUmbrellaHeaderFromDeps(const QString& genDirPath, const QJsonArray& deps, const QStringList& interfaceNames, QTextStream& err, ApiStyle apiStyle = ApiStyle::Qt, const QString& originName = QString())
{
    // Emission lives in generator_lib (makeUmbrellaHeaderFromDeps) next to the
    // per-module wrapper emitters, so the aggregate can be asserted on without
    // a filesystem; this writes what it returns.
    QDir genDir(genDirPath);
    const QString content = makeUmbrellaHeaderFromDeps(deps, interfaceNames, apiStyle, originName);

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

static bool writeUmbrellaSourceFromDeps(const QString& genDirPath, const QJsonArray& deps, const QStringList& interfaceNames, QTextStream& err)
{
    // Emission lives in generator_lib (makeUmbrellaSourceFromDeps), alongside
    // the header's; this writes what it returns.
    QDir genDir(genDirPath);
    const QString content = makeUmbrellaSourceFromDeps(deps, interfaceNames);

    QFile outFile(genDir.filePath("logos_sdk.cpp"));
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write umbrella source: " << outFile.fileName() << "\n";
        return false;
    }
    outFile.write(content.toUtf8());
    outFile.close();
    return true;
}

static int generateFromPlugin(const QString& pluginInputPath, const QString& outputDir, bool moduleOnly, ApiStyle apiStyle, const QJsonArray& events, QTextStream& out, QTextStream& err, const QJsonArray& records = {})
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
    // (default), std-typed and Qt-free when the consuming module's build
    // passed --api-style=lp (typically because it's `interface:
    // "universal"` or `"cdylib"`).
    // Both produce the same filename and class name, so the umbrella
    // doesn't need to know which style was picked. `events` (loaded
    // from a sibling `.lidl` sidecar via --events-from) adds typed
    // `on<EventName>(callback)` accessors next to the existing methods.
    QString header = makeHeader(moduleName, className, methods, apiStyle, events, BindMode::Static, records);
    QString source = makeSource(moduleName, className, headerRel, methods, apiStyle, events, BindMode::Static, records);

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

    // Parse --api-style option (qt | lp). Picks which type surface
    // the generated `<Module>` wrapper exposes. Default is qt for
    // backward compatibility — every existing module that doesn't
    // declare `interface: "universal"` in its metadata.json keeps
    // its Qt-typed LogosModules surface. Universal / cdylib modules get
    // -DLOGOS_API_STYLE=lp threaded through by mkLogosModule.nix /
    // LogosModule.cmake, which becomes `--api-style=lp` here.
    // Both forms accepted: `--api-style lp` and `--api-style=lp`.
    //
    // `std` was a third surface (std types over a QVariant/LogosAPIClient
    // body). It is retired, and rejected LOUDLY rather than aliased to qt:
    // a stale caller that still passes it wants std signatures, and silently
    // handing it the Qt surface would only fail later, further from the cause.
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
        if (apiVal == "std") {
            err << "--api-style=std was retired: the Std surface (std types over a "
                << "QVariant/LogosAPIClient body) no longer exists.\n"
                << "Use 'lp' for the Qt-free std-typed surface, or 'qt' for the "
                << "Qt-typed one.\n";
            return 1;
        }
        else if (apiVal == "lp") apiStyle = ApiStyle::Lp;
        else if (!apiVal.isEmpty() && apiVal != "qt") {
            err << "Unknown --api-style value: " << apiVal
                << " (expected 'qt' or 'lp')\n";
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

                // Collect interface dependencies. Primary source: --interface
                // flags (nix resolves both local `${src}/file` and remote
                // `${input}/file` store paths and passes them here, so the
                // generator never touches flake inputs). Fallback: self-resolve
                // LOCAL interface_dependencies entries (those without an
                // `input`) from metadata.json, relative to the metadata dir —
                // covers non-nix / source-tree builds. Flags win on collision.
                // Dedup --interface flags by name and drop malformed specs:
                // a repeated interface name would emit duplicate
                // #include "<name>_api.h" / bind_<name>(...) into logos_sdk.h
                // and fail to compile, and an empty name/path can only fail
                // later in a less actionable way.
                QVector<InterfaceSpec> ifaceSpecs;
                QSet<QString> haveIface;
                for (const InterfaceSpec& sp : parseSpecFlags(args, "--interface")) {
                    if (sp.name.isEmpty() || sp.path.isEmpty()) {
                        err << "Ignoring malformed --interface spec (empty name or path)\n";
                        continue;
                    }
                    if (haveIface.contains(sp.name)) {
                        err << "Ignoring duplicate --interface '" << sp.name << "'\n";
                        continue;
                    }
                    haveIface.insert(sp.name);
                    ifaceSpecs.append(sp);
                }

                const QString metaDir = QFileInfo(metaResolvedPath).absolutePath();
                const QJsonArray ifaceDeps = obj.value("interface_dependencies").toArray();
                for (const QJsonValue& v : ifaceDeps) {
                    if (!v.isObject()) continue;
                    const QJsonObject eo = v.toObject();
                    const QString name = eo.value("name").toString();
                    if (name.isEmpty() || haveIface.contains(name)) continue;
                    // Entries with an `input` reference another repo (flake
                    // input); only nix can resolve those, via a --interface
                    // flag. If we reach here without a matching flag, skip.
                    if (eo.contains("input")) {
                        err << "Note: interface '" << name << "' has an 'input' (cross-repo) "
                            << "but no --interface flag was passed; skipping (nix supplies the path).\n";
                        continue;
                    }
                    const QString file = eo.value("file").toString();
                    if (file.isEmpty()) continue;
                    InterfaceSpec spec;
                    spec.name = name;
                    spec.path = QDir(metaDir).filePath(file);
                    spec.implClass = eo.value("impl_class").toString();
                    ifaceSpecs.append(spec);
                    haveIface.insert(name);
                }

                // Generate one bound wrapper (<name>_api.{h,cpp}) per interface.
                if (!ifaceSpecs.isEmpty()) {
                    if (!generateInterfaceWrappers(ifaceSpecs, genDirPath, apiStyle, out, err)) {
                        return 9;
                    }
                }

                // Concrete dependencies generated from their published LIDL
                // (`--dep <name>=<lidl>`). Same backend as interfaces but
                // BindMode::Static — the module name is baked in and the dep is
                // exposed as a `<dep>` MEMBER (the umbrella already emits it from
                // `dependencies`, so no umbrella change). nix passes `--dep` only
                // for deps that publish a `lidl` output; deps without one fall
                // back to the header-copy path and are NOT passed here. Dedup vs
                // each other and vs interface names.
                QVector<InterfaceSpec> depSpecs;
                QSet<QString> haveDep;
                for (const InterfaceSpec& sp : parseSpecFlags(args, "--dep")) {
                    if (sp.name.isEmpty() || sp.path.isEmpty()) {
                        err << "Ignoring malformed --dep spec (empty name or path)\n";
                        continue;
                    }
                    if (haveIface.contains(sp.name)) {
                        err << "Ignoring --dep '" << sp.name << "' (name already used by an interface)\n";
                        continue;
                    }
                    if (haveDep.contains(sp.name)) {
                        err << "Ignoring duplicate --dep '" << sp.name << "'\n";
                        continue;
                    }
                    haveDep.insert(sp.name);
                    depSpecs.append(sp);
                }
                if (!depSpecs.isEmpty()) {
                    if (!generateInterfaceWrappers(depSpecs, genDirPath, apiStyle, out, err, BindMode::Static)) {
                        return 9;
                    }
                }

                QStringList interfaceNames;
                for (const InterfaceSpec& sp : ifaceSpecs) interfaceNames.append(sp.name);

                // Generate umbrella headers based on dependencies + interfaces.
                // For the Lp (Qt-free) flavor the umbrella bakes this module's
                // name as the lp_client origin.
                const QString originName = obj.value("name").toString();
                if (!writeUmbrellaHeaderFromDeps(genDirPath, deps, interfaceNames, err, apiStyle, originName)) {
                    return 7;
                }
                if (!writeUmbrellaSourceFromDeps(genDirPath, deps, interfaceNames, err)) {
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
                for (const QString& depName : dependencyNames(deps)) {
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
                for (const QString& depName : dependencyNames(deps)) {
                    out << depName << "\n";
                }
                out.flush();
                return 0;
            }
        }
    }

    // `--provider-header` (the LOGOS_METHOD-marked provider dispatch, i.e.
    // `interface: "provider"`) was removed: every provider now goes through the
    // module-impl C ABI. REFUSE it explicitly rather than letting it fall
    // through — the plugin-path branch below would otherwise read the flag
    // itself as a plugin path and report "Plugin file does not exist:
    // --provider-header", which reads like a missing file rather than a
    // retired mode.
    if (args.contains("--provider-header")) {
        err << "Error: --provider-header was removed. The provider dispatch it generated\n"
            << "       (interface: \"provider\", LOGOS_METHOD markers) is no longer supported.\n"
            << "       Use interface: \"universal\": write a plain src/<name>_impl.h and the\n"
            << "       contract is derived from it automatically.\n";
        return 2;
    }

    if (args.size() < 2) {
        err << "Usage: " << QFileInfo(app.applicationFilePath()).fileName() << " /absolute/path/to/plugin [--output-dir /path/to/output] [--module-only] [--events-from /path/to/<name>.lidl]\n";
        err << "   or:  " << QFileInfo(app.applicationFilePath()).fileName() << " --metadata /absolute/path/to/metadata.json [--output-dir /path/to/output] [--module-only] [--general-only]\n";
        err << "   or:  " << QFileInfo(app.applicationFilePath()).fileName() << " --metadata /absolute/path/to/metadata.json --general-only [--output-dir /path/to/output]\n";
        return 1;
    }

    // --events-from <path>: load typed event prototypes from a LIDL
    // sidecar shipped alongside a dep's pre-built headers. When set,
    // the consumer wrapper (<name>_api.{h,cpp}) gains typed
    // `on<EventName>(callback)` accessors next to the existing
    // generic `onEvent(name, callback)` channel.
    QJsonArray eventsFromSidecar;
    QJsonArray recordsFromSidecar;
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
            eventsFromSidecar = loadEventsFromLidl(evPath, err, &recordsFromSidecar);
        }
    }

    QString argPath = args.at(1);
    return generateFromPlugin(argPath, outputDir, moduleOnly, apiStyle, eventsFromSidecar, out, err, recordsFromSidecar);
}
