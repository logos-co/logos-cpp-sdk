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

static QString makeCoreManagerHeader()
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
    s << "#include \"logos_api.h\"\n";
    s << "#include \"logos_api_client.h\"\n";
    s << "#include \"logos_object.h\"\n\n";
    s << "class CoreManager {\n";
    s << "public:\n";
    s << "    explicit CoreManager(LogosAPI* api);\n\n";
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
    s << "    void initialize(int argc, char* argv[]);\n";
    s << "    void setPluginsDirectory(const QString& directory);\n";
    s << "    void start();\n";
    s << "    void cleanup();\n";
    s << "    QStringList getLoadedPlugins();\n";
    s << "    QJsonArray getKnownPlugins();\n";
    s << "    QJsonArray getPluginMethods(const QString& pluginName);\n";
    s << "    void helloWorld();\n";
    s << "    bool loadPlugin(const QString& pluginName);\n";
    s << "    bool unloadPlugin(const QString& pluginName);\n";
    s << "    QString processPlugin(const QString& filePath);\n\n";
    s << "private:\n";
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

static QString makeCoreManagerSource(const QString& headerBaseName)
{
    QString c;
    QTextStream s(&c);
    s << "#include \"" << headerBaseName << "\"\n\n";
    s << "#include <QDebug>\n";
    s << "#include <QStringList>\n\n";
    s << "CoreManager::CoreManager(LogosAPI* api) : m_api(api), m_client(api->getClient(\"core_manager\")), m_moduleName(QStringLiteral(\"core_manager\")) {}\n\n";
    s << "LogosObject* CoreManager::ensureReplica() {\n";
    s << "    if (!m_eventReplica) {\n";
    s << "        LogosObject* replica = m_client->requestObject(m_moduleName);\n";
    s << "        if (!replica) {\n";
    s << "            qWarning() << \"CoreManager: failed to acquire remote object for events on\" << m_moduleName;\n";
    s << "            return nullptr;\n";
    s << "        }\n";
    s << "        m_eventReplica = replica;\n";
    s << "    }\n";
    s << "    return m_eventReplica;\n";
    s << "}\n\n";
    s << "bool CoreManager::on(const QString& eventName, RawEventCallback callback) {\n";
    s << "    if (!callback) {\n";
    s << "        qWarning() << \"CoreManager: ignoring empty event callback for\" << eventName;\n";
    s << "        return false;\n";
    s << "    }\n";
    s << "    LogosObject* origin = ensureReplica();\n";
    s << "    if (!origin) {\n";
    s << "        return false;\n";
    s << "    }\n";
    s << "    m_client->onEvent(origin, eventName, callback);\n";
    s << "    return true;\n";
    s << "}\n\n";
    s << "bool CoreManager::on(const QString& eventName, EventCallback callback) {\n";
    s << "    if (!callback) {\n";
    s << "        qWarning() << \"CoreManager: ignoring empty event callback for\" << eventName;\n";
    s << "        return false;\n";
    s << "    }\n";
    s << "    return on(eventName, [callback](const QString&, const QVariantList& data) {\n";
    s << "        callback(data);\n";
    s << "    });\n";
    s << "}\n\n";
    s << "void CoreManager::setEventSource(LogosObject* source) {\n";
    s << "    m_eventSource = source;\n";
    s << "}\n\n";
    s << "LogosObject* CoreManager::eventSource() const {\n";
    s << "    return m_eventSource;\n";
    s << "}\n\n";
    s << "void CoreManager::trigger(const QString& eventName) {\n";
    s << "    trigger(eventName, QVariantList{});\n";
    s << "}\n\n";
    s << "void CoreManager::trigger(const QString& eventName, const QVariantList& data) {\n";
    s << "    if (!m_eventSource) {\n";
    s << "        qWarning() << \"CoreManager: no event source set for trigger\" << eventName;\n";
    s << "        return;\n";
    s << "    }\n";
    s << "    m_client->onEventResponse(m_eventSource, eventName, data);\n";
    s << "}\n\n";
    s << "void CoreManager::trigger(const QString& eventName, LogosObject* source, const QVariantList& data) {\n";
    s << "    if (!source) {\n";
    s << "        qWarning() << \"CoreManager: cannot trigger\" << eventName << \"with null source\";\n";
    s << "        return;\n";
    s << "    }\n";
    s << "    m_client->onEventResponse(source, eventName, data);\n";
    s << "}\n\n";
    s << "void CoreManager::initialize(int argc, char* argv[]) {\n";
    s << "    QStringList args;\n";
    s << "    if (argv) {\n";
    s << "        for (int i = 0; i < argc; ++i) {\n";
    s << "            args << QString::fromUtf8(argv[i] ? argv[i] : \"\");\n";
    s << "        }\n";
    s << "    }\n";
    s << "    m_client->invokeRemoteMethod(\"core_manager\", \"initialize\", argc, args);\n";
    s << "}\n\n";
    s << "void CoreManager::setPluginsDirectory(const QString& directory) {\n";
    s << "    m_client->invokeRemoteMethod(\"core_manager\", \"setPluginsDirectory\", directory);\n";
    s << "}\n\n";
    s << "void CoreManager::start() {\n";
    s << "    m_client->invokeRemoteMethod(\"core_manager\", \"start\");\n";
    s << "}\n\n";
    s << "void CoreManager::cleanup() {\n";
    s << "    m_client->invokeRemoteMethod(\"core_manager\", \"cleanup\");\n";
    s << "}\n\n";
    s << "QStringList CoreManager::getLoadedPlugins() {\n";
    s << "    QVariant _result = m_client->invokeRemoteMethod(\"core_manager\", \"getLoadedPlugins\");\n";
    s << "    return _result.toStringList();\n";
    s << "}\n\n";
    s << "QJsonArray CoreManager::getKnownPlugins() {\n";
    s << "    QVariant _result = m_client->invokeRemoteMethod(\"core_manager\", \"getKnownPlugins\");\n";
    s << "    return qvariant_cast<QJsonArray>(_result);\n";
    s << "}\n\n";
    s << "QJsonArray CoreManager::getPluginMethods(const QString& pluginName) {\n";
    s << "    QVariant _result = m_client->invokeRemoteMethod(\"core_manager\", \"getPluginMethods\", pluginName);\n";
    s << "    return qvariant_cast<QJsonArray>(_result);\n";
    s << "}\n\n";
    s << "void CoreManager::helloWorld() {\n";
    s << "    m_client->invokeRemoteMethod(\"core_manager\", \"helloWorld\");\n";
    s << "}\n\n";
    s << "bool CoreManager::loadPlugin(const QString& pluginName) {\n";
    s << "    QVariant _result = m_client->invokeRemoteMethod(\"core_manager\", \"loadPlugin\", pluginName);\n";
    s << "    return _result.toBool();\n";
    s << "}\n\n";
    s << "bool CoreManager::unloadPlugin(const QString& pluginName) {\n";
    s << "    QVariant _result = m_client->invokeRemoteMethod(\"core_manager\", \"unloadPlugin\", pluginName);\n";
    s << "    return _result.toBool();\n";
    s << "}\n\n";
    s << "QString CoreManager::processPlugin(const QString& filePath) {\n";
    s << "    QVariant _result = m_client->invokeRemoteMethod(\"core_manager\", \"processPlugin\", filePath);\n";
    s << "    return _result.toString();\n";
    s << "}\n\n";
    return c;
}

static bool ensureCoreManagerWrapper(const QString& genDirPath, QTextStream& err)
{
    const QString headerRel = QStringLiteral("core_manager_api.h");
    const QString sourceRel = QStringLiteral("core_manager_api.cpp");
    const QString headerAbs = QDir(genDirPath).filePath(headerRel);
    const QString sourceAbs = QDir(genDirPath).filePath(sourceRel);

    QString header = makeCoreManagerHeader();
    QString source = makeCoreManagerSource(headerRel);

    QFile headerFile(headerAbs);
    if (!headerFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write core manager header: " << headerAbs << "\n";
        return false;
    }
    headerFile.write(header.toUtf8());
    headerFile.close();

    QFile sourceFile(sourceAbs);
    if (!sourceFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        err << "Failed to write core manager source: " << sourceAbs << "\n";
        return false;
    }
    sourceFile.write(source.toUtf8());
    sourceFile.close();

    return true;
}

static bool writeUmbrellaHeader(const QString& genDirPath, QTextStream& err)
{
    // Generate logos-cpp-sdk/cpp/generated/logos_sdk.h that includes all *_api.h in this dir
    QDir genDir(genDirPath);
    QStringList headers = genDir.entryList(QStringList() << "*_api.h", QDir::Files | QDir::Readable);
    QString content;
    QTextStream s(&content);
    s << "#pragma once\n";
    s << "#include \"logos_api.h\"\n";
    s << "#include \"logos_api_client.h\"\n\n";
    // Includes
    for (const QString& h : headers) {
        s << "#include \"" << h << "\"\n";
    }
    s << "\n";
    // Convenience aggregator exposing module wrappers
    s << "struct LogosModules {\n";
    s << "    explicit LogosModules(LogosAPI* api) : api(api)";
    for (const QString& h : headers) {
        QString base = h;
        base.chop(QString("_api.h").size());
        QString className = toPascalCase(base);
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
    // Generate logos-cpp-sdk/cpp/generated/logos_sdk.h based on dependencies list
    QDir genDir(genDirPath);
    QString content;
    QTextStream s(&content);
    s << "#pragma once\n";
    s << "#include \"logos_api.h\"\n";
    s << "#include \"logos_api_client.h\"\n\n";
    // Includes
    s << "#include \"core_manager_api.h\"\n";
    for (const QJsonValue& v : deps) {
        if (!v.isString()) continue;
        QString depName = v.toString();
        s << "#include \"" << depName << "_api.h\"\n";
    }
    s << "\n";
    // Convenience aggregator exposing module wrappers
    s << "struct LogosModules {\n";
    s << "    explicit LogosModules(LogosAPI* api) : api(api), \n        core_manager(api)";
    for (const QJsonValue& v : deps) {
        if (!v.isString()) continue;
        QString depName = v.toString();
        s << ", \n        " << depName << "(api)";
    }
    s << " {}\n";
    s << "    LogosAPI* api;\n";
    s << "    CoreManager core_manager;\n";
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
    // Generate logos-cpp-sdk/cpp/generated/logos_sdk.cpp that includes all *_api.cpp in this dir
    QDir genDir(genDirPath);
    QStringList sources = genDir.entryList(QStringList() << "*_api.cpp", QDir::Files | QDir::Readable);
    QString content;
    QTextStream s(&content);
    s << "#include \"logos_sdk.h\"\n\n";
    for (const QString& c : sources) {
        s << "#include \"" << c << "\"\n";
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

static bool writeUmbrellaSourceFromDeps(const QString& genDirPath, const QJsonArray& deps, QTextStream& err)
{
    // Generate logos-cpp-sdk/cpp/generated/logos_sdk.cpp based on dependencies list
    QDir genDir(genDirPath);
    QString content;
    QTextStream s(&content);
    s << "#include \"logos_sdk.h\"\n\n";
    s << "#include \"core_manager_api.cpp\"\n";
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

static int generateFromPlugin(const QString& pluginInputPath, const QString& outputDir, bool moduleOnly, QTextStream& out, QTextStream& err)
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
    if (!moduleOnly && !ensureCoreManagerWrapper(genDirPath, err)) {
        return 9;
    }

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

    QString header = makeHeader(moduleName, className, methods);
    QString source = makeSource(moduleName, className, headerRel, methods);

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

int main(int argc, char* argv[])
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

            // If --general-only provided, generate only core manager and umbrella files
            if (generalOnly) {
                QString genDirPath = outputDir.isEmpty() ? QDir::current().filePath("logos-cpp-sdk/cpp/generated") : outputDir;
                QDir().mkpath(genDirPath);
                
                // Generate core manager wrapper
                if (!ensureCoreManagerWrapper(genDirPath, err)) {
                    return 9;
                }
                
                // Generate umbrella headers based on dependencies from metadata
                if (!writeUmbrellaHeaderFromDeps(genDirPath, deps, err)) {
                    return 7;
                }
                if (!writeUmbrellaSourceFromDeps(genDirPath, deps, err)) {
                    return 8;
                }
                
                out << "Generated core_manager_api.h, core_manager_api.cpp, logos_sdk.h, and logos_sdk.cpp\n";
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
                if (!moduleOnly && !ensureCoreManagerWrapper(genDirPath, err)) {
                    return 9;
                }

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
                    const int st = generateFromPlugin(pluginPath, outputDir, moduleOnly, out, err);
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
        err << "Usage: " << QFileInfo(app.applicationFilePath()).fileName() << " /absolute/path/to/plugin [--output-dir /path/to/output] [--module-only]\n";
        err << "   or:  " << QFileInfo(app.applicationFilePath()).fileName() << " --metadata /absolute/path/to/metadata.json [--output-dir /path/to/output] [--module-only] [--general-only]\n";
        err << "   or:  " << QFileInfo(app.applicationFilePath()).fileName() << " --metadata /absolute/path/to/metadata.json --general-only [--output-dir /path/to/output]\n";
        err << "   or:  " << QFileInfo(app.applicationFilePath()).fileName() << " --provider-header /path/to/impl.h [--output-dir /path/to/output]\n";
        return 1;
    }

    QString argPath = args.at(1);
    return generateFromPlugin(argPath, outputDir, moduleOnly, out, err);
}
