/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sal/config.h>

#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <rtl/ustrbuf.hxx>

#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <atomic>

namespace sw::ai {

/**
 * ErrorRecoveryManager - Comprehensive error handling and retry mechanisms
 * 
 * This class provides centralized error management for the AI Agent system,
 * implementing intelligent retry strategies, circuit breaker patterns, and
 * graceful degradation mechanisms across HTTP and WebSocket communications.
 * 
 * Design Principles:
 * - Intelligent Error Classification: Different strategies for different error types
 * - Circuit Breaker Pattern: Prevents cascading failures
 * - Exponential Backoff: Reduces system load during failures
 * - Graceful Degradation: Maintains functionality when possible
 * - Comprehensive Logging: Detailed error tracking for debugging
 */
class ErrorRecoveryManager
{
public:
    enum class ErrorType
    {
        NETWORK_TIMEOUT,          // Network request timeout
        CONNECTION_REFUSED,       // Server connection refused
        SERVICE_UNAVAILABLE,      // HTTP 503 or similar
        AUTHENTICATION_FAILED,    // HTTP 401/403
        RATE_LIMIT_EXCEEDED,     // HTTP 429
        BAD_REQUEST,             // HTTP 400 (client error)
        SERVER_ERROR,            // HTTP 500 (server error)
        WEBSOCKET_CLOSED,        // WebSocket connection closed
        PROTOCOL_ERROR,          // Protocol-level error
        RESOURCE_EXHAUSTED,      // System resource limits
        UNKNOWN_ERROR            // Unclassified error
    };

    enum class RecoveryStrategy
    {
        IMMEDIATE_RETRY,         // Retry immediately
        EXPONENTIAL_BACKOFF,     // Retry with increasing delays
        LINEAR_BACKOFF,          // Retry with fixed delay increases
        CIRCUIT_BREAKER,         // Stop trying after failure threshold
        GRACEFUL_DEGRADATION,    // Switch to offline/limited mode
        USER_INTERVENTION,       // Require user action
        NO_RETRY                 // Don't retry, fail immediately
    };

    enum class CircuitState
    {
        CLOSED,                  // Normal operation
        HALF_OPEN,              // Testing if service recovered
        OPEN                    // Service failed, blocking requests
    };

    struct ErrorContext
    {
        ErrorType eType;
        OUString sErrorMessage;
        OUString sRequestId;
        OUString sServiceName;      // "http", "websocket", "langgraph"
        sal_Int32 nErrorCode;
        sal_Int32 nAttemptCount;
        std::chrono::steady_clock::time_point aFirstAttempt;
        std::chrono::steady_clock::time_point aLastAttempt;
        std::map<OUString, OUString> aContext;  // Additional error context
        
        ErrorContext(ErrorType eT, const OUString& rsMessage, const OUString& rsService)
            : eType(eT), sErrorMessage(rsMessage), sServiceName(rsService)
            , nErrorCode(0), nAttemptCount(0)
            , aFirstAttempt(std::chrono::steady_clock::now())
            , aLastAttempt(std::chrono::steady_clock::now()) {}
    };

    struct RetryPolicy
    {
        RecoveryStrategy eStrategy;
        sal_Int32 nMaxRetries;
        sal_Int32 nInitialDelayMs;
        sal_Int32 nMaxDelayMs;
        double fBackoffMultiplier;      // For exponential backoff
        sal_Int32 nJitterMs;           // Random variance in delay
        bool bEnableCircuitBreaker;
        sal_Int32 nCircuitBreakerThreshold;
        sal_Int32 nCircuitBreakerTimeoutMs;
        
        RetryPolicy()
            : eStrategy(RecoveryStrategy::EXPONENTIAL_BACKOFF)
            , nMaxRetries(3), nInitialDelayMs(1000), nMaxDelayMs(60000)
            , fBackoffMultiplier(2.0), nJitterMs(500)
            , bEnableCircuitBreaker(true), nCircuitBreakerThreshold(5)
            , nCircuitBreakerTimeoutMs(60000) {}
    };

    struct CircuitBreakerState
    {
        CircuitState eState;
        sal_Int32 nFailureCount;
        sal_Int32 nSuccessCount;
        std::chrono::steady_clock::time_point aLastFailure;
        std::chrono::steady_clock::time_point aLastStateChange;
        
        CircuitBreakerState()
            : eState(CircuitState::CLOSED), nFailureCount(0), nSuccessCount(0)
            , aLastFailure(std::chrono::steady_clock::now())
            , aLastStateChange(std::chrono::steady_clock::now()) {}
    };

    // Callback function types
    using ErrorCallback = std::function<void(const ErrorContext&)>;
    using RecoveryCallback = std::function<bool(const ErrorContext&)>;
    using RetryCallback = std::function<bool(const ErrorContext&, sal_Int32)>;

private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    
    // Thread safety
    mutable std::mutex m_aMutex;
    
    // Error tracking and metrics
    std::map<OUString, ErrorContext> m_aActiveErrors;           // Request ID -> Error Context
    std::map<OUString, RetryPolicy> m_aRetryPolicies;          // Service -> Retry Policy
    std::map<OUString, CircuitBreakerState> m_aCircuitBreakers; // Service -> Circuit State
    
    // Configuration
    bool m_bEnabled;
    bool m_bEnableLogging;
    sal_Int32 m_nMaxConcurrentRetries;
    sal_Int32 m_nMaxErrorHistorySize;
    
    // Callbacks
    ErrorCallback m_aErrorCallback;
    RecoveryCallback m_aRecoveryCallback;
    RetryCallback m_aRetryCallback;
    
    // Statistics and monitoring
    struct ErrorStatistics
    {
        std::atomic<sal_Int32> nTotalErrors{0};
        std::atomic<sal_Int32> nRetriedErrors{0};
        std::atomic<sal_Int32> nRecoveredErrors{0};
        std::atomic<sal_Int32> nFailedErrors{0};
        std::atomic<sal_Int32> nCircuitBreakerTrips{0};
        std::chrono::steady_clock::time_point aLastReset;
        
        ErrorStatistics() : aLastReset(std::chrono::steady_clock::now()) {}
    };
    
    ErrorStatistics m_aStatistics;
    
