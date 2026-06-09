#pragma once
#include <string>
#include <cstdint>

// Parser tests only read this as text. Regression fixture for issue #76:
// a section specifier and a declaration collapsed onto one *physical* line
// (as clang-format / prettier produce) must be parsed exactly like the
// newline-separated form. The code after the colon must NOT be discarded,
// otherwise the same valid C++ is processed differently based on formatting.
class SameLineEventsImpl {
public:
    SameLineEventsImpl() = default;

    // Access specifier + declaration on one line: the method is still found.
    public: std::string greet(const std::string& name);

    // The exact prettier-formatted form from the issue: `logos_events`,
    // a space, the colon, then the event prototype — all on one line. The
    // `///` doc comment above must still attach to the event: in the
    // collapsed form there is nowhere else to put it, so dropping it would
    // make documentation formatting-dependent too.
    /// Fired once the latest version is known.
    logos_events : void versionReady(const std::string &version);

    // A further event after the section is already open (still same-line).
    void downloadProgress(const std::string &id, int64_t percent);

    // The newline-separated form keeps working alongside the collapsed form.
logos_events:
    void shutdown();
};
