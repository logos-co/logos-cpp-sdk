#ifndef LOGOS_MODE_H
#define LOGOS_MODE_H

#include <QDebug>

/**
 * @brief LogosMode defines the communication mode for the SDK
 * 
 * - Remote: Uses QRemoteObjects for inter-process communication (desktop apps)
 * - Local: Uses in-process PluginRegistry (mobile apps, single process)
 */
enum class LogosMode {
    Remote,  // Default: Use QRemoteObjects (IPC)
    Local    // Use in-process PluginRegistry
};

/**
 * @brief LogosModeConfig provides global mode configuration
 * 
 * Set the mode once at application startup before creating any LogosAPI instances.
 * 
 * Example usage:
 *   LogosModeConfig::setMode(LogosMode::Local);  // For mobile apps
 *   LogosAPI api("my_module");  // Will use local mode
 */
namespace LogosModeConfig {

    /**
     * @brief Get the current mode (internal storage)
     * @return Reference to the static mode variable
     */
    inline LogosMode& modeStorage() {
        static LogosMode mode = LogosMode::Remote;  // Default to Remote
        return mode;
    }

    /**
     * @brief Set the SDK communication mode
     * @param mode The mode to use (Remote or Local)
     * 
     * Call this before creating any LogosAPI instances.
     */
    inline void setMode(LogosMode mode) {
        modeStorage() = mode;
        qDebug() << "LogosModeConfig: Mode set to" << (mode == LogosMode::Local ? "Local" : "Remote");
    }

    /**
     * @brief Get the current SDK communication mode
     * @return The current mode
     */
    inline LogosMode getMode() {
        return modeStorage();
    }

    /**
     * @brief Check if the SDK is in Local mode
     * @return true if Local mode, false if Remote mode
     */
    inline bool isLocal() {
        return modeStorage() == LogosMode::Local;
    }

    /**
     * @brief Check if the SDK is in Remote mode
     * @return true if Remote mode, false if Local mode
     */
    inline bool isRemote() {
        return modeStorage() == LogosMode::Remote;
    }

}

#endif // LOGOS_MODE_H
