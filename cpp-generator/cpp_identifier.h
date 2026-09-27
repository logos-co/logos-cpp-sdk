#ifndef CPP_IDENTIFIER_H
#define CPP_IDENTIFIER_H

// A LIDL name as a C++ identifier. The wire keeps the LIDL spelling; the C++
// identifier gets `_` appended when it is a C++ keyword or a name the generated
// code declares in the same scope (`reserved`), until it is neither and no
// other name of the scope spells it. A usable name never changes, and names
// derived from one (`<method>Async`, `on<Event>`) keep the LIDL spelling.

#include <QSet>
#include <QString>
#include <QStringList>

// C++20, alternative tokens included: generated code must also build as C++20.
inline bool lidlIsCppKeyword(const QString& name)
{
    static const QSet<QString> keywords = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
        "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
        "protected", "public", "register", "reinterpret_cast", "requires", "return",
        "short", "signed", "sizeof", "static", "static_assert", "static_cast",
        "struct", "switch", "template", "this", "thread_local", "throw", "true",
        "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
        "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
    };
    return keywords.contains(name);
}

// The C++ spellings of one scope's LIDL names, index-aligned with `names`.
inline QStringList lidlCppNames(const QStringList& names, const QSet<QString>& reserved = {})
{
    auto clashes = [&](const QString& n) { return lidlIsCppKeyword(n) || reserved.contains(n); };
    QSet<QString> taken;
    for (const QString& n : names)
        if (!clashes(n)) taken.insert(n);
    QStringList out;
    for (const QString& n : names) {
        QString cpp = n;
        if (clashes(cpp)) {
            cpp += '_';
            while (clashes(cpp) || taken.contains(cpp)) cpp += '_';
            taken.insert(cpp);
        }
        out << cpp;
    }
    return out;
}

// `name`'s spelling in the scope `scope` (which should contain it).
inline QString lidlCppName(const QString& name, const QStringList& scope)
{
    const int i = scope.indexOf(name);
    return i < 0 ? lidlCppNames({name}).first() : lidlCppNames(scope).at(i);
}

#endif // CPP_IDENTIFIER_H
