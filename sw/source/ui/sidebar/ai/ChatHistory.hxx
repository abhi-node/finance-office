/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_CHATHISTORY_HXX
#define INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_CHATHISTORY_HXX

#include <vcl/weld.hxx>
#include <tools/datetime.hxx>
#include <memory>
#include <vector>

namespace sw::sidebar {

/**
 * Message types for chat conversation
 */
enum class MessageType
{
    User,    ///< Message from user
    AI       ///< Message from AI agent
};

/**
 * Message status for tracking delivery and processing
 */
enum class MessageStatus
{
    QUEUED,      ///< Message queued for processing
    SENDING,     ///< Message being sent to backend
    PROCESSING,  ///< Being processed by AI backend
    DELIVERED,   ///< Successfully delivered and processed
    ERROR,       ///< Error occurred during processing
    RETRY,       ///< Pending retry after error
    CANCELLED    ///< Operation was cancelled by user
};

/**
 * Chat message data structure with status tracking
 */
struct ChatMessage
{
    OUString content;           ///< Message text content
    MessageType type;           ///< Message sender type
    DateTime timestamp;         ///< Message creation time
    sal_Int32 messageId;        ///< Unique message identifier
    MessageStatus status;       ///< Current message status
    OUString statusMessage;     ///< Status-specific message (e.g., error details)
    sal_Int32 retryCount;       ///< Number of retry attempts
    bool canRetry;              ///< Whether message can be retried
    
    ChatMessage(const OUString& rContent, MessageType eType, sal_Int32 nId)
        : content(rContent)
        , type(eType)
        , timestamp(DateTime::SYSTEM)
        , messageId(nId)
        , status(eType == MessageType::User ? MessageStatus::QUEUED : MessageStatus::DELIVERED)
        , retryCount(0)
        , canRetry(false)
    {
    }
};

/**
 * Container for managing chat message collection
 */
class ChatHistory
{
public:
    ChatHistory() : m_nNextMessageId(1) {}
    
    /**
     * Add new message to history
     * @param rContent Message text
     * @param eType Message type (user/AI)
     * @return Message ID
     */
    sal_Int32 AddMessage(const OUString& rContent, MessageType eType);
    
    /**
     * Add message with specific status
     * @param rContent Message text
     * @param eType Message type (user/AI)
     * @param eStatus Initial message status
     * @return Message ID
     */
    sal_Int32 AddMessage(const OUString& rContent, MessageType eType, MessageStatus eStatus);
    
    /**
     * Update message status
     * @param nMessageId Message ID to update
     * @param eStatus New status
     * @param rStatusMessage Optional status message
     * @return true if message was found and updated
     */
    bool UpdateMessageStatus(sal_Int32 nMessageId, MessageStatus eStatus, const OUString& rStatusMessage = OUString());
    
    /**
     * Find message by ID
     * @param nMessageId Message ID to find
     * @return Message data or nullptr if not found
     */
    ChatMessage* FindMessage(sal_Int32 nMessageId);
    
    /**
     * Get message by index
     * @param nIndex Message index
     * @return Message data or nullptr if invalid index
     */
    const ChatMessage* GetMessage(sal_Int32 nIndex) const;
    
    /**
     * Get total number of messages
     * @return Message count
     */
    sal_Int32 GetMessageCount() const { return static_cast<sal_Int32>(m_aMessages.size()); }
    
    /**
     * Clear all messages
     */
    void Clear() { m_aMessages.clear(); }
    
private:
    std::vector<std::unique_ptr<ChatMessage>> m_aMessages;
    sal_Int32 m_nNextMessageId;
};

/**
 * Chat History Display Widget using weld framework
 * 
 * Modern weld-based widget for displaying chat conversation
 */
class ChatHistoryWidget
{
public:
    /**
     * Constructor
     * @param xTextView TextView widget to use for display
     */
    explicit ChatHistoryWidget(std::unique_ptr<weld::TextView> xTextView);
    
    /**
     * Destructor
     */
    ~ChatHistoryWidget();
    
    /**
     * Add new user message to chat
     * @param rMessage Message text
     * @return Message ID
     */
    sal_Int32 AddUserMessage(const OUString& rMessage);
    
    /**
     * Add new AI message to chat
     * @param rMessage Message text
     * @return Message ID
     */
    sal_Int32 AddAIMessage(const OUString& rMessage);
    
    /**
     * Update message status with visual indicators
     * @param nMessageId Message ID to update
     * @param eStatus New status
     * @param rStatusMessage Optional status message
     * @return true if message was found and updated
     */
    bool UpdateMessageStatus(sal_Int32 nMessageId, MessageStatus eStatus, const OUString& rStatusMessage = OUString());
    
    /**
     * Add error message with retry functionality
     * @param rErrorMessage Error message text
     * @param nOriginalMessageId ID of the original message that failed
     * @return Message ID of error message
     */
    sal_Int32 AddErrorMessage(const OUString& rErrorMessage, sal_Int32 nOriginalMessageId);
    
    /**
     * Set retry callback for failed messages
     * @param fCallback Function to call when retry is requested
     */
    void SetRetryCallback(const std::function<void(sal_Int32)>& fCallback);
    
    /**
     * Show typing indicator for AI response
     * @param bShow true to show, false to hide
     */
    void ShowTypingIndicator(bool bShow);
    
    /**
     * Update progress indicator for long operations
     * @param nProgress Progress percentage (0-100)
     * @param rMessage Progress message
     */
    void UpdateProgressIndicator(sal_Int32 nProgress, const OUString& rMessage);
    
    /**
     * Show loading indicator with timeout
     * @param rMessage Loading message
     * @param nTimeoutMs Timeout in milliseconds (0 = no timeout)
     */
    void ShowLoadingIndicator(const OUString& rMessage, sal_uInt32 nTimeoutMs = 30000);
    
    /**
     * Hide all loading indicators
     */
    void HideLoadingIndicators();
    
    /**
     * Clear all chat messages
     */
    void ClearMessages();
    
    /**
     * Get the underlying TextView widget
     * @return TextView widget reference
     */
    weld::TextView& GetTextView() { return *m_xTextView; }

private:
    /**
     * Format message for display with status indicators
     * @param rMessage Message to format
     * @return Formatted message text with status icons
     */
    OUString FormatMessage(const ChatMessage& rMessage) const;
    
    /**
     * Get status indicator for message
     * @param eStatus Message status
     * @return Unicode status indicator
     */
    OUString GetStatusIndicator(MessageStatus eStatus) const;
    
    /**
     * Handle retry button clicks
     * @param nMessageId Message ID to retry
     */
    void HandleRetryClick(sal_Int32 nMessageId);
    
public:
    /**
     * Update typing animation frame
     */
    void UpdateTypingAnimation();
    
private:
    
    /**
     * Get animated loading indicator
     * @param nFrame Animation frame number
     * @return Animated indicator string
     */
    OUString GetAnimatedLoadingIndicator(sal_Int32 nFrame) const;
    
    /**
     * Handle loading timeout
     */
    void HandleLoadingTimeout();
    
    /**
     * Update the display with current messages
     */
    void UpdateDisplay();

    // Member variables
    std::unique_ptr<weld::TextView> m_xTextView;  ///< Text display widget
    ChatHistory m_aChatHistory;                   ///< Message storage
    std::function<void(sal_Int32)> m_fRetryCallback; ///< Callback for retry operations
    
    // Loading and animation state
    bool m_bTypingIndicatorVisible;               ///< Whether typing indicator is shown
    bool m_bLoadingIndicatorVisible;              ///< Whether loading indicator is shown
    sal_Int32 m_nAnimationFrame;                  ///< Current animation frame
    sal_Int32 m_nProgressPercentage;              ///< Current progress (0-100)
    OUString m_sLoadingMessage;                   ///< Current loading message
    OUString m_sProgressMessage;                  ///< Current progress message
    std::chrono::steady_clock::time_point m_aLoadingStartTime; ///< When loading started
    sal_uInt32 m_nLoadingTimeoutMs;               ///< Loading timeout in milliseconds
};

} // end of namespace sw::sidebar

#endif // INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_CHATHISTORY_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */