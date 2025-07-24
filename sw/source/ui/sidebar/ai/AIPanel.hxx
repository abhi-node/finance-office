/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AIPANEL_HXX
#define INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AIPANEL_HXX

#include <sfx2/sidebar/PanelLayout.hxx>
#include <vcl/weld.hxx>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/ai/XAIAgentCoordinator.hpp>
#include <com/sun/star/uno/Reference.hxx>
#include <tools/link.hxx>
#include <comphelper/processfactory.hxx>
#include <memory>
#include <mutex>
#include <queue>
#include <chrono>
#include <map>
#include "ChatHistory.hxx"
#include "AITextInput.hxx"
#include <functional>

namespace sw::sidebar {

/**
 * AI Chat Panel for LibreOffice Writer sidebar
 * 
 * Provides a chat interface for AI-powered document assistance.
 * Uses modern weld-based UI framework.
 */
class AIPanel final : public PanelLayout
{
public:
    /**
     * Constructor - creates AI chat panel with weld-based UI
     * @param pParent Parent widget for the panel
     * @param rxFrame Frame reference for document context
     */
    static std::unique_ptr<PanelLayout> Create(
        weld::Widget* pParent,
        const css::uno::Reference<css::frame::XFrame>& rxFrame);

    /**
     * Constructor
     * @param pParent Parent widget for the panel
     * @param rxFrame Frame reference for document context
     */
    AIPanel(weld::Widget* pParent,
            const css::uno::Reference<css::frame::XFrame>& rxFrame);

    /**
     * Destructor
     */
    virtual ~AIPanel() override;

    /**
     * Add test message to demonstrate functionality
     */
    void AddTestMessage();

    /**
     * Connection states for backend communication
     */
    enum class ConnectionState
    {
        DISCONNECTED,  // Not connected to backend
        CONNECTING,    // Attempting to connect
        CONNECTED,     // Successfully connected
        RECONNECTING,  // Attempting to reconnect after failure
        FAILED         // Connection failed permanently
    };

    /**
     * Message processing states
     */
    enum class MessageState
    {
        PENDING,      // Message queued for processing
        PROCESSING,   // Being processed by AI backend
        SENT,         // Successfully sent to backend
        DELIVERED,    // Response received from backend
        ERROR,        // Error occurred during processing
        CANCELLED     // Operation was cancelled by user
    };

    /**
     * Message wrapper with metadata
     */
    struct QueuedMessage
    {
        OUString sMessageId;
        OUString sContent;
        OUString sUserId;
        MessageState eState;
        std::chrono::steady_clock::time_point aTimestamp;
        sal_Int32 nRetryCount;
        sal_Int32 nChatMessageId;  // Reference to ChatHistory message ID
        
        QueuedMessage() = default;
        
        QueuedMessage(const OUString& rsContent)
            : sContent(rsContent)
            , eState(MessageState::PENDING)
            , aTimestamp(std::chrono::steady_clock::now())
            , nRetryCount(0)
            , nChatMessageId(-1)
        {
            // Generate unique message ID
            sMessageId = generateMessageId();
            sUserId = "user"; // Default user ID
        }
        
    private:
        static OUString generateMessageId();
    };

private:
    /**
     * Initialize the panel UI components
     */
    void InitializeUI();
    
    /**
     * Handle send message from input field
     */
    void OnSendMessage();
    
    /**
     * Handle send button click
     */
    DECL_LINK(OnSendButtonClick, weld::Button&, void);
    
    /**
     * Message event handling and validation
     */
    bool validateMessage(const OUString& rsMessage);
    void sanitizeMessage(OUString& rsMessage);
    void queueMessage(const OUString& rsMessage, sal_Int32 nChatMessageId);
    void processMessageQueue();
    void updateMessageState(const OUString& rsMessageId, MessageState eNewState);
    void handleMessageRetry(sal_Int32 nChatMessageId);
    
    /**
     * AgentCoordinator integration
     */
    bool initializeAgentCoordinator();
    css::uno::Any prepareDocumentContext();
    void sendMessageToBackend(const QueuedMessage& rMessage);
    void handleBackendResponse(const OUString& rsMessageId, const OUString& rsResponse);
    void handleBackendError(const OUString& rsMessageId, const OUString& rsError);
    void resendMessage(sal_Int32 nChatMessageId);
    void handleOperationCancellation(const OUString& rsMessageId, const OUString& rsReason);
    OUString generateOperationId();
    
    /**
     * Connection state management
     */
    void updateConnectionState(ConnectionState eNewState);
    bool isConnected() const;
    void attemptReconnection();
    void handleConnectionFailure();
    bool testConnection();
    
    /**
     * Threading and cancellation support
     */
    void startBackgroundProcessing();
    void stopBackgroundProcessing();
    bool isProcessingCancelled() const;
    
    /**
     * Animation and loading indicator management
     */
    void startAnimationTimer();
    void stopAnimationTimer();
    void updateLoadingAnimations();
    
    /**
     * Show/hide different types of loading indicators
     */
    void showTypingIndicator();
    void hideTypingIndicator();
    void showProcessingIndicator(const OUString& rMessage);
    void updateProgress(sal_Int32 nProgress, const OUString& rMessage);
    
    /**
     * Operation cancellation support
     */
    void cancelCurrentOperation();
    void cancelAllOperations();
    bool isOperationCancellable() const;
    void showCancelButton(bool bShow);
    
    /**
     * Handle UI events for cancellation
     */
    DECL_LINK(OnCancelButtonClick, weld::Button&, void);
    void handleEscapeKey();

    // UI components using weld framework
    std::unique_ptr<ChatHistoryWidget> m_xChatHistory;     ///< Chat history widget
    std::unique_ptr<AITextInput> m_xAITextInput;           ///< AI text input with auto-expansion
    std::unique_ptr<weld::TextView> m_xChatHistoryView;    ///< Chat history text view from UI
    std::unique_ptr<weld::TextView> m_xTextInput;          ///< Text input from UI
    std::unique_ptr<weld::Button> m_xSendButton;           ///< Send button
    std::unique_ptr<weld::Button> m_xCancelButton;         ///< Cancel operation button

    // Frame reference for document context
    css::uno::Reference<css::frame::XFrame> m_xFrame;
    
    // AgentCoordinator integration
    css::uno::Reference<css::ai::XAIAgentCoordinator> m_xAgentCoordinator;
    
    // Message handling
    mutable std::mutex m_aMessageMutex;
    std::queue<QueuedMessage> m_aMessageQueue;
    std::map<OUString, QueuedMessage> m_aActiveMessages;  // Message ID -> Message
    
    // Connection state
    std::atomic<ConnectionState> m_eConnectionState;
    sal_Int32 m_nReconnectionAttempts;
    sal_Int32 m_nMaxReconnectionAttempts;
    
    // Threading and cancellation
    std::atomic<bool> m_bProcessingActive;
    std::atomic<bool> m_bCancellationRequested;
    sal_Int32 m_nMaxQueueSize;
    sal_Int32 m_nMaxMessageLength;
    sal_Int32 m_nLastUserMessageId;  // Track last user message for retry
    
    // Animation and timing
    std::atomic<bool> m_bAnimationActive;
    std::chrono::steady_clock::time_point m_aLastAnimationUpdate;
    sal_uInt32 m_nAnimationIntervalMs;  // Animation update interval
    
    // Operation cancellation and threading
    std::atomic<bool> m_bOperationCancellable;
    OUString m_sCurrentOperationId;               // ID of current cancellable operation
    std::chrono::steady_clock::time_point m_aOperationStartTime;
    sal_uInt32 m_nOperationTimeoutMs;             // Operation timeout in milliseconds
    std::mutex m_aCancellationMutex;              // Protect cancellation state
};

} // end of namespace sw::sidebar

#endif // INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AIPANEL_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */