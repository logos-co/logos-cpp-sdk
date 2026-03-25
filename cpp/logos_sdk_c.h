#ifndef LOGOS_SDK_C_H
#define LOGOS_SDK_C_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * C callback type matching the AsyncCallback in logos_core.h:
 *   void(int result, const char* message, void* user_data)
 */
typedef void (*LogosSdkCallback)(int result, const char* message, void* user_data);

/**
 * Call a plugin method asynchronously.
 *
 * @param plugin_name  Target plugin name
 * @param method_name  Method to call
 * @param params_json  JSON array of [{name,value,type},...] parameters (may be NULL for "[]")
 * @param callback     Receives (1=success/0=failure, message, user_data) on completion
 * @param user_data    Opaque pointer passed through to callback
 */
void logos_sdk_call_method_async(
    const char* plugin_name,
    const char* method_name,
    const char* params_json,
    LogosSdkCallback callback,
    void* user_data
);

/**
 * Register an event listener for a specific event from a plugin.
 *
 * @param plugin_name  Plugin that emits the event
 * @param event_name   Event name to listen for
 * @param callback     Receives (1, json_event_data, user_data) each time event fires
 * @param user_data    Opaque pointer passed through to callback
 */
void logos_sdk_register_event(
    const char* plugin_name,
    const char* event_name,
    LogosSdkCallback callback,
    void* user_data
);

/**
 * Shut down the SDK's internal core client, releasing connections.
 * Safe to call multiple times. After this, async calls will lazily re-create
 * the client on next use.
 */
void logos_sdk_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // LOGOS_SDK_C_H
