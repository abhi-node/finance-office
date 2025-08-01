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

#include <editeng/formatbreakitem.hxx>
#include <sal/log.hxx>
#include <osl/diagnose.h>
#include <tools/stream.hxx>
#include <doc.hxx>
#include <IDocumentStatistics.hxx>
#include <IDocumentLayoutAccess.hxx>
#include <docstat.hxx>
#include <fmtpdsc.hxx>
#include <laycache.hxx>
#include "layhelp.hxx"
#include <pagefrm.hxx>
#include <rootfrm.hxx>
#include <txtfrm.hxx>
#include <swtable.hxx>
#include <tabfrm.hxx>
#include <rowfrm.hxx>
#include <sectfrm.hxx>
#include <fmtcntnt.hxx>
#include <pagedesc.hxx>
#include <frmtool.hxx>
#include <dflyobj.hxx>
#include <dcontact.hxx>
#include <viewopt.hxx>
#include <flyfrm.hxx>
#include <sortedobjs.hxx>
#include <ndindex.hxx>
#include <node.hxx>
#include <ndtxt.hxx>
#include <frameformats.hxx>

#include <limits>

using namespace ::com::sun::star;

SwLayoutCache::SwLayoutCache() : m_nLockCount( 0 ) {}

/*
 *  Reading and writing of the layout cache.
 *  The layout cache is not necessary, but it improves
 *  the performance and reduces the text flow during
 *  the formatting.
 *  The layout cache contains the index of the paragraphs/tables
 *  at the top of every page, so it's possible to create
 *  the right count of pages and to distribute the document content
 *  to this pages before the formatting starts.
 */

void SwLayoutCache::Read( SvStream &rStream )
{
    if( !m_pImpl )
    {
        m_pImpl.reset( new SwLayCacheImpl );
        if( !m_pImpl->Read( rStream ) )
        {
            m_pImpl.reset();
        }
    }
}

void SwLayCacheImpl::Insert( sal_uInt16 nType, SwNodeOffset nIndex, sal_Int32 nOffset )
{
    m_aType.push_back( nType );
    mIndices.push_back( nIndex );
    m_aOffset.push_back( nOffset );
}

bool SwLayCacheImpl::Read( SvStream& rStream )
{
    SwLayCacheIoImpl aIo( rStream, false );
    if( aIo.GetMajorVersion() > SW_LAYCACHE_IO_VERSION_MAJOR )
        return false;

    // Due to an evil bug in the layout cache (#102759#), we cannot trust the
    // sizes of fly frames which have been written using the "old" layout cache.
    // This flag should indicate that we do not want to trust the width and
    // height of fly frames
    m_bUseFlyCache = aIo.GetMinorVersion() >= 1;

    aIo.OpenRec( SW_LAYCACHE_IO_REC_PAGES );
    aIo.OpenFlagRec();
    aIo.CloseFlagRec();
    while( aIo.BytesLeft() && !aIo.HasError() )
    {
        sal_uInt32 nIndex(0), nOffset(0);

        switch( aIo.Peek() )
        {
        case SW_LAYCACHE_IO_REC_PARA:
        {
            aIo.OpenRec( SW_LAYCACHE_IO_REC_PARA );
            sal_uInt8 cFlags = aIo.OpenFlagRec();
            aIo.GetStream().ReadUInt32( nIndex );
            if( (cFlags & 0x01) != 0 )
                aIo.GetStream().ReadUInt32( nOffset );
            else
                nOffset = COMPLETE_STRING;
            aIo.CloseFlagRec();
            Insert( SW_LAYCACHE_IO_REC_PARA, SwNodeOffset(nIndex), static_cast<sal_Int32>(nOffset) );
            aIo.CloseRec();
            break;
        }
        case SW_LAYCACHE_IO_REC_TABLE:
            aIo.OpenRec( SW_LAYCACHE_IO_REC_TABLE );
            aIo.OpenFlagRec();
            aIo.GetStream().ReadUInt32( nIndex )
                           .ReadUInt32( nOffset );
            Insert( SW_LAYCACHE_IO_REC_TABLE, SwNodeOffset(nIndex), static_cast<sal_Int32>(nOffset) );
            aIo.CloseFlagRec();
            aIo.CloseRec();
            break;
        case SW_LAYCACHE_IO_REC_FLY:
        {
            aIo.OpenRec( SW_LAYCACHE_IO_REC_FLY );
            aIo.OpenFlagRec();
            aIo.CloseFlagRec();
            sal_Int32 nX(0), nY(0), nW(0), nH(0);
            sal_uInt16 nPgNum(0);
            aIo.GetStream().ReadUInt16( nPgNum ).ReadUInt32( nIndex )
                   .ReadInt32( nX ).ReadInt32( nY ).ReadInt32( nW ).ReadInt32( nH );
            m_FlyCache.emplace_back( nPgNum, nIndex, nX, nY, nW, nH );
            aIo.CloseRec();
            break;
        }
        default:
            aIo.SkipRec();
            break;
        }
    }
    aIo.CloseRec();

    return !aIo.HasError();
}

/** writes the index (more precise: the difference between
 * the index and the first index of the document content)
 * of the first paragraph/table at the top of every page.
 * If at the top of a page is the rest of a paragraph/table
 * from the bottom of the previous page, the character/row
 * number is stored, too.
 * The position, size and page number of the text frames
 * are stored, too
 */
void SwLayoutCache::Write( SvStream &rStream, const SwDoc& rDoc )
{
    if( !rDoc.getIDocumentLayoutAccess().GetCurrentLayout() ) // the layout itself ..
        return;

    SwLayCacheIoImpl aIo( rStream, true );
    // We want to save the relative index, so we need the index
    // of the first content
    SwNodeOffset nStartOfContent = rDoc.GetNodes().GetEndOfContent().
                            StartOfSectionNode()->GetIndex();
    // The first page...
    SwPageFrame* pPage = const_cast<SwPageFrame*>(static_cast<const SwPageFrame*>(rDoc.getIDocumentLayoutAccess().GetCurrentLayout()->Lower()));

    aIo.OpenRec( SW_LAYCACHE_IO_REC_PAGES );
    aIo.OpenFlagRec( 0, 0 );
    aIo.CloseFlagRec();
    while( pPage )
    {
        if( pPage->GetPrev() )
        {
            SwLayoutFrame* pLay = pPage->FindBodyCont();
            SwFrame* pTmp = pLay ? pLay->ContainsAny() : nullptr;
            // We are only interested in paragraph or table frames,
            // a section frames contains paragraphs/tables.
            if( pTmp && pTmp->IsSctFrame() )
                pTmp = static_cast<SwSectionFrame*>(pTmp)->ContainsAny();

            if( pTmp ) // any content
            {
                if( pTmp->IsTextFrame() )
                {
                    SwTextFrame const*const pFrame(static_cast<SwTextFrame const*>(pTmp));
                    assert(!pFrame->GetMergedPara());
                    SwNodeOffset nNdIdx = pFrame->GetTextNodeFirst()->GetIndex();
                    if( nNdIdx > nStartOfContent )
                    {
                        /*  Open Paragraph Record */
                        aIo.OpenRec( SW_LAYCACHE_IO_REC_PARA );
                        bool bFollow = static_cast<SwTextFrame*>(pTmp)->IsFollow();
                        aIo.OpenFlagRec( bFollow ? 0x01 : 0x00,
                                        bFollow ? 8 : 4 );
                        nNdIdx -= nStartOfContent;
                        aIo.GetStream().WriteUInt32( sal_Int32(nNdIdx) );
                        if( bFollow )
                            aIo.GetStream().WriteUInt32( sal_Int32(static_cast<SwTextFrame*>(pTmp)->GetOffset()) );
                        aIo.CloseFlagRec();
                        /*  Close Paragraph Record */
                        aIo.CloseRec();
                    }
                }
                else if( pTmp->IsTabFrame() )
                {
                    SwTabFrame* pTab = static_cast<SwTabFrame*>(pTmp);
                    assert(pTab);
                    sal_uLong nOfst = COMPLETE_STRING;
                    if( pTab->IsFollow() )
                    {
                        // If the table is a follow, we have to look for the
                        // master and to count all rows to get the row number
                        nOfst = 0;
                        if( pTab->IsFollow() )
                            pTab = pTab->FindMaster( true );
                        while( pTab != pTmp )
                        {
                            SwFrame* pSub = pTab->Lower();
                            while( pSub )
                            {
                                ++nOfst;
                                pSub = pSub->GetNext();
                            }
                            pTab = pTab->GetFollow();
                            assert(pTab && "Table follow without master");
                        }
                    }
                    while (true)
                    {
                        SwNodeOffset nNdIdx =
                                pTab->GetTable()->GetTableNode()->GetIndex();
                        if( nNdIdx > nStartOfContent )
                        {
                            /* Open Table Record */
                            aIo.OpenRec( SW_LAYCACHE_IO_REC_TABLE );
                            aIo.OpenFlagRec( 0, 8 );
                            nNdIdx -= nStartOfContent;
                            aIo.GetStream().WriteUInt32( sal_Int32(nNdIdx) )
                                           .WriteUInt32( nOfst );
                            aIo.CloseFlagRec();
                            /* Close Table Record  */
                            aIo.CloseRec();
                        }
                        // If the table has a follow on the next page,
                        // we know already the row number and store this
                        // immediately.
                        if( pTab->GetFollow() )
                        {
                            if( nOfst == sal_uLong(COMPLETE_STRING) )
                                nOfst = 0;
                            do
                            {
                                SwFrame* pSub = pTab->Lower();
                                while( pSub )
                                {
                                    ++nOfst;
                                    pSub = pSub->GetNext();
                                }
                                pTab = pTab->GetFollow();
                                SwPageFrame *pTabPage = pTab->FindPageFrame();
                                if( pTabPage != pPage )
                                {
                                    OSL_ENSURE( pPage->GetPhyPageNum() <
                                            pTabPage->GetPhyPageNum(),
                                            "Looping Tableframes" );
                                    pPage = pTabPage;
                                    break;
                                }
                            } while ( pTab->GetFollow() );
                        }
                        else
                            break;
                    }
                }
            }
        }
        if( pPage->GetSortedObjs() )
        {
            SwSortedObjs &rObjs = *pPage->GetSortedObjs();
            for (SwAnchoredObject* pAnchoredObj : rObjs)
            {
                if (SwFlyFrame *pFly = pAnchoredObj->DynCastFlyFrame())
                {
                    if( pFly->getFrameArea().Left() != FAR_AWAY &&
                        !pFly->GetAnchorFrame()->FindFooterOrHeader() )
                    {
                        const SwContact *pC =
                                ::GetUserCall(pAnchoredObj->GetDrawObj());
                        if( pC )
                        {
                            sal_uInt32 nOrdNum = pAnchoredObj->GetDrawObj()->GetOrdNum();
                            sal_uInt16 nPageNum = pPage->GetPhyPageNum();
                            /* Open Fly Record */
                            aIo.OpenRec( SW_LAYCACHE_IO_REC_FLY );
                            aIo.OpenFlagRec( 0, 0 );
                            aIo.CloseFlagRec();
                            const SwRect& rRct = pFly->getFrameArea();
                            sal_Int32 nX = rRct.Left() - pPage->getFrameArea().Left();
                            sal_Int32 nY = rRct.Top() - pPage->getFrameArea().Top();
                            aIo.GetStream().WriteUInt16( nPageNum ).WriteUInt32( nOrdNum )
                                           .WriteInt32( nX ).WriteInt32( nY )
                                           .WriteInt32( rRct.Width() )
                                           .WriteInt32( rRct.Height() );
                            /* Close Fly Record  */
                            aIo.CloseRec();
                        }
                    }
                }
            }
        }
        pPage = static_cast<SwPageFrame*>(pPage->GetNext());
    }
    aIo.CloseRec();
}

#ifdef DBG_UTIL
bool SwLayoutCache::CompareLayout( const SwDoc& rDoc ) const
{
    if( !m_pImpl )
        return true;
    const SwRootFrame *pRootFrame = rDoc.getIDocumentLayoutAccess().GetCurrentLayout();
    if( pRootFrame )
    {
        size_t nIndex = 0;
        SwNodeOffset nStartOfContent = rDoc.GetNodes().GetEndOfContent().
                                StartOfSectionNode()->GetIndex();
        const SwPageFrame* pPage = static_cast<const SwPageFrame*>(pRootFrame->Lower());
        if( pPage )
            pPage = static_cast<const SwPageFrame*>(pPage->GetNext());
        while( pPage )
        {
            if( nIndex >= m_pImpl->size() )
                return false;

            const SwLayoutFrame* pLay = pPage->FindBodyCont();
            const SwFrame* pTmp = pLay ? pLay->ContainsAny() : nullptr;
            if( pTmp && pTmp->IsSctFrame() )
                pTmp = static_cast<const SwSectionFrame*>(pTmp)->ContainsAny();
            if( pTmp )
            {
                if( pTmp->IsTextFrame() )
                {

                    SwTextFrame const*const pFrame(static_cast<SwTextFrame const*>(pTmp));
                    assert(!pFrame->GetMergedPara());
                    SwNodeOffset nNdIdx = pFrame->GetTextNodeFirst()->GetIndex();
                    if( nNdIdx > nStartOfContent )
                    {
                        bool bFollow = static_cast<const SwTextFrame*>(pTmp)->IsFollow();
                        nNdIdx -= nStartOfContent;
                        if( m_pImpl->GetBreakIndex( nIndex ) != nNdIdx ||
                            SW_LAYCACHE_IO_REC_PARA !=
                            m_pImpl->GetBreakType( nIndex ) ||
                            (bFollow
                              ? sal_Int32(static_cast<const SwTextFrame*>(pTmp)->GetOffset())
                              : COMPLETE_STRING) != m_pImpl->GetBreakOfst(nIndex))
                        {
                            return false;
                        }
                        ++nIndex;
                    }
                }
                else if( pTmp->IsTabFrame() )
                {
                    const SwTabFrame* pTab = static_cast<const SwTabFrame*>(pTmp);
                    sal_Int32 nOfst = COMPLETE_STRING;
                    if( pTab->IsFollow() )
                    {
                        nOfst = 0;
                        if( pTab->IsFollow() )
                            pTab = pTab->FindMaster( true );
                        while( pTab != pTmp )
                        {
                            const SwFrame* pSub = pTab->Lower();
                            while( pSub )
                            {
                                ++nOfst;
                                pSub = pSub->GetNext();
                            }
                            pTab = pTab->GetFollow();
                        }
                    }
                    do
                    {
                        SwNodeOffset nNdIdx =
                                pTab->GetTable()->GetTableNode()->GetIndex();
                        if( nNdIdx > nStartOfContent )
                        {
                            nNdIdx -= nStartOfContent;
                            if( m_pImpl->GetBreakIndex( nIndex ) != nNdIdx ||
                                SW_LAYCACHE_IO_REC_TABLE !=
                                m_pImpl->GetBreakType( nIndex ) ||
                               nOfst != m_pImpl->GetBreakOfst( nIndex ) )
                            {
                                return false;
                            }
                            ++nIndex;
                        }
                        if( pTab->GetFollow() )
                        {
                            if( nOfst == COMPLETE_STRING )
                                nOfst = 0;
                            do
                            {
                                const SwFrame* pSub = pTab->Lower();
                                while( pSub )
                                {
                                    ++nOfst;
                                    pSub = pSub->GetNext();
                                }
                                pTab = pTab->GetFollow();
                                assert(pTab && "Table follow without master");
                                const SwPageFrame *pTabPage = pTab->FindPageFrame();
                                if( pTabPage != pPage )
                                {
                                    pPage = pTabPage;
                                    break;
                                }
                            } while ( pTab->GetFollow() );
                        }
                        else
                            break;
                    } while( pTab );
                }
            }
            pPage = static_cast<const SwPageFrame*>(pPage->GetNext());
        }
    }
    return true;
}
#endif

void SwLayoutCache::ClearImpl()
{
    if( !IsLocked() )
    {
        m_pImpl.reset();
    }
}

SwLayoutCache::~SwLayoutCache()
{
    OSL_ENSURE( !m_nLockCount, "Deleting a locked SwLayoutCache!?" );
}

/// helper class to create not nested section frames for nested sections.
SwActualSection::SwActualSection( SwActualSection *pUp,
                                  SwSectionFrame    *pSect,
                                  SwSectionNode   *pNd ) :
    m_pUpper( pUp ),
    m_pSectFrame( pSect ),
    m_pSectNode( pNd )
{
    if ( !m_pSectNode )
    {
        const SwNodeIndex *pIndex = pSect->GetFormat()->GetContent().GetContentIdx();
        m_pSectNode = pIndex->GetNode().FindSectionNode();
    }
}

namespace {

bool sanityCheckLayoutCache(SwLayCacheImpl const& rCache,
        SwNodes const& rNodes, SwNodeOffset nNodeIndex)
{
    auto const nStartOfContent(rNodes.GetEndOfContent().StartOfSectionNode()->GetIndex());
    nNodeIndex -= nStartOfContent;
    auto const nMaxIndex(rNodes.GetEndOfContent().GetIndex() - nStartOfContent);
    for (size_t nIndex = 0; nIndex < rCache.size(); ++nIndex)
    {
        auto const nBreakIndex(rCache.GetBreakIndex(nIndex));
        if (nBreakIndex < nNodeIndex || nMaxIndex <= nBreakIndex)
        {
            SAL_WARN("sw.layout",
                "invalid node index in layout-cache: " << nBreakIndex);
            return false;
        }
        auto const nBreakType(rCache.GetBreakType(nIndex));
        switch (nBreakType)
        {
            case SW_LAYCACHE_IO_REC_PARA:
                if (!rNodes[nBreakIndex + nStartOfContent]->IsTextNode())
                {
                    SAL_WARN("sw.layout",
                        "invalid node of type 'P' in layout-cache");
                    return false;
                }
                break;
            case SW_LAYCACHE_IO_REC_TABLE:
                if (!rNodes[nBreakIndex + nStartOfContent]->IsTableNode())
                {
                    SAL_WARN("sw.layout",
                        "invalid node of type 'T' in layout-cache");
                    return false;
                }
                break;
            default:
                assert(false); // Read shouldn't have inserted that
        }
    }
    return true;
}

} // namespace

/** helper class, which utilizes the layout cache information
 *  to distribute the document content to the right pages.
 * It's used by the InsertCnt_(..)-function.
 * If there's no layout cache, the distribution to the pages is more
 * a guess, but a guess with statistical background.
 */
SwLayHelper::SwLayHelper( SwDoc& rDoc, SwFrame* &rpF, SwFrame* &rpP, SwPageFrame* &rpPg,
                          SwLayoutFrame* &rpL, std::unique_ptr<SwActualSection> &rpA,
                          SwNodeOffset nNodeIndex, bool bCache )
    : mrpFrame( rpF )
    , mrpPrv( rpP )
    , mrpPage( rpPg )
    , mrpLay( rpL )
    , mrpActualSection( rpA )
    , mbBreakAfter(false)
    , mrDoc(rDoc)
    , mnFlyIdx( 0 )
    , mbFirst( bCache )
{
    mpImpl = mrDoc.GetLayoutCache() ? mrDoc.GetLayoutCache()->LockImpl() : nullptr;
    if( mpImpl )
    {
        SwNodes const& rNodes(mrDoc.GetNodes());
        if (sanityCheckLayoutCache(*mpImpl, rNodes, nNodeIndex))
        {
            mnIndex = 0;
            mnStartOfContent = rNodes.GetEndOfContent().StartOfSectionNode()->GetIndex();
        }
        else
        {
            mrDoc.GetLayoutCache()->UnlockImpl();
            mpImpl = nullptr;
            mnIndex = std::numeric_limits<size_t>::max();
            mnStartOfContent = SwNodeOffset(USHRT_MAX);
        }
    }
    else
    {
        mnIndex = std::numeric_limits<size_t>::max();
        mnStartOfContent = NODE_OFFSET_MAX;
    }
}

SwLayHelper::~SwLayHelper()
{
    if( mpImpl )
    {
        assert(mrDoc.GetLayoutCache() && "Missing layoutcache");
        mrDoc.GetLayoutCache()->UnlockImpl();
    }
}

/** Does NOT really calculate the page count,
 * it returns the page count value from the layout cache, if available,
 * otherwise it estimates the page count.
 */
