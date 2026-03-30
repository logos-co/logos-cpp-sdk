#include "lidl_lexer.h"

#include <QHash>

static LidlToken makeToken(LidlToken::Type type, const QString& text, int line, int col)
{
    LidlToken t;
    t.type = type;
    t.text = text;
    t.line = line;
    t.column = col;
    return t;
}

static const QHash<QString, LidlToken::Type>& lidlKeywords()
{
    static const QHash<QString, LidlToken::Type> kw = {
        {"module",      LidlToken::Module},
        {"type",        LidlToken::TypeKw},
        {"method",      LidlToken::Method},
        {"event",       LidlToken::Event},
        {"version",     LidlToken::Version},
        {"description", LidlToken::Description},
        {"category",    LidlToken::Category},
        {"depends",     LidlToken::Depends},
    };
    return kw;
}

static bool isIdentStart(QChar c) { return c.isLetter() || c == '_'; }
static bool isIdentChar(QChar c) { return c.isLetterOrNumber() || c == '_'; }

LidlLexResult lidlTokenize(const QString& source)
{
    LidlLexResult result;
    int pos = 0, line = 1, col = 1;
    const int len = source.size();

    auto makeError = [&](const QString& msg) {
        result.error = msg;
        result.errorLine = line;
        result.errorColumn = col;
    };

    while (pos < len) {
        QChar c = source[pos];

        if (c == ' ' || c == '\t' || c == '\r') { ++pos; ++col; continue; }
        if (c == '\n') { ++pos; ++line; col = 1; continue; }
        if (c == ';') { while (pos < len && source[pos] != '\n') ++pos; continue; }

        int startLine = line, startCol = col;

        if (c == '-' && pos + 1 < len && source[pos + 1] == '>') {
            result.tokens.append(makeToken(LidlToken::Arrow, "->", startLine, startCol));
            pos += 2; col += 2; continue;
        }

        LidlToken::Type symType = LidlToken::Error;
        switch (c.unicode()) {
        case '{': symType = LidlToken::LBrace; break;
        case '}': symType = LidlToken::RBrace; break;
        case '(': symType = LidlToken::LParen; break;
        case ')': symType = LidlToken::RParen; break;
        case '[': symType = LidlToken::LBracket; break;
        case ']': symType = LidlToken::RBracket; break;
        case ':': symType = LidlToken::Colon; break;
        case ',': symType = LidlToken::Comma; break;
        case '?': symType = LidlToken::Question; break;
        default: break;
        }
        if (symType != LidlToken::Error) {
            result.tokens.append(makeToken(symType, QString(c), startLine, startCol));
            ++pos; ++col; continue;
        }

        if (c == '"') {
            ++pos; ++col;
            QString value;
            while (pos < len && source[pos] != '"') {
                if (source[pos] == '\n') { makeError("Unterminated string literal"); return result; }
                if (source[pos] == '\\' && pos + 1 < len) {
                    ++pos; ++col;
                    QChar esc = source[pos];
                    if (esc == 'n') value += '\n';
                    else if (esc == 't') value += '\t';
                    else if (esc == '\\') value += '\\';
                    else if (esc == '"') value += '"';
                    else value += esc;
                } else { value += source[pos]; }
                ++pos; ++col;
            }
            if (pos >= len) { makeError("Unterminated string literal"); return result; }
            ++pos; ++col;
            result.tokens.append(makeToken(LidlToken::StringLit, value, startLine, startCol));
            continue;
        }

        if (isIdentStart(c)) {
            int start = pos;
            while (pos < len && isIdentChar(source[pos])) { ++pos; ++col; }
            QString word = source.mid(start, pos - start);
            const auto& kw = lidlKeywords();
            auto it = kw.find(word);
            result.tokens.append(makeToken(it != kw.end() ? it.value() : LidlToken::Ident, word, startLine, startCol));
            continue;
        }

        makeError(QString("Unexpected character '%1'").arg(c));
        return result;
    }

    result.tokens.append(makeToken(LidlToken::Eof, QString(), line, col));
    return result;
}
