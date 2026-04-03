#include "c_header_parser.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

// Map a C type string to a LIDL TypeExpr
static TypeExpr cTypeToLidl(const QString& raw)
{
    QString t = raw.trimmed();
    // Remove trailing const (rare but possible)
    // Normalize pointer types
    t.replace(QRegularExpression("\\s+"), " ");

    if (t == "const char*" || t == "const char *" || t == "char*" || t == "char *")
        return {TypeExpr::Primitive, "tstr", {}};
    if (t == "int64_t" || t == "long long" || t == "int32_t" || t == "int")
        return {TypeExpr::Primitive, "int", {}};
    if (t == "uint64_t" || t == "unsigned long long" || t == "uint32_t" || t == "unsigned int" || t == "size_t")
        return {TypeExpr::Primitive, "uint", {}};
    if (t == "double" || t == "float")
        return {TypeExpr::Primitive, "float64", {}};
    if (t == "bool" || t == "_Bool")
        return {TypeExpr::Primitive, "bool", {}};
    if (t == "void")
        return {TypeExpr::Primitive, "void", {}};

    return {TypeExpr::Primitive, "any", {}};
}

// Split a C parameter list respecting nested parens (should not appear but just in case)
static QStringList splitParams(const QString& params)
{
    QStringList result;
    int depth = 0;
    int start = 0;
    for (int i = 0; i < params.size(); ++i) {
        QChar c = params[i];
        if (c == '(') ++depth;
        else if (c == ')') --depth;
        else if (c == ',' && depth == 0) {
            result.append(params.mid(start, i - start).trimmed());
            start = i + 1;
        }
    }
    QString last = params.mid(start).trimmed();
    if (!last.isEmpty() && last != "void")
        result.append(last);
    return result;
}

// Parse a single parameter like "int64_t a" or "const char* name"
static bool parseCParam(const QString& raw, ParamDecl& out)
{
    QString s = raw.trimmed();
    if (s.isEmpty() || s == "void") return false;

    // Handle pointer types: find the last word as the param name
    // e.g. "const char* name" -> type="const char*", name="name"
    // e.g. "int64_t a" -> type="int64_t", name="a"

    // If there's a *, the name is after the last *
    int lastStar = s.lastIndexOf('*');
    if (lastStar >= 0) {
        QString typeStr = s.left(lastStar + 1).trimmed();
        QString nameStr = s.mid(lastStar + 1).trimmed();
        if (nameStr.isEmpty()) {
            // No param name — generate one
            nameStr = "arg";
        }
        out.name = nameStr;
        out.type = cTypeToLidl(typeStr);
        return true;
    }

    // No pointer: last word is name, rest is type
    int lastSpace = s.lastIndexOf(' ');
    if (lastSpace < 0) {
        // Single word — treat as type with generated name
        out.name = "arg";
        out.type = cTypeToLidl(s);
        return true;
    }

    out.type = cTypeToLidl(s.left(lastSpace).trimmed());
    out.name = s.mid(lastSpace + 1).trimmed();
    return true;
}

// Extract return type from a function declaration line.
// Returns the return type string, and sets funcRest to "funcname(params)"
static QString extractReturnType(const QString& line, QString& funcRest)
{
    // Find the opening paren
    int parenIdx = line.indexOf('(');
    if (parenIdx < 0) return QString();

    // Everything before '(' is "returnType funcName"
    QString beforeParen = line.left(parenIdx).trimmed();

    // Last word before '(' is the function name
    int lastSpace = beforeParen.lastIndexOf(' ');
    int lastStar = beforeParen.lastIndexOf('*');
    int splitPos = qMax(lastSpace, lastStar);

    if (splitPos < 0) return QString(); // Can't separate return type from name

    QString retType = beforeParen.left(splitPos + 1).trimmed();
    QString funcName = beforeParen.mid(splitPos + 1).trimmed();
    funcRest = funcName + line.mid(parenIdx);

    return retType;
}

CHeaderParseResult parseCHeader(const QString& headerPath,
                                const QString& prefixArg,
                                const QString& metadataPath,
                                QTextStream& err)
{
    CHeaderParseResult result;

    // Read metadata.json
    QFile metaFile(metadataPath);
    if (!metaFile.open(QIODevice::ReadOnly)) {
        result.error = "Cannot open metadata file: " + metadataPath;
        return result;
    }
    QJsonParseError jsonErr;
    QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll(), &jsonErr);
    if (doc.isNull()) {
        result.error = "Invalid metadata JSON in " + metadataPath + ": " + jsonErr.errorString();
        return result;
    }
    QJsonObject meta = doc.object();
    result.module.name = meta["name"].toString();
    result.module.version = meta["version"].toString("1.0.0");
    result.module.description = meta["description"].toString();
    result.module.category = meta["category"].toString();
    QJsonArray depsArr = meta["dependencies"].toArray();
    for (const QJsonValue& v : depsArr)
        result.module.depends.push_back(v.toString());

    // Determine prefix
    QString prefix = prefixArg;
    if (prefix.isEmpty()) {
        // Check for c_prefix in metadata.json nix section
        QJsonObject nix = meta["nix"].toObject();
        prefix = nix["c_prefix"].toString();
    }
    if (prefix.isEmpty()) {
        // Auto-derive from module name: "rust_calc_module" -> "rust_calc_"
        // Heuristic: strip "_module" suffix, then add "_"
        QString name = result.module.name;
        if (name.endsWith("_module"))
            name = name.left(name.length() - 7); // remove "_module"
        prefix = name + "_";
    }
    result.prefix = prefix;

    // Read header file
    QFile headerFile(headerPath);
    if (!headerFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = "Cannot open header file: " + headerPath;
        return result;
    }
    QString content = QString::fromUtf8(headerFile.readAll());
    QStringList lines = content.split('\n');

    // Reserved method names that conflict with PluginInterface
    QSet<QString> reservedNames = {"name", "version", "initLogos"};

    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();

        // Skip empty lines, preprocessor, comments, extern "C", braces
        if (line.isEmpty()) continue;
        if (line.startsWith('#')) continue;
        if (line.startsWith("//")) continue;
        if (line.startsWith("/*")) continue;
        if (line.startsWith("extern")) continue;
        if (line == "{" || line == "}") continue;
        if (line.startsWith("typedef")) continue;

        // Must end with ';' (function declaration)
        if (!line.endsWith(';')) continue;
        line.chop(1); // remove ';'

        // Extract return type and function signature
        QString funcRest;
        QString retTypeStr = extractReturnType(line, funcRest);
        if (retTypeStr.isEmpty()) continue;

        // Extract function name and params
        int parenOpen = funcRest.indexOf('(');
        int parenClose = funcRest.lastIndexOf(')');
        if (parenOpen < 0 || parenClose < 0) continue;

        QString funcName = funcRest.left(parenOpen).trimmed();
        QString paramsStr = funcRest.mid(parenOpen + 1, parenClose - parenOpen - 1).trimmed();

        // Must start with prefix
        if (!funcName.startsWith(prefix)) continue;

        // Strip prefix to get method name
        QString methodName = funcName.mid(prefix.length());
        if (methodName.isEmpty()) continue;

        // Skip reserved names — rename with "lib" prefix
        if (reservedNames.contains(methodName)) {
            methodName = QStringLiteral("lib") + methodName.at(0).toUpper() + methodName.mid(1);
        }

        // Parse return type
        TypeExpr retType = cTypeToLidl(retTypeStr);

        // Parse parameters
        QVector<ParamDecl> params;
        QStringList paramParts = splitParams(paramsStr);
        int argIdx = 0;
        for (const QString& p : paramParts) {
            ParamDecl pd;
            if (parseCParam(p, pd)) {
                // If param name is "arg", make it unique
                if (pd.name == "arg") {
                    pd.name = QString("arg%1").arg(argIdx);
                }
                params.push_back(pd);
                ++argIdx;
            }
        }

        MethodDecl method;
        method.name = methodName;
        method.returnType = retType;
        method.params = params;
        result.module.methods.push_back(method);
    }

    if (result.module.methods.empty()) {
        err << "Warning: No functions with prefix '" << prefix
            << "' found in " << headerPath << "\n";
    }

    return result;
}
