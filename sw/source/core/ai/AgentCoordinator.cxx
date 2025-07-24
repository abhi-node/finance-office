/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "AgentCoordinator.hxx"
#include "NetworkClient.hxx"
#include "WebSocketClient.hxx"
#include "ErrorRecoveryManager.hxx"

#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/random.hxx>
#include <tools/datetime.hxx>
#include <rtl/ustrbuf.hxx>

using namespace css;
using namespace css::uno;
using namespace css::lang;
using namespace css::beans;
using namespace css::frame;

namespace sw::core::ai {

// Service registration macros
constexpr OUStringLiteral IMPLEMENTATION_NAME = u"com.sun.star.comp.Writer.AIAgentCoordinator";
constexpr OUStringLiteral SERVICE_NAME = u"com.sun.star.ai.AIAgentCoordinator";

AgentCoordinator::AgentCoordinator(const Reference<XComponentContext>& xContext)
    : m_xContext(xContext)
    , m_bInitialized(false)
    , m_bOnlineMode(true)
    , m_aLastActivity(std::chrono::steady_clock::now())
    , m_nRequestCounter(0)
    , m_nMaxRetries(3)
    , m_nTimeoutMs(30000)
    , m_nMaxQueueSize(100)
    , m_bEnableWebSocket(false)
    , m_bEnableOfflineMode(true)
{
    SAL_INFO("sw.ai", "AgentCoordinator created");
    
    // Initialize default configuration
    loadConfiguration();
    
    // Initialize network client in background
    initializeNetworkClient();
    
    // Initialize WebSocket client if enabled
    if (m_bEnableWebSocket)
    {
        initializeWebSocketClient();
    }
    
    // Initialize error recovery manager
    initializeErrorRecovery();
    
    // Initialize message queue system
    initializeMessageQueue();
    
    // Initialize authentication manager
    initializeAuthenticationManager();
}

AgentCoordinator::~AgentCoordinator()
{
    shutdown();
    SAL_INFO("sw.ai", "AgentCoordinator destroyed");
}

// XAIAgentCoordinator interface implementation

OUString SAL_CALL AgentCoordinator::processUserRequest(
    const OUString& rsRequest,
    const Any& rDocumentContext)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (rsRequest.isEmpty())
    {
        throw IllegalArgumentException("Request cannot be empty", 
                                       static_cast<cppu::OWeakObject*>(this), 0);
    }
    
    try
    {
        // Update activity tracking
        m_aLastActivity = std::chrono::steady_clock::now();
        ++m_nRequestCounter;
        
        // Generate unique request ID
        OUString sRequestId = generateRequestId();
        
        SAL_INFO("sw.ai", "Processing request " << sRequestId << ": " << rsRequest);
        
        // Analyze request complexity for intelligent routing
        RequestComplexity eComplexity = analyzeRequestComplexity(rsRequest);
        
        // Extract and prepare document context
        Any aProcessedContext = extractDocumentContext(rDocumentContext);
        
        // Route to appropriate processing workflow
        OUString sResponse;
        switch (eComplexity)
        {
            case RequestComplexity::Simple:
                sResponse = processSimpleRequest(rsRequest, aProcessedContext);
                break;
            case RequestComplexity::Moderate:
                sResponse = processModerateRequest(rsRequest, aProcessedContext);
                break;
            case RequestComplexity::Complex:
                sResponse = processComplexRequest(rsRequest, aProcessedContext);
                break;
        }
        
        logActivity("Request " + sRequestId + " completed successfully");
        return sResponse;
    }
    catch (const Exception& e)
    {
        OUString sError = "Failed to process request: " + e.Message;
        SAL_WARN("sw.ai", sError);
        handleProcessingError(generateRequestId(), sError);
        throw RuntimeException(sError, static_cast<cppu::OWeakObject*>(this));
    }
}

void SAL_CALL AgentCoordinator::cancelOperation(sal_Int64 nOperationId)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    SAL_INFO("sw.ai", "Cancelling operation " << nOperationId);
    
    // TODO: Implement operation cancellation logic
    // This will be implemented in Task 6.4 (error handling and retry mechanisms)
    
    throw IllegalArgumentException("Operation cancellation not yet implemented",
                                   static_cast<cppu::OWeakObject*>(this), 0);
}

Sequence<OUString> SAL_CALL AgentCoordinator::getAvailableAgents()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Return list of available agents based on current implementation status
    // This will be expanded as agents are implemented
    Sequence<OUString> aAgents(7);
    aAgents[0] = "DocumentMaster";
    aAgents[1] = "ContextAnalysis"; 
    aAgents[2] = "ContentGeneration";
    aAgents[3] = "Formatting";
    aAgents[4] = "DataIntegration";
    aAgents[5] = "Validation";
    aAgents[6] = "Execution";
    
    return aAgents;
}

sal_Bool SAL_CALL AgentCoordinator::isOnline()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_bOnlineMode;
}

void SAL_CALL AgentCoordinator::setConfiguration(
    const Sequence<PropertyValue>& rConfig)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    for (const auto& rProperty : rConfig)
    {
        if (rProperty.Name == "MaxRetries")
        {
            rProperty.Value >>= m_nMaxRetries;
        }
        else if (rProperty.Name == "TimeoutMs")
        {
            rProperty.Value >>= m_nTimeoutMs;
        }
        else if (rProperty.Name == "MaxQueueSize")
        {
            rProperty.Value >>= m_nMaxQueueSize;
        }
        else if (rProperty.Name == "EnableWebSocket")
        {
            rProperty.Value >>= m_bEnableWebSocket;
        }
        else if (rProperty.Name == "EnableOfflineMode")
        {
            rProperty.Value >>= m_bEnableOfflineMode;
        }
        else
        {
            SAL_WARN("sw.ai", "Unknown configuration property: " << rProperty.Name);
        }
    }
    
    // Save configuration changes
    saveConfiguration();
}

Sequence<PropertyValue> SAL_CALL AgentCoordinator::getConfiguration()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    Sequence<PropertyValue> aConfig(5);
    
    aConfig[0] = makeConfigProperty("MaxRetries", Any(m_nMaxRetries));
    aConfig[1] = makeConfigProperty("TimeoutMs", Any(m_nTimeoutMs));
    aConfig[2] = makeConfigProperty("MaxQueueSize", Any(m_nMaxQueueSize));
    aConfig[3] = makeConfigProperty("EnableWebSocket", Any(m_bEnableWebSocket));
    aConfig[4] = makeConfigProperty("EnableOfflineMode", Any(m_bEnableOfflineMode));
    
    return aConfig;
}

// XServiceInfo interface implementation

OUString SAL_CALL AgentCoordinator::getImplementationName()
{
    return IMPLEMENTATION_NAME;
}

sal_Bool SAL_CALL AgentCoordinator::supportsService(const OUString& rServiceName)
{
    return rServiceName == SERVICE_NAME;
}

Sequence<OUString> SAL_CALL AgentCoordinator::getSupportedServiceNames()
{
    Sequence<OUString> aServices(1);
    aServices[0] = SERVICE_NAME;
    return aServices;
}

// Lifecycle management

void SAL_CALL AgentCoordinator::initialize(const Reference<XFrame>& xFrame)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (m_bInitialized)
    {
        SAL_WARN("sw.ai", "AgentCoordinator already initialized");
        return;
    }
    
    m_xFrame = xFrame;
    m_bInitialized = true;
    
    SAL_INFO("sw.ai", "AgentCoordinator initialized successfully");
}

void SAL_CALL AgentCoordinator::shutdown()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bInitialized)
        return;
    
    // Clean up resources
    m_xFrame.clear();
    m_bInitialized = false;
    
    SAL_INFO("sw.ai", "AgentCoordinator shut down");
}

// Private implementation methods

AgentCoordinator::RequestComplexity AgentCoordinator::analyzeRequestComplexity(
    const OUString& rsRequest) const
{
    // Simple pattern matching for complexity analysis
    // This will be enhanced with proper NLP analysis in future phases
    
    OUString sLowerRequest = rsRequest.toAsciiLowerCase();
    
    // Simple operations - direct formatting or basic commands
    if (sLowerRequest.indexOf("bold") >= 0 ||
        sLowerRequest.indexOf("font") >= 0 ||
        sLowerRequest.indexOf("create chart") >= 0 ||
        sLowerRequest.indexOf("insert table") >= 0)
    {
        return RequestComplexity::Simple;
    }
    
    // Complex operations - require external data or multi-step processing
    if (sLowerRequest.indexOf("financial report") >= 0 ||
        sLowerRequest.indexOf("market data") >= 0 ||
        sLowerRequest.indexOf("research") >= 0 ||
        sLowerRequest.indexOf("analysis") >= 0)
    {
        return RequestComplexity::Complex;
    }
    
    // Default to moderate complexity
    return RequestComplexity::Moderate;
}

OUString AgentCoordinator::processSimpleRequest(
    const OUString& rsRequest, const Any& rContext)
{
    // Simple request processing - fast response for basic operations
    SAL_INFO("sw.ai", "Processing simple request: " << rsRequest);
    
    try
    {
        // For simple operations, try local processing first for speed
        if (rsRequest.indexOf("bold") >= 0 || rsRequest.indexOf("font") >= 0)
        {
            // Local formatting operations don't need backend
            return "Applied formatting: " + rsRequest;
        }
        
        // For operations that need AI processing, use backend if available
        if (m_bOnlineMode && m_pNetworkClient)
        {
            // Prepare simplified request for backend
            OUString sJsonBody = R"({"request": ")" + rsRequest + 
                               R"(", "type": "simple", "complexity": "low"})";
            
            std::map<OUString, OUString> aHeaders;
            aHeaders["X-Request-Type"] = "simple";
            
            OUString sBackendUrl = "http://localhost:8000/api/simple";
            sw::ai::NetworkClient::HttpResponse aResponse = 
                m_pNetworkClient->postJson(sBackendUrl, sJsonBody, aHeaders);
            
            if (aResponse.bSuccess)
            {
                // Parse response and return processed result
                return "AI processed (simple): " + rsRequest;
            }
        }
        
        // Fallback to local processing
        return "Offline processed (simple): " + rsRequest;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Exception in simple request processing: " << e.Message);
        return "Error processing request: " + rsRequest;
    }
}

