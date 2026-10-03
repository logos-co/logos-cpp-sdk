// --typed-collections at the two layers it touches: which slots lidl_to_json
// annotates, and what the lp emitter does with the annotation. That the
// output compiles and round-trips is tests/clients' job.

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "generator_lib.h"
#include "lidl_to_json.h"

namespace {

const char* kContract = R"(
module probe {
  version "1.0.0"
  type P {
    x: float64
  }
  type R {
    name: tstr
    tags: [int]
    ? maybe: [uint]
    at: P
    ats: [P]
  }
  method ints(v: [int]) -> [int]
  method counts(v: {tstr: uint}) -> {tstr: uint}
  method greet(v: ?tstr) -> ?tstr
  method grid(v: [[P]]) -> ?P
  method names(v: [tstr]) -> [tstr]
  method anys(a: [any], m: {tstr: any}, o: ?any) -> any
  method recs(p: P, ps: [P], pm: {tstr: P}) -> R
  method keyed(v: {int: tstr}) -> tstr
  method scalar(v: uint) -> bstr
  event ticked(values: [uint], label: ?tstr)
}
)";

struct Surface {
    QJsonArray methods, events, records;
};

Surface surface(bool typed, const char* contract = kContract)
{
    const LidlParseResult pr = lidlParse(QString::fromUtf8(contract));
    EXPECT_FALSE(pr.hasError()) << pr.error;
    Surface s{moduleMethodsToJson(pr.module), moduleEventsToJson(pr.module),
              moduleRecordsToJson(pr.module)};
    if (typed) annotateTypedCollections(pr.module, "Probe::", s.methods, s.events, s.records);
    return s;
}

QJsonObject method(const Surface& s, const char* name)
{
    for (const QJsonValue& v : s.methods)
        if (v.toObject().value("name").toString() == name) return v.toObject();
    ADD_FAILURE() << "no method " << name;
    return {};
}

QString paramStd(const Surface& s, const char* name, int i)
{
    return method(s, name).value("parameters").toArray().at(i).toObject().value("stdType").toString();
}

QString returnStd(const Surface& s, const char* name)
{
    return method(s, name).value("returnStdType").toString();
}

QString fieldStd(const Surface& s, const char* record, const char* field)
{
    for (const QJsonValue& r : s.records) {
        if (r.toObject().value("name").toString() != record) continue;
        for (const QJsonValue& f : r.toObject().value("fields").toArray())
            if (f.toObject().value("name").toString() == field)
                return f.toObject().value("stdType").toString();
    }
    ADD_FAILURE() << "no field " << record << "." << field;
    return {};
}

} // namespace

TEST(TypedCollections, CollapsedSlotsGetTheirStdSpelling)
{
    const Surface s = surface(true);
    EXPECT_EQ(paramStd(s, "ints", 0), "std::vector<int64_t>");
    EXPECT_EQ(returnStd(s, "ints"), "std::vector<int64_t>");
    EXPECT_EQ(paramStd(s, "counts", 0), "std::map<std::string, uint64_t>");
    EXPECT_EQ(paramStd(s, "greet", 0), "std::optional<std::string>");
    EXPECT_EQ(returnStd(s, "greet"), "std::optional<std::string>");
    // Records inside are qualified: a return type precedes `Probe::`.
    EXPECT_EQ(paramStd(s, "grid", 0), "std::vector<std::vector<Probe::P>>");
    EXPECT_EQ(returnStd(s, "grid"), "std::optional<Probe::P>");
    EXPECT_EQ(fieldStd(s, "R", "tags"), "std::vector<int64_t>");
    EXPECT_EQ(fieldStd(s, "R", "maybe"), "std::vector<uint64_t>")
        << "a field's key types its value; optionality stays the flag";
    const QJsonArray ev = s.events.at(0).toObject().value("params").toArray();
    EXPECT_EQ(ev.at(0).toObject().value("stdType").toString(), "std::vector<uint64_t>");
    EXPECT_EQ(ev.at(1).toObject().value("stdType").toString(), "std::optional<std::string>");
}

// Everything the flat names already type, or that is untyped JSON by
// declaration, or that has no Qt-free spelling, is left alone.
TEST(TypedCollections, EverythingElseKeepsItsFlatSpelling)
{
    const Surface s = surface(true);
    EXPECT_TRUE(paramStd(s, "names", 0).isEmpty()) << "[tstr] is already std::vector<std::string>";
    for (int i = 0; i < 3; ++i) EXPECT_TRUE(paramStd(s, "anys", i).isEmpty()) << i;
    EXPECT_TRUE(returnStd(s, "anys").isEmpty());
    for (int i = 0; i < 3; ++i) EXPECT_TRUE(paramStd(s, "recs", i).isEmpty()) << "record path " << i;
    EXPECT_TRUE(returnStd(s, "recs").isEmpty());
    EXPECT_TRUE(paramStd(s, "keyed", 0).isEmpty()) << "a non-tstr key has no std::map spelling";
    EXPECT_TRUE(paramStd(s, "scalar", 0).isEmpty());
    EXPECT_TRUE(returnStd(s, "scalar").isEmpty());
    EXPECT_TRUE(fieldStd(s, "R", "name").isEmpty());
    EXPECT_TRUE(fieldStd(s, "R", "at").isEmpty());
    EXPECT_TRUE(fieldStd(s, "R", "ats").isEmpty());
}

TEST(TypedCollections, TheLpWrapperIsTypedThroughTheCodec)
{
    const Surface s = surface(true);
    const QString h = makeHeaderLp("probe", "Probe", s.methods, s.events, BindMode::Static, s.records);
    const QString c = makeSourceLp("probe", "Probe", "probe_api.h", s.methods, s.events,
                                   BindMode::Static, s.records);
    EXPECT_TRUE(h.contains("std::vector<int64_t> ints(const std::vector<int64_t>& v, "));
    EXPECT_TRUE(h.contains("std::optional<std::string> greet(const std::optional<std::string>& v, "));
    EXPECT_TRUE(h.contains("std::vector<int64_t> tags{};"));
    EXPECT_TRUE(h.contains("std::optional<std::vector<uint64_t>> maybe{};"));
    EXPECT_TRUE(h.contains("#include <optional>"));
    EXPECT_TRUE(h.contains("#include <map>"));
    EXPECT_TRUE(h.contains("explicit Probe(const std::string& origin);"));

    EXPECT_TRUE(c.contains("#include \"logos_codec.h\""));
    EXPECT_TRUE(c.contains("_args.push_back(logos::toJson<std::vector<int64_t>>(v));"));
    EXPECT_TRUE(c.contains("return logosTypedFromJson<std::vector<int64_t>>(_r);"));
    EXPECT_TRUE(c.contains("template <> struct Codec<Probe::P, void>"))
        << "a record inside a typed collection decodes through its conversions";
    EXPECT_EQ(c.count("T logosTypedFromJson("), 1);
}

TEST(TypedCollections, WithoutTheFlagNothingChanges)
{
    const Surface plain = surface(false);
    const QString h = makeHeaderLp("probe", "Probe", plain.methods, plain.events, BindMode::Static,
                                   plain.records);
    const QString c = makeSourceLp("probe", "Probe", "probe_api.h", plain.methods, plain.events,
                                   BindMode::Static, plain.records);
    EXPECT_TRUE(h.contains("LogosList ints(const LogosList& v, "));
    EXPECT_TRUE(h.contains("LogosMap greet(const LogosMap& v, "));
    EXPECT_FALSE(c.contains("logos_codec.h"));
    EXPECT_FALSE(c.contains("logosTypedFromJson"));
    EXPECT_FALSE(c.contains("struct Codec<"));
}

// A contract with nothing to type emits the same bytes with the flag as without.
TEST(TypedCollections, NothingToTypeMeansTheSameBytes)
{
    const char* scalars = R"(
module probe {
  version "1.0.0"
  type P {
    x: float64
    names: [tstr]
  }
  method get(id: uint, tag: tstr) -> P
  method all() -> [P]
  event changed(p: P, n: int)
}
)";
    const Surface a = surface(false, scalars), b = surface(true, scalars);
    EXPECT_EQ(makeHeaderLp("probe", "Probe", a.methods, a.events, BindMode::Static, a.records),
              makeHeaderLp("probe", "Probe", b.methods, b.events, BindMode::Static, b.records));
    EXPECT_EQ(makeSourceLp("probe", "Probe", "probe_api.h", a.methods, a.events, BindMode::Static, a.records),
              makeSourceLp("probe", "Probe", "probe_api.h", b.methods, b.events, BindMode::Static, b.records));
}

TEST(TypedCollections, TheClientNamesNoQt)
{
    const Surface s = surface(true);
    const QString out = makeHeaderLp("probe", "Probe", s.methods, s.events, BindMode::Static, s.records)
                      + makeSourceLp("probe", "Probe", "probe_api.h", s.methods, s.events,
                                     BindMode::Static, s.records);
    for (const char* qt : {"#include <Q", "QString", "QVariant", "LogosAPI", "logos_sdk.h"})
        EXPECT_FALSE(out.contains(qt)) << qt;
}