    // Error history for analysis
    std::queue<ErrorContext> m_aErrorHistory;

public:
    explicit ErrorRecoveryManager(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    ~ErrorRecoveryManager();

    // Lifecycle management
    
    /**
     * Initialize error recovery manager with configuration
     * 
     * @param rConfig Configuration parameters
     * @returns true if initialization successful
     */
    bool initialize(const css::uno::Sequence<css::beans::PropertyValue>& rConfig = {});
    
    /**
     * Shutdown error recovery manager
     */
    void shutdown();

    // Error handling and recovery
    
    /**
     * Report error and determine recovery strategy
     * 
     * @param eType Error type classification
     * @param rsErrorMessage Error description
     * @param rsRequestId Request identifier for correlation
     * @param rsServiceName Service that generated the error
     * @param nErrorCode Numeric error code (HTTP status, etc.)
     * @param aContext Additional error context
     * @returns Recovery strategy to apply
     */
    RecoveryStrategy reportError(ErrorType eType, const OUString& rsErrorMessage,
                               const OUString& rsRequestId, const OUString& rsServiceName,
                               sal_Int32 nErrorCode = 0,
                               const std::map<OUString, OUString>& aContext = {});
    
    /**
     * Determine if retry should be attempted for given error
     * 
     * @param rsRequestId Request identifier
     * @param rsServiceName Service name
     * @returns true if retry should be attempted
     */
    bool shouldRetry(const OUString& rsRequestId, const OUString& rsServiceName);
    
    /**
     * Calculate retry delay based on policy and attempt count
     * 
     * @param rsRequestId Request identifier
     * @param rsServiceName Service name
     * @returns Delay in milliseconds before retry
     */
    sal_Int32 calculateRetryDelay(const OUString& rsRequestId, const OUString& rsServiceName);
    
    /**
     * Report successful operation (for circuit breaker recovery)
     * 
     * @param rsRequestId Request identifier
     * @param rsServiceName Service name
     */
    void reportSuccess(const OUString& rsRequestId, const OUString& rsServiceName);
    
    /**
     * Report retry attempt
     * 
     * @param rsRequestId Request identifier
     * @param nAttemptNumber Current attempt number
     * @returns true if retry should continue
     */
    bool reportRetryAttempt(const OUString& rsRequestId, sal_Int32 nAttemptNumber);
    
    /**
     * Clear error state for completed request
     * 
     * @param rsRequestId Request identifier
     */
    void clearError(const OUString& rsRequestId);

    // Circuit breaker management
    
    /**
     * Check if circuit breaker allows request
     * 
     * @param rsServiceName Service name
     * @returns true if request should be allowed
     */
    bool isCircuitBreakerClosed(const OUString& rsServiceName);
    
    /**
     * Get current circuit breaker state
     * 
     * @param rsServiceName Service name
     * @returns Current circuit state
     */
    CircuitState getCircuitState(const OUString& rsServiceName);
    
    /**
     * Manually trip circuit breaker (for testing or manual intervention)
     * 
     * @param rsServiceName Service name
     */
    void tripCircuitBreaker(const OUString& rsServiceName);
    
    /**
     * Reset circuit breaker to closed state
     * 
     * @param rsServiceName Service name
     */
    void resetCircuitBreaker(const OUString& rsServiceName);

    // Configuration management
    
    /**
     * Set retry policy for specific service
     * 
     * @param rsServiceName Service name
     * @param rPolicy Retry policy configuration
     */
    void setRetryPolicy(const OUString& rsServiceName, const RetryPolicy& rPolicy);
    
    /**
     * Get retry policy for service
     * 
     * @param rsServiceName Service name
     * @returns Current retry policy
     */
    RetryPolicy getRetryPolicy(const OUString& rsServiceName) const;
    
    /**
     * Set default retry policy for all services
     * 
     * @param rPolicy Default retry policy
     */
    void setDefaultRetryPolicy(const RetryPolicy& rPolicy);

    // Callback registration
    
    /**
     * Set error callback for notifications
     * 
     * @param aCallback Function to call on errors
     */
    void setErrorCallback(const ErrorCallback& aCallback);
    
    /**
     * Set recovery callback for custom recovery logic
     * 
     * @param aCallback Function to call for recovery attempts
     */
    void setRecoveryCallback(const RecoveryCallback& aCallback);
    
    /**
     * Set retry callback for custom retry logic
     * 
     * @param aCallback Function to call before retries
     */
    void setRetryCallback(const RetryCallback& aCallback);

    // Monitoring and statistics
    
    /**
     * Get error statistics
     * 
     * @returns Current error statistics
     */
    ErrorStatistics getStatistics() const;
    
    /**
     * Reset error statistics
     */
    void resetStatistics();
    
    /**
     * Get active errors
     * 
     * @returns Map of active error contexts
     */
    std::map<OUString, ErrorContext> getActiveErrors() const;
    
    /**
     * Get error history
     * 
     * @returns Recent error history
     */
    std::vector<ErrorContext> getErrorHistory() const;
    
    /**
     * Enable or disable error recovery
     * 
     * @param bEnabled true to enable error recovery
     */
    void setEnabled(bool bEnabled);
    
    /**
     * Enable or disable debug logging
     * 
     * @param bEnabled true to enable logging
     */
    void setLoggingEnabled(bool bEnabled);

    // Convenience methods for common error types
    
    /**
     * Report network timeout error
     */
    RecoveryStrategy reportNetworkTimeout(const OUString& rsRequestId, const OUString& rsServiceName,
                                        const OUString& rsErrorMessage = OUString());
    
    /**
     * Report HTTP error
     */
    RecoveryStrategy reportHttpError(sal_Int32 nStatusCode, const OUString& rsRequestId,
                                   const OUString& rsServiceName, const OUString& rsErrorMessage = OUString());
    
    /**
     * Report WebSocket error
     */
    RecoveryStrategy reportWebSocketError(sal_Int32 nCloseCode, const OUString& rsRequestId,
                                        const OUString& rsServiceName, const OUString& rsErrorMessage = OUString());
    
    /**
     * Report authentication error
     */
    RecoveryStrategy reportAuthenticationError(const OUString& rsRequestId, const OUString& rsServiceName,
                                             const OUString& rsErrorMessage = OUString());

private:
    // Internal implementation methods
    
    /**
     * Classify error type based on error code and context
     */
    ErrorType classifyError(sal_Int32 nErrorCode, const OUString& rsServiceName,
                          const OUString& rsErrorMessage) const;
    
    /**
     * Determine recovery strategy based on error type and policy
     */
    RecoveryStrategy determineRecoveryStrategy(const ErrorContext& rError) const;
    
    /**
     * Update circuit breaker state based on error/success
     */
    void updateCircuitBreaker(const OUString& rsServiceName, bool bSuccess);
    
    /**
     * Calculate exponential backoff delay
     */
    sal_Int32 calculateExponentialBackoff(sal_Int32 nAttempt, const RetryPolicy& rPolicy) const;
    
    /**
     * Calculate linear backoff delay
     */
    sal_Int32 calculateLinearBackoff(sal_Int32 nAttempt, const RetryPolicy& rPolicy) const;
    
    /**
     * Add jitter to delay to prevent thundering herd
     */
    sal_Int32 addJitter(sal_Int32 nDelay, sal_Int32 nJitterMs) const;
    
    /**
     * Log error event
     */
    void logError(const ErrorContext& rError) const;
    
    /**
     * Add error to history
     */
    void addToErrorHistory(const ErrorContext& rError);
    
    /**
     * Parse configuration parameters
     */
    void parseConfiguration(const css::uno::Sequence<css::beans::PropertyValue>& rConfig);
    
    /**
     * Create default retry policies for common services
     */
    void createDefaultRetryPolicies();
    
    /**
     * Validate retry policy parameters
     */
    bool validateRetryPolicy(const RetryPolicy& rPolicy) const;
    
    /**
     * Create error context from parameters
     */
    ErrorContext createErrorContext(ErrorType eType, const OUString& rsErrorMessage,
                                  const OUString& rsRequestId, const OUString& rsServiceName,
                                  sal_Int32 nErrorCode, const std::map<OUString, OUString>& aContext) const;
};

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */