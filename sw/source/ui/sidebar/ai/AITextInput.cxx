/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "AITextInput.hxx"

#include <vcl/weld.hxx>
#include <vcl/event.hxx>
#include <rtl/ustring.hxx>
#include <tools/gen.hxx>
#include <vector>

namespace sw::sidebar {

AITextInput::AITextInput(std::unique_ptr<weld::TextView> xTextView)
    : m_xTextView(std::move(xTextView))
    , m_nCurrentLines(MIN_LINES)
    , m_nLineHeight(0)
    , m_bUpdateScheduled(false)
{
    InitializeInput();
}

AITextInput::~AITextInput() = default;

void AITextInput::InitializeInput()
{
    if (!m_xTextView)
        return;
    
    // Configure the TextView for multi-line input
    m_xTextView->set_editable(true);
    m_xTextView->set_monospace(false);
    
    // Calculate line height based on font metrics
    // This is an approximation - in a real implementation, we'd use font metrics
    m_nLineHeight = 20; // Approximate line height in pixels
    
    // Set initial size for 2 lines
    UpdateHeight();
    
    // Set up text change callback
    m_xTextView->connect_changed(LINK(this, AITextInput, OnTextChangedHdl));
    
    // Set up key press callback  
    m_xTextView->connect_key_press(LINK(this, AITextInput, OnKeyPressHdl));
}

OUString AITextInput::GetText() const
{
    if (!m_xTextView)
        return OUString();
    
    return m_xTextView->get_text();
}

void AITextInput::SetText(const OUString& rText)
{
    if (!m_xTextView)
        return;
    
    m_xTextView->set_text(rText);
    UpdateHeight();
}

void AITextInput::Clear()
{
    SetText(OUString());
}

bool AITextInput::HasFocus() const
{
    if (!m_xTextView)
        return false;
    
    return m_xTextView->has_focus();
}

void AITextInput::GrabFocus()
{
    if (m_xTextView)
        m_xTextView->grab_focus();
}

void AITextInput::SetSendCallback(const std::function<void()>& rCallback)
{
    m_aSendCallback = rCallback;
}

sal_Int32 AITextInput::CalculateLineCount() const
{
    if (!m_xTextView)
        return MIN_LINES;
    
    OUString sText = m_xTextView->get_text();
    if (sText.isEmpty())
        return MIN_LINES;
    
    // Count explicit line breaks
    sal_Int32 nExplicitLines = 1;
    for (sal_Int32 i = 0; i < sText.getLength(); ++i)
    {
        if (sText[i] == '\n')
            ++nExplicitLines;
    }
    
    // Calculate wrapped lines based on widget width
    sal_Int32 nWrappedLines = CalculateWrappedLines(sText);
    
    // Total lines is the maximum of explicit lines and wrapped lines
    sal_Int32 nTotalLines = std::max(nExplicitLines, nWrappedLines);
    
    return std::max(MIN_LINES, std::min(MAX_LINES, nTotalLines));
}

sal_Int32 AITextInput::CalculateWrappedLines(const OUString& rText) const
{
    if (!m_xTextView || rText.isEmpty())
        return MIN_LINES;
    
    // Get the current widget width for wrapping calculations
    Size aSize = m_xTextView->get_size_request();
    sal_Int32 nWidgetWidth = aSize.Width();
    
    // If no specific width is set, use a reasonable default
    if (nWidgetWidth <= 0)
        nWidgetWidth = 300; // Default width in pixels
    
    // Approximate character width (this is simplified - real implementation
    // would use font metrics from the widget's current font)
    sal_Int32 nCharWidth = 8; // Average character width in pixels
    sal_Int32 nCharsPerLine = std::max(sal_Int32(1), (nWidgetWidth - 20) / nCharWidth); // Account for padding
    
    // Split text by explicit line breaks first
    std::vector<OUString> aLines;
    sal_Int32 nStart = 0;
    sal_Int32 nPos = 0;
    
    while (nPos < rText.getLength())
    {
        if (rText[nPos] == '\n')
        {
            aLines.push_back(rText.copy(nStart, nPos - nStart));
            nStart = nPos + 1;
        }
        ++nPos;
    }
    
    // Add the last line if there's remaining text
    if (nStart < rText.getLength())
    {
        aLines.push_back(rText.copy(nStart));
    }
    
    // Now calculate wrapping for each line
    sal_Int32 nTotalWrappedLines = 0;
    
    for (const auto& sLine : aLines)
    {
        if (sLine.isEmpty())
        {
            nTotalWrappedLines += 1; // Empty line still takes one line
        }
        else
        {
            // Simple word wrapping calculation
            sal_Int32 nLineLength = sLine.getLength();
            sal_Int32 nLinesForThisSegment = (nLineLength + nCharsPerLine - 1) / nCharsPerLine;
            nTotalWrappedLines += std::max(sal_Int32(1), nLinesForThisSegment);
        }
    }
    
    return std::max(sal_Int32(1), nTotalWrappedLines);
}

sal_Int32 AITextInput::CalculateHeightForLines(sal_Int32 nLines) const
{
    // Calculate total height including padding
    sal_Int32 nTextHeight = nLines * m_nLineHeight;
    sal_Int32 nPadding = 8; // Top and bottom padding
    
    return nTextHeight + nPadding;
}

void AITextInput::UpdateHeight()
{
    if (!m_xTextView)
        return;
    
    sal_Int32 nLines = CalculateLineCount();
    
    // Only update if line count changed
    if (nLines != m_nCurrentLines)
    {
        sal_Int32 nOldHeight = CalculateHeightForLines(m_nCurrentLines);
        sal_Int32 nNewHeight = CalculateHeightForLines(nLines);
        
        // Smooth height transition with bounds checking
        SmoothHeightTransition(nOldHeight, nNewHeight);
        
        // Update current line count
        m_nCurrentLines = nLines;
    }
}

void AITextInput::SmoothHeightTransition(sal_Int32 nOldHeight, sal_Int32 nNewHeight)
{
    if (!m_xTextView)
        return;
    
    // Apply bounds checking to the new height
    sal_Int32 nBoundedHeight = ValidateHeightBounds(nNewHeight);
    
    // For now, use immediate resize (smooth animation would require more complex VCL integration)
    // In a production implementation, this could use a timer-based animation
    if (nBoundedHeight != nOldHeight)
    {
        m_xTextView->set_size_request(-1, nBoundedHeight);
        
        // Queue resize for parent containers
        if (auto pParent = m_xTextView->weld_parent())
        {
            pParent->queue_resize();
        }
    }
}

sal_Int32 AITextInput::ValidateHeightBounds(sal_Int32 nHeight) const
{
    sal_Int32 nMinHeight = CalculateHeightForLines(MIN_LINES);
    sal_Int32 nMaxHeight = CalculateHeightForLines(MAX_LINES);
    
    // Ensure height stays within bounds
    return std::max(nMinHeight, std::min(nMaxHeight, nHeight));
}

bool AITextInput::HandleKeyPress(const KeyEvent& rKeyEvent)
{
    const vcl::KeyCode& rKeyCode = rKeyEvent.GetKeyCode();
    sal_uInt16 nCode = rKeyCode.GetCode();
    
    // Handle Enter key behavior
    if (nCode == KEY_RETURN)
    {
        // Shift+Enter = new line (default behavior)
        // Enter alone = send message
        if (!rKeyCode.IsShift())
        {
            // Trigger send callback
            if (m_aSendCallback)
            {
                m_aSendCallback();
            }
            return true; // Event handled
        }
        // Shift+Enter: let default handling proceed to insert newline
        return false;
    }
    
    // Handle standard editing shortcuts
    if (rKeyCode.IsMod1()) // Ctrl key (Cmd on Mac)
    {
        switch (nCode)
        {
            case KEY_A: // Ctrl+A - Select All
                return HandleSelectAll();
                
            case KEY_C: // Ctrl+C - Copy
                return HandleCopy();
                
            case KEY_V: // Ctrl+V - Paste
                return HandlePaste();
                
            case KEY_X: // Ctrl+X - Cut
                return HandleCut();
                
            case KEY_Z: // Ctrl+Z - Undo
                return HandleUndo();
                
            case KEY_Y: // Ctrl+Y - Redo
                return HandleRedo();
                
            default:
                break;
        }
    }
    
    return false; // Let default handling proceed
}

void AITextInput::OnTextChanged()
{
    // Prevent recursive updates during text modification
    if (m_bUpdateScheduled)
        return;
    
    // Performance optimization: Avoid unnecessary processing
    OUString sText = GetText();
    
    // Input validation: Character limit enforcement
    if (sText.getLength() > MAX_CHARS)
    {
        // Truncate text to character limit and provide user feedback
        OUString sTruncated = sText.copy(0, MAX_CHARS);
        
        // Prevent recursive text change events during truncation
        // Note: weld::TextView doesn't have disconnect/connect methods
        // We'll use a flag to prevent recursion
        m_bUpdateScheduled = true;
        SetText(sTruncated);
        m_bUpdateScheduled = false;
        
        // Position cursor at end of truncated text
        m_xTextView->select_region(-1, -1);
        
        return;
    }
    
    // Input sanitization: Remove or replace potentially problematic characters
    if (ContainsMaliciousContent(sText))
    {
        OUString sSanitized = SanitizeInput(sText);
        if (sSanitized != sText)
        {
            m_bUpdateScheduled = true;
            SetText(sSanitized);
            m_bUpdateScheduled = false;
            return;
        }
    }
    
    // Performance optimization: Throttle height updates for rapid typing
    if (!m_bUpdateScheduled)
    {
        m_bUpdateScheduled = true;
        // In a production implementation, this would use a timer to delay updates
        // For now, update immediately but track the scheduling
        UpdateHeight();
        m_bUpdateScheduled = false;
    }
}

// Static callback handlers for weld framework
IMPL_LINK_NOARG(AITextInput, OnTextChangedHdl, weld::TextView&, void)
{
    OnTextChanged();
}

IMPL_LINK(AITextInput, OnKeyPressHdl, const KeyEvent&, rKeyEvent, bool)
{
    return HandleKeyPress(rKeyEvent);
}

// Standard editing shortcut handlers
bool AITextInput::HandleSelectAll()
{
    if (!m_xTextView)
        return false;
    
    m_xTextView->select_region(0, -1);
    return true;
}

bool AITextInput::HandleCopy()
{
    if (!m_xTextView)
        return false;
    
    // Let the default handling proceed for copy operation
    // The weld::TextView should handle clipboard operations automatically
    return false;
}

bool AITextInput::HandlePaste()
{
    if (!m_xTextView)
        return false;
    
    // Let the default handling proceed for paste operation
    // The weld::TextView should handle clipboard operations automatically
    // But we need to trigger height update after paste
    
    // Use a timer or delayed call to update height after paste completes
    // For now, let default handling proceed
    return false;
}

bool AITextInput::HandleCut()
{
    if (!m_xTextView)
        return false;
    
    // Let the default handling proceed for cut operation
    // The weld::TextView should handle clipboard operations automatically
    return false;
}

bool AITextInput::HandleUndo()
{
    if (!m_xTextView)
        return false;
    
    // Let the default handling proceed for undo operation
    // Most text controls have built-in undo functionality
    return false;
}

bool AITextInput::HandleRedo()
{
    if (!m_xTextView)
        return false;
    
    // Let the default handling proceed for redo operation
    // Most text controls have built-in redo functionality
    return false;
}

// Input validation and sanitization methods
bool AITextInput::ContainsMaliciousContent(const OUString& rText) const
{
    // Basic security check for potentially malicious content
    // This is a simplified implementation - production code would be more comprehensive
    
    // Check for common script injection patterns
    if (rText.indexOf("<script") >= 0 || 
        rText.indexOf("javascript:") >= 0 ||
        rText.indexOf("data:") >= 0 ||
        rText.indexOf("vbscript:") >= 0)
    {
        return true;
    }
    
    // Check for control characters (except tab, newline, and carriage return)
    for (sal_Int32 i = 0; i < rText.getLength(); ++i)
    {
        sal_Unicode nChar = rText[i];
        if (nChar < 32 && nChar != 9 && nChar != 10 && nChar != 13)
        {
            return true;
        }
    }
    
    return false;
}

OUString AITextInput::SanitizeInput(const OUString& rText) const
{
    OUString sSanitized = rText;
    
    // Remove script tags and javascript URLs
    // This is a basic implementation - production code would use proper parsing
    sSanitized = sSanitized.replaceAll("<script", "");
    sSanitized = sSanitized.replaceAll("javascript:", "");
    sSanitized = sSanitized.replaceAll("data:", "");
    sSanitized = sSanitized.replaceAll("vbscript:", "");
    
    // Remove control characters (except tab, newline, carriage return)
    OUString sResult;
    for (sal_Int32 i = 0; i < sSanitized.getLength(); ++i)
    {
        sal_Unicode nChar = sSanitized[i];
        if (nChar >= 32 || nChar == 9 || nChar == 10 || nChar == 13)
        {
            sResult += OUString(&nChar, 1);
        }
    }
    
    return sResult;
}

} // end of namespace sw::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */