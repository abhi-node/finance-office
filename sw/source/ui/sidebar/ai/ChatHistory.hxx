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
 * Chat message data structure
 */
struct ChatMessage
{
    OUString content;           ///< Message text content
    MessageType type;           ///< Message sender type
    DateTime timestamp;         ///< Message creation time
    sal_Int32 messageId;        ///< Unique message identifier
    
    ChatMessage(const OUString& rContent, MessageType eType, sal_Int32 nId)
        : content(rContent)
        , type(eType)
        , timestamp(DateTime::SYSTEM)
        , messageId(nId)
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
     */
    void AddUserMessage(const OUString& rMessage);
    
    /**
     * Add new AI message to chat
     * @param rMessage Message text
     */
    void AddAIMessage(const OUString& rMessage);
    
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
     * Format message for display
     * @param rMessage Message to format
     * @return Formatted message text
     */
    OUString FormatMessage(const ChatMessage& rMessage) const;
    
    /**
     * Update the display with current messages
     */
    void UpdateDisplay();

    // Member variables
    std::unique_ptr<weld::TextView> m_xTextView;  ///< Text display widget
    ChatHistory m_aChatHistory;                   ///< Message storage
};

} // end of namespace sw::sidebar

#endif // INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_CHATHISTORY_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */