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

#include <sal/config.h>

#include <rtl/strbuf.hxx>
#include <sal/log.hxx>

#include <sal/types.h>
#include <comphelper/diagnose_ex.hxx>
#include <vcl/salgtype.hxx>
#include <vcl/event.hxx>
#include <vcl/cursor.hxx>
#include <vcl/svapp.hxx>
#include <vcl/transfer.hxx>
#include <vcl/vclevent.hxx>
#include <vcl/window.hxx>
#include <vcl/syswin.hxx>
#include <vcl/dockwin.hxx>
#include <vcl/wall.hxx>
#include <vcl/toolkit/fixed.hxx>
#include <vcl/toolkit/button.hxx>
#include <vcl/taskpanelist.hxx>
#include <vcl/toolkit/unowrap.hxx>
#include <tools/lazydelete.hxx>
#include <vcl/virdev.hxx>
#include <vcl/settings.hxx>
#include <vcl/sysdata.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/IDialogRenderable.hxx>

#include <vcl/uitest/uiobject.hxx>

#include <ImplOutDevData.hxx>
#include <impfontcache.hxx>
#include <salframe.hxx>
#include <salobj.hxx>
#include <salinst.hxx>
#include <salgdi.hxx>
#include <svdata.hxx>
#include <window.h>
#include <toolbox.h>
#include <brdwin.hxx>
#include <helpwin.hxx>
#include <dndlistenercontainer.hxx>
#include <dndeventdispatcher.hxx>

#include <com/sun/star/accessibility/AccessibleRelation.hpp>
#include <com/sun/star/accessibility/XAccessible.hpp>
#include <com/sun/star/accessibility/XAccessibleEditableText.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/datatransfer/clipboard/XClipboard.hpp>
#include <com/sun/star/datatransfer/dnd/XDragGestureRecognizer.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/rendering/CanvasFactory.hpp>
#include <com/sun/star/rendering/XSpriteCanvas.hpp>
#include <comphelper/configuration.hxx>
#include <comphelper/lok.hxx>
#include <comphelper/processfactory.hxx>
#include <osl/diagnose.h>
#include <tools/debug.hxx>
#include <tools/json_writer.hxx>
#include <boost/property_tree/ptree.hpp>

#include <cassert>
#include <typeinfo>

#ifdef _WIN32 // see #140456#
#include <win/salframe.h>
#endif

#include "impldockingwrapper.hxx"

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::datatransfer::clipboard;
using namespace ::com::sun::star::datatransfer::dnd;

namespace vcl {

Window::Window( WindowType eType )
    : mpWindowImpl(new WindowImpl( *this, eType ))
{
    // true: this outdev will be mirrored if RTL window layout (UI mirroring) is globally active
    mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();
}

Window::Window( vcl::Window* pParent, WinBits nStyle )
    : mpWindowImpl(new WindowImpl( *this, WindowType::WINDOW ))
{
    // true: this outdev will be mirrored if RTL window layout (UI mirroring) is globally active
    mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    ImplInit( pParent, nStyle, nullptr );
}

#if OSL_DEBUG_LEVEL > 0
namespace
{
     OString lcl_createWindowInfo(const vcl::Window* pWindow)
     {
         // skip border windows, they do not carry information that
         // would help with diagnosing the problem
         const vcl::Window* pTempWin( pWindow );
         while ( pTempWin && pTempWin->GetType() == WindowType::BORDERWINDOW ) {
             pTempWin = pTempWin->GetWindow( GetWindowType::FirstChild );
         }
         // check if pTempWin is not null, otherwise use the
         // original address
         if ( pTempWin ) {
             pWindow = pTempWin;
         }

         return OString::Concat(" ") +
            typeid( *pWindow ).name() +
            "(" +
             OUStringToOString(
                 pWindow->GetText(),
                 RTL_TEXTENCODING_UTF8
                 ) +
            ")";
     }
}
#endif

void Window::dispose()
{
    assert( mpWindowImpl );
    assert( !mpWindowImpl->mbInDispose ); // should only be called from disposeOnce()
    assert( (!mpWindowImpl->mpParent ||
             mpWindowImpl->mpParent->mpWindowImpl) &&
            "vcl::Window child should have its parent disposed first" );

    // remove Key and Mouse events issued by Application::PostKey/MouseEvent
    Application::RemoveMouseAndKeyEvents( this );

    // Dispose of the canvas implementation (which, currently, has an
    // own wrapper window as a child to this one.
    GetOutDev()->ImplDisposeCanvas();

    mpWindowImpl->mbInDispose = true;

    CallEventListeners( VclEventId::ObjectDying );

    // do not send child events for frames that were registered as native frames
    if( !IsNativeFrame() && mpWindowImpl->mbReallyVisible )
        if ( ImplIsAccessibleCandidate() && GetAccessibleParentWindow() )
            GetAccessibleParentWindow()->CallEventListeners( VclEventId::WindowChildDestroyed, this );

    // remove associated data structures from dockingmanager
    ImplGetDockingManager()->RemoveWindow( this );

    // remove ownerdraw decorated windows from list in the top-most frame window
    if( (GetStyle() & WB_OWNERDRAWDECORATION) && mpWindowImpl->mbFrame )
    {
        ::std::vector< VclPtr<vcl::Window> >& rList = ImplGetOwnerDrawList();
        auto p = ::std::find( rList.begin(), rList.end(), VclPtr<vcl::Window>(this) );
        if( p != rList.end() )
            rList.erase( p );
    }

    // shutdown drag and drop
    if( mpWindowImpl->mxDNDListenerContainer.is() )
        mpWindowImpl->mxDNDListenerContainer->dispose();

    if( mpWindowImpl->mbFrame && mpWindowImpl->mpFrameData )
    {
        try
        {
            // deregister drop target listener
            if( mpWindowImpl->mpFrameData->mxDropTargetListener.is() )
            {
                Reference< XDragGestureRecognizer > xDragGestureRecognizer(mpWindowImpl->mpFrameData->mxDragSource, UNO_QUERY);
                if( xDragGestureRecognizer.is() )
                {
                    xDragGestureRecognizer->removeDragGestureListener(mpWindowImpl->mpFrameData->mxDropTargetListener);
                }

                mpWindowImpl->mpFrameData->mxDropTarget->removeDropTargetListener( mpWindowImpl->mpFrameData->mxDropTargetListener );
                mpWindowImpl->mpFrameData->mxDropTargetListener.clear();
            }

            // shutdown drag and drop for this frame window
            Reference< XComponent > xComponent( mpWindowImpl->mpFrameData->mxDropTarget, UNO_QUERY );

            // DNDEventDispatcher does not hold a reference of the DropTarget,
            // so it's ok if it does not support XComponent
            if( xComponent.is() )
                xComponent->dispose();
        }
        catch (const Exception&)
        {
            // can be safely ignored here.
        }
    }

    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper( false );
    if ( pWrapper )
        pWrapper->WindowDestroyed( this );

    if ( mpWindowImpl->mxAccessible.is() )
    {
        Reference< XComponent> xC( mpWindowImpl->mxAccessible, UNO_QUERY );
        if ( xC.is() )
            xC->dispose();
        mpWindowImpl->mxAccessible.clear();
    }

    if (mpWindowImpl->mpAccessibleInfos)
        mpWindowImpl->mpAccessibleInfos->xAccessibleParent.clear();

    ImplSVData* pSVData = ImplGetSVData();

    if ( ImplGetSVHelpData().mpHelpWin && (ImplGetSVHelpData().mpHelpWin->GetParent() == this) )
        ImplDestroyHelpWindow( true );

    SAL_WARN_IF(pSVData->mpWinData->mpTrackWin.get() == this, "vcl.window",
                "Window::~Window(): Window is in TrackingMode");
    SAL_WARN_IF(IsMouseCaptured(), "vcl.window",
                "Window::~Window(): Window has the mouse captured");

    // due to old compatibility
    if (pSVData->mpWinData->mpTrackWin == this)
        EndTracking();
    if (IsMouseCaptured())
        ReleaseMouse();

#if OSL_DEBUG_LEVEL > 0
    // always perform these tests in debug builds
    {
        OStringBuffer aErrorStr;
        bool        bError = false;
        vcl::Window*     pTempWin;

        if ( mpWindowImpl->mpFirstChild )
        {
            OStringBuffer aTempStr = "Window (" +
                lcl_createWindowInfo(this) +
                ") with live children destroyed: ";
            pTempWin = mpWindowImpl->mpFirstChild;
            while ( pTempWin )
            {
                aTempStr.append(lcl_createWindowInfo(pTempWin));
                pTempWin = pTempWin->mpWindowImpl->mpNext;
            }
            OSL_FAIL( aTempStr.getStr() );
            Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
        }

        if (mpWindowImpl->mpFrameData != nullptr)
        {
            pTempWin = mpWindowImpl->mpFrameData->mpFirstOverlap;
            while ( pTempWin )
            {
                if ( ImplIsRealParentPath( pTempWin ) )
                {
                    bError = true;
                    aErrorStr.append(lcl_createWindowInfo(pTempWin));
                }
                pTempWin = pTempWin->mpWindowImpl->mpNextOverlap;
            }
            if ( bError )
            {
                OString aTempStr =
                    "Window (" +
                    lcl_createWindowInfo(this) +
                    ") with live SystemWindows destroyed: " +
                    aErrorStr;
                OSL_FAIL(aTempStr.getStr());
                Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
            }
        }

        bError = false;
        pTempWin = pSVData->maFrameData.mpFirstFrame;
        while ( pTempWin )
        {
            if ( ImplIsRealParentPath( pTempWin ) )
            {
                bError = true;
                aErrorStr.append(lcl_createWindowInfo(pTempWin));
            }
            pTempWin = pTempWin->mpWindowImpl->mpFrameData->mpNextFrame;
        }
        if ( bError )
        {
            OString aTempStr =  "Window (" +
                lcl_createWindowInfo(this) +
                ") with live SystemWindows destroyed: " +
                aErrorStr;
            OSL_FAIL( aTempStr.getStr() );
            Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
        }

        if ( mpWindowImpl->mpFirstOverlap )
        {
            OStringBuffer aTempStr = "Window (" +
                lcl_createWindowInfo(this) +
                ") with live SystemWindows destroyed: ";
            pTempWin = mpWindowImpl->mpFirstOverlap;
            while ( pTempWin )
            {
                aTempStr.append(lcl_createWindowInfo(pTempWin));
                pTempWin = pTempWin->mpWindowImpl->mpNext;
            }
            OSL_FAIL( aTempStr.getStr() );
            Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
        }

        vcl::Window* pMyParent = GetParent();
        SystemWindow* pMySysWin = nullptr;

        while ( pMyParent )
        {
            if ( pMyParent->IsSystemWindow() )
            {
                pMySysWin = dynamic_cast<SystemWindow *>(pMyParent);
            }
            pMyParent = pMyParent->GetParent();
        }
        if ( pMySysWin && pMySysWin->ImplIsInTaskPaneList( this ) )
        {
            OString aTempStr = "Window (" +
                lcl_createWindowInfo(this) +
                ") still in TaskPanelList!";
            OSL_FAIL( aTempStr.getStr() );
            Application::Abort(OStringToOUString(aTempStr, RTL_TEXTENCODING_UTF8));
        }
    }
#endif

    if( mpWindowImpl->mbIsInTaskPaneList )
    {
        vcl::Window* pMyParent = GetParent();
        SystemWindow* pMySysWin = nullptr;

        while ( pMyParent )
        {
            if ( pMyParent->IsSystemWindow() )
            {
                pMySysWin = dynamic_cast<SystemWindow *>(pMyParent);
            }
            pMyParent = pMyParent->GetParent();
        }
        if ( pMySysWin && pMySysWin->ImplIsInTaskPaneList( this ) )
        {
            pMySysWin->GetTaskPaneList()->RemoveWindow( this );
        }
        else
        {
            SAL_WARN( "vcl", "Window (" << GetText() << ") not found in TaskPanelList");
        }
    }

    // remove from size-group if necessary
    remove_from_all_size_groups();

    // clear mnemonic labels
    std::vector<VclPtr<FixedText> > aMnemonicLabels(list_mnemonic_labels());
    for (auto const& mnemonicLabel : aMnemonicLabels)
    {
        remove_mnemonic_label(mnemonicLabel);
    }

    // hide window in order to trigger the Paint-Handling
    Hide();

    // EndExtTextInputMode
    if (pSVData->mpWinData->mpExtTextInputWin == this)
    {
        EndExtTextInput();
        if (pSVData->mpWinData->mpExtTextInputWin == this)
            pSVData->mpWinData->mpExtTextInputWin = nullptr;
    }

    // check if the focus window is our child
    bool bHasFocusedChild = false;
    if (pSVData->mpWinData->mpFocusWin && ImplIsRealParentPath(pSVData->mpWinData->mpFocusWin))
    {
        // #122232#, this must not happen and is an application bug ! but we try some cleanup to hopefully avoid crashes, see below
        bHasFocusedChild = true;
#if OSL_DEBUG_LEVEL > 0
        OUString aTempStr = "Window (" + GetText() +
                ") with focused child window destroyed ! THIS WILL LEAD TO CRASHES AND MUST BE FIXED !";
        SAL_WARN( "vcl", aTempStr );
        Application::Abort(aTempStr);
#endif
    }

    // if we get focus pass focus to another window
    vcl::Window* pOverlapWindow = ImplGetFirstOverlapWindow();
    if (pSVData->mpWinData->mpFocusWin == this
        || bHasFocusedChild) // #122232#, see above, try some cleanup
    {
        if ( mpWindowImpl->mbFrame )
        {
            pSVData->mpWinData->mpFocusWin = nullptr;
            pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;
        }
        else
        {
            vcl::Window* pParent = GetParent();
            vcl::Window* pBorderWindow = mpWindowImpl->mpBorderWindow;
        // when windows overlap, give focus to the parent
        // of the next FrameWindow
            if ( pBorderWindow )
            {
                if ( pBorderWindow->ImplIsOverlapWindow() )
                    pParent = pBorderWindow->mpWindowImpl->mpOverlapWindow;
            }
            else if ( ImplIsOverlapWindow() )
                pParent = mpWindowImpl->mpOverlapWindow;

            if ( pParent && pParent->IsEnabled() && pParent->IsInputEnabled() && ! pParent->IsInModalMode() )
                pParent->GrabFocus();
            else
                mpWindowImpl->mpFrameWindow->GrabFocus();

            // If the focus was set back to 'this' set it to nothing
            if (pSVData->mpWinData->mpFocusWin == this)
            {
                pSVData->mpWinData->mpFocusWin = nullptr;
                pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;
            }
        }
    }

    if ( pOverlapWindow != nullptr &&
         pOverlapWindow->mpWindowImpl->mpLastFocusWindow == this )
        pOverlapWindow->mpWindowImpl->mpLastFocusWindow = nullptr;

    // reset hint for DefModalDialogParent
    if( pSVData->maFrameData.mpActiveApplicationFrame == this )
        pSVData->maFrameData.mpActiveApplicationFrame = nullptr;

    // reset hint of what was the last wheeled window
    if (pSVData->mpWinData->mpLastWheelWindow == this)
        pSVData->mpWinData->mpLastWheelWindow = nullptr;

    // reset marked windows
    if ( mpWindowImpl->mpFrameData != nullptr )
    {
        if ( mpWindowImpl->mpFrameData->mpFocusWin == this )
            mpWindowImpl->mpFrameData->mpFocusWin = nullptr;
        if ( mpWindowImpl->mpFrameData->mpMouseMoveWin == this )
            mpWindowImpl->mpFrameData->mpMouseMoveWin = nullptr;
        if ( mpWindowImpl->mpFrameData->mpMouseDownWin == this )
            mpWindowImpl->mpFrameData->mpMouseDownWin = nullptr;
    }

    // reset Deactivate-Window
    if (pSVData->mpWinData->mpLastDeacWin == this)
        pSVData->mpWinData->mpLastDeacWin = nullptr;

    if ( mpWindowImpl->mbFrame && mpWindowImpl->mpFrameData )
    {
        if ( mpWindowImpl->mpFrameData->mnFocusId )
            Application::RemoveUserEvent( mpWindowImpl->mpFrameData->mnFocusId );
        mpWindowImpl->mpFrameData->mnFocusId = nullptr;
        if ( mpWindowImpl->mpFrameData->mnMouseMoveId )
            Application::RemoveUserEvent( mpWindowImpl->mpFrameData->mnMouseMoveId );
        mpWindowImpl->mpFrameData->mnMouseMoveId = nullptr;
    }

    // release SalGraphics
    VclPtr<OutputDevice> pOutDev = GetOutDev();
    pOutDev->ReleaseGraphics();

    // remove window from the lists
    ImplRemoveWindow( true );

    // de-register as "top window child" at our parent, if necessary
    if ( mpWindowImpl->mbFrame )
    {
        bool bIsTopWindow
            = mpWindowImpl->mpWinData && (mpWindowImpl->mpWinData->mnIsTopWindow == 1);
        if ( mpWindowImpl->mpRealParent && bIsTopWindow )
        {
            ImplWinData* pParentWinData = mpWindowImpl->mpRealParent->ImplGetWinData();

            auto myPos = ::std::find( pParentWinData->maTopWindowChildren.begin(),
                pParentWinData->maTopWindowChildren.end(), VclPtr<vcl::Window>(this) );
            SAL_WARN_IF( myPos == pParentWinData->maTopWindowChildren.end(), "vcl.window", "Window::~Window: inconsistency in top window chain!" );
            if ( myPos != pParentWinData->maTopWindowChildren.end() )
                pParentWinData->maTopWindowChildren.erase( myPos );
        }
    }

    mpWindowImpl->mpWinData.reset();

    // remove BorderWindow or Frame window data
    mpWindowImpl->mpBorderWindow.disposeAndClear();
    if ( mpWindowImpl->mbFrame )
    {
        if ( pSVData->maFrameData.mpFirstFrame == this )
            pSVData->maFrameData.mpFirstFrame = mpWindowImpl->mpFrameData->mpNextFrame;
        else
        {
            sal_Int32 nWindows = 0;
            vcl::Window* pSysWin = pSVData->maFrameData.mpFirstFrame;
            while ( pSysWin && pSysWin->mpWindowImpl->mpFrameData->mpNextFrame.get() != this )
            {
                pSysWin = pSysWin->mpWindowImpl->mpFrameData->mpNextFrame;
                nWindows++;
            }

            if ( pSysWin )
            {
                assert (mpWindowImpl->mpFrameData->mpNextFrame.get() != pSysWin);
                pSysWin->mpWindowImpl->mpFrameData->mpNextFrame = mpWindowImpl->mpFrameData->mpNextFrame;
            }
            else // if it is not in the list, we can't remove it.
                SAL_WARN("vcl.window", "Window " << this << " marked as frame window, "
                         "is missing from list of " << nWindows << " frames");
        }
        if (mpWindowImpl->mpFrame) // otherwise exception during init
        {
            mpWindowImpl->mpFrame->SetCallback( nullptr, nullptr );
            pSVData->mpDefInst->DestroyFrame( mpWindowImpl->mpFrame );
        }
        assert (mpWindowImpl->mpFrameData->mnFocusId == nullptr);
        assert (mpWindowImpl->mpFrameData->mnMouseMoveId == nullptr);

        mpWindowImpl->mpFrameData->mpBuffer.disposeAndClear();
        delete mpWindowImpl->mpFrameData;
        mpWindowImpl->mpFrameData = nullptr;
    }

    if (mpWindowImpl->mxWindowPeer)
        mpWindowImpl->mxWindowPeer->dispose();

    // should be the last statements
    mpWindowImpl.reset();

    pOutDev.disposeAndClear();
    // just to make loplugin:vclwidgets happy
    VclReferenceBase::dispose();
}

Window::~Window()
{
    disposeOnce();
}

// We will eventually being removing the inheritance of OutputDevice
// from Window. It will be replaced with a transient relationship such
// that the OutputDevice is only live for the scope of the Paint method.
// In the meantime this can help move us towards a Window use an
// OutputDevice, not being one.

::OutputDevice const* Window::GetOutDev() const
{
    return mpWindowImpl ? mpWindowImpl->mxOutDev.get() : nullptr;
}

::OutputDevice* Window::GetOutDev()
{
    return mpWindowImpl ? mpWindowImpl->mxOutDev.get() : nullptr;
}

Color WindowOutputDevice::GetBackgroundColor() const
{
    return mxOwnerWindow->GetDisplayBackground().GetColor();
}

bool WindowOutputDevice::CanEnableNativeWidget() const
{
    return mxOwnerWindow->IsNativeWidgetEnabled();
}

} /* namespace vcl */

WindowImpl::WindowImpl( vcl::Window& rWindow, WindowType eType )
{
    mxOutDev = VclPtr<vcl::WindowOutputDevice>::Create(rWindow);
    maZoom                              = Fraction( 1, 1 );
    mfPartialScrollX                    = 0.0;
    mfPartialScrollY                    = 0.0;
    maWinRegion                         = vcl::Region(true);
    maWinClipRegion                     = vcl::Region(true);
    mpWinData                           = nullptr;                      // Extra Window Data, that we don't need for all windows
    mpFrameData                         = nullptr;                      // Frame Data
    mpFrame                             = nullptr;                      // Pointer to frame window
    mpSysObj                            = nullptr;
    mpFrameWindow                       = nullptr;                      // window to top level parent (same as frame window)
    mpOverlapWindow                     = nullptr;                      // first overlap parent
    mpBorderWindow                      = nullptr;                      // Border-Window
    mpClientWindow                      = nullptr;                      // Client-Window of a FrameWindow
    mpParent                            = nullptr;                      // parent (incl. BorderWindow)
    mpRealParent                        = nullptr;                      // real parent (excl. BorderWindow)
    mpFirstChild                        = nullptr;                      // first child window
    mpLastChild                         = nullptr;                      // last child window
    mpFirstOverlap                      = nullptr;                      // first overlap window (only set in overlap windows)
    mpLastOverlap                       = nullptr;                      // last overlap window (only set in overlap windows)
    mpPrev                              = nullptr;                      // prev window
    mpNext                              = nullptr;                      // next window
    mpNextOverlap                       = nullptr;                      // next overlap window of frame
    mpLastFocusWindow                   = nullptr;                      // window for focus restore
    mpDlgCtrlDownWindow                 = nullptr;                      // window for dialog control
    mnEventListenersIteratingCount = 0;
    mnChildEventListenersIteratingCount = 0;
    mpCursor                            = nullptr;                      // cursor
    maPointer                           = PointerStyle::Arrow;
    mpVCLXWindow                        = nullptr;
    mpAccessibleInfos                   = nullptr;
    maControlForeground                 = COL_TRANSPARENT;  // no foreground set
    maControlBackground                 = COL_TRANSPARENT;  // no background set
    mnLeftBorder                        = 0;                         // width of left border
    mnTopBorder                         = 0;                         // width of top border
    mnRightBorder                       = 0;                         // width of right border
    mnBottomBorder                      = 0;                         // width of bottom border
    mnWidthRequest                      = -1;                        // width request
    mnHeightRequest                     = -1;                        // height request
    mnOptimalWidthCache                 = -1;                        // optimal width cache
    mnOptimalHeightCache                = -1;                        // optimal height cache
    mnX                                 = 0;                         // X-Position to Parent
    mnY                                 = 0;                         // Y-Position to Parent
    mnAbsScreenX                        = 0;                         // absolute X-position on screen, used for RTL window positioning
    mpChildClipRegion                   = nullptr;                      // Child-Clip-Region when ClipChildren
    mpPaintRegion                       = nullptr;                      // Paint-ClipRegion
    mnStyle                             = 0;                         // style (init in ImplInitWindow)
    mnPrevStyle                         = 0;                         // prevstyle (set in SetStyle)
    mnExtendedStyle                     = WindowExtendedStyle::NONE; // extended style (init in ImplInitWindow)
    meType                              = eType;                     // type
    mnGetFocusFlags                     = GetFocusFlags::NONE;       // Flags for GetFocus()-Call
    mnWaitCount                         = 0;                         // Wait-Count (>1 == "wait" mouse pointer)
    mnPaintFlags                        = ImplPaintFlags::NONE;      // Flags for ImplCallPaint
    mnParentClipMode                    = ParentClipMode::NONE;      // Flags for Parent-ClipChildren-Mode
    mnActivateMode                      = ActivateModeFlags::NONE;   // Will be converted in System/Overlap-Windows
    mnDlgCtrlFlags                      = DialogControlFlags::NONE;  // DialogControl-Flags
    meAlwaysInputMode                   = AlwaysInputNone;           // AlwaysEnableInput not called
    meHalign                            = VclAlign::Fill;
    meValign                            = VclAlign::Fill;
    mePackType                          = VclPackType::Start;
    mnPadding                           = 0;
    mnGridHeight                        = 1;
    mnGridLeftAttach                    = -1;
    mnGridTopAttach                     = -1;
    mnGridWidth                         = 1;
    mnBorderWidth                       = 0;
    mnMarginLeft                        = 0;
    mnMarginRight                       = 0;
    mnMarginTop                         = 0;
    mnMarginBottom                      = 0;
    mbFrame                             = false;                     // true: Window is a frame window
    mbBorderWin                         = false;                     // true: Window is a border window
    mbOverlapWin                        = false;                     // true: Window is an overlap window
    mbSysWin                            = false;                     // true: SystemWindow is the base class
    mbDialog                            = false;                     // true: Dialog is the base class
    mbDockWin                           = false;                     // true: DockingWindow is the base class
    mbFloatWin                          = false;                     // true: FloatingWindow is the base class
    mbPushButton                        = false;                     // true: PushButton is the base class
    mbToolBox                           = false;                     // true: ToolBox is the base class
    mbMenuFloatingWindow                = false;                     // true: MenuFloatingWindow is the base class
    mbSplitter                          = false;                     // true: Splitter is the base class
    mbVisible                           = false;                     // true: Show( true ) called
    mbOverlapVisible                    = false;                     // true: Hide called for visible window from ImplHideAllOverlapWindow()
    mbDisabled                          = false;                     // true: Enable( false ) called
    mbInputDisabled                     = false;                     // true: EnableInput( false ) called
    mbNoUpdate                          = false;                     // true: SetUpdateMode( false ) called
    mbNoParentUpdate                    = false;                     // true: SetParentUpdateMode( false ) called
    mbActive                            = false;                     // true: Window Active
    mbReallyVisible                     = false;                     // true: this and all parents to an overlapped window are visible
    mbReallyShown                       = false;                     // true: this and all parents to an overlapped window are shown
    mbInInitShow                        = false;                     // true: we are in InitShow
    mbChildPtrOverwrite                 = false;                     // true: PointerStyle overwrites Child-Pointer
    mbNoPtrVisible                      = false;                     // true: ShowPointer( false ) called
    mbPaintFrame                        = false;                     // true: Paint is visible, but not painted
    mbInPaint                           = false;                     // true: Inside PaintHdl
    mbMouseButtonDown                   = false;                     // true: BaseMouseButtonDown called
    mbMouseButtonUp                     = false;                     // true: BaseMouseButtonUp called
    mbKeyInput                          = false;                     // true: BaseKeyInput called
    mbKeyUp                             = false;                     // true: BaseKeyUp called
    mbCommand                           = false;                     // true: BaseCommand called
    mbDefPos                            = true;                      // true: Position is not Set
    mbDefSize                           = true;                      // true: Size is not Set
    mbCallMove                          = true;                      // true: Move must be called by Show
    mbCallResize                        = true;                      // true: Resize must be called by Show
    mbWaitSystemResize                  = true;                      // true: Wait for System-Resize
    mbInitWinClipRegion                 = true;                      // true: Calc Window Clip Region
    mbInitChildRegion                   = false;                     // true: InitChildClipRegion
    mbWinRegion                         = false;                     // true: Window Region
    mbClipChildren                      = false;                     // true: Child-window should be clipped
    mbClipSiblings                      = false;                     // true: Adjacent Child-window should be clipped
    mbChildTransparent                  = false;                     // true: Child-windows are allowed to switch to transparent (incl. Parent-CLIPCHILDREN)
    mbPaintTransparent                  = false;                     // true: Paints should be executed on the Parent
    mbMouseTransparent                  = false;                     // true: Window is transparent for Mouse
    mbDlgCtrlStart                      = false;                     // true: From here on own Dialog-Control
    mbFocusVisible                      = false;                     // true: Focus Visible
    mbUseNativeFocus                    = false;
    mbNativeFocusVisible                = false;                     // true: native Focus Visible
    mbInShowFocus                       = false;                     // prevent recursion
    mbInHideFocus                       = false;                     // prevent recursion
    mbTrackVisible                      = false;                     // true: Tracking Visible
    mbControlForeground                 = false;                     // true: Foreground-Property set
    mbControlBackground                 = false;                     // true: Background-Property set
    mbAlwaysOnTop                       = false;                     // true: always visible for all others windows
    mbCompoundControl                   = false;                     // true: Composite Control => Listener...
    mbCompoundControlHasFocus           = false;                     // true: Composite Control has focus somewhere
    mbPaintDisabled                     = false;                     // true: Paint should not be executed
    mbAllResize                         = false;                     // true: Also sent ResizeEvents with 0,0
    mbInDispose                         = false;                     // true: We're still in Window::dispose()
    mbExtTextInput                      = false;                     // true: ExtTextInput-Mode is active
    mbInFocusHdl                        = false;                     // true: Within GetFocus-Handler
    mbCreatedWithToolkit                = false;
    mbSuppressAccessibilityEvents       = false;                     // true: do not send any accessibility events
    mbDrawSelectionBackground           = false;                     // true: draws transparent window background to indicate (toolbox) selection
    mbIsInTaskPaneList                  = false;                     // true: window was added to the taskpanelist in the topmost system window
    mnNativeBackground                  = ControlPart::NONE;         // initialize later, depends on type
    mbHelpTextDynamic                   = false;                     // true: append help id in HELP_DEBUG case
    mbFakeFocusSet                      = false;                     // true: pretend as if the window has focus.
    mbHexpand                           = false;
    mbVexpand                           = false;
    mbExpand                            = false;
    mbFill                              = true;
    mbSecondary                         = false;
    mbNonHomogeneous                    = false;
    static bool bDoubleBuffer = getenv("VCL_DOUBLEBUFFERING_FORCE_ENABLE");
    mbDoubleBufferingRequested = bDoubleBuffer; // when we are not sure, assume it cannot do double-buffering via RenderContext
    mpLOKNotifier                       = nullptr;
    mnLOKWindowId                       = 0;
    mbUseFrameData                      = false;
}

WindowImpl::~WindowImpl()
{
    mpChildClipRegion.reset();
    mpAccessibleInfos.reset();
}

ImplWinData::ImplWinData() :
    mnCursorExtWidth(0),
    mbVertical(false),
    mnCompositionCharRects(0),
    mnTrackFlags(ShowTrackFlags::NONE),
    mnIsTopWindow(sal_uInt16(~0)), // not initialized yet, 0/1 will indicate TopWindow (see IsTopWindow())
    mbMouseOver(false),
    mbEnableNativeWidget(false)
{
}

ImplWinData::~ImplWinData()
{
    mpCompositionCharRects.reset();
}

ImplFrameData::ImplFrameData( vcl::Window *pWindow )
    : maPaintIdle( "vcl::Window maPaintIdle" ),
      maResizeIdle( "vcl::Window maResizeIdle" )
{
    ImplSVData* pSVData = ImplGetSVData();
    assert (pSVData->maFrameData.mpFirstFrame.get() != pWindow);
    mpNextFrame        = pSVData->maFrameData.mpFirstFrame;
    pSVData->maFrameData.mpFirstFrame = pWindow;
    mpFirstOverlap     = nullptr;
    mpFocusWin         = nullptr;
    mpMouseMoveWin     = nullptr;
    mpMouseDownWin     = nullptr;
    mpTrackWin         = nullptr;
    mxFontCollection   = pSVData->maGDIData.mxScreenFontList;
    mxFontCache        = pSVData->maGDIData.mxScreenFontCache;
    mnFocusId          = nullptr;
    mnMouseMoveId      = nullptr;
    mnLastMouseX       = -32767;
    mnLastMouseY       = -32767;
    mnBeforeLastMouseX = -32767;
    mnBeforeLastMouseY = -32767;
    mnFirstMouseX      = -32767;
    mnFirstMouseY      = -32767;
    mnLastMouseWinX    = -32767;
    mnLastMouseWinY    = -32767;
    mnModalMode        = 0;
    mnMouseDownTime    = 0;
    mnClickCount       = 0;
    mnFirstMouseCode   = 0;
    mnMouseCode        = 0;
    mnMouseMode        = MouseEventModifiers::NONE;
    mbHasFocus         = false;
    mbInMouseMove      = false;
    mbMouseIn          = false;
    mbStartDragCalled  = false;
    mbNeedSysWindow    = false;
    mbMinimized        = false;
    mbStartFocusState  = false;
    mbInSysObjFocusHdl = false;
    mbInSysObjToTopHdl = false;
    mbSysObjFocus      = false;
    maPaintIdle.SetPriority( TaskPriority::REPAINT );
    maPaintIdle.SetInvokeHandler( LINK( pWindow, vcl::Window, ImplHandlePaintHdl ) );
    maResizeIdle.SetPriority( TaskPriority::RESIZE );
    maResizeIdle.SetInvokeHandler( LINK( pWindow, vcl::Window, ImplHandleResizeTimerHdl ) );
    mbInternalDragGestureRecognizer = false;
    mbDragging = false;
    mbInBufferedPaint = false;
    mnDPIX = 96;
    mnDPIY = 96;
    mnTouchPanPositionX = -1;
    mnTouchPanPositionY = -1;
}

namespace vcl {

bool WindowOutputDevice::AcquireGraphics() const
{
    DBG_TESTSOLARMUTEX();

    if (isDisposed())
        return false;

    if (mpGraphics)
        return true;

    mbInitLineColor     = true;
    mbInitFillColor     = true;
    mbInitFont          = true;
    mbInitTextColor     = true;
    mbInitClipRegion    = true;

    ImplSVData* pSVData = ImplGetSVData();

    mpGraphics = mxOwnerWindow->mpWindowImpl->mpFrame->AcquireGraphics();
    // try harder if no wingraphics was available directly
    if ( !mpGraphics )
    {
        // find another output device in the same frame
        vcl::WindowOutputDevice* pReleaseOutDev = pSVData->maGDIData.mpLastWinGraphics.get();
        while ( pReleaseOutDev )
        {
            if ( pReleaseOutDev->mxOwnerWindow && pReleaseOutDev->mxOwnerWindow->mpWindowImpl->mpFrame == mxOwnerWindow->mpWindowImpl->mpFrame )
                break;
            pReleaseOutDev = static_cast<vcl::WindowOutputDevice*>(pReleaseOutDev->mpPrevGraphics.get());
        }

        if ( pReleaseOutDev )
        {
            // steal the wingraphics from the other outdev
            mpGraphics = pReleaseOutDev->mpGraphics;
            pReleaseOutDev->ReleaseGraphics( false );
        }
        else
        {
            // if needed retry after releasing least recently used wingraphics
            while ( !mpGraphics )
            {
                if ( !pSVData->maGDIData.mpLastWinGraphics )
                    break;
                pSVData->maGDIData.mpLastWinGraphics->ReleaseGraphics();
                mpGraphics = mxOwnerWindow->mpWindowImpl->mpFrame->AcquireGraphics();
            }
        }
    }

    if ( mpGraphics )
    {
        // update global LRU list of wingraphics
        mpNextGraphics = pSVData->maGDIData.mpFirstWinGraphics.get();
        pSVData->maGDIData.mpFirstWinGraphics = const_cast<vcl::WindowOutputDevice*>(this);
        if ( mpNextGraphics )
            mpNextGraphics->mpPrevGraphics = const_cast<vcl::WindowOutputDevice*>(this);
        if ( !pSVData->maGDIData.mpLastWinGraphics )
            pSVData->maGDIData.mpLastWinGraphics = const_cast<vcl::WindowOutputDevice*>(this);

        mpGraphics->SetXORMode( (RasterOp::Invert == meRasterOp) || (RasterOp::Xor == meRasterOp), RasterOp::Invert == meRasterOp );
        mpGraphics->setAntiAlias(bool(mnAntialiasing & AntialiasingFlags::Enable));
    }

    return mpGraphics != nullptr;
}

void WindowOutputDevice::ReleaseGraphics( bool bRelease )
{
    DBG_TESTSOLARMUTEX();

    if ( !mpGraphics )
        return;

    // release the fonts of the physically released graphics device
    if( bRelease )
        ImplReleaseFonts();

    ImplSVData* pSVData = ImplGetSVData();

    vcl::Window* pWindow = mxOwnerWindow.get();
    if (!pWindow)
        return;

    if ( bRelease )
        pWindow->mpWindowImpl->mpFrame->ReleaseGraphics( mpGraphics );
    // remove from global LRU list of window graphics
    if ( mpPrevGraphics )
        mpPrevGraphics->mpNextGraphics = mpNextGraphics;
    else
        pSVData->maGDIData.mpFirstWinGraphics = static_cast<vcl::WindowOutputDevice*>(mpNextGraphics.get());
    if ( mpNextGraphics )
        mpNextGraphics->mpPrevGraphics = mpPrevGraphics;
    else
        pSVData->maGDIData.mpLastWinGraphics = static_cast<vcl::WindowOutputDevice*>(mpPrevGraphics.get());

    mpGraphics      = nullptr;
    mpPrevGraphics  = nullptr;
    mpNextGraphics  = nullptr;
}

static sal_Int32 CountDPIScaleFactor(sal_Int32 nDPI)
{
#ifndef MACOSX
    // Setting of HiDPI is unfortunately all only a heuristic; and to add
    // insult to an injury, the system is constantly lying to us about
    // the DPI and whatnot
    // eg. fdo#77059 - set the value from which we do consider the
    // screen HiDPI to greater than 168
    if (nDPI > 216)      // 96 * 2   + 96 / 4
        return 250;
    else if (nDPI > 168) // 96 * 2   - 96 / 4
        return 200;
    else if (nDPI > 120) // 96 * 1.5 - 96 / 4
        return 150;
#else
    (void)nDPI;
#endif

    return 100;
}

void Window::ImplInit( vcl::Window* pParent, WinBits nStyle, SystemParentData* pSystemParentData )
{
    SAL_WARN_IF( !mpWindowImpl->mbFrame && !pParent && GetType() != WindowType::FIXEDIMAGE, "vcl.window",
        "Window::Window(): pParent == NULL" );

    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window*     pRealParent = pParent;

    // inherit 3D look
    if ( !mpWindowImpl->mbOverlapWin && pParent && (pParent->GetStyle() & WB_3DLOOK) )
        nStyle |= WB_3DLOOK;

    // create border window if necessary
    if ( !mpWindowImpl->mbFrame && !mpWindowImpl->mbBorderWin && !mpWindowImpl->mpBorderWindow
         && (nStyle & (WB_BORDER | WB_SYSTEMCHILDWINDOW) ) )
    {
        BorderWindowStyle nBorderTypeStyle = BorderWindowStyle::NONE;
        if( nStyle & WB_SYSTEMCHILDWINDOW )
        {
            // handle WB_SYSTEMCHILDWINDOW
            // these should be analogous to a top level frame; meaning they
            // should have a border window with style BorderWindowStyle::Frame
            // which controls their size
            nBorderTypeStyle |= BorderWindowStyle::Frame;
            nStyle |= WB_BORDER;
        }
        VclPtrInstance<ImplBorderWindow> pBorderWin( pParent, nStyle & (WB_BORDER | WB_DIALOGCONTROL | WB_NODIALOGCONTROL), nBorderTypeStyle );
        static_cast<vcl::Window*>(pBorderWin)->mpWindowImpl->mpClientWindow = this;
        pBorderWin->GetBorder( mpWindowImpl->mnLeftBorder, mpWindowImpl->mnTopBorder, mpWindowImpl->mnRightBorder, mpWindowImpl->mnBottomBorder );
        mpWindowImpl->mpBorderWindow  = pBorderWin;
        pParent = mpWindowImpl->mpBorderWindow;
    }
    else if( !mpWindowImpl->mbFrame && ! pParent )
    {
        mpWindowImpl->mbOverlapWin  = true;
        mpWindowImpl->mbFrame = true;
    }

    // insert window in list
    ImplInsertWindow( pParent );
    mpWindowImpl->mnStyle = nStyle;

    if( pParent && ! mpWindowImpl->mbFrame )
        mpWindowImpl->mxOutDev->mbEnableRTL = AllSettings::GetLayoutRTL();

    // test for frame creation
    if ( mpWindowImpl->mbFrame )
    {
        // create frame
        SalFrameStyleFlags nFrameStyle = SalFrameStyleFlags::NONE;

        if ( nStyle & WB_MOVEABLE )
            nFrameStyle |= SalFrameStyleFlags::MOVEABLE;
        if ( nStyle & WB_SIZEABLE )
            nFrameStyle |= SalFrameStyleFlags::SIZEABLE;
        if ( nStyle & WB_CLOSEABLE )
            nFrameStyle |= SalFrameStyleFlags::CLOSEABLE;
        if ( nStyle & WB_APP )
            nFrameStyle |= SalFrameStyleFlags::DEFAULT;
        // check for undecorated floating window
        if( // 1. floating windows that are not moveable/sizeable (only closeable allowed)
            ( !(nFrameStyle & ~SalFrameStyleFlags::CLOSEABLE) &&
            ( mpWindowImpl->mbFloatWin || ((GetType() == WindowType::BORDERWINDOW) && static_cast<ImplBorderWindow*>(this)->mbFloatWindow) || (nStyle & WB_SYSTEMFLOATWIN) ) ) ||
            // 2. borderwindows of floaters with ownerdraw decoration
            ((GetType() == WindowType::BORDERWINDOW) && static_cast<ImplBorderWindow*>(this)->mbFloatWindow && (nStyle & WB_OWNERDRAWDECORATION) ) )
        {
            nFrameStyle = SalFrameStyleFlags::FLOAT;
            if( nStyle & WB_OWNERDRAWDECORATION )
                nFrameStyle |= SalFrameStyleFlags::OWNERDRAWDECORATION | SalFrameStyleFlags::NOSHADOW;
        }
        else if( mpWindowImpl->mbFloatWin )
            nFrameStyle |= SalFrameStyleFlags::TOOLWINDOW;

        if( nStyle & WB_INTROWIN )
            nFrameStyle |= SalFrameStyleFlags::INTRO;
        if( nStyle & WB_TOOLTIPWIN )
            nFrameStyle |= SalFrameStyleFlags::TOOLTIP;

        if( nStyle & WB_NOSHADOW )
            nFrameStyle |= SalFrameStyleFlags::NOSHADOW;

        if( nStyle & WB_SYSTEMCHILDWINDOW )
            nFrameStyle |= SalFrameStyleFlags::SYSTEMCHILD;

        switch (mpWindowImpl->meType)
        {
            case WindowType::DIALOG:
            case WindowType::TABDIALOG:
            case WindowType::MODELESSDIALOG:
            case WindowType::MESSBOX:
            case WindowType::INFOBOX:
            case WindowType::WARNINGBOX:
            case WindowType::ERRORBOX:
            case WindowType::QUERYBOX:
                nFrameStyle |= SalFrameStyleFlags::DIALOG;
                break;
            default:
                break;
        }

        // tdf#144624 for the DefaultWindow, which is never visible, don't
        // create an icon for it so construction of a DefaultWindow cannot
        // trigger creation of a VirtualDevice which itself requires a
        // DefaultWindow to exist
        if( nStyle & WB_DEFAULTWIN )
            nFrameStyle |= SalFrameStyleFlags::NOICON;

        SalFrame* pParentFrame = nullptr;
        if ( pParent )
            pParentFrame = pParent->mpWindowImpl->mpFrame;
        SalFrame* pFrame;
        if ( pSystemParentData )
            pFrame = pSVData->mpDefInst->CreateChildFrame( pSystemParentData, nFrameStyle | SalFrameStyleFlags::PLUG );
        else
            pFrame = pSVData->mpDefInst->CreateFrame( pParentFrame, nFrameStyle );
        if ( !pFrame )
        {
            // do not abort but throw an exception, may be the current thread terminates anyway (plugin-scenario)
            throw RuntimeException(
                u"Could not create system window!"_ustr,
                Reference< XInterface >() );
        }

        pFrame->SetCallback( this, ImplWindowFrameProc );

        // set window frame data
        mpWindowImpl->mpFrameData     = new ImplFrameData( this );
        mpWindowImpl->mpFrame         = pFrame;
        mpWindowImpl->mpFrameWindow   = this;
        mpWindowImpl->mpOverlapWindow = this;

        if (!(nStyle & WB_DEFAULTWIN) && mpWindowImpl->mbDoubleBufferingRequested)
            RequestDoubleBuffering(true);

        if ( pRealParent && IsTopWindow() )
        {
            ImplWinData* pParentWinData = pRealParent->ImplGetWinData();
            pParentWinData->maTopWindowChildren.emplace_back(this );
        }
    }

    // init data
    mpWindowImpl->mpRealParent = pRealParent;

    // #99318: make sure fontcache and list is available before call to SetSettings
    mpWindowImpl->mxOutDev->mxFontCollection = mpWindowImpl->mpFrameData->mxFontCollection;
    mpWindowImpl->mxOutDev->mxFontCache = mpWindowImpl->mpFrameData->mxFontCache;

    if ( mpWindowImpl->mbFrame )
    {
        if ( pParent )
        {
            mpWindowImpl->mpFrameData->mnDPIX     = pParent->mpWindowImpl->mpFrameData->mnDPIX;
            mpWindowImpl->mpFrameData->mnDPIY     = pParent->mpWindowImpl->mpFrameData->mnDPIY;
        }
        else
        {
            if (auto* pGraphics = GetOutDev()->GetGraphics())
            {
                pGraphics->GetResolution(mpWindowImpl->mpFrameData->mnDPIX,
                                         mpWindowImpl->mpFrameData->mnDPIY);
            }
        }

        // add ownerdraw decorated frame windows to list in the top-most frame window
        // so they can be hidden on lose focus
        if( nStyle & WB_OWNERDRAWDECORATION )
            ImplGetOwnerDrawList().emplace_back(this );

        // delay settings initialization until first "real" frame
        // this relies on the IntroWindow not needing any system settings
        if ( !pSVData->maAppData.mbSettingsInit &&
             ! (nStyle & (WB_INTROWIN|WB_DEFAULTWIN))
             )
        {
            // side effect: ImplUpdateGlobalSettings does an ImplGetFrame()->UpdateSettings
            ImplUpdateGlobalSettings( *pSVData->maAppData.mxSettings );
            mpWindowImpl->mxOutDev->SetSettings( *pSVData->maAppData.mxSettings );
            pSVData->maAppData.mbSettingsInit = true;
        }

        // If we create a Window with default size, query this
        // size directly, because we want resize all Controls to
        // the correct size before we display the window
        if ( nStyle & (WB_MOVEABLE | WB_SIZEABLE | WB_APP) )
            mpWindowImpl->mpFrame->GetClientSize( mpWindowImpl->mxOutDev->mnOutWidth, mpWindowImpl->mxOutDev->mnOutHeight );
    }
    else
    {
        if ( pParent )
        {
            if ( !ImplIsOverlapWindow() )
            {
                mpWindowImpl->mbDisabled          = pParent->mpWindowImpl->mbDisabled;
                mpWindowImpl->mbInputDisabled     = pParent->mpWindowImpl->mbInputDisabled;
                mpWindowImpl->meAlwaysInputMode   = pParent->mpWindowImpl->meAlwaysInputMode;
            }

            if (!comphelper::IsFuzzing())
            {
                // we don't want to call the WindowOutputDevice override of this because
                // it calls back into us.
                mpWindowImpl->mxOutDev->OutputDevice::SetSettings( pParent->GetSettings() );
            }
        }

    }

    // setup the scale factor for HiDPI displays
    mpWindowImpl->mxOutDev->mnDPIScalePercentage = CountDPIScaleFactor(mpWindowImpl->mpFrameData->mnDPIY);
    mpWindowImpl->mxOutDev->mnDPIX = mpWindowImpl->mpFrameData->mnDPIX;
    mpWindowImpl->mxOutDev->mnDPIY = mpWindowImpl->mpFrameData->mnDPIY;

    if (!comphelper::IsFuzzing())
    {
        const StyleSettings& rStyleSettings = mpWindowImpl->mxOutDev->moSettings->GetStyleSettings();
        mpWindowImpl->mxOutDev->maFont = rStyleSettings.GetAppFont();

        if ( nStyle & WB_3DLOOK )
        {
            SetTextColor( rStyleSettings.GetButtonTextColor() );
            SetBackground( Wallpaper( rStyleSettings.GetFaceColor() ) );
        }
        else
        {
            SetTextColor( rStyleSettings.GetWindowTextColor() );
            SetBackground( Wallpaper( rStyleSettings.GetWindowColor() ) );
        }
    }
    else
    {
        mpWindowImpl->mxOutDev->maFont = OutputDevice::GetDefaultFont( DefaultFontType::FIXED, LANGUAGE_ENGLISH_US, GetDefaultFontFlags::NONE );
    }

    ImplPointToLogic(*GetOutDev(), mpWindowImpl->mxOutDev->maFont);

    (void)ImplUpdatePos();

    // calculate app font res (except for the Intro Window or the default window)
    if ( mpWindowImpl->mbFrame && !pSVData->maGDIData.mnAppFontX && ! (nStyle & (WB_INTROWIN|WB_DEFAULTWIN)) )
        ImplInitAppFontData( this );
}

void Window::ImplInitAppFontData( vcl::Window const * pWindow )
{
    ImplSVData* pSVData = ImplGetSVData();
    tools::Long nTextHeight = pWindow->GetTextHeight();
    tools::Long nTextWidth = pWindow->approximate_char_width() * 8;
    tools::Long nSymHeight = nTextHeight*4;
    // Make the basis wider if the font is too narrow
    // such that the dialog looks symmetrical and does not become too narrow.
    // Add some extra space when the dialog has the same width,
    // as a little more space is better.
    if ( nSymHeight > nTextWidth )
        nTextWidth = nSymHeight;
    else if ( nSymHeight+5 > nTextWidth )
        nTextWidth = nSymHeight+5;
    pSVData->maGDIData.mnAppFontX = nTextWidth * 10 / 8;
    pSVData->maGDIData.mnAppFontY = nTextHeight * 10;

#ifdef MACOSX
    // FIXME: this is currently only on macOS, check with other
    // platforms
    if( pSVData->maNWFData.mbNoFocusRects )
    {
        // try to find out whether there is a large correction
        // of control sizes, if yes, make app font scalings larger
        // so dialog positioning is not completely off
        ImplControlValue aControlValue;
        tools::Rectangle aCtrlRegion( Point(), Size( nTextWidth < 10 ? 10 : nTextWidth, nTextHeight < 10 ? 10 : nTextHeight ) );
        tools::Rectangle aBoundingRgn( aCtrlRegion );
        tools::Rectangle aContentRgn( aCtrlRegion );
        if( pWindow->GetNativeControlRegion( ControlType::Editbox, ControlPart::Entire, aCtrlRegion,
                                             ControlState::ENABLED, aControlValue,
                                             aBoundingRgn, aContentRgn ) )
        {
            // comment: the magical +6 is for the extra border in bordered
            // (which is the standard) edit fields
            if( aContentRgn.GetHeight() - nTextHeight > (nTextHeight+4)/4 )
                pSVData->maGDIData.mnAppFontY = (aContentRgn.GetHeight()-4) * 10;
        }
    }
#endif
}

ImplWinData* Window::ImplGetWinData() const
{
    if (!mpWindowImpl->mpWinData)
    {
        static const char* pNoNWF = getenv( "SAL_NO_NWF" );

        const_cast<vcl::Window*>(this)->mpWindowImpl->mpWinData.reset(new ImplWinData);
        mpWindowImpl->mpWinData->mbEnableNativeWidget = !(pNoNWF && *pNoNWF); // true: try to draw this control with native theme API
    }

    return mpWindowImpl->mpWinData.get();
}


void WindowOutputDevice::CopyDeviceArea( SalTwoRect& aPosAry, bool bWindowInvalidate )
{
    if (aPosAry.mnSrcWidth == 0 || aPosAry.mnSrcHeight == 0 || aPosAry.mnDestWidth == 0 || aPosAry.mnDestHeight == 0)
        return;

    if (bWindowInvalidate)
    {
        const tools::Rectangle aSrcRect(Point(aPosAry.mnSrcX, aPosAry.mnSrcY),
                Size(aPosAry.mnSrcWidth, aPosAry.mnSrcHeight));

        mxOwnerWindow->ImplMoveAllInvalidateRegions(aSrcRect,
                aPosAry.mnDestX-aPosAry.mnSrcX,
                aPosAry.mnDestY-aPosAry.mnSrcY,
                false);

        mpGraphics->CopyArea(aPosAry.mnDestX, aPosAry.mnDestY,
                aPosAry.mnSrcX, aPosAry.mnSrcY,
                aPosAry.mnSrcWidth, aPosAry.mnSrcHeight,
                *this);

        return;
    }

    OutputDevice::CopyDeviceArea(aPosAry, bWindowInvalidate);
}

const OutputDevice* WindowOutputDevice::DrawOutDevDirectCheck(const OutputDevice& rSrcDev) const
{
    const OutputDevice* pSrcDevChecked;
    if ( this == &rSrcDev )
        pSrcDevChecked = nullptr;
    else if (GetOutDevType() != rSrcDev.GetOutDevType())
        pSrcDevChecked = &rSrcDev;
    else if (mxOwnerWindow->mpWindowImpl->mpFrameWindow == static_cast<const vcl::WindowOutputDevice&>(rSrcDev).mxOwnerWindow->mpWindowImpl->mpFrameWindow)
        pSrcDevChecked = nullptr;
    else
        pSrcDevChecked = &rSrcDev;

    return pSrcDevChecked;
}

void WindowOutputDevice::DrawOutDevDirectProcess( const OutputDevice& rSrcDev, SalTwoRect& rPosAry, SalGraphics* pSrcGraphics )
{
    if (pSrcGraphics)
        mpGraphics->CopyBits(rPosAry, *pSrcGraphics, *this, rSrcDev);
    else
        mpGraphics->CopyBits(rPosAry, *this);
}

SalGraphics* Window::ImplGetFrameGraphics() const
{
    if ( mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics )
    {
        mpWindowImpl->mpFrameWindow->GetOutDev()->mbInitClipRegion = true;
    }
    else
    {
        OutputDevice* pFrameWinOutDev = mpWindowImpl->mpFrameWindow->GetOutDev();
        if ( ! pFrameWinOutDev->AcquireGraphics() )
        {
            return nullptr;
        }
    }
    mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics->ResetClipRegion();
    return mpWindowImpl->mpFrameWindow->GetOutDev()->mpGraphics;
}

void Window::ImplSetReallyVisible()
{
    // #i43594# it is possible that INITSHOW was never send, because the visibility state changed between
    // ImplCallInitShow() and ImplSetReallyVisible() when called from Show()
    // mbReallyShown is a useful indicator
    if( !mpWindowImpl->mbReallyShown )
        ImplCallInitShow();

    bool bBecameReallyVisible = !mpWindowImpl->mbReallyVisible;

    GetOutDev()->mbDevOutput     = true;
    mpWindowImpl->mbReallyVisible = true;
    mpWindowImpl->mbReallyShown   = true;

    // the SHOW/HIDE events serve as indicators to send child creation/destroy events to the access bridge.
    // For this, the data member of the event must not be NULL.
    // Previously, we did this in Window::Show, but there some events got lost in certain situations. Now
    // we're doing it when the visibility really changes
    if( bBecameReallyVisible && ImplIsAccessibleCandidate() )
        CallEventListeners( VclEventId::WindowShow, this );
        // TODO. It's kind of a hack that we're re-using the VclEventId::WindowShow. Normally, we should
        // introduce another event which explicitly triggers the Accessibility implementations.

    vcl::Window* pWindow = mpWindowImpl->mpFirstOverlap;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbVisible )
            pWindow->ImplSetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpNext;
    }

    pWindow = mpWindowImpl->mpFirstChild;
    while ( pWindow )
    {
        if ( pWindow->mpWindowImpl->mbVisible )
            pWindow->ImplSetReallyVisible();
        pWindow = pWindow->mpWindowImpl->mpNext;
    }
}

void Window::ImplInitResolutionSettings()
{
    // recalculate AppFont-resolution and DPI-resolution
    if (mpWindowImpl->mbFrame)
    {
        GetOutDev()->mnDPIX = mpWindowImpl->mpFrameData->mnDPIX;
        GetOutDev()->mnDPIY = mpWindowImpl->mpFrameData->mnDPIY;

        // setup the scale factor for HiDPI displays
        GetOutDev()->mnDPIScalePercentage = CountDPIScaleFactor(mpWindowImpl->mpFrameData->mnDPIY);
        const StyleSettings& rStyleSettings = GetOutDev()->moSettings->GetStyleSettings();
        SetPointFont(*GetOutDev(), rStyleSettings.GetAppFont());
    }
    else if ( mpWindowImpl->mpParent )
    {
        GetOutDev()->mnDPIX  = mpWindowImpl->mpParent->GetOutDev()->mnDPIX;
        GetOutDev()->mnDPIY  = mpWindowImpl->mpParent->GetOutDev()->mnDPIY;
        GetOutDev()->mnDPIScalePercentage = mpWindowImpl->mpParent->GetOutDev()->mnDPIScalePercentage;
    }

    // update the recalculated values for logical units
    // and also tools belonging to the values
    if (IsMapModeEnabled())
    {
        MapMode aMapMode = GetMapMode();
        SetMapMode();
        SetMapMode( aMapMode );
    }
}

void Window::ImplPointToLogic(vcl::RenderContext const & rRenderContext, vcl::Font& rFont,
                            bool bUseRenderContextDPI) const
{
    Size aSize = rFont.GetFontSize();

    if (aSize.Width())
    {
        aSize.setWidth( aSize.Width() *
            ( bUseRenderContextDPI ? rRenderContext.GetDPIX() : mpWindowImpl->mpFrameData->mnDPIX) );
        aSize.AdjustWidth(72 / 2 );
        aSize.setWidth( aSize.Width() / 72 );
    }
    aSize.setHeight( aSize.Height()
        * ( bUseRenderContextDPI ? rRenderContext.GetDPIY() : mpWindowImpl->mpFrameData->mnDPIY) );
    aSize.AdjustHeight(72/2 );
    aSize.setHeight( aSize.Height() / 72 );

    aSize = rRenderContext.PixelToLogic(aSize);

    rFont.SetFontSize(aSize);
}

void Window::ImplLogicToPoint(vcl::RenderContext const & rRenderContext, vcl::Font& rFont) const
{
    Size aSize = rFont.GetFontSize();
    aSize = rRenderContext.LogicToPixel(aSize);

    if (aSize.Width())
    {
        aSize.setWidth( aSize.Width() * 72 );
        aSize.AdjustWidth(mpWindowImpl->mpFrameData->mnDPIX / 2 );
        aSize.setWidth( aSize.Width() / ( mpWindowImpl->mpFrameData->mnDPIX) );
    }
    aSize.setHeight( aSize.Height() * 72 );
    aSize.AdjustHeight(mpWindowImpl->mpFrameData->mnDPIY / 2 );
    aSize.setHeight( aSize.Height() / ( mpWindowImpl->mpFrameData->mnDPIY) );

    rFont.SetFontSize(aSize);
}

bool Window::ImplUpdatePos()
{
    bool bSysChild = false;

    if ( ImplIsOverlapWindow() )
    {
        GetOutDev()->mnOutOffX  = mpWindowImpl->mnX;
        GetOutDev()->mnOutOffY  = mpWindowImpl->mnY;
    }
    else
    {
        vcl::Window* pParent = ImplGetParent();

        GetOutDev()->mnOutOffX  = mpWindowImpl->mnX + pParent->GetOutDev()->mnOutOffX;
        GetOutDev()->mnOutOffY  = mpWindowImpl->mnY + pParent->GetOutDev()->mnOutOffY;
    }

    VclPtr< vcl::Window > pChild = mpWindowImpl->mpFirstChild;
    while ( pChild )
    {
        if ( pChild->ImplUpdatePos() )
            bSysChild = true;
        pChild = pChild->mpWindowImpl->mpNext;
    }

    if ( mpWindowImpl->mpSysObj )
        bSysChild = true;

    return bSysChild;
}

void Window::ImplUpdateSysObjPos()
{
    if ( mpWindowImpl->mpSysObj )
        mpWindowImpl->mpSysObj->SetPosSize( GetOutDev()->mnOutOffX, GetOutDev()->mnOutOffY, GetOutDev()->mnOutWidth, GetOutDev()->mnOutHeight );

    VclPtr< vcl::Window > pChild = mpWindowImpl->mpFirstChild;
    while ( pChild )
    {
        pChild->ImplUpdateSysObjPos();
        pChild = pChild->mpWindowImpl->mpNext;
    }
}

void Window::ImplPosSizeWindow( tools::Long nX, tools::Long nY,
                                tools::Long nWidth, tools::Long nHeight, PosSizeFlags nFlags )
{
    bool    bNewPos         = false;
    bool    bNewSize        = false;
    bool    bCopyBits       = false;
    tools::Long    nOldOutOffX     = GetOutDev()->mnOutOffX;
    tools::Long    nOldOutOffY     = GetOutDev()->mnOutOffY;
    tools::Long    nOldOutWidth    = GetOutDev()->mnOutWidth;
    tools::Long    nOldOutHeight   = GetOutDev()->mnOutHeight;
    std::unique_ptr<vcl::Region> pOverlapRegion;
    std::unique_ptr<vcl::Region> pOldRegion;

    if ( IsReallyVisible() )
    {
        tools::Rectangle aOldWinRect( Point( nOldOutOffX, nOldOutOffY ),
                               Size( nOldOutWidth, nOldOutHeight ) );
        pOldRegion.reset( new vcl::Region( aOldWinRect ) );
        if ( mpWindowImpl->mbWinRegion )
            pOldRegion->Intersect( GetOutDev()->ImplPixelToDevicePixel( mpWindowImpl->maWinRegion ) );

        if ( GetOutDev()->mnOutWidth && GetOutDev()->mnOutHeight && !mpWindowImpl->mbPaintTransparent &&
             !mpWindowImpl->mbInitWinClipRegion && !mpWindowImpl->maWinClipRegion.IsEmpty() &&
             !HasPaintEvent() )
            bCopyBits = true;
    }

    bool bnXRecycled = false; // avoid duplicate mirroring in RTL case
    if ( nFlags & PosSizeFlags::Width )
    {
        if(!( nFlags & PosSizeFlags::X ))
        {
            nX = mpWindowImpl->mnX;
            nFlags |= PosSizeFlags::X;
            bnXRecycled = true; // we're using a mnX which was already mirrored in RTL case
        }

        if ( nWidth < 0 )
            nWidth = 0;
        if ( nWidth != GetOutDev()->mnOutWidth )
        {
            GetOutDev()->mnOutWidth = nWidth;
            bNewSize = true;
            bCopyBits = false;
        }
    }
    if ( nFlags & PosSizeFlags::Height )
    {
        if ( nHeight < 0 )
            nHeight = 0;
        if ( nHeight != GetOutDev()->mnOutHeight )
        {
            GetOutDev()->mnOutHeight = nHeight;
            bNewSize = true;
            bCopyBits = false;
        }
    }

    if ( nFlags & PosSizeFlags::X )
    {
        tools::Long nOrgX = nX;
        Point aPtDev( nX+GetOutDev()->mnOutOffX, 0 );
        OutputDevice *pOutDev = GetOutDev();
        if( pOutDev->HasMirroredGraphics() )
        {
            aPtDev.setX( GetOutDev()->mpGraphics->mirror2( aPtDev.X(), *GetOutDev() ) );

            // #106948# always mirror our pos if our parent is not mirroring, even
            // if we are also not mirroring
            // RTL: check if parent is in different coordinates
            if( !bnXRecycled && mpWindowImpl->mpParent && !mpWindowImpl->mpParent->mpWindowImpl->mbFrame && mpWindowImpl->mpParent->GetOutDev()->ImplIsAntiparallel() )
            {
                nX = mpWindowImpl->mpParent->GetOutDev()->mnOutWidth - GetOutDev()->mnOutWidth - nX;
            }
            /* #i99166# An LTR window in RTL UI that gets sized only would be
               expected to not moved its upper left point
            */
            if( bnXRecycled )
            {
                if( GetOutDev()->ImplIsAntiparallel() )
                {
                    aPtDev.setX( mpWindowImpl->mnAbsScreenX );
                    nOrgX = mpWindowImpl->maPos.X();
                }
            }
        }
        else if( !bnXRecycled && mpWindowImpl->mpParent && !mpWindowImpl->mpParent->mpWindowImpl->mbFrame && mpWindowImpl->mpParent->GetOutDev()->ImplIsAntiparallel() )
        {
            // mirrored window in LTR UI
            nX = mpWindowImpl->mpParent->GetOutDev()->mnOutWidth - GetOutDev()->mnOutWidth - nX;
        }

        // check maPos as well, as it could have been changed for client windows (ImplCallMove())
        if ( mpWindowImpl->mnAbsScreenX != aPtDev.X() || nX != mpWindowImpl->mnX || nOrgX != mpWindowImpl->maPos.X() )
        {
            if ( bCopyBits && !pOverlapRegion )
            {
                pOverlapRegion.reset( new vcl::Region() );
                ImplCalcOverlapRegion( GetOutputRectPixel(),
                                       *pOverlapRegion, false, true );
            }
            mpWindowImpl->mnX = nX;
            mpWindowImpl->maPos.setX( nOrgX );
            mpWindowImpl->mnAbsScreenX = aPtDev.X();
            bNewPos = true;
        }
    }
    if ( nFlags & PosSizeFlags::Y )
    {
        // check maPos as well, as it could have been changed for client windows (ImplCallMove())
        if ( nY != mpWindowImpl->mnY || nY != mpWindowImpl->maPos.Y() )
        {
            if ( bCopyBits && !pOverlapRegion )
            {
                pOverlapRegion.reset( new vcl::Region() );
                ImplCalcOverlapRegion( GetOutputRectPixel(),
                                       *pOverlapRegion, false, true );
            }
            mpWindowImpl->mnY = nY;
            mpWindowImpl->maPos.setY( nY );
            bNewPos = true;
        }
    }

    if ( !(bNewPos || bNewSize) )
        return;

    bool bUpdateSysObjPos = false;
    if ( bNewPos )
        bUpdateSysObjPos = ImplUpdatePos();

    // the borderwindow always specifies the position for its client window
    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->maPos = mpWindowImpl->mpBorderWindow->mpWindowImpl->maPos;

    if ( mpWindowImpl->mpClientWindow )
    {
        mpWindowImpl->mpClientWindow->ImplPosSizeWindow( mpWindowImpl->mpClientWindow->mpWindowImpl->mnLeftBorder,
                                           mpWindowImpl->mpClientWindow->mpWindowImpl->mnTopBorder,
                                           GetOutDev()->mnOutWidth - mpWindowImpl->mpClientWindow->mpWindowImpl->mnLeftBorder-mpWindowImpl->mpClientWindow->mpWindowImpl->mnRightBorder,
                                           GetOutDev()->mnOutHeight - mpWindowImpl->mpClientWindow->mpWindowImpl->mnTopBorder-mpWindowImpl->mpClientWindow->mpWindowImpl->mnBottomBorder,
                                           PosSizeFlags::X | PosSizeFlags::Y |
                                           PosSizeFlags::Width | PosSizeFlags::Height );
        // If we have a client window, then this is the position
        // of the Application's floating windows
        mpWindowImpl->mpClientWindow->mpWindowImpl->maPos = mpWindowImpl->maPos;
        if ( bNewPos )
        {
            if ( mpWindowImpl->mpClientWindow->IsVisible() )
            {
                mpWindowImpl->mpClientWindow->ImplCallMove();
            }
            else
            {
                mpWindowImpl->mpClientWindow->mpWindowImpl->mbCallMove = true;
            }
        }
    }

    // Move()/Resize() will be called only for Show(), such that
    // at least one is called before Show()
    if ( IsVisible() )
    {
        if ( bNewPos )
        {
            ImplCallMove();
        }
        if ( bNewSize )
        {
            ImplCallResize();
        }
    }
    else
    {
        if ( bNewPos )
            mpWindowImpl->mbCallMove = true;
        if ( bNewSize )
            mpWindowImpl->mbCallResize = true;
    }

    bool bUpdateSysObjClip = false;
    if ( IsReallyVisible() )
    {
        if ( bNewPos || bNewSize )
        {
            // set Clip-Flag
            bUpdateSysObjClip = !ImplSetClipFlag( true );
        }

        // invalidate window content ?
        if ( bNewPos || (GetOutDev()->mnOutWidth > nOldOutWidth) || (GetOutDev()->mnOutHeight > nOldOutHeight) )
        {
            if ( bNewPos )
            {
                bool bInvalidate = false;
                bool bParentPaint = true;
                if ( !ImplIsOverlapWindow() )
                    bParentPaint = mpWindowImpl->mpParent->IsPaintEnabled();
                if ( bCopyBits && bParentPaint && !HasPaintEvent() )
                {
                    vcl::Region aRegion( GetOutputRectPixel() );
                    if ( mpWindowImpl->mbWinRegion )
                        aRegion.Intersect( GetOutDev()->ImplPixelToDevicePixel( mpWindowImpl->maWinRegion ) );
                    ImplClipBoundaries( aRegion, false, true );
                    if ( !pOverlapRegion->IsEmpty() )
                    {
                        pOverlapRegion->Move( GetOutDev()->mnOutOffX - nOldOutOffX, GetOutDev()->mnOutOffY - nOldOutOffY );
                        aRegion.Exclude( *pOverlapRegion );
                    }
                    if ( !aRegion.IsEmpty() )
                    {
                        // adapt Paint areas
                        ImplMoveAllInvalidateRegions( tools::Rectangle( Point( nOldOutOffX, nOldOutOffY ),
                                                                 Size( nOldOutWidth, nOldOutHeight ) ),
                                                      GetOutDev()->mnOutOffX - nOldOutOffX, GetOutDev()->mnOutOffY - nOldOutOffY,
                                                      true );
                        SalGraphics* pGraphics = ImplGetFrameGraphics();
                        if ( pGraphics )
                        {

                            OutputDevice *pOutDev = GetOutDev();
                            const bool bSelectClipRegion = pOutDev->SelectClipRegion( aRegion, pGraphics );
                            if ( bSelectClipRegion )
                            {
                                pGraphics->CopyArea( GetOutDev()->mnOutOffX, GetOutDev()->mnOutOffY,
                                                     nOldOutOffX, nOldOutOffY,
                                                     nOldOutWidth, nOldOutHeight,
                                                     *GetOutDev() );
                            }
                            else
                                bInvalidate = true;
                        }
                        else
                            bInvalidate = true;
                        if ( !bInvalidate )
                        {
                            if ( !pOverlapRegion->IsEmpty() )
                                ImplInvalidateFrameRegion( pOverlapRegion.get(), InvalidateFlags::Children );
                        }
                    }
                    else
                        bInvalidate = true;
                }
                else
                    bInvalidate = true;
                if ( bInvalidate )
                    ImplInvalidateFrameRegion( nullptr, InvalidateFlags::Children );
            }
            else
            {
                vcl::Region aRegion( GetOutputRectPixel() );
                aRegion.Exclude( *pOldRegion );
                if ( mpWindowImpl->mbWinRegion )
                    aRegion.Intersect( GetOutDev()->ImplPixelToDevicePixel( mpWindowImpl->maWinRegion ) );
                ImplClipBoundaries( aRegion, false, true );
                if ( !aRegion.IsEmpty() )
                    ImplInvalidateFrameRegion( &aRegion, InvalidateFlags::Children );
            }
        }

        // invalidate Parent or Overlaps
        if ( bNewPos ||
             (GetOutDev()->mnOutWidth < nOldOutWidth) || (GetOutDev()->mnOutHeight < nOldOutHeight) )
        {
            vcl::Region aRegion( *pOldRegion );
            if ( !mpWindowImpl->mbPaintTransparent )
                ImplExcludeWindowRegion( aRegion );
            ImplClipBoundaries( aRegion, false, true );
            if ( !aRegion.IsEmpty() && !mpWindowImpl->mpBorderWindow )
                ImplInvalidateParentFrameRegion( aRegion );
        }
    }

    // adapt system objects
    if ( bUpdateSysObjClip )
        ImplUpdateSysObjClip();
    if ( bUpdateSysObjPos )
        ImplUpdateSysObjPos();
    if ( bNewSize && mpWindowImpl->mpSysObj )
        mpWindowImpl->mpSysObj->SetPosSize( GetOutDev()->mnOutOffX, GetOutDev()->mnOutOffY, GetOutDev()->mnOutWidth, GetOutDev()->mnOutHeight );
}

void Window::ImplNewInputContext()
{
    ImplSVData* pSVData = ImplGetSVData();
    vcl::Window* pFocusWin = pSVData->mpWinData->mpFocusWin;
    if ( !pFocusWin || !pFocusWin->mpWindowImpl || pFocusWin->isDisposed() )
        return;

    // Is InputContext changed?
    const InputContext& rInputContext = pFocusWin->GetInputContext();
    if ( rInputContext == pFocusWin->mpWindowImpl->mpFrameData->maOldInputContext )
        return;

    pFocusWin->mpWindowImpl->mpFrameData->maOldInputContext = rInputContext;

    SalInputContext         aNewContext;
    const vcl::Font&        rFont = rInputContext.GetFont();
    const OUString&         rFontName = rFont.GetFamilyName();
    if (!rFontName.isEmpty())
    {
        OutputDevice *pFocusWinOutDev = pFocusWin->GetOutDev();
        Size aSize = pFocusWinOutDev->ImplLogicToDevicePixel( rFont.GetFontSize() );
        if ( !aSize.Height() )
        {
            // only set default sizes if the font height in logical
            // coordinates equals 0
            if ( rFont.GetFontSize().Height() )
                aSize.setHeight( 1 );
            else
                aSize.setHeight( (12*pFocusWin->GetOutDev()->mnDPIY)/72 );
        }
        aNewContext.mpFont =
                        pFocusWin->GetOutDev()->mxFontCache->GetFontInstance(
                            pFocusWin->GetOutDev()->mxFontCollection.get(),
                            rFont, aSize, static_cast<float>(aSize.Height()) );
    }
    aNewContext.mnOptions   = rInputContext.GetOptions();
    pFocusWin->ImplGetFrame()->SetInputContext( &aNewContext );
}

void Window::SetDumpAsPropertyTreeHdl(const Link<tools::JsonWriter&, void>& rLink)
{
    if (mpWindowImpl) // may be called after dispose
    {
        mpWindowImpl->maDumpAsPropertyTreeHdl = rLink;
    }
}

void Window::SetModalHierarchyHdl(const Link<bool, void>& rLink)
{
    ImplGetFrame()->SetModalHierarchyHdl(rLink);
}

KeyIndicatorState Window::GetIndicatorState() const
{
    return mpWindowImpl->mpFrame->GetIndicatorState();
}

void Window::SimulateKeyPress( sal_uInt16 nKeyCode ) const
{
    mpWindowImpl->mpFrame->SimulateKeyPress(nKeyCode);
}

void Window::KeyInput( const KeyEvent& rKEvt )
{
#ifndef _WIN32 // On Windows, dialogs react to accelerators  without Alt (tdf#157649)
    KeyCode cod = rKEvt.GetKeyCode ();

    // do not respond to accelerators unless Alt or Ctrl is held
    if (cod.GetCode () >= 0x200 && cod.GetCode () <= 0x219)
    {
        bool autoacc = ImplGetSVData()->maNWFData.mbAutoAccel;
        if (autoacc && cod.GetModifier () != KEY_MOD2 && !(cod.GetModifier() & KEY_MOD1))
            return;
    }
#endif

    NotifyEvent aNEvt( NotifyEventType::KEYINPUT, this, &rKEvt );
    if ( !CompatNotify( aNEvt ) )
        mpWindowImpl->mbKeyInput = true;
}

void Window::KeyUp( const KeyEvent& rKEvt )
{
    NotifyEvent aNEvt( NotifyEventType::KEYUP, this, &rKEvt );
    if ( !CompatNotify( aNEvt ) )
        mpWindowImpl->mbKeyUp = true;
}

void Window::Draw( OutputDevice*, const Point&, SystemTextColorFlags )
{
}

void Window::Move() {}

void Window::Resize() {}

void Window::Activate() {}

void Window::Deactivate() {}

void Window::GetFocus()
{
    if ( HasFocus() && mpWindowImpl->mpLastFocusWindow && !(mpWindowImpl->mnDlgCtrlFlags & DialogControlFlags::WantFocus) )
    {
        VclPtr<vcl::Window> xWindow(this);
        mpWindowImpl->mpLastFocusWindow->GrabFocus();
        if( xWindow->isDisposed() )
            return;
    }

    NotifyEvent aNEvt( NotifyEventType::GETFOCUS, this );
    CompatNotify( aNEvt );
}

void Window::LoseFocus()
{
    NotifyEvent aNEvt( NotifyEventType::LOSEFOCUS, this );
    CompatNotify( aNEvt );
}

void Window::SetHelpHdl(const Link<vcl::Window&, bool>& rLink)
{
    if (mpWindowImpl) // may be called after dispose
    {
        mpWindowImpl->maHelpRequestHdl = rLink;
    }
}

void Window::RequestHelp( const HelpEvent& rHEvt )
{
    // if Balloon-Help is requested, show the balloon
    // with help text set
    if ( rHEvt.GetMode() & HelpEventMode::BALLOON )
    {
        OUString rStr = GetHelpText();
        if ( rStr.isEmpty() )
            rStr = GetQuickHelpText();
        if ( rStr.isEmpty() && ImplGetParent() && !ImplIsOverlapWindow() )
            ImplGetParent()->RequestHelp( rHEvt );
        else
        {
            Point aPos = GetPosPixel();
            if ( ImplGetParent() && !ImplIsOverlapWindow() )
                aPos = OutputToScreenPixel(Point(0, 0));
            tools::Rectangle   aRect( aPos, GetSizePixel() );

            Help::ShowBalloon( this, rHEvt.GetMousePosPixel(), aRect, rStr );
        }
    }
    else if ( rHEvt.GetMode() & HelpEventMode::QUICK )
    {
        const OUString& rStr = GetQuickHelpText();
        if ( rStr.isEmpty() && ImplGetParent() && !ImplIsOverlapWindow() )
            ImplGetParent()->RequestHelp( rHEvt );
        else
        {
            Point aPos = GetPosPixel();
            if ( ImplGetParent() && !ImplIsOverlapWindow() )
                aPos = OutputToScreenPixel(Point(0, 0));
            tools::Rectangle   aRect( aPos, GetSizePixel() );
            Help::ShowQuickHelp( this, aRect, rStr, QuickHelpFlags::CtrlText );
        }
    }
    else if (!mpWindowImpl->maHelpRequestHdl.IsSet() || mpWindowImpl->maHelpRequestHdl.Call(*this))
    {
        OUString aStrHelpId( GetHelpId() );
        if ( aStrHelpId.isEmpty() && ImplGetParent() )
            ImplGetParent()->RequestHelp( rHEvt );
        else
        {
            Help* pHelp = Application::GetHelp();
            if ( pHelp )
            {
                if( !aStrHelpId.isEmpty() )
                    pHelp->Start( aStrHelpId, this );
                else
                    pHelp->Start( u"" OOO_HELP_INDEX ""_ustr, this );
            }
        }
    }
}

void Window::Command( const CommandEvent& rCEvt )
{
    CallEventListeners( VclEventId::WindowCommand, const_cast<CommandEvent *>(&rCEvt) );

    NotifyEvent aNEvt( NotifyEventType::COMMAND, this, &rCEvt );
    if ( !CompatNotify( aNEvt ) )
        mpWindowImpl->mbCommand = true;
}

void Window::Tracking( const TrackingEvent& rTEvt )
{

    ImplDockingWindowWrapper *pWrapper = ImplGetDockingManager()->GetDockingWindowWrapper( this );
    if( pWrapper )
        pWrapper->Tracking( rTEvt );
}

void Window::StateChanged(StateChangedType eType)
{
    switch (eType)
    {
        //stuff that doesn't invalidate the layout
        case StateChangedType::ControlForeground:
        case StateChangedType::ControlBackground:
        case StateChangedType::UpdateMode:
        case StateChangedType::ReadOnly:
        case StateChangedType::Enable:
        case StateChangedType::State:
        case StateChangedType::Data:
        case StateChangedType::InitShow:
        case StateChangedType::ControlFocus:
            break;
        //stuff that does invalidate the layout
        default:
            queue_resize(eType);
            break;
    }
}

void Window::SetStyle( WinBits nStyle )
{
    if ( mpWindowImpl && mpWindowImpl->mnStyle != nStyle )
    {
        mpWindowImpl->mnPrevStyle = mpWindowImpl->mnStyle;
        mpWindowImpl->mnStyle = nStyle;
        CompatStateChanged( StateChangedType::Style );
    }
}

void Window::SetExtendedStyle( WindowExtendedStyle nExtendedStyle )
{

    if ( mpWindowImpl->mnExtendedStyle == nExtendedStyle )
        return;

    vcl::Window* pWindow = ImplGetBorderWindow();
    if( ! pWindow )
        pWindow = this;
    if( pWindow->mpWindowImpl->mbFrame )
    {
        SalExtStyle nExt = 0;
        if( nExtendedStyle & WindowExtendedStyle::Document )
            nExt |= SAL_FRAME_EXT_STYLE_DOCUMENT;
        if( nExtendedStyle & WindowExtendedStyle::DocModified )
            nExt |= SAL_FRAME_EXT_STYLE_DOCMODIFIED;

        pWindow->ImplGetFrame()->SetExtendedFrameStyle( nExt );
    }
    mpWindowImpl->mnExtendedStyle = nExtendedStyle;
}

void Window::SetBorderStyle( WindowBorderStyle nBorderStyle )
{

    if ( !mpWindowImpl->mpBorderWindow )
        return;

    if( nBorderStyle == WindowBorderStyle::REMOVEBORDER &&
        ! mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame &&
        mpWindowImpl->mpBorderWindow->mpWindowImpl->mpParent
        )
    {
        // this is a little awkward: some controls (e.g. svtools ProgressBar)
        // cannot avoid getting constructed with WB_BORDER but want to disable
        // borders in case of NWF drawing. So they need a method to remove their border window
        VclPtr<vcl::Window> pBorderWin = mpWindowImpl->mpBorderWindow;
        // remove us as border window's client
        pBorderWin->mpWindowImpl->mpClientWindow = nullptr;
        mpWindowImpl->mpBorderWindow = nullptr;
        mpWindowImpl->mpRealParent = pBorderWin->mpWindowImpl->mpParent;
        // reparent us above the border window
        SetParent( pBorderWin->mpWindowImpl->mpParent );
        // set us to the position and size of our previous border
        Point aBorderPos( pBorderWin->GetPosPixel() );
        Size aBorderSize( pBorderWin->GetSizePixel() );
        setPosSizePixel( aBorderPos.X(), aBorderPos.Y(), aBorderSize.Width(), aBorderSize.Height() );
        // release border window
        pBorderWin.disposeAndClear();

        // set new style bits
        SetStyle( GetStyle() & (~WB_BORDER) );
    }
    else
    {
        if ( mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW )
            static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->SetBorderStyle( nBorderStyle );
        else
            mpWindowImpl->mpBorderWindow->SetBorderStyle( nBorderStyle );
    }
}

WindowBorderStyle Window::GetBorderStyle() const
{

    if ( mpWindowImpl->mpBorderWindow )
    {
        if ( mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW )
            return static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->GetBorderStyle();
        else
            return mpWindowImpl->mpBorderWindow->GetBorderStyle();
    }

    return WindowBorderStyle::NONE;
}

tools::Long Window::CalcTitleWidth() const
{

    if ( mpWindowImpl->mpBorderWindow )
    {
        if ( mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW )
            return static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->CalcTitleWidth();
        else
            return mpWindowImpl->mpBorderWindow->CalcTitleWidth();
    }
    else if ( mpWindowImpl->mbFrame && (mpWindowImpl->mnStyle & WB_MOVEABLE) )
    {
        // we guess the width for frame windows as we do not know the
        // border of external dialogs
        const StyleSettings& rStyleSettings = GetSettings().GetStyleSettings();
        vcl::Font aFont = GetFont();
        const_cast<vcl::Window*>(this)->SetPointFont(const_cast<::OutputDevice&>(*GetOutDev()), rStyleSettings.GetTitleFont());
        tools::Long nTitleWidth = GetTextWidth( GetText() );
        const_cast<vcl::Window*>(this)->SetFont( aFont );
        nTitleWidth += rStyleSettings.GetTitleHeight() * 3;
        nTitleWidth += StyleSettings::GetBorderSize() * 2;
        nTitleWidth += 10;
        return nTitleWidth;
    }

    return 0;
}

void Window::SetInputContext( const InputContext& rInputContext )
{

    mpWindowImpl->maInputContext = rInputContext;
    if ( !mpWindowImpl->mbInFocusHdl && HasFocus() )
        ImplNewInputContext();
}

void Window::PostExtTextInputEvent(VclEventId nType, const OUString& rText)
{
    switch (nType)
    {
    case VclEventId::ExtTextInput:
    {
        std::unique_ptr<ExtTextInputAttr[]> pAttr(new ExtTextInputAttr[rText.getLength()]);
        for (int i = 0; i < rText.getLength(); ++i) {
            pAttr[i] = ExtTextInputAttr::Underline;
        }
        SalExtTextInputEvent aEvent { rText, pAttr.get(), rText.getLength(), EXTTEXTINPUT_CURSOR_OVERWRITE };
        ImplWindowFrameProc(this, SalEvent::ExtTextInput, &aEvent);
    }
    break;
    case VclEventId::EndExtTextInput:
        ImplWindowFrameProc(this, SalEvent::EndExtTextInput, nullptr);
        break;
    default:
        assert(false);
    }
}

void Window::EndExtTextInput()
{
    if ( mpWindowImpl->mbExtTextInput )
        ImplGetFrame()->EndExtTextInput( EndExtTextInputFlags::Complete );
}

void Window::SetCursorRect( const tools::Rectangle* pRect, tools::Long nExtTextInputWidth )
{

    ImplWinData* pWinData = ImplGetWinData();
    if ( pWinData->mpCursorRect )
    {
        if ( pRect )
            pWinData->mpCursorRect = *pRect;
        else
            pWinData->mpCursorRect.reset();
    }
    else
    {
        if ( pRect )
            pWinData->mpCursorRect = *pRect;
    }

    pWinData->mnCursorExtWidth = nExtTextInputWidth;

}

const tools::Rectangle* Window::GetCursorRect() const
{

    ImplWinData* pWinData = ImplGetWinData();
    return pWinData->mpCursorRect ? &*pWinData->mpCursorRect : nullptr;
}

tools::Long Window::GetCursorExtTextInputWidth() const
{

    ImplWinData* pWinData = ImplGetWinData();
    return pWinData->mnCursorExtWidth;
}

void Window::SetCompositionCharRect( const tools::Rectangle* pRect, tools::Long nCompositionLength, bool bVertical ) {

    ImplWinData* pWinData = ImplGetWinData();
    pWinData->mpCompositionCharRects.reset();
    pWinData->mbVertical = bVertical;
    pWinData->mnCompositionCharRects = nCompositionLength;
    if ( pRect && (nCompositionLength > 0) )
    {
        pWinData->mpCompositionCharRects.reset( new tools::Rectangle[nCompositionLength] );
        for (tools::Long i = 0; i < nCompositionLength; ++i)
            pWinData->mpCompositionCharRects[i] = pRect[i];
    }
}

void Window::CollectChildren(::std::vector<vcl::Window *>& rAllChildren )
{
    rAllChildren.push_back( this );

    VclPtr< vcl::Window > pChild = mpWindowImpl->mpFirstChild;
    while ( pChild )
    {
        pChild->CollectChildren( rAllChildren );
        pChild = pChild->mpWindowImpl->mpNext;
    }
}

void Window::SetPointFont(vcl::RenderContext& rRenderContext, const vcl::Font& rFont,
                          bool bUseRenderContextDPI)
{
    vcl::Font aFont = rFont;
    ImplPointToLogic(rRenderContext, aFont, bUseRenderContextDPI);
    rRenderContext.SetFont(aFont);
}

vcl::Font Window::GetPointFont(vcl::RenderContext const & rRenderContext) const
{
    vcl::Font aFont = rRenderContext.GetFont();
    ImplLogicToPoint(rRenderContext, aFont);
    return aFont;
}

void Window::Show(bool bVisible, ShowFlags nFlags)
{
    if ( !mpWindowImpl || mpWindowImpl->mbVisible == bVisible )
        return;

    VclPtr<vcl::Window> xWindow(this);

    bool bRealVisibilityChanged = false;
    mpWindowImpl->mbVisible = bVisible;

    if ( !bVisible )
    {
        ImplHideAllOverlaps();
        if( !xWindow->mpWindowImpl )
            return;

        if ( mpWindowImpl->mpBorderWindow )
        {
            bool bOldUpdate = mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate;
            if ( mpWindowImpl->mbNoParentUpdate )
                mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate = true;
            mpWindowImpl->mpBorderWindow->Show( false, nFlags );
            mpWindowImpl->mpBorderWindow->mpWindowImpl->mbNoParentUpdate = bOldUpdate;
        }
        else if ( mpWindowImpl->mbFrame )
        {
            mpWindowImpl->mbSuppressAccessibilityEvents = true;
            mpWindowImpl->mpFrame->Show( false );
        }

        CompatStateChanged( StateChangedType::Visible );

        if ( mpWindowImpl->mbReallyVisible )
        {
            if ( mpWindowImpl->mbInitWinClipRegion )
                ImplInitWinClipRegion();

            vcl::Region aInvRegion = mpWindowImpl->maWinClipRegion;

            if( !xWindow->mpWindowImpl )
                return;

            bRealVisibilityChanged = mpWindowImpl->mbReallyVisible;
            ImplResetReallyVisible();
            ImplSetClipFlag();

            if ( ImplIsOverlapWindow() && !mpWindowImpl->mbFrame )
            {
                // convert focus
                if ( !(nFlags & ShowFlags::NoFocusChange) && HasChildPathFocus() )
                {
                    if ( mpWindowImpl->mpOverlapWindow->IsEnabled() &&
                         mpWindowImpl->mpOverlapWindow->IsInputEnabled() &&
                         ! mpWindowImpl->mpOverlapWindow->IsInModalMode()
                         )
                        mpWindowImpl->mpOverlapWindow->GrabFocus();
                }
            }

            if ( !mpWindowImpl->mbFrame )
            {
                if (mpWindowImpl->mpWinData && mpWindowImpl->mpWinData->mbEnableNativeWidget)
                {
                    /*
                    * #i48371# native theming: some themes draw outside the control
                    * area we tell them to (bad thing, but we cannot do much about it ).
                    * On hiding these controls they get invalidated with their window rectangle
                    * which leads to the parts outside the control area being left and not
                    * invalidated. Workaround: invalidate an area on the parent, too
                    */
                    const int workaround_border = 5;
                    tools::Rectangle aBounds( aInvRegion.GetBoundRect() );
                    aBounds.AdjustLeft( -workaround_border );
                    aBounds.AdjustTop( -workaround_border );
                    aBounds.AdjustRight(workaround_border );
                    aBounds.AdjustBottom(workaround_border );
                    aInvRegion = aBounds;
                }
                if ( !mpWindowImpl->mbNoParentUpdate )
                {
                    if ( !aInvRegion.IsEmpty() )
                        ImplInvalidateParentFrameRegion( aInvRegion );
                }
                ImplGenerateMouseMove();
            }
        }
    }
    else
    {
        // inherit native widget flag for form controls
        // required here, because frames never show up in the child hierarchy - which should be fixed...
        // eg, the drop down of a combobox which is a system floating window
        if( mpWindowImpl->mbFrame && GetParent() && !GetParent()->isDisposed() &&
            GetParent()->IsCompoundControl() &&
            GetParent()->IsNativeWidgetEnabled() != IsNativeWidgetEnabled() &&
            !(GetStyle() & WB_TOOLTIPWIN) )
        {
            EnableNativeWidget( GetParent()->IsNativeWidgetEnabled() );
        }

        if ( mpWindowImpl->mbCallMove )
        {
            ImplCallMove();
        }
        if ( mpWindowImpl->mbCallResize )
        {
            ImplCallResize();
        }

        CompatStateChanged( StateChangedType::Visible );

        vcl::Window* pTestParent;
        if ( ImplIsOverlapWindow() )
            pTestParent = mpWindowImpl->mpOverlapWindow;
        else
            pTestParent = ImplGetParent();
        if ( mpWindowImpl->mbFrame || pTestParent->mpWindowImpl->mbReallyVisible )
        {
            // if a window becomes visible, send all child windows a StateChange,
            // such that these can initialise themselves
            ImplCallInitShow();

            // If it is a SystemWindow it automatically pops up on top of
            // all other windows if needed.
            if (ImplIsOverlapWindow())
            {
                if (!(nFlags & ShowFlags::NoActivate))
                {
                    ImplStartToTop((nFlags & ShowFlags::ForegroundTask) ? ToTopFlags::ForegroundTask
                                                                        : ToTopFlags::NONE);
                    ImplFocusToTop(ToTopFlags::NONE, false);

                    if (!(nFlags & ShowFlags::ForegroundTask))
                    {
                        // Inform user about window if we did not popup it at foreground
                        FlashWindow();
                    }
                }
            }

            // adjust mpWindowImpl->mbReallyVisible
            bRealVisibilityChanged = !mpWindowImpl->mbReallyVisible;
            ImplSetReallyVisible();

            // assure clip rectangles will be recalculated
            ImplSetClipFlag();

            if ( !mpWindowImpl->mbFrame )
            {
                InvalidateFlags nInvalidateFlags = InvalidateFlags::Children;
                if( ! IsPaintTransparent() )
                    nInvalidateFlags |= InvalidateFlags::NoTransparent;
                ImplInvalidate( nullptr, nInvalidateFlags );
                ImplGenerateMouseMove();
            }
        }

        if ( mpWindowImpl->mpBorderWindow )
            mpWindowImpl->mpBorderWindow->Show( true, nFlags );
        else if ( mpWindowImpl->mbFrame )
        {
            // #106431#, hide SplashScreen
            ImplSVData* pSVData = ImplGetSVData();
            if ( !pSVData->mpIntroWindow )
            {
                // The right way would be just to call this (not even in the 'if')
                auto pApp = GetpApp();
                if ( pApp )
                    pApp->InitFinished();
            }
            else if ( !ImplIsWindowOrChild( pSVData->mpIntroWindow ) )
            {
                // ... but the VCL splash is broken, and it needs this
                // (for ./soffice .uno:NewDoc)
                pSVData->mpIntroWindow->Hide();
            }

            //SAL_WARN_IF( mpWindowImpl->mbSuppressAccessibilityEvents, "vcl", "Window::Show() - Frame reactivated");
            mpWindowImpl->mbSuppressAccessibilityEvents = false;

            mpWindowImpl->mbPaintFrame = true;
            if (!Application::IsHeadlessModeEnabled())
            {
                bool bNoActivate(nFlags & (ShowFlags::NoActivate|ShowFlags::NoFocusChange));
                mpWindowImpl->mpFrame->Show( true, bNoActivate );
            }
            if( !xWindow->mpWindowImpl )
                return;

            // Query the correct size of the window, if we are waiting for
            // a system resize
            if ( mpWindowImpl->mbWaitSystemResize )
            {
                tools::Long nOutWidth;
                tools::Long nOutHeight;
                mpWindowImpl->mpFrame->GetClientSize( nOutWidth, nOutHeight );
                ImplHandleResize( this, nOutWidth, nOutHeight );
            }

            if (mpWindowImpl->mpFrameData->mpBuffer && mpWindowImpl->mpFrameData->mpBuffer->GetOutputSizePixel() != GetOutputSizePixel())
                // Make sure that the buffer size matches the window size, even if no resize was needed.
                mpWindowImpl->mpFrameData->mpBuffer->SetOutputSizePixel(GetOutputSizePixel());
        }

        if( !xWindow->mpWindowImpl )
            return;

        ImplShowAllOverlaps();
    }

    if( !xWindow->mpWindowImpl )
        return;

    // the SHOW/HIDE events also serve as indicators to send child creation/destroy events to the access bridge
    // However, the access bridge only uses this event if the data member is not NULL (it's kind of a hack that
    // we re-use the SHOW/HIDE events this way, with this particular semantics).
    // Since #104887#, the notifications for the access bridge are done in Impl(Set|Reset)ReallyVisible. Here, we
    // now only notify with a NULL data pointer, for all other clients except the access bridge.
    if ( !bRealVisibilityChanged )
        CallEventListeners( mpWindowImpl->mbVisible ? VclEventId::WindowShow : VclEventId::WindowHide );
}

Size Window::GetSizePixel() const
{
    if (!mpWindowImpl)
    {
        SAL_WARN("vcl.layout", "WTF no windowimpl");
        return Size(0,0);
    }

    // #i43257# trigger pending resize handler to assure correct window sizes
    if( mpWindowImpl->mpFrameData->maResizeIdle.IsActive() )
    {
        VclPtr<vcl::Window> xWindow( const_cast<Window*>(this) );
        mpWindowImpl->mpFrameData->maResizeIdle.Stop();
        mpWindowImpl->mpFrameData->maResizeIdle.Invoke( nullptr );
        if( xWindow->isDisposed() )
            return Size(0,0);
    }

    return Size( GetOutDev()->mnOutWidth + mpWindowImpl->mnLeftBorder+mpWindowImpl->mnRightBorder,
                 GetOutDev()->mnOutHeight + mpWindowImpl->mnTopBorder+mpWindowImpl->mnBottomBorder );
}

void Window::GetBorder( sal_Int32& rLeftBorder, sal_Int32& rTopBorder,
                               sal_Int32& rRightBorder, sal_Int32& rBottomBorder ) const
{
    rLeftBorder     = mpWindowImpl->mnLeftBorder;
    rTopBorder      = mpWindowImpl->mnTopBorder;
    rRightBorder    = mpWindowImpl->mnRightBorder;
    rBottomBorder   = mpWindowImpl->mnBottomBorder;
}

void Window::Enable( bool bEnable, bool bChild )
{
    if ( isDisposed() )
        return;

    if ( !bEnable )
    {
        // the tracking mode will be stopped or the capture will be stolen
        // when a window is disabled,
        if ( IsTracking() )
            EndTracking( TrackingEventFlags::Cancel );
        if ( IsMouseCaptured() )
            ReleaseMouse();
        // try to pass focus to the next control
        // if the window has focus and is contained in the dialog control
        // mpWindowImpl->mbDisabled should only be set after a call of ImplDlgCtrlNextWindow().
        // Otherwise ImplDlgCtrlNextWindow() should be used
        if ( HasFocus() )
            ImplDlgCtrlNextWindow();
    }

    if ( mpWindowImpl->mpBorderWindow )
    {
        mpWindowImpl->mpBorderWindow->Enable( bEnable, false );
        if ( (mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW) &&
             static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow )
            static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow->Enable( bEnable );
    }

    // #i56102# restore app focus win in case the
    // window was disabled when the frame focus changed
    ImplSVData* pSVData = ImplGetSVData();
    if (bEnable && pSVData->mpWinData->mpFocusWin == nullptr
        && mpWindowImpl->mpFrameData->mbHasFocus && mpWindowImpl->mpFrameData->mpFocusWin == this)
        pSVData->mpWinData->mpFocusWin = this;

    if ( mpWindowImpl->mbDisabled != !bEnable )
    {
        mpWindowImpl->mbDisabled = !bEnable;
        if ( mpWindowImpl->mpSysObj )
            mpWindowImpl->mpSysObj->Enable( bEnable && !mpWindowImpl->mbInputDisabled );
        CompatStateChanged( StateChangedType::Enable );

        CallEventListeners( bEnable ? VclEventId::WindowEnabled : VclEventId::WindowDisabled );
    }

    if ( bChild )
    {
        VclPtr< vcl::Window > pChild = mpWindowImpl->mpFirstChild;
        while ( pChild )
        {
            pChild->Enable( bEnable, bChild );
            pChild = pChild->mpWindowImpl->mpNext;
        }
    }

    if ( IsReallyVisible() )
        ImplGenerateMouseMove();
}

void Window::EnableInput( bool bEnable, bool bChild )
{
    if (!mpWindowImpl)
        return;

    if ( mpWindowImpl->mpBorderWindow )
    {
        mpWindowImpl->mpBorderWindow->EnableInput( bEnable, false );
        if ( (mpWindowImpl->mpBorderWindow->GetType() == WindowType::BORDERWINDOW) &&
             static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow )
            static_cast<ImplBorderWindow*>(mpWindowImpl->mpBorderWindow.get())->mpMenuBarWindow->EnableInput( bEnable );
    }

    if ( (!bEnable && mpWindowImpl->meAlwaysInputMode != AlwaysInputEnabled) || bEnable )
    {
        // automatically stop the tracking mode or steal capture
        // if the window is disabled
        if ( !bEnable )
        {
            if ( IsTracking() )
                EndTracking( TrackingEventFlags::Cancel );
            if ( IsMouseCaptured() )
                ReleaseMouse();
        }

        if ( mpWindowImpl->mbInputDisabled != !bEnable )
        {
            mpWindowImpl->mbInputDisabled = !bEnable;
            if ( mpWindowImpl->mpSysObj )
                mpWindowImpl->mpSysObj->Enable( !mpWindowImpl->mbDisabled && bEnable );
        }
    }

    // #i56102# restore app focus win in case the
    // window was disabled when the frame focus changed
    ImplSVData* pSVData = ImplGetSVData();
    if (bEnable && pSVData->mpWinData->mpFocusWin == nullptr
        && mpWindowImpl->mpFrameData->mbHasFocus && mpWindowImpl->mpFrameData->mpFocusWin == this)
        pSVData->mpWinData->mpFocusWin = this;

    if ( bChild )
    {
        VclPtr< vcl::Window > pChild = mpWindowImpl->mpFirstChild;
        while ( pChild )
        {
            pChild->EnableInput( bEnable, bChild );
            pChild = pChild->mpWindowImpl->mpNext;
        }
    }

    if ( IsReallyVisible() )
        ImplGenerateMouseMove();
}

void Window::EnableInput( bool bEnable, const vcl::Window* pExcludeWindow )
{
    if (!mpWindowImpl)
        return;

    EnableInput( bEnable );

    // pExecuteWindow is the first Overlap-Frame --> if this
    // shouldn't be the case, then this must be changed in dialog.cxx
    if( pExcludeWindow )
        pExcludeWindow = pExcludeWindow->ImplGetFirstOverlapWindow();
    vcl::Window* pSysWin = mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrameData->mpFirstOverlap;
    while ( pSysWin )
    {
        // Is Window in the path from this window
        if ( ImplGetFirstOverlapWindow()->ImplIsWindowOrChild( pSysWin, true ) )
        {
            // Is Window not in the exclude window path or not the
            // exclude window, then change the status
            if ( !pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild( pSysWin, true ) )
                pSysWin->EnableInput( bEnable );
        }
        pSysWin = pSysWin->mpWindowImpl->mpNextOverlap;
    }

    // enable/disable floating system windows as well
    vcl::Window* pFrameWin = ImplGetSVData()->maFrameData.mpFirstFrame;
    while ( pFrameWin )
    {
        if( pFrameWin->ImplIsFloatingWindow() )
        {
            // Is Window in the path from this window
            if ( ImplGetFirstOverlapWindow()->ImplIsWindowOrChild( pFrameWin, true ) )
            {
                // Is Window not in the exclude window path or not the
                // exclude window, then change the status
                if ( !pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild( pFrameWin, true ) )
                    pFrameWin->EnableInput( bEnable );
            }
        }
        pFrameWin = pFrameWin->mpWindowImpl->mpFrameData->mpNextFrame;
    }

    // the same for ownerdraw floating windows
    if( !mpWindowImpl->mbFrame )
        return;

    ::std::vector< VclPtr<vcl::Window> >& rList = mpWindowImpl->mpFrameData->maOwnerDrawList;
    for (auto const& elem : rList)
    {
        // Is Window in the path from this window
        if ( ImplGetFirstOverlapWindow()->ImplIsWindowOrChild( elem, true ) )
        {
            // Is Window not in the exclude window path or not the
            // exclude window, then change the status
            if ( !pExcludeWindow || !pExcludeWindow->ImplIsWindowOrChild( elem, true ) )
                elem->EnableInput( bEnable );
        }
    }
}

void Window::AlwaysEnableInput( bool bAlways, bool bChild )
{

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->AlwaysEnableInput( bAlways, false );

    if( bAlways && mpWindowImpl->meAlwaysInputMode != AlwaysInputEnabled )
    {
        mpWindowImpl->meAlwaysInputMode = AlwaysInputEnabled;
        EnableInput(true, false);
    }
    else if( ! bAlways && mpWindowImpl->meAlwaysInputMode == AlwaysInputEnabled )
    {
        mpWindowImpl->meAlwaysInputMode = AlwaysInputNone;
    }

    if ( bChild )
    {
        VclPtr< vcl::Window > pChild = mpWindowImpl->mpFirstChild;
        while ( pChild )
        {
            pChild->AlwaysEnableInput( bAlways, bChild );
            pChild = pChild->mpWindowImpl->mpNext;
        }
    }
}

void Window::SetActivateMode( ActivateModeFlags nMode )
{

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->SetActivateMode( nMode );

    if ( mpWindowImpl->mnActivateMode == nMode )
        return;

    mpWindowImpl->mnActivateMode = nMode;

    // possibly trigger Deactivate/Activate
    if ( mpWindowImpl->mnActivateMode != ActivateModeFlags::NONE )
    {
        if ( (mpWindowImpl->mbActive || (GetType() == WindowType::BORDERWINDOW)) &&
             !HasChildPathFocus( true ) )
        {
            mpWindowImpl->mbActive = false;
            Deactivate();
        }
    }
    else
    {
        if ( !mpWindowImpl->mbActive || (GetType() == WindowType::BORDERWINDOW) )
        {
            mpWindowImpl->mbActive = true;
            Activate();
        }
    }
}

void Window::setPosSizePixel( tools::Long nX, tools::Long nY,
                              tools::Long nWidth, tools::Long nHeight, PosSizeFlags nFlags )
{
    bool bHasValidSize = !mpWindowImpl->mbDefSize;

    if ( nFlags & PosSizeFlags::Pos )
        mpWindowImpl->mbDefPos = false;
    if ( nFlags & PosSizeFlags::Size )
        mpWindowImpl->mbDefSize = false;

    // The top BorderWindow is the window which is to be positioned
    VclPtr<vcl::Window> pWindow = this;
    while ( pWindow->mpWindowImpl->mpBorderWindow )
        pWindow = pWindow->mpWindowImpl->mpBorderWindow;

    if ( pWindow->mpWindowImpl->mbFrame )
    {
        // Note: if we're positioning a frame, the coordinates are interpreted
        // as being the top-left corner of the window's client area and NOT
        // as the position of the border ! (due to limitations of several UNIX window managers)
        tools::Long nOldWidth  = pWindow->GetOutDev()->mnOutWidth;

        if ( !(nFlags & PosSizeFlags::Width) )
            nWidth = pWindow->GetOutDev()->mnOutWidth;
        if ( !(nFlags & PosSizeFlags::Height) )
            nHeight = pWindow->GetOutDev()->mnOutHeight;

        sal_uInt16 nSysFlags=0;
        VclPtr<vcl::Window> pParent = GetParent();
        VclPtr<vcl::Window> pWinParent = pWindow->GetParent();

        if( nFlags & PosSizeFlags::Width )
            nSysFlags |= SAL_FRAME_POSSIZE_WIDTH;
        if( nFlags & PosSizeFlags::Height )
            nSysFlags |= SAL_FRAME_POSSIZE_HEIGHT;
        if( nFlags & PosSizeFlags::X )
        {
            nSysFlags |= SAL_FRAME_POSSIZE_X;
            if( pWinParent && (pWindow->GetStyle() & WB_SYSTEMCHILDWINDOW) )
            {
                nX += pWinParent->GetOutDev()->mnOutOffX;
            }
            if( pParent && pParent->GetOutDev()->ImplIsAntiparallel() )
            {
                tools::Rectangle aRect( Point ( nX, nY ), Size( nWidth, nHeight ) );
                const OutputDevice *pParentOutDev = pParent->GetOutDev();
                if (!comphelper::LibreOfficeKit::isActive())
                    pParentOutDev->ReMirror( aRect );
                nX = aRect.Left();
            }
        }
        if( !comphelper::LibreOfficeKit::isActive() &&
            !(nFlags & PosSizeFlags::X) && bHasValidSize &&
            pWindow->mpWindowImpl->mpFrame->GetWidth())
        {
            // RTL: make sure the old right aligned position is not changed
            // system windows will always grow to the right
            if ( pWinParent )
            {
                OutputDevice *pParentOutDev = pWinParent->GetOutDev();
                if( pParentOutDev->HasMirroredGraphics() )
                {
                    const SalFrameGeometry aSysGeometry = mpWindowImpl->mpFrame->GetUnmirroredGeometry();
                    const SalFrameGeometry aParentSysGeometry =
                        pWinParent->mpWindowImpl->mpFrame->GetUnmirroredGeometry();
                    tools::Long myWidth = nOldWidth;
                    if( !myWidth )
                        myWidth = aSysGeometry.width();
                    if( !myWidth )
                        myWidth = nWidth;
                    nFlags |= PosSizeFlags::X;
                    nSysFlags |= SAL_FRAME_POSSIZE_X;
                    nX = aParentSysGeometry.x() - aSysGeometry.leftDecoration() + aParentSysGeometry.width()
                        - myWidth - 1 - aSysGeometry.x();
                }
            }
        }
        if( nFlags & PosSizeFlags::Y )
        {
            nSysFlags |= SAL_FRAME_POSSIZE_Y;
            if( pWinParent && (pWindow->GetStyle() & WB_SYSTEMCHILDWINDOW) )
            {
                nY += pWinParent->GetOutDev()->mnOutOffY;
            }
        }

        if( nSysFlags & (SAL_FRAME_POSSIZE_WIDTH|SAL_FRAME_POSSIZE_HEIGHT) )
        {
            // check for min/max client size and adjust size accordingly
            // otherwise it may happen that the resize event is ignored, i.e. the old size remains
            // unchanged but ImplHandleResize() is called with the wrong size
            SystemWindow *pSystemWindow = dynamic_cast< SystemWindow* >( pWindow.get() );
            if( pSystemWindow )
            {
                Size aMinSize = pSystemWindow->GetMinOutputSizePixel();
                Size aMaxSize = pSystemWindow->GetMaxOutputSizePixel();
                if( nWidth < aMinSize.Width() )
                    nWidth = aMinSize.Width();
                if( nHeight < aMinSize.Height() )
                    nHeight = aMinSize.Height();

                if( nWidth > aMaxSize.Width() )
                    nWidth = aMaxSize.Width();
                if( nHeight > aMaxSize.Height() )
                    nHeight = aMaxSize.Height();
            }
        }

        pWindow->mpWindowImpl->mpFrame->SetPosSize( nX, nY, nWidth, nHeight, nSysFlags );

        // Adjust resize with the hack of different client size and frame geometries to fix
        // native menu bars. Eventually this should be replaced by proper mnTopBorder usage.
        pWindow->mpWindowImpl->mpFrame->GetClientSize(nWidth, nHeight);

        // Resize should be called directly. If we haven't
        // set the correct size, we get a second resize from
        // the system with the correct size. This can be happened
        // if the size is too small or too large.
        ImplHandleResize( pWindow, nWidth, nHeight );
    }
    else
    {
        pWindow->ImplPosSizeWindow( nX, nY, nWidth, nHeight, nFlags );
        if ( IsReallyVisible() )
            ImplGenerateMouseMove();
    }
}

Point Window::GetPosPixel() const
{
    return mpWindowImpl->maPos;
}

AbsoluteScreenPixelRectangle Window::GetDesktopRectPixel() const
{
    AbsoluteScreenPixelRectangle rRect;
    mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrame->GetWorkArea( rRect );
    return rRect;
}

Point Window::OutputToScreenPixel( const Point& rPos ) const
{
    // relative to top level parent
    return Point( rPos.X() + GetOutDev()->mnOutOffX, rPos.Y() + GetOutDev()->mnOutOffY );
}

Point Window::ScreenToOutputPixel( const Point& rPos ) const
{
    // relative to top level parent
    return Point( rPos.X() - GetOutDev()->mnOutOffX, rPos.Y() - GetOutDev()->mnOutOffY );
}

tools::Long Window::ImplGetUnmirroredOutOffX() const
{
    // revert mnOutOffX changes that were potentially made in ImplPosSizeWindow
    tools::Long offx = GetOutDev()->mnOutOffX;
    const OutputDevice *pOutDev = GetOutDev();
    if( pOutDev->HasMirroredGraphics() )
    {
        if( mpWindowImpl->mpParent && !mpWindowImpl->mpParent->mpWindowImpl->mbFrame && mpWindowImpl->mpParent->GetOutDev()->ImplIsAntiparallel() )
        {
            if ( !ImplIsOverlapWindow() )
                offx -= mpWindowImpl->mpParent->GetOutDev()->mnOutOffX;

            offx = mpWindowImpl->mpParent->GetOutDev()->mnOutWidth - GetOutDev()->mnOutWidth - offx;

            if ( !ImplIsOverlapWindow() )
                offx += mpWindowImpl->mpParent->GetOutDev()->mnOutOffX;

        }
    }
    return offx;
}

// normalized screen pixel are independent of mirroring
Point Window::OutputToNormalizedScreenPixel( const Point& rPos ) const
{
    // relative to top level parent
    tools::Long offx = ImplGetUnmirroredOutOffX();
    return Point( rPos.X()+offx, rPos.Y() + GetOutDev()->mnOutOffY );
}

Point Window::NormalizedScreenToOutputPixel( const Point& rPos ) const
{
    // relative to top level parent
    tools::Long offx = ImplGetUnmirroredOutOffX();
    return Point( rPos.X()-offx, rPos.Y() - GetOutDev()->mnOutOffY );
}

AbsoluteScreenPixelPoint Window::OutputToAbsoluteScreenPixel( const Point& rPos ) const
{
    // relative to the screen
    Point p = OutputToScreenPixel( rPos );
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
    p.AdjustX(g.x() );
    p.AdjustY(g.y() );
    return AbsoluteScreenPixelPoint(p);
}

Point Window::AbsoluteScreenToOutputPixel( const AbsoluteScreenPixelPoint& rPos ) const
{
    // relative to the screen
    Point p = ScreenToOutputPixel( Point(rPos) );
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
    p.AdjustX( -(g.x()) );
    p.AdjustY( -(g.y()) );
    return p;
}

AbsoluteScreenPixelRectangle Window::ImplOutputToUnmirroredAbsoluteScreenPixel( const tools::Rectangle &rRect ) const
{
    // this method creates unmirrored screen coordinates to be compared with the desktop
    // and is used for positioning of RTL popup windows correctly on the screen
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetUnmirroredGeometry();

    Point p1 = rRect.TopRight();
    p1 = OutputToScreenPixel(p1);
    p1.setX( g.x()+g.width()-p1.X() );
    p1.AdjustY(g.y() );

    Point p2 = rRect.BottomLeft();
    p2 = OutputToScreenPixel(p2);
    p2.setX( g.x()+g.width()-p2.X() );
    p2.AdjustY(g.y() );

    return AbsoluteScreenPixelRectangle( AbsoluteScreenPixelPoint(p1), AbsoluteScreenPixelPoint(p2) );
}

tools::Rectangle Window::ImplUnmirroredAbsoluteScreenToOutputPixel( const AbsoluteScreenPixelRectangle &rRect ) const
{
    // undo ImplOutputToUnmirroredAbsoluteScreenPixel
    SalFrameGeometry g = mpWindowImpl->mpFrame->GetUnmirroredGeometry();

    Point p1( rRect.TopRight() );
    p1.AdjustY(-g.y() );
    p1.setX( g.x()+g.width()-p1.X() );
    p1 = ScreenToOutputPixel(p1);

    Point p2( rRect.BottomLeft() );
    p2.AdjustY(-g.y());
    p2.setX( g.x()+g.width()-p2.X() );
    p2 = ScreenToOutputPixel(p2);

    return tools::Rectangle( p1, p2 );
}


// with decoration
tools::Rectangle Window::GetWindowExtentsRelative(const vcl::Window & rRelativeWindow) const
{
    AbsoluteScreenPixelRectangle aRect = GetWindowExtentsAbsolute();
    // #106399# express coordinates relative to borderwindow
    const vcl::Window *pRelWin = rRelativeWindow.mpWindowImpl->mpBorderWindow ? rRelativeWindow.mpWindowImpl->mpBorderWindow.get() : &rRelativeWindow;
    return tools::Rectangle(
        pRelWin->AbsoluteScreenToOutputPixel( aRect.GetPos() ),
        aRect.GetSize() );
}

// with decoration
AbsoluteScreenPixelRectangle Window::GetWindowExtentsAbsolute() const
{
    // make sure we use the extent of our border window,
    // otherwise we miss a few pixels
    const vcl::Window *pWin = mpWindowImpl->mpBorderWindow ? mpWindowImpl->mpBorderWindow : this;

    AbsoluteScreenPixelPoint aPos( pWin->OutputToAbsoluteScreenPixel( Point(0,0) ) );
    Size aSize ( pWin->GetSizePixel() );
    // #104088# do not add decoration to the workwindow to be compatible to java accessibility api
    if( mpWindowImpl->mbFrame || (mpWindowImpl->mpBorderWindow && mpWindowImpl->mpBorderWindow->mpWindowImpl->mbFrame && GetType() != WindowType::WORKWINDOW) )
    {
        SalFrameGeometry g = mpWindowImpl->mpFrame->GetGeometry();
        aPos.AdjustX( -sal_Int32(g.leftDecoration()) );
        aPos.AdjustY( -sal_Int32(g.topDecoration()) );
        aSize.AdjustWidth(g.leftDecoration() + g.rightDecoration() );
        aSize.AdjustHeight(g.topDecoration() + g.bottomDecoration() );
    }
    return AbsoluteScreenPixelRectangle( aPos, aSize );
}

void Window::Scroll( tools::Long nHorzScroll, tools::Long nVertScroll, ScrollFlags nFlags )
{

    ImplScroll( GetOutputRectPixel(),
                nHorzScroll, nVertScroll, nFlags & ~ScrollFlags::Clip );
}

void Window::Scroll( tools::Long nHorzScroll, tools::Long nVertScroll,
                     const tools::Rectangle& rRect, ScrollFlags nFlags )
{
    OutputDevice *pOutDev = GetOutDev();
    tools::Rectangle aRect = pOutDev->ImplLogicToDevicePixel( rRect );
    aRect.Intersection( GetOutputRectPixel() );
    if ( !aRect.IsEmpty() )
        ImplScroll( aRect, nHorzScroll, nVertScroll, nFlags );
}

void WindowOutputDevice::Flush()
{
    if (mxOwnerWindow->mpWindowImpl)
        mxOwnerWindow->mpWindowImpl->mpFrame->Flush( GetOutputRectPixel() );
}

void Window::SetUpdateMode( bool bUpdate )
{
    if (mpWindowImpl)
    {
        mpWindowImpl->mbNoUpdate = !bUpdate;
        CompatStateChanged( StateChangedType::UpdateMode );
    }
}

void Window::GrabFocus()
{
    ImplGrabFocus( GetFocusFlags::NONE );
}

bool Window::HasFocus() const
{
    return (this == ImplGetSVData()->mpWinData->mpFocusWin);
}

void Window::GrabFocusToDocument()
{
    ImplGrabFocusToDocument(GetFocusFlags::NONE);
}

VclPtr<vcl::Window> Window::GetFocusedWindow() const
{
    if (mpWindowImpl && mpWindowImpl->mpFrameData)
        return mpWindowImpl->mpFrameData->mpFocusWin;
    else
        return VclPtr<vcl::Window>();
}

void Window::SetFakeFocus( bool bFocus )
{
    ImplGetWindowImpl()->mbFakeFocusSet = bFocus;
}

bool Window::HasChildPathFocus( bool bSystemWindow ) const
{

    vcl::Window* pFocusWin = ImplGetSVData()->mpWinData->mpFocusWin;
    if ( pFocusWin )
        return ImplIsWindowOrChild( pFocusWin, bSystemWindow );
    return false;
}

void Window::SetCursor( vcl::Cursor* pCursor )
{

    if ( mpWindowImpl->mpCursor != pCursor )
    {
        if ( mpWindowImpl->mpCursor )
            mpWindowImpl->mpCursor->ImplHide();
        mpWindowImpl->mpCursor = pCursor;
        if ( pCursor )
            pCursor->ImplShow();
    }
}

void Window::SetText( const OUString& rStr )
{
    if (!mpWindowImpl || rStr == mpWindowImpl->maText)
        return;

    OUString oldTitle( mpWindowImpl->maText );
    mpWindowImpl->maText = rStr;

    if ( mpWindowImpl->mpBorderWindow )
        mpWindowImpl->mpBorderWindow->SetText( rStr );
    else if ( mpWindowImpl->mbFrame )
        mpWindowImpl->mpFrame->SetTitle( rStr );

    CallEventListeners( VclEventId::WindowFrameTitleChanged, &oldTitle );

    // #107247# needed for accessibility
    // The VclEventId::WindowFrameTitleChanged is (mis)used to notify accessible name changes.
    // Therefore a window, which is labeled by this window, must also notify an accessible
    // name change.
    if ( IsReallyVisible() )
    {
        vcl::Window* pWindow = GetAccessibleRelationLabelFor();
        if ( pWindow && pWindow != this )
            pWindow->CallEventListeners( VclEventId::WindowFrameTitleChanged, &oldTitle );
    }

    CompatStateChanged( StateChangedType::Text );
}

OUString Window::GetText() const
{

    return mpWindowImpl->maText;
}

OUString Window::GetDisplayText() const
{

    return GetText();
}

const Wallpaper& Window::GetDisplayBackground() const
{
    // FIXME: fix issue 52349, need to fix this really in
    // all NWF enabled controls
    const ToolBox* pTB = dynamic_cast<const ToolBox*>(this);
    if( pTB && IsNativeWidgetEnabled() )
        return pTB->ImplGetToolBoxPrivateData()->maDisplayBackground;

    if( !IsBackground() )
    {
        if( mpWindowImpl->mpParent )
            return mpWindowImpl->mpParent->GetDisplayBackground();
    }

    const Wallpaper& rBack = GetBackground();
    if( ! rBack.IsBitmap() &&
        ! rBack.IsGradient() &&
        rBack.GetColor()== COL_TRANSPARENT &&
        mpWindowImpl->mpParent )
            return mpWindowImpl->mpParent->GetDisplayBackground();
    return rBack;
}

const OUString& Window::GetHelpText() const
{
    const OUString& rStrHelpId(GetHelpId());
    const bool bStrHelpId = !rStrHelpId.isEmpty();

    if (mpWindowImpl->mbHelpTextDynamic && bStrHelpId)
    {
        static const char* pEnv = getenv( "HELP_DEBUG" );
        if( pEnv && *pEnv )
        {
            mpWindowImpl->maHelpText = mpWindowImpl->maHelpText + "\n------------------\n" + rStrHelpId;
        }
        mpWindowImpl->mbHelpTextDynamic = false;
    }

    //Fallback to Window::GetAccessibleDescription without reentry to GetHelpText()
    if (mpWindowImpl->maHelpText.isEmpty() && mpWindowImpl->mpAccessibleInfos && mpWindowImpl->mpAccessibleInfos->pAccessibleDescription)
        return *mpWindowImpl->mpAccessibleInfos->pAccessibleDescription;
    return mpWindowImpl->maHelpText;
}

void Window::SetWindowPeer( Reference< css::awt::XVclWindowPeer > const & xPeer, VCLXWindow* pVCLXWindow  )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        return;

    // be safe against re-entrance: first clear the old ref, then assign the new one
    if (mpWindowImpl->mxWindowPeer)
    {
        // first, disconnect the peer from ourself, otherwise disposing it, will dispose us
        UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
        SAL_WARN_IF( !pWrapper, "vcl.window", "SetComponentInterface: No Wrapper!" );
        if ( pWrapper )
            pWrapper->SetWindowInterface( nullptr, mpWindowImpl->mxWindowPeer );
        mpWindowImpl->mxWindowPeer->dispose();
        mpWindowImpl->mxWindowPeer.clear();
    }
    mpWindowImpl->mxWindowPeer = xPeer;

    mpWindowImpl->mpVCLXWindow = pVCLXWindow;
}

Reference< css::awt::XVclWindowPeer > Window::GetComponentInterface( bool bCreate )
{
    if ( !mpWindowImpl->mxWindowPeer.is() && bCreate )
    {
        UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
        if ( pWrapper )
            mpWindowImpl->mxWindowPeer = pWrapper->GetWindowInterface( this );
    }
    return mpWindowImpl->mxWindowPeer;
}

void Window::SetComponentInterface( Reference< css::awt::XVclWindowPeer > const & xIFace )
{
    UnoWrapperBase* pWrapper = UnoWrapperBase::GetUnoWrapper();
    SAL_WARN_IF( !pWrapper, "vcl.window", "SetComponentInterface: No Wrapper!" );
    if ( pWrapper )
        pWrapper->SetWindowInterface( this, xIFace );
}

typedef std::map<vcl::LOKWindowId, VclPtr<vcl::Window>> LOKWindowsMap;

namespace {

LOKWindowsMap& GetLOKWindowsMap()
{
    // Map to remember the LOKWindowId <-> Window binding.
    static LOKWindowsMap s_aLOKWindowsMap;

    return s_aLOKWindowsMap;
}

}

void Window::SetLOKNotifier(const vcl::ILibreOfficeKitNotifier* pNotifier, bool bParent)
{
    // don't allow setting this twice
    assert(mpWindowImpl->mpLOKNotifier == nullptr);
    assert(pNotifier);
    // never use this in the desktop case
    assert(comphelper::LibreOfficeKit::isActive());

    if (!bParent)
    {
        // Counter to be able to have unique id's for each window.
        static vcl::LOKWindowId sLastLOKWindowId = 1;

        // assign the LOK window id
        assert(mpWindowImpl->mnLOKWindowId == 0);
        mpWindowImpl->mnLOKWindowId = sLastLOKWindowId++;
        GetLOKWindowsMap().emplace(mpWindowImpl->mnLOKWindowId, this);
    }

    mpWindowImpl->mpLOKNotifier = pNotifier;
}

VclPtr<Window> Window::FindLOKWindow(vcl::LOKWindowId nWindowId)
{
    const auto it = GetLOKWindowsMap().find(nWindowId);
    if (it != GetLOKWindowsMap().end())
        return it->second;

    return VclPtr<Window>();
}

bool Window::IsLOKWindowsEmpty()
{
    return GetLOKWindowsMap().empty();
}

void Window::ReleaseLOKNotifier()
{
    // unregister the LOK window binding
    if (mpWindowImpl->mnLOKWindowId > 0)
        GetLOKWindowsMap().erase(mpWindowImpl->mnLOKWindowId);

    mpWindowImpl->mpLOKNotifier = nullptr;
    mpWindowImpl->mnLOKWindowId = 0;
}

ILibreOfficeKitNotifier::~ILibreOfficeKitNotifier()
{
    if (!comphelper::LibreOfficeKit::isActive())
    {
        return;
    }

    for (auto it = GetLOKWindowsMap().begin(); it != GetLOKWindowsMap().end();)
    {
        WindowImpl* pWindowImpl = it->second->ImplGetWindowImpl();
        if (pWindowImpl && pWindowImpl->mpLOKNotifier == this)
        {
            pWindowImpl->mpLOKNotifier = nullptr;
            pWindowImpl->mnLOKWindowId = 0;
            it = GetLOKWindowsMap().erase(it);
            continue;
        }

        ++it;
    }
}

const vcl::ILibreOfficeKitNotifier* Window::GetLOKNotifier() const
{
    return mpWindowImpl ? mpWindowImpl->mpLOKNotifier : nullptr;
}

vcl::LOKWindowId Window::GetLOKWindowId() const
{
    return mpWindowImpl ? mpWindowImpl->mnLOKWindowId : 0;
}

VclPtr<vcl::Window> Window::GetParentWithLOKNotifier()
{
    VclPtr<vcl::Window> pWindow(this);

    while (pWindow && !pWindow->GetLOKNotifier())
        pWindow = pWindow->GetParent();

    return pWindow;
}

namespace
{

std::string_view windowTypeName(WindowType nWindowType)
{
    switch (nWindowType)
    {
        case WindowType::NONE:                      return "none";
        case WindowType::MESSBOX:                   return "messagebox";
        case WindowType::INFOBOX:                   return "infobox";
        case WindowType::WARNINGBOX:                return "warningbox";
        case WindowType::ERRORBOX:                  return "errorbox";
        case WindowType::QUERYBOX:                  return "querybox";
        case WindowType::WINDOW:                    return "window";
        case WindowType::WORKWINDOW:                return "workwindow";
        case WindowType::CONTAINER:                 return "container";
        case WindowType::FLOATINGWINDOW:            return "floatingwindow";
        case WindowType::DIALOG:                    return "dialog";
        case WindowType::MODELESSDIALOG:            return "modelessdialog";
        case WindowType::CONTROL:                   return "control";
        case WindowType::PUSHBUTTON:                return "pushbutton";
        case WindowType::OKBUTTON:                  return "okbutton";
        case WindowType::CANCELBUTTON:              return "cancelbutton";
        case WindowType::HELPBUTTON:                return "helpbutton";
        case WindowType::IMAGEBUTTON:               return "imagebutton";
        case WindowType::MENUBUTTON:                return "menubutton";
        case WindowType::MOREBUTTON:                return "morebutton";
        case WindowType::SPINBUTTON:                return "spinbutton";
        case WindowType::RADIOBUTTON:               return "radiobutton";
        case WindowType::CHECKBOX:                  return "checkbox";
        case WindowType::TRISTATEBOX:               return "tristatebox";
        case WindowType::EDIT:                      return "edit";
        case WindowType::MULTILINEEDIT:             return "multilineedit";
        case WindowType::COMBOBOX:                  return "combobox";
        case WindowType::LISTBOX:                   return "listbox";
        case WindowType::MULTILISTBOX:              return "multilistbox";
        case WindowType::FIXEDTEXT:                 return "fixedtext";
        case WindowType::FIXEDLINE:                 return "fixedline";
        case WindowType::FIXEDBITMAP:               return "fixedbitmap";
        case WindowType::FIXEDIMAGE:                return "fixedimage";
        case WindowType::GROUPBOX:                  return "groupbox";
        case WindowType::SCROLLBAR:                 return "scrollbar";
        case WindowType::SCROLLBARBOX:              return "scrollbarbox";
        case WindowType::SPLITTER:                  return "splitter";
        case WindowType::SPLITWINDOW:               return "splitwindow";
        case WindowType::SPINFIELD:                 return "spinfield";
        case WindowType::PATTERNFIELD:              return "patternfield";
        case WindowType::METRICFIELD:               return "metricfield";
        case WindowType::FORMATTEDFIELD:            return "formattedfield";
        case WindowType::CURRENCYFIELD:             return "currencyfield";
        case WindowType::DATEFIELD:                 return "datefield";
        case WindowType::TIMEFIELD:                 return "timefield";
        case WindowType::PATTERNBOX:                return "patternbox";
        case WindowType::NUMERICBOX:                return "numericbox";
        case WindowType::METRICBOX:                 return "metricbox";
        case WindowType::CURRENCYBOX:               return "currencybox";
        case WindowType::DATEBOX:                   return "datebox";
        case WindowType::TIMEBOX:                   return "timebox";
        case WindowType::LONGCURRENCYBOX:           return "longcurrencybox";
        case WindowType::SCROLLWINDOW:              return "scrollwindow";
        case WindowType::TOOLBOX:                   return "toolbox";
        case WindowType::DOCKINGWINDOW:             return "dockingwindow";
        case WindowType::STATUSBAR:                 return "statusbar";
        case WindowType::TABPAGE:                   return "tabpage";
        case WindowType::TABCONTROL:                return "tabcontrol";
        case WindowType::TABDIALOG:                 return "tabdialog";
        case WindowType::BORDERWINDOW:              return "borderwindow";
        case WindowType::BUTTONDIALOG:              return "buttondialog";
        case WindowType::SYSTEMCHILDWINDOW:         return "systemchildwindow";
        case WindowType::SLIDER:                    return "slider";
        case WindowType::MENUBARWINDOW:             return "menubarwindow";
        case WindowType::TREELISTBOX:               return "treelistbox";
        case WindowType::HELPTEXTWINDOW:            return "helptextwindow";
        case WindowType::INTROWINDOW:               return "introwindow";
        case WindowType::LISTBOXWINDOW:             return "listboxwindow";
        case WindowType::DOCKINGAREA:               return "dockingarea";
        case WindowType::RULER:                     return "ruler";
        case WindowType::HEADERBAR:                 return "headerbar";
        case WindowType::VERTICALTABCONTROL:        return "verticaltabcontrol";
        case WindowType::PROGRESSBAR:               return "progressbar";
        case WindowType::LINK_BUTTON:               return "linkbutton";

        // nothing to do here, but for completeness
        case WindowType::TOOLKIT_FRAMEWINDOW:       return "toolkit_framewindow";
        case WindowType::TOOLKIT_SYSTEMCHILDWINDOW: return "toolkit_systemchildwindow";
    }

    return "none";
}

}

void Window::DumpAsPropertyTree(tools::JsonWriter& rJsonWriter)
{
    if (!mpWindowImpl)
        return;

    rJsonWriter.put("id", get_id());  // TODO could be missing - sort out
    rJsonWriter.put("type", windowTypeName(GetType()));
    rJsonWriter.put("text", GetText());
    rJsonWriter.put("enabled", IsEnabled());
    if (!IsVisible())
        rJsonWriter.put("visible", false);

    if (vcl::Window* pChild = mpWindowImpl->mpFirstChild)
    {
        auto childrenNode = rJsonWriter.startArray("children");
        while (pChild)
        {
            {
                auto childNode = rJsonWriter.startStruct();
                pChild->DumpAsPropertyTree(rJsonWriter);
                sal_Int32 nLeft = pChild->get_grid_left_attach();
                sal_Int32 nTop = pChild->get_grid_top_attach();
                if (nLeft != -1 && nTop != -1)
                {
                    rJsonWriter.put("left", nLeft);
                    rJsonWriter.put("top", nTop);
                }

                sal_Int32 nWidth = pChild->get_grid_width();
                if (nWidth > 1)
                    rJsonWriter.put("width", nWidth);
            }
            pChild = pChild->mpWindowImpl->mpNext;
        }
    }

    vcl::Window* pAccLabelFor = getAccessibleRelationLabelFor();
    if (pAccLabelFor)
        rJsonWriter.put("labelFor", pAccLabelFor->get_id());

    vcl::Window* pAccLabelledBy = GetAccessibleRelationLabeledBy();
    if (pAccLabelledBy)
        rJsonWriter.put("labelledBy", pAccLabelledBy->get_id());

    if(!pAccLabelFor && !pAccLabelledBy)
    {
        OUString sAccName = GetAccessibleName();
        OUString sAccDesc = GetAccessibleDescription();

        if (!sAccName.isEmpty() || !sAccDesc.isEmpty())
        {
            auto aAria = rJsonWriter.startNode("aria");
            if (!sAccName.isEmpty())
                rJsonWriter.put("label", sAccName);
            if (!sAccDesc.isEmpty())
                rJsonWriter.put("description", sAccDesc);
        }
    }

    mpWindowImpl->maDumpAsPropertyTreeHdl.Call(rJsonWriter);
}

void Window::ImplCallDeactivateListeners( vcl::Window *pNew )
{
    // no deactivation if the newly activated window is my child
    if ( !pNew || !ImplIsChild( pNew ) )
    {
        VclPtr<vcl::Window> xWindow(this);
        CallEventListeners( VclEventId::WindowDeactivate, pNew );
        if( !xWindow->mpWindowImpl )
            return;

        // #100759#, avoid walking the wrong frame's hierarchy
        //           eg, undocked docking windows (ImplDockFloatWin)
        if ( ImplGetParent() && ImplGetParent()->mpWindowImpl &&
             mpWindowImpl->mpFrameWindow == ImplGetParent()->mpWindowImpl->mpFrameWindow )
            ImplGetParent()->ImplCallDeactivateListeners( pNew );
    }
}

void Window::ImplCallActivateListeners( vcl::Window *pOld )
{
    // no activation if the old active window is my child
    if ( pOld && ImplIsChild( pOld ))
        return;

    VclPtr<vcl::Window> xWindow(this);
    CallEventListeners( VclEventId::WindowActivate, pOld );
    if( !xWindow->mpWindowImpl )
        return;

    if ( ImplGetParent() )
        ImplGetParent()->ImplCallActivateListeners( pOld );
    else if( (mpWindowImpl->mnStyle & WB_INTROWIN) == 0 )
    {
        // top level frame reached: store hint for DefModalDialogParent
        ImplGetSVData()->maFrameData.mpActiveApplicationFrame = mpWindowImpl->mpFrameWindow;
    }
}

void Window::SetClipboard(Reference<XClipboard> const & xClipboard)
{
    if (mpWindowImpl->mpFrameData)
        mpWindowImpl->mpFrameData->mxClipboard = xClipboard;
}

Reference< XClipboard > Window::GetClipboard()
{
    if (!mpWindowImpl->mpFrameData)
        return static_cast<XClipboard*>(nullptr);
    if (!mpWindowImpl->mpFrameData->mxClipboard.is())
        mpWindowImpl->mpFrameData->mxClipboard = GetSystemClipboard();
    return mpWindowImpl->mpFrameData->mxClipboard;
}

void Window::RecordLayoutData( vcl::ControlLayoutData* pLayout, const tools::Rectangle& rRect )
{
    assert(GetOutDev()->mpOutDevData);
    GetOutDev()->mpOutDevData->mpRecordLayout = pLayout;
    GetOutDev()->mpOutDevData->maRecordRect = rRect;
    Paint(*GetOutDev(), rRect);
    GetOutDev()->mpOutDevData->mpRecordLayout = nullptr;
}

void Window::DrawSelectionBackground( const tools::Rectangle& rRect,
                                      sal_uInt16 highlight,
                                      bool bChecked,
                                      bool bDrawBorder
                                      )
{
    if( rRect.IsEmpty() )
        return;

    const StyleSettings& rStyles = GetSettings().GetStyleSettings();

    // colors used for item highlighting
    Color aSelectionBorderCol( rStyles.GetHighlightColor() );
    Color aSelectionFillCol( aSelectionBorderCol );

    bool bDark = rStyles.GetFaceColor().IsDark();
    bool bBright = ( rStyles.GetFaceColor() == COL_WHITE );

    int c1 = aSelectionBorderCol.GetLuminance();
    int c2 = GetBackgroundColor().GetLuminance();

    if( !bDark && !bBright && abs( c2-c1 ) < 75 )
    {
        // contrast too low
        sal_uInt16 h,s,b;
        aSelectionFillCol.RGBtoHSB( h, s, b );
        if( b > 50 )    b -= 40;
        else            b += 40;
        aSelectionFillCol = Color::HSBtoRGB( h, s, b );
        aSelectionBorderCol = aSelectionFillCol;
    }

    tools::Rectangle aRect( rRect );
    GetOutDev()->Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);

    if( bDrawBorder )
        GetOutDev()->SetLineColor( bDark ? COL_WHITE : ( bBright ? COL_BLACK : aSelectionBorderCol ) );
    else
        GetOutDev()->SetLineColor();

    sal_uInt16 nPercent = 0;
    if( !highlight )
    {
        if( bDark )
            aSelectionFillCol = COL_BLACK;
        else
            nPercent = 80;  // just checked (light)
    }
    else
    {
        if( bChecked && highlight == 2 )
        {
            if( bDark )
                aSelectionFillCol = COL_LIGHTGRAY;
            else if ( bBright )
            {
                aSelectionFillCol = COL_BLACK;
                GetOutDev()->SetLineColor( COL_BLACK );
                nPercent = 0;
            }
            else
                nPercent = 20;          // selected, pressed or checked ( very dark )
        }
        else if( bChecked || highlight == 1 )
        {
            if( bDark )
                aSelectionFillCol = COL_GRAY;
            else if ( bBright )
            {
                aSelectionFillCol = COL_BLACK;
                GetOutDev()->SetLineColor( COL_BLACK );
                nPercent = 0;
            }
            else
                nPercent = 35;          // selected, pressed or checked ( very dark )
        }
        else
        {
            if( bDark )
                aSelectionFillCol = COL_LIGHTGRAY;
            else if ( bBright )
            {
                aSelectionFillCol = COL_BLACK;
                GetOutDev()->SetLineColor( COL_BLACK );
                if( highlight == 3 )
                    nPercent = 80;
                else
                    nPercent = 0;
            }
            else
                nPercent = 70;          // selected ( dark )
        }
    }

    GetOutDev()->SetFillColor( aSelectionFillCol );

    if( bDark )
    {
        GetOutDev()->DrawRect( aRect );
    }
    else
    {
        tools::Polygon aPoly( aRect );
        tools::PolyPolygon aPolyPoly( aPoly );
        GetOutDev()->DrawTransparent( aPolyPoly, nPercent );
    }

    GetOutDev()->Pop();
}

bool Window::IsScrollable() const
{
    // check for scrollbars
    VclPtr< vcl::Window > pChild = mpWindowImpl->mpFirstChild;
    while( pChild )
    {
        if( pChild->GetType() == WindowType::SCROLLBAR )
            return true;
        else
            pChild = pChild->mpWindowImpl->mpNext;
    }
    return false;
}

void Window::ImplMirrorFramePos( Point &pt ) const
{
    pt.setX(mpWindowImpl->mpFrame->GetWidth() - 1 - pt.X());
}

// frame based modal counter (dialogs are not modal to the whole application anymore)
bool Window::IsInModalMode() const
{
    return (mpWindowImpl->mpFrameWindow->mpWindowImpl->mpFrameData->mnModalMode != 0);
}

void Window::IncModalCount()
{
    vcl::Window* pFrameWindow = mpWindowImpl->mpFrameWindow;
    vcl::Window* pParent = pFrameWindow;
    while( pFrameWindow )
    {
        pFrameWindow->mpWindowImpl->mpFrameData->mnModalMode++;
        while( pParent && pParent->mpWindowImpl->mpFrameWindow == pFrameWindow )
        {
            pParent = pParent->GetParent();
        }
        pFrameWindow = pParent ? pParent->mpWindowImpl->mpFrameWindow.get() : nullptr;
    }
}
void Window::DecModalCount()
{
    vcl::Window* pFrameWindow = mpWindowImpl->mpFrameWindow;
    vcl::Window* pParent = pFrameWindow;
    while( pFrameWindow )
    {
        pFrameWindow->mpWindowImpl->mpFrameData->mnModalMode--;
        while( pParent && pParent->mpWindowImpl->mpFrameWindow == pFrameWindow )
        {
            pParent = pParent->GetParent();
        }
        pFrameWindow = pParent ? pParent->mpWindowImpl->mpFrameWindow.get() : nullptr;
    }
}

void Window::ImplIsInTaskPaneList( bool mbIsInTaskList )
{
    mpWindowImpl->mbIsInTaskPaneList = mbIsInTaskList;
}

void Window::ImplNotifyIconifiedState( bool bIconified )
{
    mpWindowImpl->mpFrameWindow->CallEventListeners( bIconified ? VclEventId::WindowMinimize : VclEventId::WindowNormalize );
    // #109206# notify client window as well to have toolkit topwindow listeners notified
    if( mpWindowImpl->mpFrameWindow->mpWindowImpl->mpClientWindow && mpWindowImpl->mpFrameWindow != mpWindowImpl->mpFrameWindow->mpWindowImpl->mpClientWindow )
        mpWindowImpl->mpFrameWindow->mpWindowImpl->mpClientWindow->CallEventListeners( bIconified ? VclEventId::WindowMinimize : VclEventId::WindowNormalize );
}

bool Window::HasActiveChildFrame() const
{
    bool bRet = false;
    vcl::Window *pFrameWin = ImplGetSVData()->maFrameData.mpFirstFrame;
    while( pFrameWin )
    {
        if( pFrameWin != mpWindowImpl->mpFrameWindow )
        {
            bool bDecorated = false;
            VclPtr< vcl::Window > pChildFrame = pFrameWin->ImplGetWindow();
            // #i15285# unfortunately WB_MOVEABLE is the same as WB_TABSTOP which can
            // be removed for ToolBoxes to influence the keyboard accessibility
            // thus WB_MOVEABLE is no indicator for decoration anymore
            // but FloatingWindows carry this information in their TitleType...
            // TODO: avoid duplicate WinBits !!!
            if( pChildFrame && pChildFrame->ImplIsFloatingWindow() )
                bDecorated = static_cast<FloatingWindow*>(pChildFrame.get())->GetTitleType() != FloatWinTitleType::NONE;
            if( bDecorated || (pFrameWin->mpWindowImpl->mnStyle & (WB_MOVEABLE | WB_SIZEABLE) ) )
                if( pChildFrame && pChildFrame->IsVisible() && pChildFrame->IsActive() )
                {
                    if( ImplIsChild( pChildFrame, true ) )
                    {
                        bRet = true;
                        break;
                    }
                }
        }
        pFrameWin = pFrameWin->mpWindowImpl->mpFrameData->mpNextFrame;
    }
    return bRet;
}

LanguageType Window::GetInputLanguage() const
{
    return mpWindowImpl->mpFrame->GetInputLanguage();
}

void Window::EnableNativeWidget( bool bEnable )
{
    static const char* pNoNWF = getenv( "SAL_NO_NWF" );
    if( pNoNWF && *pNoNWF )
        bEnable = false;

    if( bEnable != ImplGetWinData()->mbEnableNativeWidget )
    {
        ImplGetWinData()->mbEnableNativeWidget = bEnable;

        // send datachanged event to allow for internal changes required for NWF
        // like clipmode, transparency, etc.
        DataChangedEvent aDCEvt( DataChangedEventType::SETTINGS, &*GetOutDev()->moSettings, AllSettingsFlags::STYLE );
        CompatDataChanged( aDCEvt );

        // sometimes the borderwindow is queried, so keep it in sync
        if( mpWindowImpl->mpBorderWindow )
            mpWindowImpl->mpBorderWindow->ImplGetWinData()->mbEnableNativeWidget = bEnable;
    }

    // push down, useful for compound controls
    VclPtr< vcl::Window > pChild = mpWindowImpl->mpFirstChild;
    while( pChild )
    {
        pChild->EnableNativeWidget( bEnable );
        pChild = pChild->mpWindowImpl->mpNext;
    }
}

bool Window::IsNativeWidgetEnabled() const
{
    return mpWindowImpl && ImplGetWinData()->mbEnableNativeWidget;
}

Reference< css::rendering::XCanvas > WindowOutputDevice::ImplGetCanvas( bool bSpriteCanvas ) const
{
    // Feed any with operating system's window handle

    // common: first any is VCL pointer to window (for VCL canvas)
    Sequence< Any > aArg{
        Any(reinterpret_cast<sal_Int64>(this)),
        Any(css::awt::Rectangle( mnOutOffX, mnOutOffY, mnOutWidth, mnOutHeight )),
        Any(mxOwnerWindow->mpWindowImpl->mbAlwaysOnTop),
        Any(Reference< css::awt::XWindow >(
                             mxOwnerWindow->GetComponentInterface(),
                             UNO_QUERY )),
        GetSystemGfxDataAny()
    };

    const Reference< XComponentContext >& xContext = comphelper::getProcessComponentContext();

    // Create canvas instance with window handle

    static tools::DeleteUnoReferenceOnDeinit<XMultiComponentFactory> xStaticCanvasFactory(
        css::rendering::CanvasFactory::create( xContext ) );
    Reference<XMultiComponentFactory> xCanvasFactory(xStaticCanvasFactory.get());
    Reference< css::rendering::XCanvas > xCanvas;

    if(xCanvasFactory.is())
    {
#ifdef _WIN32
        // see #140456# - if we're running on a multiscreen setup,
        // request special, multi-screen safe sprite canvas
        // implementation (not DX5 canvas, as it cannot cope with
        // surfaces spanning multiple displays). Note: canvas
        // (without sprite) stays the same)
        const sal_uInt32 nDisplay = static_cast< WinSalFrame* >( mxOwnerWindow->mpWindowImpl->mpFrame )->mnDisplay;
        if( nDisplay >= Application::GetScreenCount() )
        {
            xCanvas.set( xCanvasFactory->createInstanceWithArgumentsAndContext(
                                 bSpriteCanvas ?
                                 OUString( "com.sun.star.rendering.SpriteCanvas.MultiScreen" ) :
                                 OUString( "com.sun.star.rendering.Canvas.MultiScreen" ),
                                 aArg,
                                 xContext ),
                             UNO_QUERY );

        }
        else
#endif
        {
            xCanvas.set( xCanvasFactory->createInstanceWithArgumentsAndContext(
                             bSpriteCanvas ?
                             u"com.sun.star.rendering.SpriteCanvas"_ustr :
                             u"com.sun.star.rendering.Canvas"_ustr,
                             aArg,
                             xContext ),
                         UNO_QUERY );

        }
    }

    // no factory??? Empty reference, then.
    return xCanvas;
}

OUString Window::GetSurroundingText() const
{
  return OUString();
}

Selection Window::GetSurroundingTextSelection() const
{
  return Selection( 0, 0 );
}

namespace
{
    using namespace com::sun::star;

    uno::Reference<accessibility::XAccessibleEditableText> lcl_GetxText(vcl::Window *pFocusWin)
    {
        uno::Reference<accessibility::XAccessibleEditableText> xText;
        try
        {
            uno::Reference< accessibility::XAccessible > xAccessible( pFocusWin->GetAccessible() );
            if (xAccessible.is())
                xText = FindFocusedEditableText(xAccessible->getAccessibleContext());
        }
        catch(const uno::Exception&)
        {
            TOOLS_WARN_EXCEPTION( "vcl.gtk3", "Exception in getting input method surrounding text");
        }
        return xText;
    }
}

// this is a rubbish implementation using a11y, ideally all subclasses implementing
// GetSurroundingText/GetSurroundingTextSelection should implement this and then this
// should be removed in favor of a stub that returns false
bool Window::DeleteSurroundingText(const Selection& rSelection)
{
    uno::Reference<accessibility::XAccessibleEditableText> xText = lcl_GetxText(this);
    if (xText.is())
    {
        sal_Int32 nPosition = xText->getCaretPosition();
        // #i111768# range checking
        sal_Int32 nDeletePos = rSelection.Min();
        sal_Int32 nDeleteEnd = rSelection.Max();
        if (nDeletePos < 0)
            nDeletePos = 0;
        if (nDeleteEnd < 0)
            nDeleteEnd = 0;
        if (nDeleteEnd > xText->getCharacterCount())
            nDeleteEnd = xText->getCharacterCount();

        xText->deleteText(nDeletePos, nDeleteEnd);
        //tdf91641 adjust cursor if deleted chars shift it forward (normal case)
        if (nDeletePos < nPosition)
        {
            if (nDeleteEnd <= nPosition)
                nPosition = nPosition - (nDeleteEnd - nDeletePos);
            else
                nPosition = nDeletePos;

            if (xText->getCharacterCount() >= nPosition)
                xText->setCaretPosition( nPosition );
        }
        return true;
    }

    return false;
}

bool WindowOutputDevice::UsePolyPolygonForComplexGradient()
{
    return meRasterOp != RasterOp::OverPaint;
}

void Window::ApplySettings(vcl::RenderContext& /*rRenderContext*/)
{
}

const SystemEnvData* Window::GetSystemData() const
{

    return mpWindowImpl->mpFrame ? &mpWindowImpl->mpFrame->GetSystemData() : nullptr;
}

bool Window::SupportsDoubleBuffering() const
{
    return mpWindowImpl->mpFrameData->mpBuffer;
}

void Window::RequestDoubleBuffering(bool bRequest)
{
    if (bRequest)
    {
        mpWindowImpl->mpFrameData->mpBuffer = VclPtrInstance<VirtualDevice>();
        // Make sure that the buffer size matches the frame size.
        mpWindowImpl->mpFrameData->mpBuffer->SetOutputSizePixel(mpWindowImpl->mpFrameWindow->GetOutputSizePixel());
    }
    else
        mpWindowImpl->mpFrameData->mpBuffer.reset();
}

/*
 * The rationale here is that we moved destructors to
 * dispose and this altered a lot of code paths, that
 * are better left unchanged for now.
 */
void Window::CompatGetFocus()
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::GetFocus();
    else
        GetFocus();
}

void Window::CompatLoseFocus()
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::LoseFocus();
    else
        LoseFocus();
}

void Window::CompatStateChanged( StateChangedType nStateChange )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::StateChanged(nStateChange);
    else
        StateChanged(nStateChange);
}

void Window::CompatDataChanged( const DataChangedEvent& rDCEvt )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        Window::DataChanged(rDCEvt);
    else
        DataChanged(rDCEvt);
}

bool Window::CompatPreNotify( NotifyEvent& rNEvt )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        return Window::PreNotify( rNEvt );
    else
        return PreNotify( rNEvt );
}

bool Window::CompatNotify( NotifyEvent& rNEvt )
{
    if (!mpWindowImpl || mpWindowImpl->mbInDispose)
        return Window::EventNotify( rNEvt );
    else
        return EventNotify( rNEvt );
}

void Window::set_id(const OUString& rID)
{
    mpWindowImpl->maID = rID;
}

const OUString& Window::get_id() const
{
    static OUString empty;
    return mpWindowImpl ? mpWindowImpl->maID : empty;
}

FactoryFunction Window::GetUITestFactory() const
{
    return WindowUIObject::create;
}

WindowOutputDevice::WindowOutputDevice(vcl::Window& rOwnerWindow) :
    ::OutputDevice(OUTDEV_WINDOW),
    mxOwnerWindow(&rOwnerWindow)
{
    assert(mxOwnerWindow);
}

WindowOutputDevice::~WindowOutputDevice()
{
    disposeOnce();
}

void WindowOutputDevice::dispose()
{
    assert((!mxOwnerWindow || mxOwnerWindow->isDisposed()) && "This belongs to the associated window and must be disposed after it");
    ::OutputDevice::dispose();
    // need to do this after OutputDevice::dispose so that the call to WindowOutputDevice::ReleaseGraphics
    // can release the graphics properly
    mxOwnerWindow.reset();
}

css::awt::DeviceInfo WindowOutputDevice::GetDeviceInfo() const
{
    css::awt::DeviceInfo aInfo = GetCommonDeviceInfo(mxOwnerWindow->GetSizePixel());
    mxOwnerWindow->GetBorder(aInfo.LeftInset, aInfo.TopInset, aInfo.RightInset, aInfo.BottomInset);
    return aInfo;
}


} /* namespace vcl */

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
