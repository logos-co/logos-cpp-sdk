#include "c_header_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

// ---------------------------------------------------------------------------
// C type string → LIDL TypeExpr
// ---------------------------------------------------------------------------

// Returns true if the raw C type is a heap-allocated char* (needs free).
// false means const char* (static/borrowed) or any other type.
static bool isMutableCharPtr(const QString& raw)
{
    QString t = raw.trimmed();
    // Must be exactly "char*" or "char *" but NOT "const char*"
    if (t.contains("const"))
        return false;
    static QRegularExpression charPtrRe("^char\\s*\\*\\s*$");
    return charPtrRe.match(t).hasMatch();
}

static TypeExpr cTypeToLidl(const QString& raw)
{
    QString t = raw.trimmed();
    // Strip const, pointer stars, whitespace
    t.remove(QRegularExpression("\\bconst\\b"));
    t.remove(QRegularExpression("\\*"));
    t = t.trimmed();

    if (t == "void")                       return { TypeExpr::Primitive, "void", {} };
    if (t == "bool" || t == "_Bool")       return { TypeExpr::Primitive, "bool", {} };
    if (t == "char")                       return { TypeExpr::Primitive, "tstr", {} };
    if (t == "int64_t" || t == "int32_t" || t == "int" || t == "long")
                                           return { TypeExpr::Primitive, "int", {} };
    if (t == "uint64_t" || t == "uint32_t" || t == "unsigned int" || t == "unsigned long")
                                           return { TypeExpr::Primitive, "uint", {} };
    if (t == "double" || t == "float")     return { TypeExpr::Primitive, "float64", {} };

    // Fallback: opaque
    return { TypeExpr::Primitive, "any", {} };
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString defaultPrefixFromModuleName(const QString& moduleName)
{
    // "rust_example_module" → "rust_example_"
    // "my_module"           → "my_"
    QString n = moduleName;
    if (n.endsWith("_module"))
        n.chop(7); // strip "_module"
    return n + "_";
}

// Rename reserved Logos method names to avoid conflicts.
static QString safeMethodName(const QString& name)
{
    if (name == "version")    return "libVersion";
    if (name == "name")       return "libName";
    if (name == "initLogos")  return "libInitLogos";
    return name;
}

// ---------------------------------------------------------------------------
// Parse a single C function declaration line into a CHeaderMethod.
// Returns false if the line does not look like a function declaration or
// does not start with the expected prefix.
// ---------------------------------------------------------------------------

static bool parseCFunctionLine(const QString& line,
                                const QString& prefix,
                                CHeaderMethod& out)
{
    // Line should look like: <return_type> <funcname>(<params>);
    // Locate the opening paren (last one in the return-type+name part)
    int parenOpen = line.indexOf('(');
    if (parenOpen < 0)
        return false;

    int parenClose = line.lastIndexOf(')');
    if (parenClose < parenOpen)
        return false;

    QString beforeParen = line.left(parenOpen).trimmed();
    QString paramStr    = line.mid(parenOpen + 1, parenClose - parenOpen - 1).trimmed();

    // Extract function name (last identifier token in beforeParen)
    int nameEnd = beforeParen.size();
    while (nameEnd > 0 && beforeParen[nameEnd - 1].isSpace())
        nameEnd--;
    int nameStart = nameEnd;
    while (nameStart > 0 && (beforeParen[nameStart - 1].isLetterOrNumber() || beforeParen[nameStart - 1] == '_'))
        nameStart--;
    if (nameStart >= nameEnd)
        return false;

    QString funcName = beforeParen.mid(nameStart, nameEnd - nameStart);

    // Must start with the prefix
    if (!funcName.startsWith(prefix))
        return false;

    QString methodName = safeMethodName(funcName.mid(prefix.size()));
    if (methodName.isEmpty())
        return false;

    QString retTypeStr = beforeParen.left(nameStart).trimmed();

    out.cFunctionName    = funcName;
    out.returnsHeapString = isMutableCharPtr(retTypeStr);
    out.decl.name        = methodName;
    out.decl.returnType  = cTypeToLidl(retTypeStr);
    out.decl.jsonReturn  = false;
    out.decl.resultReturn = false;
    out.decl.params.clear();

    // Parse parameters  (void means no params)
    if (!paramStr.isEmpty() && paramStr != "void") {
        // Split by comma, not inside angle brackets
        QStringList parts;
        int start  = 0;
        int depth  = 0;
        for (int i = 0; i < paramStr.size(); ++i) {
            if      (paramStr[i] == '<') depth++;
            else if (paramStr[i] == '>') depth--;
            else if (paramStr[i] == ',' && depth == 0) {
                parts.append(paramStr.mid(start, i - start).trimmed());
                start = i + 1;
            }
        }
        parts.append(paramStr.mid(start).trimmed());

        for (const QString& part : parts) {
            if (part.isEmpty()) continue;
            QString p = part.trimmed();

            // Extract param name (last identifier)
            int pNameEnd = p.size();
            while (pNameEnd > 0 && p[pNameEnd - 1].isSpace())
                pNameEnd--;
            int pNameStart = pNameEnd;
            while (pNameStart > 0 && (p[pNameStart - 1].isLetterOrNumber() || p[pNameStart - 1] == '_'))
                pNameStart--;

            ParamDecl pd;
            if (pNameStart < pNameEnd) {
                pd.name = p.mid(pNameStart, pNameEnd - pNameStart);
                pd.type = cTypeToLidl(p.left(pNameStart));
            } else {
                // No name — synthesize one
                pd.name = "arg" + QString::number(out.decl.params.size());
                pd.type = cTypeToLidl(p);
            }
            out.decl.params.append(pd);
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

CHeaderParseResult parseCHeader(const QString& headerPath,
                                const QString& prefix,
                                const QString& metadataPath,
                                const QString& cHeaderInclude,
                                QTextStream& err)
{
    CHeaderParseResult result;
    result.cHeaderInclude = cHeaderInclude;

    // --- Read metadata.json (same logic as impl_header_parser) ---
    {
        QFile mf(metadataPath);
        if (!mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.error = "Failed to open metadata file: " + metadataPath;
            return result;
        }
        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(mf.readAll(), &pe);
        if (pe.error != QJsonParseError::NoError) {
            result.error = "Failed to parse metadata JSON: " + pe.errorString();
            return result;
        }
        QJsonObject obj = doc.object();
        result.module.name        = obj.value("name").toString();
        result.module.version     = obj.value("version").toString();
        result.module.description = obj.value("description").toString();
        result.module.category    = obj.value("category").toString();
        QJsonArray deps = obj.value("dependencies").toArray();
        for (const QJsonValue& v : deps)
            result.module.depends.append(v.toString());
    }

    // Determine the free-string function name for this prefix
    QString freeStringFuncName = prefix + "free_string";

    // --- Read and parse header ---
    QFile hf(headerPath);
    if (!hf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = "Failed to open header file: " + headerPath;
        return result;
    }
    QString source = QString::fromUtf8(hf.readAll());
    hf.close();

    QStringList lines = source.split('\n');

    // Remove block comments (/* ... */)
    {
        QString cleaned;
        bool inBlock = false;
        for (const QChar& ch : source) {
            if (!inBlock) {
                if (ch == '/' && cleaned.endsWith('/')) {
                    cleaned.chop(1); // remove the leading /
                    // check if this is // (line comment) - handled below
                    // actually keep it simple: block comments only
                }
            }
        }
        // Simple block comment stripper
        QString stripped;
        stripped.reserve(source.size());
        for (int i = 0; i < source.size(); ++i) {
            if (!inBlock) {
                if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*') {
                    inBlock = true;
                    ++i;
                } else {
                    stripped += source[i];
                }
            } else {
                if (i + 1 < source.size() && source[i] == '*' && source[i + 1] == '/') {
                    inBlock = false;
                    ++i;
                }
            }
        }
        lines = stripped.split('\n');
    }

    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();

        // Skip blank lines, preprocessor, comments, extern guards
        if (line.isEmpty())                    continue;
        if (line.startsWith("#"))              continue;
        if (line.startsWith("//"))             continue;
        if (line.startsWith("extern \"C\""))   continue;
        if (line == "{" || line == "}")        continue;

        // Must end with ';' (forward declaration / function prototype)
        if (!line.endsWith(';'))
            continue;

        QString decl = line.left(line.size() - 1).trimmed();

        // Skip typedefs and struct/union/enum
        if (decl.startsWith("typedef") || decl.startsWith("struct")
            || decl.startsWith("union")  || decl.startsWith("enum"))
            continue;

        // Must have parentheses — must look like a function prototype
        if (!decl.contains('('))
            continue;

        CHeaderMethod mth;
        if (!parseCFunctionLine(decl, prefix, mth))
            continue;

        // Check if this is the free-string helper — remember it, don't expose as method
        if (mth.cFunctionName == freeStringFuncName) {
            result.freeStringFunc = freeStringFuncName;
            continue;
        }

        result.methods.append(mth);
        result.module.methods.append(mth.decl);
    }

    if (result.methods.isEmpty()) {
        err << "Warning: no functions matching prefix '" << prefix
            << "' found in " << headerPath << "\n";
    }

    return result;
}
