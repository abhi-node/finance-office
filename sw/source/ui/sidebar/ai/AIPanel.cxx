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
{
    InitializeUI();
}

AIPanel::~AIPanel() = default;

void AIPanel::InitializeUI()
{
    // Get widgets from the UI file
    m_xChatHistoryView = m_xBuilder->weld_text_view("chat_history_view");
    m_xTextInput = m_xBuilder->weld_text_view("text_input");
    m_xSendButton = m_xBuilder->weld_button("send_button");
    
    if (m_xChatHistoryView && m_xTextInput && m_xSendButton)
    {
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
            // Add user message to chat history
            m_xChatHistory->AddUserMessage(sMessage);
            
            // Clear input field
            m_xAITextInput->SetText("");
            
            // TODO: Send message to AI backend
            // For now, just add a simple response
            m_xChatHistory->AddAIMessage("I received your message: \"" + sMessage + "\". AI integration is in development.");
        }
    }
}

IMPL_LINK_NOARG(AIPanel, OnSendButtonClick, weld::Button&, void)
{
    OnSendMessage();
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

} // end of namespace sw::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */