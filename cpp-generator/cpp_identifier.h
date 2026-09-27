#ifndef CPP_IDENTIFIER_H
#define CPP_IDENTIFIER_H

// A LIDL name as a C++ identifier. The wire keeps the LIDL spelling; the C++
// identifier gets `_` appended when it is a name the generated code declares
// in the same scope (`reserved`), until it is not and no other name of the
// scope spells it. A usable name never changes.

#include <QSet>
#include <QString>
#include <QStringList>

// The C++ spellings of one scope's LIDL names, index-aligned with `names`.
inline QStringList lidlCppNames(const QStringList& names, const QSet<QString>& reserved = {})
{
    auto clashes = [&](const QString& n) { return reserved.contains(n); };
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

#endif // CPP_IDENTIFIER_H