OUString AgentCoordinator::processModerateRequest(
    const OUString& rsRequest, const Any& rContext)
{
    // Moderate request processing - balanced workflow
    SAL_INFO("sw.ai", "Processing moderate request: " << rsRequest);
    
    try
    {
        // Moderate operations typically require backend processing
        if (m_bOnlineMode && m_pNetworkClient)
        {
            // Prepare request with document context
            OUString sJsonBody = R"({"request": ")" + rsRequest + 
                               R"(", "type": "moderate", "complexity": "medium", )" +
                               R"("context": {"document": "current_doc"}})";
            
            std::map<OUString, OUString> aHeaders;
            aHeaders["X-Request-Type"] = "moderate";
            aHeaders["X-Include-Context"] = "true";
            
            OUString sBackendUrl = "http://localhost:8000/api/moderate";
            sw::ai::NetworkClient::HttpResponse aResponse = 
                m_pNetworkClient->postJson(sBackendUrl, sJsonBody, aHeaders);
            
            if (aResponse.bSuccess)
            {
                // Parse response and return processed result
                return "AI processed (moderate): " + rsRequest;
            }
            else
            {
                SAL_WARN("sw.ai", "Backend request failed, falling back to offline processing");
            }
        }
        
        // Fallback to simplified local processing
        return "Offline processed (moderate): " + rsRequest;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Exception in moderate request processing: " << e.Message);
        return "Error processing request: " + rsRequest;
    }
}

OUString AgentCoordinator::processComplexRequest(
    const OUString& rsRequest, const Any& rContext)
{
    // Complex request processing - full agent workflow
    SAL_INFO("sw.ai", "Processing complex request: " << rsRequest);
    
    try
    {
        // Complex operations require full backend processing
        if (m_bOnlineMode && m_pNetworkClient)
        {
            // Generate unique request ID for correlation
            OUString sRequestId = generateRequestId();
            
            // Try WebSocket first for real-time updates during complex processing
            if (isWebSocketEnabled())
            {
                SAL_INFO("sw.ai", "Using WebSocket for complex request: " << sRequestId);
                
                // Send initial request via WebSocket for streaming updates
                bool bWebSocketSent = sendWebSocketMessage(rsRequest, sRequestId);
                if (bWebSocketSent)
                {
                    // WebSocket request sent - will receive streaming updates
                    return "Processing complex request via WebSocket (ID: " + sRequestId + ") - streaming updates enabled";
                }
                else
                {
                    SAL_WARN("sw.ai", "WebSocket send failed, falling back to HTTP");
                }
            }
            
            // Fallback to HTTP for complex requests
            SAL_INFO("sw.ai", "Using HTTP for complex request: " << sRequestId);
            
            // Prepare comprehensive request with full context
            OUString sJsonBody = R"({"request": ")" + rsRequest + 
                               R"(", "type": "complex", "complexity": "high", "request_id": ")" + sRequestId +
                               R"(", "context": {"document": "full_context", )" +
                               R"("agents": ["DocumentMaster", "ContextAnalysis", "ContentGeneration", )" +
                               R"("Formatting", "DataIntegration", "Validation", "Execution"]})";
            
            std::map<OUString, OUString> aHeaders;
            aHeaders["X-Request-Type"] = "complex";
            aHeaders["X-Include-Context"] = "full";
            aHeaders["X-Agent-Workflow"] = "complete";
            aHeaders["X-Request-ID"] = sRequestId;
            
            OUString sBackendUrl = "http://localhost:8000/api/complex";
            sw::ai::NetworkClient::HttpResponse aResponse = 
                m_pNetworkClient->postJson(sBackendUrl, sJsonBody, aHeaders);
            
            if (aResponse.bSuccess)
            {
                // Parse response and return processed result
                return "AI processed (complex): " + rsRequest + " - Full agent workflow completed";
            }
            else
            {
                SAL_WARN("sw.ai", "Complex operation requires backend, operation failed");
                return "Error: Complex operation requires AI backend connection";
            }
        }
        else
        {
            // Complex operations cannot be processed offline
            SAL_WARN("sw.ai", "Complex operation attempted in offline mode");
            return "Error: Complex operations require online connection to AI backend";
        }
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Exception in complex request processing: " << e.Message);
        return "Error processing complex request: " + rsRequest;
    }
}

Any AgentCoordinator::extractDocumentContext(const Any& rContext) const
{
    // Extract relevant document context for agent processing
    // TODO: Implement proper context extraction
    return rContext;
}

OUString AgentCoordinator::getCurrentDocumentInfo() const
{
    // Extract current document information
    // TODO: Implement document info extraction
    return "Document info not yet implemented";
}

bool AgentCoordinator::initializeNetworkClient()
{
    try
    {
        // Create NetworkClient instance
        m_pNetworkClient = std::make_unique<sw::ai::NetworkClient>(m_xContext);
        
        // Configure network client
        Sequence<PropertyValue> aConfig(3);
        aConfig[0].Name = "DefaultTimeout";
        aConfig[0].Value <<= m_nTimeoutMs;
        aConfig[1].Name = "UserAgent";
        aConfig[1].Value <<= OUString("LibreOffice-Writer-AI/1.0");
        aConfig[2].Name = "MaxConnections";
        aConfig[2].Value <<= sal_Int32(5);
        
        bool bSuccess = m_pNetworkClient->initialize(aConfig);
        if (bSuccess)
        {
            SAL_INFO("sw.ai", "NetworkClient initialized successfully");
            
            // Test connectivity if online mode is enabled
            if (m_bOnlineMode)
            {
                bool bOnline = m_pNetworkClient->isOnline();
                if (!bOnline)
                {
                    SAL_WARN("sw.ai", "Network connectivity test failed, entering offline mode");
                    enterOfflineMode();
                }
            }
        }
        else
        {
            SAL_WARN("sw.ai", "NetworkClient initialization failed");
            m_pNetworkClient.reset();
        }
        
        return bSuccess;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "NetworkClient initialization exception: " << e.Message);
        m_pNetworkClient.reset();
        return false;
    }
}

bool AgentCoordinator::sendToBackend(const OUString& rsMessage, const Any& rContext)
{
    if (!m_pNetworkClient)
    {
        SAL_WARN("sw.ai", "NetworkClient not initialized");
        return false;
    }
    
    try
    {
        // Prepare JSON request body for LangGraph backend
        OUString sJsonBody = R"({"message": ")" + rsMessage + R"(", "context": {}})";
        
        // Configure request headers
        std::map<OUString, OUString> aHeaders;
        aHeaders["Authorization"] = "Bearer your-api-key-here"; // TODO: Implement in Task 6.5
        aHeaders["Content-Type"] = "application/json";
        
        // Send request to LangGraph backend
        // TODO: Make URL configurable
        OUString sBackendUrl = "http://localhost:8000/api/process";
        
        sw::ai::NetworkClient::HttpResponse aResponse = 
            m_pNetworkClient->postJson(sBackendUrl, sJsonBody, aHeaders);
        
        if (aResponse.bSuccess)
        {
            SAL_INFO("sw.ai", "Message sent to backend successfully: " << aResponse.nStatusCode);
            return true;
        }
        else
        {
            SAL_WARN("sw.ai", "Backend request failed: " << aResponse.sErrorMessage);
            
            // Handle offline mode
            if (aResponse.nStatusCode == 503 || aResponse.nStatusCode == 0)
            {
                SAL_INFO("sw.ai", "Backend unavailable, entering offline mode");
                enterOfflineMode();
            }
            
            return false;
        }
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Exception sending to backend: " << e.Message);
        return false;
    }
}

OUString AgentCoordinator::receiveFromBackend(const OUString& rsRequestId)
{
    if (!m_pNetworkClient)
    {
        SAL_WARN("sw.ai", "NetworkClient not initialized");
        return "";
    }
    
    try
    {
        // For synchronous requests, the response is already available from sendToBackend
        // For asynchronous requests, this would poll or wait for the response
        
        // TODO: Implement proper request/response tracking in Task 6.4
        // For now, return a simulated response
        return "Response received from LangGraph backend for request: " + rsRequestId;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Exception receiving from backend: " << e.Message);
        return "";
    }
}

void AgentCoordinator::handleNetworkError(const OUString& rsError)
{
    SAL_WARN("sw.ai", "Network error: " << rsError);
    
    // Report general network error
    OUString sRequestId = generateRequestId();
    reportOperationError(sRequestId, "network", rsError, 0);
    
    // Consider entering offline mode if multiple network errors
    if (m_pErrorRecovery)
    {
        auto aStatistics = m_pErrorRecovery->getStatistics();
        if (aStatistics.nTotalErrors.load() > 5)
        {
            SAL_INFO("sw.ai", "Multiple network errors detected, considering offline mode");
            if (m_bOnlineMode)
            {
                enterOfflineMode();
            }
        }
    }
}

void AgentCoordinator::handleProcessingError(const OUString& rsRequestId, const OUString& rsError)
{
    SAL_WARN("sw.ai", "Processing error for request " << rsRequestId << ": " << rsError);
    
    // Report processing error to error recovery manager
    reportOperationError(rsRequestId, "processing", rsError, 500);
    
    // Check if we should attempt recovery
    if (shouldRetryOperation(rsRequestId, "processing"))
    {
        SAL_INFO("sw.ai", "Processing error recovery recommended for request " << rsRequestId);
        
        // Add to retry queue or handle immediately based on error type
        // For now, log the recovery attempt
        SAL_INFO("sw.ai", "Processing error recovery will be attempted after delay");
    }
    else
    {
        SAL_INFO("sw.ai", "No recovery recommended for processing error " << rsRequestId);
    }
}

bool AgentCoordinator::attemptErrorRecovery(const PendingRequest& rRequest)
{
    SAL_INFO("sw.ai", "Attempting error recovery for request: " << rRequest.sRequestId);
    
    // Check if error recovery manager allows retry
    if (!shouldRetryOperation(rRequest.sRequestId, "general"))
    {
        SAL_INFO("sw.ai", "Error recovery not recommended for request: " << rRequest.sRequestId);
        return false;
    }
    
    // Calculate retry delay
    sal_Int32 nDelayMs = calculateRetryDelay(rRequest.sRequestId, "general");
    
    // For now, simulate recovery attempt
    // In a full implementation, this would:
    // 1. Wait for the calculated delay
    // 2. Retry the original operation
    // 3. Report success/failure to ErrorRecoveryManager
    
    SAL_INFO("sw.ai", "Error recovery scheduled for request " << rRequest.sRequestId << 
             " with delay: " << nDelayMs << "ms");
    
    // Report retry attempt
    if (m_pErrorRecovery)
    {
        m_pErrorRecovery->reportRetryAttempt(rRequest.sRequestId, rRequest.nRetryCount + 1);
    }
    
    // Simulated recovery success for development
    return true;
}

void AgentCoordinator::enterOfflineMode()
{
    m_bOnlineMode = false;
    SAL_INFO("sw.ai", "Entered offline mode");
}

void AgentCoordinator::exitOfflineMode()
{
    m_bOnlineMode = true;
    SAL_INFO("sw.ai", "Exited offline mode");
}

void AgentCoordinator::processOfflineQueue()
{
    // Process queued messages when coming back online
    // TODO: Implement in Task 6.5 (message queuing and authentication protocols)
}

bool AgentCoordinator::isCacheableRequest(const OUString& rsRequest) const
{
    // Determine if request can be cached
    // TODO: Implement caching logic
    return false;
}

OUString AgentCoordinator::getCachedResponse(const OUString& rsRequest) const
{
    // Get cached response
    // TODO: Implement caching logic
    return "";
}

void AgentCoordinator::cacheResponse(const OUString& rsRequest, const OUString& rsResponse)
{
    // Cache response for future use
    // TODO: Implement caching logic
}

void AgentCoordinator::loadConfiguration()
{
    // Load configuration from LibreOffice settings
    // TODO: Implement configuration persistence
    SAL_INFO("sw.ai", "Configuration loading not yet implemented");
}

void AgentCoordinator::saveConfiguration()
{
    // Save configuration to LibreOffice settings
    // TODO: Implement configuration persistence
    SAL_INFO("sw.ai", "Configuration saving not yet implemented");
}

PropertyValue AgentCoordinator::makeConfigProperty(
    const OUString& rsName, const Any& rValue) const
{
    PropertyValue aProp;
    aProp.Name = rsName;
    aProp.Value = rValue;
    return aProp;
}

OUString AgentCoordinator::generateRequestId() const
{
    // Generate unique request ID
    tools::Time aTime(tools::Time::SYSTEM);
    sal_uInt32 nRandom = comphelper::rng::uniform_uint_distribution(0, 0xFFFFFFFF);
    
    return "REQ_" + OUString::number(aTime.GetTime()) + "_" + OUString::number(nRandom);
}

bool AgentCoordinator::isRequestTimedOut(const PendingRequest& rRequest) const
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - rRequest.aTimestamp).count();
    
    return duration > m_nTimeoutMs;
}

void AgentCoordinator::logActivity(const OUString& rsMessage) const
{
    SAL_INFO("sw.ai", rsMessage);
}

// WebSocket communication management methods

bool AgentCoordinator::initializeWebSocketClient()
{
    try
    {
        // Create WebSocketClient instance
        m_pWebSocketClient = std::make_unique<sw::ai::WebSocketClient>(m_xContext);
        
        // Configure WebSocket client
        Sequence<PropertyValue> aConfig(5);
        aConfig[0].Name = "AutoReconnect";
        aConfig[0].Value <<= true;
        aConfig[1].Name = "MaxReconnectAttempts";
        aConfig[1].Value <<= sal_Int32(3);
        aConfig[2].Name = "ReconnectDelayMs";
        aConfig[2].Value <<= sal_Int32(2000);
        aConfig[3].Name = "HeartbeatIntervalMs";
        aConfig[3].Value <<= sal_Int32(30000);
        aConfig[4].Name = "EnableLogging";
        aConfig[4].Value <<= true;
        
        bool bSuccess = m_pWebSocketClient->initialize(aConfig);
        if (bSuccess)
        {
            SAL_INFO("sw.ai", "WebSocketClient initialized successfully");
            
            // Set up callbacks for real-time communication
            m_pWebSocketClient->setMessageCallback(
                [this](const sw::ai::WebSocketClient::WebSocketMessage& aMessage) {
                    handleWebSocketMessage(aMessage.sContent);
                });
            
            m_pWebSocketClient->setConnectionCallback(
                [this](sw::ai::WebSocketClient::ConnectionState eState, const OUString& /*rsMessage*/) {
                    bool bConnected = (eState == sw::ai::WebSocketClient::ConnectionState::CONNECTED);
                    handleWebSocketConnectionChange(bConnected);
                });
            
            m_pWebSocketClient->setErrorCallback(
                [this](const OUString& rsError, sal_Int32 /*nCode*/) {
                    SAL_WARN("sw.ai", "WebSocket error: " << rsError);
                    handleNetworkError(rsError);
                });
        }
        else
        {
            SAL_WARN("sw.ai", "WebSocketClient initialization failed");
            m_pWebSocketClient.reset();
        }
        
        return bSuccess;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "WebSocketClient initialization exception: " << e.Message);
        m_pWebSocketClient.reset();
        return false;
    }
}

bool AgentCoordinator::connectWebSocket(const OUString& rsUrl)
{
    if (!m_pWebSocketClient)
    {
        SAL_WARN("sw.ai", "WebSocketClient not initialized");
        return false;
    }
    
    try
    {
        SAL_INFO("sw.ai", "Connecting WebSocket to: " << rsUrl);
        return m_pWebSocketClient->connect(rsUrl, "langgraph-ai");
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "WebSocket connection exception: " << e.Message);
        return false;
    }
}

void AgentCoordinator::disconnectWebSocket()
{
    if (m_pWebSocketClient)
    {
        SAL_INFO("sw.ai", "Disconnecting WebSocket");
        m_pWebSocketClient->disconnect();
    }
}

bool AgentCoordinator::sendWebSocketMessage(const OUString& rsMessage, const OUString& rsRequestId)
{
    if (!m_pWebSocketClient || !m_pWebSocketClient->isConnected())
    {
        SAL_WARN("sw.ai", "WebSocket not connected, cannot send message");
        return false;
    }
    
    try
    {
        // Create JSON message with request ID for correlation
        OUString sJsonMessage = R"({"request_id": ")" + rsRequestId + 
                               R"(", "message": ")" + rsMessage + 
                               R"(", "timestamp": ")" + 
                               OUString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch()).count()) + R"("})";
        
        std::map<OUString, OUString> aHeaders;
        aHeaders["X-Request-ID"] = rsRequestId;
        aHeaders["X-Agent-Type"] = "libreoffice-writer";
        
        return m_pWebSocketClient->sendJsonMessage(sJsonMessage, aHeaders);
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "WebSocket message send exception: " << e.Message);
        return false;
    }
}

void AgentCoordinator::handleWebSocketMessage(const OUString& rsMessage)
{
    try
    {
        SAL_INFO("sw.ai", "WebSocket message received: " << rsMessage.copy(0, std::min(rsMessage.getLength(), 100)) << "...");
        
        // TODO: Parse JSON message and handle different message types:
        // - progress_update: Update UI with agent processing progress
        // - agent_status: Update agent availability status
        // - streaming_response: Stream partial responses to UI
        // - error_notification: Handle backend errors
        
        // For development, log the message
        logActivity("WebSocket message processed: " + rsMessage.copy(0, 50) + "...");
        
        // In a full implementation, this would:
        // 1. Parse JSON to determine message type
        // 2. Extract request ID for correlation
        // 3. Update corresponding UI elements
        // 4. Handle streaming responses for real-time updates
        // 5. Manage agent status and availability
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "WebSocket message handling exception: " << e.Message);
        handleNetworkError("WebSocket message processing failed: " + e.Message);
    }
}

void AgentCoordinator::handleWebSocketConnectionChange(bool bConnected)
{
    SAL_INFO("sw.ai", "WebSocket connection state changed: " << (bConnected ? "CONNECTED" : "DISCONNECTED"));
    
    if (bConnected)
    {
        // WebSocket connected - can now send real-time requests
        logActivity("WebSocket connected - real-time communication enabled");
        
        // If we were in offline mode due to WebSocket issues, try to go online
        if (!m_bOnlineMode && m_pNetworkClient && m_pNetworkClient->isOnline())
        {
            exitOfflineMode();
        }
    }
    else
    {
        // WebSocket disconnected - fall back to HTTP-only mode
        logActivity("WebSocket disconnected - falling back to HTTP communication");
        
        // Don't enter offline mode unless HTTP is also failing
        // WebSocket is for real-time updates, HTTP still works for basic requests
    }
}

bool AgentCoordinator::isWebSocketEnabled() const
{
    return m_bEnableWebSocket && m_pWebSocketClient && m_pWebSocketClient->isConnected();
}

// Error handling and recovery management methods

bool AgentCoordinator::initializeErrorRecovery()
{
    try
    {
        // Create ErrorRecoveryManager instance
        m_pErrorRecovery = std::make_unique<sw::ai::ErrorRecoveryManager>(m_xContext);
        
        // Configure error recovery manager
        Sequence<PropertyValue> aConfig(4);
        aConfig[0].Name = "Enabled";
        aConfig[0].Value <<= true;
        aConfig[1].Name = "EnableLogging";
        aConfig[1].Value <<= true;
        aConfig[2].Name = "MaxConcurrentRetries";
        aConfig[2].Value <<= sal_Int32(5);
        aConfig[3].Name = "MaxErrorHistorySize";
        aConfig[3].Value <<= sal_Int32(50);
        
        bool bSuccess = m_pErrorRecovery->initialize(aConfig);
        if (bSuccess)
        {
            SAL_INFO("sw.ai", "ErrorRecoveryManager initialized successfully");
            
            // Set up callbacks for integrated error handling
            m_pErrorRecovery->setErrorCallback(
                [this](const sw::ai::ErrorRecoveryManager::ErrorContext& rError) {
                    SAL_WARN("sw.ai", "Error reported by ErrorRecoveryManager - " <<
                             "Service: " << rError.sServiceName << ", " <<
                             "Request: " << rError.sRequestId << ", " <<
                             "Message: " << rError.sErrorMessage);
                });
            
            m_pErrorRecovery->setRecoveryCallback(
                [this](const sw::ai::ErrorRecoveryManager::ErrorContext& rError) -> bool {
                    // Custom recovery logic for specific services
                    if (rError.sServiceName == "websocket" && !isWebSocketEnabled())
                    {
                        // Try to reconnect WebSocket
                        if (m_pWebSocketClient)
                        {
                            return m_pWebSocketClient->connect("ws://localhost:8000/ws/libreoffice", "langgraph-ai");
                        }
                    }
                    return false; // Let default recovery handle it
                });
        }
        else
        {
            SAL_WARN("sw.ai", "ErrorRecoveryManager initialization failed");
            m_pErrorRecovery.reset();
        }
        
        return bSuccess;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "ErrorRecoveryManager initialization exception: " << e.Message);
        m_pErrorRecovery.reset();
        return false;
    }
}

bool AgentCoordinator::initializeMessageQueue()
{
    try
    {
        // Create MessageQueue instance
        m_pMessageQueue = std::make_unique<sw::ai::MessageQueue>(m_xContext);
        
        // Configure message queue
        Sequence<PropertyValue> aConfig(6);
        aConfig[0].Name = "MaxQueueSize";
        aConfig[0].Value <<= sal_Int32(1000);
        aConfig[1].Name = "MaxMessageSize";
        aConfig[1].Value <<= sal_Int32(1048576); // 1MB
        aConfig[2].Name = "DefaultTTLSeconds";
        aConfig[2].Value <<= sal_Int32(3600); // 1 hour
        aConfig[3].Name = "EnablePersistence";
        aConfig[3].Value <<= false; // Disable for now
        aConfig[4].Name = "MaxMessagesPerSecond";
        aConfig[4].Value <<= sal_Int32(50);
        aConfig[5].Name = "EnableCompression";
        aConfig[5].Value <<= false;
        
        bool bSuccess = m_pMessageQueue->initialize(aConfig);
        if (bSuccess)
        {
            bSuccess = m_pMessageQueue->start();
            SAL_INFO("sw.ai", "MessageQueue initialized and started successfully");
        }
        else
        {
            SAL_WARN("sw.ai", "MessageQueue initialization failed");
            m_pMessageQueue.reset();
        }
        
        return bSuccess;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "MessageQueue initialization exception: " << e.Message);
        m_pMessageQueue.reset();
        return false;
    }
}

bool AgentCoordinator::initializeAuthenticationManager()
{
    try
    {
        // Create AuthenticationManager instance
        m_pAuthManager = std::make_unique<sw::ai::AuthenticationManager>(m_xContext);
        
        // Configure authentication manager
        Sequence<PropertyValue> aConfig(5);
        aConfig[0].Name = "SecureStorageEnabled";
        aConfig[0].Value <<= true;
        aConfig[1].Name = "AutoRefreshEnabled";
        aConfig[1].Value <<= true;
        aConfig[2].Name = "DefaultRefreshThreshold";
        aConfig[2].Value <<= sal_Int32(300); // 5 minutes
        aConfig[3].Name = "MaxRetryAttempts";
        aConfig[3].Value <<= sal_Int32(3);
        aConfig[4].Name = "TokenValidationTimeout";
        aConfig[4].Value <<= sal_Int32(5000); // 5 seconds
        
        bool bSuccess = m_pAuthManager->initialize(aConfig);
        if (bSuccess)
        {
            SAL_INFO("sw.ai", "AuthenticationManager initialized successfully");
        }
        else
        {
            SAL_WARN("sw.ai", "AuthenticationManager initialization failed");
            m_pAuthManager.reset();
        }
        
        return bSuccess;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "AuthenticationManager initialization exception: " << e.Message);
        m_pAuthManager.reset();
        return false;
    }
}

void AgentCoordinator::reportOperationError(const OUString& rsRequestId, const OUString& rsServiceName, 
                                          const OUString& rsError, sal_Int32 nErrorCode)
{
    if (!m_pErrorRecovery)
        return;
    
    try
    {
        // Determine error type based on service and error code
        sw::ai::ErrorRecoveryManager::RecoveryStrategy eStrategy;
        
        if (rsServiceName == "http" || rsServiceName == "langgraph")
        {
            eStrategy = m_pErrorRecovery->reportHttpError(nErrorCode, rsRequestId, rsServiceName, rsError);
        }
        else if (rsServiceName == "websocket")
        {
            eStrategy = m_pErrorRecovery->reportWebSocketError(nErrorCode, rsRequestId, rsServiceName, rsError);
        }
        else
        {
            eStrategy = m_pErrorRecovery->reportError(sw::ai::ErrorRecoveryManager::ErrorType::UNKNOWN_ERROR,
                                                    rsError, rsRequestId, rsServiceName, nErrorCode);
        }
        
        // Handle recovery strategy
        switch (eStrategy)
        {
            case sw::ai::ErrorRecoveryManager::RecoveryStrategy::GRACEFUL_DEGRADATION:
                // Enter offline mode for service unavailability
                if (!m_bOnlineMode)
                {
                    SAL_INFO("sw.ai", "Already in offline mode due to previous errors");
                }
                else
                {
                    SAL_INFO("sw.ai", "Entering offline mode due to service errors");
                    enterOfflineMode();
                }
                break;
                
            case sw::ai::ErrorRecoveryManager::RecoveryStrategy::CIRCUIT_BREAKER:
                SAL_INFO("sw.ai", "Circuit breaker active for service: " << rsServiceName);
                break;
                
            case sw::ai::ErrorRecoveryManager::RecoveryStrategy::USER_INTERVENTION:
                SAL_WARN("sw.ai", "User intervention required for service: " << rsServiceName);
                // TODO: Notify UI to show user intervention dialog
                break;
                
            default:
                // Let automatic retry mechanisms handle it
                break;
        }
        
        SAL_INFO("sw.ai", "Error reported to ErrorRecoveryManager - Strategy: " << static_cast<int>(eStrategy));
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Error reporting operation error: " << e.Message);
    }
}

bool AgentCoordinator::shouldRetryOperation(const OUString& rsRequestId, const OUString& rsServiceName)
{
    if (!m_pErrorRecovery)
        return false; // No error recovery - don't retry
    
    try
    {
        bool bShouldRetry = m_pErrorRecovery->shouldRetry(rsRequestId, rsServiceName);
        
        if (bShouldRetry)
        {
            // Check if circuit breaker allows the retry
            if (!m_pErrorRecovery->isCircuitBreakerClosed(rsServiceName))
            {
                SAL_INFO("sw.ai", "Circuit breaker prevents retry for service: " << rsServiceName);
                return false;
            }
            
            SAL_INFO("sw.ai", "Retry approved for request " << rsRequestId << " (service: " << rsServiceName << ")");
        }
        else
        {
            SAL_INFO("sw.ai", "Retry not recommended for request " << rsRequestId);
        }
        
        return bShouldRetry;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Error checking retry eligibility: " << e.Message);
        return false;
    }
}

sal_Int32 AgentCoordinator::calculateRetryDelay(const OUString& rsRequestId, const OUString& rsServiceName)
{
    if (!m_pErrorRecovery)
        return 5000; // Default 5 second delay
    
    try
    {
        sal_Int32 nDelay = m_pErrorRecovery->calculateRetryDelay(rsRequestId, rsServiceName);
        
        SAL_INFO("sw.ai", "Calculated retry delay for request " << rsRequestId << 
                 ": " << nDelay << "ms");
        
        return nDelay;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Error calculating retry delay: " << e.Message);
        return 5000; // Default fallback
    }
}

void AgentCoordinator::reportOperationSuccess(const OUString& rsRequestId, const OUString& rsServiceName)
{
    if (!m_pErrorRecovery)
        return;
    
    try
    {
        m_pErrorRecovery->reportSuccess(rsRequestId, rsServiceName);
        
        SAL_INFO("sw.ai", "Success reported for request " << rsRequestId << 
                 " (service: " << rsServiceName << ")");
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Error reporting operation success: " << e.Message);
    }
}

// Static factory method for UNO service creation
Reference<XInterface> SAL_CALL AgentCoordinator::create(
    const Reference<XComponentContext>& xContext)
{
    return static_cast<cppu::OWeakObject*>(new AgentCoordinator(xContext));
}

} // namespace sw::core::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */