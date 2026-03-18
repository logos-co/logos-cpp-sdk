// logos-native-generator — Qt-free code generator for LOGOS_METHOD modules
//
// Parses C++ headers for LOGOS_METHOD markers and emits native-typed code:
//   --provider-dispatch: generates callMethod()/getMethodsJson() dispatch
//   --consumer-wrappers: generates Qt-free consumer wrapper class
//   --umbrella: generates logos_sdk.h/cpp aggregating all native wrappers

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct ParsedMethod {
    std::string returnType;
    std::string name;
    std::vector<std::pair<std::string, std::string>> params; // (type, name)
};

static std::string trim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string normalizeType(std::string t)
{
    t = trim(t);
    if (t.substr(0, 6) == "const ") t = t.substr(6);
    t = trim(t);
    if (!t.empty() && (t.back() == '&' || t.back() == '*')) t.pop_back();
    t = trim(t);
    return t;
}

static std::string toPascalCase(const std::string& name)
{
    std::string out;
    bool cap = true;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c))) { cap = true; continue; }
        if (cap) { out += static_cast<char>(std::toupper(static_cast<unsigned char>(c))); cap = false; }
        else { out += static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
    }
    if (out.empty()) return "Module";
    return out;
}

// Maps any Qt or native type to the canonical native type for code generation
static std::string mapToNativeType(const std::string& type)
{
    std::string base = normalizeType(type);
    if (base == "void" || base.empty()) return "void";
    if (base == "bool") return "bool";
    if (base == "int") return "int";
    if (base == "int64_t") return "int64_t";
    if (base == "double") return "double";
    if (base == "float") return "float";
    if (base == "QString" || base == "std::string") return "std::string";
    if (base == "QStringList" || base == "std::vector<std::string>") return "std::vector<std::string>";
    if (base == "QJsonArray" || base == "std::vector<LogosValue>") return "std::vector<LogosValue>";
    if (base == "QVariant" || base == "LogosValue") return "LogosValue";
    if (base == "LogosResult" || base == "NativeLogosResult") return "NativeLogosResult";
    if (base == "QVariantList") return "std::vector<LogosValue>";
    if (base == "QVariantMap") return "LogosValue::Map";
    return "LogosValue";
}

// Returns the LogosValue accessor expression for extracting a native type from args
static std::string argConversion(const std::string& nativeType, const std::string& argExpr)
{
    if (nativeType == "bool") return argExpr + ".toBool()";
    if (nativeType == "int") return "static_cast<int>(" + argExpr + ".toInt())";
    if (nativeType == "int64_t") return argExpr + ".toInt()";
    if (nativeType == "double") return argExpr + ".toDouble()";
    if (nativeType == "float") return "static_cast<float>(" + argExpr + ".toDouble())";
    if (nativeType == "std::string") return argExpr + ".toString()";
    if (nativeType == "std::vector<std::string>") return argExpr + ".toStringList()";
    if (nativeType == "std::vector<LogosValue>") return argExpr + ".toList()";
    if (nativeType == "LogosValue::Map") return argExpr + ".toMap()";
    if (nativeType == "LogosValue") return argExpr;
    return argExpr + ".toString()";
}

// Returns the expression to wrap a return value in LogosValue
static std::string returnWrap(const std::string& nativeType, const std::string& expr)
{
    if (nativeType == "void") return "";
    if (nativeType == "LogosValue") return expr;
    return "LogosValue(" + expr + ")";
}

// Returns the expression to unwrap a LogosValue to the native return type
static std::string returnUnwrap(const std::string& nativeType, const std::string& expr)
{
    if (nativeType == "bool") return expr + ".toBool()";
    if (nativeType == "int") return "static_cast<int>(" + expr + ".toInt())";
    if (nativeType == "int64_t") return expr + ".toInt()";
    if (nativeType == "double") return expr + ".toDouble()";
    if (nativeType == "float") return "static_cast<float>(" + expr + ".toDouble())";
    if (nativeType == "std::string") return expr + ".toString()";
    if (nativeType == "std::vector<std::string>") return expr + ".toStringList()";
    if (nativeType == "std::vector<LogosValue>") return expr + ".toList()";
    if (nativeType == "LogosValue::Map") return expr + ".toMap()";
    if (nativeType == "LogosValue") return expr;
    return expr + ".toString()";
}

