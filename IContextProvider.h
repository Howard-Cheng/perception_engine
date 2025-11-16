#pragma once

#include "third-party/include/json.hpp"
#include <string>
#include <memory>

/**
 * @brief Context Provider Interface
 * 
 * All context providers (system info, voice, camera, etc.) implement this interface
 */
class IContextProvider {
public:
    virtual ~IContextProvider() = default;
    
    /**
     * @brief Initialize the context provider
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Update context data (if periodic refresh needed)
     */
    virtual void update() = 0;
    
    /**
     * @brief Collect current context data
     * @param context Json object to populate with context data
     */
    virtual void collectContext(Json& context) const = 0;
    
    /**
     * @brief Get provider name (for logging and debugging)
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Check if provider is available
     */
    virtual bool isAvailable() const = 0;
    
    /**
     * @brief Shutdown and cleanup resources
     */
    virtual void shutdown() = 0;
};

using ContextProviderPtr = std::shared_ptr<IContextProvider>;
