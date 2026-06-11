#include "lidl_gen_cdylib.h"
#include "lidl_gen_provider.h"  // lidlTypeToStd, lidlToPascalCase via shared helpers

#include <QTextStream>

// Shared helpers from lidl_gen_provider.cpp / generator_lib
QString lidlToPascalCase(const QString& name);
QString lidlTypeToQt(const TypeExpr& te);
bool lidlIsStdConvertible(const TypeExpr& te);

namespace {

// The cdylib-supported subset: std-convertible LIDL types only.
bool typeSupported(const TypeExpr& te, bool isReturn)
{
    if (te.kind == TypeExpr::Primitive) {
        if (te.name == "tstr" || te.name == "bstr" || te.name == "int"
            || te.name == "uint" || te.name == "float64" || te.name == "bool")
            return true;
        // result (StdLogosResult) and any (LogosMap/LogosList via jsonReturn)
        // are fine as RETURNS — the generator routes them through nlohmann.
        if (isReturn && (te.name == "result" || te.name == "any"))
            return true;
        return false;
    }
    if (te.kind == TypeExpr::Array && te.elements.size() == 1) {
        const TypeExpr& e = te.elements[0];
        return e.kind == TypeExpr::Primitive
            && (e.name == "tstr" || e.name == "int" || e.name == "uint"
                || e.name == "float64" || e.name == "bool");
    }
    return false;
}

// json arg expression -> std-typed C++ expression
QString jsonArgToStd(const TypeExpr& te, const QString& expr)
{
    if (te.kind == TypeExpr::Primitive) {
        if (te.name == "tstr")    return expr + ".get<std::string>()";
        if (te.name == "bstr")    return "lidlBytesFromJson(" + expr + ")";
        if (te.name == "int")     return expr + ".get<int64_t>()";
        if (te.name == "uint")    return expr + ".get<uint64_t>()";
        if (te.name == "float64") return expr + ".get<double>()";
        if (te.name == "bool")    return expr + ".get<bool>()";
    }
    if (te.kind == TypeExpr::Array && te.elements.size() == 1) {
        const QString inner = lidlTypeToStd(te);
        return expr + ".get<" + inner + ">()";
    }
    return expr;
}

// std-typed return variable -> json expression
QString stdReturnToJson(const MethodDecl& md, const QString& var)
{
    const TypeExpr& te = md.returnType;
    if (md.resultReturn) {
        // StdLogosResult -> the canonical {success, value, error} object
        // (same shape logos_json_convert emits for Qt LogosResult).
        return "lidlResultToJson(" + var + ")";
    }
    if (md.jsonReturn) {
        return var;  // LogosMap / LogosList are nlohmann::json already
    }
    if (te.kind == TypeExpr::Primitive) {
        if (te.name == "bstr") return "lidlBytesToJson(" + var + ")";
        return "nlohmann::json(" + var + ")";
    }
    return "nlohmann::json(" + var + ")";
}

void emitInterfaceJson(QTextStream& s, const ModuleDecl& module)
{
    s << "static nlohmann::json lidlInterfaceJson()\n{\n";
    s << "    nlohmann::json methods = nlohmann::json::array();\n";
    for (const MethodDecl& md : module.methods) {
        s << "    {\n        nlohmann::json obj;\n";
        s << "        obj[\"name\"] = \"" << md.name << "\";\n";
        if (!md.description.isEmpty()) {
            QString esc = md.description;
            esc.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n");
            s << "        obj[\"description\"] = \"" << esc << "\";\n";
        }
        QString sig = md.name + "(";
        for (int i = 0; i < md.params.size(); ++i) {
            sig += lidlTypeToQt(md.params[i].type);
            if (i + 1 < md.params.size()) sig += ",";
        }
        sig += ")";
        s << "        obj[\"signature\"] = \"" << sig << "\";\n";
        s << "        obj[\"returnType\"] = \"" << lidlTypeToQt(md.returnType) << "\";\n";
        s << "        obj[\"isInvokable\"] = true;\n";
        if (!md.params.isEmpty()) {
            s << "        nlohmann::json params = nlohmann::json::array();\n";
            for (const ParamDecl& pd : md.params) {
                s << "        params.push_back({{\"type\", \"" << lidlTypeToQt(pd.type)
                  << "\"}, {\"name\", \"" << pd.name << "\"}});\n";
            }
            s << "        obj[\"parameters\"] = params;\n";
        }
        s << "        methods.push_back(obj);\n    }\n";
    }
    for (const EventDecl& ed : module.events) {
        s << "    {\n        nlohmann::json obj;\n";
        s << "        obj[\"type\"] = \"event\";\n";
        s << "        obj[\"name\"] = \"" << ed.name << "\";\n";
        if (!ed.description.isEmpty()) {
            QString esc = ed.description;
            esc.replace('\\', "\\\\").replace('"', "\\\"").replace('\n', "\\n");
            s << "        obj[\"description\"] = \"" << esc << "\";\n";
        }
        QString sig = ed.name + "(";
        for (int i = 0; i < ed.params.size(); ++i) {
            sig += lidlTypeToQt(ed.params[i].type);
            if (i + 1 < ed.params.size()) sig += ",";
        }
        sig += ")";
        s << "        obj[\"signature\"] = \"" << sig << "\";\n";
        if (!ed.params.isEmpty()) {
            s << "        nlohmann::json params = nlohmann::json::array();\n";
            for (const ParamDecl& pd : ed.params) {
                s << "        params.push_back({{\"type\", \"" << lidlTypeToQt(pd.type)
                  << "\"}, {\"name\", \"" << pd.name << "\"}});\n";
            }
            s << "        obj[\"parameters\"] = params;\n";
        }
        s << "        methods.push_back(obj);\n    }\n";
    }
    s << "    return methods;\n}\n\n";
}

} // namespace

