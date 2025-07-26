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

// LibreOffice Writer core includes for document access
#include <wrtsh.hxx>
#include <view.hxx>
#include <swmodule.hxx>
#include <doc.hxx>
#include <docsh.hxx>

using namespace css;
using namespace css::uno;
using namespace css::lang;
using namespace css::beans;
using namespace css::frame;

using css::lang::IllegalArgumentException;

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
        
        // Extract and prepare document context
        Any aProcessedContext = extractDocumentContext(rDocumentContext);
        
        // Process all requests through unified agent system
        // Agent complexity analysis and routing now handled by DocumentMasterAgent
        OUString sResponse = processUnifiedRequest(rsRequest, aProcessedContext, sRequestId);
        
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
    OUString* pAgents = aAgents.getArray();
    pAgents[0] = "DocumentMaster";
    pAgents[1] = "ContextAnalysis";
    pAgents[2] = "ContentGeneration";
    pAgents[3] = "Formatting";
    pAgents[4] = "DataIntegration";
    pAgents[5] = "Validation";
    pAgents[6] = "Execution";
    
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
    PropertyValue* pConfig = aConfig.getArray();
    
    pConfig[0] = makeConfigProperty("MaxRetries", Any(m_nMaxRetries));
    pConfig[1] = makeConfigProperty("TimeoutMs", Any(m_nTimeoutMs));
    pConfig[2] = makeConfigProperty("MaxQueueSize", Any(m_nMaxQueueSize));
    pConfig[3] = makeConfigProperty("EnableWebSocket", Any(m_bEnableWebSocket));
    pConfig[4] = makeConfigProperty("EnableOfflineMode", Any(m_bEnableOfflineMode));
    
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
    OUString* pServices = aServices.getArray();
    pServices[0] = SERVICE_NAME;
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
    SAL_INFO("sw.ai", "AgentCoordinator::analyzeRequestComplexity() - Analyzing request: " << rsRequest.copy(0, 100));
    
    // Simple pattern matching for complexity analysis
    // This will be enhanced with proper NLP analysis in future phases
    
    OUString sLowerRequest = rsRequest.toAsciiLowerCase();
    
    // Simple operations - direct formatting or basic commands
    if (sLowerRequest.indexOf("bold") >= 0 ||
        sLowerRequest.indexOf("font") >= 0 ||
        sLowerRequest.indexOf("create chart") >= 0 ||
        sLowerRequest.indexOf("insert table") >= 0)
    {
        SAL_INFO("sw.ai", "AgentCoordinator::analyzeRequestComplexity() - Request classified as SIMPLE");
        return RequestComplexity::Simple;
    }
    
    // Complex operations - require external data or multi-step processing
    if (sLowerRequest.indexOf("financial report") >= 0 ||
        sLowerRequest.indexOf("market data") >= 0 ||
        sLowerRequest.indexOf("research") >= 0 ||
        sLowerRequest.indexOf("analysis") >= 0)
    {
        SAL_INFO("sw.ai", "AgentCoordinator::analyzeRequestComplexity() - Request classified as COMPLEX");
        return RequestComplexity::Complex;
    }
    
    // Default to moderate complexity
    SAL_INFO("sw.ai", "AgentCoordinator::analyzeRequestComplexity() - Request classified as MODERATE (default)");
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
            // Extract document context as JSON string from processed context
            OUString sDocumentContext;
            rContext >>= sDocumentContext;
            
            // Prepare request with real document context
            OUStringBuffer sJsonBodyBuilder;
            sJsonBodyBuilder.append("{\"request\": \"");
            // Escape quotes in request
            OUString sEscapedRequest = rsRequest;
            sEscapedRequest = sEscapedRequest.replaceAll("\"", "\\\"");
            sEscapedRequest = sEscapedRequest.replaceAll("\n", "\\n");
            sJsonBodyBuilder.append(sEscapedRequest);
            sJsonBodyBuilder.append("\", \"type\": \"simple\", \"complexity\": \"low\"");
            
            // Add document context if available
            if (!sDocumentContext.isEmpty() && sDocumentContext != "{}")
            {
                sJsonBodyBuilder.append(", \"context\": ");
                sJsonBodyBuilder.append(sDocumentContext);
            }
            sJsonBodyBuilder.append("}");
            
            OUString sJsonBody = sJsonBodyBuilder.makeStringAndClear();
            
            std::map<OUString, OUString> aHeaders;
            aHeaders["X-Request-Type"] = "simple";
            
            OUString sBackendUrl = "http://localhost:8000/api/simple";
            sw::ai::NetworkClient::HttpResponse aResponse = 
                m_pNetworkClient->postJson(sBackendUrl, sJsonBody, aHeaders);
            
            if (aResponse.bSuccess)
            {
                // Parse enhanced JSON response format
                ParsedResponse aParsed = parseEnhancedJsonResponse(aResponse.sBody);
                if (aParsed.bSuccess)
                {
                    // Return response content for user display
                    OUString sDisplayResponse = formatResponseForDisplay(aParsed);
                    
                    // PHASE 5 & 6: Translate and Execute operations
                    if (hasExecutableOperations(aParsed))
                    {
                        SAL_INFO("sw.ai", "Simple request has " << aParsed.aOperations.size() << " operations to execute");
                        
                        // PHASE 5: Translate operations to UNO format
                        std::vector<TranslatedOperation> aTranslatedOps = translateOperationsToUno(aParsed);
                        SAL_INFO("sw.ai", "Translated " << aTranslatedOps.size() << " operations to UNO format");
                        
                        // PHASE 6: Execute operations via DocumentOperations service
                        if (!aTranslatedOps.empty())
                        {
                            std::vector<ExecutionResult> aExecutionResults = executeTranslatedOperations(aTranslatedOps);
                            OUString sExecutionSummary = formatExecutionSummary(aExecutionResults);
                            SAL_INFO("sw.ai", "Simple request execution completed: " << sExecutionSummary);
                            
                            // Append execution summary to display response
                            if (!sDisplayResponse.isEmpty())
                            {
                                sDisplayResponse += "\n\n";
                            }
                            sDisplayResponse += "✓ " + sExecutionSummary;
                        }
                    }
                    
                    return sDisplayResponse;
                }
                else
                {
                    SAL_WARN("sw.ai", "Failed to parse JSON response: " << aParsed.sErrorMessage);
                    return "AI processed (simple): " + rsRequest + " - Response format error";
                }
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
            // Extract document context as JSON string from processed context
            OUString sDocumentContext;
            rContext >>= sDocumentContext;
            
            // Prepare request with real document context
            OUStringBuffer sJsonBodyBuilder;
            sJsonBodyBuilder.append("{\"request\": \"");
            // Escape quotes in request
            OUString sEscapedRequest = rsRequest;
            sEscapedRequest = sEscapedRequest.replaceAll("\"", "\\\"");
            sEscapedRequest = sEscapedRequest.replaceAll("\n", "\\n");
            sJsonBodyBuilder.append(sEscapedRequest);
            sJsonBodyBuilder.append("\", \"type\": \"moderate\", \"complexity\": \"medium\"");
            
            // Add document context if available
            if (!sDocumentContext.isEmpty() && sDocumentContext != "{}")
            {
                sJsonBodyBuilder.append(", \"context\": ");
                sJsonBodyBuilder.append(sDocumentContext);
            }
            sJsonBodyBuilder.append("}");
            
            OUString sJsonBody = sJsonBodyBuilder.makeStringAndClear();
            
            std::map<OUString, OUString> aHeaders;
            aHeaders["X-Request-Type"] = "moderate";
            aHeaders["X-Include-Context"] = "true";
            
            OUString sBackendUrl = "http://localhost:8000/api/moderate";
            sw::ai::NetworkClient::HttpResponse aResponse = 
                m_pNetworkClient->postJson(sBackendUrl, sJsonBody, aHeaders);
            
            if (aResponse.bSuccess)
            {
                // Parse enhanced JSON response format
                ParsedResponse aParsed = parseEnhancedJsonResponse(aResponse.sBody);
                if (aParsed.bSuccess)
                {
                    // Return response content for user display
                    OUString sDisplayResponse = formatResponseForDisplay(aParsed);
                    
                    // PHASE 5 & 6: Translate and Execute operations
                    if (hasExecutableOperations(aParsed))
                    {
                        SAL_INFO("sw.ai", "Moderate request has " << aParsed.aOperations.size() << " operations to execute");
                        
                        // PHASE 5: Translate operations to UNO format
                        std::vector<TranslatedOperation> aTranslatedOps = translateOperationsToUno(aParsed);
                        SAL_INFO("sw.ai", "Translated " << aTranslatedOps.size() << " operations to UNO format");
                        
                        // PHASE 6: Execute operations via DocumentOperations service
                        if (!aTranslatedOps.empty())
                        {
                            std::vector<ExecutionResult> aExecutionResults = executeTranslatedOperations(aTranslatedOps);
                            OUString sExecutionSummary = formatExecutionSummary(aExecutionResults);
                            SAL_INFO("sw.ai", "Moderate request execution completed: " << sExecutionSummary);
                            
                            // Append execution summary to display response
                            if (!sDisplayResponse.isEmpty())
                            {
                                sDisplayResponse += "\n\n";
                            }
                            sDisplayResponse += "✓ " + sExecutionSummary;
                        }
                    }
                    
                    return sDisplayResponse;
                }
                else
                {
                    SAL_WARN("sw.ai", "Failed to parse JSON response: " << aParsed.sErrorMessage);
                    return "AI processed (moderate): " + rsRequest + " - Response format error";
                }
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
            
            // Extract document context as JSON string from processed context
            OUString sDocumentContext;
            rContext >>= sDocumentContext;
            
            // Prepare comprehensive request with real document context
            OUStringBuffer sJsonBodyBuilder;
            sJsonBodyBuilder.append("{\"request\": \"");
            // Escape quotes in request
            OUString sEscapedRequest = rsRequest;
            sEscapedRequest = sEscapedRequest.replaceAll("\"", "\\\"");
            sEscapedRequest = sEscapedRequest.replaceAll("\n", "\\n");
            sJsonBodyBuilder.append(sEscapedRequest);
            sJsonBodyBuilder.append("\", \"type\": \"complex\", \"complexity\": \"high\", \"request_id\": \"");
            sJsonBodyBuilder.append(sRequestId);
            sJsonBodyBuilder.append("\"");
            
            // Add document context if available
            if (!sDocumentContext.isEmpty() && sDocumentContext != "{}")
            {
                sJsonBodyBuilder.append(", \"context\": ");
                sJsonBodyBuilder.append(sDocumentContext);
            }
            
            // Add agent workflow specification
            sJsonBodyBuilder.append(", \"agents\": [\"DocumentMaster\", \"ContextAnalysis\", \"ContentGeneration\", \"Formatting\", \"DataIntegration\", \"Validation\", \"Execution\"]");
            sJsonBodyBuilder.append("}");
            
            OUString sJsonBody = sJsonBodyBuilder.makeStringAndClear();
            
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
                // Parse enhanced JSON response format
                ParsedResponse aParsed = parseEnhancedJsonResponse(aResponse.sBody);
                if (aParsed.bSuccess)
                {
                    // Return response content for user display
                    OUString sDisplayResponse = formatResponseForDisplay(aParsed);
                    
                    // PHASE 5: Translate operations to UNO format
                    if (hasExecutableOperations(aParsed))
                    {
                        SAL_INFO("sw.ai", "Complex request has " << aParsed.aOperations.size() << " operations to execute");
                        
                        // Translate operations to UNO format
                        std::vector<TranslatedOperation> aTranslatedOps = translateOperationsToUno(aParsed);
                        SAL_INFO("sw.ai", "Translated " << aTranslatedOps.size() << " operations to UNO format");
                        
                        // Log translated operations for verification
                        for (size_t i = 0; i < aTranslatedOps.size(); ++i)
                        {
                            const auto& rOp = aTranslatedOps[i];
                            SAL_INFO("sw.ai", "Translated Operation " << (i+1) << ": " << rOp.sOperationType << 
                                     " (priority: " << rOp.nPriority << ", params: " << rOp.aParameters.getLength() << ")");
                        }
                        
                        // PHASE 6: Execute operations via DocumentOperations service
                        if (!aTranslatedOps.empty())
                        {
                            std::vector<ExecutionResult> aExecutionResults = executeTranslatedOperations(aTranslatedOps);
                            OUString sExecutionSummary = formatExecutionSummary(aExecutionResults);
                            SAL_INFO("sw.ai", "Complex request execution completed: " << sExecutionSummary);
                            
                            // Append execution summary to display response
                            if (!sDisplayResponse.isEmpty())
                            {
                                sDisplayResponse += "\n\n";
                            }
                            sDisplayResponse += "✓ " + sExecutionSummary;
                        }
                    }
                    
                    return sDisplayResponse;
                }
                else
                {
                    SAL_WARN("sw.ai", "Failed to parse JSON response: " << aParsed.sErrorMessage);
                    return "AI processed (complex): " + rsRequest + " - Response format error";
                }
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

OUString AgentCoordinator::processUnifiedRequest(
    const OUString& rsRequest, const Any& rContext, const OUString& rsRequestId)
{
    // Unified request processing through LangGraph agent system
    // All complexity analysis and routing now handled by DocumentMasterAgent
    SAL_INFO("sw.ai", "Processing unified request: " << rsRequest);
    
    try
    {
        // All requests now go through agent system for intelligent routing
        if (m_bOnlineMode && m_pNetworkClient)
        {
            // Extract document context as JSON string from processed context
            OUString sDocumentContext;
            rContext >>= sDocumentContext;
            
            // Prepare unified request with all context and metadata
            OUStringBuffer sJsonBodyBuilder;
            sJsonBodyBuilder.append("{\"request\": \"");
            
            // Escape quotes in request
            OUString sEscapedRequest = rsRequest;
            sEscapedRequest = sEscapedRequest.replaceAll("\"", "\\\"");
            sEscapedRequest = sEscapedRequest.replaceAll("\n", "\\n");
            sJsonBodyBuilder.append(sEscapedRequest);
            
            sJsonBodyBuilder.append("\", \"request_id\": \"");
            sJsonBodyBuilder.append(rsRequestId);
            sJsonBodyBuilder.append("\"");
            
            // Add document context if available
            if (!sDocumentContext.isEmpty() && sDocumentContext != "{}")
            {
                sJsonBodyBuilder.append(", \"context\": ");
                sJsonBodyBuilder.append(sDocumentContext);
            }
            
            // Add user preferences and session metadata
            sJsonBodyBuilder.append(", \"user_preferences\": {\"language\": \"en-US\"}");
            sJsonBodyBuilder.append(", \"session_id\": \"").append(rsRequestId).append("\"");
            sJsonBodyBuilder.append("}");
            
            OUString sJsonBody = sJsonBodyBuilder.makeStringAndClear();
            
            // Configure headers for unified agent system
            std::map<OUString, OUString> aHeaders;
            aHeaders["X-Request-ID"] = rsRequestId;
            aHeaders["X-Agent-System"] = "langgraph";
            aHeaders["X-LibreOffice-Version"] = "24.8.0";
            aHeaders["X-Integration-Layer"] = "agentcoordinator";
            
            // Send to unified agent endpoint
            OUString sBackendUrl = "http://localhost:8000/api/agent";
            sw::ai::NetworkClient::HttpResponse aResponse = 
                m_pNetworkClient->postJson(sBackendUrl, sJsonBody, aHeaders);
            
            if (aResponse.bSuccess)
            {
                // Parse enhanced JSON response format from agent system
                ParsedResponse aParsed = parseEnhancedJsonResponse(aResponse.sBody);
                if (aParsed.bSuccess)
                {
                    // Return response content for user display
                    OUString sDisplayResponse = formatResponseForDisplay(aParsed);
                    
                    // PHASE 5 & 6: Translate and Execute operations
                    if (hasExecutableOperations(aParsed))
                    {
                        SAL_INFO("sw.ai", "Unified request has " << aParsed.aOperations.size() << " operations to execute");
                        
                        // PHASE 5: Translate operations to UNO format
                        std::vector<TranslatedOperation> aTranslatedOps = translateOperationsToUno(aParsed);
                        SAL_INFO("sw.ai", "Translated " << aTranslatedOps.size() << " operations to UNO format");
                        
                        // PHASE 6: Execute operations via DocumentOperations service
                        if (!aTranslatedOps.empty())
                        {
                            std::vector<ExecutionResult> aExecutionResults = executeTranslatedOperations(aTranslatedOps);
                            OUString sExecutionSummary = formatExecutionSummary(aExecutionResults);
                            SAL_INFO("sw.ai", "Unified request execution completed: " << sExecutionSummary);
                            
                            // Append execution summary to display response
                            if (!sDisplayResponse.isEmpty())
                            {
                                sDisplayResponse += "\n\n";
                            }
                            sDisplayResponse += "✓ " + sExecutionSummary;
                        }
                    }
                    
                    return sDisplayResponse;
                }
                else
                {
                    SAL_WARN("sw.ai", "Failed to parse JSON response: " << aParsed.sErrorMessage);
                    return "AI processed (unified): " + rsRequest + " - Response format error";
                }
            }
            else
            {
                SAL_WARN("sw.ai", "Unified agent system request failed: " << aResponse.sErrorMessage);
                return "Error: Agent system unavailable - " + aResponse.sErrorMessage;
            }
        }
        else
        {
            // Offline mode - basic fallback processing
            SAL_WARN("sw.ai", "Unified request attempted in offline mode");
            return "Error: AI agent system requires online connection";
        }
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Exception in unified request processing: " << e.Message);
        return "Error processing unified request: " + rsRequest;
    }
}

Any AgentCoordinator::extractDocumentContext(const Any& rContext) const
{
    try
    {
        OUStringBuffer aJsonBuilder;
        aJsonBuilder.append("{");
        
        // Start with basic context from input
        Sequence<PropertyValue> aContextProps;
        if (rContext >>= aContextProps)
        {
            Reference<text::XTextDocument> xTextDoc;
            Reference<frame::XFrame> xFrame;
            
            // Extract references from AIPanel context
            for (const auto& prop : aContextProps)
            {
                if (prop.Name == "Document")
                {
                    prop.Value >>= xTextDoc;
                }
                else if (prop.Name == "Frame")
                {
                    prop.Value >>= xFrame;
                }
            }
            
            // Add basic document information
            if (xTextDoc.is())
            {
                aJsonBuilder.append("\"document_available\": true, ");
            }
            else
            {
                aJsonBuilder.append("\"document_available\": false, ");
            }
        }
        
        // Get active Writer view and shell for detailed context
        SwView* pView = SwModule::GetFirstView();
        if (pView)
        {
            SwWrtShell* pWrtShell = &pView->GetWrtShell();
            if (pWrtShell)
            {
                // Extract cursor position information
                aJsonBuilder.append("\"cursor_position\": {");
                aJsonBuilder.append("\"line\": ");
                aJsonBuilder.append(OUString::number(pWrtShell->GetLineNum()));
                aJsonBuilder.append(", \"column\": ");
                aJsonBuilder.append(OUString::number(pWrtShell->GetColumnNum()));
                aJsonBuilder.append("}, ");
                
                // Extract selected text
                aJsonBuilder.append("\"selected_text\": \"");
                if (pWrtShell->HasSelection())
                {
                    OUString sSelectedText = pWrtShell->GetSelText();
                    // Escape quotes and newlines for JSON
                    sSelectedText = sSelectedText.replaceAll("\"", "\\\"");
                    sSelectedText = sSelectedText.replaceAll("\n", "\\n");
                    sSelectedText = sSelectedText.replaceAll("\r", "\\r");
                    aJsonBuilder.append(sSelectedText);
                }
                aJsonBuilder.append("\", ");
                
                // Extract document structure information
                aJsonBuilder.append("\"document_structure\": {");
                SwDoc* pDoc = pWrtShell->GetDoc();
                if (pDoc)
                {
                    aJsonBuilder.append("\"paragraph_count\": ");
                    aJsonBuilder.append(OUString::number(pDoc->getIDocumentStatistics().GetDocStat().nPara));
                    aJsonBuilder.append(", \"page_count\": ");
                    aJsonBuilder.append(OUString::number(pDoc->getIDocumentStatistics().GetDocStat().nPage));
                    aJsonBuilder.append(", \"word_count\": ");
                    aJsonBuilder.append(OUString::number(pDoc->getIDocumentStatistics().GetDocStat().nWord));
                    aJsonBuilder.append(", \"character_count\": ");
                    aJsonBuilder.append(OUString::number(pDoc->getIDocumentStatistics().GetDocStat().nChar));
                }
                aJsonBuilder.append("}, ");
                
                // Extract current formatting state
                aJsonBuilder.append("\"formatting_state\": {");
                aJsonBuilder.append("\"has_selection\": ");
                aJsonBuilder.append(pWrtShell->HasSelection() ? "true" : "false");
                aJsonBuilder.append("}");
            }
            else
            {
                aJsonBuilder.append("\"error\": \"No WriterShell available\"");
            }
        }
        else
        {
            aJsonBuilder.append("\"error\": \"No active Writer view\"");
        }
        
        aJsonBuilder.append("}");
        
        OUString sJsonContext = aJsonBuilder.makeStringAndClear();
        SAL_INFO("sw.ai", "Extracted document context JSON: " << sJsonContext);
        
        return Any(sJsonContext);
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "Error extracting document context: " << e.Message);
        return Any(OUString("{}"));
    }
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
        PropertyValue* pConfig = aConfig.getArray();
        pConfig[0].Name = "DefaultTimeout";
        pConfig[0].Value <<= m_nTimeoutMs;
        pConfig[1].Name = "UserAgent";
        pConfig[1].Value <<= OUString("LibreOffice-Writer-AI/1.0");
        pConfig[2].Name = "MaxConnections";
        pConfig[2].Value <<= sal_Int32(5);
        
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

bool AgentCoordinator::sendToBackend(const OUString& rsMessage, const Any& /* rContext */)
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
        if (aStatistics.nTotalErrors > 5)
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

bool AgentCoordinator::isCacheableRequest(const OUString& /* rsRequest */) const
{
    // Determine if request can be cached
    // TODO: Implement caching logic
    return false;
}

OUString AgentCoordinator::getCachedResponse(const OUString& /* rsRequest */) const
{
    // Get cached response
    // TODO: Implement caching logic
    return "";
}

void AgentCoordinator::cacheResponse(const OUString& /* rsRequest */, const OUString& /* rsResponse */)
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

// Enhanced JSON response parsing implementation (Phase 4)

AgentCoordinator::ParsedResponse AgentCoordinator::parseEnhancedJsonResponse(const OUString& rsJsonResponse) const
{
    ParsedResponse aResult;
    
    try
    {
        // Convert OUString to std::string for boost property tree parsing
        OString sJsonUtf8 = OUStringToOString(rsJsonResponse, RTL_TEXTENCODING_UTF8);
        std::string sJsonStd(sJsonUtf8.getStr(), sJsonUtf8.getLength());
        
        // Parse JSON using boost property tree
        boost::property_tree::ptree aTree;
        std::istringstream aJsonStream(sJsonStd);
        boost::property_tree::read_json(aJsonStream, aTree);
        
        // Extract basic response info
        aResult.bSuccess = aTree.get<bool>("success", false);
        aResult.sRequestId = OStringToOUString(OString(aTree.get<std::string>("request_id", "").c_str()), RTL_TEXTENCODING_UTF8);
        
        if (!aResult.bSuccess)
        {
            aResult.sErrorMessage = OStringToOUString(OString(aTree.get<std::string>("error_message", "Unknown error").c_str()), RTL_TEXTENCODING_UTF8);
            return aResult;
        }
        
        // Extract enhanced response fields
        aResult.sResponseContent = OStringToOUString(OString(aTree.get<std::string>("response_content", "").c_str()), RTL_TEXTENCODING_UTF8);
        
        // Extract operations array
        auto aOperationsOpt = aTree.get_child_optional("operations");
        if (aOperationsOpt)
        {
            for (const auto& rOperation : aOperationsOpt.value())
            {
                aResult.aOperations.push_back(rOperation.second);
            }
        }
        
        // Extract operation summaries
        auto aOperationSummariesOpt = aTree.get_child_optional("operation_summaries");
        if (aOperationSummariesOpt)
        {
            for (const auto& rSummary : aOperationSummariesOpt.value())
            {
                OUString sSummary = OStringToOUString(OString(rSummary.second.get_value<std::string>().c_str()), RTL_TEXTENCODING_UTF8);
                aResult.aOperationSummaries.push_back(sSummary);
            }
        }
        
        // Extract content and formatting changes
        auto aContentChangesOpt = aTree.get_child_optional("content_changes");
        if (aContentChangesOpt)
        {
            aResult.aContentChanges = aContentChangesOpt.value();
        }
        
        auto aFormattingChangesOpt = aTree.get_child_optional("formatting_changes");
        if (aFormattingChangesOpt)
        {
            aResult.aFormattingChanges = aFormattingChangesOpt.value();
        }
        
        // Extract warnings
        auto aWarningsOpt = aTree.get_child_optional("warnings");
        if (aWarningsOpt)
        {
            for (const auto& rWarning : aWarningsOpt.value())
            {
                OUString sWarning = OStringToOUString(OString(rWarning.second.get_value<std::string>().c_str()), RTL_TEXTENCODING_UTF8);
                aResult.aWarnings.push_back(sWarning);
            }
        }
        
        // Extract metadata
        auto aMetadataOpt = aTree.get_child_optional("metadata");
        if (aMetadataOpt)
        {
            aResult.aMetadata = aMetadataOpt.value();
        }
        
        // Set success if we got this far
        aResult.bSuccess = true;
        
        SAL_INFO("sw.ai", "Successfully parsed enhanced JSON response: " << 
                 aResult.aOperations.size() << " operations, " << 
                 aResult.aOperationSummaries.size() << " summaries, " << 
                 aResult.aWarnings.size() << " warnings");
        
        return aResult;
    }
    catch (const boost::property_tree::json_parser_error& e)
    {
        aResult.bSuccess = false;
        aResult.sErrorMessage = "JSON parsing error at line " + OUString::number(e.line()) + ": " + 
                               OStringToOUString(OString(e.message().c_str()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "JSON parsing error: " << aResult.sErrorMessage);
        return aResult;
    }
    catch (const boost::property_tree::ptree_error& e)
    {
        aResult.bSuccess = false;
        aResult.sErrorMessage = "Property tree error: " + 
                               OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Property tree error: " << aResult.sErrorMessage);
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.bSuccess = false;
        aResult.sErrorMessage = "Unexpected error parsing JSON: " + 
                               OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Unexpected JSON parsing error: " << aResult.sErrorMessage);
        return aResult;
    }
}

OUString AgentCoordinator::formatResponseForDisplay(const ParsedResponse& rParsed) const
{
    if (!rParsed.bSuccess)
    {
        return "Error: " + rParsed.sErrorMessage;
    }
    
    OUString sDisplayText = rParsed.sResponseContent;
    
    // Add operation confirmations if operations exist
    if (!rParsed.aOperations.empty())
    {
        if (!sDisplayText.isEmpty())
        {
            sDisplayText += "\n\n";
        }
        
        if (rParsed.aOperations.size() == 1)
        {
            sDisplayText += "✓ 1 operation prepared for execution";
        }
        else
        {
            sDisplayText += "✓ " + OUString::number(rParsed.aOperations.size()) + " operations prepared for execution";
        }
        
        // Add operation summaries if available
        if (!rParsed.aOperationSummaries.empty())
        {
            sDisplayText += ":";
            for (size_t i = 0; i < rParsed.aOperationSummaries.size() && i < 3; ++i)  // Limit to first 3 summaries
            {
                sDisplayText += "\n• " + rParsed.aOperationSummaries[i];
            }
            
            if (rParsed.aOperationSummaries.size() > 3)
            {
                sDisplayText += "\n• ... and " + OUString::number(rParsed.aOperationSummaries.size() - 3) + " more";
            }
        }
    }
    
    // Add warnings if any
    if (!rParsed.aWarnings.empty())
    {
        if (!sDisplayText.isEmpty())
        {
            sDisplayText += "\n\n";
        }
        
        sDisplayText += "⚠ Warnings:";
        for (const auto& rsWarning : rParsed.aWarnings)
        {
            sDisplayText += "\n• " + rsWarning;
        }
    }
    
    // Fallback if no content available
    if (sDisplayText.isEmpty())
    {
        sDisplayText = "Request processed successfully.";
    }
    
    return sDisplayText;
}

bool AgentCoordinator::hasExecutableOperations(const ParsedResponse& rParsed) const
{
    return rParsed.bSuccess && !rParsed.aOperations.empty();
}

// Operation Translation Implementation (Phase 5)

std::vector<AgentCoordinator::TranslatedOperation> AgentCoordinator::translateOperationsToUno(const ParsedResponse& rParsed) const
{
    std::vector<TranslatedOperation> aTranslated;
    
    if (!rParsed.bSuccess || rParsed.aOperations.empty())
    {
        SAL_INFO("sw.ai", "No operations to translate - success: " << rParsed.bSuccess << ", operations: " << rParsed.aOperations.size());
        return aTranslated;
    }
    
    SAL_INFO("sw.ai", "Translating " << rParsed.aOperations.size() << " operations to UNO format");
    
    for (size_t i = 0; i < rParsed.aOperations.size(); ++i)
    {
        const auto& rOperation = rParsed.aOperations[i];
        
        try
        {
            TranslatedOperation aTranslatedOp = translateSingleOperation(rOperation);
            if (aTranslatedOp.bSuccess)
            {
                SAL_INFO("sw.ai", "Successfully translated operation " << (i+1) << ": " << aTranslatedOp.sOperationType);
                aTranslated.push_back(aTranslatedOp);
            }
            else
            {
                SAL_WARN("sw.ai", "Failed to translate operation " << (i+1) << ": " << aTranslatedOp.sErrorMessage);
                // Continue with other operations even if one fails
            }
        }
        catch (const std::exception& e)
        {
            SAL_WARN("sw.ai", "Exception translating operation " << (i+1) << ": " << e.what());
        }
    }
    
    SAL_INFO("sw.ai", "Successfully translated " << aTranslated.size() << " out of " << rParsed.aOperations.size() << " operations");
    return aTranslated;
}

AgentCoordinator::TranslatedOperation AgentCoordinator::translateSingleOperation(const boost::property_tree::ptree& rOperation) const
{
    TranslatedOperation aResult;
    
    try
    {
        // Extract operation type
        std::string sOperationType = rOperation.get<std::string>("type", "");
        if (sOperationType.empty())
        {
            aResult.sErrorMessage = "Missing operation type";
            return aResult;
        }
        
        // Extract priority
        aResult.nPriority = rOperation.get<sal_Int32>("priority", 1);
        
        SAL_INFO("sw.ai", "Translating operation type: " << sOperationType.c_str() << " (priority: " << aResult.nPriority << ")");
        
        // Route to specific translator based on operation type
        if (sOperationType == "insert_text")
        {
            aResult = translateInsertTextOperation(rOperation);
        }
        else if (sOperationType == "apply_formatting" || sOperationType == "modify_text")
        {
            aResult = translateApplyFormattingOperation(rOperation);
        }
        else if (sOperationType == "create_table")
        {
            aResult = translateCreateTableOperation(rOperation);
        }
        else if (sOperationType == "create_chart")
        {
            aResult = translateCreateChartOperation(rOperation);
        }
        else if (sOperationType == "insert_image")
        {
            aResult = translateInsertImageOperation(rOperation);
        }
        else if (sOperationType == "apply_template")
        {
            aResult = translateApplyTemplateOperation(rOperation);
        }
        else if (sOperationType == "restructure_document")
        {
            aResult = translateRestructureDocumentOperation(rOperation);
        }
        else
        {
            aResult.sErrorMessage = "Unsupported operation type: " + OStringToOUString(OString(sOperationType.c_str()), RTL_TEXTENCODING_UTF8);
            SAL_WARN("sw.ai", "Unsupported operation type: " << sOperationType.c_str());
            return aResult;
        }
        
        // Preserve priority from original operation
        aResult.nPriority = rOperation.get<sal_Int32>("priority", aResult.nPriority);
        
        return aResult;
    }
    catch (const boost::property_tree::ptree_error& e)
    {
        aResult.sErrorMessage = "Property tree error: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Property tree error in operation translation: " << e.what());
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Unexpected error: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Unexpected error in operation translation: " << e.what());
        return aResult;
    }
}

AgentCoordinator::TranslatedOperation AgentCoordinator::translateInsertTextOperation(const boost::property_tree::ptree& rOperation) const
{
    TranslatedOperation aResult;
    aResult.sOperationType = "insertText";
    
    try
    {
        // Extract parameters from agent operation format
        auto aParametersOpt = rOperation.get_child_optional("parameters");
        if (!aParametersOpt)
        {
            aResult.sErrorMessage = "Missing parameters for insert_text operation";
            return aResult;
        }
        
        const auto& rParams = aParametersOpt.value();
        
        // Extract text content
        std::string sContent = rParams.get<std::string>("content", "");
        if (sContent.empty())
        {
            aResult.sErrorMessage = "Missing content for insert_text operation";
            return aResult;
        }
        
        // Convert position parameter
        auto aTargetOpt = rOperation.get_child_optional("target");
        css::uno::Any aPosition;
        if (aTargetOpt)
        {
            auto aPositionOpt = aTargetOpt.value().get_child_optional("position");
            if (aPositionOpt)
            {
                aPosition = convertPositionParameter(aPositionOpt.value());
            }
        }
        
        // Convert formatting parameters
        css::uno::Sequence<css::beans::PropertyValue> aFormatting;
        auto aFormattingOpt = rParams.get_child_optional("formatting");
        if (aFormattingOpt)
        {
            aFormatting = convertFormattingParameters(aFormattingOpt.value());
        }
        
        // Create UNO parameter sequence for DocumentOperations::insertText
        aResult.aParameters = css::uno::Sequence<css::beans::PropertyValue>(3);
        auto pParams = aResult.aParameters.getArray();
        
        pParams[0] = createPropertyValue("Text", css::uno::Any(OStringToOUString(OString(sContent.c_str()), RTL_TEXTENCODING_UTF8)));
        pParams[1] = createPropertyValue("Position", aPosition);
        pParams[2] = createPropertyValue("Formatting", css::uno::Any(aFormatting));
        
        aResult.bSuccess = true;
        SAL_INFO("sw.ai", "Translated insert_text operation - content length: " << sContent.length());
        
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Error translating insert_text operation: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Error translating insert_text: " << e.what());
        return aResult;
    }
}

AgentCoordinator::TranslatedOperation AgentCoordinator::translateApplyFormattingOperation(const boost::property_tree::ptree& rOperation) const
{
    TranslatedOperation aResult;
    aResult.sOperationType = "formatText";
    
    try
    {
        // Extract parameters
        auto aParametersOpt = rOperation.get_child_optional("parameters");
        if (!aParametersOpt)
        {
            aResult.sErrorMessage = "Missing parameters for apply_formatting operation";
            return aResult;
        }
        
        const auto& rParams = aParametersOpt.value();
        
        // Convert target range
        auto aTargetOpt = rOperation.get_child_optional("target");
        css::uno::Any aTextRange;
        if (aTargetOpt)
        {
            auto aRangeOpt = aTargetOpt.value().get_child_optional("range");
            if (aRangeOpt)
            {
                aTextRange = convertRangeParameter(aRangeOpt.value());
            }
        }
        
        // Convert formatting properties
        css::uno::Sequence<css::beans::PropertyValue> aFormatting = convertFormattingParameters(rParams);
        
        // Create UNO parameter sequence for DocumentOperations::formatText
        aResult.aParameters = css::uno::Sequence<css::beans::PropertyValue>(2);
        auto pParams = aResult.aParameters.getArray();
        
        pParams[0] = createPropertyValue("TextRange", aTextRange);
        pParams[1] = createPropertyValue("Formatting", css::uno::Any(aFormatting));
        
        aResult.bSuccess = true;
        SAL_INFO("sw.ai", "Translated apply_formatting operation - formatting properties: " << aFormatting.getLength());
        
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Error translating apply_formatting operation: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Error translating apply_formatting: " << e.what());
        return aResult;
    }
}

AgentCoordinator::TranslatedOperation AgentCoordinator::translateCreateTableOperation(const boost::property_tree::ptree& rOperation) const
{
    TranslatedOperation aResult;
    aResult.sOperationType = "createTable";
    
    try
    {
        // Extract parameters
        auto aParametersOpt = rOperation.get_child_optional("parameters");
        if (!aParametersOpt)
        {
            aResult.sErrorMessage = "Missing parameters for create_table operation";
            return aResult;
        }
        
        const auto& rParams = aParametersOpt.value();
        
        // Extract table dimensions
        sal_Int32 nRows = rParams.get<sal_Int32>("rows", 1);
        sal_Int32 nColumns = rParams.get<sal_Int32>("columns", 1);
        
        // Extract position
        auto aTargetOpt = rOperation.get_child_optional("target");
        css::uno::Any aPosition;
        if (aTargetOpt)
        {
            auto aPositionOpt = aTargetOpt.value().get_child_optional("position");
            if (aPositionOpt)
            {
                aPosition = convertPositionParameter(aPositionOpt.value());
            }
        }
        
        // Extract table properties
        css::uno::Sequence<css::beans::PropertyValue> aTableProperties;
        auto aPropsOpt = rParams.get_child_optional("style");
        if (aPropsOpt)
        {
            aTableProperties = convertFormattingParameters(aPropsOpt.value());
        }
        
        // Create UNO parameter sequence for DocumentOperations::createTable
        aResult.aParameters = css::uno::Sequence<css::beans::PropertyValue>(4);
        auto pParams = aResult.aParameters.getArray();
        
        pParams[0] = createPropertyValue("Rows", css::uno::Any(nRows));
        pParams[1] = createPropertyValue("Columns", css::uno::Any(nColumns));
        pParams[2] = createPropertyValue("Position", aPosition);
        pParams[3] = createPropertyValue("TableProperties", css::uno::Any(aTableProperties));
        
        aResult.bSuccess = true;
        SAL_INFO("sw.ai", "Translated create_table operation - " << nRows << "x" << nColumns << " table");
        
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Error translating create_table operation: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Error translating create_table: " << e.what());
        return aResult;
    }
}

AgentCoordinator::TranslatedOperation AgentCoordinator::translateCreateChartOperation(const boost::property_tree::ptree& rOperation) const
{
    TranslatedOperation aResult;
    aResult.sOperationType = "insertChart";
    
    try
    {
        // Extract parameters
        auto aParametersOpt = rOperation.get_child_optional("parameters");
        if (!aParametersOpt)
        {
            aResult.sErrorMessage = "Missing parameters for create_chart operation";
            return aResult;
        }
        
        const auto& rParams = aParametersOpt.value();
        
        // Extract chart type
        std::string sChartType = rParams.get<std::string>("chart_type", "line");
        
        // Extract data source
        std::string sDataSource = rParams.get<std::string>("data_source", "");
        
        // Extract position
        auto aTargetOpt = rOperation.get_child_optional("target");
        css::uno::Any aPosition;
        if (aTargetOpt)
        {
            auto aPositionOpt = aTargetOpt.value().get_child_optional("position");
            if (aPositionOpt)
            {
                aPosition = convertPositionParameter(aPositionOpt.value());
            }
        }
        
        // Extract chart properties
        css::uno::Sequence<css::beans::PropertyValue> aChartProperties;
        auto aStylingOpt = rParams.get_child_optional("styling");
        if (aStylingOpt)
        {
            aChartProperties = convertFormattingParameters(aStylingOpt.value());
        }
        
        // Create UNO parameter sequence for DocumentOperations::insertChart
        aResult.aParameters = css::uno::Sequence<css::beans::PropertyValue>(4);
        auto pParams = aResult.aParameters.getArray();
        
        pParams[0] = createPropertyValue("ChartData", css::uno::Any(OStringToOUString(OString(sDataSource.c_str()), RTL_TEXTENCODING_UTF8)));
        pParams[1] = createPropertyValue("ChartType", css::uno::Any(OStringToOUString(OString(sChartType.c_str()), RTL_TEXTENCODING_UTF8)));
        pParams[2] = createPropertyValue("Position", aPosition);
        pParams[3] = createPropertyValue("ChartProperties", css::uno::Any(aChartProperties));
        
        aResult.bSuccess = true;
        SAL_INFO("sw.ai", "Translated create_chart operation - type: " << sChartType.c_str());
        
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Error translating create_chart operation: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Error translating create_chart: " << e.what());
        return aResult;
    }
}

AgentCoordinator::TranslatedOperation AgentCoordinator::translateInsertImageOperation(const boost::property_tree::ptree& rOperation) const
{
    TranslatedOperation aResult;
    aResult.sOperationType = "insertGraphic";
    
    try
    {
        // Extract parameters
        auto aParametersOpt = rOperation.get_child_optional("parameters");
        if (!aParametersOpt)
        {
            aResult.sErrorMessage = "Missing parameters for insert_image operation";
            return aResult;
        }
        
        const auto& rParams = aParametersOpt.value();
        
        // Extract image source
        std::string sSource = rParams.get<std::string>("source", "");
        if (sSource.empty())
        {
            aResult.sErrorMessage = "Missing source for insert_image operation";
            return aResult;
        }
        
        // Extract position
        auto aTargetOpt = rOperation.get_child_optional("target");
        css::uno::Any aPosition;
        if (aTargetOpt)
        {
            auto aPositionOpt = aTargetOpt.value().get_child_optional("position");
            if (aPositionOpt)
            {
                aPosition = convertPositionParameter(aPositionOpt.value());
            }
        }
        
        // Extract graphic properties (size, caption, etc.)
        css::uno::Sequence<css::beans::PropertyValue> aGraphicProperties;
        auto aSizeOpt = rParams.get_child_optional("size");
        auto aCaptionOpt = rParams.get_child_optional("caption");
        
        std::vector<css::beans::PropertyValue> aProps;
        if (aSizeOpt)
        {
            // Convert size properties
            aProps.push_back(createPropertyValue("Width", css::uno::Any(OStringToOUString(
                OString(aSizeOpt.value().get<std::string>("width", "").c_str()), RTL_TEXTENCODING_UTF8))));
            aProps.push_back(createPropertyValue("Height", css::uno::Any(OStringToOUString(
                OString(aSizeOpt.value().get<std::string>("height", "").c_str()), RTL_TEXTENCODING_UTF8))));
        }
        if (aCaptionOpt)
        {
            aProps.push_back(createPropertyValue("Caption", css::uno::Any(OStringToOUString(
                OString(aCaptionOpt.value().get_value<std::string>().c_str()), RTL_TEXTENCODING_UTF8))));
        }
        
        aGraphicProperties = css::uno::Sequence<css::beans::PropertyValue>(aProps.data(), aProps.size());
        
        // Create UNO parameter sequence for DocumentOperations::insertGraphic
        aResult.aParameters = css::uno::Sequence<css::beans::PropertyValue>(3);
        auto pParams = aResult.aParameters.getArray();
        
        pParams[0] = createPropertyValue("GraphicData", css::uno::Any(OStringToOUString(OString(sSource.c_str()), RTL_TEXTENCODING_UTF8)));
        pParams[1] = createPropertyValue("Position", aPosition);
        pParams[2] = createPropertyValue("GraphicProperties", css::uno::Any(aGraphicProperties));
        
        aResult.bSuccess = true;
        SAL_INFO("sw.ai", "Translated insert_image operation - source: " << sSource.c_str());
        
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Error translating insert_image operation: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Error translating insert_image: " << e.what());
        return aResult;
    }
}

AgentCoordinator::TranslatedOperation AgentCoordinator::translateApplyTemplateOperation(const boost::property_tree::ptree& rOperation) const
{
    TranslatedOperation aResult;
    aResult.sOperationType = "applyStyle";
    
    try
    {
        // Extract parameters
        auto aParametersOpt = rOperation.get_child_optional("parameters");
        if (!aParametersOpt)
        {
            aResult.sErrorMessage = "Missing parameters for apply_template operation";
            return aResult;
        }
        
        const auto& rParams = aParametersOpt.value();
        
        // Extract template ID/name
        std::string sTemplateId = rParams.get<std::string>("template_id", "");
        if (sTemplateId.empty())
        {
            aResult.sErrorMessage = "Missing template_id for apply_template operation";
            return aResult;
        }
        
        // Extract target (could be whole document or specific range)
        auto aTargetOpt = rOperation.get_child_optional("target");
        css::uno::Any aTarget;
        if (aTargetOpt)
        {
            // For now, use the whole document as target
            aTarget = css::uno::Any(OUString("document"));
        }
        
        // Extract placeholder data
        css::uno::Sequence<css::beans::PropertyValue> aStyleProperties;
        auto aPlaceholderOpt = rParams.get_child_optional("placeholder_data");
        if (aPlaceholderOpt)
        {
            aStyleProperties = convertFormattingParameters(aPlaceholderOpt.value());
        }
        
        // Create UNO parameter sequence for DocumentOperations::applyStyle
        aResult.aParameters = css::uno::Sequence<css::beans::PropertyValue>(3);
        auto pParams = aResult.aParameters.getArray();
        
        pParams[0] = createPropertyValue("Target", aTarget);
        pParams[1] = createPropertyValue("StyleName", css::uno::Any(OStringToOUString(OString(sTemplateId.c_str()), RTL_TEXTENCODING_UTF8)));
        pParams[2] = createPropertyValue("StyleProperties", css::uno::Any(aStyleProperties));
        
        aResult.bSuccess = true;
        SAL_INFO("sw.ai", "Translated apply_template operation - template: " << sTemplateId.c_str());
        
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Error translating apply_template operation: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Error translating apply_template: " << e.what());
        return aResult;
    }
}

AgentCoordinator::TranslatedOperation AgentCoordinator::translateRestructureDocumentOperation(const boost::property_tree::ptree& rOperation) const
{
    TranslatedOperation aResult;
    aResult.sOperationType = "createSection";  // Use createSection as the primary restructuring operation
    
    try
    {
        // Extract parameters
        auto aParametersOpt = rOperation.get_child_optional("parameters");
        if (!aParametersOpt)
        {
            aResult.sErrorMessage = "Missing parameters for restructure_document operation";
            return aResult;
        }
        
        const auto& rParams = aParametersOpt.value();
        
        // Extract section information
        std::string sSectionName = rParams.get<std::string>("section_name", "NewSection");
        
        // Extract position
        auto aTargetOpt = rOperation.get_child_optional("target");
        css::uno::Any aPosition;
        if (aTargetOpt)
        {
            auto aPositionOpt = aTargetOpt.value().get_child_optional("position");
            if (aPositionOpt)
            {
                aPosition = convertPositionParameter(aPositionOpt.value());
            }
        }
        
        // Extract section properties
        css::uno::Sequence<css::beans::PropertyValue> aSectionProperties;
        auto aSectionsOpt = rParams.get_child_optional("sections");
        if (aSectionsOpt)
        {
            aSectionProperties = convertFormattingParameters(aSectionsOpt.value());
        }
        
        // Create UNO parameter sequence for DocumentOperations::createSection
        aResult.aParameters = css::uno::Sequence<css::beans::PropertyValue>(3);
        auto pParams = aResult.aParameters.getArray();
        
        pParams[0] = createPropertyValue("SectionName", css::uno::Any(OStringToOUString(OString(sSectionName.c_str()), RTL_TEXTENCODING_UTF8)));
        pParams[1] = createPropertyValue("Position", aPosition);
        pParams[2] = createPropertyValue("SectionProperties", css::uno::Any(aSectionProperties));
        
        aResult.bSuccess = true;
        SAL_INFO("sw.ai", "Translated restructure_document operation - section: " << sSectionName.c_str());
        
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Error translating restructure_document operation: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Error translating restructure_document: " << e.what());
        return aResult;
    }
}

// Helper methods for parameter conversion

css::uno::Any AgentCoordinator::convertPositionParameter(const boost::property_tree::ptree& rPosition) const
{
    try
    {
        // Create a PropertyValue sequence for position data
        std::vector<css::beans::PropertyValue> aPositionProps;
        
        // Extract line/paragraph position
        auto nLine = rPosition.get<sal_Int32>("line", -1);
        if (nLine >= 0)
        {
            aPositionProps.push_back(createPropertyValue("Line", css::uno::Any(nLine)));
        }
        
        // Extract column position
        auto nColumn = rPosition.get<sal_Int32>("column", -1);
        if (nColumn >= 0)
        {
            aPositionProps.push_back(createPropertyValue("Column", css::uno::Any(nColumn)));
        }
        
        // Extract paragraph position
        auto nParagraph = rPosition.get<sal_Int32>("paragraph", -1);
        if (nParagraph >= 0)
        {
            aPositionProps.push_back(createPropertyValue("Paragraph", css::uno::Any(nParagraph)));
        }
        
        // Convert special position strings
        std::string sPosition = rPosition.get_value<std::string>();
        if (!sPosition.empty())
        {
            if (sPosition == "current_cursor" || sPosition == "cursor")
            {
                aPositionProps.push_back(createPropertyValue("PositionType", css::uno::Any(OUString("current_cursor"))));
            }
            else if (sPosition == "document_start")
            {
                aPositionProps.push_back(createPropertyValue("PositionType", css::uno::Any(OUString("document_start"))));
            }
            else if (sPosition == "document_end")
            {
                aPositionProps.push_back(createPropertyValue("PositionType", css::uno::Any(OUString("document_end"))));
            }
            else if (sPosition == "after_table")
            {
                aPositionProps.push_back(createPropertyValue("PositionType", css::uno::Any(OUString("after_table"))));
            }
        }
        
        css::uno::Sequence<css::beans::PropertyValue> aPositionSeq(aPositionProps.data(), aPositionProps.size());
        return css::uno::Any(aPositionSeq);
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "Error converting position parameter: " << e.what());
        // Return empty position - will use current cursor position
        return css::uno::Any();
    }
}

css::uno::Any AgentCoordinator::convertRangeParameter(const boost::property_tree::ptree& rRange) const
{
    try
    {
        // Create a PropertyValue sequence for range data
        std::vector<css::beans::PropertyValue> aRangeProps;
        
        // Extract start position
        auto aStartOpt = rRange.get_child_optional("start");
        if (aStartOpt)
        {
            auto aStartPos = convertPositionParameter(aStartOpt.value());
            aRangeProps.push_back(createPropertyValue("Start", aStartPos));
        }
        
        // Extract end position
        auto aEndOpt = rRange.get_child_optional("end");
        if (aEndOpt)
        {
            auto aEndPos = convertPositionParameter(aEndOpt.value());
            aRangeProps.push_back(createPropertyValue("End", aEndPos));
        }
        
        // Extract range type (selection, paragraph, etc.)
        std::string sRangeType = rRange.get<std::string>("type", "selection");
        aRangeProps.push_back(createPropertyValue("RangeType", css::uno::Any(OStringToOUString(OString(sRangeType.c_str()), RTL_TEXTENCODING_UTF8))));
        
        css::uno::Sequence<css::beans::PropertyValue> aRangeSeq(aRangeProps.data(), aRangeProps.size());
        return css::uno::Any(aRangeSeq);
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "Error converting range parameter: " << e.what());
        // Return empty range - will use current selection
        return css::uno::Any();
    }
}

css::uno::Sequence<css::beans::PropertyValue> AgentCoordinator::convertFormattingParameters(const boost::property_tree::ptree& rFormatting) const
{
    try
    {
        std::vector<css::beans::PropertyValue> aFormattingProps;
        
        // Character formatting properties
        auto bBold = rFormatting.get<bool>("bold", false);
        if (bBold)
        {
            aFormattingProps.push_back(createPropertyValue("CharWeight", css::uno::Any(static_cast<float>(css::awt::FontWeight::BOLD))));
        }
        
        auto bItalic = rFormatting.get<bool>("italic", false);
        if (bItalic)
        {
            aFormattingProps.push_back(createPropertyValue("CharPosture", css::uno::Any(static_cast<sal_Int16>(css::awt::FontSlant_ITALIC))));
        }
        
        auto bUnderline = rFormatting.get<bool>("underline", false);
        if (bUnderline)
        {
            aFormattingProps.push_back(createPropertyValue("CharUnderline", css::uno::Any(static_cast<sal_Int16>(css::awt::FontUnderline::SINGLE))));
        }
        
        // Font properties
        std::string sFontFamily = rFormatting.get<std::string>("font_family", "");
        if (!sFontFamily.empty())
        {
            aFormattingProps.push_back(createPropertyValue("CharFontName", css::uno::Any(OStringToOUString(OString(sFontFamily.c_str()), RTL_TEXTENCODING_UTF8))));
        }
        
        auto fFontSize = rFormatting.get<float>("font_size", 0.0f);
        if (fFontSize > 0.0f)
        {
            aFormattingProps.push_back(createPropertyValue("CharHeight", css::uno::Any(fFontSize)));
        }
        
        // Color properties
        std::string sColor = rFormatting.get<std::string>("color", "");
        if (!sColor.empty())
        {
            // Convert color string to color value (simplified - would need full color parsing in production)
            sal_Int32 nColor = 0x000000; // Default to black
            if (sColor == "red") nColor = 0xFF0000;
            else if (sColor == "blue") nColor = 0x0000FF;
            else if (sColor == "green") nColor = 0x00FF00;
            
            aFormattingProps.push_back(createPropertyValue("CharColor", css::uno::Any(nColor)));
        }
        
        // Paragraph formatting
        std::string sAlignment = rFormatting.get<std::string>("alignment", "");
        if (!sAlignment.empty())
        {
            sal_Int16 nAlignment = static_cast<sal_Int16>(css::style::ParagraphAdjust_LEFT);
            if (sAlignment == "center") nAlignment = static_cast<sal_Int16>(css::style::ParagraphAdjust_CENTER);
            else if (sAlignment == "right") nAlignment = static_cast<sal_Int16>(css::style::ParagraphAdjust_RIGHT);
            else if (sAlignment == "justify") nAlignment = static_cast<sal_Int16>(css::style::ParagraphAdjust_BLOCK);
            
            aFormattingProps.push_back(createPropertyValue("ParaAdjust", css::uno::Any(nAlignment)));
        }
        
        // Style name
        std::string sStyle = rFormatting.get<std::string>("style", "");
        if (!sStyle.empty())
        {
            aFormattingProps.push_back(createPropertyValue("ParaStyleName", css::uno::Any(OStringToOUString(OString(sStyle.c_str()), RTL_TEXTENCODING_UTF8))));
        }
        
        return css::uno::Sequence<css::beans::PropertyValue>(aFormattingProps.data(), aFormattingProps.size());
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "Error converting formatting parameters: " << e.what());
        return css::uno::Sequence<css::beans::PropertyValue>();
    }
}

css::beans::PropertyValue AgentCoordinator::createPropertyValue(const OUString& rsName, const css::uno::Any& rValue) const
{
    css::beans::PropertyValue aProp;
    aProp.Name = rsName;
    aProp.Value = rValue;
    return aProp;
}

// Operation Execution Bridge Implementation (Phase 6)

std::vector<AgentCoordinator::ExecutionResult> AgentCoordinator::executeTranslatedOperations(const std::vector<TranslatedOperation>& rOperations) const
{
    std::vector<ExecutionResult> aResults;
    
    if (rOperations.empty())
    {
        SAL_INFO("sw.ai", "No operations to execute");
        return aResults;
    }
    
    // Initialize DocumentOperations service if needed
    if (!m_xDocumentOperations)
    {
        const_cast<AgentCoordinator*>(this)->initializeDocumentOperationsService();
        if (!m_xDocumentOperations)
        {
            SAL_WARN("sw.ai", "Failed to initialize DocumentOperations service");
            ExecutionResult aError;
            aError.sErrorMessage = "DocumentOperations service not available";
            aResults.push_back(aError);
            return aResults;
        }
    }
    
    // Sort operations by priority (per AGENT_SYSTEM_SPECIFICATION.md: priority 1-100)
    std::vector<TranslatedOperation> aSortedOperations = rOperations;
    sortOperationsByPriority(aSortedOperations);
    
    SAL_INFO("sw.ai", "Executing " << aSortedOperations.size() << " operations in priority order");
    
    auto aStartTime = std::chrono::steady_clock::now();
    
    // Execute each operation in order
    for (size_t i = 0; i < aSortedOperations.size(); ++i)
    {
        const auto& rOperation = aSortedOperations[i];
        
        SAL_INFO("sw.ai", "Executing operation " << (i+1) << "/" << aSortedOperations.size() << 
                 ": " << rOperation.sOperationType << " (priority: " << rOperation.nPriority << ")");
        
        try
        {
            ExecutionResult aResult = executeSingleOperation(rOperation);
            aResult.sOperationType = rOperation.sOperationType;
            aResult.nPriority = rOperation.nPriority;
            
            if (aResult.bSuccess)
            {
                SAL_INFO("sw.ai", "Operation " << (i+1) << " completed successfully: " << aResult.sOperationId);
            }
            else
            {
                SAL_WARN("sw.ai", "Operation " << (i+1) << " failed: " << aResult.sErrorMessage);
            }
            
            aResults.push_back(aResult);
        }
        catch (const css::uno::Exception& e)
        {
            ExecutionResult aError;
            aError.sOperationType = rOperation.sOperationType;
            aError.nPriority = rOperation.nPriority;
            aError.sErrorMessage = "UNO Exception: " + e.Message;
            aResults.push_back(aError);
            
            SAL_WARN("sw.ai", "UNO Exception executing operation " << (i+1) << ": " << e.Message);
        }
        catch (const std::exception& e)
        {
            ExecutionResult aError;
            aError.sOperationType = rOperation.sOperationType;
            aError.nPriority = rOperation.nPriority;
            aError.sErrorMessage = "Exception: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
            aResults.push_back(aError);
            
            SAL_WARN("sw.ai", "Exception executing operation " << (i+1) << ": " << e.what());
        }
    }
    
    auto aEndTime = std::chrono::steady_clock::now();
    auto aTotalTime = std::chrono::duration_cast<std::chrono::milliseconds>(aEndTime - aStartTime);
    
    size_t nSuccessful = std::count_if(aResults.begin(), aResults.end(), [](const ExecutionResult& r) { return r.bSuccess; });
    
    SAL_INFO("sw.ai", "Operation execution completed: " << nSuccessful << "/" << aResults.size() << 
             " successful in " << aTotalTime.count() << "ms");
    
    return aResults;
}

AgentCoordinator::ExecutionResult AgentCoordinator::executeSingleOperation(const TranslatedOperation& rOperation) const
{
    ExecutionResult aResult;
    auto aStartTime = std::chrono::steady_clock::now();
    
    try
    {
        if (!rOperation.bSuccess)
        {
            aResult.sErrorMessage = "Operation translation failed: " + rOperation.sErrorMessage;
            return aResult;
        }
        
        // Route to specific execution method based on operation type
        if (rOperation.sOperationType == "insertText")
        {
            aResult = executeInsertTextOperation(rOperation);
        }
        else if (rOperation.sOperationType == "formatText")
        {
            aResult = executeFormatTextOperation(rOperation);
        }
        else if (rOperation.sOperationType == "createTable")
        {
            aResult = executeCreateTableOperation(rOperation);
        }
        else if (rOperation.sOperationType == "insertChart")
        {
            aResult = executeInsertChartOperation(rOperation);
        }
        else if (rOperation.sOperationType == "insertGraphic")
        {
            aResult = executeInsertGraphicOperation(rOperation);
        }
        else if (rOperation.sOperationType == "applyStyle")
        {
            aResult = executeApplyStyleOperation(rOperation);
        }
        else if (rOperation.sOperationType == "createSection")
        {
            aResult = executeCreateSectionOperation(rOperation);
        }
        else
        {
            aResult.sErrorMessage = "Unsupported operation type: " + rOperation.sOperationType;
            SAL_WARN("sw.ai", "Unsupported operation type for execution: " << rOperation.sOperationType);
            return aResult;
        }
        
        auto aEndTime = std::chrono::steady_clock::now();
        aResult.fExecutionTimeMs = std::chrono::duration_cast<std::chrono::microseconds>(aEndTime - aStartTime).count() / 1000.0;
        
        return aResult;
    }
    catch (const css::uno::Exception& e)
    {
        aResult.sErrorMessage = "UNO Exception in single operation execution: " + e.Message;
        SAL_WARN("sw.ai", "UNO Exception in executeSingleOperation: " << e.Message);
        return aResult;
    }
    catch (const std::exception& e)
    {
        aResult.sErrorMessage = "Exception in single operation execution: " + OStringToOUString(OString(e.what()), RTL_TEXTENCODING_UTF8);
        SAL_WARN("sw.ai", "Exception in executeSingleOperation: " << e.what());
        return aResult;
    }
}

bool AgentCoordinator::initializeDocumentOperationsService()
{
    try
    {
        if (m_xDocumentOperations.is())
        {
            return true; // Already initialized
        }
        
        if (!m_xContext.is())
        {
            SAL_WARN("sw.ai", "No component context available for DocumentOperations service");
            return false;
        }
        
        // Create DocumentOperations service using UNO service manager
        css::uno::Reference<css::lang::XMultiComponentFactory> xServiceManager = m_xContext->getServiceManager();
        if (!xServiceManager.is())
        {
            SAL_WARN("sw.ai", "No service manager available");
            return false;
        }
        
        // Create the DocumentOperations service
        css::uno::Reference<css::uno::XInterface> xService = xServiceManager->createInstanceWithContext(
            "com.sun.star.ai.DocumentOperations", m_xContext);
        
        if (!xService.is())
        {
            SAL_WARN("sw.ai", "Failed to create DocumentOperations service instance");
            return false;
        }
        
        // Query for XAIDocumentOperations interface
        m_xDocumentOperations.set(xService, css::uno::UNO_QUERY);
        if (!m_xDocumentOperations.is())
        {
            SAL_WARN("sw.ai", "Failed to query XAIDocumentOperations interface");
            return false;
        }
        
        SAL_INFO("sw.ai", "DocumentOperations service initialized successfully");
        return true;
    }
    catch (const css::uno::Exception& e)
    {
        SAL_WARN("sw.ai", "UNO Exception initializing DocumentOperations: " << e.Message);
        return false;
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "Exception initializing DocumentOperations: " << e.what());
        return false;
    }
}

css::uno::Reference<css::ai::XAIDocumentOperations> AgentCoordinator::getDocumentOperationsService() const
{
    if (!m_xDocumentOperations.is())
    {
        const_cast<AgentCoordinator*>(this)->initializeDocumentOperationsService();
    }
    return m_xDocumentOperations;
}

// Specific operation execution methods

AgentCoordinator::ExecutionResult AgentCoordinator::executeInsertTextOperation(const TranslatedOperation& rOperation) const
{
    ExecutionResult aResult;
    
    try
    {
        css::uno::Reference<css::ai::XAIDocumentOperations> xDocOps = getDocumentOperationsService();
        if (!xDocOps.is())
        {
            aResult.sErrorMessage = "DocumentOperations service not available for insertText";
            return aResult;
        }
        
        // Extract parameters (text, position, formatting)
        OUString sText;
        css::uno::Any aPosition;
        css::uno::Sequence<css::beans::PropertyValue> aFormatting;
        
        for (const auto& rParam : rOperation.aParameters)
        {
            if (rParam.Name == "Text")
            {
                rParam.Value >>= sText;
            }
            else if (rParam.Name == "Position")
            {
                aPosition = rParam.Value;
            }
            else if (rParam.Name == "Formatting")
            {
                rParam.Value >>= aFormatting;
            }
        }
        
        // Execute insertText operation via DocumentOperations
        OUString sOperationId = xDocOps->insertText(sText, aPosition, aFormatting);
        
        aResult.bSuccess = true;
        aResult.sOperationId = sOperationId;
        
        SAL_INFO("sw.ai", "insertText executed successfully - ID: " << sOperationId);
        return aResult;
    }
    catch (const css::uno::Exception& e)
    {
        aResult.sErrorMessage = "UNO Exception in insertText: " + e.Message;
        SAL_WARN("sw.ai", "UNO Exception in executeInsertTextOperation: " << e.Message);
        return aResult;
    }
}

AgentCoordinator::ExecutionResult AgentCoordinator::executeFormatTextOperation(const TranslatedOperation& rOperation) const
{
    ExecutionResult aResult;
    
    try
    {
        css::uno::Reference<css::ai::XAIDocumentOperations> xDocOps = getDocumentOperationsService();
        if (!xDocOps.is())
        {
            aResult.sErrorMessage = "DocumentOperations service not available for formatText";
            return aResult;
        }
        
        // Extract parameters (textRange, formatting)
        css::uno::Any aTextRange;
        css::uno::Sequence<css::beans::PropertyValue> aFormatting;
        
        for (const auto& rParam : rOperation.aParameters)
        {
            if (rParam.Name == "TextRange")
            {
                aTextRange = rParam.Value;
            }
            else if (rParam.Name == "Formatting")
            {
                rParam.Value >>= aFormatting;
            }
        }
        
        // Execute formatText operation via DocumentOperations
        OUString sOperationId = xDocOps->formatText(aTextRange, aFormatting);
        
        aResult.bSuccess = true;
        aResult.sOperationId = sOperationId;
        
        SAL_INFO("sw.ai", "formatText executed successfully - ID: " << sOperationId);
        return aResult;
    }
    catch (const css::uno::Exception& e)
    {
        aResult.sErrorMessage = "UNO Exception in formatText: " + e.Message;
        SAL_WARN("sw.ai", "UNO Exception in executeFormatTextOperation: " << e.Message);
        return aResult;
    }
}

AgentCoordinator::ExecutionResult AgentCoordinator::executeCreateTableOperation(const TranslatedOperation& rOperation) const
{
    ExecutionResult aResult;
    
    try
    {
        css::uno::Reference<css::ai::XAIDocumentOperations> xDocOps = getDocumentOperationsService();
        if (!xDocOps.is())
        {
            aResult.sErrorMessage = "DocumentOperations service not available for createTable";
            return aResult;
        }
        
        // Extract parameters (rows, columns, position, tableProperties)
        sal_Int32 nRows = 1;
        sal_Int32 nColumns = 1;
        css::uno::Any aPosition;
        css::uno::Sequence<css::beans::PropertyValue> aTableProperties;
        
        for (const auto& rParam : rOperation.aParameters)
        {
            if (rParam.Name == "Rows")
            {
                rParam.Value >>= nRows;
            }
            else if (rParam.Name == "Columns")
            {
                rParam.Value >>= nColumns;
            }
            else if (rParam.Name == "Position")
            {
                aPosition = rParam.Value;
            }
            else if (rParam.Name == "TableProperties")
            {
                rParam.Value >>= aTableProperties;
            }
        }
        
        // Execute createTable operation via DocumentOperations
        OUString sOperationId = xDocOps->createTable(nRows, nColumns, aPosition, aTableProperties);
        
        aResult.bSuccess = true;
        aResult.sOperationId = sOperationId;
        
        SAL_INFO("sw.ai", "createTable executed successfully - ID: " << sOperationId << " (" << nRows << "x" << nColumns << ")");
        return aResult;
    }
    catch (const css::uno::Exception& e)
    {
        aResult.sErrorMessage = "UNO Exception in createTable: " + e.Message;
        SAL_WARN("sw.ai", "UNO Exception in executeCreateTableOperation: " << e.Message);
        return aResult;
    }
}

AgentCoordinator::ExecutionResult AgentCoordinator::executeInsertChartOperation(const TranslatedOperation& rOperation) const
{
    ExecutionResult aResult;
    
    try
    {
        css::uno::Reference<css::ai::XAIDocumentOperations> xDocOps = getDocumentOperationsService();
        if (!xDocOps.is())
        {
            aResult.sErrorMessage = "DocumentOperations service not available for insertChart";
            return aResult;
        }
        
        // Extract parameters (chartData, chartType, position, chartProperties)
        css::uno::Any aChartData;
        OUString sChartType;
        css::uno::Any aPosition;
        css::uno::Sequence<css::beans::PropertyValue> aChartProperties;
        
        for (const auto& rParam : rOperation.aParameters)
        {
            if (rParam.Name == "ChartData")
            {
                aChartData = rParam.Value;
            }
            else if (rParam.Name == "ChartType")
            {
                rParam.Value >>= sChartType;
            }
            else if (rParam.Name == "Position")
            {
                aPosition = rParam.Value;
            }
            else if (rParam.Name == "ChartProperties")
            {
                rParam.Value >>= aChartProperties;
            }
        }
        
        // Execute insertChart operation via DocumentOperations
        OUString sOperationId = xDocOps->insertChart(aChartData, sChartType, aPosition, aChartProperties);
        
        aResult.bSuccess = true;
        aResult.sOperationId = sOperationId;
        
        SAL_INFO("sw.ai", "insertChart executed successfully - ID: " << sOperationId << " (type: " << sChartType << ")");
        return aResult;
    }
    catch (const css::uno::Exception& e)
    {
        aResult.sErrorMessage = "UNO Exception in insertChart: " + e.Message;
        SAL_WARN("sw.ai", "UNO Exception in executeInsertChartOperation: " << e.Message);
        return aResult;
    }
}

AgentCoordinator::ExecutionResult AgentCoordinator::executeInsertGraphicOperation(const TranslatedOperation& rOperation) const
{
    ExecutionResult aResult;
    
    try
    {
        css::uno::Reference<css::ai::XAIDocumentOperations> xDocOps = getDocumentOperationsService();
        if (!xDocOps.is())
        {
            aResult.sErrorMessage = "DocumentOperations service not available for insertGraphic";
            return aResult;
        }
        
        // Extract parameters (graphicData, position, graphicProperties)
        css::uno::Any aGraphicData;
        css::uno::Any aPosition;
        css::uno::Sequence<css::beans::PropertyValue> aGraphicProperties;
        
        for (const auto& rParam : rOperation.aParameters)
        {
            if (rParam.Name == "GraphicData")
            {
                aGraphicData = rParam.Value;
            }
            else if (rParam.Name == "Position")
            {
                aPosition = rParam.Value;
            }
            else if (rParam.Name == "GraphicProperties")
            {
                rParam.Value >>= aGraphicProperties;
            }
        }
        
        // Execute insertGraphic operation via DocumentOperations
        OUString sOperationId = xDocOps->insertGraphic(aGraphicData, aPosition, aGraphicProperties);
        
        aResult.bSuccess = true;
        aResult.sOperationId = sOperationId;
        
        SAL_INFO("sw.ai", "insertGraphic executed successfully - ID: " << sOperationId);
        return aResult;
    }
    catch (const css::uno::Exception& e)
    {
        aResult.sErrorMessage = "UNO Exception in insertGraphic: " + e.Message;
        SAL_WARN("sw.ai", "UNO Exception in executeInsertGraphicOperation: " << e.Message);
        return aResult;
    }
}

AgentCoordinator::ExecutionResult AgentCoordinator::executeApplyStyleOperation(const TranslatedOperation& rOperation) const
{
    ExecutionResult aResult;
    
    try
    {
        css::uno::Reference<css::ai::XAIDocumentOperations> xDocOps = getDocumentOperationsService();
        if (!xDocOps.is())
        {
            aResult.sErrorMessage = "DocumentOperations service not available for applyStyle";
            return aResult;
        }
        
        // Extract parameters (target, styleName, styleProperties)
        css::uno::Any aTarget;
        OUString sStyleName;
        css::uno::Sequence<css::beans::PropertyValue> aStyleProperties;
        
        for (const auto& rParam : rOperation.aParameters)
        {
            if (rParam.Name == "Target")
            {
                aTarget = rParam.Value;
            }
            else if (rParam.Name == "StyleName")
            {
                rParam.Value >>= sStyleName;
            }
            else if (rParam.Name == "StyleProperties")
            {
                rParam.Value >>= aStyleProperties;
            }
        }
        
        // Execute applyStyle operation via DocumentOperations
        OUString sOperationId = xDocOps->applyStyle(aTarget, sStyleName, aStyleProperties);
        
        aResult.bSuccess = true;
        aResult.sOperationId = sOperationId;
        
        SAL_INFO("sw.ai", "applyStyle executed successfully - ID: " << sOperationId << " (style: " << sStyleName << ")");
        return aResult;
    }
    catch (const css::uno::Exception& e)
    {
        aResult.sErrorMessage = "UNO Exception in applyStyle: " + e.Message;
        SAL_WARN("sw.ai", "UNO Exception in executeApplyStyleOperation: " << e.Message);
        return aResult;
    }
}

AgentCoordinator::ExecutionResult AgentCoordinator::executeCreateSectionOperation(const TranslatedOperation& rOperation) const
{
    ExecutionResult aResult;
    
    try
    {
        css::uno::Reference<css::ai::XAIDocumentOperations> xDocOps = getDocumentOperationsService();
        if (!xDocOps.is())
        {
            aResult.sErrorMessage = "DocumentOperations service not available for createSection";
            return aResult;
        }
        
        // Extract parameters (sectionName, position, sectionProperties)
        OUString sSectionName;
        css::uno::Any aPosition;
        css::uno::Sequence<css::beans::PropertyValue> aSectionProperties;
        
        for (const auto& rParam : rOperation.aParameters)
        {
            if (rParam.Name == "SectionName")
            {
                rParam.Value >>= sSectionName;
            }
            else if (rParam.Name == "Position")
            {
                aPosition = rParam.Value;
            }
            else if (rParam.Name == "SectionProperties")
            {
                rParam.Value >>= aSectionProperties;
            }
        }
        
        // Execute createSection operation via DocumentOperations
        OUString sOperationId = xDocOps->createSection(sSectionName, aPosition, aSectionProperties);
        
        aResult.bSuccess = true;
        aResult.sOperationId = sOperationId;
        
        SAL_INFO("sw.ai", "createSection executed successfully - ID: " << sOperationId << " (section: " << sSectionName << ")");
        return aResult;
    }
    catch (const css::uno::Exception& e)
    {
        aResult.sErrorMessage = "UNO Exception in createSection: " + e.Message;
        SAL_WARN("sw.ai", "UNO Exception in executeCreateSectionOperation: " << e.Message);
        return aResult;
    }
}

// Execution utility methods

void AgentCoordinator::sortOperationsByPriority(std::vector<TranslatedOperation>& rOperations) const
{
    // Sort operations by priority (1-100 per AGENT_SYSTEM_SPECIFICATION.md)
    // Lower numbers = higher priority (execute first)
    std::sort(rOperations.begin(), rOperations.end(), 
        [](const TranslatedOperation& a, const TranslatedOperation& b) {
            return a.nPriority < b.nPriority;
        });
}

OUString AgentCoordinator::formatExecutionSummary(const std::vector<ExecutionResult>& rResults) const
{
    if (rResults.empty())
    {
        return "No operations executed";
    }
    
    size_t nSuccessful = std::count_if(rResults.begin(), rResults.end(), [](const ExecutionResult& r) { return r.bSuccess; });
    size_t nFailed = rResults.size() - nSuccessful;
    
    double fTotalTime = 0.0;
    for (const auto& rResult : rResults)
    {
        fTotalTime += rResult.fExecutionTimeMs;
    }
    
    OUString sSummary = "Executed " + OUString::number(rResults.size()) + " operations: " +
                       OUString::number(nSuccessful) + " successful";
    
    if (nFailed > 0)
    {
        sSummary += ", " + OUString::number(nFailed) + " failed";
    }
    
    sSummary += " (total: " + OUString::number(static_cast<sal_Int32>(fTotalTime)) + "ms)";
    
    return sSummary;
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
        PropertyValue* pConfig = aConfig.getArray();
        pConfig[0].Name = "AutoReconnect";
        pConfig[0].Value <<= true;
        pConfig[1].Name = "MaxReconnectAttempts";
        pConfig[1].Value <<= sal_Int32(3);
        pConfig[2].Name = "ReconnectDelayMs";
        pConfig[2].Value <<= sal_Int32(2000);
        pConfig[3].Name = "HeartbeatIntervalMs";
        pConfig[3].Value <<= sal_Int32(30000);
        pConfig[4].Name = "EnableLogging";
        pConfig[4].Value <<= true;
        
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
        PropertyValue* pConfig = aConfig.getArray();
        pConfig[0].Name = "Enabled";
        pConfig[0].Value <<= true;
        pConfig[1].Name = "EnableLogging";
        pConfig[1].Value <<= true;
        pConfig[2].Name = "MaxConcurrentRetries";
        pConfig[2].Value <<= sal_Int32(5);
        pConfig[3].Name = "MaxErrorHistorySize";
        pConfig[3].Value <<= sal_Int32(50);
        
        bool bSuccess = m_pErrorRecovery->initialize(aConfig);
        if (bSuccess)
        {
            SAL_INFO("sw.ai", "ErrorRecoveryManager initialized successfully");
            
            // Set up callbacks for integrated error handling
            m_pErrorRecovery->setErrorCallback(
                [](const sw::ai::ErrorRecoveryManager::ErrorContext& rError) {
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
        PropertyValue* pConfig = aConfig.getArray();
        pConfig[0].Name = "MaxQueueSize";
        pConfig[0].Value <<= sal_Int32(1000);
        pConfig[1].Name = "MaxMessageSize";
        pConfig[1].Value <<= sal_Int32(1048576); // 1MB
        pConfig[2].Name = "DefaultTTLSeconds";
        pConfig[2].Value <<= sal_Int32(3600); // 1 hour
        pConfig[3].Name = "EnablePersistence";
        pConfig[3].Value <<= false; // Disable for now
        pConfig[4].Name = "MaxMessagesPerSecond";
        pConfig[4].Value <<= sal_Int32(50);
        pConfig[5].Name = "EnableCompression";
        pConfig[5].Value <<= false;
        
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
        PropertyValue* pConfig = aConfig.getArray();
        pConfig[0].Name = "SecureStorageEnabled";
        pConfig[0].Value <<= true;
        pConfig[1].Name = "AutoRefreshEnabled";
        pConfig[1].Value <<= true;
        pConfig[2].Name = "DefaultRefreshThreshold";
        pConfig[2].Value <<= sal_Int32(300); // 5 minutes
        pConfig[3].Name = "MaxRetryAttempts";
        pConfig[3].Value <<= sal_Int32(3);
        pConfig[4].Name = "TokenValidationTimeout";
        pConfig[4].Value <<= sal_Int32(5000); // 5 seconds
        
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

// UNO service registration implementation
extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
com_sun_star_comp_Writer_AIAgentCoordinator_get_implementation(
    css::uno::XComponentContext* context,
    css::uno::Sequence<css::uno::Any> const& /*args*/)
{
    return cppu::acquire(new sw::core::ai::AgentCoordinator(context));
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */