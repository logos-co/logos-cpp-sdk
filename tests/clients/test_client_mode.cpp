// The client mode's output, compiled and run: `logos-cpp-generator --lidl
// typed_probe.lidl --api-style lp`, without (plain_client_tests) and with
// (typed_client_tests) --typed-collections. Neither target sees a Qt header.
//
// The lp_* C ABI is stubbed, and lp_invoke answers with the call's first
// argument, so every value goes out through the generated encode and comes
// back through the generated decode.

#include "typed_probe_api.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef LOGOS_TYPED_COLLECTIONS
#define LOGOS_TYPED_COLLECTIONS 0
#endif

namespace {

struct Stub {
    std::vector<std::pair<std::string, std::string>> created;  // (target, origin)
    std::string lastMethod;
    nlohmann::json lastArgs;
    bool answerSet = false;  // answer `answer` instead of echoing
    nlohmann::json answer;
    std::map<std::string, std::pair<lp_event_cb, void*>> events;
};
Stub g;

char* heapCopy(const std::string& s)
{
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

nlohmann::json answerFor(const char* method, const char* args)
{
    g.lastMethod = method;
    g.lastArgs = nlohmann::json::parse(args ? args : "[]");
    if (g.answerSet) return g.answer;
    return g.lastArgs.empty() ? nlohmann::json() : g.lastArgs.at(0);
}

void fire(const std::string& event, const nlohmann::json& payload)
{
    const auto it = g.events.find(event);
    ASSERT_NE(it, g.events.end()) << "nothing subscribed to " << event;
    it->second.first(event.c_str(), payload.dump().c_str(), it->second.second);
}

} // namespace

extern "C" {
lp_client* lp_client_create(const char* target, const char* origin, const char*, const char*)
{
    g.created.emplace_back(target, origin);
    return reinterpret_cast<lp_client*>(new int(0));
}
void lp_client_destroy(lp_client* client) { delete reinterpret_cast<int*>(client); }
int lp_invoke(lp_client*, const char* method, const char* args, int, char** out, char** err)
{
    if (out) *out = heapCopy(answerFor(method, args).dump());
    if (err) *err = nullptr;
    return LP_OK;
}
int lp_invoke_async(lp_client*, const char* method, const char* args, int, lp_result_cb cb,
                    void* ud)
{
    cb(1, answerFor(method, args).dump().c_str(), ud);
    return LP_OK;
}
lp_subscription* lp_subscribe(lp_client*, const char* event, lp_event_cb cb, void* ud)
{
    g.events[event] = {cb, ud};
    return reinterpret_cast<lp_subscription*>(new int(0));
}
void lp_unsubscribe(lp_subscription* sub) { delete reinterpret_cast<int*>(sub); }
void lp_string_free(char* s) { std::free(s); }
int lp_client_set_subscription_status_cb(lp_client*, lp_subscription_status_cb, void*) { return 1; }
unsigned long long lp_client_subscription_generation(lp_client*) { return 0; }
int lp_client_set_subscription_options(lp_client*, const char*) { return 1; }
int lp_client_rearm_subscriptions(lp_client*) { return 0; }
}

namespace {

using Point = TypedProbe::Point;
using Route = TypedProbe::Route;

constexpr uint64_t kBigUint = (uint64_t(1) << 40) + 3;  // needs more than 32 bits
const std::vector<uint8_t> kHighBytes = {0x00, 0xff, 0x80, 0x7f, 0xfe, 0x01};

class ClientMode : public ::testing::Test {
protected:
    void SetUp() override { g = Stub{}; }
    TypedProbe probe{"probe_app"};
};

void expectSame(const Point& a, const Point& b)
{
    EXPECT_DOUBLE_EQ(a.x, b.x);
    EXPECT_DOUBLE_EQ(a.y, b.y);
}

void expectSame(const std::vector<Point>& a, const std::vector<Point>& b)
{
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) expectSame(a[i], b[i]);
}

Route sampleRoute()
{
    Route r;
    r.name = "north";
    r.start = {1.5, -2.5};
    r.stops = {{1, 2}, {3, 4}};
    r.named = {{"home", {0, 0}}, {"work", {5.25, 6}}};
    r.via = std::vector<std::string>{"a", "b"};
    r.maybe_end = Point{7, 8};
#if LOGOS_TYPED_COLLECTIONS
    r.legs = {{{1, 1}}, {}, {{2, 2}, {3, 3}}};
    r.distances = {0, kBigUint, std::numeric_limits<uint64_t>::max()};
    r.tags = {-1, 0, std::numeric_limits<int64_t>::min()};
    r.blobs = {kHighBytes, {}};
    r.limits = std::map<std::string, uint64_t>{{"max", kBigUint}};
#else
    r.legs = nlohmann::json::array({nlohmann::json::array({{{"x", 1}, {"y", 1}}})});
    r.distances = nlohmann::json::array({0, kBigUint});
    r.tags = nlohmann::json::array({-1, 0});
    r.blobs = nlohmann::json::array({logos::bytesToJson(kHighBytes)});
    r.limits = nlohmann::json{{"max", kBigUint}};
#endif
    return r;
}

void expectSameRoute(const Route& a, const Route& b)
{
    EXPECT_EQ(a.name, b.name);
    expectSame(a.start, b.start);
    expectSame(a.stops, b.stops);
    ASSERT_EQ(a.named.size(), b.named.size());
    for (const auto& kv : b.named) expectSame(a.named.at(kv.first), kv.second);
    EXPECT_EQ(a.via, b.via);
    ASSERT_TRUE(a.maybe_end.has_value());
    expectSame(*a.maybe_end, *b.maybe_end);
#if LOGOS_TYPED_COLLECTIONS
    ASSERT_EQ(a.legs.size(), b.legs.size());
    for (size_t i = 0; i < a.legs.size(); ++i) expectSame(a.legs[i], b.legs[i]);
#else
    EXPECT_EQ(a.legs, b.legs);
#endif
    EXPECT_EQ(a.distances, b.distances);
    EXPECT_EQ(a.tags, b.tags);
    EXPECT_EQ(a.blobs, b.blobs);
    EXPECT_EQ(a.limits, b.limits);
}

// ── both flavours ────────────────────────────────────────────────────────────

TEST_F(ClientMode, TheClientCallsUnderTheOriginItWasGiven)
{
    static_assert(std::is_constructible_v<TypedProbe, const std::string&>);
    static_assert(!std::is_default_constructible_v<TypedProbe>);
    EXPECT_TRUE(g.created.empty()) << "the lp_client is created on first use";
    probe.echoUint(1);
    ASSERT_EQ(g.created.size(), 1u);
    EXPECT_EQ(g.created[0].first, "typed_probe");
    EXPECT_EQ(g.created[0].second, "probe_app");
}

TEST_F(ClientMode, AUintAboveTwoToThe32IsExact)
{
    for (const uint64_t v : {kBigUint, std::numeric_limits<uint64_t>::max()}) {
        logos::CallError err;
        EXPECT_EQ(probe.echoUint(v, &err), v);
        EXPECT_TRUE(err.ok());
        ASSERT_TRUE(g.lastArgs.at(0).is_number_unsigned());
        EXPECT_EQ(g.lastArgs.at(0).get<uint64_t>(), v);
    }
}

TEST_F(ClientMode, HighBytesTravelTagged)
{
    EXPECT_EQ(probe.echoBytes(kHighBytes), kHighBytes);
    EXPECT_TRUE(logos::isTaggedBytes(g.lastArgs.at(0))) << g.lastArgs.dump();
}

TEST_F(ClientMode, ARecordOfRecordsRoundTrips)
{
    const Route in = sampleRoute();
    const Route out = probe.echoRoute(in);
    expectSameRoute(out, in);

    const nlohmann::json& wire = g.lastArgs.at(0);
    EXPECT_EQ(wire.at("start"), (nlohmann::json{{"x", 1.5}, {"y", -2.5}}));
    EXPECT_EQ(wire.at("named").at("work").at("x"), 5.25);
    EXPECT_EQ(wire.at("distances").at(1).get<uint64_t>(), kBigUint);
    EXPECT_TRUE(logos::isTaggedBytes(wire.at("blobs").at(0))) << wire.dump();
}

TEST_F(ClientMode, AnOmittedOptionalFieldStaysEmpty)
{
    Route in = sampleRoute();
    in.via.reset();
    in.maybe_end.reset();
    const Route out = probe.echoRoute(in);
    EXPECT_FALSE(g.lastArgs.at(0).contains("via")) << "an empty named slot omits its key";
    EXPECT_FALSE(out.via.has_value());
    EXPECT_FALSE(out.maybe_end.has_value());
}

TEST_F(ClientMode, AMapOfRecordsRoundTrips)
{
    const std::map<std::string, Point> in = {{"a", {1, 2}}, {"b", {-3, 4.5}}};
    const auto out = probe.echoIndex(in);
    ASSERT_EQ(out.size(), 2u);
    expectSame(out.at("b"), in.at("b"));
    EXPECT_TRUE(g.lastArgs.at(0).at("a").is_object());
}

TEST_F(ClientMode, TheAsyncTwinsDecodeTheSameValue)
{
    const Route in = sampleRoute();
    bool called = false;
    probe.echoRouteAsync(in, [&](Route out) { expectSameRoute(out, in); called = true; });
    EXPECT_TRUE(called);

    called = false;
    probe.echoRouteAsyncResult(in, [&](logos::AsyncResult<Route> r) {
        EXPECT_TRUE(r.ok());
        expectSameRoute(r.value, in);
        called = true;
    });
    EXPECT_TRUE(called);
}

TEST_F(ClientMode, AnEventCarriesARecordOfRecords)
{
    std::vector<Route> seen;
    ASSERT_TRUE(probe.onRouted([&](const Route& r) { seen.push_back(r); }));
    probe.echoRoute(sampleRoute());
    fire("routed", nlohmann::json::array({g.lastArgs.at(0)}));
    ASSERT_EQ(seen.size(), 1u);
    expectSameRoute(seen[0], sampleRoute());
}

TEST_F(ClientMode, StringListsAndAnyKeepTheirSpelling)
{
    static_assert(std::is_same_v<decltype(probe.echoNames({})), std::vector<std::string>>);
    static_assert(std::is_same_v<decltype(probe.echoAny(LogosList())), LogosList>);
    EXPECT_EQ(probe.echoNames({"x", "y"}), (std::vector<std::string>{"x", "y"}));
    const LogosList mixed = nlohmann::json::array({1, "two", nullptr});
    EXPECT_EQ(probe.echoAny(mixed), mixed);
}

// ── the flat spellings, without --typed-collections ─────────────────────────

#if !LOGOS_TYPED_COLLECTIONS

TEST_F(ClientMode, CollectionsAndPositionalOptionalsStayUntyped)
{
    static_assert(std::is_same_v<decltype(probe.echoInts(LogosList())), LogosList>);
    static_assert(std::is_same_v<decltype(probe.echoCounts(LogosMap())), LogosMap>);
    static_assert(std::is_same_v<decltype(probe.greet(LogosMap())), LogosMap>);
    static_assert(std::is_same_v<decltype(Route::tags), LogosList>);
    const LogosList ints = nlohmann::json::array({-1, 2});
    EXPECT_EQ(probe.echoInts(ints), ints);
}

#else

// ── --typed-collections ──────────────────────────────────────────────────────

TEST_F(ClientMode, CollectionsAndPositionalOptionalsAreTyped)
{
    static_assert(std::is_same_v<decltype(probe.echoInts({})), std::vector<int64_t>>);
    static_assert(std::is_same_v<decltype(probe.echoUints({})), std::vector<uint64_t>>);
    static_assert(std::is_same_v<decltype(probe.echoBlobs({})),
                                 std::vector<std::vector<uint8_t>>>);
    static_assert(std::is_same_v<decltype(probe.echoCounts({})),
                                 std::map<std::string, uint64_t>>);
    static_assert(std::is_same_v<decltype(probe.echoGrid({})), std::vector<std::vector<Point>>>);
    static_assert(std::is_same_v<decltype(probe.greet(std::nullopt)), std::optional<std::string>>);
    static_assert(std::is_same_v<decltype(probe.echoMaybePoint(std::nullopt)),
                                 std::optional<Point>>);
    static_assert(std::is_same_v<decltype(Route::tags), std::vector<int64_t>>);
    static_assert(std::is_same_v<decltype(Route::limits),
                                 std::optional<std::map<std::string, uint64_t>>>);
}

TEST_F(ClientMode, TypedListsRoundTrip)
{
    const std::vector<int64_t> ints = {std::numeric_limits<int64_t>::min(), -1, 0,
                                       std::numeric_limits<int64_t>::max()};
    EXPECT_EQ(probe.echoInts(ints), ints);
    EXPECT_EQ(g.lastArgs.at(0), nlohmann::json(ints));

    const std::vector<uint64_t> uints = {0, kBigUint, std::numeric_limits<uint64_t>::max()};
    EXPECT_EQ(probe.echoUints(uints), uints);
    EXPECT_EQ(g.lastArgs.at(0).at(2).get<uint64_t>(), std::numeric_limits<uint64_t>::max());

    const std::vector<std::vector<uint8_t>> blobs = {kHighBytes, {}, {0x80}};
    EXPECT_EQ(probe.echoBlobs(blobs), blobs);
    for (const auto& e : g.lastArgs.at(0)) EXPECT_TRUE(logos::isTaggedBytes(e)) << e.dump();
}

TEST_F(ClientMode, TypedMapsRoundTrip)
{
    const std::map<std::string, uint64_t> counts = {{"big", kBigUint}, {"zero", 0}};
    EXPECT_EQ(probe.echoCounts(counts), counts);
    EXPECT_EQ(g.lastArgs.at(0).at("big").get<uint64_t>(), kBigUint);

    const std::map<std::string, std::vector<uint64_t>> buckets = {{"a", {kBigUint, 1}}, {"b", {}}};
    EXPECT_EQ(probe.echoBuckets(buckets), buckets);

    const std::map<std::string, std::vector<uint8_t>> chunks = {{"k", kHighBytes}};
    EXPECT_EQ(probe.echoChunks(chunks), chunks);
    EXPECT_TRUE(logos::isTaggedBytes(g.lastArgs.at(0).at("k"))) << g.lastArgs.dump();
}

TEST_F(ClientMode, TheOtherLeavesRoundTrip)
{
    const std::vector<bool> flags = {true, false, true};
    EXPECT_EQ(probe.echoFlags(flags), flags);
    const std::vector<double> reals = {0.5, -1.25, 3};
    EXPECT_EQ(probe.echoReals(reals), reals);

    const std::vector<std::optional<int64_t>> holes = {1, std::nullopt, -3};
    EXPECT_EQ(probe.echoHoles(holes), holes);
    EXPECT_TRUE(g.lastArgs.at(0).at(1).is_null()) << "an empty element keeps its position";

    const std::optional<std::vector<std::string>> names = std::vector<std::string>{"x"};
    EXPECT_EQ(probe.echoMaybeNames(names), names);
    EXPECT_FALSE(probe.echoMaybeNames(std::nullopt).has_value());
}

TEST_F(ClientMode, APositionalOptionalHasTwoStates)
{
    EXPECT_FALSE(probe.greet(std::nullopt).has_value());
    EXPECT_TRUE(g.lastArgs.at(0).is_null()) << "an empty positional slot is null";
    EXPECT_EQ(probe.greet(std::string("ada")), std::optional<std::string>("ada"));
    EXPECT_EQ(g.lastArgs.at(0), "ada");
}

TEST_F(ClientMode, RecordsNestInsideTypedCollections)
{
    const std::vector<std::vector<Point>> grid = {{{1, 2}}, {}, {{3, 4}, {5, 6}}};
    const auto out = probe.echoGrid(grid);
    ASSERT_EQ(out.size(), grid.size());
    for (size_t i = 0; i < grid.size(); ++i) expectSame(out[i], grid[i]);
    EXPECT_EQ(g.lastArgs.at(0).at(2).at(1), (nlohmann::json{{"x", 5}, {"y", 6}}));

    const auto some = probe.echoMaybePoint(Point{9, 10});
    ASSERT_TRUE(some.has_value());
    expectSame(*some, Point{9, 10});
    EXPECT_FALSE(probe.echoMaybePoint(std::nullopt).has_value());
}

// Lenient, as every other decode on this surface: a wrong shape anywhere in a
// typed value yields its default, and never throws out of a call or callback.
TEST_F(ClientMode, AWrongShapeDecodesToTheDefault)
{
    g.answerSet = true;
    for (const nlohmann::json& bad : {nlohmann::json{{"not", "a list"}},
                                      nlohmann::json::array({1, "two"}),
                                      nlohmann::json::array({-1.5})}) {
        g.answer = bad;
        logos::CallError err;
        EXPECT_TRUE(probe.echoInts({1}, &err).empty()) << bad.dump();
        EXPECT_TRUE(err.ok());
    }
    g.answer = 42;
    EXPECT_FALSE(probe.greet(std::string("x")).has_value());

    bool called = false;
    g.answer = "not a map";
    probe.echoCountsAsyncResult({}, [&](logos::AsyncResult<std::map<std::string, uint64_t>> r) {
        EXPECT_TRUE(r.ok());
        EXPECT_TRUE(r.value.empty());
        called = true;
    });
    EXPECT_TRUE(called);

    // Inside a record the bad field alone falls back.
    g.answer = {{"name", "kept"}, {"tags", nlohmann::json::array({"x"})}};
    const Route r = probe.echoRoute(Route{});
    EXPECT_EQ(r.name, "kept");
    EXPECT_TRUE(r.tags.empty());
}

TEST_F(ClientMode, ARejectionIsStillAnError)
{
    g.answerSet = true;
    g.answer = {{"code", "dispatch_failed"}, {"message", "bad list"}, {"origin", "typed_probe"}};
    logos::CallError err;
    EXPECT_TRUE(probe.echoInts({1, 2}, &err).empty());
    EXPECT_EQ(err.code, "dispatch_failed");
    EXPECT_EQ(err.message, "bad list");
}

TEST_F(ClientMode, TypedEventArgumentsDecode)
{
    std::vector<std::pair<std::vector<uint64_t>, std::optional<std::string>>> seen;
    ASSERT_TRUE(probe.onTicked([&](const std::vector<uint64_t>& values,
                                   const std::optional<std::string>& label) {
        seen.emplace_back(values, label);
    }));
    fire("ticked", nlohmann::json::array({nlohmann::json::array({1, kBigUint}), nullptr}));
    fire("ticked", nlohmann::json::array({nlohmann::json::array({5}), "lbl"}));
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0].first, (std::vector<uint64_t>{1, kBigUint}));
    EXPECT_FALSE(seen[0].second.has_value());
    EXPECT_EQ(seen[1].second, std::optional<std::string>("lbl"));
}

#endif

} // namespace
