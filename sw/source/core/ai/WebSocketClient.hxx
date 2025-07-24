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
#include <com/sun/star/io/XStreamListener.hpp>
#include <rtl/ustrbuf.hxx>

#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace sw::ai {

/**
 * WebSocketClient - Real-time WebSocket communication for AI Agent system
 * 
 * This class provides WebSocket communication capabilities for streaming
 * real-time updates from the LangGraph backend during agent processing.
 * Built to integrate seamlessly with LibreOffice's existing network infrastructure.
 * 
 * Design Principles:
 * - Compatible with LibreOffice's threading and event model
 * - Supports streaming progress updates during long-running operations
 * - JSON message format for LangGraph protocol compatibility
 * - Graceful degradation when WebSocket unavailable
 * - Thread-safe operation with callback-based message handling
 */
class WebSocketClient
{
public:
    enum class ConnectionState
    {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTING,
        FAILED
    };

    enum class MessageType
    {
        TEXT,
        BINARY,
        PROGRESS_UPDATE,
        AGENT_STATUS,
        ERROR_NOTIFICATION,
        HEARTBEAT
    };

    struct WebSocketMessage
    {
        MessageType eType;
        OUString sContent;
        std::map<OUString, OUString> aHeaders;
        std::chrono::steady_clock::time_point aTimestamp;
        
        WebSocketMessage(MessageType eT, const OUString& rsContent)
            : eType(eT), sContent(rsContent), aTimestamp(std::chrono::steady_clock::now()) {}
    };

    // Callback function types for message handling
    using MessageCallback = std::function<void(const WebSocketMessage&)>;
    using ConnectionCallback = std::function<void(ConnectionState, const OUString&)>;
    using ErrorCallback = std::function<void(const OUString&, sal_Int32)>;

private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    
    // Connection management
    mutable std::mutex m_aMutex;
    ConnectionState m_eConnectionState;
    OUString m_sWebSocketUrl;
    OUString m_sProtocol;
    sal_Int32 m_nPort;
    bool m_bAutoReconnect;
    sal_Int32 m_nMaxReconnectAttempts;
    sal_Int32 m_nReconnectDelayMs;
    sal_Int32 m_nCurrentReconnectAttempt;
    
    // Threading and async operations
    std::unique_ptr<std::thread> m_pConnectionThread;
    std::unique_ptr<std::thread> m_pMessageProcessingThread;
    bool m_bShutdownRequested;
    std::condition_variable m_aConnectionCV;
    std::condition_variable m_aMessageCV;
    
    // Message handling
    std::queue<WebSocketMessage> m_aIncomingMessages;
    std::queue<WebSocketMessage> m_aOutgoingMessages;
    MessageCallback m_aMessageCallback;
    ConnectionCallback m_aConnectionCallback;
    ErrorCallback m_aErrorCallback;
    
    // Performance and debugging
    struct ConnectionMetrics
    {
        std::chrono::steady_clock::time_point aConnectedTime;
        std::chrono::steady_clock::time_point aLastMessageTime;
        sal_Int32 nMessagesSent;
        sal_Int32 nMessagesReceived;
        sal_Int32 nReconnectCount;
        sal_Int32 nErrorCount;
        
        ConnectionMetrics() : nMessagesSent(0), nMessagesReceived(0), 
                             nReconnectCount(0), nErrorCount(0) {}
    };
    
    ConnectionMetrics m_aMetrics;
    bool m_bEnableLogging;
    sal_Int32 m_nHeartbeatIntervalMs;
    std::chrono::steady_clock::time_point m_aLastHeartbeat;

public:
    explicit WebSocketClient(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    ~WebSocketClient();

    // Connection lifecycle management
    
    /**
     * Initialize WebSocket client with configuration
     * 
     * @param rConfig Configuration parameters including URL, protocols, timeouts
     * @returns true if initialization successful
     */
    bool initialize(const css::uno::Sequence<css::beans::PropertyValue>& rConfig = {});
    
    /**
     * Connect to WebSocket server for real-time communication
     * 
     * @param rsUrl WebSocket URL (ws:// or wss://)
     * @param rsProtocol WebSocket sub-protocol (optional)
     * @returns true if connection initiated successfully
     */
    bool connect(const OUString& rsUrl, const OUString& rsProtocol = OUString());
    
    /**
     * Disconnect from WebSocket server
     */
    void disconnect();
    
    /**
     * Shutdown WebSocket client and cleanup resources
     */
    void shutdown();
    
    /**
     * Check current connection state
     * 
     * @returns Current ConnectionState
     */
    ConnectionState getConnectionState() const;
    
    /**
     * Check if WebSocket is connected and ready for messaging
     * 
     * @returns true if connected and operational
     */
    bool isConnected() const;

    // Message handling
    
    /**
     * Send text message to WebSocket server
     * 
     * @param rsMessage Message content to send
     * @param eType Message type for proper handling
     * @returns true if message queued successfully
     */
    bool sendMessage(const OUString& rsMessage, MessageType eType = MessageType::TEXT);
    
    /**
     * Send JSON message for LangGraph communication
     * 
     * @param rsJsonMessage JSON-formatted message
     * @param aHeaders Additional message headers
     * @returns true if message queued successfully
     */
    bool sendJsonMessage(const OUString& rsJsonMessage, 
                        const std::map<OUString, OUString>& aHeaders = {});
    
    /**
     * Send progress update request to backend
     * 
     * @param rsRequestId Request ID to get progress for
     * @returns true if progress request sent
     */
    bool requestProgress(const OUString& rsRequestId);
    
    /**
     * Send agent status query to backend
     * 
     * @param rsAgentName Agent name to query status for
     * @returns true if status query sent
     */
    bool queryAgentStatus(const OUString& rsAgentName);

    // Callback registration
    
    /**
     * Set callback for incoming messages
     * 
     * @param aCallback Function to call when messages received
     */
    void setMessageCallback(const MessageCallback& aCallback);
    
    /**
     * Set callback for connection state changes
     * 
     * @param aCallback Function to call on connection events
     */
    void setConnectionCallback(const ConnectionCallback& aCallback);
    
    /**
     * Set callback for error handling
     * 
     * @param aCallback Function to call on errors
     */
    void setErrorCallback(const ErrorCallback& aCallback);

    // Configuration and monitoring
    
    /**
     * Enable or disable automatic reconnection
     * 
     * @param bEnable true to enable auto-reconnect
     * @param nMaxAttempts Maximum reconnection attempts
     * @param nDelayMs Delay between reconnection attempts
     */
    void setAutoReconnect(bool bEnable, sal_Int32 nMaxAttempts = 5, sal_Int32 nDelayMs = 2000);
    
    /**
     * Set heartbeat interval for connection monitoring
     * 
     * @param nIntervalMs Heartbeat interval in milliseconds
     */
    void setHeartbeatInterval(sal_Int32 nIntervalMs);
    
    /**
     * Get connection performance metrics
     * 
     * @returns ConnectionMetrics with performance data
     */
    ConnectionMetrics getConnectionMetrics() const;
    
    /**
     * Clear accumulated metrics
     */
    void clearMetrics();
    
    /**
     * Enable or disable debug logging
     * 
     * @param bEnable true to enable logging
     */
    void setLoggingEnabled(bool bEnable);

private:
    // Internal implementation methods
    
    /**
     * Background thread for managing WebSocket connection
     */
    void connectionThreadMain();
    
    /**
     * Background thread for processing incoming messages
     */
    void messageProcessingThreadMain();
    
    /**
     * Attempt WebSocket connection with proper error handling
     * 
     * @returns true if connection successful
     */
    bool attemptConnection();
    
    /**
     * Handle WebSocket reconnection logic
     */
    void handleReconnection();
    
    /**
     * Process incoming WebSocket message
     * 
     * @param aMessage Message to process
     */
    void processIncomingMessage(const WebSocketMessage& aMessage);
    
    /**
     * Send outgoing WebSocket message
     * 
     * @param aMessage Message to send
     * @returns true if sent successfully
     */
    bool sendOutgoingMessage(const WebSocketMessage& aMessage);
    
    /**
     * Parse configuration parameters
     * 
     * @param rConfig Configuration property sequence
     */
    void parseConfiguration(const css::uno::Sequence<css::beans::PropertyValue>& rConfig);
    
    /**
     * Validate WebSocket URL format
     * 
     * @param rsUrl URL to validate
     * @returns true if URL is valid WebSocket format
     */
    bool validateWebSocketUrl(const OUString& rsUrl);
    
    /**
     * Update connection state with proper notifications
     * 
     * @param eNewState New connection state
     * @param rsMessage Optional status message
     */
    void updateConnectionState(ConnectionState eNewState, const OUString& rsMessage = OUString());
    
    /**
     * Handle WebSocket error with logging and callbacks
     * 
     * @param rsError Error description
     * @param nErrorCode Error code
     */
    void handleWebSocketError(const OUString& rsError, sal_Int32 nErrorCode = 0);
    
    /**
     * Send heartbeat message to maintain connection
     */
    void sendHeartbeat();
    
    /**
     * Log debug message if logging enabled
     * 
     * @param rsMessage Message to log
     */
    void logDebug(const OUString& rsMessage) const;
    
    /**
     * Create message headers for LangGraph protocol
     * 
     * @param eType Message type
     * @returns Header map with protocol information
     */
    std::map<OUString, OUString> createProtocolHeaders(MessageType eType) const;
    
    /**
     * Format message content for WebSocket transmission
     * 
     * @param aMessage Message to format
     * @returns Formatted message string
     */
    OUString formatMessageForTransmission(const WebSocketMessage& aMessage) const;
    
    /**
     * Parse incoming WebSocket message content
     * 
     * @param rsRawMessage Raw message from WebSocket
     * @returns Parsed WebSocketMessage
     */
    WebSocketMessage parseIncomingMessage(const OUString& rsRawMessage) const;
};

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */