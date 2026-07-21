#ifndef LOGOS_TOKEN_MANAGER_CONTEXT_H
#define LOGOS_TOKEN_MANAGER_CONTEXT_H

#include <functional>
#include <string>
#include <type_traits>

#include "logos_module_context.h"

// ---------------------------------------------------------------------------
// LogosTokenManagerContext — opt-in mixin for a TOKEN-AUTHORITY module.
//
// Only capability_module needs this. As a Qt-free universal (cdylib) module it
// cannot touch the host's Qt TokenManager / LogosAPI directly, yet its job is
// exactly to (a) read the host's stored auth tokens and (b) inform an ARBITRARY
// target module of a caller's freshly-minted token. This context bridges both
// through host-installed callbacks — the same injection mechanism
// LogosModuleContext uses for the event callback (see
// _logosCoreSetEmitEvent_): the generated cdylib provider, which runs in this
// module's OWN subprocess and holds the LogosAPI, installs the two bridge
// functions in onInit(); the impl calls the protected accessors below.
//
// The accessors no-op (return ""/false) when the impl is constructed outside a
// framework-provisioned context (unit tests), matching LogosModuleContext's
// empty-string fallbacks. Module code stays Qt-free: std::string only.
// ---------------------------------------------------------------------------
class LogosTokenManagerContext : public LogosModuleContext {
public:
    // Framework-only entry point — invoked by the generated provider's onInit
    // once the LogosAPI is available. Leading/trailing underscore signals "do
    // not call from user code", matching _logosCoreSetContext_ et al.
    void _logosCoreSetTokenBridge_(
        std::function<std::string(const std::string& moduleName)> getToken,
        std::function<bool(const std::string& target,
                           const std::string& targetToken,
                           const std::string& caller,
                           const std::string& newToken)> informModuleToken)
    {
        m_getToken = std::move(getToken);
        m_informModuleToken = std::move(informModuleToken);
    }

protected:
    // The host's stored auth token for `moduleName`, or "" when the module is
    // unknown (never loaded / never issued a token). A non-empty result doubles
    // as the "is this a known module identity?" check.
    std::string getToken(const std::string& moduleName) const {
        return m_getToken ? m_getToken(moduleName) : std::string();
    }

    // Tell `target` (authenticated with the host's `targetToken`) that `caller`
    // may call it using `newToken`. Returns false when the bridge is unwired or
    // the inform fails. The host implementation acquires the target's client on
    // its owner thread and marshals the call, so this is safe to call from a
    // concurrency:"multi" worker thread.
    bool informModuleToken(const std::string& target,
                           const std::string& targetToken,
                           const std::string& caller,
                           const std::string& newToken) const {
        return m_informModuleToken
            ? m_informModuleToken(target, targetToken, caller, newToken)
            : false;
    }

private:
    std::function<std::string(const std::string&)> m_getToken;
    std::function<bool(const std::string&, const std::string&,
                       const std::string&, const std::string&)> m_informModuleToken;
};

// ---------------------------------------------------------------------------
// _logos_codegen_::maybeSetTokenBridge — codegen helper, do not call directly.
// Same tag-dispatch trick as maybeSetContext / maybeSetEmitEvent: impls that
// don't inherit LogosTokenManagerContext fall through to the no-op overload and
// compile unchanged, so this addition is zero-cost for every other module.
// ---------------------------------------------------------------------------
namespace _logos_codegen_ {

template<class T>
inline auto maybeSetTokenBridge(
    T& impl,
    std::function<std::string(const std::string&)> getToken,
    std::function<bool(const std::string&, const std::string&,
                       const std::string&, const std::string&)> informModuleToken)
    -> std::enable_if_t<std::is_base_of_v<LogosTokenManagerContext, T>>
{
    static_cast<LogosTokenManagerContext&>(impl)._logosCoreSetTokenBridge_(
        std::move(getToken), std::move(informModuleToken));
}

template<class T>
inline auto maybeSetTokenBridge(
    T&,
    std::function<std::string(const std::string&)>,
    std::function<bool(const std::string&, const std::string&,
                       const std::string&, const std::string&)>)
    -> std::enable_if_t<!std::is_base_of_v<LogosTokenManagerContext, T>>
{
    // Module impl didn't opt into LogosTokenManagerContext; nothing to do.
}

} // namespace _logos_codegen_

#endif // LOGOS_TOKEN_MANAGER_CONTEXT_H