bool lidlCdylibSupported(const ModuleDecl& module, QString* error)
{
    for (const MethodDecl& md : module.methods) {
        for (const ParamDecl& pd : md.params) {
            if (!typeSupported(pd.type, /*isReturn=*/false)) {
                if (error)
                    *error = QString("method '%1': parameter '%2' has a type outside the "
                                     "cdylib-supported (Qt-free) subset")
                                 .arg(md.name, pd.name);
                return false;
            }
        }
        const bool voidReturn =
            md.returnType.kind == TypeExpr::Primitive && md.returnType.name.isEmpty();
        if (!voidReturn && !md.jsonReturn && !md.resultReturn
            && !typeSupported(md.returnType, /*isReturn=*/true)) {
            if (error)
                *error = QString("method '%1': return type outside the cdylib-supported "
                                 "(Qt-free) subset").arg(md.name);
            return false;
        }
    }
    for (const EventDecl& ed : module.events) {
        for (const ParamDecl& pd : ed.params) {
            if (!typeSupported(pd.type, /*isReturn=*/false)) {
                if (error)
                    *error = QString("event '%1': parameter '%2' has a type outside the "
                                     "cdylib-supported (Qt-free) subset")
                                 .arg(ed.name, pd.name);
                return false;
            }
        }
    }
    return true;
}

QString lidlMakeModuleImplExports(const ModuleDecl& module,
                                  const QString& implClass,
                                  const QString& implHeader)
{
    QString c;
    QTextStream s(&c);

    s << "// AUTO-GENERATED by logos-cpp-generator --cdylib -- do not edit\n";
    s << "//\n";
    s << "// The common module-impl C ABI exports (logos_module_impl.h) around the\n";
    s << "// universal impl class `" << implClass << "`. Qt-FREE: compiled into the\n";
    s << "// module's cdylib; the uniform Qt-plugin glue (or a future no-Qt host)\n";
    s << "// drives it exclusively through these symbols.\n";
    s << "#include \"" << implHeader << "\"\n";
    s << "#include \"logos_module_impl.h\"\n";
    s << "#include \"logos_protocol.h\"\n";
    s << "#include \"logos_module_context.h\"\n";
    s << "#include \"logos_result.h\"\n";
    s << "#include <nlohmann/json.hpp>\n";
    s << "#include <cstdlib>\n";
    s << "#include <cstring>\n";
    s << "#include <map>\n";
    s << "#include <mutex>\n";
    s << "#include <string>\n";
    s << "#include <vector>\n\n";

    // -- shared statics ------------------------------------------------------
    s << "namespace {\n\n";
    s << implClass << "& lidlImpl()\n{\n    static " << implClass << " impl;\n    return impl;\n}\n\n";
    s << "logos_module_emit_cb g_emitCb = nullptr;\n";
    s << "void* g_emitUd = nullptr;\n";
    s << "std::mutex g_emitMutex;\n";
    s << "std::map<std::string, std::string> g_tokens;\n";
    s << "std::mutex g_tokensMutex;\n\n";

    s << "char* lidlStrdup(const std::string& str)\n{\n";
    s << "    char* out = static_cast<char*>(std::malloc(str.size() + 1));\n";
    s << "    if (out) std::memcpy(out, str.data(), str.size() + 1);\n";
    s << "    return out;\n}\n\n";

    s << "// Canonical tagged bytes form {\"_bytes\": base64url} (see logos_protocol.h)\n";
    s << "std::string lidlB64UrlEncode(const std::vector<uint8_t>& bytes)\n{\n";
    s << "    static const char* alpha = \"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_\";\n";
    s << "    std::string out;\n";
    s << "    size_t i = 0;\n";
    s << "    while (i + 3 <= bytes.size()) {\n";
    s << "        uint32_t n = (uint32_t(bytes[i]) << 16) | (uint32_t(bytes[i+1]) << 8) | uint32_t(bytes[i+2]);\n";
    s << "        out += alpha[(n >> 18) & 0x3f]; out += alpha[(n >> 12) & 0x3f];\n";
    s << "        out += alpha[(n >> 6) & 0x3f]; out += alpha[n & 0x3f];\n";
    s << "        i += 3;\n    }\n";
    s << "    if (i < bytes.size()) {\n";
    s << "        uint32_t n = uint32_t(bytes[i]) << 16;\n";
    s << "        if (i + 1 < bytes.size()) n |= uint32_t(bytes[i+1]) << 8;\n";
    s << "        out += alpha[(n >> 18) & 0x3f]; out += alpha[(n >> 12) & 0x3f];\n";
    s << "        if (i + 1 < bytes.size()) out += alpha[(n >> 6) & 0x3f];\n";
    s << "    }\n    return out;\n}\n\n";

    s << "int lidlB64Idx(char ch)\n{\n";
    s << "    if (ch >= 'A' && ch <= 'Z') return ch - 'A';\n";
    s << "    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;\n";
    s << "    if (ch >= '0' && ch <= '9') return ch - '0' + 52;\n";
    s << "    if (ch == '-') return 62;\n    if (ch == '_') return 63;\n    return -1;\n}\n\n";

    s << "std::vector<uint8_t> lidlBytesFromJson(const nlohmann::json& j)\n{\n";
    s << "    std::vector<uint8_t> out;\n";
    s << "    if (!j.is_object() || j.size() != 1 || !j.contains(\"_bytes\") || !j[\"_bytes\"].is_string())\n";
    s << "        return out;\n";
    s << "    const std::string s64 = j[\"_bytes\"].get<std::string>();\n";
    s << "    size_t i = 0;\n";
    s << "    while (i + 4 <= s64.size()) {\n";
    s << "        int a = lidlB64Idx(s64[i]), b = lidlB64Idx(s64[i+1]), c2 = lidlB64Idx(s64[i+2]), d = lidlB64Idx(s64[i+3]);\n";
    s << "        if (a < 0 || b < 0 || c2 < 0 || d < 0) return {};\n";
    s << "        uint32_t n = (uint32_t(a) << 18) | (uint32_t(b) << 12) | (uint32_t(c2) << 6) | uint32_t(d);\n";
    s << "        out.push_back((n >> 16) & 0xff); out.push_back((n >> 8) & 0xff); out.push_back(n & 0xff);\n";
    s << "        i += 4;\n    }\n";
    s << "    size_t rem = s64.size() - i;\n";
    s << "    if (rem == 2 || rem == 3) {\n";
    s << "        int a = lidlB64Idx(s64[i]), b = lidlB64Idx(s64[i+1]);\n";
    s << "        if (a < 0 || b < 0) return {};\n";
    s << "        uint32_t n = (uint32_t(a) << 18) | (uint32_t(b) << 12);\n";
    s << "        out.push_back((n >> 16) & 0xff);\n";
    s << "        if (rem == 3) {\n";
    s << "            int c2 = lidlB64Idx(s64[i+2]);\n";
    s << "            if (c2 < 0) return {};\n";
    s << "            n |= uint32_t(c2) << 6;\n";
    s << "            out.push_back((n >> 8) & 0xff);\n";
    s << "        }\n    }\n    return out;\n}\n\n";

    s << "nlohmann::json lidlBytesToJson(const std::vector<uint8_t>& bytes)\n{\n";
    s << "    return nlohmann::json{{\"_bytes\", lidlB64UrlEncode(bytes)}};\n}\n\n";

    s << "nlohmann::json lidlResultToJson(const StdLogosResult& r)\n{\n";
    s << "    nlohmann::json obj;\n";
    s << "    obj[\"success\"] = r.success;\n";
    s << "    obj[\"value\"] = r.value;\n";
    s << "    obj[\"error\"] = r.error.empty() ? nlohmann::json() : nlohmann::json(r.error);\n";
    s << "    return obj;\n}\n\n";

    emitInterfaceJson(s, module);
    s << "} // namespace\n\n";

    // -- event wiring (install once, lazily) ---------------------------------
    s << "static void lidlEnsureEmitWiring()\n{\n";
    s << "    static std::once_flag once;\n";
    s << "    std::call_once(once, []() {\n";
    s << "        _logos_codegen_::maybeSetEmitEvent(lidlImpl(),\n";
    s << "            [](const std::string& name, void* args) {\n";
    s << "                // cdylib events sidecar marshals into nlohmann::json\n";
    s << "                const nlohmann::json* payload = static_cast<const nlohmann::json*>(args);\n";
    s << "                std::lock_guard<std::mutex> lock(g_emitMutex);\n";
    s << "                if (g_emitCb) {\n";
    s << "                    const std::string dumped = payload ? payload->dump() : \"[]\";\n";
    s << "                    g_emitCb(name.c_str(), dumped.c_str(), g_emitUd);\n";
    s << "                }\n";
    s << "            });\n";
    s << "    });\n}\n\n";

    // -- exports -------------------------------------------------------------
    s << "extern \"C\" {\n\n";

    s << "char* logos_module_dispatch(const char* method, const char* args_json)\n{\n";
    s << "    if (!method) return nullptr;\n";
    s << "    lidlEnsureEmitWiring();\n";
    s << "    nlohmann::json args = nlohmann::json::array();\n";
    s << "    if (args_json && *args_json) {\n";
    s << "        args = nlohmann::json::parse(args_json, nullptr, false);\n";
    s << "        if (args.is_discarded() || !args.is_array()) return nullptr;\n";
    s << "    }\n";
    s << "    const std::string m(method);\n";
    s << "    try {\n";

    for (const MethodDecl& md : module.methods) {
        s << "        if (m == \"" << md.name << "\") {\n";
        s << "            if (args.size() < " << md.params.size() << ") return nullptr;\n";
        QString call = "lidlImpl()." + md.name + "(";
        for (int i = 0; i < md.params.size(); ++i) {
            call += jsonArgToStd(md.params[i].type,
                                 QString("args.at(%1)").arg(i));
            if (i + 1 < md.params.size()) call += ", ";
        }
        call += ")";
        const bool voidReturn = lidlTypeToQt(md.returnType) == "void";
        if (voidReturn) {
            s << "            " << call << ";\n";
            s << "            return lidlStrdup(\"true\");\n";
        } else {
            s << "            auto result = " << call << ";\n";
            s << "            return lidlStrdup(" << stdReturnToJson(md, "result") << ".dump());\n";
        }
        s << "        }\n";
    }

    s << "    } catch (const std::exception& e) {\n";
    s << "        nlohmann::json err{{\"code\", \"dispatch_failed\"}, {\"message\", e.what()},\n";
    s << "                           {\"origin\", \"" << module.name << "\"}};\n";
    s << "        return lidlStrdup(err.dump());\n";
    s << "    }\n";
    s << "    return nullptr;  // unknown method\n";
    s << "}\n\n";

    s << "char* logos_module_get_methods(void)\n{\n";
    s << "    return lidlStrdup(lidlInterfaceJson().dump());\n}\n\n";

    s << "void logos_module_set_context(const char* module_path,\n";
    s << "                              const char* instance_id,\n";
    s << "                              const char* instance_persistence_path)\n{\n";
    s << "    lidlEnsureEmitWiring();\n";
    s << "    _logos_codegen_::maybeSetContext(lidlImpl(),\n";
    s << "        module_path ? module_path : \"\",\n";
    s << "        instance_id ? instance_id : \"\",\n";
    s << "        instance_persistence_path ? instance_persistence_path : \"\");\n";
    s << "}\n\n";

    s << "void logos_module_set_emit_callback(logos_module_emit_cb cb, void* user_data)\n{\n";
    s << "    lidlEnsureEmitWiring();\n";
    s << "    std::lock_guard<std::mutex> lock(g_emitMutex);\n";
    s << "    g_emitCb = cb;\n";
    s << "    g_emitUd = user_data;\n";
    s << "}\n\n";

    s << "int logos_module_accept_token(const char* module_name, const char* token)\n{\n";
    s << "    if (!module_name || !token) return -1;\n";
    s << "    std::lock_guard<std::mutex> lock(g_tokensMutex);\n";
    s << "    g_tokens[module_name] = token;\n";
    s << "    return 0;\n}\n\n";

    s << "const char* logos_module_get_protocol_version(void)\n{\n";
    s << "    return LOGOS_PROTOCOL_VERSION_STRING;\n}\n\n";

    s << "void logos_module_string_free(char* str)\n{\n";
    s << "    std::free(str);\n}\n\n";

    s << "} // extern \"C\"\n";
    return c;
}

