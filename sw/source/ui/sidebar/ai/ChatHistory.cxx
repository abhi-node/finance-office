/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "ChatHistory.hxx"

#include <vcl/weld.hxx>
#include <rtl/ustring.hxx>
#include <tools/time.hxx>

namespace sw::sidebar {

// ChatHistory implementation

sal_Int32 ChatHistory::AddMessage(const OUString& rContent, MessageType eType)
{
    auto pMessage = std::make_unique<ChatMessage>(rContent, eType, m_nNextMessageId++);
    sal_Int32 nId = pMessage->messageId;
    m_aMessages.push_back(std::move(pMessage));
    return nId;
}

const ChatMessage* ChatHistory::GetMessage(sal_Int32 nIndex) const
{
    if (nIndex >= 0 && nIndex < static_cast<sal_Int32>(m_aMessages.size()))
        return m_aMessages[nIndex].get();
    return nullptr;
}

// ChatHistoryWidget implementation

ChatHistoryWidget::ChatHistoryWidget(std::unique_ptr<weld::TextView> xTextView)
    : m_xTextView(std::move(xTextView))
{
    // Configure the TextView for chat display
    if (m_xTextView)
    {
        m_xTextView->set_editable(false);
        m_xTextView->set_monospace(false);
    }
}

ChatHistoryWidget::~ChatHistoryWidget() = default;

void ChatHistoryWidget::AddUserMessage(const OUString& rMessage)
{
    m_aChatHistory.AddMessage(rMessage, MessageType::User);
    UpdateDisplay();
}

void ChatHistoryWidget::AddAIMessage(const OUString& rMessage)
{
    m_aChatHistory.AddMessage(rMessage, MessageType::AI);
    UpdateDisplay();
}

void ChatHistoryWidget::ClearMessages()
{
    m_aChatHistory.Clear();
    UpdateDisplay();
}

OUString ChatHistoryWidget::FormatMessage(const ChatMessage& rMessage) const
{
    OUString sPrefix;
    if (rMessage.type == MessageType::User)
    {
        sPrefix = "You: ";
    }
    else
    {
        sPrefix = "AI: ";
    }
    
    // Format timestamp - get individual time components from DateTime
    sal_uInt16 nHour = rMessage.timestamp.GetHour();
    sal_uInt16 nMin = rMessage.timestamp.GetMin();
    sal_uInt16 nSec = rMessage.timestamp.GetSec();
    
    // Format time string with zero padding (HH:MM:SS) - use explicit OUString constructor
    OUString sHour, sMin, sSec;
    if (nHour < 10)
        sHour = OUString("0") + OUString::number(nHour);
    else
        sHour = OUString::number(nHour);
        
    if (nMin < 10)
        sMin = OUString("0") + OUString::number(nMin);
    else
        sMin = OUString::number(nMin);
        
    if (nSec < 10)
        sSec = OUString("0") + OUString::number(nSec);
    else
        sSec = OUString::number(nSec);
    
    OUString sTime = sHour + ":" + sMin + ":" + sSec;
    
    return "[" + sTime + "] " + sPrefix + rMessage.content + "\n";
}

void ChatHistoryWidget::UpdateDisplay()
{
    if (!m_xTextView)
        return;
    
    OUString sAllText;
    
    // Build complete chat history text
    for (sal_Int32 i = 0; i < m_aChatHistory.GetMessageCount(); ++i)
    {
        const ChatMessage* pMessage = m_aChatHistory.GetMessage(i);
        if (pMessage)
        {
            sAllText += FormatMessage(*pMessage);
        }
    }
    
    // Update the display
    m_xTextView->set_text(sAllText);
    
    // Scroll to bottom to show latest message
    // Note: Proper scrolling would need additional TextView methods
}

} // end of namespace sw::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */