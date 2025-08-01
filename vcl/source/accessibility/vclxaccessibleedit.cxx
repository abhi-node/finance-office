/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <accessibility/vclxaccessibleedit.hxx>

#include <com/sun/star/accessibility/AccessibleStateType.hpp>
#include <com/sun/star/accessibility/AccessibleEventId.hpp>
#include <com/sun/star/accessibility/AccessibleRole.hpp>
#include <com/sun/star/datatransfer/clipboard/XClipboard.hpp>
#include <com/sun/star/accessibility/AccessibleTextType.hpp>
#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>
#include <comphelper/accessiblecontexthelper.hxx>
#include <comphelper/string.hxx>
#include <vcl/svapp.hxx>
#include <vcl/window.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/settings.hxx>
#include <vcl/toolkit/vclmedit.hxx>
#include <vcl/textdata.hxx>
#include <vcl/txtattr.hxx>
#include <vcl/unohelp.hxx>
#include <vcl/xtextedt.hxx>
#include <sot/exchange.hxx>
#include <sot/formats.hxx>

#include <algorithm>

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::accessibility;
using namespace ::comphelper;


// VCLXAccessibleEdit


VCLXAccessibleEdit::VCLXAccessibleEdit(Edit* pEdit)
    :ImplInheritanceHelper(pEdit)
{
    m_nCaretPosition = getCaretPosition();
}


void VCLXAccessibleEdit::ProcessWindowEvent( const VclWindowEvent& rVclWindowEvent )
{
    switch ( rVclWindowEvent.GetId() )
    {
        case VclEventId::EditModify:
        {
            SetText( implGetText() );
        }
        break;
        case VclEventId::EditCaretChanged:
        {
            sal_Int32 nOldCaretPosition = m_nCaretPosition;
            m_nCaretPosition = getCaretPosition();

            VclPtr<vcl::Window> pWindow = GetWindow();
            if (pWindow && pWindow->HasChildPathFocus())
            {
                if (m_nCaretPosition != nOldCaretPosition)
                {
                    Any aOldValue, aNewValue;
                    aOldValue <<= nOldCaretPosition;
                    aNewValue <<= m_nCaretPosition;
                    NotifyAccessibleEvent( AccessibleEventId::CARET_CHANGED, aOldValue, aNewValue );
                }
            }
        }
        break;
        case VclEventId::EditSelectionChanged:
        {
            VclPtr<vcl::Window> pWindow = GetWindow();
            if (pWindow && pWindow->HasChildPathFocus())
            {
                NotifyAccessibleEvent( AccessibleEventId::TEXT_SELECTION_CHANGED, Any(), Any() );
            }
        }
        break;
        default:
            VCLXAccessibleTextComponent::ProcessWindowEvent( rVclWindowEvent );
   }
}


void VCLXAccessibleEdit::FillAccessibleStateSet( sal_Int64& rStateSet )
{
    VCLXAccessibleTextComponent::FillAccessibleStateSet( rStateSet );

    vcl::Window* pWindow = GetWindow();
    if (pWindow)
    {
        rStateSet |= AccessibleStateType::FOCUSABLE;

        if (pWindow->GetType() == WindowType::MULTILINEEDIT)
            rStateSet |= AccessibleStateType::MULTI_LINE;
        else
            rStateSet |= AccessibleStateType::SINGLE_LINE;

        if (isEditable())
            rStateSet |= AccessibleStateType::EDITABLE;
    }
}


// OCommonAccessibleText


OUString VCLXAccessibleEdit::implGetText()
{
    OUString aText;

    VclPtr< Edit > pEdit = GetAs< Edit >();
    if ( pEdit )
    {
        aText = removeMnemonicFromString( pEdit->GetText() );

        if ( implGetAccessibleRole() == AccessibleRole::PASSWORD_TEXT )
        {
            sal_Unicode cEchoChar = pEdit->GetEchoChar();
            if ( !cEchoChar )
                cEchoChar = '*';
            OUStringBuffer sTmp(aText.getLength());
            aText = comphelper::string::padToLength(sTmp, aText.getLength(),
                cEchoChar).makeStringAndClear();
        }
    }

    return aText;
}


void VCLXAccessibleEdit::implGetSelection( sal_Int32& nStartIndex, sal_Int32& nEndIndex )
{
    Selection aSelection;
    VclPtr<Edit> pEdit = GetAs<Edit>();
    if (pEdit)
        aSelection = pEdit->GetSelection();

    nStartIndex = aSelection.Min();
    nEndIndex = aSelection.Max();
}

// VCLXAccessibleTextComponent
bool VCLXAccessibleEdit::PreferFullTextInTextChangedEvent()
{
    // for a combobox subedit, the Orca screen reader announces the new/added text
    // so always send the whole old and new text and not just
    // the changed characters, so the whole entry text gets announced
    return isComboBoxChild();
}

// XServiceInfo


OUString VCLXAccessibleEdit::getImplementationName()
{
    return u"com.sun.star.comp.toolkit.AccessibleEdit"_ustr;
}


Sequence< OUString > VCLXAccessibleEdit::getSupportedServiceNames()
{
    return { u"com.sun.star.awt.AccessibleEdit"_ustr };
}

// XAccessibleContext

sal_Int64 VCLXAccessibleEdit::getAccessibleChildCount()
{
    OExternalLockGuard aGuard( this );

    return 0;
}


Reference< XAccessible > VCLXAccessibleEdit::getAccessibleChild( sal_Int64 )
{
    throw IndexOutOfBoundsException();
}

OUString VCLXAccessibleEdit::getAccessibleName()
{
    OExternalLockGuard aGuard(this);

    // for combobox edit, return name of the parent
    if (isComboBoxChild())
        return getAccessibleParent()->getAccessibleContext()->getAccessibleName();

    return VCLXAccessibleTextComponent::getAccessibleName();
}

sal_Int16 VCLXAccessibleEdit::getAccessibleRole(  )
{
    OExternalLockGuard aGuard( this );

    return implGetAccessibleRole();
}

sal_Int16 VCLXAccessibleEdit::implGetAccessibleRole(  )
{
    sal_Int16 nRole;
    VclPtr< Edit > pEdit = GetAs< Edit >();
    if ( pEdit && ( pEdit->IsPassword() || pEdit->GetEchoChar() ) )
        nRole = AccessibleRole::PASSWORD_TEXT;
    else if ( pEdit && ( pEdit->GetStyle() & WB_READONLY ) )
        nRole = AccessibleRole::STATIC;
    else
        nRole = AccessibleRole::TEXT;

    return nRole;
}


// XAccessibleAction


sal_Int32 VCLXAccessibleEdit::getAccessibleActionCount( )
{
    OExternalLockGuard aGuard( this );

    // There is one action: activate
    return 1;
}


sal_Bool VCLXAccessibleEdit::doAccessibleAction ( sal_Int32 nIndex )
{
    OExternalLockGuard aGuard( this );

    if ( nIndex != 0 )
        throw IndexOutOfBoundsException();

    bool bDoAction = false;
    VclPtr<vcl::Window> pWindow = GetWindow();
    if ( pWindow )
    {
        pWindow->GrabFocus();
        bDoAction = true;
    }

    return bDoAction;
}


OUString VCLXAccessibleEdit::getAccessibleActionDescription ( sal_Int32 nIndex )
{
    OExternalLockGuard aGuard( this );

    if ( nIndex != 0)
        throw IndexOutOfBoundsException();

    return u"activate"_ustr;
}


Reference< XAccessibleKeyBinding > VCLXAccessibleEdit::getAccessibleActionKeyBinding( sal_Int32 nIndex )
{
    OExternalLockGuard aGuard( this );

    if ( nIndex != 0 )
        throw IndexOutOfBoundsException();

    return Reference< XAccessibleKeyBinding >();
}


// XAccessibleText


sal_Int32 VCLXAccessibleEdit::getCaretPosition(  )
{
    return getSelectionEnd();
}


sal_Bool VCLXAccessibleEdit::setCaretPosition( sal_Int32 nIndex )
{
    return setSelection( nIndex, nIndex );
}


sal_Unicode VCLXAccessibleEdit::getCharacter( sal_Int32 nIndex )
{
    return VCLXAccessibleTextComponent::getCharacter( nIndex );
}


Sequence< PropertyValue > VCLXAccessibleEdit::getCharacterAttributes( sal_Int32 nIndex, const Sequence< OUString >& aRequestedAttributes )
{
    OExternalLockGuard aGuard( this );
    Sequence< PropertyValue > aProperties = VCLXAccessibleTextComponent::getCharacterAttributes( nIndex, aRequestedAttributes );
    auto aNonConstRange = asNonConstRange(aProperties);

    // Handle multiline edit character properties
    VclPtr<VclMultiLineEdit> pMulitLineEdit = GetAsDynamic< VclMultiLineEdit >();
    if ( pMulitLineEdit )
    {
        ExtTextEngine* pTextEngine = pMulitLineEdit->GetTextEngine();
        TextPaM aCursor( 0, nIndex );
        const TextAttribFontColor* pFontColor = static_cast<const TextAttribFontColor* >(pTextEngine->FindAttrib( aCursor, TEXTATTR_FONTCOLOR ));
        if ( pFontColor )
        {
            for (PropertyValue& aValue : aNonConstRange )
            {
                if (aValue.Name == "CharColor")
                {
                    aValue.Value <<= pFontColor->GetColor().GetRGBColor();
                    break;
                }
            }
        }
    }

    // Set default character color if it is not set yet to a valid value
    for (PropertyValue& aValue : aNonConstRange )
    {
        if (aValue.Name == "CharColor")
        {
            if ( aValue.Value == sal_Int32(-1) )
            {
                OutputDevice* pDev = Application::GetDefaultDevice();
                if ( pDev )
                {
                    aValue.Value <<= pDev->GetSettings().GetStyleSettings().GetFieldTextColor();
                }
            }
            break;
        }
    }

    return aProperties;
}


awt::Rectangle VCLXAccessibleEdit::getCharacterBounds( sal_Int32 nIndex )
{
    OExternalLockGuard aGuard( this );

    awt::Rectangle aBounds( 0, 0, 0, 0 );
    sal_Int32 nLength = implGetText().getLength();

    if ( !implIsValidRange( nIndex, nIndex, nLength ) )
        throw IndexOutOfBoundsException();

    VclPtr< Control > pControl = GetAs< Control >();
    if ( pControl )
    {
        if ( nIndex == nLength )
        {
            // #108914# calculate virtual bounding rectangle
            for ( sal_Int32 i = 0; i < nLength; ++i )
            {
                tools::Rectangle aRect = pControl->GetCharacterBounds( i );
                sal_Int32 nHeight = aRect.GetHeight();
                if ( aBounds.Height < nHeight )
                {
                    aBounds.Y = aRect.Top();
                    aBounds.Height = nHeight;
                }
                if ( i == nLength - 1 )
                {
                    aBounds.X = aRect.Right() + 1;
                    aBounds.Width = 1;
                }
            }
        }
        else
        {
            aBounds = vcl::unohelper::ConvertToAWTRect(pControl->GetCharacterBounds(nIndex));
        }
    }

    return aBounds;
}


sal_Int32 VCLXAccessibleEdit::getCharacterCount(  )
{
    return VCLXAccessibleTextComponent::getCharacterCount();
}


sal_Int32 VCLXAccessibleEdit::getIndexAtPoint( const awt::Point& aPoint )
{
    return VCLXAccessibleTextComponent::getIndexAtPoint( aPoint );
}


OUString VCLXAccessibleEdit::getSelectedText(  )
{
    return VCLXAccessibleTextComponent::getSelectedText();
}


sal_Int32 VCLXAccessibleEdit::getSelectionStart(  )
{
    return VCLXAccessibleTextComponent::getSelectionStart();
}


sal_Int32 VCLXAccessibleEdit::getSelectionEnd(  )
{
    return VCLXAccessibleTextComponent::getSelectionEnd();
}


sal_Bool VCLXAccessibleEdit::setSelection( sal_Int32 nStartIndex, sal_Int32 nEndIndex )
{
    OExternalLockGuard aGuard( this );

    bool bReturn = false;
    OUString sText( implGetText() );

    if ( !implIsValidRange( nStartIndex, nEndIndex, sText.getLength() ) )
        throw IndexOutOfBoundsException();

    VclPtr< Edit > pEdit = GetAs< Edit >();
    if (pEdit && pEdit->IsEnabled())
    {
        pEdit->SetSelection(Selection(nStartIndex, nEndIndex));
        bReturn = true;
    }

    return bReturn;
}


OUString VCLXAccessibleEdit::getText(  )
{
    return VCLXAccessibleTextComponent::getText();
}


OUString VCLXAccessibleEdit::getTextRange( sal_Int32 nStartIndex, sal_Int32 nEndIndex )
{
    return VCLXAccessibleTextComponent::getTextRange( nStartIndex, nEndIndex );
}


css::accessibility::TextSegment VCLXAccessibleEdit::getTextAtIndex( sal_Int32 nIndex, sal_Int16 aTextType )
{
    OExternalLockGuard aGuard( this );
    // Override general text component behavior: MultiLineEdit can have more text portions
    if ( aTextType == AccessibleTextType::ATTRIBUTE_RUN )
    {
        VclPtr<VclMultiLineEdit> pMulitLineEdit = GetAsDynamic< VclMultiLineEdit >();
        if ( pMulitLineEdit )
        {
            ExtTextEngine* pTextEngine = pMulitLineEdit->GetTextEngine();
            TextPaM aCursor( 0, nIndex );
            TextSegment aResult;
            pTextEngine->GetTextPortionRange( aCursor, aResult.SegmentStart, aResult.SegmentEnd );
            return aResult;
        }
    }

    return VCLXAccessibleTextComponent::getTextAtIndex( nIndex, aTextType );
}


css::accessibility::TextSegment VCLXAccessibleEdit::getTextBeforeIndex( sal_Int32 nIndex, sal_Int16 aTextType )
{
    return VCLXAccessibleTextComponent::getTextBeforeIndex( nIndex, aTextType );
}


css::accessibility::TextSegment VCLXAccessibleEdit::getTextBehindIndex( sal_Int32 nIndex, sal_Int16 aTextType )
{
    return VCLXAccessibleTextComponent::getTextBehindIndex( nIndex, aTextType );
}


sal_Bool VCLXAccessibleEdit::copyText( sal_Int32 nStartIndex, sal_Int32 nEndIndex )
{
    return VCLXAccessibleTextComponent::copyText( nStartIndex, nEndIndex );
}

sal_Bool VCLXAccessibleEdit::scrollSubstringTo( sal_Int32, sal_Int32, AccessibleScrollType )
{
    return false;
}

// XAccessibleEditableText


sal_Bool VCLXAccessibleEdit::cutText( sal_Int32 nStartIndex, sal_Int32 nEndIndex )
{
    return copyText( nStartIndex, nEndIndex ) && deleteText( nStartIndex, nEndIndex );
}


sal_Bool VCLXAccessibleEdit::pasteText( sal_Int32 nIndex )
{
    OExternalLockGuard aGuard( this );

    bool bReturn = false;

    if ( GetWindow() )
    {
        Reference< datatransfer::clipboard::XClipboard > xClipboard = GetWindow()->GetClipboard();
        if ( xClipboard.is() )
        {
            Reference< datatransfer::XTransferable > xDataObj;
            {
                SolarMutexReleaser aReleaser;
                xDataObj = xClipboard->getContents();
            }
            if ( xDataObj.is() )
            {
                datatransfer::DataFlavor aFlavor;
                SotExchange::GetFormatDataFlavor( SotClipboardFormatId::STRING, aFlavor );
                if ( xDataObj->isDataFlavorSupported( aFlavor ) )
                {
                    Any aData = xDataObj->getTransferData( aFlavor );
                    OUString sText;
                    aData >>= sText;
                    bReturn = replaceText( nIndex, nIndex, sText );
                }
            }
        }
    }

    return bReturn;
}


sal_Bool VCLXAccessibleEdit::deleteText( sal_Int32 nStartIndex, sal_Int32 nEndIndex )
{
    return replaceText( nStartIndex, nEndIndex, OUString() );
}


sal_Bool VCLXAccessibleEdit::insertText( const OUString& sText, sal_Int32 nIndex )
{
    return replaceText( nIndex, nIndex, sText );
}


sal_Bool VCLXAccessibleEdit::replaceText( sal_Int32 nStartIndex, sal_Int32 nEndIndex, const OUString& sReplacement )
{
    OExternalLockGuard aGuard( this );

    bool bReturn = false;
    OUString sText( implGetText() );

    if ( !implIsValidRange( nStartIndex, nEndIndex, sText.getLength() ) )
        throw IndexOutOfBoundsException();

    sal_Int32 nMinIndex = std::min( nStartIndex, nEndIndex );
    sal_Int32 nMaxIndex = std::max( nStartIndex, nEndIndex );


    if (isEditable())
    {
        VclPtr<Edit> pEdit = GetAs<Edit>();
        assert(pEdit);
        pEdit->SetText(sText.replaceAt(nMinIndex, nMaxIndex - nMinIndex, sReplacement));
        sal_Int32 nIndex = nMinIndex + sReplacement.getLength();
        setSelection( nIndex, nIndex );
        bReturn = true;
    }

    return bReturn;
}


sal_Bool VCLXAccessibleEdit::setAttributes( sal_Int32 nStartIndex, sal_Int32 nEndIndex, const Sequence<PropertyValue>& )
{
    OExternalLockGuard aGuard( this );

    if ( !implIsValidRange( nStartIndex, nEndIndex, implGetText().getLength() ) )
        throw IndexOutOfBoundsException();

    return false;   // attributes cannot be set for an edit
}


sal_Bool VCLXAccessibleEdit::setText( const OUString& sText )
{
    OExternalLockGuard aGuard( this );

    bool bReturn = false;

    if (isEditable())
    {
        VclPtr<Edit> pEdit = GetAs<Edit>();
        assert(pEdit);
        pEdit->SetText(sText);
        sal_Int32 nSize = sText.getLength();
        pEdit->SetSelection(Selection(nSize, nSize) );
        bReturn = true;
    }

    return bReturn;
}

bool VCLXAccessibleEdit::isComboBoxChild()
{
    Reference<XAccessible> xParent = getAccessibleParent();
    if (xParent.is())
    {
        Reference<XAccessibleContext> xParentContext = xParent->getAccessibleContext();
        if (xParentContext.is() && xParentContext->getAccessibleRole() == AccessibleRole::COMBO_BOX)
            return true;
    }
    return false;
}

bool VCLXAccessibleEdit::isEditable()
{
    VclPtr<Edit> pEdit = GetAs<Edit>();
    return pEdit && !pEdit->IsReadOnly() && pEdit->IsEnabled();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