// Whether a param type should be passed by const ref
static bool shouldPassByConstRef(const std::string& nativeType)
{
    return nativeType == "std::string" ||
           nativeType == "std::vector<std::string>" ||
           nativeType == "std::vector<LogosValue>" ||
           nativeType == "LogosValue" ||
           nativeType == "LogosValue::Map" ||
           nativeType == "NativeLogosResult";
}

static std::vector<ParsedMethod> parseHeader(const std::string& headerPath)
{
    std::vector<ParsedMethod> methods;
    std::ifstream file(headerPath);
    if (!file.is_open()) {
        std::cerr << "Cannot open header file: " << headerPath << "\n";
        return methods;
    }

    std::regex re(R"(^\s*LOGOS_METHOD\s+(.+?)\s+(\w+)\s*\(([^)]*)\)\s*;)");
    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (!std::regex_search(line, match, re)) continue;

        ParsedMethod m;
        m.returnType = normalizeType(match[1].str());
        m.name = match[2].str();

        std::string paramStr = trim(match[3].str());
        if (!paramStr.empty()) {
            std::istringstream pstream(paramStr);
            std::string part;
            while (std::getline(pstream, part, ',')) {
                std::string trimmed = trim(part);
                // Strip default value
                auto eqPos = trimmed.find('=');
                if (eqPos != std::string::npos)
                    trimmed = trim(trimmed.substr(0, eqPos));
                // Split type and name: find last space or &
                auto lastSpace = trimmed.rfind(' ');
                auto lastAmp = trimmed.rfind('&');
                size_t splitAt = std::string::npos;
                if (lastSpace != std::string::npos && lastAmp != std::string::npos)
                    splitAt = std::max(lastSpace, lastAmp);
                else if (lastSpace != std::string::npos)
                    splitAt = lastSpace;
                else if (lastAmp != std::string::npos)
                    splitAt = lastAmp;
                if (splitAt != std::string::npos && splitAt > 0) {
                    std::string type = normalizeType(trimmed.substr(0, splitAt + 1));
                    std::string pname = trim(trimmed.substr(splitAt + 1));
                    m.params.push_back({type, pname});
                } else {
                    m.params.push_back({normalizeType(trimmed),
                                        "arg" + std::to_string(m.params.size())});
                }
            }
        }
        methods.push_back(m);
    }
    return methods;
}

static std::string findClassName(const std::string& headerPath)
{
    std::ifstream file(headerPath);
    if (!file.is_open()) return "";

    std::regex re(R"(class\s+(\w+)\s*:\s*public\s+NativeProviderBase)");
    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, re))
            return match[1].str();
    }

    // Fallback: also support LogosProviderBase for migration
    file.clear();
    file.seekg(0);
    std::regex re2(R"(class\s+(\w+)\s*:\s*public\s+LogosProviderBase)");
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, re2))
            return match[1].str();
    }
    return "";
}

// --provider-dispatch: generates native callMethod/getMethodsJson
static int generateProviderDispatch(const std::string& headerPath, const std::string& outputDir)
{
    auto methods = parseHeader(headerPath);
    if (methods.empty()) {
        std::cerr << "No LOGOS_METHOD markers found in: " << headerPath << "\n";
        return 3;
    }

    std::string className = findClassName(headerPath);
    if (className.empty()) {
        std::cerr << "Could not find class inheriting NativeProviderBase in: " << headerPath << "\n";
        return 4;
    }

    std::string headerBaseName = fs::path(headerPath).filename().string();
    std::string genDir = outputDir.empty() ? fs::path(headerPath).parent_path().string() : outputDir;
    fs::create_directories(genDir);

    std::ostringstream s;
    s << "// AUTO-GENERATED by logos-native-generator -- do not edit\n";
    s << "#include \"" << headerBaseName << "\"\n";
    s << "#include \"logos_value.h\"\n";
    s << "#include \"logos_native_types.h\"\n\n";

    // Group by name for overloads
    std::map<std::string, std::vector<const ParsedMethod*>> byName;
    for (const auto& m : methods)
        byName[m.name].push_back(&m);

    s << "LogosValue " << className << "::callMethod(\n";
    s << "    const std::string& methodName, const std::vector<LogosValue>& args)\n";
    s << "{\n";
    for (const auto& [name, overloads] : byName) {
        s << "    if (methodName == \"" << name << "\") {\n";
        bool multi = overloads.size() > 1;
        for (const auto* m : overloads) {
            std::string nativeRet = mapToNativeType(m->returnType);
            if (multi) {
                s << "        if (args.size() == " << m->params.size() << ") {\n    ";
            }
            if (nativeRet == "void") {
                s << "        " << m->name << "(";
                for (size_t i = 0; i < m->params.size(); ++i) {
                    std::string nt = mapToNativeType(m->params[i].first);
                    s << argConversion(nt, "args.at(" + std::to_string(i) + ")");
                    if (i + 1 < m->params.size()) s << ", ";
                }
                s << ");\n";
                if (multi) s << "    ";
                s << "        return LogosValue(true);\n";
            } else if (nativeRet == "NativeLogosResult") {
                s << "        {\n";
                if (multi) s << "    ";
                s << "            NativeLogosResult _r = " << m->name << "(";
                for (size_t i = 0; i < m->params.size(); ++i) {
                    std::string nt = mapToNativeType(m->params[i].first);
                    s << argConversion(nt, "args.at(" + std::to_string(i) + ")");
                    if (i + 1 < m->params.size()) s << ", ";
                }
                s << ");\n";
                if (multi) s << "    ";
                s << "            LogosValue::Map _m;\n";
                if (multi) s << "    ";
                s << "            _m[\"success\"] = LogosValue(_r.success);\n";
                if (multi) s << "    ";
                s << "            _m[\"value\"] = _r.value;\n";
                if (multi) s << "    ";
                s << "            _m[\"error\"] = LogosValue(_r.error);\n";
                if (multi) s << "    ";
                s << "            return LogosValue(_m);\n";
                if (multi) s << "    ";
                s << "        }\n";
            } else {
                std::string call = m->name + "(";
                for (size_t i = 0; i < m->params.size(); ++i) {
                    std::string nt = mapToNativeType(m->params[i].first);
                    call += argConversion(nt, "args.at(" + std::to_string(i) + ")");
                    if (i + 1 < m->params.size()) call += ", ";
                }
                call += ")";
                s << "        return " << returnWrap(nativeRet, call) << ";\n";
            }
            if (multi) s << "        }\n";
        }
        s << "    }\n";
    }
    s << "    return LogosValue();\n";
    s << "}\n\n";

    // getMethodsJson()
    s << "std::string " << className << "::getMethodsJson()\n";
    s << "{\n";
    s << "    return R\"LOGOS_JSON([";
    for (size_t i = 0; i < methods.size(); ++i) {
        const auto& m = methods[i];
        std::string nativeRet = mapToNativeType(m.returnType);
        s << "{\"name\":\"" << m.name << "\",\"returnType\":\"" << nativeRet
          << "\",\"isInvokable\":true,\"signature\":\"" << m.name << "(";
        for (size_t p = 0; p < m.params.size(); ++p) {
            s << mapToNativeType(m.params[p].first);
            if (p + 1 < m.params.size()) s << ",";
        }
        s << ")\"";
        if (!m.params.empty()) {
            s << ",\"parameters\":[";
            for (size_t p = 0; p < m.params.size(); ++p) {
                s << "{\"type\":\"" << mapToNativeType(m.params[p].first)
                  << "\",\"name\":\"" << m.params[p].second << "\"}";
                if (p + 1 < m.params.size()) s << ",";
            }
            s << "]";
        }
        s << "}";
        if (i + 1 < methods.size()) s << ",";
    }
    s << "])LOGOS_JSON\";\n";
    s << "}\n";

    std::string outPath = (fs::path(genDir) / "logos_provider_dispatch.cpp").string();
    std::ofstream out(outPath);
    if (!out.is_open()) {
        std::cerr << "Failed to write dispatch file: " << outPath << "\n";
        return 5;
    }
    out << s.str();
    out.close();

    std::cout << "Generated provider dispatch: " << outPath
              << " (" << methods.size() << " methods from " << className << ")\n";
    return 0;
}

// --consumer-wrappers: generates Qt-free consumer wrapper
static int generateConsumerWrappers(const std::string& headerPath,
                                    const std::string& moduleName,
                                    const std::string& outputDir)
{
    auto methods = parseHeader(headerPath);
    if (methods.empty()) {
        std::cerr << "No LOGOS_METHOD markers found in: " << headerPath << "\n";
        return 3;
    }

    std::string className = toPascalCase(moduleName);
    std::string genDir = outputDir.empty() ? fs::path(headerPath).parent_path().string() : outputDir;
    fs::create_directories(genDir);

    std::string headerRel = moduleName + "_api.h";
    std::string sourceRel = moduleName + "_api.cpp";

    // Generate header
    {
        std::ostringstream s;
        s << "#pragma once\n";
        s << "#include <string>\n";
        s << "#include <vector>\n";
        s << "#include <functional>\n";
        s << "#include \"logos_value.h\"\n";
        s << "#include \"logos_native_types.h\"\n\n";
        s << "class NativeLogosClient;\n";
        s << "class NativeLogosAPI;\n";
        s << "class LogosAPI;\n";
        s << "class LogosObject;\n\n";
        s << "class " << className << " {\n";
        s << "public:\n";
        s << "    explicit " << className << "(NativeLogosAPI* api);\n";
        s << "    explicit " << className << "(LogosAPI* api);\n\n";

        for (const auto& m : methods) {
            std::string nativeRet = mapToNativeType(m.returnType);
            s << "    " << nativeRet << " " << m.name << "(";
            for (size_t i = 0; i < m.params.size(); ++i) {
                std::string nt = mapToNativeType(m.params[i].first);
                if (shouldPassByConstRef(nt))
                    s << "const " << nt << "& " << m.params[i].second;
                else
                    s << nt << " " << m.params[i].second;
                if (i + 1 < m.params.size()) s << ", ";
            }
            s << ");\n";
        }

        s << "\n    using EventCallback = std::function<void(const std::string&, const std::vector<LogosValue>&)>;\n";
        s << "    bool on(const std::string& eventName, EventCallback callback);\n";
        s << "    void trigger(const std::string& eventName, const std::vector<LogosValue>& data = {});\n\n";

        s << "private:\n";
        s << "    NativeLogosAPI* m_api;\n";
        s << "    NativeLogosAPI* m_ownedApi = nullptr;\n";
        s << "    NativeLogosClient* m_client;\n";
        s << "    std::string m_moduleName;\n";
        s << "    LogosObject* m_eventReplica = nullptr;\n";
        s << "};\n";

        std::string headerPath = (fs::path(genDir) / headerRel).string();
        std::ofstream out(headerPath);
        if (!out.is_open()) {
            std::cerr << "Failed to write header: " << headerPath << "\n";
            return 5;
        }
        out << s.str();
    }

    // Generate source
    {
        std::ostringstream s;
        s << "#include \"" << headerRel << "\"\n";
        s << "#include \"logos_native_api.h\"\n";
        s << "#include \"logos_native_client.h\"\n\n";

        s << className << "::" << className << "(NativeLogosAPI* api)\n";
        s << "    : m_api(api), m_client(api->getClient(\"" << moduleName << "\")),\n";
        s << "      m_moduleName(\"" << moduleName << "\") {}\n\n";

        s << className << "::" << className << "(LogosAPI* api)\n";
        s << "    : m_ownedApi(new NativeLogosAPI(api)), m_api(m_ownedApi),\n";
        s << "      m_client(m_api->getClient(\"" << moduleName << "\")),\n";
        s << "      m_moduleName(\"" << moduleName << "\") {}\n\n";

        for (const auto& m : methods) {
            std::string nativeRet = mapToNativeType(m.returnType);
            s << nativeRet << " " << className << "::" << m.name << "(";
            for (size_t i = 0; i < m.params.size(); ++i) {
                std::string nt = mapToNativeType(m.params[i].first);
                if (shouldPassByConstRef(nt))
                    s << "const " << nt << "& " << m.params[i].second;
                else
                    s << nt << " " << m.params[i].second;
                if (i + 1 < m.params.size()) s << ", ";
            }
            s << ") {\n";

            // Build invocation
            if (nativeRet != "void") {
                s << "    LogosValue _result = ";
            } else {
                s << "    ";
            }

            if (m.params.empty()) {
                s << "m_client->invokeMethod(m_moduleName, \"" << m.name << "\");\n";
            } else if (m.params.size() <= 5) {
                s << "m_client->invokeMethod(m_moduleName, \"" << m.name << "\"";
                for (const auto& [ptype, pname] : m.params) {
                    std::string nt = mapToNativeType(ptype);
                    if (nt == "LogosValue")
                        s << ", " << pname;
                    else
                        s << ", LogosValue(" << pname << ")";
                }
                s << ");\n";
            } else {
                s << "m_client->invokeMethod(m_moduleName, \"" << m.name << "\", std::vector<LogosValue>{";
                for (size_t i = 0; i < m.params.size(); ++i) {
                    std::string nt = mapToNativeType(m.params[i].first);
                    if (nt == "LogosValue")
                        s << m.params[i].second;
                    else
                        s << "LogosValue(" << m.params[i].second << ")";
                    if (i + 1 < m.params.size()) s << ", ";
                }
                s << "});\n";
            }

            // Return conversion
            if (nativeRet == "void") {
                // nothing
            } else if (nativeRet == "NativeLogosResult") {
                s << "    NativeLogosResult _nr;\n";
                s << "    if (_result.isMap()) {\n";
                s << "        auto _m = _result.toMap();\n";
                s << "        auto _sIt = _m.find(\"success\");\n";
                s << "        _nr.success = (_sIt != _m.end()) ? _sIt->second.toBool() : false;\n";
                s << "        auto _vIt = _m.find(\"value\");\n";
                s << "        _nr.value = (_vIt != _m.end()) ? _vIt->second : LogosValue();\n";
                s << "        auto _eIt = _m.find(\"error\");\n";
                s << "        _nr.error = (_eIt != _m.end()) ? _eIt->second.toString() : \"\";\n";
                s << "    }\n";
                s << "    return _nr;\n";
            } else {
                s << "    return " << returnUnwrap(nativeRet, "_result") << ";\n";
            }

            s << "}\n\n";
        }

        // Event methods
        s << "bool " << className << "::on(const std::string& eventName, EventCallback callback) {\n";
        s << "    if (!callback) return false;\n";
        s << "    if (!m_eventReplica) {\n";
        s << "        m_eventReplica = m_client->requestObject(m_moduleName);\n";
        s << "        if (!m_eventReplica) return false;\n";
        s << "    }\n";
        s << "    m_client->onEvent(m_eventReplica, eventName, callback);\n";
        s << "    return true;\n";
        s << "}\n\n";

        s << "void " << className << "::trigger(const std::string& eventName, const std::vector<LogosValue>& data) {\n";
        s << "    if (!m_eventReplica) {\n";
        s << "        m_eventReplica = m_client->requestObject(m_moduleName);\n";
        s << "        if (!m_eventReplica) return;\n";
        s << "    }\n";
        s << "    m_client->onEventResponse(m_eventReplica, eventName, data);\n";
        s << "}\n";

        std::string sourcePath = (fs::path(genDir) / sourceRel).string();
        std::ofstream out(sourcePath);
        if (!out.is_open()) {
            std::cerr << "Failed to write source: " << sourcePath << "\n";
            return 6;
        }
        out << s.str();
    }

    std::cout << "Generated consumer wrappers: " << (fs::path(genDir) / headerRel).string()
              << " and " << (fs::path(genDir) / sourceRel).string() << "\n";
    return 0;
}

// --umbrella: generates logos_sdk.h/cpp for native modules
static int generateUmbrella(const std::string& outputDir, const std::vector<std::string>& deps)
{
    if (outputDir.empty()) {
        std::cerr << "--umbrella requires --output-dir\n";
        return 1;
    }
    fs::create_directories(outputDir);

    // Generate logos_sdk.h
    {
        std::ostringstream s;
        s << "#pragma once\n";
        s << "#include \"logos_native_api.h\"\n\n";

        for (const auto& dep : deps) {
            s << "#include \"" << dep << "_api.h\"\n";
        }
        s << "\n";

        s << "struct LogosModules {\n";
        s << "    explicit LogosModules(NativeLogosAPI* api) : api(api)";
        for (const auto& dep : deps) {
            s << ",\n        " << dep << "(api)";
        }
        s << " {}\n";
        s << "    NativeLogosAPI* api;\n";
        for (const auto& dep : deps) {
            std::string className = toPascalCase(dep);
            s << "    " << className << " " << dep << ";\n";
        }
        s << "};\n";

        std::string path = (fs::path(outputDir) / "logos_sdk.h").string();
        std::ofstream out(path);
        if (!out.is_open()) {
            std::cerr << "Failed to write umbrella header: " << path << "\n";
            return 5;
        }
        out << s.str();
    }

    // Generate logos_sdk.cpp
    {
        std::ostringstream s;
        s << "#include \"logos_sdk.h\"\n\n";
        for (const auto& dep : deps) {
            s << "#include \"" << dep << "_api.cpp\"\n";
        }
        s << "\n";

        std::string path = (fs::path(outputDir) / "logos_sdk.cpp").string();
        std::ofstream out(path);
        if (!out.is_open()) {
            std::cerr << "Failed to write umbrella source: " << path << "\n";
            return 6;
        }
        out << s.str();
    }

    std::cout << "Generated umbrella: logos_sdk.h and logos_sdk.cpp in " << outputDir << "\n";
    return 0;
}

