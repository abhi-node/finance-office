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

#include <cppuhelper/implbase.hxx>
#include <com/sun/star/ai/XAIAgentCoordinator.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/uno/Sequence.hxx>

#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <chrono>

// Forward declarations for scalability
namespace sw::ai {
    class DocumentContext;
    class MessageQueue;
    class NetworkClient;
    class WebSocketClient;
    class AuthenticationManager;
    class ErrorRecoveryManager;
}

#include "DocumentContext.hxx"
#include "MessageQueue.hxx"
#include "AuthenticationManager.hxx"
#include "ErrorRecoveryManager.hxx"

namespace sw::core::ai {

/**
 * AgentCoordinator - Main interface between LibreOffice UI and LangGraph AI backend
 * 
 * This class serves as the primary communication bridge between the LibreOffice
 * UI components (AIPanel.cxx) and the future LangGraph multi-agent system.
 * 
 * Design Principles:
 * - Compatibility: Uses established LibreOffice UNO service patterns
 * - Scalability: Designed for future extension with minimal interface changes
 * - Performance: Asynchronous processing with intelligent routing
 * - Reliability: Comprehensive error handling and offline capability
 */
class AgentCoordinator final : public cppu::WeakImplHelper<
    css::ai::XAIAgentCoordinator,
    css::lang::XServiceInfo>
{
private:
    // Core LibreOffice integration
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    css::uno::Reference<css::frame::XFrame> m_xFrame;
    
    // Thread safety and state management
    mutable std::mutex m_aMutex;
    bool m_bInitialized;
    bool m_bOnlineMode;
    
    // Scalable component architecture - forward-declared for future implementation
    std::unique_ptr<sw::ai::DocumentContext> m_pDocumentContext;
    std::unique_ptr<sw::ai::MessageQueue> m_pMessageQueue;
    std::unique_ptr<sw::ai::NetworkClient> m_pNetworkClient;
    std::unique_ptr<sw::ai::WebSocketClient> m_pWebSocketClient;
    std::unique_ptr<sw::ai::AuthenticationManager> m_pAuthManager;
    std::unique_ptr<sw::ai::ErrorRecoveryManager> m_pErrorRecovery;
    
    // Performance optimization and intelligent routing
    std::chrono::steady_clock::time_point m_aLastActivity;
    sal_Int32 m_nRequestCounter;
    
    // Message processing state
    struct PendingRequest
    {
        OUString sRequestId;
        OUString sMessage;
        css::uno::Any aDocumentContext;
        std::chrono::steady_clock::time_point aTimestamp;
        sal_Int32 nRetryCount;
        bool bHighPriority;
        
        PendingRequest(const OUString& rId, const OUString& rMsg, const css::uno::Any& rContext)
            : sRequestId(rId), sMessage(rMsg), aDocumentContext(rContext)
            , aTimestamp(std::chrono::steady_clock::now())
            , nRetryCount(0), bHighPriority(false) {}
    };
    
    std::queue<PendingRequest> m_aPendingRequests;
    std::queue<PendingRequest> m_aOfflineQueue;
    
    // Configuration and capabilities
    sal_Int32 m_nMaxRetries;
    sal_Int32 m_nTimeoutMs;
    sal_Int32 m_nMaxQueueSize;
    bool m_bEnableWebSocket;
    bool m_bEnableOfflineMode;
    
public:
    explicit AgentCoordinator(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    virtual ~AgentCoordinator() override;
    
    // XAIAgentCoordinator interface - main API for LibreOffice integration
    virtual OUString SAL_CALL processUserRequest(
        const OUString& rsRequest,
        const css::uno::Any& rDocumentContext) override;
        
    virtual void SAL_CALL cancelOperation(sal_Int64 nOperationId) override;
    
    virtual css::uno::Sequence<OUString> SAL_CALL getAvailableAgents() override;
    
    virtual sal_Bool SAL_CALL isOnline() override;
    
    virtual void SAL_CALL setConfiguration(
        const css::uno::Sequence<css::beans::PropertyValue>& rConfig) override;
    
    virtual css::uno::Sequence<css::beans::PropertyValue> SAL_CALL 
        getConfiguration() override;
    
    // XServiceInfo interface - LibreOffice service registration
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& rServiceName) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;
    
    // Lifecycle management
    void SAL_CALL initialize(const css::uno::Reference<css::frame::XFrame>& xFrame);
    void SAL_CALL shutdown();
    
private:
    // Internal processing methods - designed for future extension
    
    /**
     * Analyzes request complexity for intelligent routing
     * Future: Will route simple requests (1-2s) vs complex requests (3-5s)
     */
    enum class RequestComplexity { Simple, Moderate, Complex };
    RequestComplexity analyzeRequestComplexity(const OUString& rsRequest) const;
    
    /**
     * Processes requests based on complexity
     * Future: Simple = direct local processing, Complex = full LangGraph workflow
     */
    OUString processSimpleRequest(const OUString& rsRequest, const css::uno::Any& rContext);
    OUString processModerateRequest(const OUString& rsRequest, const css::uno::Any& rContext);
    OUString processComplexRequest(const OUString& rsRequest, const css::uno::Any& rContext);
    
    /**
     * Document context extraction and management
     * Extracts cursor position, selection, document structure for agent processing
     */
    css::uno::Any extractDocumentContext(const css::uno::Any& rContext) const;
    OUString getCurrentDocumentInfo() const;
    
    /**
     * Network and communication management (Phase 2)
     * Designed for future HTTP/WebSocket integration with LangGraph backend
     */
    bool initializeNetworkClient();
    bool sendToBackend(const OUString& rsMessage, const css::uno::Any& rContext);
    OUString receiveFromBackend(const OUString& rsRequestId);
    
    /**
     * WebSocket communication management (Phase 3)
     * Real-time communication for streaming progress updates
     */
    bool initializeWebSocketClient();
    bool connectWebSocket(const OUString& rsUrl);
    void disconnectWebSocket();
    bool sendWebSocketMessage(const OUString& rsMessage, const OUString& rsRequestId);
    void handleWebSocketMessage(const OUString& rsMessage);
    void handleWebSocketConnectionChange(bool bConnected);
    bool isWebSocketEnabled() const;
    
    /**
     * Error handling and recovery (Phase 4)
     * Comprehensive error management with fallback strategies
     */
    bool initializeErrorRecovery();
    
    /**
     * Message queuing system (Phase 5)
     * Reliable message delivery and offline capabilities
     */
    bool initializeMessageQueue();
    
    /**
     * Authentication management (Phase 5)
     * Secure credential storage and token management
     */
    bool initializeAuthenticationManager();
    void handleNetworkError(const OUString& rsError);
    void handleProcessingError(const OUString& rsRequestId, const OUString& rsError);
    bool attemptErrorRecovery(const PendingRequest& rRequest);
    void reportOperationError(const OUString& rsRequestId, const OUString& rsServiceName, 
                             const OUString& rsError, sal_Int32 nErrorCode = 0);
    bool shouldRetryOperation(const OUString& rsRequestId, const OUString& rsServiceName);
    sal_Int32 calculateRetryDelay(const OUString& rsRequestId, const OUString& rsServiceName);
    void reportOperationSuccess(const OUString& rsRequestId, const OUString& rsServiceName);
    
    /**
     * Offline mode and message queuing
     * Ensures functionality when backend is unavailable
     */
    void enterOfflineMode();
    void exitOfflineMode();
    void processOfflineQueue();
    
    /**
     * Performance optimization
     * Caching and intelligent request batching
     */
    bool isCacheableRequest(const OUString& rsRequest) const;
    OUString getCachedResponse(const OUString& rsRequest) const;
    void cacheResponse(const OUString& rsRequest, const OUString& rsResponse);
    
    /**
     * Configuration management
     * Handles user preferences and system settings
     */
    void loadConfiguration();
    void saveConfiguration();
    css::beans::PropertyValue makeConfigProperty(
        const OUString& rsName, const css::uno::Any& rValue) const;
    
    /**
     * Utility methods
     */
    OUString generateRequestId() const;
    bool isRequestTimedOut(const PendingRequest& rRequest) const;
    void logActivity(const OUString& rsMessage) const;
    
    // Static factory method for UNO service creation
public:
    static css::uno::Reference<css::uno::XInterface> SAL_CALL create(
        const css::uno::Reference<css::uno::XComponentContext>& xContext);
};

} // namespace sw::core::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */