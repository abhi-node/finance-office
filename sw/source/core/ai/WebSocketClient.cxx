/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "WebSocketClient.hxx"

#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/random.hxx>
#include <tools/datetime.hxx>
#include <rtl/ustrbuf.hxx>
#include <rtl/uri.hxx>

using namespace css;
using namespace css::uno;
using namespace css::beans;

namespace sw::ai {

WebSocketClient::WebSocketClient(const Reference<XComponentContext>& xContext)
    : m_xContext(xContext)
    , m_eConnectionState(ConnectionState::DISCONNECTED)
    , m_sProtocol("langgraph-ai")
    , m_nPort(8000)
    , m_bAutoReconnect(true)
    , m_nMaxReconnectAttempts(5)
    , m_nReconnectDelayMs(2000)
    , m_nCurrentReconnectAttempt(0)
    , m_bShutdownRequested(false)
    , m_bEnableLogging(true)
    , m_nHeartbeatIntervalMs(30000)
{
    SAL_INFO("sw.ai", "WebSocketClient created");
}

WebSocketClient::~WebSocketClient()
{
    shutdown();
    SAL_INFO("sw.ai", "WebSocketClient destroyed");
}

bool WebSocketClient::initialize(const Sequence<PropertyValue>& rConfig)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    try
    {
        // Parse configuration parameters
        parseConfiguration(rConfig);
        
        // Initialize connection metrics
        m_aMetrics = ConnectionMetrics();
        
        // Set up default callbacks if none provided
        if (!m_aMessageCallback)
        {
            m_aMessageCallback = [this](const WebSocketMessage& /*aMessage*/) {
                logDebug("Default message callback - message received");
            };
        }
        
        if (!m_aConnectionCallback)
        {
            m_aConnectionCallback = [this](ConnectionState eState, const OUString& rsMessage) {
                logDebug("Connection state changed to " + OUString::number(static_cast<int>(eState)) + ": " + rsMessage);
            };
        }
        
        if (!m_aErrorCallback)
        {
            m_aErrorCallback = [](const OUString& rsError, sal_Int32 nCode) {
                SAL_WARN("sw.ai", "WebSocket error (" << nCode << "): " << rsError);
            };
        }
        
        SAL_INFO("sw.ai", "WebSocketClient initialized successfully");
        return true;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "WebSocketClient initialization failed: " << e.Message);
        return false;
    }
}

void WebSocketClient::shutdown()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (m_bShutdownRequested)
        return;
    
    m_bShutdownRequested = true;
    
    // Disconnect if connected
    if (m_eConnectionState == ConnectionState::CONNECTED || 
        m_eConnectionState == ConnectionState::CONNECTING)
    {
        updateConnectionState(ConnectionState::DISCONNECTED, "Shutdown requested");
    }
    
    // Wake up waiting threads
    m_aConnectionCV.notify_all();
    m_aMessageCV.notify_all();
    
    // Wait for threads to complete
    if (m_pConnectionThread && m_pConnectionThread->joinable())
    {
        m_pConnectionThread->join();
        m_pConnectionThread.reset();
    }
    
    if (m_pMessageProcessingThread && m_pMessageProcessingThread->joinable())
    {
        m_pMessageProcessingThread->join();
        m_pMessageProcessingThread.reset();
    }
    
    // Clear message queues
    std::queue<WebSocketMessage> empty1;
    std::queue<WebSocketMessage> empty2;
    m_aIncomingMessages.swap(empty1);
    m_aOutgoingMessages.swap(empty2);
    
    SAL_INFO("sw.ai", "WebSocketClient shut down");
}

bool WebSocketClient::connect(const OUString& rsUrl, const OUString& rsProtocol)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (m_eConnectionState == ConnectionState::CONNECTED || 
        m_eConnectionState == ConnectionState::CONNECTING)
    {
        SAL_WARN("sw.ai", "WebSocket already connected or connecting");
        return false;
    }
    
    if (!validateWebSocketUrl(rsUrl))
    {
        handleWebSocketError("Invalid WebSocket URL: " + rsUrl, 400);
        return false;
    }
    
    m_sWebSocketUrl = rsUrl;
    if (!rsProtocol.isEmpty())
        m_sProtocol = rsProtocol;
    
    m_nCurrentReconnectAttempt = 0;
    updateConnectionState(ConnectionState::CONNECTING, "Initiating connection");
    
    // Start connection thread
    m_pConnectionThread = std::make_unique<std::thread>(&WebSocketClient::connectionThreadMain, this);
    
    // Start message processing thread
    m_pMessageProcessingThread = std::make_unique<std::thread>(&WebSocketClient::messageProcessingThreadMain, this);
    
    logDebug("WebSocket connection initiated to: " + rsUrl);
    return true;
}

void WebSocketClient::disconnect()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (m_eConnectionState == ConnectionState::DISCONNECTED)
        return;
    
    updateConnectionState(ConnectionState::DISCONNECTED, "Manual disconnect");
    m_aConnectionCV.notify_all();
    
    logDebug("WebSocket disconnected");
}

WebSocketClient::ConnectionState WebSocketClient::getConnectionState() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_eConnectionState;
}

bool WebSocketClient::isConnected() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_eConnectionState == ConnectionState::CONNECTED;
}

bool WebSocketClient::sendMessage(const OUString& rsMessage, MessageType eType)
{
    std::unique_lock<std::mutex> aLock(m_aMutex);
    
    if (m_eConnectionState != ConnectionState::CONNECTED)
    {
        logDebug("Cannot send message - not connected");
        return false;
    }
    
    WebSocketMessage aMessage(eType, rsMessage);
    aMessage.aHeaders = createProtocolHeaders(eType);
    
    m_aOutgoingMessages.push(aMessage);
    aLock.unlock();
    
    m_aMessageCV.notify_one();
    
    ++m_aMetrics.nMessagesSent;
    logDebug("Message queued for sending: " + rsMessage.copy(0, std::min(rsMessage.getLength(), 50)) + "...");
    return true;
}

bool WebSocketClient::sendJsonMessage(const OUString& rsJsonMessage, 
                                     const std::map<OUString, OUString>& aHeaders)
{
    WebSocketMessage aMessage(MessageType::TEXT, rsJsonMessage);
    aMessage.aHeaders = createProtocolHeaders(MessageType::TEXT);
    
    // Add custom headers
    for (const auto& rHeader : aHeaders)
    {
        aMessage.aHeaders[rHeader.first] = rHeader.second;
    }
    
    aMessage.aHeaders["Content-Type"] = "application/json";
    
    std::unique_lock<std::mutex> aLock(m_aMutex);
    
    if (m_eConnectionState != ConnectionState::CONNECTED)
    {
        logDebug("Cannot send JSON message - not connected");
        return false;
    }
    
    m_aOutgoingMessages.push(aMessage);
    aLock.unlock();
    
    m_aMessageCV.notify_one();
    
    ++m_aMetrics.nMessagesSent;
    logDebug("JSON message queued for sending");
    return true;
}

bool WebSocketClient::requestProgress(const OUString& rsRequestId)
{
    OUString sProgressRequest = R"({"type": "progress_request", "request_id": ")" + 
                               rsRequestId + R"(", "timestamp": ")" + 
                               OUString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch()).count()) + R"("})";
    
    return sendMessage(sProgressRequest, MessageType::PROGRESS_UPDATE);
}

bool WebSocketClient::queryAgentStatus(const OUString& rsAgentName)
{
    OUString sStatusQuery = R"({"type": "agent_status_query", "agent_name": ")" + 
                           rsAgentName + R"(", "timestamp": ")" + 
                           OUString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch()).count()) + R"("})";
    
    return sendMessage(sStatusQuery, MessageType::AGENT_STATUS);
}

void WebSocketClient::setMessageCallback(const MessageCallback& aCallback)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aMessageCallback = aCallback;
}

void WebSocketClient::setConnectionCallback(const ConnectionCallback& aCallback)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aConnectionCallback = aCallback;
}

void WebSocketClient::setErrorCallback(const ErrorCallback& aCallback)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aErrorCallback = aCallback;
}

void WebSocketClient::setAutoReconnect(bool bEnable, sal_Int32 nMaxAttempts, sal_Int32 nDelayMs)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_bAutoReconnect = bEnable;
    m_nMaxReconnectAttempts = nMaxAttempts;
    m_nReconnectDelayMs = nDelayMs;
}

void WebSocketClient::setHeartbeatInterval(sal_Int32 nIntervalMs)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_nHeartbeatIntervalMs = nIntervalMs;
}

WebSocketClient::ConnectionMetrics WebSocketClient::getConnectionMetrics() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_aMetrics;
}

void WebSocketClient::clearMetrics()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aMetrics = ConnectionMetrics();
}

void WebSocketClient::setLoggingEnabled(bool bEnable)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_bEnableLogging = bEnable;
}

// Private implementation methods

void WebSocketClient::connectionThreadMain()
{
    logDebug("Connection thread started");
    
    while (!m_bShutdownRequested)
    {
        std::unique_lock<std::mutex> aLock(m_aMutex);
        
        if (m_eConnectionState == ConnectionState::CONNECTING)
        {
            aLock.unlock();
            
            if (attemptConnection())
            {
                updateConnectionState(ConnectionState::CONNECTED, "Connection established");
                m_aMetrics.aConnectedTime = std::chrono::steady_clock::now();
                m_aLastHeartbeat = std::chrono::steady_clock::now();
            }
            else
            {
                if (m_bAutoReconnect && m_nCurrentReconnectAttempt < m_nMaxReconnectAttempts)
                {
                    handleReconnection();
                }
                else
                {
                    updateConnectionState(ConnectionState::FAILED, "Connection failed - max attempts reached");
                }
            }
        }
        else if (m_eConnectionState == ConnectionState::CONNECTED)
        {
            // Send periodic heartbeat
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_aLastHeartbeat).count() 
                >= m_nHeartbeatIntervalMs)
            {
                aLock.unlock();
                sendHeartbeat();
                m_aLastHeartbeat = now;
            }
            else
            {
                // Wait for next heartbeat or state change
                m_aConnectionCV.wait_for(aLock, std::chrono::milliseconds(1000));
            }
        }
        else
        {
            // Wait for state change
            m_aConnectionCV.wait(aLock);
        }
    }
    
    logDebug("Connection thread ended");
}

void WebSocketClient::messageProcessingThreadMain()
{
    logDebug("Message processing thread started");
    
    while (!m_bShutdownRequested)
    {
        std::unique_lock<std::mutex> aLock(m_aMutex);
        
        // Process outgoing messages
        while (!m_aOutgoingMessages.empty())
        {
            WebSocketMessage aMessage = m_aOutgoingMessages.front();
            m_aOutgoingMessages.pop();
            
            aLock.unlock();
            sendOutgoingMessage(aMessage);
            aLock.lock();
        }
        
        // Process incoming messages
        while (!m_aIncomingMessages.empty())
        {
            WebSocketMessage aMessage = m_aIncomingMessages.front();
            m_aIncomingMessages.pop();
            
            aLock.unlock();
            processIncomingMessage(aMessage);
            ++m_aMetrics.nMessagesReceived;
            m_aMetrics.aLastMessageTime = std::chrono::steady_clock::now();
            aLock.lock();
        }
        
        // Wait for new messages or shutdown
        m_aMessageCV.wait_for(aLock, std::chrono::milliseconds(100));
    }
    
    logDebug("Message processing thread ended");
}

bool WebSocketClient::attemptConnection()
{
    // NOTE: LibreOffice doesn't have native WebSocket support in C++
    // In a production implementation, this would either:
    // 1. Use a third-party WebSocket library (like libwebsockets)
    // 2. Implement WebSocket protocol over TCP sockets
    // 3. Bridge to Java WebSocket implementation
    // 4. Use HTTP/2 Server-Sent Events as fallback
    
    // For now, simulate connection attempt for development
    logDebug("Attempting WebSocket connection to: " + m_sWebSocketUrl);
    
    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Check URL for mock success/failure
    if (m_sWebSocketUrl.indexOf("localhost") >= 0 || m_sWebSocketUrl.indexOf("127.0.0.1") >= 0)
    {
        // Simulate successful connection to local development server
        logDebug("WebSocket connection simulated successfully");
        return true;
    }
    else
    {
        // Simulate connection failure to unknown hosts
        handleWebSocketError("WebSocket connection failed - host unreachable", 1001);
        return false;
    }
}

void WebSocketClient::handleReconnection()
{
    ++m_nCurrentReconnectAttempt;
    ++m_aMetrics.nReconnectCount;
    
    updateConnectionState(ConnectionState::RECONNECTING, 
                         "Reconnection attempt " + OUString::number(m_nCurrentReconnectAttempt));
    
    // Wait before reconnection attempt
    std::this_thread::sleep_for(std::chrono::milliseconds(m_nReconnectDelayMs));
    
    updateConnectionState(ConnectionState::CONNECTING, "Retrying connection");
}

void WebSocketClient::processIncomingMessage(const WebSocketMessage& aMessage)
{
    logDebug("Processing incoming message of type: " + OUString::number(static_cast<int>(aMessage.eType)));
    
    try
    {
        // Call user-provided message callback
        if (m_aMessageCallback)
        {
            m_aMessageCallback(aMessage);
        }
        
        // Handle special message types
        switch (aMessage.eType)
        {
            case MessageType::HEARTBEAT:
                // Update last heartbeat time
                m_aLastHeartbeat = std::chrono::steady_clock::now();
                break;
                
            case MessageType::ERROR_NOTIFICATION:
                if (m_aErrorCallback)
                {
                    m_aErrorCallback(aMessage.sContent, 0);
                }
                break;
                
            default:
                // Regular message processing handled by callback
                break;
        }
    }
    catch (const std::exception& e)
    {
        handleWebSocketError("Error processing message: " + OUString::createFromAscii(e.what()), 500);
    }
}

bool WebSocketClient::sendOutgoingMessage(const WebSocketMessage& aMessage)
{
    if (m_eConnectionState != ConnectionState::CONNECTED)
    {
        logDebug("Cannot send message - not connected");
        return false;
    }
    
    try
    {
        // Format message for WebSocket transmission
        OUString sFormattedMessage = formatMessageForTransmission(aMessage);
        
        // NOTE: In a real implementation, this would send over actual WebSocket connection
        // For development, we log the message
        logDebug("Sending WebSocket message: " + sFormattedMessage.copy(0, std::min(sFormattedMessage.getLength(), 100)) + "...");
        
        // Simulate successful send
        return true;
    }
    catch (const std::exception& e)
    {
        handleWebSocketError("Failed to send message: " + OUString::createFromAscii(e.what()), 1002);
        return false;
    }
}

void WebSocketClient::parseConfiguration(const Sequence<PropertyValue>& rConfig)
{
    for (const auto& rProperty : rConfig)
    {
        if (rProperty.Name == "AutoReconnect")
        {
            rProperty.Value >>= m_bAutoReconnect;
        }
        else if (rProperty.Name == "MaxReconnectAttempts")
        {
            rProperty.Value >>= m_nMaxReconnectAttempts;
        }
        else if (rProperty.Name == "ReconnectDelayMs")
        {
            rProperty.Value >>= m_nReconnectDelayMs;
        }
        else if (rProperty.Name == "HeartbeatIntervalMs")
        {
            rProperty.Value >>= m_nHeartbeatIntervalMs;
        }
        else if (rProperty.Name == "EnableLogging")
        {
            rProperty.Value >>= m_bEnableLogging;
        }
        else if (rProperty.Name == "Protocol")
        {
            rProperty.Value >>= m_sProtocol;
        }
        else
        {
            SAL_WARN("sw.ai", "Unknown WebSocket configuration property: " << rProperty.Name);
        }
    }
}

bool WebSocketClient::validateWebSocketUrl(const OUString& rsUrl)
{
    if (rsUrl.isEmpty())
        return false;
    
    if (!rsUrl.startsWithIgnoreAsciiCase("ws://") && 
        !rsUrl.startsWithIgnoreAsciiCase("wss://"))
    {
        return false;
    }
    
    // Basic URL validation - in production would be more thorough
    return true;
}

void WebSocketClient::updateConnectionState(ConnectionState eNewState, const OUString& rsMessage)
{
    ConnectionState eOldState = m_eConnectionState;
    m_eConnectionState = eNewState;
    
    if (eOldState != eNewState)
    {
        logDebug("Connection state changed from " + OUString::number(static_cast<int>(eOldState)) + 
                " to " + OUString::number(static_cast<int>(eNewState)) + ": " + rsMessage);
        
        if (m_aConnectionCallback)
        {
            m_aConnectionCallback(eNewState, rsMessage);
        }
    }
}

void WebSocketClient::handleWebSocketError(const OUString& rsError, sal_Int32 nErrorCode)
{
    ++m_aMetrics.nErrorCount;
    
    SAL_WARN("sw.ai", "WebSocket error (" << nErrorCode << "): " << rsError);
    
    if (m_aErrorCallback)
    {
        m_aErrorCallback(rsError, nErrorCode);
    }
    
    // Consider reconnection on certain errors
    if (nErrorCode == 1001 || nErrorCode == 1006) // Connection closed or abnormal closure
    {
        if (m_bAutoReconnect && m_eConnectionState == ConnectionState::CONNECTED)
        {
            updateConnectionState(ConnectionState::RECONNECTING, "Auto-reconnect after error");
            m_aConnectionCV.notify_one();
        }
    }
}

void WebSocketClient::sendHeartbeat()
{
    OUString sHeartbeat = R"({"type": "heartbeat", "timestamp": ")" + 
                         OUString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now().time_since_epoch()).count()) + R"("})";
    
    sendMessage(sHeartbeat, MessageType::HEARTBEAT);
}

void WebSocketClient::logDebug(const OUString& rsMessage) const
{
    if (m_bEnableLogging)
    {
        SAL_INFO("sw.ai", "WebSocketClient: " << rsMessage);
    }
}

std::map<OUString, OUString> WebSocketClient::createProtocolHeaders(MessageType eType) const
{
    std::map<OUString, OUString> aHeaders;
    
    aHeaders["X-LangGraph-Protocol"] = m_sProtocol;
    aHeaders["X-Message-Type"] = OUString::number(static_cast<int>(eType));
    aHeaders["X-Timestamp"] = OUString::number(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    
    return aHeaders;
}

OUString WebSocketClient::formatMessageForTransmission(const WebSocketMessage& aMessage) const
{
    OUStringBuffer aBuffer;
    
    // Add headers
    for (const auto& rHeader : aMessage.aHeaders)
    {
        aBuffer.append(rHeader.first + ": " + rHeader.second + "\r\n");
    }
    
    aBuffer.append("\r\n");
    aBuffer.append(aMessage.sContent);
    
    return aBuffer.makeStringAndClear();
}

WebSocketClient::WebSocketMessage WebSocketClient::parseIncomingMessage(const OUString& rsRawMessage) const
{
    // Simple message parsing - in production would be more robust
    sal_Int32 nContentStart = rsRawMessage.indexOf("\r\n\r\n");
    if (nContentStart < 0)
    {
        // No headers, treat entire message as content
        return WebSocketMessage(MessageType::TEXT, rsRawMessage);
    }
    
    OUString sContent = rsRawMessage.copy(nContentStart + 4);
    return WebSocketMessage(MessageType::TEXT, sContent);
}

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */