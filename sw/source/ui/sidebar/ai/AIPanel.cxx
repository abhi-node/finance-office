/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "AIPanel.hxx"

#include <vcl/weld.hxx>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/text/XText.hpp>
#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <rtl/uuid.h>
#include <rtl/ustrbuf.hxx>

using namespace css;

namespace sw::sidebar {

std::unique_ptr<PanelLayout> AIPanel::Create(
    weld::Widget* pParent,
    const uno::Reference<frame::XFrame>& rxFrame)
{
    return std::make_unique<AIPanel>(pParent, rxFrame);
}

AIPanel::AIPanel(weld::Widget* pParent,
                 const uno::Reference<frame::XFrame>& rxFrame)
    : PanelLayout(pParent, "AIPanel", "modules/swriter/ui/aipanel.ui")
    , m_xFrame(rxFrame)
    , m_eConnectionState(ConnectionState::DISCONNECTED)
    , m_nReconnectionAttempts(0)
    , m_nMaxReconnectionAttempts(3)
    , m_bProcessingActive(false)
    , m_bCancellationRequested(false)
    , m_nMaxQueueSize(100)
    , m_nMaxMessageLength(10000)
    , m_nLastUserMessageId(-1)
    , m_bAnimationActive(false)
    , m_aLastAnimationUpdate(std::chrono::steady_clock::now())
    , m_nAnimationIntervalMs(500) // Update every 500ms
    , m_bOperationCancellable(false)
    , m_aOperationStartTime(std::chrono::steady_clock::now())
    , m_nOperationTimeoutMs(60000) // 60 second default timeout
{
    InitializeUI();
    initializeAgentCoordinator();
}

AIPanel::~AIPanel() = default;

void AIPanel::InitializeUI()
{
    // Get widgets from the UI file
    m_xChatHistoryView = m_xBuilder->weld_text_view("chat_history_view");
    m_xTextInput = m_xBuilder->weld_text_view("text_input");
    m_xSendButton = m_xBuilder->weld_button("send_button");
    m_xCancelButton = m_xBuilder->weld_button("cancel_button");
    
    if (m_xChatHistoryView && m_xTextInput && m_xSendButton)
    {
        // Initialize cancel button (initially hidden)  
        if (m_xCancelButton)
        {
            m_xCancelButton->set_visible(false);
            m_xCancelButton->connect_clicked(LINK(this, AIPanel, OnCancelButtonClick));
        }
        // Initialize chat history widget
        if (!m_xChatHistory)
        {
            m_xChatHistory = std::make_unique<ChatHistoryWidget>(
                std::unique_ptr<weld::TextView>(m_xChatHistoryView.get()));
        }
        
        // Initialize text input
        if (!m_xAITextInput)
        {
            m_xAITextInput = std::make_unique<AITextInput>(
                std::unique_ptr<weld::TextView>(m_xTextInput.get()));
                
            // Set up send callback
            m_xAITextInput->SetSendCallback([this]() {
                OnSendMessage();
            });
        }
        
        // Connect send button
        m_xSendButton->connect_clicked(LINK(this, AIPanel, OnSendButtonClick));
        
        // Set initial welcome message
        if (m_xChatHistory)
        {
            m_xChatHistory->AddAIMessage("Hello! I'm your AI Writing Assistant. How can I help you with your document today?");
        }
    }
}

void AIPanel::OnSendMessage()
{
    if (m_xAITextInput && m_xChatHistory)
    {
        OUString sMessage = m_xAITextInput->GetText();
        if (!sMessage.isEmpty())
        {
            // Validate and sanitize message
            if (!validateMessage(sMessage))
            {
                SAL_WARN("sw.ai", "Invalid message rejected");
                return;
            }
            
            OUString sSanitizedMessage = sMessage;
            sanitizeMessage(sSanitizedMessage);
            
            // Add user message to chat history with queued status
            sal_Int32 nMessageId = m_xChatHistory->AddUserMessage(sSanitizedMessage);
            
            // Update message status to sending
            m_xChatHistory->UpdateMessageStatus(nMessageId, MessageStatus::SENDING);
            
            // Clear input field
            m_xAITextInput->SetText("");
            
            // Store message ID for potential retry
            m_nLastUserMessageId = nMessageId;
            
            // Queue message for processing with message ID
            queueMessage(sSanitizedMessage, nMessageId);
            
            // Start processing if not already active
            if (!m_bProcessingActive)
            {
                startBackgroundProcessing();
            }
        }
    }
}

IMPL_LINK_NOARG(AIPanel, OnSendButtonClick, weld::Button&, void)
{
    OnSendMessage();
}

// Message ID generation
OUString AIPanel::QueuedMessage::generateMessageId()
{
    sal_uInt8 aBuffer[16];
    rtl_createUuid(aBuffer, nullptr, false);
    
    OUStringBuffer aBuf(36);
    for (int i = 0; i < 16; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            aBuf.append('-');
        aBuf.append(static_cast<sal_Unicode>('a' + (aBuffer[i] >> 4)));
        aBuf.append(static_cast<sal_Unicode>('a' + (aBuffer[i] & 0x0F)));
    }
    
    return aBuf.makeStringAndClear();
}

// Message validation and sanitization
bool AIPanel::validateMessage(const OUString& rsMessage)
{
    // Check message length
    if (rsMessage.getLength() > m_nMaxMessageLength)
    {
        SAL_WARN("sw.ai", "Message too long: " << rsMessage.getLength() << " characters");
        return false;
    }
    
    // Check for empty or whitespace-only messages
    if (rsMessage.trim().isEmpty())
    {
        return false;
    }
    
    // Basic content validation (no malicious patterns)
    if (rsMessage.indexOf("<script") >= 0 || rsMessage.indexOf("javascript:") >= 0)
    {
        SAL_WARN("sw.ai", "Message contains potentially malicious content");
        return false;
    }
    
    return true;
}

void AIPanel::sanitizeMessage(OUString& rsMessage)
{
    // Trim whitespace
    rsMessage = rsMessage.trim();
    
    // Remove excessive whitespace
    rsMessage = rsMessage.replaceAll("  ", " ");
    
    // Basic HTML escaping if needed
    rsMessage = rsMessage.replaceAll("<", "&lt;");
    rsMessage = rsMessage.replaceAll(">", "&gt;");
}

void AIPanel::queueMessage(const OUString& rsMessage, sal_Int32 nChatMessageId)
{
    std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
    
    // Check queue size limit
    if (static_cast<sal_Int32>(m_aMessageQueue.size()) >= m_nMaxQueueSize)
    {
        SAL_WARN("sw.ai", "Message queue full, dropping oldest message");
        m_aMessageQueue.pop();
    }
    
    // Create and queue message
    QueuedMessage aMessage(rsMessage);
    aMessage.nChatMessageId = nChatMessageId;  // Set the chat message ID
    m_aActiveMessages[aMessage.sMessageId] = aMessage;
    m_aMessageQueue.push(aMessage);
    
    SAL_INFO("sw.ai", "Message queued: " << aMessage.sMessageId);
}

void AIPanel::updateMessageState(const OUString& rsMessageId, MessageState eNewState)
{
    std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
    
    auto it = m_aActiveMessages.find(rsMessageId);
    if (it != m_aActiveMessages.end())
    {
        it->second.eState = eNewState;
        SAL_INFO("sw.ai", "Message state updated: " << rsMessageId << " -> " << static_cast<int>(eNewState));
    }
}

bool AIPanel::initializeAgentCoordinator()
{
    updateConnectionState(ConnectionState::CONNECTING);
    
    try
    {
        // Get the service manager
        uno::Reference<lang::XMultiServiceFactory> xServiceManager = 
            comphelper::getProcessServiceFactory();
        
        if (!xServiceManager.is())
        {
            SAL_WARN("sw.ai", "Failed to get service manager");
            updateConnectionState(ConnectionState::FAILED);
            return false;
        }
        
        // Create AgentCoordinator service
        m_xAgentCoordinator = uno::Reference<ai::XAIAgentCoordinator>(
            xServiceManager->createInstance("com.sun.star.ai.AIAgentCoordinator"),
            uno::UNO_QUERY);
        
        if (!m_xAgentCoordinator.is())
        {
            SAL_WARN("sw.ai", "Failed to create AgentCoordinator service");
            updateConnectionState(ConnectionState::FAILED);
            return false;
        }
        
        // Test the connection
        if (testConnection())
        {
            updateConnectionState(ConnectionState::CONNECTED);
            m_nReconnectionAttempts = 0;
            SAL_INFO("sw.ai", "AgentCoordinator initialized and connected successfully");
            return true;
        }
        else
        {
            updateConnectionState(ConnectionState::FAILED);
            return false;
        }
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("sw.ai", "Exception initializing AgentCoordinator: " << e.Message);
        updateConnectionState(ConnectionState::FAILED);
        return false;
    }
}

css::uno::Any AIPanel::prepareDocumentContext()
{
    try
    {
        if (!m_xFrame.is())
            return uno::Any();
        
        // Get document reference
        uno::Reference<frame::XController> xController = m_xFrame->getController();
        if (!xController.is())
            return uno::Any();
            
        uno::Reference<text::XTextDocument> xTextDoc(xController->getModel(), uno::UNO_QUERY);
        if (!xTextDoc.is())
            return uno::Any();
        
        // Create comprehensive document context
        uno::Sequence<beans::PropertyValue> aContext(4);
        beans::PropertyValue* pContext = aContext.getArray();
        
        // Document reference
        pContext[0].Name = "Document";
        pContext[0].Value = uno::Any(xTextDoc);
        
        // Frame reference for more context
        pContext[1].Name = "Frame";
        pContext[1].Value = uno::Any(m_xFrame);
        
        // Current timestamp
        pContext[2].Name = "Timestamp";
        pContext[2].Value = uno::Any(sal_Int64(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));
        
        // User preferences (placeholder)
        pContext[3].Name = "UserPreferences";
        pContext[3].Value = uno::Any(OUString("default"));
        
        return uno::Any(aContext);
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("sw.ai", "Exception preparing document context: " << e.Message);
        return uno::Any();
    }
}

void AIPanel::processMessageQueue()
{
    std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
    
    while (!m_aMessageQueue.empty() && !isProcessingCancelled())
    {
        QueuedMessage aMessage = m_aMessageQueue.front();
        m_aMessageQueue.pop();
        
        // Update message state to processing
        updateMessageState(aMessage.sMessageId, MessageState::PROCESSING);
        
        // Show processing indicator
        if (m_xChatHistory)
        {
            m_xChatHistory->UpdateMessageStatus(aMessage.nChatMessageId, MessageStatus::PROCESSING, "Processing request...");
            showProcessingIndicator("Processing your request");
        }
        
        // Send to backend (this will be done in a separate thread)
        sendMessageToBackend(aMessage);
    }
}

void AIPanel::sendMessageToBackend(const QueuedMessage& rMessage)
{
    // Check connection state
    if (!isConnected())
    {
        if (m_eConnectionState.load() == ConnectionState::FAILED)
        {
            // Attempt reconnection
            attemptReconnection();
            if (!isConnected())
            {
                // Update chat history first
                if (m_xChatHistory)
                {
                    m_xChatHistory->UpdateMessageStatus(rMessage.nChatMessageId, MessageStatus::ERROR, "Cannot connect to AI service");
                }
                handleBackendError(rMessage.sMessageId, "Cannot connect to AI service");
                return;
            }
        }
        else
        {
            // Update chat history first
            if (m_xChatHistory)
            {
                m_xChatHistory->UpdateMessageStatus(rMessage.nChatMessageId, MessageStatus::ERROR, "Not connected to AI service");
            }
            handleBackendError(rMessage.sMessageId, "AI service not connected");
            return;
        }
    }
    
    if (!m_xAgentCoordinator.is())
    {
        // Update chat history first
        if (m_xChatHistory)
        {
            m_xChatHistory->UpdateMessageStatus(rMessage.nChatMessageId, MessageStatus::ERROR, "AgentCoordinator not available");
        }
        handleBackendError(rMessage.sMessageId, "AgentCoordinator not available");
        return;
    }
    
    try
    {
        // Prepare document context
        uno::Any aDocumentContext = prepareDocumentContext();
        
        // Send message to AgentCoordinator
        updateMessageState(rMessage.sMessageId, MessageState::SENT);
        
        // Show typing indicator
        if (m_xChatHistory)
        {
            m_xChatHistory->UpdateMessageStatus(rMessage.nChatMessageId, MessageStatus::PROCESSING, "Waiting for AI response...");
            showTypingIndicator();
        }
        
        // Call AgentCoordinator (this should be async in a real implementation)
        OUString sResponse = m_xAgentCoordinator->processUserRequest(
            rMessage.sContent, aDocumentContext);
        
        // Handle response
        handleBackendResponse(rMessage.sMessageId, sResponse);
    }
    catch (const uno::Exception& e)
    {
        // Connection-related errors should trigger reconnection
        OUString sErrorMsg = e.Message;
        if (sErrorMsg.indexOf("connection") >= 0 || sErrorMsg.indexOf("timeout") >= 0)
        {
            updateConnectionState(ConnectionState::DISCONNECTED);
            attemptReconnection();
        }
        
        handleBackendError(rMessage.sMessageId, sErrorMsg);
    }
}

void AIPanel::handleBackendResponse(const OUString& rsMessageId, const OUString& rsResponse)
{
    updateMessageState(rsMessageId, MessageState::DELIVERED);
    
    // Find the original message to update its status
    sal_Int32 nChatMessageId = -1;
    {
        std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
        auto it = m_aActiveMessages.find(rsMessageId);
        if (it != m_aActiveMessages.end())
        {
            nChatMessageId = it->second.nChatMessageId;
        }
    }
    
    // Hide loading indicators
    hideTypingIndicator();
    
    // Update original message status to delivered
    if (m_xChatHistory && nChatMessageId > 0)
    {
        m_xChatHistory->UpdateMessageStatus(nChatMessageId, MessageStatus::DELIVERED, "Response received");
    }
    
    // Add AI response to chat history (must be done on main thread)
    if (m_xChatHistory)
    {
        m_xChatHistory->AddAIMessage(rsResponse);
    }
    
    // Remove from active messages
    std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
    m_aActiveMessages.erase(rsMessageId);
    
    SAL_INFO("sw.ai", "Backend response handled for message: " << rsMessageId);
}

void AIPanel::handleBackendError(const OUString& rsMessageId, const OUString& rsError)
{
    updateMessageState(rsMessageId, MessageState::ERROR);
    
    // Find the original message to update its status
    sal_Int32 nChatMessageId = -1;
    {
        std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
        auto it = m_aActiveMessages.find(rsMessageId);
        if (it != m_aActiveMessages.end())
        {
            nChatMessageId = it->second.nChatMessageId;
        }
    }
    
    // Hide loading indicators
    hideTypingIndicator();
    
    // Update original message status to error
    if (m_xChatHistory && nChatMessageId > 0)
    {
        m_xChatHistory->UpdateMessageStatus(nChatMessageId, MessageStatus::ERROR, rsError);
    }
    
    // Add error message with retry option to chat history
    if (m_xChatHistory)
    {
        OUString sErrorMsg = "Error processing your request: " + rsError;
        m_xChatHistory->AddErrorMessage(sErrorMsg, nChatMessageId);
    }
    
    // Remove from active messages
    std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
    m_aActiveMessages.erase(rsMessageId);
    
    SAL_WARN("sw.ai", "Backend error for message " << rsMessageId << ": " << rsError);
}

void AIPanel::startBackgroundProcessing()
{
    m_bProcessingActive = true;
    m_bCancellationRequested = false;
    
    // For now, process synchronously
    // TODO: Implement proper background threading
    processMessageQueue();
    
    m_bProcessingActive = false;
}

void AIPanel::stopBackgroundProcessing()
{
    m_bCancellationRequested = true;
    m_bProcessingActive = false;
}

bool AIPanel::isProcessingCancelled() const
{
    return m_bCancellationRequested.load();
}

// Connection state management methods
void AIPanel::updateConnectionState(ConnectionState eNewState)
{
    ConnectionState eOldState = m_eConnectionState.load();
    m_eConnectionState = eNewState;
    
    SAL_INFO("sw.ai", "Connection state changed from " << static_cast<int>(eOldState) << 
             " to " << static_cast<int>(eNewState));
    
    // Update UI based on connection state (placeholder)
    if (m_xChatHistory)
    {
        switch (eNewState)
        {
            case ConnectionState::CONNECTING:
                // Will be implemented in Task 7.4 (loading indicators)
                break;
            case ConnectionState::CONNECTED:
                // Connection successful - no need to show status
                break;
            case ConnectionState::FAILED:
                m_xChatHistory->AddAIMessage("Connection to AI service failed. Please check your connection and try again.");
                break;
            case ConnectionState::RECONNECTING:
                m_xChatHistory->AddAIMessage("Connection lost. Attempting to reconnect...");
                break;
            default:
                break;
        }
    }
}

bool AIPanel::isConnected() const
{
    return m_eConnectionState.load() == ConnectionState::CONNECTED;
}

void AIPanel::attemptReconnection()
{
    if (m_nReconnectionAttempts >= m_nMaxReconnectionAttempts)
    {
        SAL_WARN("sw.ai", "Max reconnection attempts reached");
        updateConnectionState(ConnectionState::FAILED);
        return;
    }
    
    ++m_nReconnectionAttempts;
    updateConnectionState(ConnectionState::RECONNECTING);
    
    SAL_INFO("sw.ai", "Attempting reconnection " << m_nReconnectionAttempts << "/" << m_nMaxReconnectionAttempts);
    
    if (initializeAgentCoordinator())
    {
        SAL_INFO("sw.ai", "Reconnection successful");
    }
    else
    {
        SAL_WARN("sw.ai", "Reconnection attempt " << m_nReconnectionAttempts << " failed");
        if (m_nReconnectionAttempts < m_nMaxReconnectionAttempts)
        {
            // Try again after a delay (in a real implementation, this would be scheduled)
            attemptReconnection();
        }
        else
        {
            handleConnectionFailure();
        }
    }
}

void AIPanel::handleConnectionFailure()
{
    updateConnectionState(ConnectionState::FAILED);
    m_nReconnectionAttempts = 0;
    
    if (m_xChatHistory)
    {
        m_xChatHistory->AddAIMessage("Unable to connect to AI service after multiple attempts. Please try again later.");
    }
    
    SAL_WARN("sw.ai", "Connection failure - all reconnection attempts exhausted");
}

bool AIPanel::testConnection()
{
    if (!m_xAgentCoordinator.is())
        return false;
    
    try
    {
        // Test connection by checking if service is online
        // For now, we'll assume the service is available if we can create it
        return m_xAgentCoordinator->isOnline();
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("sw.ai", "Connection test failed: " << e.Message);
        return false;
    }
}

void AIPanel::AddTestMessage()
{
    // Test function to verify the panel works
    // Adds test messages to demonstrate chat history functionality
    
    if (m_xChatHistory)
    {
        m_xChatHistory->AddUserMessage("Hello AI, can you help me format this document?");
        m_xChatHistory->AddAIMessage("I'd be happy to help you format your document. What specific formatting would you like to apply?");
        m_xChatHistory->AddUserMessage("I need to create a professional report with proper headings.");
        m_xChatHistory->AddAIMessage("I can help you create a professional report. I'll apply heading styles and proper formatting to your document.");
    }
}

void AIPanel::startAnimationTimer()
{
    m_bAnimationActive = true;
    m_aLastAnimationUpdate = std::chrono::steady_clock::now();
}

void AIPanel::stopAnimationTimer()
{
    m_bAnimationActive = false;
    if (m_xChatHistory)
    {
        m_xChatHistory->HideLoadingIndicators();
    }
}

void AIPanel::updateLoadingAnimations()
{
    if (!m_bAnimationActive || !m_xChatHistory)
        return;
        
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_aLastAnimationUpdate);
    
    if (elapsed.count() >= static_cast<long>(m_nAnimationIntervalMs))
    {
        m_xChatHistory->UpdateTypingAnimation();
        m_aLastAnimationUpdate = now;
    }
}

void AIPanel::showTypingIndicator()
{
    if (m_xChatHistory)
    {
        m_xChatHistory->ShowTypingIndicator(true);
        startAnimationTimer();
    }
}

void AIPanel::hideTypingIndicator()
{
    if (m_xChatHistory)
    {
        m_xChatHistory->ShowTypingIndicator(false);
    }
}

void AIPanel::showProcessingIndicator(const OUString& rMessage)
{
    if (m_xChatHistory)
    {
        m_xChatHistory->ShowLoadingIndicator(rMessage, 30000); // 30 second timeout
        startAnimationTimer();
    }
}

void AIPanel::updateProgress(sal_Int32 nProgress, const OUString& rMessage)
{
    if (m_xChatHistory)
    {
        m_xChatHistory->UpdateProgressIndicator(nProgress, rMessage);
    }
}

void AIPanel::cancelCurrentOperation()
{
    std::lock_guard<std::mutex> aCancelGuard(m_aCancellationMutex);
    
    if (!m_bOperationCancellable)
        return;
        
    // Set cancellation flag
    m_bCancellationRequested = true;
    
    SAL_INFO("sw.ai", "User initiated cancellation of operation: " << m_sCurrentOperationId);
    
    // Update UI immediately
    if (m_xChatHistory)
    {
        m_xChatHistory->AddAIMessage("Operation cancelled by user.");
        m_xChatHistory->HideLoadingIndicators();
    }
    
    // Hide cancel button
    showCancelButton(false);
    
    // Clear operation state
    m_bOperationCancellable = false;
    m_sCurrentOperationId.clear();
    
    // Stop processing
    stopBackgroundProcessing();
}

void AIPanel::cancelAllOperations()
{
    cancelCurrentOperation();
    
    // Clear message queue
    std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
    while (!m_aMessageQueue.empty())
    {
        m_aMessageQueue.pop();
    }
    m_aActiveMessages.clear();
    
    SAL_INFO("sw.ai", "All operations cancelled and queue cleared");
}

bool AIPanel::isOperationCancellable() const
{
    std::lock_guard<std::mutex> aCancelGuard(const_cast<std::mutex&>(m_aCancellationMutex));
    return m_bOperationCancellable;
}

void AIPanel::showCancelButton(bool bShow)
{
    if (m_xCancelButton)
    {
        m_xCancelButton->set_visible(bShow);
        m_xCancelButton->set_sensitive(bShow);
    }
}

IMPL_LINK_NOARG(AIPanel, OnCancelButtonClick, weld::Button&, void)
{
    cancelCurrentOperation();
}

void AIPanel::handleEscapeKey()
{
    // Handle escape key for cancellation
    if (isOperationCancellable())
    {
        cancelCurrentOperation();
    }
}

void AIPanel::handleOperationCancellation(const OUString& rsMessageId, const OUString& rsReason)
{
    updateMessageState(rsMessageId, MessageState::CANCELLED);
    
    // Find the original message to update its status
    sal_Int32 nChatMessageId = -1;
    {
        std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
        auto it = m_aActiveMessages.find(rsMessageId);
        if (it != m_aActiveMessages.end())
        {
            nChatMessageId = it->second.nChatMessageId;
        }
    }
    
    // Update chat history
    if (m_xChatHistory && nChatMessageId > 0)
    {
        m_xChatHistory->UpdateMessageStatus(nChatMessageId, MessageStatus::ERROR, "Cancelled: " + rsReason);
    }
    
    // Hide loading indicators
    hideTypingIndicator();
    showCancelButton(false);
    
    // Clear cancellation state
    {
        std::lock_guard<std::mutex> aCancelGuard(m_aCancellationMutex);
        m_bOperationCancellable = false;
        m_sCurrentOperationId.clear();
    }
    
    // Remove from active messages
    std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
    m_aActiveMessages.erase(rsMessageId);
    
    SAL_INFO("sw.ai", "Operation cancelled: " << rsMessageId << " - " << rsReason);
}

OUString AIPanel::generateOperationId()
{
    // Generate a simple operation ID based on timestamp
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "op_" + OUString::number(timestamp);
}

} // end of namespace sw::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */