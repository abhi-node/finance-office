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

#include <config_wasm_strip.h>

#include <svl/itemiter.hxx>
#include <vcl/imap.hxx>
#include <tools/helpers.hxx>
#include <editeng/protitem.hxx>
#include <editeng/opaqitem.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/frmdiritem.hxx>
#include <fmtfsize.hxx>
#include <fmtclds.hxx>
#include <fmtcntnt.hxx>
#include <fmturl.hxx>
#include <fmtsrnd.hxx>
#include <fmtornt.hxx>
#include <fmtcnct.hxx>
#include <ndgrf.hxx>
#include <tolayoutanchoredobjectposition.hxx>
#include <fmtfollowtextflow.hxx>
#include <sortedobjs.hxx>
#include <objectformatter.hxx>
#include <ndole.hxx>
#include <swtable.hxx>
#include <svx/svdoashp.hxx>
#include <svx/svdpage.hxx>
#include <layouter.hxx>
#include <layact.hxx>
#include <pagefrm.hxx>
#include <rootfrm.hxx>
#include <viewimp.hxx>
#include <viewopt.hxx>
#include <dcontact.hxx>
#include <dflyobj.hxx>
#include <dview.hxx>
#include <frmatr.hxx>
#include <frmtool.hxx>
#include <hints.hxx>
#include <tabfrm.hxx>
#include <txtfrm.hxx>
#include <notxtfrm.hxx>
#include <flyfrms.hxx>
#include <sectfrm.hxx>
#include <vcl/svapp.hxx>
#include <calbck.hxx>
#include <IDocumentDrawModelAccess.hxx>
#include <IDocumentSettingAccess.hxx>
#include <IDocumentLayoutAccess.hxx>
#include <textboxhelper.hxx>
#include <txtfly.hxx>
#include <ndindex.hxx>
#include <basegfx/matrix/b2dhommatrixtools.hxx>
#include <osl/diagnose.h>
#include <o3tl/string_view.hxx>
#include <rtl/math.hxx>

#include <wrtsh.hxx>
#include <view.hxx>
#include <edtwin.hxx>
#include <bodyfrm.hxx>
#include <FrameControlsManager.hxx>
#include <ndtxt.hxx>
#include <formatflysplit.hxx>

using namespace ::com::sun::star;

namespace
{
/// Gets the bottom position which is a deadline for a split fly.
SwTwips GetFlyAnchorBottom(SwFlyFrame* pFly, const SwFrame& rAnchor)
{
    SwRectFnSet aRectFnSet(pFly);

    const SwPageFrame* pPage = rAnchor.FindPageFrame();
    if (!pPage)
    {
        return 0;
    }

    const SwFrame* pBody = pPage->FindBodyCont();
    if (!pBody)
    {
        return 0;
    }

    const auto* pFrameFormat = pFly->GetFrameFormat();
    const IDocumentSettingAccess& rIDSA = pFrameFormat->getIDocumentSettingAccess();
    // Allow overlap with bottom margin / footer only in case we're relative to the page frame.
    bool bVertPageFrame = pFrameFormat->GetVertOrient().GetRelationOrient() == text::RelOrientation::PAGE_FRAME;
    bool bInBody = rAnchor.IsInDocBody();
    bool bLegacy = rIDSA.get(DocumentSettingId::TAB_OVER_MARGIN) && (bVertPageFrame || !bInBody);
    if (bLegacy)
    {
        // Word <= 2010 style: the fly can overlap with the bottom margin / footer area in case the
        // fly height fits the body height and the fly bottom fits the page.
        // See if the fly height would fit at least the page height, ignoring the vertical offset.
        SwTwips nFlyHeight = aRectFnSet.GetHeight(pFly->getFrameArea());
        SwTwips nPageHeight = aRectFnSet.GetHeight(pPage->getFramePrintArea());
        SwTwips nFlyTop = aRectFnSet.GetTop(pFly->getFrameArea());
        SwTwips nBodyTop = aRectFnSet.GetTop(pBody->getFrameArea());
        if (nFlyTop < nBodyTop)
        {
            // Fly frame overlaps with the top margin area, ignore that part of the fly frame for
            // top/height purposes.
            nFlyHeight -= nBodyTop - nFlyTop;
            nFlyTop = nBodyTop;
        }
        if (nFlyHeight <= nPageHeight)
        {
            // Yes, it would fit: allow overlap if there is no problematic vertical offset.
            SwTwips nDeadline = aRectFnSet.GetBottom(pPage->getFrameArea());
            SwTwips nBodyHeight = aRectFnSet.GetHeight(pBody->getFramePrintArea());
            if (nDeadline - nFlyTop > nBodyHeight)
            {
                // If the fly would now grow to nDeadline then it would not fit the body height, so
                // limit the height.
                nDeadline = nFlyTop + nBodyHeight;
            }
            return nDeadline;
        }
    }

    // Word >= 2013 style: the fly has to stay inside the body frame.
    return aRectFnSet.GetPrtBottom(*pBody);
}
}

static SwTwips lcl_CalcAutoWidth( const SwLayoutFrame& rFrame );

SwFlyFrame::SwFlyFrame( SwFlyFrameFormat *pFormat, SwFrame* pSib, SwFrame *pAnch, bool bFollow ) :
    SwLayoutFrame( pFormat, pSib ),
     // #i26791#
    m_pPrevLink( nullptr ),
    m_pNextLink( nullptr ),
    m_bInCnt( false ),
    m_bAtCnt( false ),
    m_bLayout( false ),
    m_bAutoPosition( false ),
    m_bDeleted( false ),
    m_nAuthor( std::string::npos ),
    m_bValidContentPos( false )
{
    mnFrameType = SwFrameType::Fly;

    m_bInvalid = m_bNotifyBack = true;
    m_bLocked  = m_bMinHeight =
    m_bHeightClipped = m_bWidthClipped = m_bFormatHeightOnly = false;

    // Size setting: Fixed size is always the width
    const SwFormatFrameSize &rFrameSize = pFormat->GetFrameSize();
    const SvxFrameDirection nDir = pFormat->GetFormatAttr( RES_FRAMEDIR ).GetValue();
    if( SvxFrameDirection::Environment == nDir )
    {
        mbDerivedVert = true;
        mbDerivedR2L = true;
    }
    else
    {
        mbInvalidVert = false;
        mbDerivedVert = false;
        mbDerivedR2L = false;
        if( SvxFrameDirection::Horizontal_LR_TB == nDir || SvxFrameDirection::Horizontal_RL_TB == nDir )
        {
            mbVertLR = false;
            mbVertical = false;
        }
        else
        {
            const SwViewShell *pSh = getRootFrame() ? getRootFrame()->GetCurrShell() : nullptr;
            if( pSh && pSh->GetViewOptions()->getBrowseMode() )
            {
                mbVertLR = false;
                mbVertical = false;
            }
            else
            {
                mbVertical = true;

                if ( SvxFrameDirection::Vertical_LR_TB == nDir )
                    mbVertLR = true;
                else if (nDir == SvxFrameDirection::Vertical_LR_BT)
                {
                    mbVertLR = true;
                    mbVertLRBT = true;
                }
                else
                    mbVertLR = false;
            }
        }

        mbInvalidR2L = false;
        if( SvxFrameDirection::Horizontal_RL_TB == nDir )
            mbRightToLeft = true;
        else
            mbRightToLeft = false;
    }

    {
        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
        aFrm.Width( rFrameSize.GetWidth() );
        aFrm.Height( rFrameSize.GetHeightSizeType() == SwFrameSize::Variable ? MINFLY : rFrameSize.GetHeight() );
    }

    // Fixed or variable Height?
    if ( rFrameSize.GetHeightSizeType() == SwFrameSize::Minimum )
        m_bMinHeight = true;
    else if ( rFrameSize.GetHeightSizeType() == SwFrameSize::Fixed )
        mbFixSize = true;

    // insert columns, if necessary
    InsertColumns();

    // First the Init, then the Content:
    // This is due to the fact that the Content may have Objects/Frames,
    // which are then registered
    InitDrawObj(*pAnch);

    Chain( pAnch );

    if (!bFollow)
    {
        InsertCnt();
    }

    // Put it somewhere outside so that out document is not formatted unnecessarily often
    SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
    aFrm.Pos().setX(FAR_AWAY);
    aFrm.Pos().setY(FAR_AWAY);
}

void SwFlyFrame::Chain( SwFrame* _pAnch )
{
    // Connect to chain neighbours.
    // No problem, if a neighbor doesn't exist - the construction of the
    // neighbor will make the connection
    const SwFormatChain& rChain = GetFormat()->GetChain();
    if ( !(rChain.GetPrev() || rChain.GetNext()) )
        return;

    if ( rChain.GetNext() )
    {
        SwFlyFrame* pFollow = FindChainNeighbour( *rChain.GetNext(), _pAnch );
        if ( pFollow )
        {
            OSL_ENSURE( !pFollow->GetPrevLink(), "wrong chain detected" );
            if ( !pFollow->GetPrevLink() )
                SwFlyFrame::ChainFrames( *this, *pFollow );
        }
    }
    if ( rChain.GetPrev() )
    {
        SwFlyFrame *pMaster = FindChainNeighbour( *rChain.GetPrev(), _pAnch );
        if ( pMaster )
        {
            OSL_ENSURE( !pMaster->GetNextLink(), "wrong chain detected" );
            if ( !pMaster->GetNextLink() )
                SwFlyFrame::ChainFrames( *pMaster, *this );
        }
    }
}

void SwFlyFrame::InsertCnt()
{
    if ( GetPrevLink() )
        return;

    const SwFormatContent& rContent = GetFormat()->GetContent();
    OSL_ENSURE( rContent.GetContentIdx(), ":-( no content prepared." );
    SwNodeOffset nIndex = rContent.GetContentIdx()->GetIndex();
    // Lower() means SwColumnFrame; the Content then needs to be inserted into the (Column)BodyFrame
    ::InsertCnt_( Lower() ? static_cast<SwLayoutFrame*>(static_cast<SwLayoutFrame*>(Lower())->Lower()) : static_cast<SwLayoutFrame*>(this),
                  GetFormat()->GetDoc(), nIndex );

    // NoText always have a fixed height.
    SwFrame* pLower = Lower();
    if ( pLower && pLower->IsNoTextFrame() )
    {
        mbFixSize = true;
        m_bMinHeight = false;
    }
}

void SwFlyFrame::InsertColumns()
{
    // #i97379#
    // Check, if column are allowed.
    // Columns are not allowed for fly frames, which represent graphics or embedded objects.
    const SwFormatContent& rContent = GetFormat()->GetContent();
    OSL_ENSURE( rContent.GetContentIdx(), "<SwFlyFrame::InsertColumns()> - no content prepared." );
    SwNodeIndex aFirstContent( *(rContent.GetContentIdx()), 1 );
    if ( aFirstContent.GetNode().IsNoTextNode() )
    {
        return;
    }

    const SwFormatCol &rCol = GetFormat()->GetCol();
    if ( rCol.GetNumCols() <= 1 )
        return;

    // Start off PrtArea to be as large as Frame, so that we can put in the columns
    // properly. It'll adjust later on.
    {
        SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
        aPrt.Width( getFrameArea().Width() );
        aPrt.Height( getFrameArea().Height() );
    }

    const SwFormatCol aOld; // ChgColumns() also needs an old value passed
    ChgColumns( aOld, rCol );
}

void SwFlyFrame::DestroyImpl()
{
    // Accessible objects for fly frames will be destroyed in this destructor.
    // For frames bound as char or frames that don't have an anchor we have
    // to do that ourselves. For any other frame the call RemoveFly at the
    // anchor will do that.
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
    if( IsAccessibleFrame() && GetFormat() && (IsFlyInContentFrame() || !GetAnchorFrame()) )
    {
        SwRootFrame *pRootFrame = getRootFrame();
        if( pRootFrame && pRootFrame->IsAnyShellAccessible() )
        {
            SwViewShell *pVSh = pRootFrame->GetCurrShell();
            if( pVSh && pVSh->Imp() )
            {
                // Lowers aren't disposed already, so we have to do a recursive
                // dispose
                pVSh->Imp()->DisposeAccessibleFrame( this, true );
            }
        }
    }
#endif

    if( GetFormat() && !GetFormat()->GetDoc().IsInDtor() )
    {
        ClearTmpConsiderWrapInfluence(); // remove this from SwLayouter

        Unchain();

        DeleteCnt();

        if ( GetAnchorFrame() )
            AnchorFrame()->RemoveFly( this );
    }

    FinitDrawObj();

    SwLayoutFrame::DestroyImpl();

    SwWrtShell* pWrtSh = dynamic_cast<SwWrtShell*>(getRootFrame()->GetCurrShell());
    UpdateUnfloatButton(pWrtSh, false);
}

SwFlyFrame::~SwFlyFrame()
{
}

const IDocumentDrawModelAccess& SwFlyFrame::getIDocumentDrawModelAccess()
{
    return GetFormat()->getIDocumentDrawModelAccess();
}

void SwFlyFrame::Unchain()
{
    if ( GetPrevLink() )
        UnchainFrames( *GetPrevLink(), *this );
    if ( GetNextLink() )
        UnchainFrames( *this, *GetNextLink() );
}

void SwFlyFrame::DeleteCnt()
{
    SwFrame* pFrame = m_pLower;
    while ( pFrame )
    {
        while ( pFrame->GetDrawObjs() && pFrame->GetDrawObjs()->size() )
        {
            SwAnchoredObject *pAnchoredObj = (*pFrame->GetDrawObjs())[0];
            if ( auto pFlyFrame = pAnchoredObj->DynCastFlyFrame() )
            {
                SwFrame::DestroyFrame(pFlyFrame);
            }
            else if ( dynamic_cast<const SwAnchoredDrawObject*>( pAnchoredObj) !=  nullptr )
            {
                // consider 'virtual' drawing objects
                SdrObject* pObj = pAnchoredObj->DrawObj();
                if ( auto pDrawVirtObj = dynamic_cast<SwDrawVirtObj*>( pObj) )
                {
                    pDrawVirtObj->RemoveFromWriterLayout();
                    pDrawVirtObj->RemoveFromDrawingPage();
                }
                else
                {
                    SwDrawContact* pContact =
                            static_cast<SwDrawContact*>(::GetUserCall( pObj ));
                    if ( pContact )
                    {
                        pContact->DisconnectFromLayout();
                    }
                }
            }
        }

        pFrame->RemoveFromLayout();
        SwFrame::DestroyFrame(pFrame);
        pFrame = m_pLower;
    }

    InvalidatePage();
}

void SwFlyFrame::InitDrawObj(SwFrame& rAnchorFrame)
{
    SetDrawObj(*SwFlyDrawContact::CreateNewRef(this, GetFormat(), rAnchorFrame));

    // Set the right Layer
    const IDocumentSettingAccess& rIDSA = GetFormat()->getIDocumentSettingAccess();
    bool isPaintHellOverHF = rIDSA.get(DocumentSettingId::PAINT_HELL_OVER_HEADER_FOOTER);
    IDocumentDrawModelAccess& rIDDMA = GetFormat()->getIDocumentDrawModelAccess();
    SdrLayerID nHeavenId = rIDDMA.GetHeavenId();
    SdrLayerID nHellId = rIDDMA.GetHellId();
    bool isOpaque = GetFormat()->GetOpaque().GetValue();
    if (!isOpaque && isPaintHellOverHF)
    {
        if (!rAnchorFrame.FindFooterOrHeader())
            nHellId = rIDDMA.GetHeaderFooterHellId();
    }
    GetVirtDrawObj()->SetLayer( isOpaque ? nHeavenId :nHellId );
}

static SwPosition ResolveFlyAnchor(SwFrameFormat const& rFlyFrame)
{
    SwFormatAnchor const& rAnch(rFlyFrame.GetAnchor());
    if (rAnch.GetAnchorId() == RndStdIds::FLY_AT_PAGE)
    {   // arbitrarily pick last node
        return SwPosition(rFlyFrame.GetDoc().GetNodes().GetEndOfContent(), SwNodeOffset(-1));
    }
    else
    {
        SwPosition const*const pPos(rAnch.GetContentAnchor());
        assert(pPos);
        if (SwFrameFormat const*const pParent = pPos->GetNode().GetFlyFormat())
        {
            return ResolveFlyAnchor(*pParent);
        }
        else if (pPos->GetContentNode())
        {
            return *pPos;
        }
        else
        {
            return SwPosition(*pPos->GetNode().GetContentNode(), 0);
        }
    }
}

void SwFlyFrame::FinitDrawObj()
{
    if(!GetVirtDrawObj() )
        return;
    SwFormat* pFormat = GetFormat();
    // Deregister from SdrPageViews if the Objects is still selected there.
    if(!pFormat->GetDoc().IsInDtor())
    {
        SwViewShell* p1St = getRootFrame()->GetCurrShell();
        if(p1St)
        {
            for(SwViewShell& rCurrentShell : p1St->GetRingContainer())
            {   // At the moment the Drawing can do just do an Unmark on everything,
                // as the Object was already removed
                if (rCurrentShell.HasDrawView() &&
                    rCurrentShell.Imp()->GetDrawView()->GetMarkedObjectList().GetMarkCount())
                {
                    SwFlyFrame const*const pOldSelFly = ::GetFlyFromMarked(nullptr, &rCurrentShell);
                    if (pOldSelFly == this)
                    {
                        assert(rCurrentShell.Imp()->GetDrawView()->GetMarkedObjectList().GetMarkCount() == 1);
                        if (SwFEShell *const pFEShell = dynamic_cast<SwFEShell*>(&rCurrentShell))
                        {   // tdf#131679 move any cursor out of fly
                            rCurrentShell.Imp()->GetDrawView()->UnmarkAll();
                            SwPaM const temp(ResolveFlyAnchor(*pOldSelFly->GetFormat()));
                            pFEShell->SetSelection(temp);
                            // could also call SetCursor() like SwFEShell::SelectObj()
                            // does, but that would access layout a bit much...
                        }
                        else
                        {
                            rCurrentShell.Imp()->GetDrawView()->UnmarkAll();
                        }
                    }
                }
            }
        }
    }

    SwVirtFlyDrawObj* pVirtDrawObj = GetVirtDrawObj();
    // Else calls delete of the ContactObj
    pVirtDrawObj->SetUserCall(nullptr);

    if ( pVirtDrawObj->getSdrPageFromSdrObject() )
        pVirtDrawObj->getSdrPageFromSdrObject()->RemoveObject( pVirtDrawObj->GetOrdNum() );
    ClearDrawObj();
}

void SwFlyFrame::ChainFrames( SwFlyFrame &rMaster, SwFlyFrame &rFollow )
{
    OSL_ENSURE( !rMaster.GetNextLink(), "link can not be changed" );
    OSL_ENSURE( !rFollow.GetPrevLink(), "link can not be changed" );

    rMaster.m_pNextLink = &rFollow;
    rFollow.m_pPrevLink = &rMaster;

    if ( rMaster.ContainsContent() )
    {
        // To get a text flow we need to invalidate
        SwFrame *pInva = rMaster.FindLastLower();
        SwRectFnSet aRectFnSet(&rMaster);
        const tools::Long nBottom = aRectFnSet.GetPrtBottom(rMaster);
        while ( pInva )
        {
            if( aRectFnSet.BottomDist( pInva->getFrameArea(), nBottom ) <= 0 )
            {
                pInva->InvalidateSize();
                pInva->Prepare();
                pInva = pInva->FindPrev();
            }
            else
                pInva = nullptr;
        }
    }

    if ( rFollow.ContainsContent() )
    {
        // There's only the content from the Masters left; the content from the Follow
        // does not have any Frames left (should always be exactly one empty TextNode).
        SwFrame *pFrame = rFollow.ContainsContent();
        OSL_ENSURE( !pFrame->IsTabFrame() && !pFrame->FindNext(), "follow in chain contains content" );
        pFrame->Cut();
        SwFrame::DestroyFrame(pFrame);
    }

    // invalidate accessible relation set (accessibility wrapper)
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
    SwViewShell* pSh = rMaster.getRootFrame()->GetCurrShell();
    if( pSh )
    {
        SwRootFrame* pLayout = rMaster.getRootFrame();
        if( pLayout && pLayout->IsAnyShellAccessible() )
            pSh->Imp()->InvalidateAccessibleRelationSet(rMaster, rFollow);
    }
#endif
}

void SwFlyFrame::UnchainFrames( SwFlyFrame &rMaster, SwFlyFrame &rFollow )
{
    rMaster.m_pNextLink = nullptr;
    rFollow.m_pPrevLink = nullptr;

    if ( rFollow.ContainsContent() )
    {
        // The Master sucks up the content of the Follow
        SwLayoutFrame *pUpper = &rMaster;
        SwFrame* pLower = pUpper->Lower();
        if ( pLower && pLower->IsColumnFrame() )
        {
            pUpper = static_cast<SwLayoutFrame*>(pUpper->GetLastLower());
            pUpper = static_cast<SwLayoutFrame*>(pUpper->Lower()); // The (Column)BodyFrame
            OSL_ENSURE( pUpper && pUpper->IsColBodyFrame(), "Missing ColumnBody" );
        }
        SwFlyFrame *pFoll = &rFollow;
        while ( pFoll )
        {
            SwFrame *pTmp = ::SaveContent( pFoll );
            if ( pTmp )
                ::RestoreContent( pTmp, pUpper, rMaster.FindLastLower() );
            pFoll->SetCompletePaint();
            pFoll->InvalidateSize();
            pFoll = pFoll->GetNextLink();
        }
    }

    // The Follow needs his own content to be served
    const SwFormatContent &rContent = rFollow.GetFormat()->GetContent();
    OSL_ENSURE( rContent.GetContentIdx(), ":-( No content prepared." );
    SwNodeOffset nIndex = rContent.GetContentIdx()->GetIndex();
    // Lower() means SwColumnFrame: this one contains another SwBodyFrame
    SwFrame* pLower = rFollow.Lower();
    ::InsertCnt_( pLower ? const_cast<SwLayoutFrame*>(static_cast<const SwLayoutFrame*>(static_cast<const SwLayoutFrame*>(pLower)->Lower()))
                                   : static_cast<SwLayoutFrame*>(&rFollow),
                  rFollow.GetFormat()->GetDoc(), ++nIndex );

    // invalidate accessible relation set (accessibility wrapper)
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
    SwViewShell* pSh = rMaster.getRootFrame()->GetCurrShell();
    if( pSh )
    {
        SwRootFrame* pLayout = rMaster.getRootFrame();
        if( pLayout && pLayout->IsAnyShellAccessible() )
            pSh->Imp()->InvalidateAccessibleRelationSet(rMaster, rFollow);
    }
#endif
}

SwFlyFrame *SwFlyFrame::FindChainNeighbour( SwFrameFormat const &rChain, SwFrame *pAnch )
{
    // We look for the Fly that's in the same Area.
    // Areas can for now only be Head/Footer or Flys.

    if ( !pAnch ) // If an Anchor was passed along, that one counts (ctor!)
        pAnch = AnchorFrame();

    SwLayoutFrame *pLay;
    if ( pAnch->IsInFly() )
        pLay = pAnch->FindFlyFrame();
    else
    {
        // FindFooterOrHeader is not appropriate here, as we may not have a
        // connection to the Anchor yet.
        pLay = pAnch->GetUpper();
        while ( pLay && !(pLay->GetType() & (SwFrameType::Header|SwFrameType::Footer)) )
            pLay = pLay->GetUpper();
    }

    SwIterator<SwFlyFrame,SwFormat> aIter( rChain );
    SwFlyFrame *pFly = aIter.First();
    if ( pLay )
    {
        while ( pFly )
        {
            if ( pFly->GetAnchorFrame() )
            {
                if ( pFly->GetAnchorFrame()->IsInFly() )
                {
                    if ( pFly->AnchorFrame()->FindFlyFrame() == pLay )
                        break;
                }
                else if ( pLay == pFly->FindFooterOrHeader() )
                    break;
            }
            pFly = aIter.Next();
        }
    }
    else if ( pFly )
    {
        OSL_ENSURE( !aIter.Next(), "chain with more than one instance" );
    }
    return pFly;
}

bool SwFlyFrame::IsFlySplitAllowed() const
{
    if (!IsFlyAtContentFrame())
    {
        return false;
    }

    const IDocumentSettingAccess& rIDSA = GetFormat()->getIDocumentSettingAccess();
    if (rIDSA.get(DocumentSettingId::DO_NOT_BREAK_WRAPPED_TABLES))
    {
        return false;
    }

    if (FindFooterOrHeader())
    {
        // Adding a new page would not increase the header/footer area.
        return false;
    }

    const SwFrame* pFlyAnchor = GetAnchorFrame();
    if (pFlyAnchor && pFlyAnchor->FindColFrame())
    {
        // No split in multi-column sections, so GetFlyAnchorBottom() can assume that our innermost
        // body frame and the page's body frame is the same.
        // This is also consistent with the Word behavior.
        return false;
    }

    if (pFlyAnchor && pFlyAnchor->IsInFootnote())
    {
        // No split in footnotes.
        return false;
    }

    const SwFlyFrameFormat* pFormat = GetFormat();
    const SwFormatVertOrient& rVertOrient = pFormat->GetVertOrient();
    if (rVertOrient.GetVertOrient() == text::VertOrientation::BOTTOM)
    {
        // We have to grow from bottom to top, and the fly split code assumes that we grow from top
        // to bottom, so don't split for now.
        if (rVertOrient.GetRelationOrient() == text::RelOrientation::PAGE_PRINT_AREA)
        {
            // Growing from the bottom of the body frame.
            return false;
        }
    }

    return pFormat->GetFlySplit().GetValue();
}

SwFrame *SwFlyFrame::FindLastLower()
{
    SwFrame *pRet = ContainsAny();
    if ( pRet && pRet->IsInTab() )
        pRet = pRet->FindTabFrame();
    SwFrame *pNxt = pRet;
    while ( pNxt && IsAnLower( pNxt ) )
    {   pRet = pNxt;
        pNxt = pNxt->FindNext();
    }
    return pRet;
}

bool SwFlyFrame::FrameSizeChg( const SwFormatFrameSize &rFrameSize )
{
    bool bRet = false;
    SwTwips nDiffHeight = getFrameArea().Height();
    if ( rFrameSize.GetHeightSizeType() == SwFrameSize::Variable )
        mbFixSize = m_bMinHeight = false;
    else
    {
        if ( rFrameSize.GetHeightSizeType() == SwFrameSize::Fixed )
        {
            mbFixSize = true;
            m_bMinHeight = false;
        }
        else if ( rFrameSize.GetHeightSizeType() == SwFrameSize::Minimum )
        {
            mbFixSize = false;
            m_bMinHeight = true;
        }
        nDiffHeight -= rFrameSize.GetHeight();
    }
    // If the Fly contains columns, we already need to set the Fly
    // and the Columns to the required value or else we run into problems.
    if (SwFrame* pLower = Lower())
    {
        if ( pLower->IsColumnFrame() )
        {
            const SwRect aOld( GetObjRectWithSpaces() );
            const Size   aOldSz( getFramePrintArea().SSize() );
            const SwTwips nDiffWidth = getFrameArea().Width() - rFrameSize.GetWidth();

            {
                SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
                aFrm.Height( aFrm.Height() - nDiffHeight );
                aFrm.Width ( aFrm.Width()  - nDiffWidth  );
            }

            // #i68520#
            InvalidateObjRectWithSpaces();

            {
                SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
                aPrt.Height( aPrt.Height() - nDiffHeight );
                aPrt.Width ( aPrt.Width()  - nDiffWidth  );
            }

            ChgLowersProp( aOldSz );
            ::Notify( this, FindPageFrame(), aOld );
            setFrameAreaPositionValid(false);
            bRet = true;
        }
        else if ( pLower->IsNoTextFrame() )
        {
            mbFixSize = true;
            m_bMinHeight = false;
        }
    }
    return bRet;
}

void SwFlyFrame::SwClientNotify(const SwModify& rMod, const SfxHint& rHint)
{
    if (rHint.GetId() == SfxHintId::SwFormatChange ||
        rHint.GetId() == SfxHintId::SwLegacyModify ||
        rHint.GetId() == SfxHintId::SwAttrSetChange)
    {
        SwFlyFrameInvFlags eInvFlags = SwFlyFrameInvFlags::NONE;
        if (rHint.GetId() == SfxHintId::SwFormatChange)
        {
            auto pChangeHint = static_cast<const SwFormatChangeHint*>(&rHint);
            UpdateAttrForFormatChange(pChangeHint->m_pOldFormat, pChangeHint->m_pNewFormat, eInvFlags);
        }
        else if (rHint.GetId() == SfxHintId::SwAttrSetChange)
        {
            auto pChangeHint = static_cast<const sw::AttrSetChangeHint*>(&rHint);
            if(pChangeHint->m_pNew && pChangeHint->m_pOld)
            {
                SfxItemIter aNIter(*pChangeHint->m_pNew->GetChgSet());
                SfxItemIter aOIter(*pChangeHint->m_pOld->GetChgSet());
                const SfxPoolItem* pNItem = aNIter.GetCurItem();
                const SfxPoolItem* pOItem = aOIter.GetCurItem();
                SwAttrSetChg aOldSet(*pChangeHint->m_pOld);
                SwAttrSetChg aNewSet(*pChangeHint->m_pNew);
                do
                {
                    UpdateAttr_(pOItem, pNItem, eInvFlags, &aOldSet, &aNewSet);
                    pNItem = aNIter.NextItem();
                    pOItem = aOIter.NextItem();
                } while(pNItem);
                if(aOldSet.Count() || aNewSet.Count())
                    SwLayoutFrame::SwClientNotify(rMod, sw::AttrSetChangeHint(&aOldSet, &aNewSet));
            }
        }
        else // rHint.GetId() == SfxHintId::SwLegacyModify
        {
            auto pLegacy = static_cast<const sw::LegacyModifyHint*>(&rHint);
            UpdateAttr_(pLegacy->m_pOld, pLegacy->m_pNew, eInvFlags);
        }

        if(eInvFlags == SwFlyFrameInvFlags::NONE)
            return;

        Invalidate_();
        if(eInvFlags & SwFlyFrameInvFlags::InvalidatePos)
        {
            InvalidatePos_();
            // #i68520#
            InvalidateObjRectWithSpaces();
        }
        if(eInvFlags & SwFlyFrameInvFlags::InvalidateSize)
        {
            InvalidateSize_();
            // #i68520#
            InvalidateObjRectWithSpaces();
        }
        if(eInvFlags & SwFlyFrameInvFlags::InvalidatePrt)
            InvalidatePrt_();
        if(eInvFlags & SwFlyFrameInvFlags::SetNotifyBack)
            SetNotifyBack();
        if(eInvFlags & SwFlyFrameInvFlags::SetCompletePaint)
            SetCompletePaint();

        SwFrame* pLower = Lower();
        if((eInvFlags & SwFlyFrameInvFlags::ClearContourCache) && pLower && pLower->IsNoTextFrame())
            ClrContourCache( GetVirtDrawObj() );
        SwRootFrame *pRoot;
        if(eInvFlags & SwFlyFrameInvFlags::InvalidateBrowseWidth && nullptr != (pRoot = getRootFrame()))
            pRoot->InvalidateBrowseWidth();
        // #i28701#
        if(eInvFlags & SwFlyFrameInvFlags::UpdateObjInSortedList)
        {
            // update sorted object lists, the Writer fly frame is registered at.
            UpdateObjInSortedList();
        }

        // #i87645# - reset flags for the layout process (only if something has been invalidated)
        ResetLayoutProcessBools();
    }
    else if (rHint.GetId() == SfxHintId::SwAutoFormatUsedHint)
    {
        // There's a FlyFrame, so use it
        static_cast<const sw::AutoFormatUsedHint&>(rHint).SetUsed();
        return;
    }
    else if (rHint.GetId() == SfxHintId::SwGetZOrder)
    {
        auto pGetZOrdnerHint = static_cast<const sw::GetZOrderHint*>(&rHint);
        const auto& rFormat(dynamic_cast<const SwFrameFormat&>(rMod));
        if (rFormat.Which() == RES_FLYFRMFMT && rFormat.getIDocumentLayoutAccess().GetCurrentViewShell()) // #i11176#
            pGetZOrdnerHint->m_rnZOrder = GetVirtDrawObj()->GetOrdNum();
    }
    else if (rHint.GetId() == SfxHintId::SwGetObjectConnected)
    {
        auto pConnectedHint = static_cast<const sw::GetObjectConnectedHint*>(&rHint);
        const auto& rFormat(dynamic_cast<const SwFrameFormat&>(rMod));
        if (!pConnectedHint->m_risConnected && rFormat.Which() == RES_FLYFRMFMT && (!pConnectedHint->m_pRoot || pConnectedHint->m_pRoot == getRootFrame()))
            pConnectedHint->m_risConnected = true;
    }
}

void SwFlyFrame::UpdateAttr_( const SfxPoolItem *pOld, const SfxPoolItem *pNew,
                            SwFlyFrameInvFlags &rInvFlags,
                            SwAttrSetChg *pOldSet, SwAttrSetChg *pNewSet )
{
    bool bClear = true;
    const sal_uInt16 nWhich = pOld ? pOld->Which() : pNew ? pNew->Which() : 0;
    SwViewShell *pSh = getRootFrame()->GetCurrShell();
    switch( nWhich )
    {
        case RES_VERT_ORIENT:
        case RES_HORI_ORIENT:
        //  #i18732# - consider new option 'follow text flow'
        case RES_FOLLOW_TEXT_FLOW:
        {
            // ATTENTION: Always also change Action in ChgRePos()!
            rInvFlags |= SwFlyFrameInvFlags::InvalidatePos | SwFlyFrameInvFlags::SetNotifyBack;
        }
        break;
        // #i28701# - consider new option 'wrap influence on position'
        case RES_WRAP_INFLUENCE_ON_OBJPOS:
        {
            rInvFlags |= SwFlyFrameInvFlags::InvalidatePos | SwFlyFrameInvFlags::SetNotifyBack
                         | SwFlyFrameInvFlags::UpdateObjInSortedList;
        }
        break;
        case RES_SURROUND:
        {
            //#i28701# - invalidate position on change of
            // wrapping style.
            rInvFlags |= SwFlyFrameInvFlags::InvalidatePos | SwFlyFrameInvFlags::ClearContourCache;
            // The background needs to be messaged and invalidated
            const SwRect aTmp( GetObjRectWithSpaces() );
            NotifyBackground( FindPageFrame(), aTmp, PrepareHint::FlyFrameAttributesChanged );

            // By changing the flow of frame-bound Frames, a vertical alignment
            // can be activated/deactivated => MakeFlyPos
            if( RndStdIds::FLY_AT_FLY == GetFormat()->GetAnchor().GetAnchorId() )
                rInvFlags |= SwFlyFrameInvFlags::InvalidatePos | SwFlyFrameInvFlags::SetNotifyBack;

            // Delete contour in the Node if necessary
            SwFrame* pLower = Lower();
            if ( pLower && pLower->IsNoTextFrame() &&
                 !GetFormat()->GetSurround().IsContour() )
            {
                SwNoTextNode *pNd = static_cast<SwNoTextNode*>(static_cast<SwNoTextFrame*>(pLower)->GetNode());
                if ( pNd->HasContour() )
                    pNd->SetContour( nullptr );
            }
            // #i28701# - perform reorder of object lists
            // at anchor frame and at page frame.
            rInvFlags |= SwFlyFrameInvFlags::UpdateObjInSortedList;
        }
        break;

        case RES_PROTECT:
            if (pNew)
            {
                const SvxProtectItem *pP = static_cast<const SvxProtectItem*>(pNew);
                GetVirtDrawObj()->SetMoveProtect( pP->IsPosProtected()   );
                GetVirtDrawObj()->SetResizeProtect( pP->IsSizeProtected() );
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
                if( pSh )
                {
                    SwRootFrame* pLayout = getRootFrame();
                    if( pLayout && pLayout->IsAnyShellAccessible() )
                        pSh->Imp()->InvalidateAccessibleEditableState( true, this );
                }
#endif
            }
            break;
        case RES_COL:
            if (pOld && pNew)
            {
                ChgColumns( *static_cast<const SwFormatCol*>(pOld), *static_cast<const SwFormatCol*>(pNew) );
                const SwFormatFrameSize &rNew = GetFormat()->GetFrameSize();
                if ( FrameSizeChg( rNew ) )
                    NotifyDrawObj();
                rInvFlags |= SwFlyFrameInvFlags::InvalidateSize | SwFlyFrameInvFlags::SetNotifyBack
                             | SwFlyFrameInvFlags::SetCompletePaint;
            }
            break;

        case RES_FRM_SIZE:
        case RES_FLY_SPLIT:
        {
            const SwFormatFrameSize &rNew = GetFormat()->GetFrameSize();
            if ( FrameSizeChg( rNew ) )
                NotifyDrawObj();
            rInvFlags |= SwFlyFrameInvFlags::InvalidatePos | SwFlyFrameInvFlags::InvalidateSize
                         | SwFlyFrameInvFlags::InvalidatePrt | SwFlyFrameInvFlags::SetNotifyBack
                         | SwFlyFrameInvFlags::SetCompletePaint
                         | SwFlyFrameInvFlags::InvalidateBrowseWidth
                         | SwFlyFrameInvFlags::ClearContourCache;

            SwFormatURL aURL( GetFormat()->GetURL() );

            SwFormatFrameSize *pNewFormatFrameSize = nullptr;
            if (nWhich == RES_FRM_SIZE)
                pNewFormatFrameSize = const_cast<SwFormatFrameSize*>(static_cast<const SwFormatFrameSize*>(pNew));
            else if (nWhich == RES_FLY_SPLIT)
            {
                // If the fly frame has a table lower, invalidate that, so it joins its follow tab
                // frames and re-splits according to the new fly split rule.
                if (Lower() && Lower()->IsTabFrame())
                {
                    Lower()->InvalidateAll_();
                }
            }

            if (aURL.GetMap() && pNewFormatFrameSize)
            {
                const SwFormatFrameSize &rOld = *pNewFormatFrameSize;
                //#35091# Can be "times zero", when loading the template
                if ( rOld.GetWidth() && rOld.GetHeight() )
                {

                    Fraction aScaleX( rOld.GetWidth(), rNew.GetWidth() );
                    Fraction aScaleY( rOld.GetHeight(), rOld.GetHeight() );
                    aURL.GetMap()->Scale( aScaleX, aScaleY );
                    SwFrameFormat *pFormat = GetFormat();
                    pFormat->LockModify();
                    pFormat->SetFormatAttr( aURL );
                    pFormat->UnlockModify();
                }
            }
            const SvxProtectItem &rP = GetFormat()->GetProtect();
            GetVirtDrawObj()->SetMoveProtect( rP.IsPosProtected()    );
            GetVirtDrawObj()->SetResizeProtect( rP.IsSizeProtected() );

            if ( pSh )
                pSh->InvalidateWindows( getFrameArea() );
            const IDocumentDrawModelAccess& rIDDMA = GetFormat()->getIDocumentDrawModelAccess();
            const IDocumentSettingAccess& rIDSA = GetFormat()->getIDocumentSettingAccess();
            bool isPaintHellOverHF = rIDSA.get(DocumentSettingId::PAINT_HELL_OVER_HEADER_FOOTER);
            SdrLayerID nHellId = rIDDMA.GetHellId();

            if (isPaintHellOverHF && !GetAnchorFrame()->FindFooterOrHeader())
            {
                nHellId = rIDDMA.GetHeaderFooterHellId();
            }
            bool bNoClippingWithWrapPolygon = rIDSA.get(DocumentSettingId::NO_CLIPPING_WITH_WRAP_POLYGON);
            SdrLayerID nId = nHellId;
            if (GetFormat()->GetOpaque().GetValue() &&
                !(bNoClippingWithWrapPolygon && GetFrameFormat()->GetSurround().IsContour()))
                nId = rIDDMA.GetHeavenId();
            GetVirtDrawObj()->SetLayer( nId );

            if ( SwFrame* pLower = Lower() )
            {
                // Delete contour in the Node if necessary
                if( pLower->IsNoTextFrame() &&
                     !GetFormat()->GetSurround().IsContour() )
                {
                    SwNoTextNode *pNd = static_cast<SwNoTextNode*>(static_cast<SwNoTextFrame*>(pLower)->GetNode());
                    if ( pNd->HasContour() )
                        pNd->SetContour( nullptr );
                }
                else if( !pLower->IsColumnFrame() )
                {
                    SwFrame* pFrame = GetLastLower();
                    if( pFrame->IsTextFrame() && static_cast<SwTextFrame*>(pFrame)->IsUndersized() )
                        pFrame->Prepare( PrepareHint::AdjustSizeWithoutFormatting );
                }
            }

            // #i28701# - perform reorder of object lists
            // at anchor frame and at page frame.
            rInvFlags |= SwFlyFrameInvFlags::UpdateObjInSortedList;

            break;
        }
        case RES_UL_SPACE:
        case RES_LR_SPACE:
        {
            rInvFlags |= SwFlyFrameInvFlags::InvalidatePos | SwFlyFrameInvFlags::ClearContourCache;
            if( pSh && pSh->GetViewOptions()->getBrowseMode() )
                getRootFrame()->InvalidateBrowseWidth();
            SwRect aNew( GetObjRectWithSpaces() );
            SwRect aOld( getFrameArea() );
            if (pNew)
            {
                if ( RES_UL_SPACE == nWhich )
                {
                    const SvxULSpaceItem &rUL = *static_cast<const SvxULSpaceItem*>(pNew);
                    aOld.Top( std::max( aOld.Top() - tools::Long(rUL.GetUpper()), tools::Long(0) ) );
                    aOld.AddHeight(rUL.GetLower() );
                }
                else
                {
                    const SvxLRSpaceItem &rLR = *static_cast<const SvxLRSpaceItem*>(pNew);
                    aOld.Left(std::max(aOld.Left() - rLR.ResolveLeft({}), tools::Long(0)));
                    aOld.AddWidth(rLR.ResolveRight({}));
                }
            }
            aNew.Union( aOld );
            NotifyBackground( FindPageFrame(), aNew, PrepareHint::Clear );
        }
        break;

        case RES_TEXT_VERT_ADJUST:
        {
            InvalidateContentPos();
            rInvFlags |= SwFlyFrameInvFlags::SetCompletePaint;
        }
        break;

        case RES_BOX:
        case RES_SHADOW:
            rInvFlags |= SwFlyFrameInvFlags::InvalidatePos | SwFlyFrameInvFlags::InvalidateSize
                         | SwFlyFrameInvFlags::InvalidatePrt | SwFlyFrameInvFlags::SetCompletePaint;
            break;

        case RES_FRAMEDIR :
            SetDerivedVert( false );
            SetDerivedR2L( false );
            CheckDirChange();
            break;

        case RES_OPAQUE:
            if (pNew)
            {
                if ( pSh )
                    pSh->InvalidateWindows( getFrameArea() );

                const IDocumentDrawModelAccess& rIDDMA = GetFormat()->getIDocumentDrawModelAccess();
                const SdrLayerID nId = static_cast<const SvxOpaqueItem*>(pNew)->GetValue() ?
                                    rIDDMA.GetHeavenId() :
                                    rIDDMA.GetHellId();
                GetVirtDrawObj()->SetLayer( nId );
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
                if( pSh )
                {
                    SwRootFrame* pLayout = getRootFrame();
                    if( pLayout && pLayout->IsAnyShellAccessible() )
                    {
                        pSh->Imp()->DisposeAccessibleFrame( this );
                        pSh->Imp()->AddAccessibleFrame( this );
                    }
                }
#endif
                // #i28701# - perform reorder of object lists
                // at anchor frame and at page frame.
                rInvFlags |= SwFlyFrameInvFlags::UpdateObjInSortedList;
            }
            break;

        case RES_URL:
        {
            // The interface changes the frame size when interacting with text frames,
            // the Map, however, needs to be relative to FrameSize().
            SwFrame* pLower = Lower();
            if ( (!pLower || !pLower->IsNoTextFrame()) && pNew && pOld &&
                 static_cast<const SwFormatURL*>(pNew)->GetMap() && static_cast<const SwFormatURL*>(pOld)->GetMap() )
            {
                const SwFormatFrameSize &rSz = GetFormat()->GetFrameSize();
                if ( rSz.GetHeight() != getFrameArea().Height() ||
                     rSz.GetWidth()  != getFrameArea().Width() )
                {
                    SwFormatURL aURL( GetFormat()->GetURL() );
                    Fraction aScaleX( getFrameArea().Width(),  rSz.GetWidth() );
                    Fraction aScaleY( getFrameArea().Height(), rSz.GetHeight() );
                    aURL.GetMap()->Scale( aScaleX, aScaleY );
                    SwFrameFormat *pFormat = GetFormat();
                    pFormat->LockModify();
                    pFormat->SetFormatAttr( aURL );
                    pFormat->UnlockModify();
                }
            }
        }
        // No invalidation necessary
        break;

        case RES_CHAIN:
            if (pNew)
            {
                const SwFormatChain *pChain = static_cast<const SwFormatChain*>(pNew);
                if ( pChain->GetNext() )
                {
                    SwFlyFrame *pFollow = FindChainNeighbour( *pChain->GetNext() );
                    if ( GetNextLink() && pFollow != GetNextLink() )
                        SwFlyFrame::UnchainFrames( *this, *GetNextLink());
                    if ( pFollow )
                    {
                        if ( pFollow->GetPrevLink() &&
                             pFollow->GetPrevLink() != this )
                            SwFlyFrame::UnchainFrames( *pFollow->GetPrevLink(),
                                                     *pFollow );
                        if ( !GetNextLink() )
                            SwFlyFrame::ChainFrames( *this, *pFollow );
                    }
                }
                else if ( GetNextLink() )
                    SwFlyFrame::UnchainFrames( *this, *GetNextLink() );
                if ( pChain->GetPrev() )
                {
                    SwFlyFrame *pMaster = FindChainNeighbour( *pChain->GetPrev() );
                    if ( GetPrevLink() && pMaster != GetPrevLink() )
                        SwFlyFrame::UnchainFrames( *GetPrevLink(), *this );
                    if ( pMaster )
                    {
                        if ( pMaster->GetNextLink() &&
                             pMaster->GetNextLink() != this )
                            SwFlyFrame::UnchainFrames( *pMaster, *pMaster->GetNextLink() );
                        if ( !GetPrevLink() )
                            SwFlyFrame::ChainFrames( *pMaster, *this );
                    }
                }
                else if ( GetPrevLink() )
                    SwFlyFrame::UnchainFrames( *GetPrevLink(), *this );
            }
            [[fallthrough]];
        default:
            bClear = false;
    }
    if ( !bClear )
        return;

    if ( pOldSet || pNewSet )
    {
        if ( pOldSet )
            pOldSet->ClearItem( nWhich );
        if ( pNewSet )
            pNewSet->ClearItem( nWhich );
    }
    else
    {
        SwModify aMod;
        SwLayoutFrame::SwClientNotify(aMod, sw::LegacyModifyHint(pOld, pNew));
    }
}

void SwFlyFrame::UpdateAttrForFormatChange( SwFormat *pOldFormat, SwFormat *pNewFormat,
                            SwFlyFrameInvFlags &rInvFlags )
{
    SwViewShell *pSh = getRootFrame()->GetCurrShell();
    {
        const SwFormatFrameSize &rNew = GetFormat()->GetFrameSize();
        if ( FrameSizeChg( rNew ) )
            NotifyDrawObj();
        rInvFlags |= SwFlyFrameInvFlags::InvalidatePos | SwFlyFrameInvFlags::InvalidateSize
                     | SwFlyFrameInvFlags::InvalidatePrt | SwFlyFrameInvFlags::SetNotifyBack
                     | SwFlyFrameInvFlags::SetCompletePaint
                     | SwFlyFrameInvFlags::InvalidateBrowseWidth
                     | SwFlyFrameInvFlags::ClearContourCache;
        {
            SwRect aNew( GetObjRectWithSpaces() );
            SwRect aOld( getFrameArea() );
            const SvxULSpaceItem &rUL = pOldFormat->GetULSpace();
            aOld.Top( std::max( aOld.Top() - tools::Long(rUL.GetUpper()), tools::Long(0) ) );
            aOld.AddHeight(rUL.GetLower() );
            const SvxLRSpaceItem &rLR = pOldFormat->GetLRSpace();
            aOld.Left(std::max(aOld.Left() - rLR.ResolveLeft({}), tools::Long(0)));
            aOld.AddWidth(rLR.ResolveRight({}));
            aNew.Union( aOld );
            NotifyBackground( FindPageFrame(), aNew, PrepareHint::Clear );

            // Special case:
            // When assigning a template we cannot rely on the old column
            // attribute. As there need to be at least enough for ChgColumns,
            // we need to create a temporary attribute.
            SwFormatCol aCol;
            if ( Lower() && Lower()->IsColumnFrame() )
            {
                sal_uInt16 nCol = 0;
                SwFrame *pTmp = Lower();
                do
                {   ++nCol;
                    pTmp = pTmp->GetNext();
                } while ( pTmp );
                aCol.Init( nCol, 0, 1000 );
            }
            ChgColumns( aCol, GetFormat()->GetCol() );
        }

        SwFormatURL aURL( GetFormat()->GetURL() );

        if (aURL.GetMap() && pOldFormat)
        {
            const SwFormatFrameSize &rOld = pOldFormat->GetFrameSize();
            //#35091# Can be "times zero", when loading the template
            if ( rOld.GetWidth() && rOld.GetHeight() )
            {

                Fraction aScaleX( rOld.GetWidth(), rNew.GetWidth() );
                Fraction aScaleY( rOld.GetHeight(), rOld.GetHeight() );
                aURL.GetMap()->Scale( aScaleX, aScaleY );
                SwFrameFormat *pFormat = GetFormat();
                pFormat->LockModify();
                pFormat->SetFormatAttr( aURL );
                pFormat->UnlockModify();
            }
        }
        const SvxProtectItem &rP = GetFormat()->GetProtect();
        GetVirtDrawObj()->SetMoveProtect( rP.IsPosProtected()    );
        GetVirtDrawObj()->SetResizeProtect( rP.IsSizeProtected() );

        if ( pSh )
            pSh->InvalidateWindows( getFrameArea() );
        const IDocumentDrawModelAccess& rIDDMA = GetFormat()->getIDocumentDrawModelAccess();
        const IDocumentSettingAccess& rIDSA = GetFormat()->getIDocumentSettingAccess();
        bool isPaintHellOverHF = rIDSA.get(DocumentSettingId::PAINT_HELL_OVER_HEADER_FOOTER);
        SdrLayerID nHellId = rIDDMA.GetHellId();

        if (isPaintHellOverHF && !GetAnchorFrame()->FindFooterOrHeader())
        {
            nHellId = rIDDMA.GetHeaderFooterHellId();
        }
        bool bNoClippingWithWrapPolygon = rIDSA.get(DocumentSettingId::NO_CLIPPING_WITH_WRAP_POLYGON);
        SdrLayerID nId = nHellId;
        if (GetFormat()->GetOpaque().GetValue() &&
            !(bNoClippingWithWrapPolygon && GetFrameFormat()->GetSurround().IsContour()))
            nId = rIDDMA.GetHeavenId();
        GetVirtDrawObj()->SetLayer( nId );

        SwFrame* pFrame = Lower();
        if ( pFrame )
        {
            // Delete contour in the Node if necessary
            if( pFrame->IsNoTextFrame() &&
                 !GetFormat()->GetSurround().IsContour() )
            {
                SwNoTextNode *pNd = static_cast<SwNoTextNode*>(static_cast<SwNoTextFrame*>(pFrame)->GetNode());
                if ( pNd->HasContour() )
                    pNd->SetContour( nullptr );
            }
            else if( !pFrame->IsColumnFrame() )
            {
                pFrame = GetLastLower();
                if( pFrame->IsTextFrame() && static_cast<SwTextFrame*>(pFrame)->IsUndersized() )
                    pFrame->Prepare( PrepareHint::AdjustSizeWithoutFormatting );
            }
        }

        // #i28701# - perform reorder of object lists
        // at anchor frame and at page frame.
        rInvFlags |= SwFlyFrameInvFlags::UpdateObjInSortedList;
    }

    SwModify aMod;
    SwLayoutFrame::SwClientNotify(aMod, SwFormatChangeHint(pOldFormat, pNewFormat));
}

void SwFlyFrame::Invalidate_( SwPageFrame const *pPage )
{
    InvalidatePage( pPage );
    m_bNotifyBack = m_bInvalid = true;

    SwFlyFrame *pFrame;
    if ( GetAnchorFrame() && nullptr != (pFrame = AnchorFrame()->FindFlyFrame()) )
    {
        // Very bad case: If the Fly is bound within another Fly which
        // contains columns, the Format should be from that one.
        SwFrame* pLower = pFrame->Lower();
        if ( !pFrame->IsLocked() && !pFrame->IsColLocked() &&
             pLower && pLower->IsColumnFrame() )
            pFrame->InvalidateSize();
    }

    // #i85216#
    // if vertical position is oriented at a layout frame inside a ghost section,
    // assure that the position is invalidated and that the information about
    // the vertical position oriented frame is cleared
    if ( GetVertPosOrientFrame() && GetVertPosOrientFrame()->IsLayoutFrame() )
    {
        const SwSectionFrame* pSectFrame( GetVertPosOrientFrame()->FindSctFrame() );
        if ( pSectFrame && pSectFrame->GetSection() == nullptr )
        {
            InvalidatePos();
            ClearVertPosOrientFrame();
        }
    }
}

/** Change the relative position
 *
 * The position will be Fix automatically and the attribute is changed accordingly.
 */
void SwFlyFrame::ChgRelPos( const Point &rNewPos )
{
    if ( GetCurrRelPos() == rNewPos )
        return;

    SwFrameFormat *pFormat = GetFormat();
    const bool bVert = GetAnchorFrame()->IsVertical();
    const SwTwips nNewY = bVert ? rNewPos.X() : rNewPos.Y();
    SwTwips nTmpY = nNewY == LONG_MAX ? 0 : nNewY;
    if( bVert )
        nTmpY = -nTmpY;
    SfxItemSetFixed<RES_VERT_ORIENT, RES_HORI_ORIENT> aSet( pFormat->GetDoc().GetAttrPool() );

    SwFormatVertOrient aVert( pFormat->GetVertOrient() );
    const SwTextFrame *pAutoFrame = nullptr;
    // #i34948# - handle also at-page and at-fly anchored
    // Writer fly frames
    const RndStdIds eAnchorType = GetFrameFormat()->GetAnchor().GetAnchorId();
    if ( eAnchorType == RndStdIds::FLY_AT_PAGE )
    {
        aVert.SetVertOrient( text::VertOrientation::NONE );
        aVert.SetRelationOrient( text::RelOrientation::PAGE_FRAME );
    }
    else if ( eAnchorType == RndStdIds::FLY_AT_FLY )
    {
        aVert.SetVertOrient( text::VertOrientation::NONE );
        aVert.SetRelationOrient( text::RelOrientation::FRAME );
    }
    else if ( IsFlyAtContentFrame() || text::VertOrientation::NONE != aVert.GetVertOrient() )
    {
        if( text::RelOrientation::CHAR == aVert.GetRelationOrient() && IsAutoPos() )
        {
            if( LONG_MAX != nNewY )
            {
                aVert.SetVertOrient( text::VertOrientation::NONE );
                assert(GetAnchorFrame()->IsTextFrame());
                pAutoFrame = static_cast<const SwTextFrame*>(GetAnchorFrame());
                TextFrameIndex const nOfs(pAutoFrame->MapModelToViewPos(
                            *pFormat->GetAnchor().GetContentAnchor()));
                while( pAutoFrame->GetFollow() &&
                       pAutoFrame->GetFollow()->GetOffset() <= nOfs )
                {
                    if( pAutoFrame == GetAnchorFrame() )
                        nTmpY += pAutoFrame->GetRelPos().Y();
                    nTmpY -= pAutoFrame->GetUpper()->getFramePrintArea().Height();
                    pAutoFrame = pAutoFrame->GetFollow();
                }
                nTmpY = static_cast<SwFlyAtContentFrame*>(this)->GetRelCharY(pAutoFrame)-nTmpY;
            }
            else
                aVert.SetVertOrient( text::VertOrientation::CHAR_BOTTOM );
        }
        else
        {
            aVert.SetVertOrient( text::VertOrientation::NONE );
            aVert.SetRelationOrient( text::RelOrientation::FRAME );
        }
    }
    aVert.SetPos( nTmpY );
    aSet.Put( aVert );

    // For Flys in the Cnt, the horizontal orientation is of no interest,
    // as it's always 0
    if ( !IsFlyInContentFrame() )
    {
        const SwTwips nNewX = bVert ? rNewPos.Y() : rNewPos.X();
        SwTwips nTmpX = nNewX == LONG_MAX ? 0 : nNewX;
        SwFormatHoriOrient aHori( pFormat->GetHoriOrient() );
        // #i34948# - handle also at-page and at-fly anchored
        // Writer fly frames
        if ( eAnchorType == RndStdIds::FLY_AT_PAGE )
        {
            aHori.SetHoriOrient( text::HoriOrientation::NONE );
            aHori.SetRelationOrient( text::RelOrientation::PAGE_FRAME );
            aHori.SetPosToggle( false );
        }
        else if ( eAnchorType == RndStdIds::FLY_AT_FLY )
        {
            aHori.SetHoriOrient( text::HoriOrientation::NONE );
            aHori.SetRelationOrient( text::RelOrientation::FRAME );
            aHori.SetPosToggle( false );
        }
        else if ( IsFlyAtContentFrame() || text::HoriOrientation::NONE != aHori.GetHoriOrient() )
        {
            aHori.SetHoriOrient( text::HoriOrientation::NONE );
            if( text::RelOrientation::CHAR == aHori.GetRelationOrient() && IsAutoPos() )
            {
                if( LONG_MAX != nNewX )
                {
                    if( !pAutoFrame )
                    {
                        assert(GetAnchorFrame()->IsTextFrame());
                        pAutoFrame = static_cast<const SwTextFrame*>(GetAnchorFrame());
                        TextFrameIndex const nOfs(pAutoFrame->MapModelToViewPos(
                                    *pFormat->GetAnchor().GetContentAnchor()));
                        while( pAutoFrame->GetFollow() &&
                               pAutoFrame->GetFollow()->GetOffset() <= nOfs )
                            pAutoFrame = pAutoFrame->GetFollow();
                    }
                    nTmpX -= static_cast<SwFlyAtContentFrame*>(this)->GetRelCharX(pAutoFrame);
                }
            }
            else
                aHori.SetRelationOrient( text::RelOrientation::FRAME );
            aHori.SetPosToggle( false );
        }
        aHori.SetPos( nTmpX );
        aSet.Put( aHori );
    }
    SetCurrRelPos( rNewPos );
    pFormat->GetDoc().SetAttr( aSet, *pFormat );

}

/** "Formats" the Frame; Frame and PrtArea.
 *
 * The FixSize is not inserted here.
 */
void SwFlyFrame::Format( vcl::RenderContext* /*pRenderContext*/, const SwBorderAttrs *pAttrs )
{
    OSL_ENSURE( pAttrs, "FlyFrame::Format, pAttrs is 0." );

    ColLock();

    if ( !isFrameAreaSizeValid() )
    {
        if ( getFrameArea().Top() == FAR_AWAY && getFrameArea().Left() == FAR_AWAY )
        {
            // Remove safety switch (see SwFrame::CTor)
            {
                SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
                aFrm.Pos().setX(0);
                aFrm.Pos().setY(0);
            }

            // #i68520#
            InvalidateObjRectWithSpaces();
        }

        // Check column width and set it if needed
        SwFrame* pLower = Lower();
        if ( pLower && pLower->IsColumnFrame() )
            AdjustColumns( nullptr, false );

        setFrameAreaSizeValid(true);

        const SwTwips nUL = pAttrs->CalcTopLine()  + pAttrs->CalcBottomLine();
        const SwTwips nLR = pAttrs->CalcLeftLine() + pAttrs->CalcRightLine();
        const SwFormatFrameSize &rFrameSz = GetFormat()->GetFrameSize();
        Size aRelSize( CalcRel( rFrameSz ) );

        OSL_ENSURE( pAttrs->GetSize().Height() != 0 || rFrameSz.GetHeightPercent(), "FrameAttr height is 0." );
        OSL_ENSURE( pAttrs->GetSize().Width()  != 0 || rFrameSz.GetWidthPercent(), "FrameAttr width is 0." );

        SwRectFnSet aRectFnSet(this);
        if( !HasFixSize() )
        {
            tools::Long nMinHeight = 0;
            if( IsMinHeight() )
                nMinHeight = aRectFnSet.IsVert() ? aRelSize.Width() : aRelSize.Height();

            SwTwips nRemaining = CalcContentHeight(pAttrs, nMinHeight, nUL);
            if( IsMinHeight() && (nRemaining + nUL) < nMinHeight )
                nRemaining = nMinHeight - nUL;
            // Because the Grow/Shrink of the Flys does not directly
            // set the size - only indirectly by triggering a Format()
            // via Invalidate() - the sizes need to be set here.
            // Notification is running along already.
            // As we already got a lot of zeros per attribute, we block them
            // from now on.

            if ( nRemaining < MINFLY )
                nRemaining = MINFLY;

            const SwFrame* pAnchor = GetAnchorFrame();
            if (SwFrame* pAnchorChar = FindAnchorCharFrame())
            {
                // If we find a follow of the anchor that is effectively the anchor of this fly,
                // then use that as the anchor for sizing purposes.
                pAnchor = pAnchorChar;
            }
            if (pAnchor && IsFlySplitAllowed())
            {
                // If the fly is allowed to be split, then limit its size to the upper of the
                // anchor.
                SwTwips nDeadline = GetFlyAnchorBottom(this, *pAnchor);
                SwTwips nTop = aRectFnSet.GetTop(getFrameArea());
                SwTwips nBottom = aRectFnSet.GetTop(getFrameArea()) + nRemaining;
                if (nBottom > nDeadline)
                {
                    if (nDeadline > nTop)
                    {
                        nRemaining = nDeadline - nTop;
                    }
                    else
                    {
                        // Even the top is below the deadline, set size to empty and mark it as
                        // clipped so we re-format later.
                        nRemaining = 0;
                        m_bHeightClipped = true;
                    }
                }
            }

            {
                SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
                aRectFnSet.SetHeight( aPrt, nRemaining );
            }

            nRemaining -= aRectFnSet.GetHeight(getFrameArea());

            {
                SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
                aRectFnSet.AddBottom( aFrm, nRemaining + nUL );
            }

            // #i68520#
            if ( nRemaining + nUL != 0 )
            {
                InvalidateObjRectWithSpaces();
            }

            setFrameAreaSizeValid(true);

            if (SwFrameFormat* pShapeFormat = SwTextBoxHelper::getOtherTextBoxFormat(GetFormat(), RES_FLYFRMFMT))
            {
                // This fly is a textbox of a draw shape.
                SdrObject* pShape = pShapeFormat->FindSdrObject();
                if (SdrObjCustomShape* pCustomShape = dynamic_cast<SdrObjCustomShape*>( pShape) )
                {
                    // The shape is a customshape: then inform it about the calculated fly size.
                    Size aSize(getFrameArea().Width(), getFrameArea().Height());
                    pCustomShape->SuggestTextFrameSize(aSize);
                    // Do the calculations normally done after touching editeng text of the shape.
                    pCustomShape->NbcSetOutlinerParaObjectForText(std::nullopt, nullptr);
                }
            }
        }
        else
        {
            // Fixed Frames do not Format itself
            setFrameAreaSizeValid(true);

            // Flys set their size using the attr
            SwTwips nNewSize = aRectFnSet.IsVert() ? aRelSize.Width() : aRelSize.Height();
            nNewSize -= nUL;
            if( nNewSize < MINFLY )
                nNewSize = MINFLY;

            {
                SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
                aRectFnSet.SetHeight( aPrt, nNewSize );
            }

            nNewSize += nUL - aRectFnSet.GetHeight(getFrameArea());

            {
                SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
                aRectFnSet.AddBottom( aFrm, nNewSize );
            }

            // #i68520#
            if ( nNewSize != 0 )
            {
                InvalidateObjRectWithSpaces();
            }
        }

        if ( !m_bFormatHeightOnly )
        {
            OSL_ENSURE( aRelSize == CalcRel( rFrameSz ), "SwFlyFrame::Format CalcRel problem" );
            SwTwips nNewSize = aRectFnSet.IsVert() ? aRelSize.Height() : aRelSize.Width();

            if ( rFrameSz.GetWidthSizeType() != SwFrameSize::Fixed )
            {
                // #i9046# Autowidth for fly frames
                const SwTwips nAutoWidth = lcl_CalcAutoWidth( *this );
                if ( nAutoWidth )
                {
                    if( SwFrameSize::Minimum == rFrameSz.GetWidthSizeType() )
                        nNewSize = std::max( nNewSize - nLR, nAutoWidth );
                    else
                        nNewSize = nAutoWidth;
                }
            }
            else
                nNewSize -= nLR;

            if( nNewSize < MINFLY )
                nNewSize = MINFLY;

            {
                SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
                aRectFnSet.SetWidth( aPrt, nNewSize );
            }

            nNewSize += nLR - aRectFnSet.GetWidth(getFrameArea());

            {
                SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
                aRectFnSet.AddRight( aFrm, nNewSize );
            }

            // #i68520#
            if ( nNewSize != 0 )
            {
                InvalidateObjRectWithSpaces();
            }
        }
    }
    ColUnlock();
}

// #i11760# - change parameter <bNoColl>: type <bool>;
//   add new parameter <bNoCalcFollow> with
//  new parameter <bNoCalcFollow> was used by method
//                          <FormatWidthCols(..)> to avoid follow formatting
//                          for text frames. But, unformatted follows causes
//                          problems in method <SwContentFrame::WouldFit_(..)>,
//                          which assumes that the follows are formatted.
//                          Thus, <bNoCalcFollow> no longer used by <FormatWidthCols(..)>.
void CalcContent( SwLayoutFrame *pLay, bool bNoColl )
{
    SwViewShell & rShell(*pLay->getRootFrame()->GetCurrShell());
    vcl::RenderContext* pRenderContext = rShell.GetOut();
    SwSectionFrame* pSect;
    bool bCollect = false;
    if( pLay->IsSctFrame() )
    {
        pSect = static_cast<SwSectionFrame*>(pLay);
        if( pSect->IsEndnAtEnd() && !bNoColl )
        {
            bCollect = true;
            SwLayouter::CollectEndnotes( pLay->GetFormat()->GetDoc(), pSect );
        }
        pSect->CalcFootnoteContent();
    }
    else
        pSect = nullptr;
    SwFrame *pFrame = pLay->ContainsAny();
    if ( !pFrame )
    {
        if( pSect )
        {
            if( pSect->HasFollow() )
                pFrame = pSect->GetFollow()->ContainsAny();
            if( !pFrame )
            {
                if( pSect->IsEndnAtEnd() )
                {
                    if( bCollect )
                        pLay->GetFormat()->GetDoc().getIDocumentLayoutAccess().GetLayouter()->
                            InsertEndnotes( pSect );
                    bool bLock = pSect->IsFootnoteLock();
                    pSect->SetFootnoteLock( true );
                    pSect->CalcFootnoteContent();
                    pSect->CalcFootnoteContent();
                    pSect->SetFootnoteLock( bLock );
                }
                return;
            }
            pFrame->InvalidatePos_();
        }
        else
            return;
    }
    pFrame->InvalidatePage();

    do
    {
        // local variables to avoid loops caused by anchored object positioning
        SwAnchoredObject* pAgainObj1 = nullptr;
        SwAnchoredObject* pAgainObj2 = nullptr;

        // FME 2007-08-30 #i81146# new loop control
        int nLoopControlRuns = 0;
        // tdf#152106 loop control for multi-column sections
        int nLoopControlRunsInMultiCol = 0;
        const int nLoopControlMax = 20;
        const SwFrame* pLoopControlCond = nullptr;

        SwFrame* pLast;
        do
        {
            pLast = pFrame;
            bool const wasFrameLowerOfLay(pLay->IsAnLower(pFrame));
            if( pFrame->IsVertical() ?
                ( pFrame->GetUpper()->getFramePrintArea().Height() != pFrame->getFrameArea().Height() )
                : ( pFrame->GetUpper()->getFramePrintArea().Width() != pFrame->getFrameArea().Width() ) )
            {
                pFrame->Prepare( PrepareHint::FixSizeChanged );
                pFrame->InvalidateSize_();
            }

            if ( pFrame->IsTabFrame() )
            {
                static_cast<SwTabFrame*>(pFrame)->m_bCalcLowers = true;
                // #i18103# - lock move backward of follow table,
                // if no section content is formatted or follow table belongs
                // to the section, which content is formatted.
                if ( static_cast<SwTabFrame*>(pFrame)->IsFollow() &&
                     ( !pSect || pSect == pFrame->FindSctFrame() ) )
                {
                    static_cast<SwTabFrame*>(pFrame)->m_bLockBackMove = true;
                }
            }

            {
                SwFrameDeleteGuard aDeletePageGuard(pSect ? pSect->FindPageFrame() : nullptr);
                SwFrameDeleteGuard aDeleteGuard(pSect);
                pFrame->Calc(pRenderContext);
            }

            // #i11760# - reset control flag for follow format.
            if ( pFrame->IsTextFrame() )
            {
                static_cast<SwTextFrame*>(pFrame)->AllowFollowFormat();
            }

            // The keep-attribute can cause the position
            // of the prev to be invalid:
            // Do not consider invalid previous frame
            // due to its keep-attribute, if current frame is a follow or is locked.
            // #i44049# - do not consider invalid previous
            // frame due to its keep-attribute, if it can't move forward.
            // #i57765# - do not consider invalid previous
            // frame, if current frame has a column/page break before attribute.
            assert(pFrame->IsFlowFrame());
            SwFlowFrame* pTmpFlowFrame = SwFlowFrame::CastFlowFrame(pFrame);
            SwFrame* pTmpPrev = pTmpFlowFrame->FindPrevIgnoreHidden();
            SwFlowFrame* pTmpPrevFlowFrame = pTmpPrev && pTmpPrev->IsFlowFrame() ? SwFlowFrame::CastFlowFrame(pTmpPrev) : nullptr;

            bool bPrevInvalid = pTmpPrevFlowFrame && pTmpFlowFrame &&
                               !pTmpFlowFrame->IsFollow() &&
                               !StackHack::IsLocked() && // #i76382#
                               !pTmpFlowFrame->IsJoinLocked() &&
                               !pTmpPrev->isFrameAreaPositionValid() &&
                                pLay->IsAnLower( pTmpPrev ) &&
                                pTmpPrevFlowFrame->IsKeep(pTmpPrev->GetAttrSet()->GetKeep(), pTmpPrev->GetBreakItem()) &&
                                pTmpPrevFlowFrame->IsKeepFwdMoveAllowed();

            // format floating screen objects anchored to the frame.
            if ( !bPrevInvalid && pFrame->GetDrawObjs() && pLay->IsAnLower( pFrame ) )
            {
                bool bAgain = false;
                bool bRestartLayoutProcess = false;
                size_t nCnt = pFrame->GetDrawObjs()->size();
                size_t i = 0;
                while ( i < nCnt )
                {
                    // pFrame can move to a different page in FormatObj()
                    SwPageFrame *const pPageFrame = pFrame->FindPageFrame();

                    // #i28701#
                    SwAnchoredObject* pAnchoredObj = (*pFrame->GetDrawObjs())[i];
                    assert(pAnchoredObj);

                    // determine if anchored object has to be
                    // formatted and, in case, format it
                    if ( !pAnchoredObj->PositionLocked() && pAnchoredObj->IsFormatPossible() )
                    {
                        // #i43737# - no invalidation of
                        // anchored object needed - causes loops for as-character
                        // anchored objects.
                        //pAnchoredObj->InvalidateObjPos();
                        SwRect aRect( pAnchoredObj->GetObjRect() );

                        SwFrame* pAnchorFrame = pFrame;
                        SwPageFrame* pAnchorPageFrame = pPageFrame;
                        if (SwFlyFrame* pFlyFrame = pAnchoredObj->DynCastFlyFrame())
                        {
                            if (pFlyFrame->IsFlySplitAllowed())
                            {
                                // Split flys are at-para anchored, but the follow fly's anchor char
                                // frame is not the master frame but can be also a follow of pFrame.
                                SwTextFrame* pAnchorCharFrame = pFlyFrame->FindAnchorCharFrame();
                                if (pAnchorCharFrame)
                                {
                                    // Found an anchor char frame, update the anchor frame and the
                                    // anchor page frame accordingly.
                                    pAnchorFrame = pAnchorCharFrame;
                                    pAnchorPageFrame = pAnchorCharFrame->FindPageFrame();
                                }
                            }
                        }

                        if (!SwObjectFormatter::FormatObj(*pAnchoredObj, pAnchorFrame, pAnchorPageFrame,
                                rShell.Imp()->IsAction() ? &rShell.Imp()->GetLayAction() : nullptr))
                        {
                            if (rShell.Imp()->IsAction() && rShell.Imp()->GetLayAction().IsAgain())
                            {   // tdf#159015 will always fail, don't loop
                                return;
                            }
                            bRestartLayoutProcess = true;
                            break;
                        }
                        // #i3317# - restart layout process,
                        // if the position of the anchored object is locked now.
                        if ( pAnchoredObj->PositionLocked() )
                        {
                            bRestartLayoutProcess = true;
                            break;
                        }

                        if ( aRect != pAnchoredObj->GetObjRect() )
                        {
                            bAgain = true;
                            if ( pAgainObj2 == pAnchoredObj )
                            {
                                OSL_FAIL( "::CalcContent(..) - loop detected, perform attribute changes to avoid the loop" );
                                // Prevent oscillation
                                SwFrameFormat* pFormat = pAnchoredObj->GetFrameFormat();
                                SwFormatSurround aAttr( pFormat->GetSurround() );
                                if( css::text::WrapTextMode_THROUGH != aAttr.GetSurround() )
                                {
                                    // When on auto position, we can only set it to
                                    // flow through
                                    if ((pFormat->GetAnchor().GetAnchorId() ==
                                            RndStdIds::FLY_AT_CHAR) &&
                                        (css::text::WrapTextMode_PARALLEL ==
                                            aAttr.GetSurround()))
                                    {
                                        aAttr.SetSurround( css::text::WrapTextMode_THROUGH );
                                    }
                                    else
                                    {
                                        aAttr.SetSurround( css::text::WrapTextMode_PARALLEL );
                                    }
                                    pFormat->LockModify();
                                    pFormat->SetFormatAttr( aAttr );
                                    pFormat->UnlockModify();
                                }
                            }
                            else
                            {
                                if ( pAgainObj1 == pAnchoredObj )
                                    pAgainObj2 = pAnchoredObj;
                                pAgainObj1 = pAnchoredObj;
                            }
                        }

                        if ( !pFrame->GetDrawObjs() )
                            break;
                        if ( pFrame->GetDrawObjs()->size() < nCnt )
                        {
                            --nCnt;
                            // Do not increment index, in this case
                            continue;
                        }
                    }
                    ++i;
                }

                // #i28701# - restart layout process, if
                // requested by floating screen object formatting
                if (bRestartLayoutProcess
                    // tdf#152106 loop control in multi-column sections to avoid of freezing
                    && nLoopControlRunsInMultiCol < nLoopControlMax
                    // tdf#142080 if it was already on next page, and still is,
                    // ignore restart, as restart could cause infinite loop
                    && (wasFrameLowerOfLay || pLay->IsAnLower(pFrame)))
                {
                    bool bIsMultiColumn = pSect && pSect->GetSection() && pSect->Lower() &&
                            pSect->Lower()->IsColumnFrame() && pSect->Lower()->GetNext();
                    if ( bIsMultiColumn )
                        ++nLoopControlRunsInMultiCol;
                    pFrame = pLay->ContainsAny();
                    pAgainObj1 = nullptr;
                    pAgainObj2 = nullptr;
                    continue;
                }

                // #i28701# - format anchor frame after its objects
                // are formatted, if the wrapping style influence has to be considered.
                if ( pLay->GetFormat()->getIDocumentSettingAccess().get(DocumentSettingId::CONSIDER_WRAP_ON_OBJECT_POSITION) )
                {
                    pFrame->Calc(pRenderContext);
                }

                if ( bAgain )
                {
                    pFrame = pLay->ContainsContent();
                    if ( pFrame && pFrame->IsInTab() )
                        pFrame = pFrame->FindTabFrame();
                    if( pFrame && pFrame->IsInSct() )
                    {
                        SwSectionFrame* pTmp = pFrame->FindSctFrame();
                        if( pTmp != pLay && pLay->IsAnLower( pTmp ) )
                            pFrame = pTmp;
                    }

                    if ( pFrame == pLoopControlCond )
                        ++nLoopControlRuns;
                    else
                    {
                        nLoopControlRuns = 0;
                        pLoopControlCond = pFrame;
                    }

                    if ( nLoopControlRuns < nLoopControlMax )
                        continue;

                    OSL_FAIL( "LoopControl in CalcContent" );
                }
            }
            if ( pFrame->IsTabFrame() )
            {
                if (static_cast<SwTabFrame*>(pFrame)->m_bLockBackMove)
                {
                    assert(static_cast<SwTabFrame*>(pFrame)->IsFollow());
                    static_cast<SwTabFrame*>(pFrame)->m_bLockBackMove = false;
                    // tdf#150606 encourage it to move back in FormatLayout()
                    if (static_cast<SwTabFrame*>(pFrame)->m_bWantBackMove)
                    {
                        static_cast<SwTabFrame*>(pFrame)->m_bWantBackMove = false;
                        pFrame->InvalidatePos();
                    }
                }
            }

            pFrame = bPrevInvalid ? pTmpPrev : pFrame->FindNext();
            if( !bPrevInvalid && pFrame && pFrame->IsSctFrame() && pSect )
            {
                // Empty SectionFrames could be present here
                while( pFrame && pFrame->IsSctFrame() && !static_cast<SwSectionFrame*>(pFrame)->GetSection() )
                    pFrame = pFrame->FindNext();

                // If FindNext returns the Follow of the original Area, we want to
                // continue with this content as long as it flows back.
                if( pFrame && pFrame->IsSctFrame() && ( pFrame == pSect->GetFollow() ||
                    static_cast<SwSectionFrame*>(pFrame)->IsAnFollow( pSect ) ) )
                {
                    pFrame = static_cast<SwSectionFrame*>(pFrame)->ContainsAny();
                    if( pFrame )
                        pFrame->InvalidatePos_();
                }
            }
          // Stay in the pLay.
          // Except for SectionFrames with Follow: the first ContentFrame of the
          // Follow will be formatted, so that it gets a chance to move back
          // into the pLay.  Continue as long as these Frames land in pLay.
        } while ( pFrame &&
                  ( pLay->IsAnLower( pFrame ) ||
                    ( pSect &&
                      ( ( pSect->HasFollow() &&
                          ( pLay->IsAnLower( pLast ) ||
                            ( pLast->IsInSct() &&
                              pLast->FindSctFrame()->IsAnFollow(pSect) ) ) &&
                          pSect->GetFollow()->IsAnLower( pFrame )  ) ||
                        ( pFrame->IsInSct() &&
                          pFrame->FindSctFrame()->IsAnFollow( pSect ) ) ) ) ) );
        if( pSect )
        {
            if( bCollect )
            {
                pLay->GetFormat()->GetDoc().getIDocumentLayoutAccess().GetLayouter()->InsertEndnotes(pSect);
                pSect->CalcFootnoteContent();
            }
            if( pSect->HasFollow() )
            {
                SwSectionFrame* pNxt = pSect->GetFollow();
                while( pNxt && !pNxt->ContainsContent() )
                    pNxt = pNxt->GetFollow();
                if( pNxt )
                    pNxt->CalcFootnoteContent();
            }
            if( bCollect )
            {
                pFrame = pLay->ContainsAny();
                bCollect = false;
                if( pFrame )
                    continue;
            }
        }
        break;
    }
    while( true );
}

void SwFlyFrame::MakeObjPos()
{
    if ( isFrameAreaPositionValid() )
        return;

    vcl::RenderContext* pRenderContext = getRootFrame()->GetCurrShell()->GetOut();
    setFrameAreaPositionValid(true);

    // use new class to position object
    GetAnchorFrame()->Calc(pRenderContext);
    objectpositioning::SwToLayoutAnchoredObjectPosition
            aObjPositioning( *GetVirtDrawObj() );
    aObjPositioning.CalcPosition();

    // #i58280#
    // update relative position
    SetCurrRelPos( aObjPositioning.GetRelPos() );

    {
        SwRectFnSet aRectFnSet(GetAnchorFrame());
        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
        aFrm.Pos( aObjPositioning.GetRelPos() );
        aFrm.Pos() += aRectFnSet.GetPos(GetAnchorFrame()->getFrameArea());
    }

    // #i69335#
    InvalidateObjRectWithSpaces();
}

void SwFlyFrame::MakePrtArea( const SwBorderAttrs &rAttrs )
{
    if ( !isFramePrintAreaValid() )
    {
        setFramePrintAreaValid(true);

        // consider vertical layout
        SwRectFnSet aRectFnSet(this);
        SwTwips nLeftLine = rAttrs.CalcLeftLine();

        // The fly frame may be partially outside the page, check for this case.
        SwPageFrame* pPageFrame = FindPageFrame();
        SwFrameFormat* pFlyFormat = GetFormat();
        SwFrameFormat* pDrawFormat = SwTextBoxHelper::getOtherTextBoxFormat(pFlyFormat, RES_FLYFRMFMT);
        const SwFrameFormat* pFormat = pDrawFormat ? pDrawFormat : pFlyFormat;
        // Don't increase the left padding if the wrap mode is through.
        bool bIsWrapThrough = pFormat && pFormat->GetSurround().GetSurround() == text::WrapTextMode::WrapTextMode_THROUGH;
        if (pPageFrame && pFlyFormat && !bIsWrapThrough)
        {
            const IDocumentSettingAccess& rIDSA = pFlyFormat->getIDocumentSettingAccess();
            bool bDoNotCaptureDrawObjsOnPage = rIDSA.get(DocumentSettingId::DO_NOT_CAPTURE_DRAW_OBJS_ON_PAGE);
            bool bLRTB = pFlyFormat->GetFrameDir().GetValue() == SvxFrameDirection::Horizontal_LR_TB;
            SwTwips nFlyLeft = getFrameArea().Left();
            SwTwips nPageLeft = pPageFrame->getFrameArea().Left();
            if (bDoNotCaptureDrawObjsOnPage && bLRTB && nFlyLeft < nPageLeft)
            {
                // It is outside: only start the left padding of the text inside the page frame,
                // when we're in Word compatibility mode.
                nLeftLine += (nPageLeft - nFlyLeft);
            }
        }

        aRectFnSet.SetXMargins( *this, nLeftLine,
                                        rAttrs.CalcRightLine() );
        aRectFnSet.SetYMargins( *this, rAttrs.CalcTopLine(),
                                        rAttrs.CalcBottomLine() );
    }
}

void SwFlyFrame::MakeContentPos( const SwBorderAttrs &rAttrs )
{
    if ( m_bValidContentPos )
        return;

    m_bValidContentPos = true;

    const SwTwips nUL = rAttrs.CalcTopLine()  + rAttrs.CalcBottomLine();
    Size aRelSize( CalcRel( GetFormat()->GetFrameSize() ) );

    SwRectFnSet aRectFnSet(this);
    tools::Long nMinHeight = 0;
    if( IsMinHeight() )
        nMinHeight = aRectFnSet.IsVert() ? aRelSize.Width() : aRelSize.Height();

    Point aNewContentPos = getFramePrintArea().Pos();
    const SdrTextVertAdjust nAdjust = GetFormat()->GetTextVertAdjust().GetValue();

    if( nAdjust != SDRTEXTVERTADJUST_TOP )
    {
        const SwTwips nContentHeight = CalcContentHeight(&rAttrs, nMinHeight, nUL);
        SwTwips nDiff = 0;

        if( nContentHeight != 0)
            nDiff = aRectFnSet.GetHeight(getFramePrintArea()) - nContentHeight;

        if( nDiff > 0 )
        {
            if( nAdjust == SDRTEXTVERTADJUST_CENTER )
            {
                if( aRectFnSet.IsVertL2R() )
                    aNewContentPos.setX(aNewContentPos.getX() + nDiff/2);
                else if( aRectFnSet.IsVert() )
                    aNewContentPos.setX(aNewContentPos.getX() - nDiff/2);
                else
                    aNewContentPos.setY(aNewContentPos.getY() + nDiff/2);
            }
            else if( nAdjust == SDRTEXTVERTADJUST_BOTTOM )
            {
                if( aRectFnSet.IsVertL2R() )
                    aNewContentPos.setX(aNewContentPos.getX() + nDiff);
                else if( aRectFnSet.IsVert() )
                    aNewContentPos.setX(aNewContentPos.getX() - nDiff);
                else
                    aNewContentPos.setY(aNewContentPos.getY() + nDiff);
            }
        }
    }
    if( aNewContentPos != ContentPos() )
    {
        ContentPos() = aNewContentPos;
        for( SwFrame *pFrame = Lower(); pFrame; pFrame = pFrame->GetNext())
        {
            pFrame->InvalidatePos();
        }
    }

}

void SwFlyFrame::InvalidateContentPos()
{
    m_bValidContentPos = false;
    Invalidate_();
}

void SwFlyFrame::SelectionHasChanged(SwFEShell* pShell)
{
    SwWrtShell* pWrtSh = dynamic_cast< SwWrtShell* >(pShell);
    if (pWrtSh == nullptr)
        return;

    UpdateUnfloatButton(pWrtSh, IsShowUnfloatButton(pWrtSh));
}

bool SwFlyFrame::IsShowUnfloatButton(SwWrtShell* pWrtSh) const
{
    if (pWrtSh == nullptr)
        return false;

    // In read only mode we don't allow unfloat operation
    if (pWrtSh->GetViewOptions()->IsReadonly())
        return false;

    const SdrObject *pObj = GetFrameFormat()->FindRealSdrObject();
    if (pObj == nullptr)
        return false;

    // SwFlyFrame itself can mean images, ole objects, etc, but we interested in actual text frames
    if (SwFEShell::GetObjCntType(*pObj) != OBJCNT_FLY)
        return false;

    // We show the button only for the selected text frame
    SwDrawView *pView = pWrtSh->Imp()->GetDrawView();
    if (pView == nullptr)
        return false;

    // Fly frame can be selected only alone
    if (pView->GetMarkedObjectList().GetMarkCount() != 1)
        return false;

    if(!pView->IsObjMarked(pObj))
        return false;

    // A frame is a floating table if there is only one table (and maybe some whitespaces) inside it
    int nTableCount = 0;
    const SwFrame* pLower = GetLower();
    const SwTabFrame* pTable = nullptr;
    while (pLower)
    {
        if (pLower->IsTabFrame())
        {
            pTable = static_cast<const SwTabFrame*>(pLower);
            ++nTableCount;
            if (nTableCount > 1)
                return false;
        }

        if (pLower->IsTextFrame())
        {
            const SwTextFrame* pTextFrame = static_cast<const SwTextFrame*>(pLower);
            if (!o3tl::trim(pTextFrame->GetText()).empty())
                return false;
        }
        pLower = pLower->GetNext();
    }

    if (nTableCount != 1 || pTable == nullptr)
        return false;

    // Show the unfold button only for multipage tables
    const SwBodyFrame *pBody = GetAnchorFrame()->FindBodyFrame();
    if (pBody == nullptr)
        return false;

    tools::Long nBodyHeight = pBody->getFrameArea().Height();
    tools::Long nTableHeight = pTable->getFrameArea().Height();
    tools::Long nFrameOffset = std::abs(GetAnchorFrame()->getFrameArea().Top() - pBody->getFrameArea().Top());

    return nBodyHeight < nTableHeight + nFrameOffset;
}

void SwFlyFrame::ActiveUnfloatButton(SwWrtShell* pWrtSh)
{
    SwEditWin& rEditWin = pWrtSh->GetView().GetEditWin();
    SwFrameControlsManager& rMngr = rEditWin.GetFrameControlsManager();
    SwFrameControlPtr pControl = rMngr.GetControl(FrameControlType::FloatingTable, this);
    if (pControl && pControl->GetIFacePtr())
    {
        pControl->GetIFacePtr()->GetButton()->clicked();
    }
}

void SwFlyFrame::UpdateUnfloatButton(SwWrtShell* pWrtSh, bool bShow) const
{
    if (pWrtSh == nullptr)
        return;

    SwEditWin& rEditWin = pWrtSh->GetView().GetEditWin();
    SwFrameControlsManager& rMngr = rEditWin.GetFrameControlsManager();
    Point aTopRightPixel = rEditWin.LogicToPixel( getFrameArea().TopRight() );
    rMngr.SetUnfloatTableButton(this, bShow,  aTopRightPixel);
}

SwFlyAtContentFrame* SwFlyFrame::DynCastFlyAtContentFrame()
{
    return IsFlyAtContentFrame() ? static_cast<SwFlyAtContentFrame*>(this) : nullptr;
}

SwTwips SwFlyFrame::Grow_(SwTwips nDist, SwResizeLimitReason& reason, bool bTst)
{
    if (!Lower())
    {
        reason = SwResizeLimitReason::Unspecified; // refusing because we have no content?
        return 0;
    }
    if (IsColLocked() || HasFixSize())
    {
        if (nDist <= 0 || !HasFixSize())
            reason = SwResizeLimitReason::Unspecified;
        else
            reason = GetNextLink() ? SwResizeLimitReason::FlowToFollow
                                   : SwResizeLimitReason::FixedSizeFrame;
        return 0;
    }

    SwRectFnSet aRectFnSet(this);
    SwTwips nSize = aRectFnSet.GetHeight(getFrameArea());
    if( nSize > 0 && nDist > ( LONG_MAX - nSize ) )
        nDist = LONG_MAX - nSize;

    if ( nDist <= 0 )
    {
        reason = SwResizeLimitReason::Unspecified;
        return 0;
    }

    if ( Lower()->IsColumnFrame() )
    {   // If it's a Column Frame, the Format takes control of the
        // resizing (due to the adjustment).
        if ( !bTst )
        {
            // #i28701# - unlock position of Writer fly frame
            UnlockPosition();
            InvalidatePos_();
            InvalidateSize();
        }
        reason = SwResizeLimitReason::BalancedColumns;
        return 0;
    }

    reason = SwResizeLimitReason::Unspecified;

    if (bTst)
    {
        // We're in test mode. Don't promise infinite growth for split flys, rather limit the
        // max size to the bottom of the upper.
        const SwFrame* pAnchor = GetAnchorFrame();
        if (SwFrame* pAnchorChar = FindAnchorCharFrame())
        {
            pAnchor = pAnchorChar;
        }
        if (pAnchor && IsFlySplitAllowed())
        {
            SwTwips nDeadline = GetFlyAnchorBottom(this, *pAnchor);
            SwTwips nTop = aRectFnSet.GetTop(getFrameArea());
            SwTwips nBottom = nTop + aRectFnSet.GetHeight(getFrameArea());
            // Calculate max grow and compare to the requested growth, adding to nDist may
            // overflow when it's LONG_MAX.
            SwTwips nMaxGrow = nDeadline - nBottom;
            if (nDist > nMaxGrow)
            {
                nDist = nMaxGrow;
                reason = SwResizeLimitReason::FlowToFollow;
            }
        }
        return nDist;
    }

    const SwRect aOld( GetObjRectWithSpaces() );
    InvalidateSize_();
    const bool bOldLock = m_bLocked;
    Unlock();
    if ( IsFlyFreeFrame() )
    {
        // #i37068# - no format of position here
        // and prevent move in method <CheckClip(..)>.
        // This is needed to prevent layout loop caused by nested
        // Writer fly frames - inner Writer fly frames format its
        // anchor, which grows/shrinks the outer Writer fly frame.
        // Note: position will be invalidated below.
        setFrameAreaPositionValid(true);

        // #i55416#
        // Suppress format of width for autowidth frame, because the
        // format of the width would call <SwTextFrame::CalcFitToContent()>
        // for the lower frame, which initiated this grow.
        const bool bOldFormatHeightOnly = m_bFormatHeightOnly;
        const SwFormatFrameSize& rFrameSz = GetFormat()->GetFrameSize();
        if ( rFrameSz.GetWidthSizeType() != SwFrameSize::Fixed )
        {
            m_bFormatHeightOnly = true;
        }
        SwViewShell* pSh = getRootFrame()->GetCurrShell();
        if (pSh)
        {
            static_cast<SwFlyFreeFrame*>(this)->SetNoMoveOnCheckClip( true );
            static_cast<SwFlyFreeFrame*>(this)->SwFlyFreeFrame::MakeAll(pSh->GetOut());
            static_cast<SwFlyFreeFrame*>(this)->SetNoMoveOnCheckClip( false );
        }
        // #i55416#
        if ( rFrameSz.GetWidthSizeType() != SwFrameSize::Fixed )
        {
            m_bFormatHeightOnly = bOldFormatHeightOnly;
        }
    }
    else
        MakeAll(getRootFrame()->GetCurrShell()->GetOut());
    InvalidateSize_();
    InvalidatePos();
    if ( bOldLock )
        Lock();
    SwRect aNew(GetObjRectWithSpaces());
    if (IsFlySplitAllowed() && aNew.Height() - aOld.Height() < nDist)
    {
        // We are allowed to split and the actual growth is less than the requested growth.
        const SwFrame* pAnchor = GetAnchorFrame();
        if (SwFrame* pAnchorChar = FindAnchorCharFrame())
        {
            pAnchor = pAnchorChar;
        }
        if (pAnchor)
        {
            SwTwips nDeadline = GetFlyAnchorBottom(this, *pAnchor);
            SwTwips nTop = aRectFnSet.GetTop(getFrameArea());
            SwTwips nBottom = nTop + aRectFnSet.GetHeight(getFrameArea());
            SwTwips nMaxGrow = nDeadline - nBottom;
            if (nDist > nMaxGrow)
            {
                // The requested growth is more than what we can provide, limit it.
                nDist = nMaxGrow;
                reason = SwResizeLimitReason::FlowToFollow;
            }
            // Grow & invalidate the size.
            SwTwips nRemaining = nDist - (aNew.Height() - aOld.Height());
            {
                SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
                aRectFnSet.AddBottom(aFrm, nRemaining);
            }
            InvalidateObjRectWithSpaces();
            {
                // Margins are unchanged, so increase the print height similar to the frame
                // height.
                SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
                aRectFnSet.AddBottom(aPrt, nRemaining );
            }
            aNew = GetObjRectWithSpaces();
        }
    }
    if ( aOld != aNew )
        ::Notify( this, FindPageFrame(), aOld );
    return aRectFnSet.GetHeight(aNew)-aRectFnSet.GetHeight(aOld);
}

SwTwips SwFlyFrame::Shrink_( SwTwips nDist, bool bTst )
{
    SwFrame* pLower = Lower();
    if( pLower && !IsColLocked() && !HasFixSize() )
    {
        SwRectFnSet aRectFnSet(this);
        SwTwips nHeight = aRectFnSet.GetHeight(getFrameArea());
        if ( nDist > nHeight )
            nDist = nHeight;

        SwTwips nVal = nDist;
        if ( IsMinHeight() )
        {
            const SwFormatFrameSize& rFormatSize = GetFormat()->GetFrameSize();
            SwTwips nFormatHeight = aRectFnSet.IsVert() ? rFormatSize.GetWidth() : rFormatSize.GetHeight();

            nVal = std::min( nDist, nHeight - nFormatHeight );
        }

        if ( nVal <= 0 )
            return 0;

        if ( pLower->IsColumnFrame() )
        {   // If it's a Column Frame, the Format takes control of the
            // resizing (due to the adjustment).
            if ( !bTst )
            {
                SwRect aOld( GetObjRectWithSpaces() );

                {
                    SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
                    aRectFnSet.SetHeight( aFrm, nHeight - nVal );
                }

                // #i68520#
                if ( nHeight - nVal != 0 )
                {
                    InvalidateObjRectWithSpaces();
                }

                nHeight = aRectFnSet.GetHeight(getFramePrintArea());

                {
                    SwFrameAreaDefinition::FramePrintAreaWriteAccess aPrt(*this);
                    aRectFnSet.SetHeight( aPrt, nHeight - nVal );
                }

                InvalidatePos_();
                InvalidateSize();
                ::Notify( this, FindPageFrame(), aOld );
                NotifyDrawObj();
                if ( GetAnchorFrame()->IsInFly() )
                    AnchorFrame()->FindFlyFrame()->Shrink( nDist, bTst );
            }
            return 0;
        }

        if ( !bTst )
        {
            const SwRect aOld( GetObjRectWithSpaces() );
            InvalidateSize_();
            const bool bOldLocked = m_bLocked;
            Unlock();
            if ( IsFlyFreeFrame() )
            {
                // #i37068# - no format of position here
                // and prevent move in method <CheckClip(..)>.
                // This is needed to prevent layout loop caused by nested
                // Writer fly frames - inner Writer fly frames format its
                // anchor, which grows/shrinks the outer Writer fly frame.
                // Note: position will be invalidated below.
                setFrameAreaPositionValid(true);

                // #i55416#
                // Suppress format of width for autowidth frame, because the
                // format of the width would call <SwTextFrame::CalcFitToContent()>
                // for the lower frame, which initiated this shrink.
                const bool bOldFormatHeightOnly = m_bFormatHeightOnly;
                const SwFormatFrameSize& rFrameSz = GetFormat()->GetFrameSize();
                if ( rFrameSz.GetWidthSizeType() != SwFrameSize::Fixed )
                {
                    m_bFormatHeightOnly = true;
                }
                static_cast<SwFlyFreeFrame*>(this)->SetNoMoveOnCheckClip( true );
                static_cast<SwFlyFreeFrame*>(this)->SwFlyFreeFrame::MakeAll(getRootFrame()->GetCurrShell()->GetOut());
                static_cast<SwFlyFreeFrame*>(this)->SetNoMoveOnCheckClip( false );
                // #i55416#
                if ( rFrameSz.GetWidthSizeType() != SwFrameSize::Fixed )
                {
                    m_bFormatHeightOnly = bOldFormatHeightOnly;
                }
            }
            else
                MakeAll(getRootFrame()->GetCurrShell()->GetOut());
            InvalidateSize_();
            InvalidatePos();
            if ( bOldLocked )
                Lock();
            const SwRect aNew( GetObjRectWithSpaces() );
            if ( aOld != aNew )
            {
                ::Notify( this, FindPageFrame(), aOld );
                if ( GetAnchorFrame()->IsInFly() )
                    AnchorFrame()->FindFlyFrame()->Shrink( nDist, bTst );
            }
            return aRectFnSet.GetHeight(aOld) -
                   aRectFnSet.GetHeight(aNew);
        }
        return nVal;
    }
    return 0;
}

bool SwFlyFrame::IsResizeValid(const SwBorderAttrs *pAttrs, Size aTargetSize)
{
    SwFormatFrameSize rFrameSz = GetFormat()->GetFrameSize();
    Size aFrameSize = rFrameSz.GetSize();
    bool bAutosizeHeight = !HasFixSize() && IsMinHeight();
    bool bAutosizeWidth =  !m_bFormatHeightOnly && rFrameSz.GetWidthSizeType() == SwFrameSize::Minimum;

    if (!bAutosizeHeight && !bAutosizeWidth)
        return true;

    bool bIsValidResize = true;

    /**
    if (either AutoSizeWidth or AutoSizeHeight, not both),
        if the autosize dimension goes smaller than min value and the other dimension changed
            return valid
        else
            remember invalid
    */
    tools::Long nMinFrameHeight = 0;
    if (bAutosizeHeight)
    {
        const SwTwips nUL = pAttrs->CalcTopLine()  + pAttrs->CalcBottomLine();
        rFrameSz.SetHeight(aTargetSize.Height());
        rFrameSz.SetWidth(aTargetSize.Width());
        Size aRelSize( CalcRel( rFrameSz ) );

        tools::Long nMinHeight = 0;
        SwRectFnSet aRectFnSet(this);
        nMinHeight = aRectFnSet.IsVert() ? aRelSize.Width() : aRelSize.Height();
        SwTwips nRemaining = CalcContentHeight(pAttrs, nMinHeight, nUL);
        nMinFrameHeight = nRemaining + nUL;

        if (nMinHeight < nMinFrameHeight)
        {
            bIsValidResize = false;
            // if height less than minHeight and width changed when not AutoSizeWidth
            if (!bAutosizeWidth && aTargetSize.Width() != aFrameSize.Width())
                return true;
        }
    }

    tools::Long nMinFrameWidth = 0;
    if (bAutosizeWidth)
    {
        const SwTwips nLR = pAttrs->CalcLeftLine() + pAttrs->CalcRightLine();
        const SwTwips nAutoWidth = lcl_CalcAutoWidth( *this );
        nMinFrameWidth = nAutoWidth + nLR;

        if (aTargetSize.Width() < nMinFrameWidth)
            bIsValidResize = false;
        if (!bAutosizeHeight && aTargetSize.Height() != aFrameSize.Height())
            return true;
    }

    // if not valid resize, and both AutoSizeWidth and AutoSizeHeight,
    // then consider resize is valid if any one of the dimensions was changed from its original size
    // (the frame's dimensions), and the destination dimension is a valid one.
    if (bAutosizeWidth && bAutosizeHeight && !bIsValidResize)
    {
        return (aTargetSize.Width() != aFrameSize.Width() && aTargetSize.Width() >= nMinFrameWidth) ||
            (aTargetSize.Height() != aFrameSize.Height() && aTargetSize.Height() >= nMinFrameHeight);
    }

    return bIsValidResize;
}

Size SwFlyFrame::ChgSize( const Size& aNewSize )
{
    // #i53298#
    // If the fly frame anchored at-paragraph or at-character contains an OLE
    // object, assure that the new size fits into the current clipping area
    // of the fly frame
    Size aAdjustedNewSize( aNewSize );
    if (dynamic_cast<SwFlyAtContentFrame*>(this))
    {
        auto pLower = dynamic_cast<SwNoTextFrame*>(Lower());
        if ( pLower && pLower->GetNode()->GetOLENode() )
        {
            SwRect aClipRect;
            ::CalcClipRect( GetVirtDrawObj(), aClipRect, false );
            if ( aAdjustedNewSize.Width() > aClipRect.Width() )
            {
                aAdjustedNewSize.setWidth( aClipRect.Width() );
            }
            if ( aAdjustedNewSize.Height() > aClipRect.Height() )
            {
                aAdjustedNewSize.setWidth( aClipRect.Height() );
            }
        }
    }

    if ( aAdjustedNewSize != getFrameArea().SSize() )
    {
        SwFrameFormat *pFormat = GetFormat();
        SwFormatFrameSize aSz( pFormat->GetFrameSize() );
        aSz.SetWidth( aAdjustedNewSize.Width() );
        aSz.SetHeight( aAdjustedNewSize.Height() );
        // go via the Doc for UNDO
        pFormat->GetDoc().SetAttr( aSz, *pFormat );
        return aSz.GetSize();
    }
    else
        return getFrameArea().SSize();
}

bool SwFlyFrame::IsLowerOf( const SwLayoutFrame* pUpperFrame ) const
{
    OSL_ENSURE( GetAnchorFrame(), "8-( Fly is lost in Space." );
    const SwFrame* pFrame = GetAnchorFrame();
    do
    {
        if ( pFrame == pUpperFrame )
            return true;
        pFrame = pFrame->IsFlyFrame()
               ? static_cast<const SwFlyFrame*>(pFrame)->GetAnchorFrame()
               : pFrame->GetUpper();
    } while ( pFrame );
    return false;
}

void SwFlyFrame::Cut()
{
}

void SwFrame::AppendFly( SwFlyFrame *pNew )
{
    if (!m_pDrawObjs)
    {
        m_pDrawObjs.reset(new SwSortedObjs());
    }
    m_pDrawObjs->Insert( *pNew );
    pNew->ChgAnchorFrame( this );

    // Register at the page
    // If there's none present, register via SwPageFrame::PreparePage
    SwPageFrame* pPage = FindPageFrame();
    if ( pPage != nullptr )
    {
        pPage->AppendFlyToPage( pNew );
    }
}

void SwFrame::RemoveFly( SwFlyFrame *pToRemove )
{
    // Deregister from the page
    // Could already have happened, if the page was already destructed
    SwPageFrame *pPage = pToRemove->FindPageFrame();
    if ( pPage && pPage->GetSortedObjs() )
    {
        pPage->RemoveFlyFromPage( pToRemove );
    }
    // #i73201#
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
    else
    {
        if ( pToRemove->IsAccessibleFrame() &&
             pToRemove->GetFormat() &&
             !pToRemove->IsFlyInContentFrame() )
        {
            SwRootFrame *pRootFrame = getRootFrame();
            if( pRootFrame && pRootFrame->IsAnyShellAccessible() )
            {
                SwViewShell *pVSh = pRootFrame->GetCurrShell();
                if( pVSh && pVSh->Imp() )
                {
                    pVSh->Imp()->DisposeAccessibleFrame( pToRemove );
                }
            }
        }
    }
#endif

    m_pDrawObjs->Remove(*pToRemove);
    if (!m_pDrawObjs->size())
    {
        m_pDrawObjs.reset();
    }

    pToRemove->ChgAnchorFrame( nullptr );

    if ( !pToRemove->IsFlyInContentFrame() && GetUpper() && IsInTab() )//MA_FLY_HEIGHT
        GetUpper()->InvalidateSize();
}

void SwFrame::AppendDrawObj( SwAnchoredObject& _rNewObj )
{
    assert(!m_pDrawObjs || m_pDrawObjs->is_sorted());

    if ( dynamic_cast<const SwAnchoredDrawObject*>( &_rNewObj) ==  nullptr )
    {
        OSL_FAIL( "SwFrame::AppendDrawObj(..) - anchored object of unexpected type -> object not appended" );
        return;
    }

    if ( dynamic_cast<const SwDrawVirtObj*>(_rNewObj.GetDrawObj()) ==  nullptr &&
         _rNewObj.GetAnchorFrame() && _rNewObj.GetAnchorFrame() != this )
    {
        assert(!m_pDrawObjs || m_pDrawObjs->is_sorted());
        // perform disconnect from layout, if 'master' drawing object is appended
        // to a new frame.
        if (SwDrawContact* pContact = static_cast<SwDrawContact*>(::GetUserCall( _rNewObj.GetDrawObj() )))
            pContact->DisconnectFromLayout( false );
        assert(!m_pDrawObjs || m_pDrawObjs->is_sorted());
    }

    if ( _rNewObj.GetAnchorFrame() != this )
    {
        if (!m_pDrawObjs)
        {
            m_pDrawObjs.reset(new SwSortedObjs());
        }
        m_pDrawObjs->Insert(_rNewObj);
        _rNewObj.ChgAnchorFrame( this );
    }

    // #i113730#
    // Assure the control objects and group objects containing controls are on the control layer
    if ( ::CheckControlLayer( _rNewObj.DrawObj() ) )
    {
        const IDocumentDrawModelAccess& rIDDMA = getIDocumentDrawModelAccess();
        const SdrLayerID aCurrentLayer(_rNewObj.DrawObj()->GetLayer());
        const SdrLayerID aControlLayerID(rIDDMA.GetControlsId());
        const SdrLayerID aInvisibleControlLayerID(rIDDMA.GetInvisibleControlsId());

        if(aCurrentLayer != aControlLayerID && aCurrentLayer != aInvisibleControlLayerID)
        {
            if ( aCurrentLayer == rIDDMA.GetInvisibleHellId() ||
                 aCurrentLayer == rIDDMA.GetInvisibleHeavenId() )
            {
                _rNewObj.DrawObj()->SetLayer(aInvisibleControlLayerID);
            }
            else
            {
                _rNewObj.DrawObj()->SetLayer(aControlLayerID);
            }
            //The layer is part of the key used to sort the obj, so update
            //its position if the layer changed.
            m_pDrawObjs->Update(_rNewObj);
        }
    }

    // no direct positioning needed, but invalidate the drawing object position
    _rNewObj.InvalidateObjPos();

    // register at page frame
    SwPageFrame* pPage = FindPageFrame();
    if ( pPage )
    {
        pPage->AppendDrawObjToPage( _rNewObj );
    }

    // Notify accessible layout.
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
    SwViewShell* pSh = getRootFrame()->GetCurrShell();
    if( pSh )
    {
        SwRootFrame* pLayout = getRootFrame();
        if( pLayout && pLayout->IsAnyShellAccessible() )
        {
            pSh->Imp()->AddAccessibleObj( _rNewObj.GetDrawObj() );
        }
    }
#endif

    assert(!m_pDrawObjs || m_pDrawObjs->is_sorted());
}

void SwFrame::RemoveDrawObj( SwAnchoredObject& _rToRemoveObj )
{
    // Notify accessible layout.
#if !ENABLE_WASM_STRIP_ACCESSIBILITY
    if (!mbInDtor)
    {
        SwViewShell* pSh = getRootFrame()->GetCurrShell();
        if (pSh)
        {
            SwRootFrame* pLayout = getRootFrame();
            if (pLayout && pLayout->IsAnyShellAccessible())
                pSh->Imp()->DisposeAccessibleObj(_rToRemoveObj.GetDrawObj(), false);
        }
    }
#endif

    // deregister from page frame
    SwPageFrame* pPage = _rToRemoveObj.GetPageFrame();
    if ( pPage && pPage->GetSortedObjs() )
        pPage->RemoveDrawObjFromPage( _rToRemoveObj );

    m_pDrawObjs->Remove(_rToRemoveObj);
    if (!m_pDrawObjs->size())
    {
        m_pDrawObjs.reset();
    }
    _rToRemoveObj.ChgAnchorFrame( nullptr );

    assert(!m_pDrawObjs || m_pDrawObjs->is_sorted());
}

void SwFrame::InvalidateObjs( const bool _bNoInvaOfAsCharAnchoredObjs )
{
    if ( !GetDrawObjs() )
        return;

    // #i26945# - determine page the frame is on,
    // in order to check, if anchored object is registered at the same
    // page.
    const SwPageFrame* pPageFrame = FindPageFrame();
    // #i28701# - re-factoring
    for (SwAnchoredObject* pAnchoredObj : *GetDrawObjs())
    {
        if ( _bNoInvaOfAsCharAnchoredObjs &&
             (pAnchoredObj->GetFrameFormat()->GetAnchor().GetAnchorId()
                == RndStdIds::FLY_AS_CHAR) )
        {
            continue;
        }
        // #i26945# - no invalidation, if anchored object
        // isn't registered at the same page and instead is registered at
        // the page, where its anchor character text frame is on.
        if ( pAnchoredObj->GetPageFrame() &&
             pAnchoredObj->GetPageFrame() != pPageFrame )
        {
            SwTextFrame* pAnchorCharFrame = pAnchoredObj->FindAnchorCharFrame();
            if ( pAnchorCharFrame &&
                 pAnchoredObj->GetPageFrame() == pAnchorCharFrame->FindPageFrame() )
            {
                continue;
            }
            // #115759# - unlock its position, if anchored
            // object isn't registered at the page, where its anchor
            // character text frame is on, respectively if it has no
            // anchor character text frame.
            else
            {
                pAnchoredObj->UnlockPosition();
            }
        }
        // #i51474# - reset flag, that anchored object
        // has cleared environment, and unlock its position, if the anchored
        // object is registered at the same page as the anchor frame is on.
        if ( pAnchoredObj->ClearedEnvironment() &&
             pAnchoredObj->GetPageFrame() &&
             pAnchoredObj->GetPageFrame() == pPageFrame )
        {
            pAnchoredObj->UnlockPosition();
            pAnchoredObj->SetClearedEnvironment( false );
        }
        // distinguish between writer fly frames and drawing objects
        if ( auto pFly = pAnchoredObj->DynCastFlyFrame() )
        {
            pFly->Invalidate_();
            pFly->InvalidatePos_();
        }
        else
        {
            pAnchoredObj->InvalidateObjPos();
        }
    } // end of loop on objects, which are connected to the frame
}

// #i26945# - correct check, if anchored object is a lower
// of the layout frame. E.g., anchor character text frame can be a follow text
// frame.
// #i44016# - add parameter <_bUnlockPosOfObjs> to
// force an unlockposition call for the lower objects.
void SwLayoutFrame::NotifyLowerObjs( const bool _bUnlockPosOfObjs )
{
    // invalidate lower floating screen objects
    SwPageFrame* pPageFrame = FindPageFrame();
    if ( !(pPageFrame && pPageFrame->GetSortedObjs()) )
        return;

    SwSortedObjs& rObjs = *(pPageFrame->GetSortedObjs());
    for (SwAnchoredObject* pObj : rObjs)
    {
        // #i26945# - check if anchored object is a lower
        // of the layout frame is changed to check, if its anchor frame
        // is a lower of the layout frame.
        // Determine the anchor frame - usually it's the anchor frame,
        // for at-character/as-character anchored objects the anchor character
        // text frame is taken.
        const SwFrame* pAnchorFrame = pObj->GetAnchorFrameContainingAnchPos();
        if ( auto pFly = pObj->DynCastFlyFrame() )
        {
            if ( pFly->getFrameArea().Left() == FAR_AWAY )
                continue;

            if ( pFly->IsAnLower( this ) )
                continue;

            // #i26945# - use <pAnchorFrame> to check, if
            // fly frame is lower of layout frame resp. if fly frame is
            // at a different page registered as its anchor frame is on.
            const bool bLow = IsAnLower( pAnchorFrame );
            if ( bLow || pAnchorFrame->FindPageFrame() != pPageFrame )
            {
                pFly->Invalidate_( pPageFrame );
                if ( !bLow || pFly->IsFlyAtContentFrame() )
                {
                    // #i44016#
                    if ( _bUnlockPosOfObjs )
                    {
                        pFly->UnlockPosition();
                    }
                    pFly->InvalidatePos_();
                }
                else
                    pFly->InvalidatePrt_();
            }
        }
        else
        {
            assert( dynamic_cast<const SwAnchoredDrawObject*>( pObj) &&
                    "<SwLayoutFrame::NotifyFlys() - anchored object of unexpected type" );
            // tdf#156728 invalidate fly positioned dependent on header/footer size
            bool isPositionedByHF(false);
            if (IsHeaderFrame() || IsFooterFrame())
            {
                auto const nO(pObj->GetFrameFormat()->GetVertOrient().GetRelationOrient());
                if (nO == text::RelOrientation::PAGE_PRINT_AREA
                    || nO == text::RelOrientation::PAGE_PRINT_AREA_BOTTOM
                    || nO == text::RelOrientation::PAGE_PRINT_AREA_TOP)
                {
                    isPositionedByHF = true;
                }
            }
            // #i26945# - use <pAnchorFrame> to check, if
            // fly frame is lower of layout frame resp. if fly frame is
            // at a different page registered as its anchor frame is on.
            if ( IsAnLower( pAnchorFrame ) ||
                 isPositionedByHF ||
                 pAnchorFrame->FindPageFrame() != pPageFrame )
            {
                // #i44016#
                if ( _bUnlockPosOfObjs )
                {
                    pObj->UnlockPosition();
                }
                pObj->InvalidateObjPos();
            }
        }
    }
}

void SwFlyFrame::NotifyDrawObj()
{
    SwVirtFlyDrawObj* pObj = GetVirtDrawObj();
    pObj->SetRect();
    pObj->SetBoundAndSnapRectsDirty();
    pObj->SetChanged();
    pObj->BroadcastObjectChange();

    if ( GetFormat()->GetSurround().IsContour() )
    {
        ClrContourCache( pObj );
    }
    else if(IsFlyFreeFrame() && static_cast< const SwFlyFreeFrame* >(this)->supportsAutoContour())
    {
        // RotateFlyFrame3: Also need to clear when changes happen
        // Caution: isTransformableSwFrame is already reset when resetting rotation, so
        // *additionally* reset in SwFlyFreeFrame::MakeAll when no more rotation
        ClrContourCache( pObj );
    }
}

Size SwFlyFrame::CalcRel( const SwFormatFrameSize &rSz ) const
{
    Size aRet( rSz.GetSize() );

    const SwFrame *pRel = IsFlyLayFrame() ? GetAnchorFrame() : GetAnchorFrame()->GetUpper();
    if( pRel ) // LAYER_IMPL
    {
        tools::Long nRelWidth = LONG_MAX, nRelHeight = LONG_MAX;
        const SwViewShell *pSh = getRootFrame()->GetCurrShell();
        if ( ( pRel->IsBodyFrame() || pRel->IsPageFrame() ) &&
             pSh && pSh->GetViewOptions()->getBrowseMode() &&
             pSh->VisArea().HasArea() )
        {
            nRelWidth  = pSh->GetBrowseWidth();
            nRelHeight = pSh->VisArea().Height();
            Size aBorder = pSh->GetOut()->PixelToLogic( pSh->GetBrowseBorder() );
            nRelWidth  = std::min( nRelWidth,  pRel->getFramePrintArea().Width() );
            nRelHeight -= 2*aBorder.Height();
            nRelHeight = std::min( nRelHeight, pRel->getFramePrintArea().Height() );
        }

        // At the moment only the "== PAGE_FRAME" and "!= PAGE_FRAME" cases are handled.
        // When size is a relative to page size, ignore size of SwBodyFrame.
        if (rSz.GetWidthPercentRelation() != text::RelOrientation::PAGE_FRAME)
            nRelWidth  = std::min( nRelWidth,  pRel->getFramePrintArea().Width() );
        else if ( pRel->IsPageFrame() )
            nRelWidth  = std::min( nRelWidth,  pRel->getFrameArea().Width() );

        if (rSz.GetHeightPercentRelation() != text::RelOrientation::PAGE_FRAME)
            nRelHeight = std::min( nRelHeight, pRel->getFramePrintArea().Height() );
        else if ( pRel->IsPageFrame() )
            nRelHeight = std::min( nRelHeight, pRel->getFrameArea().Height() );

        if( !pRel->IsPageFrame() )
        {
            const SwPageFrame* pPage = FindPageFrame();
            if( pPage )
            {
                if (rSz.GetWidthPercentRelation() == text::RelOrientation::PAGE_FRAME)
                    // Ignore margins of pPage.
                    nRelWidth  = std::min( nRelWidth,  pPage->getFrameArea().Width() );
                else
                    nRelWidth  = std::min( nRelWidth,  pPage->getFramePrintArea().Width() );
                if (rSz.GetHeightPercentRelation() == text::RelOrientation::PAGE_FRAME)
                    // Ignore margins of pPage.
                    nRelHeight = std::min( nRelHeight, pPage->getFrameArea().Height() );
                else
                    nRelHeight = std::min( nRelHeight, pPage->getFramePrintArea().Height() );
            }
        }

        if ( rSz.GetWidthPercent() && rSz.GetWidthPercent() != SwFormatFrameSize::SYNCED )
            aRet.setWidth(rtl::math::round(double(nRelWidth) * rSz.GetWidthPercent() / 100));
        if ( rSz.GetHeightPercent() && rSz.GetHeightPercent() != SwFormatFrameSize::SYNCED )
            aRet.setHeight(rtl::math::round(double(nRelHeight) * rSz.GetHeightPercent() / 100));

        if ( rSz.GetHeight() && rSz.GetWidthPercent() == SwFormatFrameSize::SYNCED )
        {
            aRet.setWidth( aRet.Width() * ( aRet.Height()) );
            aRet.setWidth( aRet.Width() / ( rSz.GetHeight()) );
        }
        else if ( rSz.GetWidth() && rSz.GetHeightPercent() == SwFormatFrameSize::SYNCED )
        {
            aRet.setHeight( aRet.Height() * ( aRet.Width()) );
            aRet.setHeight( aRet.Height() / ( rSz.GetWidth()) );
        }
    }
    return aRet;
}

static SwTwips lcl_CalcAutoWidth( const SwLayoutFrame& rFrame )
{
    SwTwips nRet = 0;
    SwTwips nMin = 0;
    const SwFrame* pFrame = rFrame.Lower();

    // No autowidth defined for columned frames
    if ( !pFrame || pFrame->IsColumnFrame() )
        return nRet;

    int nParagraphCount = 0;
    while ( pFrame )
    {
        nParagraphCount++;
        if ( pFrame->IsSctFrame() )
        {
            nMin = lcl_CalcAutoWidth( *static_cast<const SwSectionFrame*>(pFrame) );
        }
        if ( pFrame->IsTextFrame() )
        {
            nMin = const_cast<SwTextFrame*>(static_cast<const SwTextFrame*>(pFrame))->CalcFitToContent();
            auto const& rParaSet(static_cast<const SwTextFrame*>(pFrame)->GetTextNodeForParaProps()->GetSwAttrSet());
            SvxFirstLineIndentItem const& rFirstLine(rParaSet.GetFirstLineIndent());
            SvxTextLeftMarginItem const& rLeftMargin(rParaSet.GetTextLeftMargin());
            SvxRightMarginItem const& rRightMargin(rParaSet.GetRightMargin());
            if (!static_cast<const SwTextFrame*>(pFrame)->IsLocked())
            {
                nMin += rRightMargin.ResolveRight({}) + rLeftMargin.ResolveTextLeft({})
                        + rFirstLine.ResolveTextFirstLineOffset({});
            }
        }
        else if ( pFrame->IsTabFrame() )
        {
            const SwFormatFrameSize& rTableFormatSz = static_cast<const SwTabFrame*>(pFrame)->GetTable()->GetFrameFormat()->GetFrameSize();
            if ( USHRT_MAX == rTableFormatSz.GetSize().Width() ||
                 text::HoriOrientation::NONE == static_cast<const SwTabFrame*>(pFrame)->GetFormat()->GetHoriOrient().GetHoriOrient() )
            {
                const SwPageFrame* pPage = rFrame.FindPageFrame();
                // auto width table
                nMin = pFrame->GetUpper()->IsVertical() ?
                    pPage->getFramePrintArea().Height() :
                    pPage->getFramePrintArea().Width();
            }
            else
            {
                nMin = rTableFormatSz.GetSize().Width();
            }
        }

        if ( nMin > nRet )
            nRet = nMin;

        pFrame = pFrame->GetNext();
    }

    // tdf#124423 In Microsoft compatibility mode: widen the frame to max (PrintArea of the frame it anchored to) if it contains at least 2 paragraphs,
    // or 1 paragraph wider than its parent area.
    if (rFrame.GetFormat()->getIDocumentSettingAccess().get(DocumentSettingId::FRAME_AUTOWIDTH_WITH_MORE_PARA))
    {
        const SwFrame* pFrameRect = nullptr;
        if (rFrame.IsFlyFrame())
            pFrameRect = static_cast<const SwFlyFrame*>(&rFrame)->GetAnchorFrame();
        else
        {
            if (const SwFrame* pLower = rFrame.Lower())
                pFrameRect = pLower->FindPageFrame();
        }
        if (pFrameRect)
        {
            SwTwips nParentWidth = rFrame.IsVertical() ? pFrameRect->getFramePrintArea().Height() : pFrameRect->getFramePrintArea().Width();
            if (nParagraphCount > 1 || nRet > nParentWidth)
            {
                return nParentWidth;
            }
        }
    }

    return nRet;
}

///  #i13147# -  If called for paint and the <SwNoTextFrame> contains
/// a graphic, load of intrinsic graphic has to be avoided.
bool SwFlyFrame::GetContour( tools::PolyPolygon&   rContour,
                           const bool _bForPaint ) const
{
    vcl::RenderContext* pRenderContext = getRootFrame()->GetCurrShell()->GetOut();
    bool bRet = false;
    const SwFrame* pLower = Lower();
    const bool bIsCandidate(pLower && pLower->IsNoTextFrame());

    if(bIsCandidate)
    {
        if(GetFormat()->GetSurround().IsContour())
        {
            SwNoTextNode *pNd = const_cast<SwNoTextNode*>(static_cast<const SwNoTextNode*>(static_cast<const SwNoTextFrame*>(pLower)->GetNode()));
            // #i13147# - determine <GraphicObject> instead of <Graphic>
            // in order to avoid load of graphic, if <SwNoTextNode> contains a graphic
            // node and method is called for paint.
            std::unique_ptr<GraphicObject> xTmpGrfObj;
            const GraphicObject* pGrfObj = nullptr;
            const SwGrfNode* pGrfNd = pNd->GetGrfNode();
            if ( pGrfNd && _bForPaint )
            {
                pGrfObj = &(pGrfNd->GetGrfObj());
            }
            else
            {
                xTmpGrfObj.reset(new GraphicObject(pNd->GetGraphic()));
                pGrfObj = xTmpGrfObj.get();
            }
            assert(pGrfObj && "SwFlyFrame::GetContour() - No Graphic/GraphicObject found at <SwNoTextNode>.");
            if (pGrfObj->GetType() != GraphicType::NONE)
            {
                if( !pNd->HasContour() )
                {
                    //#i13147# - no <CreateContour> for a graphic
                    // during paint. Thus, return (value of <bRet> should be <false>).
                    if ( pGrfNd && _bForPaint )
                    {
                        OSL_FAIL( "SwFlyFrame::GetContour() - No Contour found at <SwNoTextNode> during paint." );
                        return bRet;
                    }
                    pNd->CreateContour();
                }
                pNd->GetContour( rContour );
                // The Node holds the Polygon matching the original size of the graphic
                // We need to include the scaling here
                SwRect aClip;
                SwRect aOrig;
                Lower()->Calc(pRenderContext);
                static_cast<const SwNoTextFrame*>(Lower())->GetGrfArea( aClip, &aOrig );
                // #i13147# - copy method code <SvxContourDlg::ScaleContour(..)>
                // in order to avoid that graphic has to be loaded for contour scale.
                //SvxContourDlg::ScaleContour( rContour, aGrf, MapUnit::MapTwip, aOrig.SSize() );
                {
                    OutputDevice*   pOutDev = Application::GetDefaultDevice();
                    const MapMode   aDispMap( MapUnit::MapTwip );
                    const MapMode   aGrfMap( pGrfObj->GetPrefMapMode() );
                    const Size      aGrfSize( pGrfObj->GetPrefSize() );
                    Size            aOrgSize;
                    Point           aNewPoint;
                    bool            bPixelMap = aGrfMap.GetMapUnit() == MapUnit::MapPixel;

                    if ( bPixelMap )
                        aOrgSize = pOutDev->PixelToLogic( aGrfSize, aDispMap );
                    else
                        aOrgSize = OutputDevice::LogicToLogic( aGrfSize, aGrfMap, aDispMap );

                    if ( aOrgSize.Width() && aOrgSize.Height() )
                    {
                        double fScaleX = static_cast<double>(aOrig.Width()) / aOrgSize.Width();
                        double fScaleY = static_cast<double>(aOrig.Height()) / aOrgSize.Height();

                        for ( sal_uInt16 j = 0, nPolyCount = rContour.Count(); j < nPolyCount; j++ )
                        {
                            tools::Polygon& rPoly = rContour[ j ];

                            for ( sal_uInt16 i = 0, nCount = rPoly.GetSize(); i < nCount; i++ )
                            {
                                if ( bPixelMap )
                                    aNewPoint = pOutDev->PixelToLogic( rPoly[ i ], aDispMap  );
                                else
                                    aNewPoint = OutputDevice::LogicToLogic( rPoly[ i ], aGrfMap, aDispMap  );

                                rPoly[ i ] = Point( basegfx::fround<tools::Long>( aNewPoint.getX() * fScaleX ), basegfx::fround<tools::Long>( aNewPoint.getY() * fScaleY ) );
                            }
                        }
                    }
                }
                // destroy created <GraphicObject>.
                xTmpGrfObj.reset();
                rContour.Move( aOrig.Left(), aOrig.Top() );
                if( !aClip.Width() )
                    aClip.Width( 1 );
                if( !aClip.Height() )
                    aClip.Height( 1 );
                rContour.Clip( aClip.SVRect() );
                rContour.Optimize(PolyOptimizeFlags::CLOSE);
                bRet = true;
            }
        }
        else if (IsFlyFreeFrame())
        {
            const SwFlyFreeFrame* pSwFlyFreeFrame(static_cast< const SwFlyFreeFrame* >(this));

            if(nullptr != pSwFlyFreeFrame &&
                pSwFlyFreeFrame->supportsAutoContour() &&
                // isTransformableSwFrame already used in supportsAutoContour(), but
                // better check twice when it may get changed there...
                pSwFlyFreeFrame->isTransformableSwFrame())
            {
                // RotateFlyFrame: use untransformed SwFrame to allow text floating around.
                // Will be transformed below
                const TransformableSwFrame* pTransformableSwFrame(pSwFlyFreeFrame->getTransformableSwFrame());
                const SwRect aFrameArea(pTransformableSwFrame->getUntransformedFrameArea());
                rContour = tools::PolyPolygon(tools::Polygon(aFrameArea.SVRect()));
                bRet = (0 != rContour.Count());
            }
        }

        if(bRet && 0 != rContour.Count())
        {
            if (IsFlyFreeFrame() &&
                static_cast< const SwFlyFreeFrame* >(this)->isTransformableSwFrame())
            {
                // Need to adapt contour to transformation
                basegfx::B2DVector aScale, aTranslate;
                double fRotate, fShearX;
                getFrameAreaTransformation().decompose(aScale, aTranslate, fRotate, fShearX);

                if(!basegfx::fTools::equalZero(fRotate))
                {
                    basegfx::B2DPolyPolygon aSource(rContour.getB2DPolyPolygon());
                    const basegfx::B2DPoint aCenter(getFrameAreaTransformation() * basegfx::B2DPoint(0.5, 0.5));
                    const basegfx::B2DHomMatrix aRotateAroundCenter(
                        basegfx::utils::createRotateAroundPoint(
                            aCenter.getX(),
                            aCenter.getY(),
                            fRotate));
                    aSource.transform(aRotateAroundCenter);
                    rContour = tools::PolyPolygon(aSource);
                }
            }
        }
    }

    return bRet;
}


const SwVirtFlyDrawObj* SwFlyFrame::GetVirtDrawObj() const
{
    return static_cast<const SwVirtFlyDrawObj*>(GetDrawObj());
}
SwVirtFlyDrawObj* SwFlyFrame::GetVirtDrawObj()
{
    return static_cast<SwVirtFlyDrawObj*>(DrawObj());
}

// implementation of pure virtual method declared in
// base class <SwAnchoredObject>

void SwFlyFrame::InvalidateObjPos()
{
    InvalidatePos();
    // #i68520#
    InvalidateObjRectWithSpaces();
}

SwFrameFormat* SwFlyFrame::GetFrameFormat()
{
    OSL_ENSURE( GetFormat(),
            "<SwFlyFrame::GetFrameFormat()> - missing frame format -> crash." );
    return GetFormat();
}
const SwFrameFormat* SwFlyFrame::GetFrameFormat() const
{
    OSL_ENSURE( GetFormat(),
            "<SwFlyFrame::GetFrameFormat()> - missing frame format -> crash." );
    return GetFormat();
}

SwRect SwFlyFrame::GetObjRect() const
{
    return getFrameArea();
}

// #i70122#
// for Writer fly frames the bounding rectangle equals the object rectangles
SwRect SwFlyFrame::GetObjBoundRect() const
{
    return GetObjRect();
}

// #i68520#
bool SwFlyFrame::SetObjTop_( const SwTwips _nTop )
{
    const bool bChanged( getFrameArea().Pos().getY() != _nTop );
    SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
    aFrm.Pos().setY(_nTop);

    return bChanged;
}
bool SwFlyFrame::SetObjLeft_( const SwTwips _nLeft )
{
    const bool bChanged( getFrameArea().Pos().getX() != _nLeft );
    SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*this);
    aFrm.Pos().setX(_nLeft);

    return bChanged;
}

/** method to assure that anchored object is registered at the correct
    page frame

    OD 2004-07-02 #i28701#
*/
void SwFlyFrame::RegisterAtCorrectPage()
{
    // default behaviour is to do nothing.
}

void SwFlyFrame::RegisterAtPage(SwPageFrame &)
{
    // default behaviour is to do nothing.
}

/** method to determine, if a <MakeAll()> on the Writer fly frame is possible

    OD 2004-05-11 #i28701#
*/
bool SwFlyFrame::IsFormatPossible() const
{
    return SwAnchoredObject::IsFormatPossible() &&
           !IsLocked() && !IsColLocked();
}

void SwFlyFrame::GetAnchoredObjects( std::vector<SwAnchoredObject*>& aVector, const SwFormat& rFormat )
{
    SwIterator<SwFlyFrame,SwFormat> aIter( rFormat );
    for( SwFlyFrame* pFlyFrame = aIter.First(); pFlyFrame; pFlyFrame = aIter.Next() )
        aVector.push_back( pFlyFrame );
}

const SwFlyFrameFormat * SwFlyFrame::GetFormat() const
{
    return static_cast< const SwFlyFrameFormat * >( GetDep() );
}

SwFlyFrameFormat * SwFlyFrame::GetFormat()
{
    return static_cast< SwFlyFrameFormat * >( GetDep() );
}

void SwFlyFrame::dumpAsXml(xmlTextWriterPtr writer) const
{
    (void)xmlTextWriterStartElement(writer, reinterpret_cast<const xmlChar*>("fly"));
    dumpAsXmlAttributes(writer);

    SwLayoutFrame::dumpAsXml(writer);

    SwAnchoredObject::dumpAsXml(writer);

    (void)xmlTextWriterEndElement(writer);
}

void SwFlyFrame::Calc(vcl::RenderContext* pRenderContext) const
{
    if ( !m_bValidContentPos )
        const_cast<SwFlyFrame*>(this)->PrepareMake(pRenderContext);
    else
        SwLayoutFrame::Calc(pRenderContext);
}

SwTwips SwFlyFrame::CalcContentHeight(const SwBorderAttrs *pAttrs, const SwTwips nMinHeight, const SwTwips nUL)
{
    SwRectFnSet aRectFnSet(this);
    SwTwips nHeight = 0;
    if ( Lower() )
    {
        if ( Lower()->IsColumnFrame() )
        {
            FormatWidthCols( *pAttrs, nUL, nMinHeight );
            nHeight = aRectFnSet.GetHeight(Lower()->getFrameArea());
        }
        else
        {
            SwFrame *pFrame = Lower();
            while ( pFrame )
            {
                nHeight += aRectFnSet.GetHeight(pFrame->getFrameArea());
                if( pFrame->IsTextFrame() && static_cast<SwTextFrame*>(pFrame)->IsUndersized() )
                // This TextFrame would like to be a bit larger
                    nHeight += static_cast<SwTextFrame*>(pFrame)->GetParHeight()
                            - aRectFnSet.GetHeight(pFrame->getFramePrintArea());
                else if( pFrame->IsSctFrame() && static_cast<SwSectionFrame*>(pFrame)->IsUndersized() )
                    nHeight += static_cast<SwSectionFrame*>(pFrame)->Undersize();
                pFrame = pFrame->GetNext();
            }
        }
        if ( GetDrawObjs() )
        {
            const size_t nCnt = GetDrawObjs()->size();
            SwTwips nTop = aRectFnSet.GetTop(getFrameArea());
            SwTwips nBorder = aRectFnSet.GetHeight(getFrameArea()) -
            aRectFnSet.GetHeight(getFramePrintArea());
            for ( size_t i = 0; i < nCnt; ++i )
            {
                SwAnchoredObject* pAnchoredObj = (*GetDrawObjs())[i];
                if ( auto pFly = pAnchoredObj->DynCastFlyFrame() )
                {
                    // consider only Writer fly frames, which follow the text flow.
                    if ( pFly->IsFlyLayFrame() &&
                        pFly->getFrameArea().Top() != FAR_AWAY &&
                        pFly->GetFormat()->GetFollowTextFlow().GetValue() )
                    {
                        SwTwips nDist = -aRectFnSet.BottomDist( pFly->getFrameArea(), nTop );
                        if( nDist > nBorder + nHeight )
                            nHeight = nDist - nBorder;
                    }
                }
            }
        }
    }
    return nHeight;
}

const SwFormatAnchor* SwFlyFrame::GetAnchorFromPoolItem(const SfxPoolItem& rItem)
{
    switch(rItem.Which())
    {
        case RES_ANCHOR:
            return static_cast<const SwFormatAnchor*>(&rItem);
        default:
            return nullptr;
    }
}

const SwFormatAnchor* SwFlyFrame::GetAnchorFromPoolItem(const SwAttrSetChg& rItem)
{
    return rItem.GetChgSet()->GetItem(RES_ANCHOR, false);
}

const SwFlyFrame* SwFlyFrame::DynCastFlyFrame() const
{
    return this;
}

SwFlyFrame* SwFlyFrame::DynCastFlyFrame()
{
    return this;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
