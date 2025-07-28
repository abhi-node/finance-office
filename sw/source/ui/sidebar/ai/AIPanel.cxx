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

// Include AgentCoordinator for callback registration
#include "../../core/ai/AgentCoordinator.hxx"

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

AIPanel::~AIPanel() 
{
    // Unregister callback when panel is destroyed
    sw::core::ai::AgentCoordinator::unregisterChatPanelCallback();
}

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
    SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - User initiated message send");
    
    if (m_xAITextInput && m_xChatHistory)
    {
        OUString sMessage = m_xAITextInput->GetText();
        SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - Raw message length: " << sMessage.getLength());
        
        if (!sMessage.isEmpty())
        {
            // Validate and sanitize message
            if (!validateMessage(sMessage))
            {
                SAL_WARN("sw.ai", "AIPanel::OnSendMessage() - Invalid message rejected: " << sMessage.copy(0, 100));
                return;
            }
            
            OUString sSanitizedMessage = sMessage;
            sanitizeMessage(sSanitizedMessage);
            SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - Message validated and sanitized");
            
            // Add user message to chat history (final - no status updates needed)
            sal_Int32 nMessageId = m_xChatHistory->AddUserMessage(sSanitizedMessage);
            SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - User message added to chat history with ID: " << nMessageId);
            
            // Add "Processing..." message to chat history immediately
            sal_Int32 nProcessingMessageId = m_xChatHistory->AddAIMessage("Processing your request...");
            SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - Processing message added to chat with ID: " << nProcessingMessageId);
            
            // Clear input field
            m_xAITextInput->SetText("");
            
            // Store message ID for potential retry
            m_nLastUserMessageId = nMessageId;
            
            // Queue message for processing with processing message ID (so we can replace it with response)
            queueMessage(sSanitizedMessage, nProcessingMessageId);
            SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - Message queued for processing");
            
            // Start processing if not already active
            if (!m_bProcessingActive)
            {
                SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - Starting background processing");
                startBackgroundProcessing();
            }
            else
            {
                SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - Background processing already active");
            }
        }
        else
        {
            SAL_INFO("sw.ai", "AIPanel::OnSendMessage() - Empty message ignored");
        }
    }
    else
    {
        SAL_WARN("sw.ai", "AIPanel::OnSendMessage() - UI components not initialized");
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
        m_xAgentCoordinator = uno::Reference<css::ai::XAIAgentCoordinator>(
            xServiceManager->createInstance("com.sun.star.ai.AIAgentCoordinator"),
            uno::UNO_QUERY);
        
        if (!m_xAgentCoordinator.is())
        {
            SAL_WARN("sw.ai", "Failed to create AgentCoordinator service");
            updateConnectionState(ConnectionState::FAILED);
            return false;
        }
        
        // Initialize AgentCoordinator with frame
        if (m_xFrame.is())
        {
            try
            {
                // Get the AgentCoordinator implementation to call initialize with frame
                css::uno::Reference<css::uno::XInterface> xInterface(m_xAgentCoordinator, css::uno::UNO_QUERY);
                if (xInterface.is())
                {
                    // Cast to the implementation class to access initialize method
                    sw::core::ai::AgentCoordinator* pAgentCoordinator = 
                        dynamic_cast<sw::core::ai::AgentCoordinator*>(xInterface.get());
                    if (pAgentCoordinator)
                    {
                        pAgentCoordinator->initialize(m_xFrame);
                        SAL_INFO("sw.ai", "AgentCoordinator initialized with frame successfully");
                    }
                    else
                    {
                        SAL_WARN("sw.ai", "Failed to cast AgentCoordinator for frame initialization");
                    }
                }
            }
            catch (const uno::Exception& e)
            {
                SAL_WARN("sw.ai", "Exception during AgentCoordinator frame initialization: " << e.Message);
            }
        }
        else
        {
            SAL_WARN("sw.ai", "No frame available for AgentCoordinator initialization");
        }
        
        // Test the connection
        if (testConnection())
        {
            updateConnectionState(ConnectionState::CONNECTED);
            m_nReconnectionAttempts = 0;
            
            // Register callback for AI responses to be displayed in chat panel
            sw::core::ai::AgentCoordinator::registerChatPanelCallback(
                [this](const OUString& rsResponse) {
                    // Add AI message to chat history
                    if (m_xChatHistory)
                    {
                        m_xChatHistory->AddAIMessage(rsResponse);
                        SAL_INFO("sw.ai", "AI response added to chat panel via callback");
                    }
                    else
                    {
                        SAL_WARN("sw.ai", "Chat history widget not available for callback");
                    }
                });
                
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
    SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - ENTRY");
    
    try
    {
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Checking frame reference");
        if (!m_xFrame.is())
        {
            SAL_WARN("sw.ai", "AIPanel::prepareDocumentContext() - Frame is NULL, returning empty context");
            return uno::Any();
        }
        
        // Get document reference
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Getting controller from frame");
        uno::Reference<frame::XController> xController = m_xFrame->getController();
        if (!xController.is())
        {
            SAL_WARN("sw.ai", "AIPanel::prepareDocumentContext() - Controller is NULL, returning empty context");
            return uno::Any();
        }
        
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Getting document model from controller");
        uno::Reference<text::XTextDocument> xTextDoc(xController->getModel(), uno::UNO_QUERY);
        if (!xTextDoc.is())
        {
            SAL_WARN("sw.ai", "AIPanel::prepareDocumentContext() - TextDocument is NULL, returning empty context");
            return uno::Any();
        }
        
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Document references obtained successfully");
        
        // Create comprehensive document context
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Creating context sequence");
        uno::Sequence<beans::PropertyValue> aContext(4);
        beans::PropertyValue* pContext = aContext.getArray();
        
        // Document reference
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Setting document reference");
        pContext[0].Name = "Document";
        pContext[0].Value = uno::Any(xTextDoc);
        
        // Frame reference for more context
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Setting frame reference");
        pContext[1].Name = "Frame";
        pContext[1].Value = uno::Any(m_xFrame);
        
        // Current timestamp
        pContext[2].Name = "Timestamp";
        pContext[2].Value = uno::Any(sal_Int64(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));
        
        // User preferences (placeholder)
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Setting user preferences");
        pContext[3].Name = "UserPreferences";
        pContext[3].Value = uno::Any(OUString("default"));
        
        SAL_INFO("sw.ai", "AIPanel::prepareDocumentContext() - Context prepared successfully, returning");
        return uno::Any(aContext);
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("sw.ai", "Exception preparing document context: " << e.Message);
        SAL_WARN("sw.ai", "AIPanel::prepareDocumentContext() - Returning empty context due to exception");
        return uno::Any();
    }
}

void AIPanel::processMessageQueue()
{
    SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Starting message queue processing");
    
    std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
    
    SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Queue size: " << m_aMessageQueue.size());
    
    while (!m_aMessageQueue.empty() && !isProcessingCancelled())
    {
        QueuedMessage aMessage = m_aMessageQueue.front();
        m_aMessageQueue.pop();
        
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Processing message ID: " << aMessage.sMessageId);
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Message content: " << aMessage.sContent);
        
        // Update internal message state only (no UI updates to avoid deadlock)
        // Note: We already hold m_aMessageMutex, so update directly without calling updateMessageState()
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - About to update message state directly");
        auto it = m_aActiveMessages.find(aMessage.sMessageId);
        if (it != m_aActiveMessages.end())
        {
            it->second.eState = MessageState::PROCESSING;
            SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Message state updated to PROCESSING: " << aMessage.sMessageId);
        }
        else
        {
            SAL_WARN("sw.ai", "AIPanel::processMessageQueue() - Could not find message to update: " << aMessage.sMessageId);
        }
        
        // Skip problematic UI updates that cause deadlock
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Skipping UI updates to avoid deadlock, processing message directly");
        
        // Send to backend (this will be done in a separate thread)
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - About to send message to backend");
        sendMessageToBackend(aMessage);
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Message sent to backend, continuing queue processing");
        
        // Clean up this message from active messages without calling updateMessageState
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Cleaning up message from active messages");
        m_aActiveMessages.erase(aMessage.sMessageId);
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Message cleanup completed");
    }
    
    if (isProcessingCancelled())
    {
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Processing cancelled, exiting queue processing");
    }
    else
    {
        SAL_INFO("sw.ai", "AIPanel::processMessageQueue() - Queue processing completed normally");
    }
}

void AIPanel::sendMessageToBackend(const QueuedMessage& rMessage)
{
    SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Starting backend send for message ID: " << rMessage.sMessageId);
    SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Message content: " << rMessage.sContent);
    SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Chat message ID: " << rMessage.nChatMessageId);
    
    // Check connection state
    if (!isConnected())
    {
        SAL_WARN("sw.ai", "AIPanel::sendMessageToBackend() - Not connected to backend, connection state: " << static_cast<int>(m_eConnectionState.load()));
        
        if (m_eConnectionState.load() == ConnectionState::FAILED)
        {
            SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Attempting reconnection due to failed state");
            // Attempt reconnection
            attemptReconnection();
            if (!isConnected())
            {
                SAL_WARN("sw.ai", "AIPanel::sendMessageToBackend() - Reconnection failed, aborting message send");
                // Update chat history first
                if (m_xChatHistory)
                {
                    m_xChatHistory->UpdateMessageStatus(rMessage.nChatMessageId, MessageStatus::ERROR, "Cannot connect to AI service");
                }
                handleBackendError(rMessage.sMessageId, "Cannot connect to AI service");
                return;
            }
            SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Reconnection successful, proceeding with message send");
        }
        else
        {
            SAL_WARN("sw.ai", "AIPanel::sendMessageToBackend() - Not connected and not in failed state, aborting message send");
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
        SAL_WARN("sw.ai", "AIPanel::sendMessageToBackend() - AgentCoordinator service not available");
        // Update chat history first
        if (m_xChatHistory)
        {
            m_xChatHistory->UpdateMessageStatus(rMessage.nChatMessageId, MessageStatus::ERROR, "AgentCoordinator not available");
        }
        handleBackendError(rMessage.sMessageId, "AgentCoordinator not available");
        return;
    }
    
    SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - AgentCoordinator available, proceeding with request");
    
    try
    {
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - About to prepare document context");
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - m_xAgentCoordinator valid: " << (m_xAgentCoordinator.is() ? "TRUE" : "FALSE"));
        
        // Prepare document context
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Calling prepareDocumentContext()");
        uno::Any aDocumentContext = prepareDocumentContext();
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - prepareDocumentContext() completed successfully");
        
        // Skip all problematic state updates and go straight to AgentCoordinator
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Skipping state updates, calling AgentCoordinator directly");
        
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - About to call AgentCoordinator.processUserRequest()");
        // Call AgentCoordinator (this should be async in a real implementation)
        OUString sResponse = m_xAgentCoordinator->processUserRequest(
            rMessage.sContent, aDocumentContext);
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - AgentCoordinator.processUserRequest() completed");
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Response length: " << sResponse.getLength());
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Response preview: " << sResponse.copy(0, std::min(100, sResponse.getLength())));
        
        // Handle response directly without state management
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Adding response directly to chat");
        if (m_xChatHistory)
        {
            parseAndDisplayEnhancedResponse(sResponse);
            SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Response added to chat successfully");
        }
        
        SAL_INFO("sw.ai", "AIPanel::sendMessageToBackend() - Backend processing completed successfully");
    }
    catch (const uno::Exception& e)
    {
        SAL_WARN("sw.ai", "AIPanel::sendMessageToBackend() - Exception caught: " << e.Message);
        
        // Add error message directly to chat without complex state management
        if (m_xChatHistory)
        {
            OUString sErrorMessage = "Error: " + e.Message;
            m_xChatHistory->AddAIMessage(sErrorMessage);
            SAL_WARN("sw.ai", "AIPanel::sendMessageToBackend() - Error message added to chat");
        }
    }
}

void AIPanel::handleBackendResponse(const OUString& rsMessageId, const OUString& rsResponse)
{
    SAL_INFO("sw.ai", "AIPanel::handleBackendResponse() - Starting response handling for message ID: " << rsMessageId);
    SAL_INFO("sw.ai", "AIPanel::handleBackendResponse() - Response length: " << rsResponse.getLength());
    SAL_INFO("sw.ai", "AIPanel::handleBackendResponse() - Response preview: " << rsResponse.copy(0, std::min(200, rsResponse.getLength())));
    
    updateMessageState(rsMessageId, MessageState::DELIVERED);
    SAL_INFO("sw.ai", "AIPanel::handleBackendResponse() - Message state updated to DELIVERED");
    
    // Find the original message to update its status
    sal_Int32 nChatMessageId = -1;
    {
        std::lock_guard<std::mutex> aGuard(m_aMessageMutex);
        auto it = m_aActiveMessages.find(rsMessageId);
        if (it != m_aActiveMessages.end())
        {
            nChatMessageId = it->second.nChatMessageId;
            SAL_INFO("sw.ai", "AIPanel::handleBackendResponse() - Found chat message ID: " << nChatMessageId);
        }
        else
        {
            SAL_WARN("sw.ai", "AIPanel::handleBackendResponse() - Could not find chat message ID for message: " << rsMessageId);
        }
    }
    
    // Hide loading indicators
    SAL_INFO("sw.ai", "AIPanel::handleBackendResponse() - Hiding typing indicator");
    hideTypingIndicator();
    
    // Add the AI response as a new message (keep the processing message for now)
    if (m_xChatHistory)
    {
        SAL_INFO("sw.ai", "AIPanel::handleBackendResponse() - Adding AI response to chat");
        parseAndDisplayEnhancedResponse(rsResponse);
        SAL_INFO("sw.ai", "AIPanel::handleBackendResponse() - AI response added to chat successfully");
    }
    else
    {
        SAL_WARN("sw.ai", "AIPanel::handleBackendResponse() - Cannot display response - chat history not available");
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

// PHASE 7: Enhanced Response Display Implementation
void AIPanel::parseAndDisplayEnhancedResponse(const OUString& rsResponse)
{
    SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - Starting enhanced response parsing");
    SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - Response length: " << rsResponse.getLength());
    
    try
    {
        // Convert OUString to std::string for boost JSON parsing
        OString aUtf8Response = OUStringToOString(rsResponse, RTL_TEXTENCODING_UTF8);
        std::string sJsonString(aUtf8Response.getStr());
        SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - UTF8 conversion completed");
        
        // Check if response looks like JSON (starts with { or [)
        if (sJsonString.empty() || (sJsonString[0] != '{' && sJsonString[0] != '['))
        {
            // Not JSON - treat as simple text response (fallback)
            SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - Response not in JSON format, displaying as simple text");
            m_xChatHistory->AddAIMessage(rsResponse);
            return;
        }
        
        // Parse JSON response
        boost::property_tree::ptree aParsedResponse;
        std::istringstream sStream(sJsonString);
        boost::property_tree::read_json(sStream, aParsedResponse);
        SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - JSON parsing successful");
        
        // Extract components per AGENT_SYSTEM_SPECIFICATION.md
        bool bSuccess = aParsedResponse.get<bool>("success", true);
        OUString sRequestId = OStringToOUString(OString(aParsedResponse.get<std::string>("request_id", "").c_str()), RTL_TEXTENCODING_UTF8);
        double fExecutionTime = aParsedResponse.get<double>("execution_time_ms", 0.0);
        
        SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - Extracted metadata: success=" << bSuccess << 
                 ", request_id=" << sRequestId << ", execution_time=" << fExecutionTime << "ms");
        
        // Extract agent content for chat display
        OUString sAgentContent;
        auto aContentOpt = aParsedResponse.get_child_optional("content");
        if (aContentOpt)
        {
            sAgentContent = OStringToOUString(OString(aContentOpt.value().get_value<std::string>().c_str()), RTL_TEXTENCODING_UTF8);
            SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - Agent content extracted, length: " << sAgentContent.getLength());
        }
        
        // If no separate content field, use the whole response as fallback
        if (sAgentContent.isEmpty())
        {
            sAgentContent = rsResponse;
            SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - No separate content field, using full response as fallback");
        }
        
        // Display agent content first
        if (!sAgentContent.isEmpty())
        {
            m_xChatHistory->AddAIMessage(sAgentContent);
            SAL_INFO("sw.ai", "AIPanel::parseAndDisplayEnhancedResponse() - Agent content displayed in chat");
        }
        
        // Extract and display operation confirmations
        auto aOperationsOpt = aParsedResponse.get_child_optional("operations");
        if (aOperationsOpt && !aOperationsOpt.value().empty())
        {
            OUString sOperationSummary = formatOperationConfirmations(aOperationsOpt.value(), bSuccess, fExecutionTime);
            if (!sOperationSummary.isEmpty())
            {
                m_xChatHistory->AddAIMessage(sOperationSummary);
            }
        }
        
        // Display metadata if present
        auto aMetadataOpt = aParsedResponse.get_child_optional("metadata");
        if (aMetadataOpt)
        {
            OUString sMetadataSummary = formatMetadataSummary(aMetadataOpt.value());
            if (!sMetadataSummary.isEmpty())
            {
                m_xChatHistory->AddAIMessage(sMetadataSummary);
            }
        }
        
        // Display error details if present
        auto aErrorOpt = aParsedResponse.get_child_optional("error_details");
        if (aErrorOpt)
        {
            OUString sErrorSummary = formatErrorDetails(aErrorOpt.value());
            if (!sErrorSummary.isEmpty())
            {
                m_xChatHistory->AddAIMessage("⚠️ " + sErrorSummary);
            }
        }
        
        SAL_INFO("sw.ai", "Enhanced response parsed and displayed - Request ID: " << sRequestId << 
                 ", Execution time: " << fExecutionTime << "ms");
    }
    catch (const boost::property_tree::json_parser_error& e)
    {
        SAL_WARN("sw.ai", "JSON parsing error in enhanced response: " << e.what());
        // Fallback to simple text display
        m_xChatHistory->AddAIMessage(rsResponse);
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "Error parsing enhanced response: " << e.what());
        // Fallback to simple text display
        m_xChatHistory->AddAIMessage(rsResponse);
    }
}

OUString AIPanel::formatOperationConfirmations(const boost::property_tree::ptree& rOperations, bool /*bSuccess*/, double fExecutionTime) const
{
    try
    {
        if (rOperations.empty())
        {
            return OUString();
        }
        
        sal_Int32 nOperationCount = 0;
        sal_Int32 nSuccessCount = 0;
        OUString sOperationDetails;
        
        // Count and format operations
        for (const auto& rOpPair : rOperations)
        {
            const auto& rOperation = rOpPair.second;
            nOperationCount++;
            
            std::string sType = rOperation.get<std::string>("type", "unknown");
            sal_Int32 nPriority = rOperation.get<sal_Int32>("priority", 0);
            
            // Convert type to user-friendly description
            OUString sDescription = getOperationDescription(sType);
            
            // Add operation to details
            sOperationDetails += "  • " + sDescription;
            if (nPriority > 0)
            {
                sOperationDetails += " (priority: " + OUString::number(nPriority) + ")";
            }
            sOperationDetails += "\n";
            
            nSuccessCount++; // Assume success if present in operations list
        }
        
        // Format summary
        OUString sSummary = "🔧 Operations Executed:\n";
        sSummary += sOperationDetails;
        
        if (nOperationCount > 0)
        {
            sSummary += "✅ " + OUString::number(nSuccessCount) + "/" + OUString::number(nOperationCount) + " operations completed";
            
            if (fExecutionTime > 0)
            {
                sSummary += " in " + OUString::number(static_cast<sal_Int32>(fExecutionTime)) + "ms";
            }
        }
        
        return sSummary;
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "Error formatting operation confirmations: " << e.what());
        return "🔧 Operations were executed (details unavailable)";
    }
}

OUString AIPanel::getOperationDescription(const std::string& sType) const
{
    // Convert operation types from AGENT_SYSTEM_SPECIFICATION.md to user-friendly descriptions
    if (sType == "insert_text")
        return "Text inserted";
    else if (sType == "modify_text")
        return "Text modified";
    else if (sType == "apply_formatting")
        return "Formatting applied";
    else if (sType == "create_table")
        return "Table created";
    else if (sType == "create_chart")
        return "Chart created";
    else if (sType == "insert_image")
        return "Image inserted";
    else if (sType == "restructure_document")
        return "Document structure modified";
    else if (sType == "apply_template")
        return "Template applied";
    else
        return OStringToOUString(OString(sType.c_str()), RTL_TEXTENCODING_UTF8) + " operation";
}

OUString AIPanel::formatMetadataSummary(const boost::property_tree::ptree& rMetadata) const
{
    try
    {
        std::string sComplexity = rMetadata.get<std::string>("complexity_detected", "");
        bool bPerformanceTarget = rMetadata.get<bool>("performance_target_met", true);
        bool bCacheUsed = rMetadata.get<bool>("cache_used", false);
        
        auto aAgentsOpt = rMetadata.get_child_optional("agents_involved");
        
        OUString sSummary = "ℹ️ Process Details:\n";
        
        if (!sComplexity.empty())
        {
            sSummary += "  • Complexity: " + OStringToOUString(OString(sComplexity.c_str()), RTL_TEXTENCODING_UTF8) + "\n";
        }
        
        if (aAgentsOpt && !aAgentsOpt.value().empty())
        {
            sSummary += "  • Agents: ";
            bool bFirst = true;
            for (const auto& rAgentPair : aAgentsOpt.value())
            {
                if (!bFirst) sSummary += ", ";
                sSummary += OStringToOUString(OString(rAgentPair.second.get_value<std::string>().c_str()), RTL_TEXTENCODING_UTF8);
                bFirst = false;
            }
            sSummary += "\n";
        }
        
        if (!bPerformanceTarget)
        {
            sSummary += "  • ⚠️ Performance target exceeded\n";
        }
        
        if (bCacheUsed)
        {
            sSummary += "  • 📋 Used cached data\n";
        }
        
        return sSummary;
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "Error formatting metadata summary: " << e.what());
        return OUString();
    }
}

OUString AIPanel::formatErrorDetails(const boost::property_tree::ptree& rErrorDetails) const
{
    try
    {
        std::string sErrorCode = rErrorDetails.get<std::string>("error_code", "");
        std::string sUserMessage = rErrorDetails.get<std::string>("user_message", "");
        auto aSuggestedActionsOpt = rErrorDetails.get_child_optional("suggested_actions");
        
        OUString sErrorSummary = "Error Details:\n";
        
        if (!sErrorCode.empty())
        {
            sErrorSummary += "  • Code: " + OStringToOUString(OString(sErrorCode.c_str()), RTL_TEXTENCODING_UTF8) + "\n";
        }
        
        if (!sUserMessage.empty())
        {
            sErrorSummary += "  • " + OStringToOUString(OString(sUserMessage.c_str()), RTL_TEXTENCODING_UTF8) + "\n";
        }
        
        if (aSuggestedActionsOpt && !aSuggestedActionsOpt.value().empty())
        {
            sErrorSummary += "  • Suggestions:\n";
            for (const auto& rActionPair : aSuggestedActionsOpt.value())
            {
                sErrorSummary += "    - " + OStringToOUString(OString(rActionPair.second.get_value<std::string>().c_str()), RTL_TEXTENCODING_UTF8) + "\n";
            }
        }
        
        return sErrorSummary;
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "Error formatting error details: " << e.what());
        return "An error occurred (details unavailable)";
    }
}

} // end of namespace sw::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */