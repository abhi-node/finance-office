/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ErrorRecoveryManager.hxx"

#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/random.hxx>
#include <tools/datetime.hxx>
#include <rtl/ustrbuf.hxx>

using namespace css;
using namespace css::uno;
using namespace css::beans;

namespace sw::ai {

ErrorRecoveryManager::ErrorRecoveryManager(const Reference<XComponentContext>& xContext)
    : m_xContext(xContext)
    , m_bEnabled(true)
    , m_bEnableLogging(true)
    , m_nMaxConcurrentRetries(10)
    , m_nMaxErrorHistorySize(100)
{
    SAL_INFO("sw.ai", "ErrorRecoveryManager created");
}

ErrorRecoveryManager::~ErrorRecoveryManager()
{
    shutdown();
    SAL_INFO("sw.ai", "ErrorRecoveryManager destroyed");
}

bool ErrorRecoveryManager::initialize(const Sequence<PropertyValue>& rConfig)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    try
    {
        // Parse configuration parameters
        parseConfiguration(rConfig);
        
        // Create default retry policies for common services
        createDefaultRetryPolicies();
        
        // Initialize statistics
        m_aStatistics.reset();
        
        // Set up default callbacks if none provided
        if (!m_aErrorCallback)
        {
            m_aErrorCallback = [this](const ErrorContext& rError) {
                logError(rError);
            };
        }
        
        SAL_INFO("sw.ai", "ErrorRecoveryManager initialized successfully");
        return true;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "ErrorRecoveryManager initialization failed: " << e.Message);
        return false;
    }
}

void ErrorRecoveryManager::shutdown()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Clear all active errors
    m_aActiveErrors.clear();
    m_aRetryPolicies.clear();
    m_aCircuitBreakers.clear();
    
    // Clear error history
    std::queue<ErrorContext> empty;
    m_aErrorHistory.swap(empty);
    
    SAL_INFO("sw.ai", "ErrorRecoveryManager shut down");
}

ErrorRecoveryManager::RecoveryStrategy ErrorRecoveryManager::reportError(
    ErrorType eType, const OUString& rsErrorMessage, const OUString& rsRequestId,
    const OUString& rsServiceName, sal_Int32 nErrorCode,
    const std::map<OUString, OUString>& aContext)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bEnabled)
    {
        return RecoveryStrategy::NO_RETRY;
    }
    
    ++m_aStatistics.nTotalErrors;
    
    // Create or update error context
    ErrorContext rError = createErrorContext(eType, rsErrorMessage, rsRequestId, 
                                            rsServiceName, nErrorCode, aContext);
    
    auto it = m_aActiveErrors.find(rsRequestId);
    if (it != m_aActiveErrors.end())
    {
        // Update existing error context
        rError.nAttemptCount = it->second.nAttemptCount + 1;
        rError.aFirstAttempt = it->second.aFirstAttempt;
    }
    else
    {
        rError.nAttemptCount = 1;
    }
    
    m_aActiveErrors[rsRequestId] = rError;
    
    // Add to error history
    addToErrorHistory(rError);
    
    // Update circuit breaker
    updateCircuitBreaker(rsServiceName, false);
    
    // Determine recovery strategy
    RecoveryStrategy eStrategy = determineRecoveryStrategy(rError);
    
    // Call error callback
    if (m_aErrorCallback)
    {
        m_aErrorCallback(rError);
    }
    
    logError(rError);
    
    SAL_INFO("sw.ai", "Error reported for request " << rsRequestId << 
             " (service: " << rsServiceName << ", strategy: " << static_cast<int>(eStrategy) << ")");
    
    return eStrategy;
}

bool ErrorRecoveryManager::shouldRetry(const OUString& rsRequestId, const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bEnabled)
        return false;
    
    // Check circuit breaker
    if (!isCircuitBreakerClosed(rsServiceName))
    {
        SAL_INFO("sw.ai", "Circuit breaker open for service " << rsServiceName << ", blocking retry");
        return false;
    }
    
    // Check if error exists and retry is appropriate
    auto it = m_aActiveErrors.find(rsRequestId);
    if (it == m_aActiveErrors.end())
        return false; // No error context found
    
    const ErrorContext& rError = it->second;
    const RetryPolicy& rPolicy = getRetryPolicy(rsServiceName);
    
    // Check max retries
    if (rError.nAttemptCount >= rPolicy.nMaxRetries)
    {
        SAL_INFO("sw.ai", "Max retries exceeded for request " << rsRequestId << 
                 " (" << rError.nAttemptCount << "/" << rPolicy.nMaxRetries << ")");
        return false;
    }
    
    // Check if error type allows retry
    switch (rError.eType)
    {
        case ErrorType::BAD_REQUEST:
        case ErrorType::AUTHENTICATION_FAILED:
        case ErrorType::PROTOCOL_ERROR:
            // Client errors - don't retry
            return false;
            
        case ErrorType::NETWORK_TIMEOUT:
        case ErrorType::CONNECTION_REFUSED:
        case ErrorType::SERVICE_UNAVAILABLE:
        case ErrorType::SERVER_ERROR:
        case ErrorType::WEBSOCKET_CLOSED:
        case ErrorType::RESOURCE_EXHAUSTED:
            // Retryable errors
            return true;
            
        case ErrorType::RATE_LIMIT_EXCEEDED:
            // Retry with longer delay
            return true;
            
        case ErrorType::UNKNOWN_ERROR:
        default:
            // Conservative approach - allow retry
            return true;
    }
}

sal_Int32 ErrorRecoveryManager::calculateRetryDelay(const OUString& rsRequestId, const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aActiveErrors.find(rsRequestId);
    if (it == m_aActiveErrors.end())
        return 1000; // Default 1 second delay
    
    const ErrorContext& rError = it->second;
    const RetryPolicy& rPolicy = getRetryPolicy(rsServiceName);
    
    sal_Int32 nDelay = 0;
    
    switch (rPolicy.eStrategy)
    {
        case RecoveryStrategy::IMMEDIATE_RETRY:
            nDelay = 0;
            break;
            
        case RecoveryStrategy::EXPONENTIAL_BACKOFF:
            nDelay = calculateExponentialBackoff(rError.nAttemptCount, rPolicy);
            break;
            
        case RecoveryStrategy::LINEAR_BACKOFF:
            nDelay = calculateLinearBackoff(rError.nAttemptCount, rPolicy);
            break;
            
        default:
            nDelay = rPolicy.nInitialDelayMs;
            break;
    }
    
    // Add jitter to prevent thundering herd
    nDelay = addJitter(nDelay, rPolicy.nJitterMs);
    
    // Apply special delays for specific error types
    switch (rError.eType)
    {
        case ErrorType::RATE_LIMIT_EXCEEDED:
            // Exponential backoff for rate limiting
            nDelay = std::max(nDelay, 5000 * rError.nAttemptCount);
            break;
            
        case ErrorType::SERVER_ERROR:
            // Longer delay for server errors
            nDelay = std::max(nDelay, 3000);
            break;
            
        case ErrorType::AUTHENTICATION_FAILED:
            // Very long delay for auth failures
            nDelay = std::max(nDelay, 10000);
            break;
            
        default:
            break;
    }
    
    // Cap at maximum delay
    nDelay = std::min(nDelay, rPolicy.nMaxDelayMs);
    
    SAL_INFO("sw.ai", "Calculated retry delay for request " << rsRequestId << 
             ": " << nDelay << "ms (attempt " << rError.nAttemptCount << ")");
    
    return nDelay;
}

void ErrorRecoveryManager::reportSuccess(const OUString& rsRequestId, const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Update circuit breaker with success
    updateCircuitBreaker(rsServiceName, true);
    
    // Check if this was a recovered error
    auto it = m_aActiveErrors.find(rsRequestId);
    if (it != m_aActiveErrors.end())
    {
        if (it->second.nAttemptCount > 1)
        {
            ++m_aStatistics.nRecoveredErrors;
            SAL_INFO("sw.ai", "Request " << rsRequestId << " recovered after " << 
                     it->second.nAttemptCount << " attempts");
        }
        
        // Remove from active errors
        m_aActiveErrors.erase(it);
    }
}

bool ErrorRecoveryManager::reportRetryAttempt(const OUString& rsRequestId, sal_Int32 nAttemptNumber)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    ++m_aStatistics.nRetriedErrors;
    
    auto it = m_aActiveErrors.find(rsRequestId);
    if (it != m_aActiveErrors.end())
    {
        it->second.nAttemptCount = nAttemptNumber;
        it->second.aLastAttempt = std::chrono::steady_clock::now();
        
        // Call retry callback if provided
        if (m_aRetryCallback)
        {
            return m_aRetryCallback(it->second, nAttemptNumber);
        }
    }
    
    SAL_INFO("sw.ai", "Retry attempt " << nAttemptNumber << " for request " << rsRequestId);
    return true;
}

void ErrorRecoveryManager::clearError(const OUString& rsRequestId)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aActiveErrors.find(rsRequestId);
    if (it != m_aActiveErrors.end())
    {
        if (it->second.nAttemptCount > 1)
        {
            ++m_aStatistics.nFailedErrors;
        }
        
        m_aActiveErrors.erase(it);
    }
}

bool ErrorRecoveryManager::isCircuitBreakerClosed(const OUString& rsServiceName)
{
    auto it = m_aCircuitBreakers.find(rsServiceName);
    if (it == m_aCircuitBreakers.end())
    {
        // No circuit breaker state - assume closed
        return true;
    }
    
    const CircuitBreakerState& rState = it->second;
    const RetryPolicy& rPolicy = getRetryPolicy(rsServiceName);
    
    switch (rState.eState)
    {
        case CircuitState::CLOSED:
            return true;
            
        case CircuitState::OPEN:
        {
            // Check if timeout period has elapsed
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - rState.aLastStateChange).count();
            
            if (elapsed >= rPolicy.nCircuitBreakerTimeoutMs)
            {
                // Move to half-open state
                const_cast<CircuitBreakerState&>(rState).eState = CircuitState::HALF_OPEN;
                const_cast<CircuitBreakerState&>(rState).aLastStateChange = now;
                SAL_INFO("sw.ai", "Circuit breaker for " << rsServiceName << " moved to HALF_OPEN");
                return true;
            }
            return false;
        }
        
        case CircuitState::HALF_OPEN:
            // Allow limited requests in half-open state
            return true;
            
        default:
            return false;
    }
}

ErrorRecoveryManager::CircuitState ErrorRecoveryManager::getCircuitState(const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aCircuitBreakers.find(rsServiceName);
    if (it == m_aCircuitBreakers.end())
    {
        return CircuitState::CLOSED;
    }
    
    return it->second.eState;
}

void ErrorRecoveryManager::tripCircuitBreaker(const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto& rState = m_aCircuitBreakers[rsServiceName];
    rState.eState = CircuitState::OPEN;
    rState.aLastStateChange = std::chrono::steady_clock::now();
    
    ++m_aStatistics.nCircuitBreakerTrips;
    
    SAL_INFO("sw.ai", "Circuit breaker manually tripped for service: " << rsServiceName);
}

void ErrorRecoveryManager::resetCircuitBreaker(const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto& rState = m_aCircuitBreakers[rsServiceName];
    rState.eState = CircuitState::CLOSED;
    rState.nFailureCount = 0;
    rState.nSuccessCount = 0;
    rState.aLastStateChange = std::chrono::steady_clock::now();
    
    SAL_INFO("sw.ai", "Circuit breaker reset for service: " << rsServiceName);
}

void ErrorRecoveryManager::setRetryPolicy(const OUString& rsServiceName, const RetryPolicy& rPolicy)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (validateRetryPolicy(rPolicy))
    {
        m_aRetryPolicies[rsServiceName] = rPolicy;
        SAL_INFO("sw.ai", "Retry policy set for service: " << rsServiceName);
    }
    else
    {
        SAL_WARN("sw.ai", "Invalid retry policy for service: " << rsServiceName);
    }
}

ErrorRecoveryManager::RetryPolicy ErrorRecoveryManager::getRetryPolicy(const OUString& rsServiceName) const
{
    auto it = m_aRetryPolicies.find(rsServiceName);
    if (it != m_aRetryPolicies.end())
    {
        return it->second;
    }
    
    // Return default policy if not found
    RetryPolicy aDefaultPolicy;
    return aDefaultPolicy;
}

void ErrorRecoveryManager::setDefaultRetryPolicy(const RetryPolicy& rPolicy)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (validateRetryPolicy(rPolicy))
    {
        // Apply to all services that don't have specific policies
        for (const OUString& rsService : {OUString("http"), OUString("websocket"), OUString("langgraph")})
        {
            if (m_aRetryPolicies.find(rsService) == m_aRetryPolicies.end())
            {
                m_aRetryPolicies[rsService] = rPolicy;
            }
        }
    }
}

void ErrorRecoveryManager::setErrorCallback(const ErrorCallback& aCallback)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aErrorCallback = aCallback;
}

void ErrorRecoveryManager::setRecoveryCallback(const RecoveryCallback& aCallback)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aRecoveryCallback = aCallback;
}

void ErrorRecoveryManager::setRetryCallback(const RetryCallback& aCallback)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aRetryCallback = aCallback;
}

ErrorRecoveryManager::ErrorStatisticsData ErrorRecoveryManager::getStatistics() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_aStatistics.getData();
}

void ErrorRecoveryManager::resetStatistics()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aStatistics.reset();
}

std::map<OUString, ErrorRecoveryManager::ErrorContext> ErrorRecoveryManager::getActiveErrors() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_aActiveErrors;
}

std::vector<ErrorRecoveryManager::ErrorContext> ErrorRecoveryManager::getErrorHistory() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    std::vector<ErrorContext> aHistory;
    
    // Convert queue to vector (most recent first)
    std::queue<ErrorContext> tempQueue = m_aErrorHistory;
    while (!tempQueue.empty())
    {
        aHistory.insert(aHistory.begin(), tempQueue.front());
        tempQueue.pop();
    }
    
    return aHistory;
}

void ErrorRecoveryManager::setEnabled(bool bEnabled)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_bEnabled = bEnabled;
    
    SAL_INFO("sw.ai", "ErrorRecoveryManager " << (bEnabled ? "enabled" : "disabled"));
}

void ErrorRecoveryManager::setLoggingEnabled(bool bEnabled)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_bEnableLogging = bEnabled;
}

// Convenience methods for common error types

ErrorRecoveryManager::RecoveryStrategy ErrorRecoveryManager::reportNetworkTimeout(
    const OUString& rsRequestId, const OUString& rsServiceName, const OUString& rsErrorMessage)
{
    OUString sMessage = rsErrorMessage.isEmpty() ? "Network request timeout" : rsErrorMessage;
    return reportError(ErrorType::NETWORK_TIMEOUT, sMessage, rsRequestId, rsServiceName, 408);
}

ErrorRecoveryManager::RecoveryStrategy ErrorRecoveryManager::reportHttpError(
    sal_Int32 nStatusCode, const OUString& rsRequestId, const OUString& rsServiceName, const OUString& rsErrorMessage)
{
    ErrorType eType = classifyError(nStatusCode, rsServiceName, rsErrorMessage);
    OUString sMessage = rsErrorMessage.isEmpty() ? 
                       ("HTTP error " + OUString::number(nStatusCode)) : rsErrorMessage;
                       
    return reportError(eType, sMessage, rsRequestId, rsServiceName, nStatusCode);
}

ErrorRecoveryManager::RecoveryStrategy ErrorRecoveryManager::reportWebSocketError(
    sal_Int32 nCloseCode, const OUString& rsRequestId, const OUString& rsServiceName, const OUString& rsErrorMessage)
{
    ErrorType eType = ErrorType::WEBSOCKET_CLOSED;
    if (nCloseCode >= 1002 && nCloseCode <= 1003)
        eType = ErrorType::PROTOCOL_ERROR;
    else if (nCloseCode == 1008)
        eType = ErrorType::AUTHENTICATION_FAILED;
    
    OUString sMessage = rsErrorMessage.isEmpty() ? 
                       ("WebSocket closed with code " + OUString::number(nCloseCode)) : rsErrorMessage;
                       
    return reportError(eType, sMessage, rsRequestId, rsServiceName, nCloseCode);
}

ErrorRecoveryManager::RecoveryStrategy ErrorRecoveryManager::reportAuthenticationError(
    const OUString& rsRequestId, const OUString& rsServiceName, const OUString& rsErrorMessage)
{
    OUString sMessage = rsErrorMessage.isEmpty() ? "Authentication failed" : rsErrorMessage;
    return reportError(ErrorType::AUTHENTICATION_FAILED, sMessage, rsRequestId, rsServiceName, 401);
}

// Private implementation methods

ErrorRecoveryManager::ErrorType ErrorRecoveryManager::classifyError(
    sal_Int32 nErrorCode, const OUString& rsServiceName, const OUString& rsErrorMessage) const
{
    // HTTP status code classification
    if (rsServiceName == "http" || rsServiceName == "langgraph")
    {
        if (nErrorCode == 408)
            return ErrorType::NETWORK_TIMEOUT;
        else if (nErrorCode == 401 || nErrorCode == 403)
            return ErrorType::AUTHENTICATION_FAILED;
        else if (nErrorCode == 429)
            return ErrorType::RATE_LIMIT_EXCEEDED;
        else if (nErrorCode >= 400 && nErrorCode < 500)
            return ErrorType::BAD_REQUEST;
        else if (nErrorCode >= 500 && nErrorCode < 600)
            return ErrorType::SERVER_ERROR;
        else if (nErrorCode == 503)
            return ErrorType::SERVICE_UNAVAILABLE;
    }
    
    // WebSocket close code classification
    if (rsServiceName == "websocket")
    {
        if (nErrorCode == 1001 || nErrorCode == 1006)
            return ErrorType::WEBSOCKET_CLOSED;
        else if (nErrorCode >= 1002 && nErrorCode <= 1003)
            return ErrorType::PROTOCOL_ERROR;
        else if (nErrorCode == 1008)
            return ErrorType::AUTHENTICATION_FAILED;
    }
    
    // Message-based classification
    if (rsErrorMessage.indexOf("timeout") >= 0 || rsErrorMessage.indexOf("timed out") >= 0)
        return ErrorType::NETWORK_TIMEOUT;
    else if (rsErrorMessage.indexOf("connection refused") >= 0)
        return ErrorType::CONNECTION_REFUSED;
    else if (rsErrorMessage.indexOf("authentication") >= 0 || rsErrorMessage.indexOf("unauthorized") >= 0)
        return ErrorType::AUTHENTICATION_FAILED;
    
    return ErrorType::UNKNOWN_ERROR;
}

ErrorRecoveryManager::RecoveryStrategy ErrorRecoveryManager::determineRecoveryStrategy(const ErrorContext& rError) const
{
    const RetryPolicy& rPolicy = getRetryPolicy(rError.sServiceName);
    
    // Check circuit breaker first
    if (rPolicy.bEnableCircuitBreaker)
    {
        auto it = m_aCircuitBreakers.find(rError.sServiceName);
        if (it != m_aCircuitBreakers.end() && it->second.eState == CircuitState::OPEN)
        {
            return RecoveryStrategy::CIRCUIT_BREAKER;
        }
    }
    
    // Error type specific strategies
    switch (rError.eType)
    {
        case ErrorType::BAD_REQUEST:
        case ErrorType::PROTOCOL_ERROR:
            return RecoveryStrategy::NO_RETRY;
            
        case ErrorType::AUTHENTICATION_FAILED:
            return RecoveryStrategy::USER_INTERVENTION;
            
        case ErrorType::RATE_LIMIT_EXCEEDED:
            return RecoveryStrategy::EXPONENTIAL_BACKOFF;
            
        case ErrorType::SERVICE_UNAVAILABLE:
        case ErrorType::SERVER_ERROR:
            return RecoveryStrategy::GRACEFUL_DEGRADATION;
            
        case ErrorType::NETWORK_TIMEOUT:
        case ErrorType::CONNECTION_REFUSED:
        case ErrorType::WEBSOCKET_CLOSED:
            return rPolicy.eStrategy;
            
        case ErrorType::RESOURCE_EXHAUSTED:
            return RecoveryStrategy::LINEAR_BACKOFF;
            
        case ErrorType::UNKNOWN_ERROR:
        default:
            return rPolicy.eStrategy;
    }
}

void ErrorRecoveryManager::updateCircuitBreaker(const OUString& rsServiceName, bool bSuccess)
{
    const RetryPolicy& rPolicy = getRetryPolicy(rsServiceName);
    if (!rPolicy.bEnableCircuitBreaker)
        return;
    
    auto& rState = m_aCircuitBreakers[rsServiceName];
    
    if (bSuccess)
    {
        ++rState.nSuccessCount;
        
        if (rState.eState == CircuitState::HALF_OPEN)
        {
            // Recovery successful - close circuit
            if (rState.nSuccessCount >= 3) // Require multiple successes
            {
                rState.eState = CircuitState::CLOSED;
                rState.nFailureCount = 0;
                rState.aLastStateChange = std::chrono::steady_clock::now();
                SAL_INFO("sw.ai", "Circuit breaker for " << rsServiceName << " closed after recovery");
            }
        }
        else if (rState.eState == CircuitState::CLOSED)
        {
            // Reset failure count on success
            rState.nFailureCount = std::max(0, rState.nFailureCount - 1);
        }
    }
    else
    {
        ++rState.nFailureCount;
        rState.aLastFailure = std::chrono::steady_clock::now();
        
        if (rState.eState == CircuitState::HALF_OPEN)
        {
            // Failed in half-open state - reopen circuit
            rState.eState = CircuitState::OPEN;
            rState.aLastStateChange = std::chrono::steady_clock::now();
            ++m_aStatistics.nCircuitBreakerTrips;
            SAL_INFO("sw.ai", "Circuit breaker for " << rsServiceName << " reopened");
        }
        else if (rState.eState == CircuitState::CLOSED && 
                 rState.nFailureCount >= rPolicy.nCircuitBreakerThreshold)
        {
            // Threshold exceeded - open circuit
            rState.eState = CircuitState::OPEN;
            rState.aLastStateChange = std::chrono::steady_clock::now();
            ++m_aStatistics.nCircuitBreakerTrips;
            SAL_INFO("sw.ai", "Circuit breaker for " << rsServiceName << " opened (failures: " << 
                     rState.nFailureCount << ")");
        }
    }
}

sal_Int32 ErrorRecoveryManager::calculateExponentialBackoff(sal_Int32 nAttempt, const RetryPolicy& rPolicy) const
{
    // Calculate: initial_delay * (multiplier^(attempt-1))
    double fDelay = rPolicy.nInitialDelayMs * std::pow(rPolicy.fBackoffMultiplier, nAttempt - 1);
    return static_cast<sal_Int32>(std::min(fDelay, static_cast<double>(rPolicy.nMaxDelayMs)));
}

sal_Int32 ErrorRecoveryManager::calculateLinearBackoff(sal_Int32 nAttempt, const RetryPolicy& rPolicy) const
{
    // Calculate: initial_delay * attempt
    sal_Int32 nDelay = rPolicy.nInitialDelayMs * nAttempt;
    return std::min(nDelay, rPolicy.nMaxDelayMs);
}

sal_Int32 ErrorRecoveryManager::addJitter(sal_Int32 nDelay, sal_Int32 nJitterMs) const
{
    if (nJitterMs <= 0)
        return nDelay;
    
    // Add random jitter: ±jitterMs
    sal_Int32 nRandomJitter = comphelper::rng::uniform_int_distribution(-nJitterMs, nJitterMs);
    return std::max(0, nDelay + nRandomJitter);
}

void ErrorRecoveryManager::logError(const ErrorContext& rError) const
{
    if (!m_bEnableLogging)
        return;
    
    SAL_WARN("sw.ai", "Error reported - Type: " << static_cast<int>(rError.eType) << 
             ", Service: " << rError.sServiceName << 
             ", Request: " << rError.sRequestId << 
             ", Attempt: " << rError.nAttemptCount << 
             ", Message: " << rError.sErrorMessage);
}

void ErrorRecoveryManager::addToErrorHistory(const ErrorContext& rError)
{
    m_aErrorHistory.push(rError);
    
    // Maintain maximum history size
    while (static_cast<sal_Int32>(m_aErrorHistory.size()) > m_nMaxErrorHistorySize)
    {
        m_aErrorHistory.pop();
    }
}

void ErrorRecoveryManager::parseConfiguration(const Sequence<PropertyValue>& rConfig)
{
    for (const auto& rProperty : rConfig)
    {
        if (rProperty.Name == "Enabled")
        {
            rProperty.Value >>= m_bEnabled;
        }
        else if (rProperty.Name == "EnableLogging")
        {
            rProperty.Value >>= m_bEnableLogging;
        }
        else if (rProperty.Name == "MaxConcurrentRetries")
        {
            rProperty.Value >>= m_nMaxConcurrentRetries;
        }
        else if (rProperty.Name == "MaxErrorHistorySize")
        {
            rProperty.Value >>= m_nMaxErrorHistorySize;
        }
        else
        {
            SAL_WARN("sw.ai", "Unknown ErrorRecoveryManager configuration property: " << rProperty.Name);
        }
    }
}

void ErrorRecoveryManager::createDefaultRetryPolicies()
{
    // HTTP service policy
    RetryPolicy aHttpPolicy;
    aHttpPolicy.eStrategy = RecoveryStrategy::EXPONENTIAL_BACKOFF;
    aHttpPolicy.nMaxRetries = 3;
    aHttpPolicy.nInitialDelayMs = 1000;
    aHttpPolicy.nMaxDelayMs = 30000;
    aHttpPolicy.fBackoffMultiplier = 2.0;
    aHttpPolicy.nJitterMs = 500;
    aHttpPolicy.bEnableCircuitBreaker = true;
    aHttpPolicy.nCircuitBreakerThreshold = 5;
    aHttpPolicy.nCircuitBreakerTimeoutMs = 60000;
    m_aRetryPolicies["http"] = aHttpPolicy;
    
    // WebSocket service policy
    RetryPolicy aWebSocketPolicy;
    aWebSocketPolicy.eStrategy = RecoveryStrategy::EXPONENTIAL_BACKOFF;
    aWebSocketPolicy.nMaxRetries = 5;
    aWebSocketPolicy.nInitialDelayMs = 2000;
    aWebSocketPolicy.nMaxDelayMs = 60000;
    aWebSocketPolicy.fBackoffMultiplier = 1.5;
    aWebSocketPolicy.nJitterMs = 1000;
    aWebSocketPolicy.bEnableCircuitBreaker = true;
    aWebSocketPolicy.nCircuitBreakerThreshold = 3;
    aWebSocketPolicy.nCircuitBreakerTimeoutMs = 30000;
    m_aRetryPolicies["websocket"] = aWebSocketPolicy;
    
    // LangGraph service policy
    RetryPolicy aLangGraphPolicy;
    aLangGraphPolicy.eStrategy = RecoveryStrategy::EXPONENTIAL_BACKOFF;
    aLangGraphPolicy.nMaxRetries = 2;
    aLangGraphPolicy.nInitialDelayMs = 2000;
    aLangGraphPolicy.nMaxDelayMs = 20000;
    aLangGraphPolicy.fBackoffMultiplier = 3.0;
    aLangGraphPolicy.nJitterMs = 800;
    aLangGraphPolicy.bEnableCircuitBreaker = true;
    aLangGraphPolicy.nCircuitBreakerThreshold = 3;
    aLangGraphPolicy.nCircuitBreakerTimeoutMs = 120000; // 2 minutes
    m_aRetryPolicies["langgraph"] = aLangGraphPolicy;
}

bool ErrorRecoveryManager::validateRetryPolicy(const RetryPolicy& rPolicy) const
{
    if (rPolicy.nMaxRetries < 0 || rPolicy.nMaxRetries > 10)
        return false;
    
    if (rPolicy.nInitialDelayMs < 0 || rPolicy.nInitialDelayMs > 60000)
        return false;
    
    if (rPolicy.nMaxDelayMs < rPolicy.nInitialDelayMs)
        return false;
    
    if (rPolicy.fBackoffMultiplier <= 1.0 || rPolicy.fBackoffMultiplier > 10.0)
        return false;
    
    if (rPolicy.nCircuitBreakerThreshold < 1 || rPolicy.nCircuitBreakerThreshold > 20)
        return false;
    
    return true;
}

ErrorRecoveryManager::ErrorContext ErrorRecoveryManager::createErrorContext(
    ErrorType eType, const OUString& rsErrorMessage, const OUString& rsRequestId,
    const OUString& rsServiceName, sal_Int32 nErrorCode, 
    const std::map<OUString, OUString>& aContext) const
{
    ErrorContext rError(eType, rsErrorMessage, rsServiceName);
    rError.sRequestId = rsRequestId;
    rError.nErrorCode = nErrorCode;
    rError.aContext = aContext;
    
    return rError;
}

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */