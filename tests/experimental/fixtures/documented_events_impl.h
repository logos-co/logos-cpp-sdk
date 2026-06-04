#pragma once
#include <string>

// Parser tests only read this as text. Exercises doc-comment capture on
// events declared in a `logos_events:` section (mirrors how `///` comments
// above methods become a method's description).
class DocumentedEventsImpl {
public:
    DocumentedEventsImpl() = default;

    void doWork();

logos_events:
    /// Fired once the user has authenticated.
    /// Carries the freshly issued session token.
    void userLoggedIn(const std::string& userId, const std::string& token);

    // Plain (non-doc) comment — must NOT be captured as a description.
    void heartbeat();

    /// Single-line documented event.
    void shutdown();
};