QString lidlMakeEventsSourceCdylib(const ModuleDecl& module,
                                   const QString& implClass,
                                   const QString& implHeader)
{
    QString c;
    QTextStream s(&c);
    s << "// AUTO-GENERATED by logos-cpp-generator --cdylib -- do not edit\n";
    s << "// Typed `logos_events:` bodies, cdylib flavor: marshal into\n";
    s << "// nlohmann::json and route through LogosModuleContext::emitEventImpl_\n";
    s << "// (the export wrapper forwards to the host's emit callback).\n";
    s << "#include \"" << implHeader << "\"\n";
    s << "#include <nlohmann/json.hpp>\n\n";

    for (const EventDecl& ed : module.events) {
        s << "void " << implClass << "::" << ed.name << "(";
        for (int i = 0; i < ed.params.size(); ++i) {
            const QString stdType = lidlTypeToStd(ed.params[i].type);
            if (stdType == "std::string" || stdType.startsWith("std::vector"))
                s << "const " << stdType << "& " << ed.params[i].name;
            else
                s << stdType << " " << ed.params[i].name;
            if (i + 1 < ed.params.size()) s << ", ";
        }
        s << ")\n{\n";
        s << "    nlohmann::json args = nlohmann::json::array();\n";
        for (const ParamDecl& pd : ed.params) {
            if (pd.type.kind == TypeExpr::Primitive && pd.type.name == "bstr")
                s << "    args.push_back(nlohmann::json{{\"_bytes\", \"\"}}); // bstr events: encode upstream\n";
            else
                s << "    args.push_back(" << pd.name << ");\n";
        }
        s << "    emitEventImpl_(\"" << ed.name << "\", &args);\n";
        s << "}\n\n";
    }
    return c;
}

QString lidlMakeCdylibGlueHeader(const ModuleDecl& module)
{
    const QString className = lidlToPascalCase(module.name);
    QString c;
    QTextStream s(&c);

    s << "// AUTO-GENERATED by logos-cpp-generator --cdylib -- do not edit\n";
    s << "//\n";
    s << "// The UNIFORM Qt-plugin glue over the common module-impl C ABI\n";
    s << "// (logos_module_impl.h). Identical regardless of the module's source\n";
    s << "// language (C++ or Rust): it only knows the C symbols, which are\n";
    s << "// linked in from the module's cdylib. logos_host loads it unchanged.\n";
    s << "#pragma once\n\n";
    s << "#include \"interface.h\"\n";
    s << "#include \"logos_provider_interface.h\"\n";
    s << "#include \"logos_json_convert.h\"\n";
    s << "#include \"logos_module_impl.h\"\n";
    s << "#include \"logos_types.h\"\n";
    s << "#include <QObject>\n";
    s << "#include <QJsonArray>\n";
    s << "#include <QJsonDocument>\n";
    s << "#include <QSet>\n";
    s << "#include <QString>\n";
    s << "#include <QVariant>\n";
    s << "#include <QVariantList>\n";
    s << "#include <nlohmann/json.hpp>\n\n";

    s << "class " << className << "CdylibProvider : public LogosProviderObject {\n";
    s << "public:\n";
    s << "    QVariant callMethod(const QString& methodName, const QVariantList& args) override;\n";
    s << "    QJsonArray getMethods() override;\n";
    s << "    bool informModuleToken(const QString& moduleName, const QString& token) override;\n";
    s << "    void setEventListener(EventCallback callback) override;\n";
    s << "    void init(void* apiInstance) override;\n";
    s << "    QString providerName() const override { return QStringLiteral(\"" << module.name << "\"); }\n";
    s << "    QString providerVersion() const override { return QStringLiteral(\"" << (module.version.isEmpty() ? QStringLiteral("1.0.0") : module.version) << "\"); }\n";
    s << "private:\n";
    s << "    EventCallback m_eventCallback;\n";
    s << "    static void emitTrampoline(const char* eventName, const char* dataJson, void* userData);\n";
    s << "};\n\n";

    // The root plugin must also implement PluginInterface — logos_host's
    // module_initializer hard-requires it before any provider detection.
    s << "class " << className << "CdylibPlugin : public QObject, public PluginInterface, public LogosProviderPlugin {\n";
    s << "    Q_OBJECT\n";
    s << "    Q_PLUGIN_METADATA(IID LogosProviderPlugin_iid FILE \"metadata.json\")\n";
    s << "    Q_INTERFACES(PluginInterface LogosProviderPlugin)\n";
    s << "public:\n";
    s << "    QString name() const override { return QStringLiteral(\"" << module.name << "\"); }\n";
    s << "    QString version() const override { return QStringLiteral(\"" << (module.version.isEmpty() ? QStringLiteral("1.0.0") : module.version) << "\"); }\n";
    s << "    LogosProviderObject* createProviderObject() override {\n";
    s << "        return new " << className << "CdylibProvider();\n";
    s << "    }\n";
    s << "};\n";
    return c;
}

QString lidlMakeCdylibGlueSource(const ModuleDecl& module)
{
    const QString className = lidlToPascalCase(module.name);
    const QString provider = className + "CdylibProvider";

    // Which methods return StdLogosResult — the glue re-materializes the Qt
    // LogosResult QVariant callers expect on the wire.
    QStringList resultMethods;
    for (const MethodDecl& md : module.methods)
        if (md.resultReturn) resultMethods << md.name;

    QString c;
    QTextStream s(&c);
    s << "// AUTO-GENERATED by logos-cpp-generator --cdylib -- do not edit\n";
    s << "#include \"" << module.name << "_cdylib_glue.h\"\n\n";

    s << "QVariant " << provider << "::callMethod(const QString& methodName, const QVariantList& args)\n{\n";
    s << "    nlohmann::json jArgs = nlohmann::json::array();\n";
    s << "    for (const QVariant& a : args)\n";
    s << "        jArgs.push_back(logos::qvariantToNlohmann(a));\n";
    s << "    const std::string dumped = jArgs.dump();\n";
    s << "    char* result = logos_module_dispatch(methodName.toUtf8().constData(), dumped.c_str());\n";
    s << "    if (!result) return QVariant();\n";
    s << "    nlohmann::json jResult = nlohmann::json::parse(result, nullptr, false);\n";
    s << "    logos_module_string_free(result);\n";
    s << "    if (jResult.is_discarded()) return QVariant();\n";
    if (!resultMethods.isEmpty()) {
        s << "    // StdLogosResult-returning methods: re-materialize the Qt LogosResult\n";
        s << "    // QVariant the wire expects ({success, value, error} crossed the C ABI).\n";
        s << "    static const QSet<QString> kResultMethods = {";
        for (int i = 0; i < resultMethods.size(); ++i) {
            s << "QStringLiteral(\"" << resultMethods[i] << "\")";
            if (i + 1 < resultMethods.size()) s << ", ";
        }
        s << "};\n";
        s << "    if (kResultMethods.contains(methodName) && jResult.is_object()) {\n";
        s << "        LogosResult lr;\n";
        s << "        lr.success = jResult.value(\"success\", false);\n";
        s << "        lr.value = logos::nlohmannToQVariant(jResult.value(\"value\", nlohmann::json()));\n";
        s << "        lr.error = jResult.contains(\"error\") && jResult[\"error\"].is_string()\n";
        s << "            ? QVariant(QString::fromStdString(jResult[\"error\"].get<std::string>()))\n";
        s << "            : QVariant();\n";
        s << "        return QVariant::fromValue(lr);\n";
        s << "    }\n";
    }
    s << "    return logos::nlohmannToQVariant(jResult);\n";
    s << "}\n\n";

    s << "QJsonArray " << provider << "::getMethods()\n{\n";
    s << "    char* json = logos_module_get_methods();\n";
    s << "    if (!json) return QJsonArray();\n";
    s << "    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json));\n";
    s << "    logos_module_string_free(json);\n";
    s << "    return doc.isArray() ? doc.array() : QJsonArray();\n";
    s << "}\n\n";

    s << "bool " << provider << "::informModuleToken(const QString& moduleName, const QString& token)\n{\n";
    s << "    return logos_module_accept_token(moduleName.toUtf8().constData(),\n";
    s << "                                     token.toUtf8().constData()) == 0;\n";
    s << "}\n\n";

    s << "void " << provider << "::setEventListener(EventCallback callback)\n{\n";
    s << "    m_eventCallback = std::move(callback);\n";
    s << "    logos_module_set_emit_callback(&" << provider << "::emitTrampoline, this);\n";
    s << "}\n\n";

    s << "void " << provider << "::emitTrampoline(const char* eventName, const char* dataJson, void* userData)\n{\n";
    s << "    auto* self = static_cast<" << provider << "*>(userData);\n";
    s << "    if (!self || !self->m_eventCallback || !eventName) return;\n";
    s << "    nlohmann::json payload = dataJson\n";
    s << "        ? nlohmann::json::parse(dataJson, nullptr, false)\n";
    s << "        : nlohmann::json::array();\n";
    s << "    if (payload.is_discarded() || !payload.is_array()) payload = nlohmann::json::array();\n";
    s << "    self->m_eventCallback(QString::fromUtf8(eventName),\n";
    s << "                          logos::nlohmannArgsToQVariantList(payload));\n";
    s << "}\n\n";

    s << "void " << provider << "::init(void* apiInstance)\n{\n";
    s << "    // Context comes from the host's property stamping on the LogosAPI\n";
    s << "    // object — forwarded across the C ABI; the cdylib never sees Qt.\n";
    s << "    QObject* api = static_cast<QObject*>(apiInstance);\n";
    s << "    if (!api) return;\n";
    s << "    logos_module_set_context(\n";
    s << "        api->property(\"modulePath\").toString().toUtf8().constData(),\n";
    s << "        api->property(\"instanceId\").toString().toUtf8().constData(),\n";
    s << "        api->property(\"instancePersistencePath\").toString().toUtf8().constData());\n";
    s << "}\n";
    return c;
}