sal_uLong SwLayHelper::CalcPageCount()
{
    sal_uLong nPgCount;
    SwLayCacheImpl *pCache = mrDoc.GetLayoutCache() ?
                             mrDoc.GetLayoutCache()->LockImpl() : nullptr;
    if( pCache )
    {
        nPgCount = pCache->size() + 1;
        mrDoc.GetLayoutCache()->UnlockImpl();
    }
    else
    {
        nPgCount = mrDoc.getIDocumentStatistics().GetDocStat().nPage;
        if ( nPgCount <= 10 ) // no page insertion for less than 10 pages
            nPgCount = 0;
        sal_Int32 nNdCount = mrDoc.getIDocumentStatistics().GetDocStat().nPara;
        if ( nNdCount <= 1 )
        {
            //Estimates the number of paragraphs.
            SwNodeOffset nTmp = mrDoc.GetNodes().GetEndOfContent().GetIndex() -
                        mrDoc.GetNodes().GetEndOfExtras().GetIndex();
            //Tables have a little overhead...
            nTmp -= SwNodeOffset(mrDoc.GetTableFrameFormats()->size() * 25);
            //Fly frames, too ..
            nTmp -= (mrDoc.GetNodes().GetEndOfAutotext().GetIndex() -
                       mrDoc.GetNodes().GetEndOfInserts().GetIndex()) / SwNodeOffset(3 * 5);
            if ( nTmp > SwNodeOffset(0) )
                nNdCount = sal_Int32(nTmp);
        }
        if ( nNdCount < 1000 )
            nPgCount = 0;// no progress bar for small documents
    }
    return nPgCount;
}

/**
 * inserts a page and return true, if
 * - the break after flag is set
 * - the actual content wants a break before
 * - the maximum count of paragraph/rows is reached
 *
 * The break after flag is set, if the actual content
 * wants a break after.
 */
bool SwLayHelper::CheckInsertPage(
    SwPageFrame *& rpPage,
    SwLayoutFrame *& rpLay,
    SwFrame *& rpFrame,
    bool & rIsBreakAfter)
{
    bool bEnd = nullptr == rpPage->GetNext();
    const SvxFormatBreakItem& rBrk = rpFrame->GetBreakItem();
    const SwFormatPageDesc& rDesc = rpFrame->GetPageDescItem();
    // #118195# Do not evaluate page description if frame
    // is a follow frame!
    const SwPageDesc* pDesc = rpFrame->IsFlowFrame()
                            && SwFlowFrame::CastFlowFrame(rpFrame)->IsFollow()
                          ? nullptr
                          : rDesc.GetPageDesc();

    bool bBrk = rIsBreakAfter;
    rIsBreakAfter = rBrk.GetBreak() == SvxBreak::PageAfter ||
                   rBrk.GetBreak() == SvxBreak::PageBoth;
    if ( !bBrk )
        bBrk = rBrk.GetBreak() == SvxBreak::PageBefore ||
               rBrk.GetBreak() == SvxBreak::PageBoth;

    if ( bBrk || pDesc )
    {
        ::std::optional<sal_uInt16> oPgNum;
        if ( !pDesc )
        {
            pDesc = rpPage->GetPageDesc()->GetFollow();
        }
        else
        {
            oPgNum = rDesc.GetNumOffset();
            if ( oPgNum )
                static_cast<SwRootFrame*>(rpPage->GetUpper())->SetVirtPageNum(true);
        }
        bool bNextPageRight = !rpPage->OnRightPage();
        bool bInsertEmpty = false;
        assert(rpPage->GetUpper()->GetLower());
        if (oPgNum && bNextPageRight != IsRightPageByNumber(
                    *static_cast<SwRootFrame*>(rpPage->GetUpper()), *oPgNum))
        {
            bNextPageRight = !bNextPageRight;
            bInsertEmpty = true;
        }
        // If the page style is changing, we'll have a first page.
        bool bNextPageFirst = pDesc != rpPage->GetPageDesc();
        ::InsertNewPage( const_cast<SwPageDesc&>(*pDesc), rpPage->GetUpper(),
             bNextPageRight, bNextPageFirst, bInsertEmpty, false, rpPage->GetNext());
        if ( bEnd )
        {
            OSL_ENSURE( rpPage->GetNext(), "No new page?" );
            do
            {
                rpPage = static_cast<SwPageFrame*>(rpPage->GetNext());
            } while (rpPage->GetNext());
        }
        else
        {
            OSL_ENSURE( rpPage->GetNext(), "No new page?" );
            rpPage = static_cast<SwPageFrame*>(rpPage->GetNext());
            if (rpPage->IsEmptyPage())
            {
                OSL_ENSURE( rpPage->GetNext(), "No new page?" );
                rpPage = static_cast<SwPageFrame*>(rpPage->GetNext());
            }
        }
        rpLay = rpPage->FindBodyCont();
        while (rpLay->Lower())
            rpLay = static_cast<SwLayoutFrame*>(rpLay->Lower());
        return true;
    }
    return false;
}

/** entry point for the InsertCnt_-function.
 *  The document content index is checked either it is
 *  in the layout cache either it's time to insert a page
 *  cause the maximal estimation of content per page is reached.
 *  A really big table or long paragraph may contains more than
 *  one page, in this case the needed count of pages will inserted.
 */
bool SwLayHelper::CheckInsert( SwNodeOffset nNodeIndex )
{
    bool bRet = false;
    nNodeIndex -= mnStartOfContent;
    sal_uInt16 nRows( 0 );
    if( mrpFrame->IsTabFrame() )
    {
        SwFrame *pLow = static_cast<SwTabFrame*>(mrpFrame)->Lower();
        nRows = 0;
        do
        {
            ++nRows;
            pLow = pLow->GetNext();
        } while ( pLow );
    }
    if( mbFirst && mpImpl && mnIndex < mpImpl->size() &&
        mpImpl->GetBreakIndex( mnIndex ) == nNodeIndex &&
        ( mpImpl->GetBreakOfst( mnIndex ) < COMPLETE_STRING ||
          ( ++mnIndex < mpImpl->size() &&
          mpImpl->GetBreakIndex( mnIndex ) == nNodeIndex ) ) )
        mbFirst = false;
    if (!mbFirst)
    {
        sal_Int32 nRowCount = 0;
        do
        {
            if (mpImpl)
            {
                sal_Int32 nOfst = COMPLETE_STRING;
                sal_uInt16 nType = SW_LAYCACHE_IO_REC_PAGES;
                while( mnIndex < mpImpl->size() &&
                       mpImpl->GetBreakIndex(mnIndex) < nNodeIndex)
                    ++mnIndex;
                if( mnIndex < mpImpl->size() &&
                    mpImpl->GetBreakIndex(mnIndex) == nNodeIndex )
                {
                    nType = mpImpl->GetBreakType( mnIndex );
                    nOfst = mpImpl->GetBreakOfst( mnIndex++ );
                    mbBreakAfter = true;
                }

                if( nOfst < COMPLETE_STRING )
                {
                    bool bSplit = false;
                    sal_uInt16 nRepeat( 0 );
                    if( mrpFrame->IsTextFrame() &&
                        SW_LAYCACHE_IO_REC_PARA == nType &&
                        nOfst < static_cast<SwTextFrame*>(mrpFrame)->GetText().getLength())
                        bSplit = true;
                    else if( mrpFrame->IsTabFrame() && nRowCount < nOfst &&
                             ( SW_LAYCACHE_IO_REC_TABLE == nType ) )
                    {
                        nRepeat = static_cast<SwTabFrame*>(mrpFrame)->
                                  GetTable()->GetRowsToRepeat();
                        bSplit = nOfst < nRows && nRowCount + nRepeat < nOfst;
                    }
                    if( bSplit )
                    {
                        mrpFrame->InsertBehind( mrpLay, mrpPrv );

                        {
                            SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*mrpFrame);
                            aFrm.Pos() = mrpLay->getFrameArea().Pos();
                            aFrm.Pos().AdjustY(1 );
                        }

                        mrpPrv = mrpFrame;
                        if( mrpFrame->IsTabFrame() )
                        {
                            SwTabFrame* pTab = static_cast<SwTabFrame*>(mrpFrame);
                            // #i33629#, #i29955#
                            ::RegistFlys( pTab->FindPageFrame(), pTab );
                            SwFrame *pRow = pTab->Lower();
                            SwTabFrame *pFoll = new SwTabFrame( *pTab );

                            SwFrame *pPrv;
                            if( nRepeat > 0 )
                            {
                                sw::FlyCreationSuppressor aSuppressor;
                                // Insert new headlines:
                                sal_uInt16 nRowIdx = 0;
                                SwRowFrame* pHeadline = nullptr;
                                while( nRowIdx < nRepeat )
                                {
                                    OSL_ENSURE( pTab->GetTable()->GetTabLines()[ nRowIdx ], "Table without rows?" );
                                    pHeadline =
                                        new SwRowFrame( *pTab->GetTable()->GetTabLines()[ nRowIdx ], pTab );
                                    pHeadline->SetRepeatedHeadline( true );
                                    pHeadline->InsertBefore( pFoll, nullptr );
                                    pHeadline->RegistFlys();

                                    ++nRowIdx;
                                }
                                pPrv = pHeadline;
                                nRows = nRows + nRepeat;
                            }
                            else
                                pPrv = nullptr;
                            while( pRow && nRowCount < nOfst )
                            {
                                pRow = pRow->GetNext();
                                ++nRowCount;
                            }
                            while ( pRow )
                            {
                                SwFrame* pNxt = pRow->GetNext();
                                pRow->RemoveFromLayout();
                                pRow->InsertBehind( pFoll, pPrv );
                                pPrv = pRow;
                                pRow = pNxt;
                            }
                            mrpFrame = pFoll;
                        }
                        else
                        {
                            SwTextFrame *const pNew = static_cast<SwTextFrame*>(
                                static_cast<SwTextFrame*>(mrpFrame)
                                    ->GetTextNodeFirst()->MakeFrame(mrpFrame));
                            pNew->ManipOfst( TextFrameIndex(nOfst) );
                            pNew->SetFollow( static_cast<SwTextFrame*>(mrpFrame)->GetFollow() );
                            static_cast<SwTextFrame*>(mrpFrame)->SetFollow( pNew );
                            mrpFrame = pNew;
                        }
                    }
                }
            }

            SwPageFrame* pLastPage = mrpPage;
            if (CheckInsertPage(mrpPage, mrpLay, mrpFrame, mbBreakAfter))
            {
                CheckFlyCache_( pLastPage );
                if( mrpPrv && mrpPrv->IsTextFrame() && !mrpPrv->isFrameAreaSizeValid() )
                {
                    SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*mrpPrv);
                    aFrm.Height( mrpPrv->GetUpper()->getFramePrintArea().Height() );
                }

                bRet = true;
                mrpPrv = nullptr;

                if ( mrpActualSection )
                {
                    //Did the SectionFrame even have a content? If not, we can
                    //directly put it somewhere else
                    SwSectionFrame *pSct;
                    bool bInit = false;
                    if ( !mrpActualSection->GetSectionFrame()->ContainsContent())
                    {
                        pSct = mrpActualSection->GetSectionFrame();
                        pSct->RemoveFromLayout();
                    }
                    else
                    {
                        pSct = new SwSectionFrame(
                            *mrpActualSection->GetSectionFrame(), false );
                        mrpActualSection->GetSectionFrame()->SimpleFormat();
                        bInit = true;
                    }
                    mrpActualSection->SetSectionFrame( pSct );
                    pSct->InsertBehind( mrpLay, nullptr );

                    if( bInit )
                    {
                        pSct->Init();
                    }

                    {
                        SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*pSct);
                        aFrm.Pos() = mrpLay->getFrameArea().Pos();
                        aFrm.Pos().AdjustY(1 ); //because of the notifications
                    }

                    mrpLay = pSct;
                    SwFrame* pLower = mrpLay->Lower();
                    if ( pLower && pLower->IsLayoutFrame() )
                        mrpLay = mrpLay->GetNextLayoutLeaf();
                }
            }
        } while( mpImpl && mnIndex < mpImpl->size() &&
                 mpImpl->GetBreakIndex( mnIndex ) == nNodeIndex );
    }
    mbFirst = false;
    return bRet;
}

namespace {

struct SdrObjectCompare
{
  bool operator()( const SdrObject* pF1, const SdrObject* pF2 ) const
  {
    return pF1->GetOrdNum() < pF2->GetOrdNum();
  }
};

struct FlyCacheCompare
{
  bool operator()( const SwFlyCache* pC1, const SwFlyCache* pC2 ) const
  {
    return pC1->nOrdNum < pC2->nOrdNum;
  }
};

}

/**
 * If a new page is inserted, the last page is analysed.
 * If there are text frames with default position, the fly cache
 * is checked, if these frames are stored in the cache.
 */
void SwLayHelper::CheckFlyCache_( SwPageFrame* pPage )
{
    if( !mpImpl || !pPage )
        return;
    const size_t nFlyCount = mpImpl->GetFlyCount();
    // Any text frames at the page, fly cache available?
    if( !(pPage->GetSortedObjs() && mnFlyIdx < nFlyCount) )
        return;

    SwSortedObjs &rObjs = *pPage->GetSortedObjs();
    sal_uInt16 nPgNum = pPage->GetPhyPageNum();

    // NOTE: Here we do not use the absolute ordnums but
    // relative ordnums for the objects on this page.

    // skip fly frames from pages before the current page
    while( mnFlyIdx < nFlyCount &&
           mpImpl->GetFlyCache(mnFlyIdx).nPageNum < nPgNum )
        ++mnFlyIdx;

    // sort cached objects on this page by ordnum
    o3tl::sorted_vector< const SwFlyCache*, FlyCacheCompare > aFlyCacheSet;
    size_t nIdx = mnFlyIdx;

    SwFlyCache* pFlyC;
    while( nIdx < nFlyCount &&
           ( pFlyC = &mpImpl->GetFlyCache( nIdx ) )->nPageNum == nPgNum )
    {
        aFlyCacheSet.insert( pFlyC );
        ++nIdx;
    }

    // sort objects on this page by ordnum
    o3tl::sorted_vector< const SdrObject*, SdrObjectCompare > aFlySet;
    for (SwAnchoredObject* pAnchoredObj : rObjs)
    {
        if (SwFlyFrame *pFly = pAnchoredObj->DynCastFlyFrame())  // a text frame?
        {
            if( pFly->GetAnchorFrame() &&
                !pFly->GetAnchorFrame()->FindFooterOrHeader() )
            {
                const SwContact *pC = ::GetUserCall( pAnchoredObj->GetDrawObj() );
                if( pC )
                {
                    aFlySet.insert( pAnchoredObj->GetDrawObj() );
                }
            }
        }
    }

    if ( aFlyCacheSet.size() != aFlySet.size() )
        return;

    auto aFlySetIt = aFlySet.begin();

    for ( const SwFlyCache* pFlyCache : aFlyCacheSet )
    {
        SwFlyFrame* pFly = const_cast<SwVirtFlyDrawObj*>(static_cast<const SwVirtFlyDrawObj*>(*aFlySetIt))->GetFlyFrame();

        if ( pFly->getFrameArea().Left() == FAR_AWAY )
        {
            // we get the stored information
            SwFrameAreaDefinition::FrameAreaWriteAccess aFrm(*pFly);
            aFrm.Pos().setX( pFlyCache->Left() + pPage->getFrameArea().Left() );
            aFrm.Pos().setY( pFlyCache->Top() + pPage->getFrameArea().Top() );

            if ( mpImpl->IsUseFlyCache() )
            {
                aFrm.Width( pFlyCache->Width() );
                aFrm.Height( pFlyCache->Height() );
            }
        }

        ++aFlySetIt;
    }
}

SwLayCacheIoImpl::SwLayCacheIoImpl( SvStream& rStrm, bool bWrtMd ) :
    m_pStream( &rStrm ),
    m_nFlagRecEnd ( 0 ),
    m_nMajorVersion(SW_LAYCACHE_IO_VERSION_MAJOR),
    m_nMinorVersion(SW_LAYCACHE_IO_VERSION_MINOR),
    m_bWriteMode( bWrtMd ),
    m_bError( false  )
{
    if( m_bWriteMode )
        m_pStream->WriteUInt16( m_nMajorVersion )
                .WriteUInt16( m_nMinorVersion );

    else
        m_pStream->ReadUInt16( m_nMajorVersion )
                .ReadUInt16( m_nMinorVersion );
}

void SwLayCacheIoImpl::OpenRec( sal_uInt8 cType )
{
    sal_uInt64 nPos = m_pStream->Tell();
    if( m_bWriteMode )
    {
        m_aRecords.emplace_back(cType, nPos );
        m_pStream->WriteUInt32( 0 );
    }
    else
    {
        sal_uInt32 nVal(0);
        m_pStream->ReadUInt32( nVal );
        sal_uInt8 cRecTyp = static_cast<sal_uInt8>(nVal);
        if (!nVal || cRecTyp != cType || !m_pStream->good())
        {
            OSL_ENSURE( nVal, "OpenRec: Record-Header is 0" );
            OSL_ENSURE( cRecTyp == cType, "OpenRec: Wrong Record Type" );
            m_aRecords.emplace_back(0, m_pStream->Tell() );
            m_bError = true;
        }
        else
        {
            sal_uInt32 nSize = nVal >> 8;
            m_aRecords.emplace_back(cRecTyp, nPos+nSize );
        }
    }
}

// Close record
void SwLayCacheIoImpl::CloseRec()
{
    bool bRes = true;
    OSL_ENSURE( !m_aRecords.empty(), "CloseRec: no levels" );
    if( !m_aRecords.empty() )
    {
        sal_uInt64 nPos = m_pStream->Tell();
        if( m_bWriteMode )
        {
            sal_uInt32 nBgn = m_aRecords.back().size;
            m_pStream->Seek( nBgn );
            sal_uInt32 nSize = nPos - nBgn;
            sal_uInt32 nVal = ( nSize << 8 ) | m_aRecords.back().type;
            m_pStream->WriteUInt32( nVal );
            m_pStream->Seek( nPos );
            if( m_pStream->GetError() != ERRCODE_NONE )
                 bRes = false;
        }
        else
        {
            sal_uInt32 n = m_aRecords.back().size;
            OSL_ENSURE( n >= nPos, "CloseRec: too much data read" );
            if( n != nPos )
            {
                m_pStream->Seek( n );
                if( n < nPos )
                     bRes = false;
            }
            if( m_pStream->GetErrorCode() != ERRCODE_NONE )
                bRes = false;
        }
        m_aRecords.pop_back();
    }

    if( !bRes )
        m_bError = true;
}

sal_uInt32 SwLayCacheIoImpl::BytesLeft()
{
    sal_uInt32 n = 0;
    if( !m_bError && !m_aRecords.empty() )
    {
        sal_uInt32 nEndPos = m_aRecords.back().size;
        sal_uInt64 nPos = m_pStream->Tell();
        if( nEndPos > nPos )
            n = nEndPos - nPos;
    }
    return n;
}

sal_uInt8 SwLayCacheIoImpl::Peek()
{
    sal_uInt8 c(0);
    if( !m_bError )
    {
        sal_uInt64 nPos = m_pStream->Tell();
        m_pStream->ReadUChar( c );
        m_pStream->Seek( nPos );
        if( m_pStream->GetErrorCode() != ERRCODE_NONE )
        {
            c = 0;
            m_bError = true;
        }
    }
    return c;
}

void SwLayCacheIoImpl::SkipRec()
{
    sal_uInt8 c = Peek();
    OpenRec( c );
    m_pStream->Seek( m_aRecords.back().size );
    CloseRec();
}

sal_uInt8 SwLayCacheIoImpl::OpenFlagRec()
{
    OSL_ENSURE( !m_bWriteMode, "OpenFlagRec illegal in write  mode" );
    sal_uInt8 cFlags(0);
    m_pStream->ReadUChar( cFlags );
    m_nFlagRecEnd = m_pStream->Tell() + ( cFlags & 0x0F );
    return (cFlags >> 4);
}

void SwLayCacheIoImpl::OpenFlagRec( sal_uInt8 nFlags, sal_uInt8 nLen )
{
    OSL_ENSURE( m_bWriteMode, "OpenFlagRec illegal in read  mode" );
    OSL_ENSURE( (nFlags & 0xF0) == 0, "illegal flags set" );
    OSL_ENSURE( nLen < 16, "wrong flag record length" );
    sal_uInt8 cFlags = (nFlags << 4) + nLen;
    m_pStream->WriteUChar( cFlags );
    m_nFlagRecEnd = m_pStream->Tell() + nLen;
}

void SwLayCacheIoImpl::CloseFlagRec()
{
    if( m_bWriteMode )
    {
        OSL_ENSURE( m_pStream->Tell() == m_nFlagRecEnd, "Wrong amount of data written" );
    }
    else
    {
        OSL_ENSURE( m_pStream->Tell() <= m_nFlagRecEnd, "Too many data read" );
        if( m_pStream->Tell() != m_nFlagRecEnd )
            m_pStream->Seek( m_nFlagRecEnd );
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