static void printUsage(const char* progName)
{
    std::cerr << "Usage:\n"
              << "  " << progName << " --provider-dispatch <header> [--output-dir <dir>]\n"
              << "  " << progName << " --consumer-wrappers <header> --module-name <name> [--output-dir <dir>]\n"
              << "  " << progName << " --umbrella --deps <dep1,dep2,...> --output-dir <dir>\n";
}

static std::string getArg(const std::vector<std::string>& args, const std::string& flag)
{
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == flag && i + 1 < args.size())
            return args[i + 1];
    }
    return "";
}

static bool hasFlag(const std::vector<std::string>& args, const std::string& flag)
{
    for (const auto& a : args)
        if (a == flag) return true;
    return false;
}

int main(int argc, char* argv[])
{
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i)
        args.emplace_back(argv[i]);

    std::string outputDir = getArg(args, "--output-dir");

    if (hasFlag(args, "--provider-dispatch")) {
        std::string header = getArg(args, "--provider-dispatch");
        if (header.empty()) {
            printUsage(argv[0]);
            return 1;
        }
        return generateProviderDispatch(header, outputDir);
    }

    if (hasFlag(args, "--consumer-wrappers")) {
        std::string header = getArg(args, "--consumer-wrappers");
        std::string moduleName = getArg(args, "--module-name");
        if (header.empty() || moduleName.empty()) {
            printUsage(argv[0]);
            return 1;
        }
        return generateConsumerWrappers(header, moduleName, outputDir);
    }

    if (hasFlag(args, "--umbrella")) {
        std::string depsStr = getArg(args, "--deps");
        if (depsStr.empty() || outputDir.empty()) {
            printUsage(argv[0]);
            return 1;
        }
        std::vector<std::string> deps;
        std::istringstream dstream(depsStr);
        std::string dep;
        while (std::getline(dstream, dep, ',')) {
            dep = trim(dep);
            if (!dep.empty()) deps.push_back(dep);
        }
        return generateUmbrella(outputDir, deps);
    }

    printUsage(argv[0]);
    return 1;
}
