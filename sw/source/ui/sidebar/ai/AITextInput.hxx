/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AITEXTINPUT_HXX
#define INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AITEXTINPUT_HXX

#include <vcl/weld.hxx>
#include <vcl/event.hxx>
#include <tools/link.hxx>
#include <memory>

namespace sw::sidebar {

/**
 * Auto-expanding text input field for AI chat interface
 * 
 * Provides a multi-line text input that automatically expands from 2 to 10 lines
 * based on content. Includes proper keyboard handling for Enter/Shift+Enter.
 */
class AITextInput
{
public:
    /**
     * Constructor
     * @param xTextView TextView widget to use for input
     */
    explicit AITextInput(std::unique_ptr<weld::TextView> xTextView);
    
    /**
     * Destructor
     */
    ~AITextInput();
    
    /**
     * Get the text content from the input field
     * @return Current text content
     */
    OUString GetText() const;
    
    /**
     * Set the text content in the input field
     * @param rText Text to set
     */
    void SetText(const OUString& rText);
    
    /**
     * Clear all text from the input field
     */
    void Clear();
    
    /**
     * Check if the input field has focus
     * @return true if focused
     */
    bool HasFocus() const;
    
    /**
     * Set focus to the input field
     */
    void GrabFocus();
    
    /**
     * Get the underlying TextView widget
     * @return TextView widget reference
     */
    weld::TextView& GetTextView() { return *m_xTextView; }
    
    /**
     * Set callback for send action (Enter key)
     * @param rCallback Function to call when send is triggered
     */
    void SetSendCallback(const std::function<void()>& rCallback);

private:
    /**
     * Initialize the text input with proper configuration
     */
    void InitializeInput();
    
    /**
     * Calculate the number of lines in the current text
     * @return Number of lines (including wrapped lines)
     */
    sal_Int32 CalculateLineCount() const;
    
    /**
     * Calculate wrapped lines for given text based on widget width
     * @param rText Text to analyze for wrapping
     * @return Number of lines after text wrapping
     */
    sal_Int32 CalculateWrappedLines(const OUString& rText) const;
    
    /**
     * Calculate required height for given number of lines
     * @param nLines Number of lines
     * @return Required height in pixels
     */
    sal_Int32 CalculateHeightForLines(sal_Int32 nLines) const;
    
    /**
     * Update the height of the input field based on content
     */
    void UpdateHeight();
    
    /**
     * Perform smooth height transition between old and new heights
     * @param nOldHeight Previous height in pixels
     * @param nNewHeight Target height in pixels
     */
    void SmoothHeightTransition(sal_Int32 nOldHeight, sal_Int32 nNewHeight);
    
    /**
     * Validate and apply height bounds checking
     * @param nHeight Proposed height in pixels
     * @return Height adjusted to stay within bounds
     */
    sal_Int32 ValidateHeightBounds(sal_Int32 nHeight) const;
    
    /**
     * Handle key press events for Enter/Shift+Enter behavior
     * @param rKeyEvent Key event to handle
     * @return true if event was handled
     */
    bool HandleKeyPress(const KeyEvent& rKeyEvent);
    
    /**
     * Handle text change events to trigger height updates
     */
    void OnTextChanged();
    
    /**
     * LINK callback for text changed events
     */
    DECL_LINK(OnTextChangedHdl, weld::TextView&, void);
    
    /**
     * LINK callback for key press events
     */
    DECL_LINK(OnKeyPressHdl, const KeyEvent&, bool);
    
    /**
     * Handle standard editing shortcuts
     */
    bool HandleSelectAll();
    bool HandleCopy();
    bool HandlePaste();
    bool HandleCut();
    bool HandleUndo();
    bool HandleRedo();
    
    /**
     * Input validation and sanitization methods
     */
    bool ContainsMaliciousContent(const OUString& rText) const;
    OUString SanitizeInput(const OUString& rText) const;

    // Constants for input behavior
    static constexpr sal_Int32 MIN_LINES = 2;   ///< Minimum number of lines
    static constexpr sal_Int32 MAX_LINES = 10;  ///< Maximum number of lines
    static constexpr sal_Int32 MAX_CHARS = 5000; ///< Character limit

    // Member variables
    std::unique_ptr<weld::TextView> m_xTextView;        ///< Text input widget
    std::function<void()> m_aSendCallback;              ///< Callback for send action
    sal_Int32 m_nCurrentLines;                          ///< Current number of lines
    sal_Int32 m_nLineHeight;                            ///< Height of one line in pixels
    bool m_bUpdateScheduled;                            ///< Flag to prevent rapid height updates
};

} // end of namespace sw::sidebar

#endif // INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AITEXTINPUT_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */