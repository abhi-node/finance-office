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
#include <tools/link.hxx>
#include <memory>
#include "ChatHistory.hxx"
#include "AITextInput.hxx"

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

    // UI components using weld framework
    std::unique_ptr<ChatHistoryWidget> m_xChatHistory;     ///< Chat history widget
    std::unique_ptr<AITextInput> m_xAITextInput;           ///< AI text input with auto-expansion
    std::unique_ptr<weld::TextView> m_xChatHistoryView;    ///< Chat history text view from UI
    std::unique_ptr<weld::TextView> m_xTextInput;          ///< Text input from UI
    std::unique_ptr<weld::Button> m_xSendButton;           ///< Send button

    // Frame reference for document context
    css::uno::Reference<css::frame::XFrame> m_xFrame;
};

} // end of namespace sw::sidebar

#endif // INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AIPANEL_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */