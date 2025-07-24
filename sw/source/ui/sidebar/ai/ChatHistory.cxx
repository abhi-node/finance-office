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

sal_Int32 ChatHistory::AddMessage(const OUString& rContent, MessageType eType, MessageStatus eStatus)
{
    auto pMessage = std::make_unique<ChatMessage>(rContent, eType, m_nNextMessageId++);
    pMessage->status = eStatus;
    sal_Int32 nId = pMessage->messageId;
    m_aMessages.push_back(std::move(pMessage));
    return nId;
}

bool ChatHistory::UpdateMessageStatus(sal_Int32 nMessageId, MessageStatus eStatus, const OUString& rStatusMessage)
{
    auto pMessage = FindMessage(nMessageId);
    if (pMessage)
    {
        pMessage->status = eStatus;
        if (!rStatusMessage.isEmpty())
        {
            pMessage->statusMessage = rStatusMessage;
        }
        return true;
    }
    return false;
}

ChatMessage* ChatHistory::FindMessage(sal_Int32 nMessageId)
{
    for (auto& pMessage : m_aMessages)
    {
        if (pMessage && pMessage->messageId == nMessageId)
        {
            return pMessage.get();
        }
    }
    return nullptr;
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

sal_Int32 ChatHistoryWidget::AddUserMessage(const OUString& rMessage)
{
    sal_Int32 nId = m_aChatHistory.AddMessage(rMessage, MessageType::User);
    UpdateDisplay();
    return nId;
}

sal_Int32 ChatHistoryWidget::AddAIMessage(const OUString& rMessage)
{
    sal_Int32 nId = m_aChatHistory.AddMessage(rMessage, MessageType::AI);
    UpdateDisplay();
    return nId;
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
    
    // Add typing indicator if visible
    if (m_bTypingIndicatorVisible)
    {
        OUString sTypingDots = GetAnimatedLoadingIndicator(m_nAnimationFrame);
        sAllText += "AI is typing" + sTypingDots + "\n";
    }
    
    // Add loading indicator if visible
    if (m_bLoadingIndicatorVisible)
    {
        OUString sLoadingDots = GetAnimatedLoadingIndicator(m_nAnimationFrame);
        sAllText += "🔄 " + m_sLoadingMessage + sLoadingDots + "\n";
    }
    
    // Add progress indicator if active
    if (m_nProgressPercentage > 0 && !m_sProgressMessage.isEmpty())
    {
        // Create simple progress bar with Unicode blocks
        sal_Int32 nFilledBlocks = (m_nProgressPercentage * 10) / 100; // 10-block progress bar
        OUString sProgressBar;
        for (sal_Int32 i = 0; i < 10; ++i)
        {
            if (i < nFilledBlocks)
                sProgressBar += "█"; // Full block
            else
                sProgressBar += "░"; // Light shade
        }
        sAllText += "📊 " + m_sProgressMessage + " [" + sProgressBar + "] " + 
                   OUString::number(m_nProgressPercentage) + "%\n";
    }
    
    // Update the display
    m_xTextView->set_text(sAllText);
    
    // Scroll to bottom to show latest message
    // Note: Proper scrolling would need additional TextView methods
}

void ChatHistoryWidget::HandleRetryClick(sal_Int32 nMessageId)
{
    if (m_fRetryCallback)
    {
        // Update message status to retry
        m_aChatHistory.UpdateMessageStatus(nMessageId, MessageStatus::RETRY, "Retrying...");
        UpdateDisplay();
        
        // Call retry callback
        m_fRetryCallback(nMessageId);
    }
}

void ChatHistoryWidget::ShowTypingIndicator(bool bShow)
{
    if (m_bTypingIndicatorVisible != bShow)
    {
        m_bTypingIndicatorVisible = bShow;
        if (bShow)
        {
            m_nAnimationFrame = 0;
        }
        UpdateDisplay();
    }
}

void ChatHistoryWidget::UpdateProgressIndicator(sal_Int32 nProgress, const OUString& rMessage)
{
    m_nProgressPercentage = std::min(std::max(nProgress, sal_Int32(0)), sal_Int32(100));
    m_sProgressMessage = rMessage;
    UpdateDisplay();
}

void ChatHistoryWidget::ShowLoadingIndicator(const OUString& rMessage, sal_uInt32 nTimeoutMs)
{
    m_bLoadingIndicatorVisible = true;
    m_sLoadingMessage = rMessage;
    m_nLoadingTimeoutMs = nTimeoutMs;
    m_aLoadingStartTime = std::chrono::steady_clock::now();
    m_nAnimationFrame = 0;
    UpdateDisplay();
}

void ChatHistoryWidget::HideLoadingIndicators()
{
    m_bTypingIndicatorVisible = false;
    m_bLoadingIndicatorVisible = false;
    m_nProgressPercentage = 0;
    m_sLoadingMessage.clear();
    m_sProgressMessage.clear();
    UpdateDisplay();
}

void ChatHistoryWidget::UpdateTypingAnimation()
{
    if (m_bTypingIndicatorVisible || m_bLoadingIndicatorVisible)
    {
        m_nAnimationFrame = (m_nAnimationFrame + 1) % 4; // 4-frame animation cycle
        
        // Check for timeout
        if (m_nLoadingTimeoutMs > 0 && m_bLoadingIndicatorVisible)
        {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_aLoadingStartTime);
            if (elapsed.count() > static_cast<long>(m_nLoadingTimeoutMs))
            {
                HandleLoadingTimeout();
                return;
            }
        }
        
        UpdateDisplay();
    }
}

OUString ChatHistoryWidget::GetAnimatedLoadingIndicator(sal_Int32 nFrame) const
{
    // Animated dots pattern
    switch (nFrame % 4)
    {
        case 0: return ".   ";
        case 1: return "..  ";
        case 2: return "... ";
        case 3: return "....";
        default: return "    ";
    }
}

void ChatHistoryWidget::HandleLoadingTimeout()
{
    // Add timeout message to chat
    OUString sTimeoutMsg = "Request timed out after " + OUString::number(m_nLoadingTimeoutMs / 1000) + " seconds. Please try again.";
    AddErrorMessage(sTimeoutMsg, -1);
    
    // Hide loading indicators
    HideLoadingIndicators();
}

OUString ChatHistoryWidget::GetStatusIndicator(MessageStatus eStatus) const
{
    switch (eStatus)
    {
        case MessageStatus::QUEUED:
            return "⏳ ";      // Hourglass for queued
        case MessageStatus::SENDING:
            return "📤 ";      // Outbox for sending
        case MessageStatus::PROCESSING:
            return "⚙️ ";       // Gear for processing
        case MessageStatus::DELIVERED:
            return "✅ ";      // Check mark for delivered
        case MessageStatus::ERROR:
            return "❌ ";      // Cross mark for error
        case MessageStatus::RETRY:
            return "🔄 ";      // Refresh for retry
        case MessageStatus::CANCELLED:
            return "⛔ ";      // No entry sign for cancelled
        default:
            return "";
    }
}

bool ChatHistoryWidget::UpdateMessageStatus(sal_Int32 nMessageId, MessageStatus eStatus, const OUString& rStatusMessage)
{
    bool bSuccess = m_aChatHistory.UpdateMessageStatus(nMessageId, eStatus, rStatusMessage);
    if (bSuccess)
    {
        // Update display
        UpdateDisplay();
    }
    return bSuccess;
}

sal_Int32 ChatHistoryWidget::AddErrorMessage(const OUString& rErrorMessage, sal_Int32 nOriginalMessageId)
{
    // Add error message to the chat history using AI type (since there's no ERROR type)
    sal_Int32 nMessageId = m_aChatHistory.AddMessage(rErrorMessage, MessageType::AI, MessageStatus::ERROR);
    
    // Find the message to set additional error-specific properties
    auto pMessage = m_aChatHistory.FindMessage(nMessageId);
    if (pMessage)
    {
        pMessage->canRetry = true;  // Error messages can typically be retried
        // Store reference to original failed message if needed
        static_cast<void>(nOriginalMessageId); // Mark as used
    }
    
    UpdateDisplay();
    return nMessageId;
}

} // end of namespace sw::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */