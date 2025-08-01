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

#include <com/sun/star/embed/Aspects.hpp>
#include <com/sun/star/embed/XEmbeddedObject.hpp>

#include <math.h>
#include <limits>
#include <limits.h>
#include <utility>
#include <vector>

#include <o3tl/any.hxx>
#include <o3tl/safeint.hxx>
#include <osl/file.hxx>
#include <tools/solar.h>
#include <sal/log.hxx>
#include <rtl/math.hxx>

#include <comphelper/classids.hxx>
#include <toolkit/helper/vclunohelper.hxx>
#include <comphelper/configuration.hxx>
#include <unotools/streamwrap.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/string.hxx>
#include <comphelper/seqstream.hxx>
#include <comphelper/storagehelper.hxx>
#include <comphelper/sequence.hxx>
#include <sot/exchange.hxx>
#include <sot/storinfo.hxx>
#include <vcl/cvtgrf.hxx>
#include <vcl/wmf.hxx>
#include <vcl/settings.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/BitmapTools.hxx>
#include "viscache.hxx"

// SvxItem-Mapping. Is needed to successfully include the SvxItem-Header
#include <editeng/eeitem.hxx>
#include <editeng/editdata.hxx>
#include <tools/bigint.hxx>
#include <tools/debug.hxx>
#include <tools/stream.hxx>
#include <tools/zcodec.hxx>
#include <filter/msfilter/escherex.hxx>
#include <basegfx/numeric/ftools.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <com/sun/star/drawing/Position3D.hpp>
#include <com/sun/star/drawing/Direction3D.hpp>
#include <editeng/charscaleitem.hxx>
#include <editeng/kernitem.hxx>
#include <vcl/graphicfilter.hxx>
#include <tools/urlobj.hxx>
#include <vcl/virdev.hxx>
#include <vcl/BitmapReadAccess.hxx>
#include <sot/storage.hxx>
#include <sfx2/docfilt.hxx>
#include <sfx2/fcontnr.hxx>
#include <svx/xbtmpit.hxx>
#include <svx/xsflclit.hxx>
#include <svx/xfilluseslidebackgrounditem.hxx>
#include <svx/xflgrit.hxx>
#include <svx/xflftrit.hxx>
#include <svx/sdgcpitm.hxx>
#include <svx/sdgmoitm.hxx>
#include <svx/svdmodel.hxx>
#include <svx/svdobj.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdogrp.hxx>
#include <svx/svdograf.hxx>
#include <svx/svdotext.hxx>
#include <svx/svdorect.hxx>
#include <svx/svdoedge.hxx>
#include <svx/svdoutl.hxx>
#include <svx/svdoole2.hxx>
#include <svx/svdopath.hxx>
#include <svx/xlntrit.hxx>
#include <svx/xfillit0.hxx>
#include <svx/xflbmtit.hxx>
#include <svx/xflclit.hxx>
#include <svx/xfltrit.hxx>
#include <svx/xflbmsxy.hxx>
#include <svx/xflbmsli.hxx>
#include <editeng/frmdir.hxx>
#include <editeng/frmdiritem.hxx>
#include <svx/svdtrans.hxx>
#include <svx/sxenditm.hxx>
#include <svx/sdgluitm.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/wghtitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/crossedoutitem.hxx>
#include <editeng/shdditem.hxx>
#include <editeng/fontitem.hxx>
#include <svx/sxekitm.hxx>
#include <svx/xpoly.hxx>
#include <svx/xlineit0.hxx>
#include <svx/xlncapit.hxx>
#include <svx/xlinjoit.hxx>
#include <svx/xlndsit.hxx>
#include <svx/xlnclit.hxx>
#include <svx/xlnwtit.hxx>
#include <svx/xlnstwit.hxx>
#include <svx/xlnedwit.hxx>
#include <svx/xlnstit.hxx>
#include <svx/xlnedit.hxx>
#include <svx/xlnstcit.hxx>
#include <svx/xlnedcit.hxx>
#include <svx/sdasitm.hxx>
#include <svx/sdggaitm.hxx>
#include <svx/sdshcitm.hxx>
#include <svx/sdshitm.hxx>
#include <svx/sdshtitm.hxx>
#include <svx/sdsxyitm.hxx>
#include <svx/sdtagitm.hxx>
#include <svx/sdtcfitm.hxx>
#include <svx/sdtditm.hxx>
#include <svx/sdtfsitm.hxx>
#include <svx/sdtmfitm.hxx>
#include <filter/msfilter/classids.hxx>
#include <filter/msfilter/msdffimp.hxx>
#include <editeng/outliner.hxx>
#include <editeng/outlobj.hxx>
#include <com/sun/star/drawing/ShadeMode.hpp>
#include <vcl/dibtools.hxx>
#include <vcl/svapp.hxx>
#include <svx/svdoashp.hxx>
#include <svx/EnhancedCustomShapeTypeNames.hxx>
#include <svx/EnhancedCustomShapeGeometry.hxx>
#include <com/sun/star/drawing/EnhancedCustomShapeParameterPair.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeParameterType.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeSegment.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeGluePointType.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeSegmentCommand.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeTextFrame.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeAdjustmentValue.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeTextPathMode.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeMetalType.hpp>
#include <com/sun/star/beans/PropertyValues.hpp>
#include <com/sun/star/beans/XPropertySetInfo.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/drawing/ProjectionMode.hpp>
#include <svx/EnhancedCustomShape2d.hxx>
#include <rtl/ustring.hxx>
#include <svtools/embedhlp.hxx>
#include <memory>

using namespace ::com::sun::star    ;
using namespace ::com::sun::star::drawing;
using namespace uno                 ;
using namespace beans               ;
using namespace drawing             ;
using namespace container           ;

// static counter for OLE-Objects
static sal_uInt32 nMSOleObjCntr = 0;
constexpr OUString MSO_OLE_Obj = u"MSO_OLE_Obj"_ustr;

namespace {
/* Office File Formats - 2.2.23  */
enum class OfficeArtBlipRecInstance : sal_uInt32
{
    EMF = 0x3D4, // defined in section 2.2.24.
    WMF = 0x216, // defined in section 2.2.25.
    PICT = 0x542, // as defined in section 2.2.26.
    JPEG_RGB = 0x46A, // defined in section 2.2.27.
    JPEG_CMYK = 0x6E2, // defined in section 2.2.27.
    PNG = 0x6E0, // defined in section 2.2.28.
    DIB = 0x7A8, // defined in section 2.2.29.
    TIFF = 0x6E4 // defined in section 2.2.30.
};

}

/************************************************************************/
void Impl_OlePres::Write( SvStream & rStm )
{
    WriteClipboardFormat( rStm, SotClipboardFormatId::GDIMETAFILE );
    rStm.WriteInt32( 4 );       // a TargetDevice that's always empty
    rStm.WriteUInt32( nAspect );
    rStm.WriteInt32( -1 );      //L-Index always -1
    rStm.WriteInt32( nAdvFlags );
    rStm.WriteInt32( 0 );       //Compression
    rStm.WriteInt32( aSize.Width() );
    rStm.WriteInt32( aSize.Height() );
    sal_uInt64 nPos = rStm.Tell();
    rStm.WriteInt32( 0 );

    if( nFormat == SotClipboardFormatId::GDIMETAFILE && pMtf )
    {
        // Always to 1/100 mm, until Mtf-Solution found
        // Assumption (no scaling, no origin translation)
        DBG_ASSERT( pMtf->GetPrefMapMode().GetScaleX() == Fraction( 1, 1 ),
                    "x-scale in the Mtf is wrong" );
        DBG_ASSERT( pMtf->GetPrefMapMode().GetScaleY() == Fraction( 1, 1 ),
                    "y-scale in the Mtf is wrong" );
        DBG_ASSERT( pMtf->GetPrefMapMode().GetOrigin() == Point(),
                    "origin-shift in the Mtf is wrong" );
        MapUnit nMU = pMtf->GetPrefMapMode().GetMapUnit();
        if( MapUnit::Map100thMM != nMU )
        {
            Size aPrefS( pMtf->GetPrefSize() );
            Size aS = OutputDevice::LogicToLogic(aPrefS, MapMode(nMU), MapMode(MapUnit::Map100thMM));

            pMtf->Scale( Fraction( aS.Width(), aPrefS.Width() ),
                         Fraction( aS.Height(), aPrefS.Height() ) );
            pMtf->SetPrefMapMode(MapMode(MapUnit::Map100thMM));
            pMtf->SetPrefSize( aS );
        }
        WriteWindowMetafileBits( rStm, *pMtf );
    }
    else
    {
        OSL_FAIL( "unknown format" );
    }
    sal_uInt64 nEndPos = rStm.Tell();
    rStm.Seek( nPos );
    rStm.WriteUInt32( nEndPos - nPos - 4 );
    rStm.Seek( nEndPos );
}

DffPropertyReader::DffPropertyReader( const SvxMSDffManager& rMan )
    : rManager(rMan)
    , mnFix16Angle(0)
    , mbRotateGranientFillWithAngle(false)
{
    InitializePropSet( DFF_msofbtOPT );
}

void DffPropertyReader::SetDefaultPropSet( SvStream& rStCtrl, sal_uInt32 nOffsDgg ) const
{
    const_cast<DffPropertyReader*>(this)->pDefaultPropSet.reset();
    sal_uInt64 nOldPos = rStCtrl.Tell();
    bool bOk = checkSeek(rStCtrl, nOffsDgg);
    DffRecordHeader aRecHd;
    if (bOk)
        bOk = ReadDffRecordHeader( rStCtrl, aRecHd );
    if (bOk && aRecHd.nRecType == DFF_msofbtDggContainer)
    {
        if ( SvxMSDffManager::SeekToRec( rStCtrl, DFF_msofbtOPT, aRecHd.GetRecEndFilePos() ) )
        {
            const_cast<DffPropertyReader*>(this)->pDefaultPropSet.reset( new DffPropSet );
            ReadDffPropSet( rStCtrl, *pDefaultPropSet );
        }
    }
    rStCtrl.Seek( nOldPos );
}

#ifdef DBG_CUSTOMSHAPE
void DffPropertyReader::ReadPropSet( SvStream& rIn, SvxMSDffClientData* pClientData, sal_uInt32 nShapeId ) const
#else
void DffPropertyReader::ReadPropSet( SvStream& rIn, SvxMSDffClientData* pClientData ) const
#endif
{
    sal_uInt64 nFilePos = rIn.Tell();
    ReadDffPropSet( rIn, const_cast<DffPropertyReader&>(*this) );

    if ( IsProperty( DFF_Prop_hspMaster ) )
    {
        if ( rManager.SeekToShape( rIn, pClientData, GetPropertyValue( DFF_Prop_hspMaster, 0 ) ) )
        {
            DffRecordHeader aRecHd;
            bool bOk = ReadDffRecordHeader(rIn, aRecHd);
            if (bOk && SvxMSDffManager::SeekToRec(rIn, DFF_msofbtOPT, aRecHd.GetRecEndFilePos()))
            {
                rIn |= const_cast<DffPropertyReader&>(*this);
            }
        }
    }

    const_cast<DffPropertyReader*>(this)->mnFix16Angle = Fix16ToAngle( GetPropertyValue( DFF_Prop_Rotation, 0 ) );

#ifdef DBG_CUSTOMSHAPE

    OUString aURLStr;

    if( osl::FileBase::getFileURLFromSystemPath( OUString("d:\\ashape.dbg"), aURLStr ) == osl::FileBase::E_None )
    {
        std::unique_ptr<SvStream> xOut(::utl::UcbStreamHelper::CreateStream( aURLStr, StreamMode::WRITE ));

        if( xOut )
        {
            xOut->Seek( STREAM_SEEK_TO_END );

            if ( IsProperty( DFF_Prop_adjustValue ) || IsProperty( DFF_Prop_pVertices ) )
            {
                xOut->WriteLine( "" );
                OString aString("ShapeId: " + OString::number(nShapeId));
                xOut->WriteLine(aString);
            }
            for ( sal_uInt32 i = DFF_Prop_adjustValue; i <= DFF_Prop_adjust10Value; i++ )
            {
                if ( IsProperty( i ) )
                {
                    OString aString("Prop_adjustValue" + OString::number( ( i - DFF_Prop_adjustValue ) + 1 ) +
                                    ":" + OString::number(GetPropertyValue(i)) );
                    xOut->WriteLine(aString);
                }
            }
            sal_Int32 i;
            for ( i = 320; i < 383; i++ )
            {
                if ( ( i >= DFF_Prop_adjustValue ) && ( i <= DFF_Prop_adjust10Value ) )
                    continue;
                if ( IsProperty( i ) )
                {
                    if ( SeekToContent( i, rIn ) )
                    {
                        sal_Int32 nLen = (sal_Int32)GetPropertyValue( i );
                        if ( nLen )
                        {
                            xOut->WriteLine( "" );
                            OStringBuffer aDesc("Property:" + OString::number(i) +
                                                "  Size:" + OString::number(nLen));
                            xOut->WriteLine(aDesc.makeStringAndClear());
                            sal_Int16   nNumElem, nNumElemMem, nNumSize;
                            rIn >> nNumElem >> nNumElemMem >> nNumSize;
                            aDesc.append("Entries: " + OString::number(nNumElem) +
                                         "  Size:" + OString::number(nNumSize));
                            xOut->WriteLine(aDesc.makeStringAndClear());
                            if ( nNumSize < 0 )
                                nNumSize = ( ( -nNumSize ) >> 2 );
                            if ( !nNumSize )
                                nNumSize = 16;
                            nLen -= 6;
                            while ( nLen > 0 )
                            {
                                for ( sal_uInt32 j = 0; nLen && ( j < ( nNumSize >> 1 ) ); j++ )
                                {
                                    for ( sal_uInt32 k = 0; k < 2; k++ )
                                    {
                                        if ( nLen )
                                        {
                                            sal_uInt8 nVal;
                                            rIn >> nVal;
                                            if ( ( nVal >> 4 ) > 9 )
                                                *xOut << (sal_uInt8)( ( nVal >> 4 ) + 'A' - 10 );
                                            else
                                                *xOut << (sal_uInt8)( ( nVal >> 4 ) + '0' );

                                            if ( ( nVal & 0xf ) > 9 )
                                                *xOut << (sal_uInt8)( ( nVal & 0xf ) + 'A' - 10 );
                                            else
                                                *xOut << (sal_uInt8)( ( nVal & 0xf ) + '0' );

                                            nLen--;
                                        }
                                    }
                                    *xOut << (char)( ' ' );
                                }
                                xOut->WriteLine( OString() );
                            }
                        }
                    }
                    else
                    {
                        OString aString("Property" + OString::number(i) +
                                        ":" + OString::number(GetPropertyValue(i)));
                        xOut->WriteLine(aString);
                    }
                }
            }
        }
    }

#endif

    rIn.Seek( nFilePos );
}


Degree100 DffPropertyReader::Fix16ToAngle( sal_Int32 nContent )
{
    Degree100 nAngle(0);
    if ( nContent )
    {
        nAngle = Degree100(( static_cast<sal_Int16>( nContent >> 16) * 100L ) + ( ( ( nContent & 0x0000ffff) * 100L ) >> 16 ));
        nAngle = NormAngle36000( -nAngle );
    }
    return nAngle;
}

DffPropertyReader::~DffPropertyReader()
{
}

static SvStream& operator>>( SvStream& rIn, SvxMSDffConnectorRule& rRule )
{
    sal_uInt32 nRuleId;
    rIn.ReadUInt32( nRuleId )
       .ReadUInt32( rRule.nShapeA )
       .ReadUInt32( rRule.nShapeB )
       .ReadUInt32( rRule.nShapeC )
       .ReadUInt32( rRule.ncptiA )
       .ReadUInt32( rRule.ncptiB );

    return rIn;
}

SvxMSDffSolverContainer::SvxMSDffSolverContainer()
{
}

SvxMSDffSolverContainer::~SvxMSDffSolverContainer()
{
}

SvStream& ReadSvxMSDffSolverContainer( SvStream& rIn, SvxMSDffSolverContainer& rContainer )
{
    DffRecordHeader aHd;
    bool bOk = ReadDffRecordHeader( rIn, aHd );
    if (!bOk || aHd.nRecType != DFF_msofbtSolverContainer)
        return rIn;

    DffRecordHeader aCRule;
    auto nEndPos = DffPropSet::SanitizeEndPos(rIn, aHd.GetRecEndFilePos());
    while ( rIn.good() && ( rIn.Tell() < nEndPos ) )
    {
        if (!ReadDffRecordHeader(rIn, aCRule))
            break;
        if ( aCRule.nRecType == DFF_msofbtConnectorRule )
        {
            std::unique_ptr<SvxMSDffConnectorRule> pRule(new SvxMSDffConnectorRule);
            rIn >> *pRule;
            rContainer.aCList.push_back( std::move(pRule) );
        }
        if (!aCRule.SeekToEndOfRecord(rIn))
            break;
    }
    return rIn;
}

void SvxMSDffManager::SolveSolver( const SvxMSDffSolverContainer& rSolver )
{
    size_t i, nCnt;
    for ( i = 0, nCnt = rSolver.aCList.size(); i < nCnt; i++ )
    {
        SvxMSDffConnectorRule* pPtr = rSolver.aCList[ i ].get();
        if ( pPtr->pCObj )
        {
            for ( int nN = 0; nN < 2; nN++ )
            {
                SdrObject*  pO;
                sal_uInt32  nC;
                ShapeFlag   nSpFlags;
                if ( !nN )
                {
                    pO = pPtr->pAObj;
                    nC = pPtr->ncptiA;
                    nSpFlags = pPtr->nSpFlagsA;
                }
                else
                {
                    pO = pPtr->pBObj;
                    nC = pPtr->ncptiB;
                    nSpFlags = pPtr->nSpFlagsB;
                }
                if ( pO )
                {
                    SdrGluePoint aGluePoint;
                    Reference< XShape > aXShape( pO->getUnoShape(), UNO_QUERY );
                    Reference< XShape > aXConnector( pPtr->pCObj->getUnoShape(), UNO_QUERY );
                    SdrGluePointList* pList = pO->ForceGluePointList();

                    sal_Int32 nId = nC;
                    SdrInventor nInventor = pO->GetObjInventor();

                    if( nInventor == SdrInventor::Default )
                    {
                        bool bValidGluePoint = false;
                        SdrObjKind nObjId = pO->GetObjIdentifier();
                        switch( nObjId )
                        {
                            case SdrObjKind::Group :
                            case SdrObjKind::Graphic :
                            case SdrObjKind::Rectangle :
                            case SdrObjKind::Text :
                            case SdrObjKind::Page :
                            case SdrObjKind::TitleText :
                            case SdrObjKind::OutlineText :
                            {
                                if ( nC & 1 )
                                {
                                    if ( nSpFlags & ShapeFlag::FlipH )
                                        nC ^= 2;    // 1 <-> 3
                                }
                                else
                                {
                                    if ( nSpFlags & ShapeFlag::FlipV )
                                        nC ^= 1;    // 0 <-> 2
                                }
                                switch( nC )
                                {
                                    case 0 :
                                        nId = 0;    // SdrAlign::VERT_TOP;
                                    break;
                                    case 1 :
                                        nId = 3;    // SdrAlign::HORZ_RIGHT;
                                    break;
                                    case 2 :
                                        nId = 2;    // SdrAlign::VERT_BOTTOM;
                                    break;
                                    case 3 :
                                        nId = 1; // SdrAlign::HORZ_LEFT;
                                    break;
                                }
                                if ( nId <= 3 )
                                    bValidGluePoint = true;
                            }
                            break;
                            case SdrObjKind::Polygon :
                            case SdrObjKind::PolyLine :
                            case SdrObjKind::Line :
                            case SdrObjKind::PathLine :
                            case SdrObjKind::PathFill :
                            case SdrObjKind::FreehandLine :
                            case SdrObjKind::FreehandFill :
                            case SdrObjKind::PathPoly :
                            case SdrObjKind::PathPolyLine :
                            {
                                if (pList)
                                {
                                    if (pList->GetCount() > nC )
                                    {
                                        bValidGluePoint = true;
                                        nId = static_cast<sal_Int32>((*pList)[ static_cast<sal_uInt16>(nC)].GetId() + 3 );
                                    }
                                    else
                                    {
                                        bool bNotFound = true;

                                        tools::PolyPolygon aPolyPoly( EscherPropertyContainer::GetPolyPolygon( aXShape ) );
                                        sal_uInt16 k, j, nPolySize = aPolyPoly.Count();
                                        if ( nPolySize )
                                        {
                                            tools::Rectangle aBoundRect( aPolyPoly.GetBoundRect() );
                                            if ( aBoundRect.GetWidth() && aBoundRect.GetHeight() )
                                            {
                                                sal_uInt32  nPointCount = 0;
                                                for ( k = 0; bNotFound && ( k < nPolySize ); k++ )
                                                {
                                                    const tools::Polygon& rPolygon = aPolyPoly.GetObject( k );
                                                    for ( j = 0; bNotFound && ( j < rPolygon.GetSize() ); j++ )
                                                    {
                                                        PolyFlags eFlags = rPolygon.GetFlags( j );
                                                        if ( eFlags == PolyFlags::Normal )
                                                        {
                                                            if ( nC == nPointCount )
                                                            {
                                                                const Point& rPoint = rPolygon.GetPoint( j );
                                                                double fXRel = rPoint.X() - aBoundRect.Left();
                                                                double fYRel = rPoint.Y() - aBoundRect.Top();
                                                                sal_Int32 nWidth = aBoundRect.GetWidth();
                                                                if ( !nWidth )
                                                                    nWidth = 1;
                                                                sal_Int32 nHeight= aBoundRect.GetHeight();
                                                                if ( !nHeight )
                                                                    nHeight = 1;
                                                                fXRel /= static_cast<double>(nWidth);
                                                                fXRel *= 10000;
                                                                fYRel /= static_cast<double>(nHeight);
                                                                fYRel *= 10000;
                                                                aGluePoint.SetPos( Point( static_cast<sal_Int32>(fXRel), static_cast<sal_Int32>(fYRel) ) );
                                                                aGluePoint.SetPercent( true );
                                                                aGluePoint.SetAlign( SdrAlign::VERT_TOP | SdrAlign::HORZ_LEFT );
                                                                aGluePoint.SetEscDir( SdrEscapeDirection::SMART );
                                                                nId = static_cast<sal_Int32>((*pList)[ pList->Insert( aGluePoint ) ].GetId() + 3 );
                                                                bNotFound = false;
                                                            }
                                                            nPointCount++;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        if ( !bNotFound )
                                        {
                                            bValidGluePoint = true;
                                        }
                                    }
                                }
                            }
                            break;

                            case SdrObjKind::CustomShape :
                            {
                                const SfxPoolItem& aCustomShape =  static_cast<SdrObjCustomShape*>(pO)->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY );
                                SdrCustomShapeGeometryItem aGeometryItem( static_cast<const SdrCustomShapeGeometryItem&>(aCustomShape) );
                                static constexpr OUString sPath( u"Path"_ustr );
                                sal_Int16 nGluePointType = EnhancedCustomShapeGluePointType::SEGMENTS;
                                css::uno::Any* pAny = aGeometryItem.GetPropertyValueByName( sPath, u"GluePointType"_ustr );
                                if ( pAny )
                                    *pAny >>= nGluePointType;
                                else
                                {
                                    OUString sShapeType;
                                    pAny = aGeometryItem.GetPropertyValueByName( u"Type"_ustr );
                                    if ( pAny )
                                        *pAny >>= sShapeType;
                                    MSO_SPT eSpType = EnhancedCustomShapeTypeNames::Get( sShapeType );
                                    nGluePointType = GetCustomShapeConnectionTypeDefault( eSpType );
                                }
                                if ( nGluePointType == EnhancedCustomShapeGluePointType::CUSTOM )
                                {
                                    if ( pList && ( pList->GetCount() > nC ) )
                                    {
                                        bValidGluePoint = true;
                                        nId = static_cast<sal_Int32>((*pList)[ static_cast<sal_uInt16>(nC)].GetId() + 3 );
                                    }
                                }
                                else if ( nGluePointType == EnhancedCustomShapeGluePointType::RECT )
                                {
                                    if ( nC & 1 )
                                    {
                                        if ( nSpFlags & ShapeFlag::FlipH )
                                            nC ^= 2;    // 1 <-> 3
                                    }
                                    else
                                    {
                                        if ( nSpFlags & ShapeFlag::FlipV )
                                            nC ^= 1;    // 0 <-> 2
                                    }
                                    switch( nC )
                                    {
                                        case 0 :
                                            nId = 0;    // SdrAlign::VERT_TOP;
                                        break;
                                        case 1 :
                                            nId = 3;    // SdrAlign::HORZ_RIGHT;
                                        break;
                                        case 2 :
                                            nId = 2;    // SdrAlign::VERT_BOTTOM;
                                        break;
                                        case 3 :
                                            nId = 1; // SdrAlign::HORZ_LEFT;
                                        break;
                                    }
                                    if ( nId <= 3 )
                                        bValidGluePoint = true;
                                }
                                else if ( nGluePointType == EnhancedCustomShapeGluePointType::SEGMENTS )
                                {
                                    sal_uInt32 nPt = nC;
                                    css::uno::Sequence< css::drawing::EnhancedCustomShapeSegment > aSegments;
                                    pAny = aGeometryItem.GetPropertyValueByName( sPath, u"Segments"_ustr );
                                    if ( pAny && (*pAny >>= aSegments) )
                                    {
                                        nPt = 0;
                                        for ( sal_Int32 k = 1; nC && ( k < aSegments.getLength() ); k++ )
                                        {
                                            sal_Int16 j, nCnt2 = aSegments[ k ].Count;
                                            if ( aSegments[ k ].Command != EnhancedCustomShapeSegmentCommand::UNKNOWN )
                                            {
                                                for ( j = 0; nC && ( j < nCnt2 ); j++ )
                                                {
                                                    switch( aSegments[ k ].Command )
                                                    {
                                                        case EnhancedCustomShapeSegmentCommand::ENDSUBPATH :
                                                        case EnhancedCustomShapeSegmentCommand::CLOSESUBPATH :
                                                        case EnhancedCustomShapeSegmentCommand::LINETO :
                                                        case EnhancedCustomShapeSegmentCommand::MOVETO :
                                                        {
                                                            nC--;
                                                            nPt++;
                                                        }
                                                        break;
                                                        case EnhancedCustomShapeSegmentCommand::ELLIPTICALQUADRANTX :
                                                        case EnhancedCustomShapeSegmentCommand::ELLIPTICALQUADRANTY :
                                                        break;

                                                        case EnhancedCustomShapeSegmentCommand::CURVETO :
                                                        case EnhancedCustomShapeSegmentCommand::ANGLEELLIPSETO :
                                                        case EnhancedCustomShapeSegmentCommand::ANGLEELLIPSE :
                                                        {
                                                            nC--;
                                                            nPt += 3;
                                                        }
                                                        break;
                                                        case EnhancedCustomShapeSegmentCommand::ARCTO :
                                                        case EnhancedCustomShapeSegmentCommand::ARC :
                                                        case EnhancedCustomShapeSegmentCommand::CLOCKWISEARCTO :
                                                        case EnhancedCustomShapeSegmentCommand::CLOCKWISEARC :
                                                        {
                                                            nC--;
                                                            nPt += 4;
                                                        }
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    pAny = aGeometryItem.GetPropertyValueByName( sPath, u"Coordinates"_ustr );
                                    if ( pAny )
                                    {
                                        css::uno::Sequence< css::drawing::EnhancedCustomShapeParameterPair > aCoordinates;
                                        *pAny >>= aCoordinates;
                                        if ( nPt < o3tl::make_unsigned(aCoordinates.getLength()) )
                                        {
                                            nId = 4;
                                            css::drawing::EnhancedCustomShapeParameterPair& rPara = aCoordinates.getArray()[ nPt ];
                                            sal_Int32 nX = 0, nY = 0;
                                            if ( ( rPara.First.Value >>= nX ) && ( rPara.Second.Value >>= nY ) )
                                            {
                                                static constexpr OUString sGluePoints( u"GluePoints"_ustr );
                                                css::uno::Sequence< css::drawing::EnhancedCustomShapeParameterPair > aGluePoints;
                                                pAny = aGeometryItem.GetPropertyValueByName( sPath, sGluePoints );
                                                if ( pAny )
                                                    *pAny >>= aGluePoints;
                                                sal_Int32 nGluePoints = aGluePoints.getLength();
                                                aGluePoints.realloc( nGluePoints + 1 );
                                                auto pGluePoints = aGluePoints.getArray();
                                                EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( pGluePoints[ nGluePoints ].First, nX );
                                                EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( pGluePoints[ nGluePoints ].Second, nY );
                                                PropertyValue aProp;
                                                aProp.Name = sGluePoints;
                                                aProp.Value <<= aGluePoints;
                                                aGeometryItem.SetPropertyValue( sPath, aProp );
                                                bValidGluePoint = true;
                                                static_cast<SdrObjCustomShape*>(pO)->SetMergedItem( aGeometryItem );
                                                SdrGluePointList* pLst = pO->ForceGluePointList();
                                                if ( pLst->GetCount() > nGluePoints )
                                                    nId = static_cast<sal_Int32>((*pLst)[ static_cast<sal_uInt16>(nGluePoints) ].GetId() + 3 );
                                            }
                                        }
                                    }
                                }
                            }
                            break;
                            default: ;
                        }
                        if ( bValidGluePoint )
                        {
                            Reference< XPropertySet > xPropSet( aXConnector, UNO_QUERY );
                            if ( xPropSet.is() )
                            {
                                if ( nN )
                                {
                                    OUString aPropName( u"EndShape"_ustr );
                                    SetPropValue( Any(aXShape), xPropSet, aPropName );
                                    aPropName = "EndGluePointIndex";
                                    SetPropValue( Any(nId), xPropSet, aPropName );
                                }
                                else
                                {
                                    OUString aPropName( u"StartShape"_ustr );
                                    SetPropValue( Any(aXShape), xPropSet, aPropName );
                                    aPropName = "StartGluePointIndex";
                                    SetPropValue( Any(nId), xPropSet, aPropName );
                                }

                                // Not sure what this is good for, repaint or broadcast of object change.
                                //( Thus I am adding repaint here
                                pO->SetChanged();
                                pO->BroadcastObjectChange();
                            }
                        }
                    }
                }
            }
        }
    }
}

static basegfx::B2DPolyPolygon GetLineArrow( const sal_Int32 nLineWidth, const sal_uInt32 eLineEnd,
    const sal_uInt32 eLineWidth, const sal_uInt32 eLineLength,
    sal_Int32& rnArrowWidth, bool& rbArrowCenter,
    OUString& rsArrowName, bool bScaleArrow )
{
    basegfx::B2DPolyPolygon aRetPolyPoly;
    // 70 100mm = 2pt = 40 twip. In MS, line width less than 2pt has the same size arrow as 2pt
    //If the unit is twip. Make all use this unit especially the critical value 70/40.
    sal_Int32   nLineWidthCritical = bScaleArrow ? 40 : 70;
    double      fLineWidth = nLineWidth < nLineWidthCritical ? nLineWidthCritical : nLineWidth;

    double      fLengthMul, fWidthMul;
    sal_Int32   nLineNumber;
    switch( eLineLength )
    {
        default :
        case mso_lineMediumLenArrow     : fLengthMul = 3.0; nLineNumber = 2; break;
        case mso_lineShortArrow         : fLengthMul = 2.0; nLineNumber = 1; break;
        case mso_lineLongArrow          : fLengthMul = 5.0; nLineNumber = 3; break;
    }
    switch( eLineWidth )
    {
        default :
        case mso_lineMediumWidthArrow   : fWidthMul = 3.0; nLineNumber += 3; break;
        case mso_lineNarrowArrow        : fWidthMul = 2.0; break;
        case mso_lineWideArrow          : fWidthMul = 5.0; nLineNumber += 6; break;
    }

    rbArrowCenter = false;
    OUStringBuffer aArrowName;
    switch ( eLineEnd )
    {
        case mso_lineArrowEnd :
        {
            basegfx::B2DPolygon aTriangle;
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.50, 0.0 ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth, fLengthMul * fLineWidth ));
            aTriangle.append(basegfx::B2DPoint( 0.0, fLengthMul * fLineWidth ));
            aTriangle.setClosed(true);
            aRetPolyPoly = basegfx::B2DPolyPolygon(aTriangle);
            aArrowName.append("msArrowEnd ");
        }
        break;

        case mso_lineArrowOpenEnd :
        {
            switch( eLineLength )
            {
                default :
                case mso_lineMediumLenArrow     : fLengthMul = 4.5; break;
                case mso_lineShortArrow         : fLengthMul = 3.5; break;
                case mso_lineLongArrow          : fLengthMul = 6.0; break;
            }
            switch( eLineWidth )
            {
                default :
                case mso_lineMediumWidthArrow   : fWidthMul = 4.5; break;
                case mso_lineNarrowArrow        : fWidthMul = 3.5; break;
                case mso_lineWideArrow          : fWidthMul = 6.0; break;
            }
            basegfx::B2DPolygon aTriangle;
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.50 , 0.0 ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth, fLengthMul * fLineWidth * 0.91 ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.85, fLengthMul * fLineWidth ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.50, fLengthMul * fLineWidth * 0.36 ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.15, fLengthMul * fLineWidth ));
            aTriangle.append(basegfx::B2DPoint( 0.0, fLengthMul * fLineWidth * 0.91 ));
            aTriangle.setClosed(true);
            aRetPolyPoly = basegfx::B2DPolyPolygon(aTriangle);
            aArrowName.append("msArrowOpenEnd ");
        }
        break;
        case mso_lineArrowStealthEnd :
        {
            basegfx::B2DPolygon aTriangle;
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.50 , 0.0 ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth , fLengthMul * fLineWidth ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.50 , fLengthMul * fLineWidth * 0.60 ));
            aTriangle.append(basegfx::B2DPoint( 0.0, fLengthMul * fLineWidth ));
            aTriangle.setClosed(true);
            aRetPolyPoly = basegfx::B2DPolyPolygon(aTriangle);
            aArrowName.append("msArrowStealthEnd ");
        }
        break;
        case mso_lineArrowDiamondEnd :
        {
            basegfx::B2DPolygon aTriangle;
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.50 , 0.0 ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth , fLengthMul * fLineWidth * 0.50 ));
            aTriangle.append(basegfx::B2DPoint( fWidthMul * fLineWidth * 0.50 , fLengthMul * fLineWidth ));
            aTriangle.append(basegfx::B2DPoint( 0.0, fLengthMul * fLineWidth * 0.50 ));
            aTriangle.setClosed(true);
            aRetPolyPoly = basegfx::B2DPolyPolygon(aTriangle);
            rbArrowCenter = true;
            aArrowName.append("msArrowDiamondEnd ");
        }
        break;
        case mso_lineArrowOvalEnd :
        {
            aRetPolyPoly = basegfx::B2DPolyPolygon(
                            XPolygon(
                                Point( static_cast<sal_Int32>( fWidthMul * fLineWidth * 0.50 ), 0 ),
                                static_cast<sal_Int32>( fWidthMul * fLineWidth * 0.50 ),
                                static_cast<sal_Int32>( fLengthMul * fLineWidth * 0.50 ),
                                0_deg100, 36000_deg100 ).getB2DPolygon() );
            rbArrowCenter = true;
            aArrowName.append("msArrowOvalEnd ");
        }
        break;
        default: break;
    }
    aArrowName.append(nLineNumber);
    rsArrowName = aArrowName.makeStringAndClear();
    rnArrowWidth = static_cast<sal_Int32>( fLineWidth * fWidthMul );

    return aRetPolyPoly;
}

void DffPropertyReader::ApplyLineAttributes( SfxItemSet& rSet, const MSO_SPT eShapeType ) const // #i28269#
{
    sal_uInt32 nLineFlags(GetPropertyValue( DFF_Prop_fNoLineDrawDash, 0 ));

    if(!IsHardAttribute( DFF_Prop_fLine ) && !IsCustomShapeStrokedByDefault( eShapeType ))
    {
        nLineFlags &= ~0x08;
    }

    if ( nLineFlags & 8 )
    {
        // Line Attributes
        sal_Int32 nLineWidth = static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_lineWidth, 9525 ));

        // support LineCap
        auto eLineCap = GetPropertyValue(DFF_Prop_lineEndCapStyle, mso_lineEndCapFlat);

        switch(eLineCap)
        {
            default: /* case mso_lineEndCapFlat */
            {
                // no need to set, it is the default. If this changes, this needs to be activated
                // rSet.Put(XLineCapItem(css::drawing::LineCap_BUTT));
                break;
            }
            case mso_lineEndCapRound:
            {
                rSet.Put(XLineCapItem(css::drawing::LineCap_ROUND));
                break;
            }
            case mso_lineEndCapSquare:
            {
                rSet.Put(XLineCapItem(css::drawing::LineCap_SQUARE));
                break;
            }
        }

        auto eLineDashing = GetPropertyValue( DFF_Prop_lineDashing, mso_lineSolid);
        if (eLineDashing == mso_lineSolid || nLineWidth < 0)
            rSet.Put(XLineStyleItem( drawing::LineStyle_SOLID ) );
        else
        {
            // Despite of naming "dot" and "dash", that are all dashes and a "dot" can be longer
            // than a "dash". The naming indicates the order, "dot" is always the first dash and
            // "dash" is always the second dash. MS Office always starts with the longer dash, so
            // set it here accordingly.
            // The preset from binary is essentially the same as from OOXML. So here the same
            // setting is used as in oox import. The comment corresponds to
            // "dots, dotLen, dashes, dashLen, distance" there.
            // MS Office uses always relative length, so no need to consider nLineWidth
            // here. Values are of kind 300 for 300% in css::drawing::DashStyle, for example.

            sal_uInt16  nDots = 1; // in all cases, "solid" is treated above
            // initialize, will be changed if necessary
            sal_uInt32  nDotLen = 300;
            sal_uInt16  nDashes = 0;
            sal_uInt32  nDashLen = 0;
            sal_uInt32  nDistance = 300;
            switch ( eLineDashing )
            {
                default:
                case mso_lineDotSys : // 1 1 0 0 1
                {
                    nDotLen =100;
                    nDistance = 100;
                }
                break;

                case mso_lineDashGEL : // 1 4 0 0 3
                {
                    nDotLen = 400;
                }
                break;

                case mso_lineDashDotGEL : // 1 4 1 1 3
                {
                    nDotLen = 400;
                    nDashes = 1;
                    nDashLen = 100;
                }
                break;

                case mso_lineLongDashGEL : // 1 8 0 0 3
                {
                    nDotLen = 800;
                }
                break;

                case mso_lineLongDashDotGEL : // 1 8 1 1 3
                {
                    nDotLen = 800;
                    nDashes = 1;
                    nDashLen = 100;
                }
                break;

                case mso_lineLongDashDotDotGEL: // 1 8 2 1 3
                {
                    nDotLen = 800;
                    nDashes = 2;
                    nDashLen = 100;
                }
                break;

                case mso_lineDotGEL: // 1 1 0 0 3
                {
                    nDotLen = 100;
                }
                break;

                case mso_lineDashSys: // 1 3 0 0 1
                {
                    nDistance = 100;
                }
                break;

                case mso_lineDashDotSys: // 1 3 1 1 1
                {
                    nDashes = 1;
                    nDashLen = 100;
                    nDistance = 100;
                }
                break;

                case mso_lineDashDotDotSys: // 1 3 2 1 1
                {
                    nDashes = 2;
                    nDashLen = 100;
                    nDistance = 100;
                }
                break;
            }
            rSet.Put( XLineDashItem( OUString(), XDash( css::drawing::DashStyle_RECTRELATIVE, nDots, nDotLen, nDashes, nDashLen, nDistance ) ) );
            rSet.Put( XLineStyleItem( drawing::LineStyle_DASH ) );
        }
        rSet.Put( XLineColorItem( OUString(), rManager.MSO_CLR_ToColor( GetPropertyValue( DFF_Prop_lineColor, 0 ) ) ) );
        if ( IsProperty( DFF_Prop_lineOpacity ) )
        {
            double nTrans = GetPropertyValue(DFF_Prop_lineOpacity, 0x10000);
            nTrans = (nTrans * 100) / 65536;
            rSet.Put(XLineTransparenceItem(
                sal_uInt16(100 - ::rtl::math::round(nTrans))));
        }

        rManager.ScaleEmu( nLineWidth );
        rSet.Put( XLineWidthItem( nLineWidth ) );

        // SJ: LineJoint (setting each time a line is set, because our internal joint type has another default)
        MSO_LineJoin eLineJointDefault = mso_lineJoinMiter;
        if ( eShapeType == mso_sptMin )
            eLineJointDefault = mso_lineJoinRound;
        auto eLineJoint = GetPropertyValue(DFF_Prop_lineJoinStyle, eLineJointDefault);
        css::drawing::LineJoint eXLineJoint( css::drawing::LineJoint_MITER );
        if ( eLineJoint == mso_lineJoinBevel )
            eXLineJoint = css::drawing::LineJoint_BEVEL;
        else if ( eLineJoint == mso_lineJoinRound )
            eXLineJoint = css::drawing::LineJoint_ROUND;
        rSet.Put( XLineJointItem( eXLineJoint ) );

        if ( nLineFlags & 0x10 )
        {
            bool bScaleArrows = rManager.pSdrModel->GetScaleUnit() == MapUnit::MapTwip;

            // LineStart

            if ( IsProperty( DFF_Prop_lineStartArrowhead ) )
            {
                auto eLineEnd = GetPropertyValue(DFF_Prop_lineStartArrowhead, 0);
                auto eWidth = GetPropertyValue(DFF_Prop_lineStartArrowWidth, mso_lineMediumWidthArrow);
                auto eLength = GetPropertyValue(DFF_Prop_lineStartArrowLength, mso_lineMediumLenArrow);

                sal_Int32   nArrowWidth;
                bool        bArrowCenter;
                OUString    aArrowName;
                basegfx::B2DPolyPolygon aPolyPoly(GetLineArrow( nLineWidth, eLineEnd, eWidth, eLength, nArrowWidth, bArrowCenter, aArrowName, bScaleArrows ));

                rSet.Put( XLineStartWidthItem( nArrowWidth ) );
                rSet.Put( XLineStartItem( std::move(aArrowName), std::move(aPolyPoly) ) );
                rSet.Put( XLineStartCenterItem( bArrowCenter ) );
            }

            // LineEnd

            if ( IsProperty( DFF_Prop_lineEndArrowhead ) )
            {
                auto eLineEnd = GetPropertyValue(DFF_Prop_lineEndArrowhead, 0);
                auto eWidth = GetPropertyValue(DFF_Prop_lineEndArrowWidth, mso_lineMediumWidthArrow);
                auto eLength = GetPropertyValue(DFF_Prop_lineEndArrowLength, mso_lineMediumLenArrow);

                sal_Int32   nArrowWidth;
                bool        bArrowCenter;
                OUString    aArrowName;
                basegfx::B2DPolyPolygon aPolyPoly(GetLineArrow( nLineWidth, eLineEnd, eWidth, eLength, nArrowWidth, bArrowCenter, aArrowName, bScaleArrows ));

                rSet.Put( XLineEndWidthItem( nArrowWidth ) );
                rSet.Put( XLineEndItem( std::move(aArrowName), std::move(aPolyPoly) ) );
                rSet.Put( XLineEndCenterItem( bArrowCenter ) );
            }
        }
    }
    else
        rSet.Put( XLineStyleItem( drawing::LineStyle_NONE ) );
}

namespace {

struct ShadeColor
{
    Color       aColor;
    double      fDist;

    ShadeColor( const Color& rC, double fR ) : aColor( rC ), fDist( fR ) {};
};

}

static void GetShadeColors( const SvxMSDffManager& rManager, const DffPropertyReader& rProperties, SvStream& rIn, std::vector< ShadeColor >& rShadeColors )
{
    sal_uInt64 nPos = rIn.Tell();
    if ( rProperties.IsProperty( DFF_Prop_fillShadeColors ) )
    {
        sal_uInt16 i = 0, nNumElem = 0;
        bool bOk = false;
        if (rProperties.SeekToContent(DFF_Prop_fillShadeColors, rIn))
        {
            sal_uInt16 nNumElemReserved = 0, nSize = 0;
            rIn.ReadUInt16( nNumElem ).ReadUInt16( nNumElemReserved ).ReadUInt16( nSize );
            //sanity check that the stream is long enough to fulfill nNumElem * 2 sal_Int32s
            bOk = rIn.remainingSize() / (2*sizeof(sal_Int32)) >= nNumElem;
        }
        if (bOk)
        {
            for ( ; i < nNumElem; i++ )
            {
                sal_Int32 nColor(0);
                sal_Int32 nDist(0);

                rIn.ReadInt32( nColor ).ReadInt32( nDist );
                rShadeColors.emplace_back( rManager.MSO_CLR_ToColor( nColor, DFF_Prop_fillColor ), 1.0 - ( nDist / 65536.0 ) );
            }
        }
    }
    if ( rShadeColors.empty() )
    {
        rShadeColors.emplace_back( rManager.MSO_CLR_ToColor( rProperties.GetPropertyValue( DFF_Prop_fillBackColor, sal_uInt32(COL_WHITE) ), DFF_Prop_fillBackColor ), 0 );
        rShadeColors.emplace_back( rManager.MSO_CLR_ToColor( rProperties.GetPropertyValue( DFF_Prop_fillColor, sal_uInt32(COL_WHITE) ), DFF_Prop_fillColor ), 1 );
    }
    rIn.Seek( nPos );
}

static void ApplyRectangularGradientAsBitmap( const SvxMSDffManager& rManager, SvStream& rIn, SfxItemSet& rSet, const std::vector< ShadeColor >& rShadeColors, const DffObjData& rObjData, Degree100 nFix16Angle )
{
    Size aBitmapSizePixel( static_cast< sal_Int32 >( ( rObjData.aBoundRect.GetWidth() / 2540.0 ) * 90.0 ),      // we will create a bitmap with 90 dpi
                           static_cast< sal_Int32 >( ( rObjData.aBoundRect.GetHeight() / 2540.0 ) * 90.0 ) );
    if (aBitmapSizePixel.IsEmpty() || aBitmapSizePixel.Width() > 1024 || aBitmapSizePixel.Height() > 1024)
        return;

    double fFocusX = rManager.GetPropertyValue( DFF_Prop_fillToRight, 0 ) / 65536.0;
    double fFocusY = rManager.GetPropertyValue( DFF_Prop_fillToBottom, 0 ) / 65536.0;

    vcl::bitmap::RawBitmap aBitmap(aBitmapSizePixel, 24);

    for ( tools::Long nY = 0; nY < aBitmapSizePixel.Height(); nY++ )
    {
        for ( tools::Long nX = 0; nX < aBitmapSizePixel.Width(); nX++ )
        {
            double fX = static_cast< double >( nX ) / aBitmapSizePixel.Width();
            double fY = static_cast< double >( nY ) / aBitmapSizePixel.Height();

            double fD, fDist;
            if ( fX < fFocusX )
            {
                if ( fY < fFocusY )
                {
                    if ( fX > fY )
                    {
                        fDist = fY;
                        fD = fFocusY;
                    }
                    else
                    {
                        fDist = fX;
                        fD = fFocusX;
                    }
                }
                else
                {
                    if ( fX > ( 1 - fY ) )
                    {
                        fDist = 1 - fY;
                        fD = 1 - fFocusY;
                    }
                    else
                    {
                        fDist = fX;
                        fD = fFocusX;
                    }
                }
            }
            else
            {
                if ( fY < fFocusY )
                {
                    if ( ( 1 - fX ) > fY )
                    {
                        fDist = fY;
                        fD = fFocusY;
                    }
                    else
                    {
                        fDist = 1 - fX;
                        fD = 1 - fFocusX;
                    }
                }
                else
                {
                    if ( ( 1 - fX ) > ( 1 - fY ) )
                    {
                        fDist = 1 - fY;
                        fD = 1 - fFocusY;
                    }
                    else
                    {
                        fDist = 1 - fX;
                        fD = 1 - fFocusX;
                    }
                }
            }
            if ( fD != 0.0 )
                fDist /= fD;

            double fA = 0.0;
            Color aColorA = rShadeColors.front().aColor;
            double fB = 1.0;
            Color aColorB( aColorA );
            for ( const auto& rShadeColor : rShadeColors )
            {
                if ( fA <= rShadeColor.fDist && rShadeColor.fDist <= fDist )
                {
                    fA = rShadeColor.fDist;
                    aColorA = rShadeColor.aColor;
                }
                if ( fDist < rShadeColor.fDist && rShadeColor.fDist <= fB )
                {
                    fB = rShadeColor.fDist;
                    aColorB = rShadeColor.aColor;
                }
            }
            double fRed = aColorA.GetRed(), fGreen = aColorA.GetGreen(), fBlue = aColorA.GetBlue();
            double fD1 = fB - fA;
            if ( fD1 != 0.0 )
            {
                fRed   += ( ( ( fDist - fA ) * ( aColorB.GetRed() - aColorA.GetRed() ) ) / fD1 );       // + aQuantErrCurrScan[ nX ].fRed;
                fGreen += ( ( ( fDist - fA ) * ( aColorB.GetGreen() - aColorA.GetGreen() ) ) / fD1 );   // + aQuantErrCurrScan[ nX ].fGreen;
                fBlue  += ( ( ( fDist - fA ) * ( aColorB.GetBlue() - aColorA.GetBlue() ) ) / fD1 );     // + aQuantErrCurrScan[ nX ].fBlue;
            }
            sal_Int16 nRed   = static_cast< sal_Int16 >( fRed   + 0.5 );
            sal_Int16 nGreen = static_cast< sal_Int16 >( fGreen + 0.5 );
            sal_Int16 nBlue  = static_cast< sal_Int16 >( fBlue  + 0.5 );
            if ( nRed < 0 )
                nRed = 0;
            if ( nRed > 255 )
                nRed = 255;
            if ( nGreen < 0 )
                nGreen = 0;
            if ( nGreen > 255 )
                nGreen = 255;
            if ( nBlue < 0 )
                nBlue = 0;
            if ( nBlue > 255 )
                nBlue = 255;

            aBitmap.SetPixel(nY, nX, Color(static_cast<sal_Int8>(nRed), static_cast<sal_Int8>(nGreen), static_cast<sal_Int8>(nBlue)));
        }
    }
    BitmapEx aBitmapEx = vcl::bitmap::CreateFromData( std::move(aBitmap) );

    if ( nFix16Angle )
    {
        bool bRotateWithShape = true;   // sal_True seems to be default
        sal_uInt64 nPos = rIn.Tell();
        if ( const_cast< SvxMSDffManager& >( rManager ).maShapeRecords.SeekToContent( rIn, DFF_msofbtUDefProp, SEEK_FROM_CURRENT_AND_RESTART ) )
        {
            const_cast< SvxMSDffManager& >( rManager ).maShapeRecords.Current()->SeekToBegOfRecord( rIn );
            DffPropertyReader aSecPropSet( rManager );
            aSecPropSet.ReadPropSet( rIn, nullptr );
            sal_Int32 nSecFillProperties = aSecPropSet.GetPropertyValue( DFF_Prop_fNoFillHitTest, 0x200020 );
            bRotateWithShape = ( nSecFillProperties & 0x0020 );
        }
        rIn.Seek( nPos );
        if ( bRotateWithShape )
        {
            // convert from 100th to 10th degrees
            aBitmapEx.Rotate( to<Degree10>(nFix16Angle), rShadeColors[ 0 ].aColor );

            BmpMirrorFlags nMirrorFlags = BmpMirrorFlags::NONE;
            if ( rObjData.nSpFlags & ShapeFlag::FlipV )
                nMirrorFlags |= BmpMirrorFlags::Vertical;
            if ( rObjData.nSpFlags & ShapeFlag::FlipH )
                nMirrorFlags |= BmpMirrorFlags::Horizontal;
            if ( nMirrorFlags != BmpMirrorFlags::NONE )
                aBitmapEx.Mirror( nMirrorFlags );
        }
    }

    rSet.Put(XFillBmpTileItem(false));
    rSet.Put(XFillBitmapItem(OUString(), Graphic(aBitmapEx)));
}

void DffPropertyReader::ApplyFillAttributes( SvStream& rIn, SfxItemSet& rSet, const DffObjData& rObjData ) const
{
    sal_uInt32 nFillFlags(GetPropertyValue( DFF_Prop_fNoFillHitTest, 0 ));

    std::vector< ShadeColor > aShadeColors;
    GetShadeColors( rManager, *this, rIn, aShadeColors );

    if(!IsHardAttribute( DFF_Prop_fFilled ) && !IsCustomShapeFilledByDefault( rObjData.eShapeType ))
    {
        nFillFlags &= ~0x10;
    }

    if ( nFillFlags & 0x10 )
    {
        auto eMSO_FillType = GetPropertyValue(DFF_Prop_fillType, mso_fillSolid);
        bool bUseSlideBackground = false;
        drawing::FillStyle eXFill = drawing::FillStyle_NONE;
        switch( eMSO_FillType )
        {
            case mso_fillSolid :            // Fill with a solid color
                eXFill = drawing::FillStyle_SOLID;
            break;
            case mso_fillPattern :          // Fill with a pattern (bitmap)
            case mso_fillTexture :          // A texture (pattern with its own color map)
            case mso_fillPicture :          // Center a picture in the shape
                eXFill = drawing::FillStyle_BITMAP;
            break;
            case mso_fillShadeCenter :      // Shade from bounding rectangle to end point
            {
                //If it is imported as a bitmap, it will not work well with transparency especially 100
                //But the gradient look well comparing with imported as gradient. And rotate with shape
                //also works better. So here just keep it.
                if ( rObjData.aBoundRect.IsEmpty() )// size of object needed to be able
                    eXFill = drawing::FillStyle_GRADIENT;        // to create a bitmap substitution
                else
                    eXFill = drawing::FillStyle_BITMAP;
            }
            break;
            case mso_fillShade :            // Shade from start to end points
            case mso_fillShadeShape :       // Shade from shape outline to end point
            case mso_fillShadeScale :       // Similar to mso_fillShade, but the fillAngle
            case mso_fillShadeTitle :       // special type - shade to title ---  for PP
                eXFill = drawing::FillStyle_GRADIENT;
            break;
            case mso_fillBackground :       // Use the background fill color/pattern
                eXFill = drawing::FillStyle_NONE;
                bUseSlideBackground = true;
            break;
            default: break;
        }
        rSet.Put( XFillStyleItem( eXFill ) );

        double dTrans  = 1.0;
        double dBackTrans = 1.0;
        if (IsProperty(DFF_Prop_fillOpacity))
        {
            dTrans = GetPropertyValue(DFF_Prop_fillOpacity, 0) / 65536.0;
            if ( eXFill != drawing::FillStyle_GRADIENT )
            {
                dTrans = dTrans * 100;
                rSet.Put(XFillTransparenceItem(
                    sal_uInt16(100 - ::rtl::math::round(dTrans))));
            }
        }

        if ( IsProperty(DFF_Prop_fillBackOpacity) )
            dBackTrans = GetPropertyValue(DFF_Prop_fillBackOpacity, 0) / 65536.0;

        if ( ( eMSO_FillType == mso_fillShadeCenter ) && ( eXFill == drawing::FillStyle_BITMAP ) )
        {
            ApplyRectangularGradientAsBitmap( rManager, rIn, rSet, aShadeColors, rObjData, mnFix16Angle );
        }
        else if ( eXFill == drawing::FillStyle_GRADIENT )
        {
            ImportGradientColor ( rSet, eMSO_FillType, dTrans , dBackTrans );
        }
        else if ( eXFill == drawing::FillStyle_BITMAP )
        {
            if( IsProperty( DFF_Prop_fillBlip ) )
            {
                Graphic aGraf;
                // first try to get BLIP from cache
                bool bOK = const_cast<SvxMSDffManager&>(rManager).GetBLIP( GetPropertyValue( DFF_Prop_fillBlip, 0 ), aGraf );
                // then try directly from stream (i.e. Excel chart hatches/bitmaps)
                if ( !bOK )
                    bOK = SeekToContent( DFF_Prop_fillBlip, rIn ) && SvxMSDffManager::GetBLIPDirect( rIn, aGraf );
                if ( bOK )
                {
                    if ( eMSO_FillType == mso_fillPattern )
                    {
                        Bitmap aBmp( aGraf.GetBitmapEx().GetBitmap() );
                        if (aBmp.GetSizePixel().Width() == 8 &&
                            aBmp.GetSizePixel().Height() == 8 &&
                            aBmp.getPixelFormat() == vcl::PixelFormat::N8_BPP)
                        {
                            Color aCol1( COL_WHITE ), aCol2( COL_WHITE );

                            if ( IsProperty( DFF_Prop_fillColor ) )
                                aCol1 = rManager.MSO_CLR_ToColor( GetPropertyValue( DFF_Prop_fillColor, 0 ), DFF_Prop_fillColor );

                            if ( IsProperty( DFF_Prop_fillBackColor ) )
                                aCol2 = rManager.MSO_CLR_ToColor( GetPropertyValue( DFF_Prop_fillBackColor, 0 ), DFF_Prop_fillBackColor );

                            // Create a bitmap for the pattern with expected colors
                            vcl::bitmap::RawBitmap aResult(Size(8, 8), 24);
                            {
                                BitmapScopedReadAccess pRead(aBmp);

                                for (tools::Long y = 0; y < aResult.Height(); ++y)
                                {
                                    Scanline pScanlineRead = pRead->GetScanline( y );
                                    for (tools::Long x = 0; x < aResult.Width(); ++x)
                                    {
                                        Color aReadColor;
                                        if (pRead->HasPalette())
                                            aReadColor = pRead->GetPaletteColor(pRead->GetIndexFromData(pScanlineRead, x));
                                        else
                                            aReadColor = pRead->GetPixelFromData(pScanlineRead, x);

                                        if (aReadColor == Color(0))
                                            aResult.SetPixel(y, x, aCol2);
                                        else
                                            aResult.SetPixel(y, x, aCol1);
                                    }
                                }
                            }
                            aGraf = Graphic(vcl::bitmap::CreateFromData(std::move(aResult)));
                        }

                        rSet.Put(XFillBitmapItem(OUString(), std::move(aGraf)));
                    }
                    else if ( eMSO_FillType == mso_fillTexture )
                    {
                        rSet.Put(XFillBmpTileItem(true));
                        rSet.Put(XFillBitmapItem(OUString(), std::move(aGraf)));
                        rSet.Put(XFillBmpSizeXItem(GetPropertyValue(DFF_Prop_fillWidth, 0) / 360));
                        rSet.Put(XFillBmpSizeYItem(GetPropertyValue(DFF_Prop_fillHeight, 0) / 360));
                        rSet.Put(XFillBmpSizeLogItem(true));
                    }
                    else
                    {
                        rSet.Put(XFillBitmapItem(OUString(), std::move(aGraf)));
                        rSet.Put(XFillBmpTileItem(false));
                    }
                }
            }
        }
        else if (eXFill == drawing::FillStyle_NONE && bUseSlideBackground)
        {
            rSet.Put( XFillStyleItem( drawing::FillStyle_NONE ) );
            XFillUseSlideBackgroundItem aFillBgItem(true);
            rSet.Put(aFillBgItem);
        }
    }
    else
        rSet.Put( XFillStyleItem( drawing::FillStyle_NONE ) );
}

void DffPropertyReader::ApplyCustomShapeTextAttributes( SfxItemSet& rSet ) const
{
    bool  bVerticalText = false;
    sal_Int32 nTextLeft = GetPropertyValue( DFF_Prop_dxTextLeft, 25 * 3600 ) / 360;     // 0.25 cm (emu)
    sal_Int32 nTextRight = GetPropertyValue( DFF_Prop_dxTextRight, 25 * 3600 ) / 360;   // 0.25 cm (emu)
    sal_Int32 nTextTop = GetPropertyValue( DFF_Prop_dyTextTop, 13 * 3600 ) / 360;       // 0.13 cm (emu)
    sal_Int32 nTextBottom = GetPropertyValue( DFF_Prop_dyTextBottom, 13 * 3600 ) /360;  // 0.13 cm (emu)

    SdrTextVertAdjust eTVA;
    SdrTextHorzAdjust eTHA;

    if ( IsProperty( DFF_Prop_txflTextFlow ) )
    {
        auto eTextFlow = GetPropertyValue(DFF_Prop_txflTextFlow, 0) & 0xFFFF;
        switch( eTextFlow )
        {
            case mso_txflTtoBA :    /* #68110# */   // Top to Bottom @-font, oben -> unten
            case mso_txflTtoBN :                    // Top to Bottom non-@, oben -> unten
            case mso_txflVertN :                    // Vertical, non-@, oben -> unten
                bVerticalText = true;           // nTextRotationAngle += 27000;
            break;
            default: break;
        }
    }
    sal_Int32 nFontDirection = GetPropertyValue( DFF_Prop_cdirFont, mso_cdir0 );
    if ( ( nFontDirection == 1 ) || ( nFontDirection == 3 ) )
        bVerticalText = !bVerticalText;

    if ( bVerticalText )
    {
        eTHA = SDRTEXTHORZADJUST_CENTER;

        // read text anchor
        sal_uInt32 eTextAnchor = GetPropertyValue( DFF_Prop_anchorText, mso_anchorTop );

        switch( eTextAnchor )
        {
            case mso_anchorTop:
            case mso_anchorTopCentered:
            case mso_anchorTopBaseline:
            case mso_anchorTopCenteredBaseline:
                eTHA = SDRTEXTHORZADJUST_RIGHT;
            break;

            case mso_anchorMiddle :
            case mso_anchorMiddleCentered:
                eTHA = SDRTEXTHORZADJUST_CENTER;
            break;

            case mso_anchorBottom:
            case mso_anchorBottomCentered:
            case mso_anchorBottomBaseline:
            case mso_anchorBottomCenteredBaseline:
                eTHA = SDRTEXTHORZADJUST_LEFT;
            break;
        }
        // if there is a 100% use of following attributes, the textbox can been aligned also in vertical direction
        switch ( eTextAnchor )
        {
            case mso_anchorTopCentered :
            case mso_anchorMiddleCentered :
            case mso_anchorBottomCentered :
            case mso_anchorTopCenteredBaseline:
            case mso_anchorBottomCenteredBaseline:
                eTVA = SDRTEXTVERTADJUST_CENTER;
            break;

            default :
                eTVA = SDRTEXTVERTADJUST_TOP;
            break;
        }
    }
    else
    {
        eTVA = SDRTEXTVERTADJUST_CENTER;

        // read text anchor
        sal_uInt32 eTextAnchor = GetPropertyValue( DFF_Prop_anchorText, mso_anchorTop );

        switch( eTextAnchor )
        {
            case mso_anchorTop:
            case mso_anchorTopCentered:
            case mso_anchorTopBaseline:
            case mso_anchorTopCenteredBaseline:
                eTVA = SDRTEXTVERTADJUST_TOP;
            break;

            case mso_anchorMiddle :
            case mso_anchorMiddleCentered:
                eTVA = SDRTEXTVERTADJUST_CENTER;
            break;

            case mso_anchorBottom:
            case mso_anchorBottomCentered:
            case mso_anchorBottomBaseline:
            case mso_anchorBottomCenteredBaseline:
                eTVA = SDRTEXTVERTADJUST_BOTTOM;
            break;
        }
        // if there is a 100% usage of following attributes, the textbox can be aligned also in horizontal direction
        switch ( eTextAnchor )
        {
            case mso_anchorTopCentered :
            case mso_anchorMiddleCentered :
            case mso_anchorBottomCentered :
            case mso_anchorTopCenteredBaseline:
            case mso_anchorBottomCenteredBaseline:
                eTHA = SDRTEXTHORZADJUST_CENTER;    // the text has to be displayed using the full width;
            break;

            default :
                eTHA = SDRTEXTHORZADJUST_LEFT;
            break;
        }
    }
    rSet.Put( SvxFrameDirectionItem( bVerticalText ? SvxFrameDirection::Vertical_RL_TB : SvxFrameDirection::Horizontal_LR_TB, EE_PARA_WRITINGDIR ) );

    rSet.Put( SdrTextVertAdjustItem( eTVA ) );
    rSet.Put( SdrTextHorzAdjustItem( eTHA ) );

    rSet.Put( makeSdrTextLeftDistItem( nTextLeft ) );
    rSet.Put( makeSdrTextRightDistItem( nTextRight ) );
    rSet.Put( makeSdrTextUpperDistItem( nTextTop ) );
    rSet.Put( makeSdrTextLowerDistItem( nTextBottom ) );

    rSet.Put( makeSdrTextWordWrapItem( GetPropertyValue(DFF_Prop_WrapText, mso_wrapSquare) != mso_wrapNone ) );
    rSet.Put( makeSdrTextAutoGrowHeightItem( ( GetPropertyValue( DFF_Prop_FitTextToShape, 0 ) & 2 ) != 0 ) );
}

void DffPropertyReader::ApplyCustomShapeGeometryAttributes( SvStream& rIn, SfxItemSet& rSet, const DffObjData& rObjData ) const
{

    sal_uInt32 nAdjustmentsWhichNeedsToBeConverted = 0;


    // creating SdrCustomShapeGeometryItem

    typedef std::vector< beans::PropertyValue > PropVec;

    // aPropVec will be filled with all PropertyValues
    PropVec aPropVec;
    PropertyValue aProp;


    // "Type" property, including the predefined CustomShape type name

    aProp.Name  = "Type";
    aProp.Value <<= EnhancedCustomShapeTypeNames::Get( rObjData.eShapeType );
    aPropVec.push_back( aProp );


    // "ViewBox"


    sal_Int32 nCoordWidth = 21600;  // needed to replace handle type center with absolute value
    sal_Int32 nCoordHeight= 21600;
    if ( IsProperty( DFF_Prop_geoLeft ) || IsProperty( DFF_Prop_geoTop ) || IsProperty( DFF_Prop_geoRight ) || IsProperty( DFF_Prop_geoBottom ) )
    {
        css::awt::Rectangle aViewBox;
        aViewBox.X = GetPropertyValue( DFF_Prop_geoLeft, 0 );
        aViewBox.Y = GetPropertyValue( DFF_Prop_geoTop, 0 );
        aViewBox.Width = nCoordWidth = o3tl::saturating_sub<sal_Int32>(GetPropertyValue(DFF_Prop_geoRight, 21600), aViewBox.X);
        aViewBox.Height = nCoordHeight = o3tl::saturating_sub<sal_Int32>(GetPropertyValue(DFF_Prop_geoBottom, 21600), aViewBox.Y);
        aProp.Name = "ViewBox";
        aProp.Value <<= aViewBox;
        aPropVec.push_back( aProp );
    }

    // TextRotateAngle

    if ( IsProperty( DFF_Prop_txflTextFlow ) || IsProperty( DFF_Prop_cdirFont ) )
    {
        sal_Int32 nTextRotateAngle = 0;
        auto eTextFlow = GetPropertyValue(DFF_Prop_txflTextFlow, 0) & 0xFFFF;

        if ( eTextFlow == mso_txflBtoT )    // Bottom to Top non-@
            nTextRotateAngle += 90;
        switch( GetPropertyValue( DFF_Prop_cdirFont, mso_cdir0 ) )  // SJ: mso_cdir90 and mso_cdir270 will be simulated by
        {                                                           // activating vertical writing for the text objects
            case mso_cdir90 :
            {
                if ( eTextFlow == mso_txflTtoBA )
                    nTextRotateAngle -= 180;
            }
            break;
            case mso_cdir180: nTextRotateAngle -= 180; break;
            case mso_cdir270:
            {
                if ( eTextFlow != mso_txflTtoBA )
                    nTextRotateAngle -= 180;
            }
            break;
            default: break;
        }
        if ( nTextRotateAngle )
        {
            double fTextRotateAngle = nTextRotateAngle;
            aProp.Name = "TextRotateAngle";
            aProp.Value <<= fTextRotateAngle;
            aPropVec.push_back( aProp );
        }
    }

    // "Extrusion" PropertySequence element

    bool bExtrusionOn = ( GetPropertyValue( DFF_Prop_fc3DLightFace, 0 ) & 8 ) != 0;
    if ( bExtrusionOn )
    {
        PropVec aExtrusionPropVec;

        // "Extrusion"
        aProp.Name = "Extrusion";
        aProp.Value <<= bExtrusionOn;
        aExtrusionPropVec.push_back( aProp );

        // "Brightness"
        // MS Office default 0x00004E20 16.16 FixedPoint, 20000/65536=0.30517, ODF default 33%.
        // Thus must set value even if default.
        double fBrightness = 20000.0;
        if ( IsProperty( DFF_Prop_c3DAmbientIntensity ) )
        {
            // Value must be in range 0.0 to 1.0 in MS Office binary specification, but larger
            // values are in fact interpreted.
            fBrightness = GetPropertyValue( DFF_Prop_c3DAmbientIntensity, 0 );
        }
        fBrightness /= 655.36;
        aProp.Name = "Brightness";
        aProp.Value <<= fBrightness;
        aExtrusionPropVec.push_back( aProp );

        // "Depth" in 1/100mm
        if ( IsProperty( DFF_Prop_c3DExtrudeBackward ) || IsProperty( DFF_Prop_c3DExtrudeForward ) )
        {
            double fBackDepth = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DExtrudeBackward, 1270 * 360 ))) / 360.0;
            double fForeDepth = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DExtrudeForward, 0 ))) / 360.0;
            double fDepth = fBackDepth + fForeDepth;
            double fFraction = fDepth != 0.0 ? fForeDepth / fDepth : 0;
            EnhancedCustomShapeParameterPair aDepthParaPair;
            aDepthParaPair.First.Value <<= fDepth;
            aDepthParaPair.First.Type = EnhancedCustomShapeParameterType::NORMAL;
            aDepthParaPair.Second.Value <<= fFraction;
            aDepthParaPair.Second.Type = EnhancedCustomShapeParameterType::NORMAL;
            aProp.Name = "Depth";
            aProp.Value <<= aDepthParaPair;
            aExtrusionPropVec.push_back( aProp );
        }
        // "Diffusion"
        // ODF default is 0%, MS Office default is 100%. Thus must set value even if default.
        double fDiffusion = 100;
        if ( IsProperty( DFF_Prop_c3DDiffuseAmt ) )
        {
            fDiffusion = static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DDiffuseAmt, 0 ));
            fDiffusion /= 655.36;
        }
        aProp.Name = "Diffusion";
        aProp.Value <<= fDiffusion;
        aExtrusionPropVec.push_back( aProp );

        // "NumberOfLineSegments"
        if ( IsProperty( DFF_Prop_c3DTolerance ) )
        {
            aProp.Name = "NumberOfLineSegments";
            aProp.Value <<= static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DTolerance, 0 ));
            aExtrusionPropVec.push_back( aProp );
        }
        // "LightFace"
        bool bExtrusionLightFace = ( GetPropertyValue( DFF_Prop_fc3DLightFace, 0 ) & 1 ) != 0;
        aProp.Name = "LightFace";
        aProp.Value <<= bExtrusionLightFace;
        aExtrusionPropVec.push_back( aProp );
        // "FirstLightHarsh"
        bool bExtrusionFirstLightHarsh = ( GetPropertyValue( DFF_Prop_fc3DFillHarsh, 0 ) & 2 ) != 0;
        aProp.Name = "FirstLightHarsh";
        aProp.Value <<= bExtrusionFirstLightHarsh;
        aExtrusionPropVec.push_back( aProp );
        // "SecondLightHarsh"
        bool bExtrusionSecondLightHarsh = ( GetPropertyValue( DFF_Prop_fc3DFillHarsh, 0 ) & 1 ) != 0;
        aProp.Name = "SecondLightHarsh";
        aProp.Value <<= bExtrusionSecondLightHarsh;
        aExtrusionPropVec.push_back( aProp );

        // "FirstLightLevel"
        // MS Office default 0x00009470 16.16 FixedPoint, 38000/65536 = 0.5798, ODF default 66%.
        // Thus must set value even if default.
        double fFirstLightLevel = 38000.0;
        if ( IsProperty( DFF_Prop_c3DKeyIntensity ) )
        {
            // value<0 and value>1 are allowed in MS Office. Clamp such in ODF export, not here.
            fFirstLightLevel = static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DKeyIntensity, 0 ));
        }
        fFirstLightLevel /= 655.36;
        aProp.Name = "FirstLightLevel";
        aProp.Value <<= fFirstLightLevel;
        aExtrusionPropVec.push_back( aProp );

        // "SecondLightLevel"
        // MS Office default 0x00009470 16.16 FixedPoint, 38000/65536 = 0.5798, ODF default 66%.
        // Thus must set value even if default.
        double fSecondLightLevel = 38000.0;
        if ( IsProperty( DFF_Prop_c3DFillIntensity ) )
        {
            // value<0 and value>1 are allowed in MS Office. Clamp such in ODF export, not here.
            fSecondLightLevel = static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DFillIntensity, 0 ));
        }
        fSecondLightLevel /= 655.36;
        aProp.Name = "SecondLightLevel";
        aProp.Value <<= fSecondLightLevel;
        aExtrusionPropVec.push_back( aProp );

        // "FirstLightDirection"
        if ( IsProperty( DFF_Prop_c3DKeyX ) || IsProperty( DFF_Prop_c3DKeyY ) || IsProperty( DFF_Prop_c3DKeyZ ) )
        {
            double fLightX = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DKeyX, 50000 )));
            double fLightY = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DKeyY, 0 )));
            double fLightZ = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DKeyZ, 10000 )));
            css::drawing::Direction3D aExtrusionFirstLightDirection( fLightX, fLightY, fLightZ );
            aProp.Name = "FirstLightDirection";
            aProp.Value <<= aExtrusionFirstLightDirection;
            aExtrusionPropVec.push_back( aProp );
        }
        // "SecondLightDirection"
        if ( IsProperty( DFF_Prop_c3DFillX ) || IsProperty( DFF_Prop_c3DFillY ) || IsProperty( DFF_Prop_c3DFillZ ) )
        {
            double fLight2X = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DFillX, sal_uInt32(-50000) )));
            double fLight2Y = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DFillY, 0 )));
            double fLight2Z = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DFillZ, 10000 )));
            css::drawing::Direction3D aExtrusionSecondLightDirection( fLight2X, fLight2Y, fLight2Z );
            aProp.Name = "SecondLightDirection";
            aProp.Value <<= aExtrusionSecondLightDirection;
            aExtrusionPropVec.push_back( aProp );
        }

        // "Metal"
        bool bExtrusionMetal = ( GetPropertyValue( DFF_Prop_fc3DLightFace, 0 ) & 4 ) != 0;
        aProp.Name = "Metal";
        aProp.Value <<= bExtrusionMetal;
        aExtrusionPropVec.push_back( aProp );
        aProp.Name = "MetalType";
        aProp.Value <<= css::drawing::EnhancedCustomShapeMetalType::MetalMSCompatible;
        aExtrusionPropVec.push_back(aProp);

        // "ShadeMode"
        if ( IsProperty( DFF_Prop_c3DRenderMode ) )
        {
            sal_uInt32 nExtrusionRenderMode = GetPropertyValue( DFF_Prop_c3DRenderMode, 0 );
            css::drawing::ShadeMode eExtrusionShadeMode( css::drawing::ShadeMode_FLAT );
            if ( nExtrusionRenderMode == mso_Wireframe )
                eExtrusionShadeMode = css::drawing::ShadeMode_DRAFT;

            aProp.Name = "ShadeMode";
            aProp.Value <<= eExtrusionShadeMode;
            aExtrusionPropVec.push_back( aProp );
        }
        // "RotateAngle" in Degree
        if ( IsProperty( DFF_Prop_c3DXRotationAngle ) || IsProperty( DFF_Prop_c3DYRotationAngle ) )
        {
            double fAngleX = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DXRotationAngle, 0 ))) / 65536.0;
            double fAngleY = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DYRotationAngle, 0 ))) / 65536.0;
            EnhancedCustomShapeParameterPair aRotateAnglePair;
            aRotateAnglePair.First.Value <<= fAngleX;
            aRotateAnglePair.First.Type = EnhancedCustomShapeParameterType::NORMAL;
            aRotateAnglePair.Second.Value <<= fAngleY;
            aRotateAnglePair.Second.Type = EnhancedCustomShapeParameterType::NORMAL;
            aProp.Name = "RotateAngle";
            aProp.Value <<= aRotateAnglePair;
            aExtrusionPropVec.push_back( aProp );
        }

        // "AutoRotationCenter"
        if ( ( GetPropertyValue( DFF_Prop_fc3DFillHarsh, 0 ) & 8 ) == 0 )
        {
            // "RotationCenter"
            if ( IsProperty( DFF_Prop_c3DRotationCenterX ) || IsProperty( DFF_Prop_c3DRotationCenterY ) || IsProperty( DFF_Prop_c3DRotationCenterZ ) )
            {
                // tdf#145904 X- and Y-component is fraction, Z-component in EMU
                css::drawing::Direction3D aRotationCenter(
                    static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DRotationCenterX, 0 ))) / 65536.0,
                    static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DRotationCenterY, 0 ))) / 65536.0,
                    static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DRotationCenterZ, 0 ))) / 360.0 );

                aProp.Name = "RotationCenter";
                aProp.Value <<= aRotationCenter;
                aExtrusionPropVec.push_back( aProp );
            }
        }
        // "Shininess"
        // MS Office default 5, ODF default 50%.
        if ( IsProperty( DFF_Prop_c3DShininess ) )
        {
            double fShininess = static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DShininess, 0 ));
            fShininess *= 10.0; // error in [MS ODRAW] (2021), type is not FixedPoint but long.
            aProp.Name = "Shininess";
            aProp.Value <<= fShininess;
            aExtrusionPropVec.push_back( aProp );
        }

        // "Skew"
        // MS Office angle file value is 16.16 FixedPoint, default 0xFF790000,
        // -8847360/65536=-135, ODF default 45. Thus must set value even if default.
        double fSkewAngle = -135.0;
        // MS Office amount file value is signed integer in range 0xFFFFFF9C to 0x00000064,
        // default 0x00000032, ODF default 50.0
        double fSkewAmount = 50.0;
        if ( IsProperty( DFF_Prop_c3DSkewAmount ) || IsProperty( DFF_Prop_c3DSkewAngle ) )
        {
            fSkewAmount = static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DSkewAmount, 50 ));
            fSkewAngle = static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DSkewAngle, sal::static_int_cast< sal_uInt32 >(-135 * 65536) ));
            fSkewAngle /= 65536.0;
        }
        EnhancedCustomShapeParameterPair aSkewPair;
        aSkewPair.First.Value <<= fSkewAmount;
        aSkewPair.First.Type = EnhancedCustomShapeParameterType::NORMAL;
        aSkewPair.Second.Value <<= fSkewAngle;
        aSkewPair.Second.Type = EnhancedCustomShapeParameterType::NORMAL;
        aProp.Name = "Skew";
        aProp.Value <<= aSkewPair;
        aExtrusionPropVec.push_back( aProp );

        // "Specularity"
        // Type Fixed point 16.16, percent in API
        if ( IsProperty( DFF_Prop_c3DSpecularAmt ) )
        {
            double fSpecularity = static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DSpecularAmt, 0 ));
            fSpecularity /= 655.36;
            aProp.Name = "Specularity";
            aProp.Value <<= fSpecularity;
            aExtrusionPropVec.push_back( aProp );
        }
        // "ProjectionMode"
        ProjectionMode eProjectionMode = (GetPropertyValue( DFF_Prop_fc3DFillHarsh, 0 ) & 4) ? ProjectionMode_PARALLEL : ProjectionMode_PERSPECTIVE;
        aProp.Name = "ProjectionMode";
        aProp.Value <<= eProjectionMode;
        aExtrusionPropVec.push_back( aProp );

        // "ViewPoint" in 1/100mm
        // MS Office default 1250000 EMU=3472.222 Hmm, ODF default 3.5cm
        // Thus must set value even if default.
        double fViewX = 1250000.0 / 360.0;
        double fViewY = -1250000.0 / 360.0;;
        double fViewZ = 9000000.0 / 360.0;
        if ( IsProperty( DFF_Prop_c3DXViewpoint ) || IsProperty( DFF_Prop_c3DYViewpoint ) || IsProperty( DFF_Prop_c3DZViewpoint ) )
        {
            fViewX = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DXViewpoint, 1250000 ))) / 360.0;
            fViewY = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DYViewpoint, sal_uInt32(-1250000) )))/ 360.0;
            fViewZ = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DZViewpoint, 9000000 ))) / 360.0;
        }
        css::drawing::Position3D aExtrusionViewPoint( fViewX, fViewY, fViewZ );
        aProp.Name = "ViewPoint";
        aProp.Value <<= aExtrusionViewPoint;
        aExtrusionPropVec.push_back( aProp );

        // "Origin"
        if ( IsProperty( DFF_Prop_c3DOriginX ) || IsProperty( DFF_Prop_c3DOriginY ) )
        {
            double fOriginX = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DOriginX, 32768 )));
            double fOriginY = static_cast<double>(static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_c3DOriginY, sal_uInt32(-32768) )));
            fOriginX /= 65536;
            fOriginY /= 65536;
            EnhancedCustomShapeParameterPair aOriginPair;
            aOriginPair.First.Value <<= fOriginX;
            aOriginPair.First.Type = EnhancedCustomShapeParameterType::NORMAL;
            aOriginPair.Second.Value <<= fOriginY;
            aOriginPair.Second.Type = EnhancedCustomShapeParameterType::NORMAL;
            aProp.Name = "Origin";
            aProp.Value <<= aOriginPair;
            aExtrusionPropVec.push_back( aProp );
        }
        // "ExtrusionColor"
        bool bExtrusionColor = IsProperty( DFF_Prop_c3DExtrusionColor );    // ( GetPropertyValue( DFF_Prop_fc3DLightFace ) & 2 ) != 0;
        aProp.Name = "Color";
        aProp.Value <<= bExtrusionColor;
        aExtrusionPropVec.push_back( aProp );
        if ( IsProperty( DFF_Prop_c3DExtrusionColor ) )
            rSet.Put( XSecondaryFillColorItem( OUString(), rManager.MSO_CLR_ToColor(
                GetPropertyValue( DFF_Prop_c3DExtrusionColor, 0 ), DFF_Prop_c3DExtrusionColor ) ) );
        // pushing the whole Extrusion element
        aProp.Name = "Extrusion";
        aProp.Value <<= comphelper::containerToSequence(aExtrusionPropVec);
        aPropVec.push_back( aProp );
    }


    // "Equations" PropertySequence element

    if ( IsProperty( DFF_Prop_pFormulas ) )
    {
        sal_uInt16 nNumElem = 0;

        if ( SeekToContent( DFF_Prop_pFormulas, rIn ) )
        {
            sal_uInt16 nNumElemMem = 0;
            sal_uInt16 nElemSize = 8;
            rIn.ReadUInt16( nNumElem ).ReadUInt16( nNumElemMem ).ReadUInt16( nElemSize );
        }
        if ( nNumElem <= 128 )
        {
            uno::Sequence< OUString > aEquations( nNumElem );
            for ( auto& rEquation : asNonConstRange(aEquations) )
            {
                sal_Int16 nP1(0), nP2(0), nP3(0);
                sal_uInt16 nFlags(0);
                rIn.ReadUInt16( nFlags ).ReadInt16( nP1 ).ReadInt16( nP2 ).ReadInt16( nP3 );
                rEquation = EnhancedCustomShape2d::GetEquation( nFlags, nP1, nP2, nP3 );
            }
            // pushing the whole Equations element
            aProp.Name = "Equations";
            aProp.Value <<= aEquations;
            aPropVec.push_back( aProp );
        }
    }


    // "Handles" PropertySequence element

    if ( IsProperty( DFF_Prop_Handles ) )
    {
        sal_uInt16 nNumElem = 0;
        sal_uInt16 nElemSize = 36;

        if ( SeekToContent( DFF_Prop_Handles, rIn ) )
        {
            sal_uInt16 nNumElemMem = 0;
            rIn.ReadUInt16( nNumElem ).ReadUInt16( nNumElemMem ).ReadUInt16( nElemSize );
        }
        bool bImport = false;
        if (nElemSize == 36)
        {
            //sanity check that the stream is long enough to fulfill nNumElem * nElemSize;
            bImport = rIn.remainingSize() / nElemSize >= nNumElem;
        }
        if (bImport)
        {
            uno::Sequence< beans::PropertyValues > aHandles( nNumElem );
            auto aHandlesRange = asNonConstRange(aHandles);
            for (sal_uInt32 i = 0; i < nNumElem; ++i)
            {
                PropVec aHandlePropVec;
                sal_uInt32 nFlagsTmp(0);
                sal_Int32  nPositionX(0), nPositionY(0), nCenterX(0), nCenterY(0), nRangeXMin(0), nRangeXMax(0), nRangeYMin(0), nRangeYMax(0);
                rIn.ReadUInt32( nFlagsTmp )
                   .ReadInt32( nPositionX )
                   .ReadInt32( nPositionY )
                   .ReadInt32( nCenterX )
                   .ReadInt32( nCenterY )
                   .ReadInt32( nRangeXMin )
                   .ReadInt32( nRangeXMax )
                   .ReadInt32( nRangeYMin )
                   .ReadInt32( nRangeYMax );
                SvxMSDffHandleFlags nFlags = static_cast<SvxMSDffHandleFlags>(nFlagsTmp);
                if ( nPositionX == 2 )  // replacing center position with absolute value
                    nPositionX = nCoordWidth / 2;
                if ( nPositionY == 2 )
                    nPositionY = nCoordHeight / 2;
                EnhancedCustomShapeParameterPair aPosition;
                EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aPosition.First,  nPositionX, true, true  );
                EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aPosition.Second, nPositionY, true, false );
                aProp.Name = "Position";
                aProp.Value <<= aPosition;
                aHandlePropVec.push_back( aProp );

                if ( nFlags & SvxMSDffHandleFlags::MIRRORED_X )
                {
                    aProp.Name = "MirroredX";
                    aProp.Value <<= true;
                    aHandlePropVec.push_back( aProp );
                }
                if ( nFlags & SvxMSDffHandleFlags::MIRRORED_Y )
                {
                    aProp.Name = "MirroredY";
                    aProp.Value <<= true;
                    aHandlePropVec.push_back( aProp );
                }
                if ( nFlags & SvxMSDffHandleFlags::SWITCHED )
                {
                    aProp.Name = "Switched";
                    aProp.Value <<= true;
                    aHandlePropVec.push_back( aProp );
                }
                if ( nFlags & SvxMSDffHandleFlags::POLAR )
                {
                    if ( nCenterX == 2 )
                        nCenterX = nCoordWidth / 2;
                    if ( nCenterY == 2 )
                        nCenterY = nCoordHeight / 2;
                    if ((nPositionY >= 0x256 || nPositionY <= 0x107) && i < sizeof(sal_uInt32) * 8)   // position y
                        nAdjustmentsWhichNeedsToBeConverted |= ( 1U << i );
                    EnhancedCustomShapeParameterPair aPolar;
                    EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aPolar.First,  nCenterX, bool( nFlags & SvxMSDffHandleFlags::CENTER_X_IS_SPECIAL ), true  );
                    EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aPolar.Second, nCenterY, bool( nFlags & SvxMSDffHandleFlags::CENTER_Y_IS_SPECIAL ), false );
                    aProp.Name = "Polar";
                    aProp.Value <<= aPolar;
                    aHandlePropVec.push_back( aProp );
                }
                if ( nFlags & SvxMSDffHandleFlags::MAP )
                {
                    if ( nCenterX == 2 )
                        nCenterX = nCoordWidth / 2;
                    if ( nCenterY == 2 )
                        nCenterY = nCoordHeight / 2;
                    EnhancedCustomShapeParameterPair aMap;
                    EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aMap.First,  nCenterX, bool( nFlags & SvxMSDffHandleFlags::CENTER_X_IS_SPECIAL ), true  );
                    EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aMap.Second, nCenterY, bool( nFlags & SvxMSDffHandleFlags::CENTER_Y_IS_SPECIAL ), false );
                    aProp.Name = "Map";
                    aProp.Value <<= aMap;
                    aHandlePropVec.push_back( aProp );
                }
                if ( nFlags & SvxMSDffHandleFlags::RANGE )
                {
                    if ( static_cast<sal_uInt32>(nRangeXMin) != 0x80000000 )
                    {
                        if ( nRangeXMin == 2 )
                            nRangeXMin = nCoordWidth / 2;
                        EnhancedCustomShapeParameter aRangeXMinimum;
                        EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aRangeXMinimum,  nRangeXMin,
                            bool( nFlags & SvxMSDffHandleFlags::RANGE_X_MIN_IS_SPECIAL ), true  );
                        aProp.Name = "RangeXMinimum";
                        aProp.Value <<= aRangeXMinimum;
                        aHandlePropVec.push_back( aProp );
                    }
                    if ( static_cast<sal_uInt32>(nRangeXMax) != 0x7fffffff )
                    {
                        if ( nRangeXMax == 2 )
                            nRangeXMax = nCoordWidth / 2;
                        EnhancedCustomShapeParameter aRangeXMaximum;
                        EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aRangeXMaximum, nRangeXMax,
                            bool( nFlags & SvxMSDffHandleFlags::RANGE_X_MAX_IS_SPECIAL ), false );
                        aProp.Name = "RangeXMaximum";
                        aProp.Value <<= aRangeXMaximum;
                        aHandlePropVec.push_back( aProp );
                    }
                    if ( static_cast<sal_uInt32>(nRangeYMin) != 0x80000000 )
                    {
                        if ( nRangeYMin == 2 )
                            nRangeYMin = nCoordHeight / 2;
                        EnhancedCustomShapeParameter aRangeYMinimum;
                        EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aRangeYMinimum, nRangeYMin,
                            bool( nFlags & SvxMSDffHandleFlags::RANGE_Y_MIN_IS_SPECIAL ), true );
                        aProp.Name = "RangeYMinimum";
                        aProp.Value <<= aRangeYMinimum;
                        aHandlePropVec.push_back( aProp );
                    }
                    if ( static_cast<sal_uInt32>(nRangeYMax) != 0x7fffffff )
                    {
                        if ( nRangeYMax == 2 )
                            nRangeYMax = nCoordHeight / 2;
                        EnhancedCustomShapeParameter aRangeYMaximum;
                        EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aRangeYMaximum, nRangeYMax,
                            bool( nFlags & SvxMSDffHandleFlags::RANGE_Y_MAX_IS_SPECIAL ), false );
                        aProp.Name = "RangeYMaximum";
                        aProp.Value <<= aRangeYMaximum;
                        aHandlePropVec.push_back( aProp );
                    }
                }
                if ( nFlags & SvxMSDffHandleFlags::RADIUS_RANGE )
                {
                    if ( static_cast<sal_uInt32>(nRangeXMin) != 0x7fffffff )
                    {
                        if ( nRangeXMin == 2 )
                            nRangeXMin = nCoordWidth / 2;
                        EnhancedCustomShapeParameter aRadiusRangeMinimum;
                        EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aRadiusRangeMinimum, nRangeXMin,
                            bool( nFlags & SvxMSDffHandleFlags::RANGE_X_MIN_IS_SPECIAL ), true  );
                        aProp.Name = "RadiusRangeMinimum";
                        aProp.Value <<= aRadiusRangeMinimum;
                        aHandlePropVec.push_back( aProp );
                    }
                    if ( static_cast<sal_uInt32>(nRangeXMax) != 0x80000000 )
                    {
                        if ( nRangeXMax == 2 )
                            nRangeXMax = nCoordWidth / 2;
                        EnhancedCustomShapeParameter aRadiusRangeMaximum;
                        EnhancedCustomShape2d::SetEnhancedCustomShapeHandleParameter( aRadiusRangeMaximum, nRangeXMax,
                            bool( nFlags & SvxMSDffHandleFlags::RANGE_X_MAX_IS_SPECIAL ), false );
                        aProp.Name = "RadiusRangeMaximum";
                        aProp.Value <<= aRadiusRangeMaximum;
                        aHandlePropVec.push_back( aProp );
                    }
                }
                aHandlesRange[ i ] = comphelper::containerToSequence(aHandlePropVec);
            }
            // pushing the whole Handles element
            aProp.Name = "Handles";
            aProp.Value <<= aHandles;
            aPropVec.push_back( aProp );
        }
    }
    else
    {
        const mso_CustomShape* pDefCustomShape = GetCustomShapeContent( rObjData.eShapeType );
        if (pDefCustomShape && !pDefCustomShape->pHandles.empty())
        {
            // TODO: This is very similar to EscherPropertyContainer::LookForPolarHandles
            sal_uInt32 i, nCnt = pDefCustomShape->pHandles.size();
            for (i = 0; i < nCnt; i++)
            {
                const SvxMSDffHandle* pData = &pDefCustomShape->pHandles[i];
                if ( pData->nFlags & SvxMSDffHandleFlags::POLAR )
                {
                    if ( ( pData->nPositionY >= 0x256 ) || ( pData->nPositionY <= 0x107 ) )
                        nAdjustmentsWhichNeedsToBeConverted |= ( 1U << i );
                }
            }
        }
    }

    // "Path" PropertySequence element

    {
        PropVec aPathPropVec;

        // "Path/ExtrusionAllowed"
        if ( IsHardAttribute( DFF_Prop_f3DOK ) )
        {
            bool bExtrusionAllowed = ( GetPropertyValue( DFF_Prop_fFillOK, 0 ) & 16 ) != 0;
            aProp.Name = "ExtrusionAllowed";
            aProp.Value <<= bExtrusionAllowed;
            aPathPropVec.push_back( aProp );
        }
        // "Path/ConcentricGradientFillAllowed"
        if ( IsHardAttribute( DFF_Prop_fFillShadeShapeOK ) )
        {
            bool bConcentricGradientFillAllowed = ( GetPropertyValue( DFF_Prop_fFillOK, 0 ) & 2 ) != 0;
            aProp.Name = "ConcentricGradientFillAllowed";
            aProp.Value <<= bConcentricGradientFillAllowed;
            aPathPropVec.push_back( aProp );
        }
        // "Path/TextPathAllowed"
        if ( IsHardAttribute( DFF_Prop_fGtextOK ) || ( GetPropertyValue( DFF_Prop_gtextFStrikethrough, 0 ) & 0x4000 ) )
        {
            bool bTextPathAllowed = ( GetPropertyValue( DFF_Prop_fFillOK, 0 ) & 4 ) != 0;
            aProp.Name = "TextPathAllowed";
            aProp.Value <<= bTextPathAllowed;
            aPathPropVec.push_back( aProp );
        }
        // Path/Coordinates
        if ( IsProperty( DFF_Prop_pVertices ) )
        {
            css::uno::Sequence< css::drawing::EnhancedCustomShapeParameterPair > aCoordinates;
            sal_uInt16 nNumElemVert = 0;
            sal_uInt16 nElemSizeVert = 8;

            if ( SeekToContent( DFF_Prop_pVertices, rIn ) )
            {
                sal_uInt16 nNumElemMemVert = 0;
                rIn.ReadUInt16( nNumElemVert ).ReadUInt16( nNumElemMemVert ).ReadUInt16( nElemSizeVert );
                // If this value is 0xFFF0 then this record is an array of truncated 8 byte elements. Only the 4
                // low-order bytes are recorded
                if (nElemSizeVert == 0xFFF0)
                    nElemSizeVert = 4;
            }
            //sanity check that the stream is long enough to fulfill nNumElem * nElemSize;
            bool bImport = nElemSizeVert && (rIn.remainingSize() / nElemSizeVert >= nNumElemVert);
            if (bImport)
            {
                aCoordinates.realloc( nNumElemVert );
                for (auto& rCoordinate : asNonConstRange(aCoordinates))
                {
                    sal_Int32 nX(0), nY(0);

                    if ( nElemSizeVert == 8 )
                    {
                        rIn.ReadInt32( nX )
                           .ReadInt32( nY );
                    }
                    else
                    {
                        // The mso-spt19 (arc) uses this. But it needs unsigned integer. I don't
                        // know if other shape types also need it. They can be added as necessary.
                        bool bNeedsUnsigned = rObjData.eShapeType == mso_sptArc;
                        if (bNeedsUnsigned)
                        {
                            sal_uInt16 nTmpA(0), nTmpB(0);
                            rIn.ReadUInt16(nTmpA)
                               .ReadUInt16(nTmpB);
                            nX = nTmpA;
                            nY = nTmpB;
                        }
                        else
                        {
                            sal_Int16 nTmpA(0), nTmpB(0);
                            rIn.ReadInt16( nTmpA )
                               .ReadInt16( nTmpB );
                            nX = nTmpA;
                            nY = nTmpB;
                        }
                    }
                    EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( rCoordinate.First, nX );
                    EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( rCoordinate.Second, nY );
                }
            }
            aProp.Name = "Coordinates";
            aProp.Value <<= aCoordinates;
            aPathPropVec.push_back( aProp );
        }
        // Path/Segments
        if ( IsProperty( DFF_Prop_pSegmentInfo ) )
        {
            css::uno::Sequence< css::drawing::EnhancedCustomShapeSegment > aSegments;

            sal_uInt16 nNumElemSeg = 0;

            if ( SeekToContent( DFF_Prop_pSegmentInfo, rIn ) )
            {
                sal_uInt16 nNumElemMemSeg = 0;
                sal_uInt16 nElemSizeSeg = 2;
                rIn.ReadUInt16( nNumElemSeg ).ReadUInt16( nNumElemMemSeg ).ReadUInt16( nElemSizeSeg );
            }
            sal_uInt64 nMaxEntriesPossible = rIn.remainingSize() / sizeof(sal_uInt16);
            if (nNumElemSeg > nMaxEntriesPossible)
            {
                SAL_WARN("filter.ms", "NumElem list is longer than remaining bytes, ppt or parser is wrong");
                nNumElemSeg = nMaxEntriesPossible;
            }
            if ( nNumElemSeg )
            {
                aSegments.realloc( nNumElemSeg );
                for (auto& rSegment : asNonConstRange(aSegments))
                {
                    sal_uInt16 nTmp(0);
                    rIn.ReadUInt16( nTmp );
                    sal_Int16 nCommand = EnhancedCustomShapeSegmentCommand::UNKNOWN;
                    sal_Int16 nCnt = static_cast<sal_Int16>( nTmp & 0x1fff );//Last 13 bits for segment points number
                    switch( nTmp >> 13 )//First 3 bits for command type
                    {
                        case 0x0:
                            nCommand = EnhancedCustomShapeSegmentCommand::LINETO;
                            if ( !nCnt ) nCnt = 1;
                            break;
                        case 0x1:
                            nCommand = EnhancedCustomShapeSegmentCommand::CURVETO;
                            if ( !nCnt ) nCnt = 1;
                            break;
                        case 0x2:
                            nCommand = EnhancedCustomShapeSegmentCommand::MOVETO;
                            if ( !nCnt ) nCnt = 1;
                            break;
                        case 0x3:
                            nCommand = EnhancedCustomShapeSegmentCommand::CLOSESUBPATH;
                            nCnt = 0;
                            break;
                        case 0x4:
                            nCommand = EnhancedCustomShapeSegmentCommand::ENDSUBPATH;
                            nCnt = 0;
                            break;
                        case 0x5:
                        case 0x6:
                        {
                            switch ( ( nTmp >> 8 ) & 0x1f )//5 bits next to command type is for path escape type
                            {
                                case 0x0:
                                {
                                    //It is msopathEscapeExtension which is transformed into LINETO.
                                    //If issue happens, I think this part can be comment so that it will be taken as unknown command.
                                    //When export, origin data will be export without any change.
                                    nCommand = EnhancedCustomShapeSegmentCommand::LINETO;
                                    if ( !nCnt )
                                        nCnt = 1;
                                }
                                break;
                                case 0x1:
                                {
                                    nCommand = EnhancedCustomShapeSegmentCommand::ANGLEELLIPSETO;
                                    nCnt = ( nTmp & 0xff ) / 3;
                                }
                                break;
                                case 0x2:
                                {
                                    nCommand = EnhancedCustomShapeSegmentCommand::ANGLEELLIPSE;
                                    nCnt = ( nTmp & 0xff ) / 3;
                                }
                                break;
                                case 0x3:
                                {
                                    nCommand = EnhancedCustomShapeSegmentCommand::ARCTO;
                                    nCnt = ( nTmp & 0xff ) >> 2;
                                };
                                break;
                                case 0x4:
                                {
                                    nCommand = EnhancedCustomShapeSegmentCommand::ARC;
                                    nCnt = ( nTmp & 0xff ) >> 2;
                                }
                                break;
                                case 0x5:
                                {
                                    nCommand = EnhancedCustomShapeSegmentCommand::CLOCKWISEARCTO;
                                    nCnt = ( nTmp & 0xff ) >> 2;
                                }
                                break;
                                case 0x6:
                                {
                                    nCommand = EnhancedCustomShapeSegmentCommand::CLOCKWISEARC;
                                    nCnt = ( nTmp & 0xff ) >> 2;
                                }
                                break;
                                case 0x7:
                                {
                                    nCommand = EnhancedCustomShapeSegmentCommand::ELLIPTICALQUADRANTX;
                                    nCnt = nTmp & 0xff;
                                }
                                break;
                                case 0x8:
                                {
                                    nCommand = EnhancedCustomShapeSegmentCommand::ELLIPTICALQUADRANTY;
                                    nCnt = nTmp & 0xff;
                                }
                                break;
                                case 0xa: nCommand = EnhancedCustomShapeSegmentCommand::NOFILL; nCnt = 0; break;
                                case 0xb: nCommand = EnhancedCustomShapeSegmentCommand::NOSTROKE; nCnt = 0; break;
                            }
                        }
                        break;
                    }
                    // if the command is unknown, we will store all the data in nCnt, so it will be possible to export without loss
                    if ( nCommand == EnhancedCustomShapeSegmentCommand::UNKNOWN )
                        nCnt = static_cast<sal_Int16>(nTmp);
                    rSegment.Command = nCommand;
                    rSegment.Count = nCnt;
                }
            }
            aProp.Name = "Segments";
            aProp.Value <<= aSegments;
            aPathPropVec.push_back( aProp );
        }
        // Path/StretchX
        if ( IsProperty( DFF_Prop_stretchPointX ) )
        {
            sal_Int32 nStretchX = GetPropertyValue( DFF_Prop_stretchPointX, 0 );
            aProp.Name = "StretchX";
            aProp.Value <<= nStretchX;
            aPathPropVec.push_back( aProp );
        }
        // Path/StretchX
        if ( IsProperty( DFF_Prop_stretchPointY ) )
        {
            sal_Int32 nStretchY = GetPropertyValue( DFF_Prop_stretchPointY, 0 );
            aProp.Name = "StretchY";
            aProp.Value <<= nStretchY;
            aPathPropVec.push_back( aProp );
        }
        // Path/TextFrames
        if ( IsProperty( DFF_Prop_textRectangles ) )
        {
            sal_uInt16 nNumElem = 0;
            sal_uInt16 nElemSize = 16;

            if ( SeekToContent( DFF_Prop_textRectangles, rIn ) )
            {
                sal_uInt16 nNumElemMem = 0;
                rIn.ReadUInt16( nNumElem ).ReadUInt16( nNumElemMem ).ReadUInt16( nElemSize );
            }
            bool bImport = false;
            if (nElemSize == 16)
            {
                //sanity check that the stream is long enough to fulfill nNumElem * nElemSize;
                bImport = rIn.remainingSize() / nElemSize >= nNumElem;
            }
            if (bImport)
            {
                css::uno::Sequence< css::drawing::EnhancedCustomShapeTextFrame > aTextFrames( nNumElem );
                for (auto& rTextFrame : asNonConstRange(aTextFrames))
                {
                    sal_Int32 nLeft(0), nTop(0), nRight(0), nBottom(0);

                    rIn.ReadInt32( nLeft )
                       .ReadInt32( nTop )
                       .ReadInt32( nRight )
                       .ReadInt32( nBottom );

                    EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( rTextFrame.TopLeft.First,  nLeft );
                    EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( rTextFrame.TopLeft.Second, nTop  );
                    EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( rTextFrame.BottomRight.First,  nRight );
                    EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( rTextFrame.BottomRight.Second, nBottom);
                }
                aProp.Name = "TextFrames";
                aProp.Value <<= aTextFrames;
                aPathPropVec.push_back( aProp );
            }
        }
        //Path/GluePoints
        if ( IsProperty( DFF_Prop_connectorPoints ) )
        {
            css::uno::Sequence< css::drawing::EnhancedCustomShapeParameterPair > aGluePoints;
            sal_uInt16 nNumElemVert = 0;
            sal_uInt16 nElemSizeVert = 8;

            if ( SeekToContent( DFF_Prop_connectorPoints, rIn ) )
            {
                sal_uInt16 nNumElemMemVert = 0;
                rIn.ReadUInt16( nNumElemVert ).ReadUInt16( nNumElemMemVert ).ReadUInt16( nElemSizeVert );
                // If this value is 0xFFF0 then this record is an array of truncated 8 byte elements. Only the 4
                // low-order bytes are recorded
                if (nElemSizeVert == 0xFFF0)
                    nElemSizeVert = 4;
            }

            // sanity check that the stream is long enough to fulfill nNumElemVert * nElemSizeVert;
            bool bImport = nElemSizeVert && (rIn.remainingSize() / nElemSizeVert >= nNumElemVert);
            if (bImport)
            {
                aGluePoints.realloc( nNumElemVert );
                for (auto& rGluePoint : asNonConstRange(aGluePoints))
                {
                    sal_Int32 nX(0), nY(0);
                    if ( nElemSizeVert == 8 )
                    {
                        rIn.ReadInt32( nX )
                           .ReadInt32( nY );
                    }
                    else
                    {
                        sal_Int16 nTmpA(0), nTmpB(0);

                        rIn.ReadInt16( nTmpA )
                           .ReadInt16( nTmpB );

                        nX = nTmpA;
                        nY = nTmpB;
                    }
                    EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( rGluePoint.First,  nX );
                    EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( rGluePoint.Second, nY );
                }
            }
            aProp.Name = "GluePoints";
            aProp.Value <<= aGluePoints;
            aPathPropVec.push_back( aProp );
        }
        if ( IsProperty( DFF_Prop_connectorType ) )
        {
            sal_Int16 nGluePointType = static_cast<sal_uInt16>(GetPropertyValue( DFF_Prop_connectorType, 0 ));
            aProp.Name = "GluePointType";
            aProp.Value <<= nGluePointType;
            aPathPropVec.push_back( aProp );
        }
        // pushing the whole Path element
        if ( !aPathPropVec.empty() )
        {
            aProp.Name = "Path";
            aProp.Value <<= comphelper::containerToSequence(aPathPropVec);
            aPropVec.push_back( aProp );
        }
    }

    // "TextPath" PropertySequence element

    bool bTextPathOn = ( GetPropertyValue( DFF_Prop_gtextFStrikethrough, 0 ) & 0x4000 ) != 0;
    if ( bTextPathOn )
    {
        PropVec aTextPathPropVec;

        // TextPath
        aProp.Name = "TextPath";
        aProp.Value <<= bTextPathOn;
        aTextPathPropVec.push_back( aProp );

        // TextPathMode
        bool bTextPathFitPath = ( GetPropertyValue( DFF_Prop_gtextFStrikethrough, 0 ) & 0x100 ) != 0;

        bool bTextPathFitShape;
        if ( IsHardAttribute( DFF_Prop_gtextFStretch ) )
            bTextPathFitShape = ( GetPropertyValue( DFF_Prop_gtextFStrikethrough, 0 ) & 0x400 ) != 0;
        else
        {
            bTextPathFitShape = true;
            switch( rObjData.eShapeType )
            {
                case mso_sptTextArchUpCurve :
                case mso_sptTextArchDownCurve :
                case mso_sptTextCircleCurve :
                case mso_sptTextButtonCurve :
                    bTextPathFitShape = false;
                    break;
                default : break;
            }
        }
        EnhancedCustomShapeTextPathMode eTextPathMode( EnhancedCustomShapeTextPathMode_NORMAL );
        if ( bTextPathFitShape )
            eTextPathMode = EnhancedCustomShapeTextPathMode_SHAPE;
        else if ( bTextPathFitPath )
            eTextPathMode = EnhancedCustomShapeTextPathMode_PATH;
        aProp.Name = "TextPathMode";
        aProp.Value <<= eTextPathMode;
        aTextPathPropVec.push_back( aProp );

        // ScaleX
        bool bTextPathScaleX = ( GetPropertyValue( DFF_Prop_gtextFStrikethrough, 0 ) & 0x40 ) != 0;
        aProp.Name = "ScaleX";
        aProp.Value <<= bTextPathScaleX;
        aTextPathPropVec.push_back( aProp );
        // SameLetterHeights
        bool bSameLetterHeight = ( GetPropertyValue( DFF_Prop_gtextFStrikethrough, 0 ) & 0x80 ) != 0;
        aProp.Name = "SameLetterHeights";
        aProp.Value <<= bSameLetterHeight;
        aTextPathPropVec.push_back( aProp );

        // pushing the whole TextPath element
        aProp.Name = "TextPath";
        aProp.Value <<= comphelper::containerToSequence(aTextPathPropVec);
        aPropVec.push_back( aProp );
    }

    // "AdjustmentValues" // The AdjustmentValues are imported at last, because depending to the type of the
    //////////////////////// handle (POLAR) we will convert the adjustment value from a fixed float to double

    // checking the last used adjustment handle, so we can determine how many handles are to allocate
    sal_uInt32 i = DFF_Prop_adjust10Value;
    while ( ( i >= DFF_Prop_adjustValue ) && !IsProperty( i ) )
        i--;
    sal_Int32 nAdjustmentValues = ( i - DFF_Prop_adjustValue ) + 1;
    if ( nAdjustmentValues )
    {
        uno::Sequence< css::drawing::EnhancedCustomShapeAdjustmentValue > aAdjustmentSeq( nAdjustmentValues );
        auto pAdjustmentSeq = aAdjustmentSeq.getArray();
        while( --nAdjustmentValues >= 0 )
        {
            sal_Int32 nValue = 0;
            beans::PropertyState ePropertyState = beans::PropertyState_DEFAULT_VALUE;
            if ( IsProperty( i ) )
            {
                nValue = GetPropertyValue( i, 0 );
                ePropertyState = beans::PropertyState_DIRECT_VALUE;
            }
            if ( nAdjustmentsWhichNeedsToBeConverted & ( 1 << ( i - DFF_Prop_adjustValue ) ) )
            {
                double fValue = nValue;
                fValue /= 65536;
                pAdjustmentSeq[ nAdjustmentValues ].Value <<= fValue;
            }
            else
                pAdjustmentSeq[ nAdjustmentValues ].Value <<= nValue;
            pAdjustmentSeq[ nAdjustmentValues ].State = ePropertyState;
            i--;
        }
        aProp.Name = "AdjustmentValues";
        aProp.Value <<= aAdjustmentSeq;
        aPropVec.push_back( aProp );
    }

    // creating the whole property set
    rSet.Put( SdrCustomShapeGeometryItem( comphelper::containerToSequence(aPropVec) ) );
}

void DffPropertyReader::ApplyAttributes( SvStream& rIn, SfxItemSet& rSet ) const
{
    DffRecordHeader aHdTemp;
    DffObjData aDffObjTemp( aHdTemp, tools::Rectangle(), 0 );
    ApplyAttributes( rIn, rSet, aDffObjTemp );
}

void DffPropertyReader::ApplyAttributes( SvStream& rIn, SfxItemSet& rSet, DffObjData const & rObjData ) const
{
    bool bHasShadow = false;
    bool bNonZeroShadowOffset = false;

    if ( IsProperty( DFF_Prop_gtextSize ) )
        rSet.Put( SvxFontHeightItem( rManager.ScalePt( GetPropertyValue( DFF_Prop_gtextSize, 0 ) ), 100, EE_CHAR_FONTHEIGHT ) );
    sal_uInt32 nFontAttributes = GetPropertyValue( DFF_Prop_gtextFStrikethrough, 0 );
    if ( nFontAttributes & 0x20 )
        rSet.Put( SvxWeightItem( (nFontAttributes & 0x20) ? WEIGHT_BOLD : WEIGHT_NORMAL, EE_CHAR_WEIGHT ) );
    if ( nFontAttributes & 0x10 )
        rSet.Put( SvxPostureItem( (nFontAttributes & 0x10) ? ITALIC_NORMAL : ITALIC_NONE, EE_CHAR_ITALIC ) );
    if ( nFontAttributes & 0x08 )
        rSet.Put( SvxUnderlineItem( (nFontAttributes & 0x08) ? LINESTYLE_SINGLE : LINESTYLE_NONE, EE_CHAR_UNDERLINE ) );
    if ( nFontAttributes & 0x40 )
        rSet.Put( SvxShadowedItem( (nFontAttributes & 0x40) != 0, EE_CHAR_SHADOW ) );
//    if ( nFontAttributes & 0x02 )
//        rSet.Put( SvxCaseMapItem( nFontAttributes & 0x02 ? SvxCaseMap::SmallCaps : SvxCaseMap::NotMapped ) );
    if ( nFontAttributes & 0x01 )
        rSet.Put( SvxCrossedOutItem( (nFontAttributes & 0x01) ? STRIKEOUT_SINGLE : STRIKEOUT_NONE, EE_CHAR_STRIKEOUT ) );
    if ( IsProperty( DFF_Prop_fillColor ) )
        rSet.Put( XFillColorItem( OUString(), rManager.MSO_CLR_ToColor( GetPropertyValue( DFF_Prop_fillColor, 0 ), DFF_Prop_fillColor ) ) );
    if ( IsProperty( DFF_Prop_shadowColor ) )
        rSet.Put( makeSdrShadowColorItem( rManager.MSO_CLR_ToColor( GetPropertyValue( DFF_Prop_shadowColor, 0 ), DFF_Prop_shadowColor ) ) );
    else
    {
        //The default value for this property is 0x00808080
        rSet.Put( makeSdrShadowColorItem( rManager.MSO_CLR_ToColor( 0x00808080, DFF_Prop_shadowColor ) ) );
    }
    if ( IsProperty( DFF_Prop_shadowOpacity ) )
        rSet.Put( makeSdrShadowTransparenceItem( static_cast<sal_uInt16>( ( 0x10000 - GetPropertyValue( DFF_Prop_shadowOpacity, 0 ) ) / 655 ) ) );
    if ( IsProperty( DFF_Prop_shadowOffsetX ) )
    {
        sal_Int32 nVal = static_cast< sal_Int32 >( GetPropertyValue( DFF_Prop_shadowOffsetX, 0 ) );
        rManager.ScaleEmu( nVal );
        rSet.Put( makeSdrShadowXDistItem( nVal ) );
        bNonZeroShadowOffset = ( nVal > 0 );
    }
    if ( IsProperty( DFF_Prop_shadowOffsetY ) )
    {
        sal_Int32 nVal = static_cast< sal_Int32 >( GetPropertyValue( DFF_Prop_shadowOffsetY, 0 ) );
        rManager.ScaleEmu( nVal );
        rSet.Put( makeSdrShadowYDistItem( nVal ) );
        bNonZeroShadowOffset = ( nVal > 0 );
    }
    if ( IsProperty( DFF_Prop_fshadowObscured ) )
    {
        bHasShadow = ( GetPropertyValue( DFF_Prop_fshadowObscured, 0 ) & 2 ) != 0;
        if ( bHasShadow )
        {
            if ( !IsProperty( DFF_Prop_shadowOffsetX ) )
                rSet.Put( makeSdrShadowXDistItem( 35 ) );
            if ( !IsProperty( DFF_Prop_shadowOffsetY ) )
                rSet.Put( makeSdrShadowYDistItem( 35 ) );
        }
    }
    if ( IsProperty( DFF_Prop_shadowType ) )
    {
        auto eShadowType = GetPropertyValue(DFF_Prop_shadowType, 0);
        if( eShadowType != mso_shadowOffset && !bNonZeroShadowOffset )
        {
            //0.12" == 173 twip == 302 100mm
            sal_uInt32 nDist = rManager.pSdrModel->GetScaleUnit() == MapUnit::MapTwip ? 173: 302;
            rSet.Put( makeSdrShadowXDistItem( nDist ) );
            rSet.Put( makeSdrShadowYDistItem( nDist ) );
        }
    }
    if ( bHasShadow )
    {
        static bool bCheckShadow(false); // loplugin:constvars:ignore

        // #i124477# Found no reason not to set shadow, esp. since it is applied to evtl. existing text
        // and will lead to an error if in PPT someone used text and added the object shadow to the
        // object carrying that text. I found no cases where this leads to problems (the old bugtracker
        // task #160376# from sj is unfortunately no longer available). Keeping the code for now
        // to allow easy fallback when this shows problems in the future
        if(bCheckShadow)
        {
            // #160376# sj: activating shadow only if fill and or linestyle is used
            // this is required because of the latest drawing layer core changes.
            // #i104085# is related to this.
            sal_uInt32 nLineFlags(GetPropertyValue( DFF_Prop_fNoLineDrawDash, 0 ));
            if(!IsHardAttribute( DFF_Prop_fLine ) && !IsCustomShapeStrokedByDefault( rObjData.eShapeType ))
                nLineFlags &= ~0x08;
            sal_uInt32 nFillFlags(GetPropertyValue( DFF_Prop_fNoFillHitTest, 0 ));
            if(!IsHardAttribute( DFF_Prop_fFilled ) && !IsCustomShapeFilledByDefault( rObjData.eShapeType ))
                nFillFlags &= ~0x10;
            if ( nFillFlags & 0x10 )
            {
                auto eMSO_FillType = GetPropertyValue(DFF_Prop_fillType, mso_fillSolid);
                switch( eMSO_FillType )
                {
                    case mso_fillSolid :
                    case mso_fillPattern :
                    case mso_fillTexture :
                    case mso_fillPicture :
                    case mso_fillShade :
                    case mso_fillShadeCenter :
                    case mso_fillShadeShape :
                    case mso_fillShadeScale :
                    case mso_fillShadeTitle :
                    break;
                    default:
                        nFillFlags &=~0x10;         // no fillstyle used
                    break;
                }
            }
            if ( ( ( nLineFlags & 0x08 ) == 0 ) && ( ( nFillFlags & 0x10 ) == 0 ) && ( rObjData.eShapeType != mso_sptPictureFrame ))    // if there is no fillstyle and linestyle
                bHasShadow = false;                                             // we are turning shadow off.
        }

        if ( bHasShadow )
            rSet.Put( makeSdrShadowItem( bHasShadow ) );
    }
    ApplyLineAttributes( rSet, rObjData.eShapeType ); // #i28269#
    ApplyFillAttributes( rIn, rSet, rObjData );
    if ( rObjData.eShapeType != mso_sptNil || IsProperty( DFF_Prop_pVertices ) )
    {
        ApplyCustomShapeGeometryAttributes( rIn, rSet, rObjData );
        ApplyCustomShapeTextAttributes( rSet );
        if ( rManager.GetSvxMSDffSettings() & SVXMSDFF_SETTINGS_IMPORT_EXCEL )
        {
            if ( mnFix16Angle || ( rObjData.nSpFlags & ShapeFlag::FlipV ) )
                CheckAndCorrectExcelTextRotation( rIn, rSet, rObjData );
        }
    }
}

void DffPropertyReader::CheckAndCorrectExcelTextRotation( SvStream& rIn, SfxItemSet& rSet, DffObjData const & rObjData ) const
{
    bool bRotateTextWithShape = rObjData.bRotateTextWithShape;
    if ( rObjData.bOpt2 )        // sj: #158494# is the second property set available ? if then we have to check the xml data of
    {                            // the shape, because the textrotation of Excel 2003 and greater versions is stored there
                                // (upright property of the textbox)
        if ( rManager.pSecPropSet->SeekToContent( DFF_Prop_metroBlob, rIn ) )
        {
            sal_uInt32 nLen = rManager.pSecPropSet->GetPropertyValue( DFF_Prop_metroBlob, 0 );
            if ( nLen )
            {
                css::uno::Sequence< sal_Int8 > aXMLDataSeq( nLen );
                rIn.ReadBytes(aXMLDataSeq.getArray(), nLen);
                css::uno::Reference< css::io::XInputStream > xInputStream
                    ( new ::comphelper::SequenceInputStream( aXMLDataSeq ) );
                try
                {
                    const css::uno::Reference< css::uno::XComponentContext >& xContext( ::comphelper::getProcessComponentContext() );
                    css::uno::Reference< css::embed::XStorage > xStorage
                        ( ::comphelper::OStorageHelper::GetStorageOfFormatFromInputStream(
                            OFOPXML_STORAGE_FORMAT_STRING, xInputStream, xContext, true ) );
                    if ( xStorage.is() )
                    {
                        css::uno::Reference< css::embed::XStorage >
                            xStorageDRS( xStorage->openStorageElement( u"drs"_ustr, css::embed::ElementModes::SEEKABLEREAD ) );
                        if ( xStorageDRS.is() )
                        {
                            css::uno::Reference< css::io::XStream > xShapeXMLStream( xStorageDRS->openStreamElement( u"shapexml.xml"_ustr, css::embed::ElementModes::SEEKABLEREAD ) );
                            if ( xShapeXMLStream.is() )
                            {
                                css::uno::Reference< css::io::XInputStream > xShapeXMLInputStream( xShapeXMLStream->getInputStream() );
                                if ( xShapeXMLInputStream.is() )
                                {
                                    css::uno::Sequence< sal_Int8 > aSeq;
                                    sal_Int32 nBytesRead = xShapeXMLInputStream->readBytes( aSeq, 0x7fffffff );
                                    if ( nBytesRead )
                                    {    // for only one property I spare to use a XML parser at this point, this
                                        // should be enhanced if needed

                                        bRotateTextWithShape = true;    // using the correct xml default
                                        const char* pArry = reinterpret_cast< char* >( aSeq.getArray() );
                                        const char* const pUpright = "upright=";
                                        const char* pEnd = pArry + nBytesRead;
                                        const char* pPtr = pArry;
                                        while( ( pPtr + 12 ) < pEnd )
                                        {
                                            if ( !memcmp( pUpright, pPtr, 8 ) )
                                            {
                                                bRotateTextWithShape = ( pPtr[ 9 ] != '1' ) && ( pPtr[ 9 ] != 't' );
                                                break;
                                            }
                                            else
                                                pPtr++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                catch( css::uno::Exception& )
                {
                }
            }
        }
    }
    if ( bRotateTextWithShape )
        return;

    const css::uno::Any* pAny;
    SdrCustomShapeGeometryItem aGeometryItem(rSet.Get( SDRATTR_CUSTOMSHAPE_GEOMETRY ));
    static constexpr OUString sTextRotateAngle( u"TextRotateAngle"_ustr );
    pAny = aGeometryItem.GetPropertyValueByName( sTextRotateAngle );
    double fExtraTextRotateAngle = 0.0;
    if ( pAny )
        *pAny >>= fExtraTextRotateAngle;

    if ( rManager.mnFix16Angle )
        fExtraTextRotateAngle += toDegrees(mnFix16Angle);
    if ( rObjData.nSpFlags & ShapeFlag::FlipV )
        fExtraTextRotateAngle -= 180.0;

    css::beans::PropertyValue aTextRotateAngle;
    aTextRotateAngle.Name = sTextRotateAngle;
    aTextRotateAngle.Value <<= fExtraTextRotateAngle;
    aGeometryItem.SetPropertyValue( aTextRotateAngle );
    rSet.Put( aGeometryItem );
}


void DffPropertyReader::ImportGradientColor( SfxItemSet& aSet, sal_uInt32 eMSO_FillType, double dTrans , double dBackTrans) const
{
    //MS Focus prop will impact the start and end color position. And AOO does not
    //support this prop. So need some swap for the two color to keep fidelity with AOO and MS shape.
    //So below var is defined.
    sal_Int32 nChgColors = 0;
    sal_Int32 nAngleFix16 = GetPropertyValue( DFF_Prop_fillAngle, 0 );
    if(nAngleFix16 >= 0)
        nChgColors ^= 1;

    //Translate a MS clockwise(+) or count clockwise angle(-) into an AOO count clock wise angle
    Degree10 nAngle( 3600_deg10 - to<Degree10>( Fix16ToAngle(nAngleFix16) ) );
    //Make sure this angle belongs to 0~3600
    while ( nAngle >= 3600_deg10 ) nAngle -= 3600_deg10;
    while ( nAngle < 0_deg10 ) nAngle += 3600_deg10;

    //Rotate angle
    if ( mbRotateGranientFillWithAngle )
    {
        sal_Int32 nRotateAngle = GetPropertyValue( DFF_Prop_Rotation, 0 );
        //nAngle is a clockwise angle. If nRotateAngle is a clockwise angle, then gradient needs to be rotated a little less
        //or it needs to be rotated a little more
        nAngle -= to<Degree10>(Fix16ToAngle(nRotateAngle));
    }
    while ( nAngle >= 3600_deg10 ) nAngle -= 3600_deg10;
    while ( nAngle < 0_deg10 ) nAngle += 3600_deg10;

    css::awt::GradientStyle eGrad = css::awt::GradientStyle_LINEAR;

    sal_Int32 nFocus = GetPropertyValue( DFF_Prop_fillFocus, 0 );
    if ( !nFocus )
        nChgColors ^= 1;
    else if ( nFocus < 0 )//If it is a negative focus, the color will be swapped
    {
        nFocus = o3tl::saturating_toggle_sign(nFocus);
        nChgColors ^= 1;
    }

    if( nFocus > 40 && nFocus < 60 )
    {
        eGrad = css::awt::GradientStyle_AXIAL;//A axial gradient other than linear
        nChgColors ^= 1;
    }
    //if the type is linear or axial, just save focus to nFocusX and nFocusY for export
    //Core function does no need them. They serve for rect gradient(CenterXY).
    sal_uInt16 nFocusX = static_cast<sal_uInt16>(nFocus);
    sal_uInt16 nFocusY = static_cast<sal_uInt16>(nFocus);

    switch( eMSO_FillType )
    {
    case mso_fillShadeShape :
        {
            eGrad = css::awt::GradientStyle_RECT;
            nFocusY = nFocusX = 50;
            nChgColors ^= 1;
        }
        break;
    case mso_fillShadeCenter :
        {
            eGrad = css::awt::GradientStyle_RECT;
            //A MS fillTo prop specifies the relative position of the left boundary
            //of the center rectangle in a concentric shaded fill. Use 100 or 0 to keep fidelity
            nFocusX=(GetPropertyValue( DFF_Prop_fillToRight, 0 )==0x10000) ? 100 : 0;
            nFocusY=(GetPropertyValue( DFF_Prop_fillToBottom,0 )==0x10000) ? 100 : 0;
            nChgColors ^= 1;
        }
        break;
        default: break;
    }

    Color aCol1( rManager.MSO_CLR_ToColor( GetPropertyValue( DFF_Prop_fillColor, sal_uInt32(COL_WHITE) ), DFF_Prop_fillColor ) );
    Color aCol2( rManager.MSO_CLR_ToColor( GetPropertyValue( DFF_Prop_fillBackColor, sal_uInt32(COL_WHITE) ), DFF_Prop_fillBackColor ) );
    if ( nChgColors )
    {
        //Swap start and end color
        Color aZwi( aCol1 );
        aCol1 = aCol2;
        aCol2 = aZwi;
        //Swap two colors' transparency
        std::swap( dTrans, dBackTrans );
    }

    //Construct gradient item
    basegfx::BGradient aGrad(
        basegfx::BColorStops(aCol2.getBColor(), aCol1.getBColor()),
        eGrad, nAngle, nFocusX, nFocusY );
    //Intensity has been merged into color. So here just set is as 100
    aGrad.SetStartIntens( 100 );
    aGrad.SetEndIntens( 100 );
    aSet.Put( XFillGradientItem( OUString(), aGrad ) );
    //Construct transparency item. This item can coordinate with both solid and gradient.
    if ( dTrans < 1.0 || dBackTrans < 1.0 )
    {
        sal_uInt8 nStartCol = static_cast<sal_uInt8>( (1 - dTrans )* 255 );
        sal_uInt8 nEndCol = static_cast<sal_uInt8>( ( 1- dBackTrans ) * 255 );
        aCol1 = Color(nStartCol, nStartCol, nStartCol);
        aCol2 = Color(nEndCol, nEndCol, nEndCol);

        basegfx::BGradient aGrad2(
            basegfx::BColorStops(aCol2.getBColor(), aCol1.getBColor()),
            eGrad, nAngle, nFocusX, nFocusY );
        aSet.Put( XFillFloatTransparenceItem( OUString(), aGrad2 ) );
    }
}


//- Record Manager ----------------------------------------------------------


DffRecordList::DffRecordList( DffRecordList* pList ) :
    nCount                  ( 0 ),
    nCurrent                ( 0 ),
    pPrev                   ( pList )
{
    if ( pList )
        pList->pNext.reset( this );
}

DffRecordList::~DffRecordList()
{
}

DffRecordManager::DffRecordManager() :
    DffRecordList   ( nullptr ),
    pCList          ( static_cast<DffRecordList*>(this) )
{
}

DffRecordManager::DffRecordManager( SvStream& rIn ) :
    DffRecordList   ( nullptr ),
    pCList          ( static_cast<DffRecordList*>(this) )
{
    Consume( rIn );
}

void DffRecordManager::Consume( SvStream& rIn, sal_uInt32 nStOfs )
{
    Clear();
    sal_uInt64 nOldPos = rIn.Tell();
    if ( !nStOfs )
    {
        DffRecordHeader aHd;
        bool bOk = ReadDffRecordHeader( rIn, aHd );
        if (bOk && aHd.nRecVer == DFF_PSFLAG_CONTAINER)
            nStOfs = aHd.GetRecEndFilePos();
    }
    if ( !nStOfs )
        return;

    pCList = this;
    while ( pCList->pNext )
        pCList = pCList->pNext.get();
    while (rIn.good() && ( ( rIn.Tell() + 8 ) <=  nStOfs ))
    {
        if ( pCList->nCount == DFF_RECORD_MANAGER_BUF_SIZE )
            pCList = new DffRecordList( pCList );
        if (!ReadDffRecordHeader(rIn, pCList->mHd[ pCList->nCount ]))
            break;
        bool bSeekSucceeded = pCList->mHd[ pCList->nCount++ ].SeekToEndOfRecord(rIn);
        if (!bSeekSucceeded)
            break;
    }
    rIn.Seek( nOldPos );
}

void DffRecordManager::Clear()
{
    pCList = this;
    pNext.reset();
    nCurrent = 0;
    nCount = 0;
}

DffRecordHeader* DffRecordManager::Current()
{
    DffRecordHeader* pRet = nullptr;
    if ( pCList->nCurrent < pCList->nCount )
        pRet = &pCList->mHd[ pCList->nCurrent ];
    return pRet;
}

DffRecordHeader* DffRecordManager::First()
{
    DffRecordHeader* pRet = nullptr;
    pCList = this;
    if ( pCList->nCount )
    {
        pCList->nCurrent = 0;
        pRet = &pCList->mHd[ 0 ];
    }
    return pRet;
}

DffRecordHeader* DffRecordManager::Next()
{
    DffRecordHeader* pRet = nullptr;
    sal_uInt32 nC = pCList->nCurrent + 1;
    if ( nC < pCList->nCount )
    {
        pCList->nCurrent++;
        pRet = &pCList->mHd[ nC ];
    }
    else if ( pCList->pNext )
    {
        pCList = pCList->pNext.get();
        pCList->nCurrent = 0;
        pRet = &pCList->mHd[ 0 ];
    }
    return pRet;
}

DffRecordHeader* DffRecordManager::Prev()
{
    DffRecordHeader* pRet = nullptr;
    sal_uInt32 nCur = pCList->nCurrent;
    if ( !nCur && pCList->pPrev )
    {
        pCList = pCList->pPrev;
        nCur = pCList->nCount;
    }
    if ( nCur-- )
    {
        pCList->nCurrent = nCur;
        pRet = &pCList->mHd[ nCur ];
    }
    return pRet;
}

DffRecordHeader* DffRecordManager::Last()
{
    DffRecordHeader* pRet = nullptr;
    while ( pCList->pNext )
        pCList = pCList->pNext.get();
    sal_uInt32 nCnt = pCList->nCount;
    if ( nCnt-- )
    {
        pCList->nCurrent = nCnt;
        pRet = &pCList->mHd[ nCnt ];
    }
    return pRet;
}

bool DffRecordManager::SeekToContent( SvStream& rIn, sal_uInt16 nRecId, DffSeekToContentMode eMode )
{
    DffRecordHeader* pHd = GetRecordHeader( nRecId, eMode );
    if ( pHd )
    {
        pHd->SeekToContent( rIn );
        return true;
    }
    else
        return false;
}

DffRecordHeader* DffRecordManager::GetRecordHeader( sal_uInt16 nRecId, DffSeekToContentMode eMode )
{
    sal_uInt32 nOldCurrent = pCList->nCurrent;
    DffRecordList* pOldList = pCList;
    DffRecordHeader* pHd;

    if ( eMode == SEEK_FROM_BEGINNING )
        pHd = First();
    else
        pHd = Next();

    while ( pHd )
    {
        if ( pHd->nRecType == nRecId )
            break;
        pHd = Next();
    }
    if ( !pHd && eMode == SEEK_FROM_CURRENT_AND_RESTART )
    {
        DffRecordHeader* pBreak = &pOldList->mHd[ nOldCurrent ];
        pHd = First();
        if ( pHd )
        {
            while ( pHd != pBreak )
            {
                if ( pHd->nRecType == nRecId )
                    break;
                pHd = Next();
            }
            if ( pHd->nRecType != nRecId )
                pHd = nullptr;
        }
    }
    if ( !pHd )
    {
        pCList = pOldList;
        pOldList->nCurrent = nOldCurrent;
    }
    return pHd;
}


//  private methods


bool CompareSvxMSDffShapeInfoById::operator() (
    std::shared_ptr<SvxMSDffShapeInfo> const& lhs,
    std::shared_ptr<SvxMSDffShapeInfo> const& rhs) const
{
    return lhs->nShapeId < rhs->nShapeId;
}

bool CompareSvxMSDffShapeInfoByTxBxComp::operator() (
    std::shared_ptr<SvxMSDffShapeInfo> const& lhs,
    std::shared_ptr<SvxMSDffShapeInfo> const& rhs) const
{
    return lhs->nTxBxComp < rhs->nTxBxComp;
}

void SvxMSDffManager::Scale( sal_Int32& rVal ) const
{
    if ( bNeedMap )
    {
        if (rVal > nMaxAllowedVal)
        {
            SAL_WARN("filter.ms", "Cannot scale value: " << rVal);
            rVal = SAL_MAX_INT32;
            return;
        }
        else if (rVal < nMinAllowedVal)
        {
            SAL_WARN("filter.ms", "Cannot scale value: " << rVal);
            rVal = SAL_MAX_INT32;
            return;
        }

        rVal = BigMulDiv( rVal, nMapMul, nMapDiv );
    }
}

void SvxMSDffManager::Scale( Point& rPos ) const
{
    rPos.AdjustX(nMapXOfs );
    rPos.AdjustY(nMapYOfs );
    if ( bNeedMap )
    {
        rPos.setX( BigMulDiv( rPos.X(), nMapMul, nMapDiv ) );
        rPos.setY( BigMulDiv( rPos.Y(), nMapMul, nMapDiv ) );
    }
}

void SvxMSDffManager::Scale( Size& rSiz ) const
{
    if ( bNeedMap )
    {
        rSiz.setWidth( BigMulDiv( rSiz.Width(), nMapMul, nMapDiv ) );
        rSiz.setHeight( BigMulDiv( rSiz.Height(), nMapMul, nMapDiv ) );
    }
}

void SvxMSDffManager::ScaleEmu( sal_Int32& rVal ) const
{
    rVal = BigMulDiv( rVal, nEmuMul, nEmuDiv );
}

sal_uInt32 SvxMSDffManager::ScalePt( sal_uInt32 nVal ) const
{
    MapUnit eMap = pSdrModel->GetScaleUnit();
    Fraction aFact( GetMapFactor( MapUnit::MapPoint, eMap ).X() );
    tools::Long aMul = aFact.GetNumerator();
    tools::Long aDiv = aFact.GetDenominator() * 65536;
    aFact = Fraction( aMul, aDiv ); // try again to shorten it
    return BigMulDiv( nVal, aFact.GetNumerator(), aFact.GetDenominator() );
}

sal_Int32 SvxMSDffManager::ScalePoint( sal_Int32 nVal ) const
{
    return BigMulDiv( nVal, nPntMul, nPntDiv );
};

void SvxMSDffManager::SetModel(SdrModel* pModel, tools::Long nApplicationScale)
{
    pSdrModel = pModel;
    if( pModel && (0 < nApplicationScale) )
    {
        // PPT works in units of 576DPI
        // WW on the other side uses twips, i.e. 1440DPI.
        MapUnit eMap = pSdrModel->GetScaleUnit();
        Fraction aFact( GetMapFactor(MapUnit::MapInch, eMap).X() );
        tools::Long nMul=aFact.GetNumerator();
        tools::Long nDiv=aFact.GetDenominator()*nApplicationScale;
        aFact=Fraction(nMul,nDiv); // try again to shorten it
        // For 100TH_MM -> 2540/576=635/144
        // For Twip     -> 1440/576=5/2
        nMapMul  = aFact.GetNumerator();
        nMapDiv  = aFact.GetDenominator();
        bNeedMap = nMapMul!=nMapDiv;

        // MS-DFF-Properties are mostly given in EMU (English Metric Units)
        // 1mm=36000emu, 1twip=635emu
        aFact=GetMapFactor(MapUnit::Map100thMM,eMap).X();
        nMul=aFact.GetNumerator();
        nDiv=aFact.GetDenominator()*360;
        aFact=Fraction(nMul,nDiv); // try again to shorten it
        // For 100TH_MM ->                            1/360
        // For Twip     -> 14,40/(25,4*360)=144/91440=1/635
        nEmuMul=aFact.GetNumerator();
        nEmuDiv=aFact.GetDenominator();

        // And something for typographic Points
        aFact=GetMapFactor(MapUnit::MapPoint,eMap).X();
        nPntMul=aFact.GetNumerator();
        nPntDiv=aFact.GetDenominator();
    }
    else
    {
        pModel = nullptr;
        nMapMul = nMapDiv = nMapXOfs = nMapYOfs = nEmuMul = nEmuDiv = nPntMul = nPntDiv = 0;
        bNeedMap = false;
    }

    if (bNeedMap)
    {
        assert(nMapMul > nMapDiv);

        BigInt aMinVal(SAL_MIN_INT32);
        aMinVal /= nMapMul;
        aMinVal *= nMapDiv;
        nMinAllowedVal = aMinVal;

        BigInt aMaxVal(SAL_MAX_INT32);
        aMaxVal /= nMapMul;
        aMaxVal *= nMapDiv;
        nMaxAllowedVal = aMaxVal;
    }
    else
    {
        nMinAllowedVal = SAL_MIN_INT32;
        nMaxAllowedVal = SAL_MAX_INT32;
    }
}

bool SvxMSDffManager::SeekToShape( SvStream& rSt, SvxMSDffClientData* /* pClientData */, sal_uInt32 nId ) const
{
    bool bRet = false;
    if ( !maFidcls.empty() )
    {
        sal_uInt64 nOldPos = rSt.Tell();
        sal_uInt32 nSec = ( nId >> 10 ) - 1;
        if ( nSec < mnIdClusters )
        {
            OffsetMap::const_iterator it = maDgOffsetTable.find( maFidcls[ nSec ].dgid );
            if ( it != maDgOffsetTable.end() )
            {
                sal_uInt64 nOfs = it->second;
                rSt.Seek( nOfs );
                DffRecordHeader aEscherF002Hd;
                bool bOk = ReadDffRecordHeader( rSt, aEscherF002Hd );
                sal_uLong nEscherF002End = bOk ? aEscherF002Hd.GetRecEndFilePos() : 0;
                while (rSt.good() && rSt.Tell() < nEscherF002End)
                {
                    DffRecordHeader aEscherObjListHd;
                    if (!ReadDffRecordHeader(rSt, aEscherObjListHd))
                        break;
                    if ( aEscherObjListHd.nRecVer != 0xf )
                    {
                        bool bSeekSuccess = aEscherObjListHd.SeekToEndOfRecord(rSt);
                        if (!bSeekSuccess)
                            break;
                    }
                    else if ( aEscherObjListHd.nRecType == DFF_msofbtSpContainer )
                    {
                        DffRecordHeader aShapeHd;
                        if ( SeekToRec( rSt, DFF_msofbtSp, aEscherObjListHd.GetRecEndFilePos(), &aShapeHd ) )
                        {
                            sal_uInt32 nShapeId(0);
                            rSt.ReadUInt32( nShapeId );
                            if ( nId == nShapeId )
                            {
                                aEscherObjListHd.SeekToBegOfRecord( rSt );
                                bRet = true;
                                break;
                            }
                        }
                        bool bSeekSuccess = aEscherObjListHd.SeekToEndOfRecord(rSt);
                        if (!bSeekSuccess)
                            break;
                    }
                }
            }
        }
        if ( !bRet )
            rSt.Seek( nOldPos );
    }
    return bRet;
}

bool SvxMSDffManager::SeekToRec( SvStream& rSt, sal_uInt16 nRecId, sal_uLong nMaxFilePos, DffRecordHeader* pRecHd, sal_uLong nSkipCount )
{
    bool bRet = false;
    sal_uInt64 nOldFPos = rSt.Tell(); // store FilePos to restore it later if necessary
    do
    {
        DffRecordHeader aHd;
        if (!ReadDffRecordHeader(rSt, aHd))
            break;
        if (aHd.nRecLen > nMaxLegalDffRecordLength)
            break;
        if ( aHd.nRecType == nRecId )
        {
            if ( nSkipCount )
                nSkipCount--;
            else
            {
                bRet = true;
                if ( pRecHd != nullptr )
                    *pRecHd = aHd;
                else
                {
                    bool bSeekSuccess = aHd.SeekToBegOfRecord(rSt);
                    if (!bSeekSuccess)
                    {
                        bRet = false;
                        break;
                    }
                }
            }
        }
        if ( !bRet )
        {
            bool bSeekSuccess = aHd.SeekToEndOfRecord(rSt);
            if (!bSeekSuccess)
                break;
        }
    }
    while ( rSt.good() && rSt.Tell() < nMaxFilePos && !bRet );
    if ( !bRet )
        rSt.Seek( nOldFPos );  // restore original FilePos
    return bRet;
}

bool SvxMSDffManager::SeekToRec2( sal_uInt16 nRecId1, sal_uInt16 nRecId2, sal_uLong nMaxFilePos ) const
{
    bool bRet = false;
    sal_uInt64 nOldFPos = rStCtrl.Tell();   // remember FilePos for conditionally later restoration
    do
    {
        DffRecordHeader aHd;
        if (!ReadDffRecordHeader(rStCtrl, aHd))
            break;
        if ( aHd.nRecType == nRecId1 || aHd.nRecType == nRecId2 )
        {
            bRet = true;
            bool bSeekSuccess = aHd.SeekToBegOfRecord(rStCtrl);
            if (!bSeekSuccess)
            {
                bRet = false;
                break;
            }
        }
        if ( !bRet )
        {
            bool bSeekSuccess = aHd.SeekToEndOfRecord(rStCtrl);
            if (!bSeekSuccess)
                break;
        }
    }
    while ( rStCtrl.good() && rStCtrl.Tell() < nMaxFilePos && !bRet );
    if ( !bRet )
        rStCtrl.Seek( nOldFPos ); // restore FilePos
    return bRet;
}


bool SvxMSDffManager::GetColorFromPalette( sal_uInt16 /* nNum */, Color& rColor ) const
{
    // This method has to be overwritten in the class
    // derived for the excel export
    rColor = COL_WHITE;
    return true;
}

// sj: the documentation is not complete, especially in ppt the normal rgb for text
// color is written as 0xfeRRGGBB, this can't be explained by the documentation, nearly
// every bit in the upper code is set -> so there seems to be a special handling for
// ppt text colors, i decided not to fix this in MSO_CLR_ToColor because of possible
// side effects, instead MSO_TEXT_CLR_ToColor is called for PPT text colors, to map
// the color code to something that behaves like the other standard color codes used by
// fill and line color
Color SvxMSDffManager::MSO_TEXT_CLR_ToColor( sal_uInt32 nColorCode ) const
{
    // for text colors: Header is 0xfeRRGGBB
    if ( ( nColorCode & 0xfe000000 ) == 0xfe000000 )
        nColorCode &= 0x00ffffff;
    else
    {
        // for colorscheme colors the color index are the lower three bits of the upper byte
        if ( ( nColorCode & 0xf8000000 ) == 0 ) // this must be a colorscheme index
        {
            nColorCode >>= 24;
            nColorCode |= 0x8000000;
        }
    }
    return MSO_CLR_ToColor( nColorCode );
}

Color SvxMSDffManager::MSO_CLR_ToColor( sal_uInt32 nColorCode, sal_uInt16 nContentProperty ) const
{
    Color aColor( mnDefaultColor );

    // for text colors: Header is 0xfeRRGGBB
    if ( ( nColorCode & 0xfe000000 ) == 0xfe000000 )    // sj: it needs to be checked if 0xfe is used in
        nColorCode &= 0x00ffffff;                       // other cases than ppt text -> if not this code can be removed

    sal_uInt8 nUpper = static_cast<sal_uInt8>( nColorCode >> 24 );

    // sj: below change from 0x1b to 0x19 was done because of i84812 (0x02 -> rgb color),
    // now I have some problems to fix i104685 (there the color value is 0x02000000 which requires
    // a 0x2 scheme color to be displayed properly), the color docu seems to be incomplete
    if( nUpper & 0x19 )      // if( nUpper & 0x1f )
    {
        if( ( nUpper & 0x08 ) || ( ( nUpper & 0x10 ) == 0 ) )
        {
            // SCHEMECOLOR
            if ( !GetColorFromPalette( ( nUpper & 8 ) ? static_cast<sal_uInt16>(nColorCode) : nUpper, aColor ) )
            {
                switch( nContentProperty )
                {
                    case DFF_Prop_pictureTransparent :
                    case DFF_Prop_shadowColor :
                    case DFF_Prop_fillBackColor :
                    case DFF_Prop_fillColor :
                        aColor = COL_WHITE;
                    break;
                    case DFF_Prop_lineColor :
                    {
                        aColor = COL_BLACK;
                    }
                    break;
                }
            }
        }
        else    // SYSCOLOR
        {
            const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();

            sal_uInt16 nParameter = sal_uInt16(( nColorCode >> 16 ) & 0x00ff);  // the HiByte of nParameter is not zero, an exclusive AND is helping :o
            sal_uInt16 nFunctionBits = static_cast<sal_uInt16>( ( nColorCode & 0x00000f00 ) >> 8 );
            sal_uInt16 nAdditionalFlags = static_cast<sal_uInt16>( ( nColorCode & 0x0000f000) >> 8 );
            sal_uInt16 nColorIndex = sal_uInt16(nColorCode & 0x00ff);
            sal_uInt32 nPropColor = 0;

            sal_uInt16  nCProp = 0;

            switch ( nColorIndex )
            {
                case mso_syscolorButtonFace :           aColor = rStyleSettings.GetFaceColor(); break;
                case mso_syscolorWindowText :           aColor = rStyleSettings.GetWindowTextColor(); break;
                case mso_syscolorMenu :                 aColor = rStyleSettings.GetMenuColor(); break;
                case mso_syscolor3DLight :
                case mso_syscolorButtonHighlight :
                case mso_syscolorHighlight :            aColor = rStyleSettings.GetHighlightColor(); break;
                case mso_syscolorHighlightText :        aColor = rStyleSettings.GetHighlightTextColor(); break;
                case mso_syscolorCaptionText :          aColor = rStyleSettings.GetMenuTextColor(); break;
                case mso_syscolorActiveCaption :        aColor = rStyleSettings.GetHighlightColor(); break;
                case mso_syscolorButtonShadow :         aColor = rStyleSettings.GetShadowColor(); break;
                case mso_syscolorButtonText :           aColor = rStyleSettings.GetButtonTextColor(); break;
                case mso_syscolorGrayText :             aColor = rStyleSettings.GetDeactiveColor(); break;
                case mso_syscolorInactiveCaption :      aColor = rStyleSettings.GetDeactiveColor(); break;
                case mso_syscolorInactiveCaptionText :  aColor = rStyleSettings.GetDeactiveColor(); break;
                case mso_syscolorInfoBackground :       aColor = rStyleSettings.GetFaceColor(); break;
                case mso_syscolorInfoText :             aColor = rStyleSettings.GetLabelTextColor(); break;
                case mso_syscolorMenuText :             aColor = rStyleSettings.GetMenuTextColor(); break;
                case mso_syscolorScrollbar :            aColor = rStyleSettings.GetFaceColor(); break;
                case mso_syscolorWindow :               aColor = rStyleSettings.GetWindowColor(); break;
                case mso_syscolorWindowFrame :          aColor = rStyleSettings.GetWindowColor(); break;

                case mso_colorFillColor :
                {
                    nPropColor = GetPropertyValue( DFF_Prop_fillColor, 0xffffff );
                    nCProp = DFF_Prop_fillColor;
                }
                break;
                case mso_colorLineOrFillColor :     // ( use the line color only if there is a line )
                {
                    if ( GetPropertyValue( DFF_Prop_fNoLineDrawDash, 0 ) & 8 )
                    {
                        nPropColor = GetPropertyValue( DFF_Prop_lineColor, 0 );
                        nCProp = DFF_Prop_lineColor;
                    }
                    else
                    {
                        nPropColor = GetPropertyValue( DFF_Prop_fillColor, 0xffffff );
                        nCProp = DFF_Prop_fillColor;
                    }
                }
                break;
                case mso_colorLineColor :
                {
                    nPropColor = GetPropertyValue( DFF_Prop_lineColor, 0 );
                    nCProp = DFF_Prop_lineColor;
                }
                break;
                case mso_colorShadowColor :
                {
                    nPropColor = GetPropertyValue( DFF_Prop_shadowColor, 0x808080 );
                    nCProp = DFF_Prop_shadowColor;
                }
                break;
                case mso_colorThis :                // ( use this color ... )
                {
                    nPropColor = GetPropertyValue( DFF_Prop_fillColor, 0xffffff );  //?????????????
                    nCProp = DFF_Prop_fillColor;
                }
                break;
                case mso_colorFillBackColor :
                {
                    nPropColor = GetPropertyValue( DFF_Prop_fillBackColor, 0xffffff );
                    nCProp = DFF_Prop_fillBackColor;
                }
                break;
                case mso_colorLineBackColor :
                {
                    nPropColor = GetPropertyValue( DFF_Prop_lineBackColor, 0xffffff );
                    nCProp = DFF_Prop_lineBackColor;
                }
                break;
                case mso_colorFillThenLine :        // ( use the fillcolor unless no fill and line )
                {
                    nPropColor = GetPropertyValue( DFF_Prop_fillColor, 0xffffff );  //?????????????
                    nCProp = DFF_Prop_fillColor;
                }
                break;
                case mso_colorIndexMask :           // ( extract the color index ) ?
                {
                    nPropColor = GetPropertyValue( DFF_Prop_fillColor, 0xffffff );  //?????????????
                    nCProp = DFF_Prop_fillColor;
                }
                break;
            }
            if ( nCProp && ( nPropColor & 0x10000000 ) == 0 )       // beware of looping recursive
                aColor = MSO_CLR_ToColor( nPropColor, nCProp );

            if( nAdditionalFlags & 0x80 )           // make color gray
            {
                sal_uInt8 nZwi = aColor.GetLuminance();
                aColor = Color( nZwi, nZwi, nZwi );
            }
            switch( nFunctionBits )
            {
                case 0x01 :     // darken color by parameter
                {
                    aColor.SetRed( sal::static_int_cast< sal_uInt8 >( ( nParameter * aColor.GetRed() ) >> 8 ) );
                    aColor.SetGreen( sal::static_int_cast< sal_uInt8 >( ( nParameter * aColor.GetGreen() ) >> 8 ) );
                    aColor.SetBlue( sal::static_int_cast< sal_uInt8 >( ( nParameter * aColor.GetBlue() ) >> 8 ) );
                }
                break;
                case 0x02 :     // lighten color by parameter
                {
                    sal_uInt16 nInvParameter = ( 0x00ff - nParameter ) * 0xff;
                    aColor.SetRed( sal::static_int_cast< sal_uInt8 >( ( nInvParameter + ( nParameter * aColor.GetRed() ) ) >> 8 ) );
                    aColor.SetGreen( sal::static_int_cast< sal_uInt8 >( ( nInvParameter + ( nParameter * aColor.GetGreen() ) ) >> 8 ) );
                    aColor.SetBlue( sal::static_int_cast< sal_uInt8 >( ( nInvParameter + ( nParameter * aColor.GetBlue() ) ) >> 8 ) );
                }
                break;
                case 0x03 :     // add grey level RGB(p,p,p)
                {
                    sal_Int16 nR = static_cast<sal_Int16>(aColor.GetRed()) + static_cast<sal_Int16>(nParameter);
                    sal_Int16 nG = static_cast<sal_Int16>(aColor.GetGreen()) + static_cast<sal_Int16>(nParameter);
                    sal_Int16 nB = static_cast<sal_Int16>(aColor.GetBlue()) + static_cast<sal_Int16>(nParameter);
                    if ( nR > 0x00ff )
                        nR = 0x00ff;
                    if ( nG > 0x00ff )
                        nG = 0x00ff;
                    if ( nB > 0x00ff )
                        nB = 0x00ff;
                    aColor = Color( static_cast<sal_uInt8>(nR), static_cast<sal_uInt8>(nG), static_cast<sal_uInt8>(nB) );
                }
                break;
                case 0x04 :     // subtract grey level RGB(p,p,p)
                {
                    sal_Int16 nR = static_cast<sal_Int16>(aColor.GetRed()) - static_cast<sal_Int16>(nParameter);
                    sal_Int16 nG = static_cast<sal_Int16>(aColor.GetGreen()) - static_cast<sal_Int16>(nParameter);
                    sal_Int16 nB = static_cast<sal_Int16>(aColor.GetBlue()) - static_cast<sal_Int16>(nParameter);
                    if ( nR < 0 )
                        nR = 0;
                    if ( nG < 0 )
                        nG = 0;
                    if ( nB < 0 )
                        nB = 0;
                    aColor = Color( static_cast<sal_uInt8>(nR), static_cast<sal_uInt8>(nG), static_cast<sal_uInt8>(nB) );
                }
                break;
                case 0x05 :     // subtract from gray level RGB(p,p,p)
                {
                    sal_Int16 nR = static_cast<sal_Int16>(nParameter) - static_cast<sal_Int16>(aColor.GetRed());
                    sal_Int16 nG = static_cast<sal_Int16>(nParameter) - static_cast<sal_Int16>(aColor.GetGreen());
                    sal_Int16 nB = static_cast<sal_Int16>(nParameter) - static_cast<sal_Int16>(aColor.GetBlue());
                    if ( nR < 0 )
                        nR = 0;
                    if ( nG < 0 )
                        nG = 0;
                    if ( nB < 0 )
                        nB = 0;
                    aColor = Color( static_cast<sal_uInt8>(nR), static_cast<sal_uInt8>(nG), static_cast<sal_uInt8>(nB) );
                }
                break;
                case 0x06 :     // per component: black if < p, white if >= p
                {
                    aColor.SetRed( aColor.GetRed() < nParameter ? 0x00 : 0xff );
                    aColor.SetGreen( aColor.GetGreen() < nParameter ? 0x00 : 0xff );
                    aColor.SetBlue( aColor.GetBlue() < nParameter ? 0x00 : 0xff );
                }
                break;
            }
            if ( nAdditionalFlags & 0x40 )                  // top-bit invert
                aColor = Color( aColor.GetRed() ^ 0x80, aColor.GetGreen() ^ 0x80, aColor.GetBlue() ^ 0x80 );

            if ( nAdditionalFlags & 0x20 )                  // invert color
                aColor = Color(0xff - aColor.GetRed(), 0xff - aColor.GetGreen(), 0xff - aColor.GetBlue());
        }
    }
    else if ( ( nUpper & 4 ) && ( ( nColorCode & 0xfffff8 ) == 0 ) )
    {   // case of nUpper == 4 powerpoint takes this as argument for a colorschemecolor
        GetColorFromPalette( nUpper, aColor );
    }
    else    // attributed hard, maybe with hint to SYSTEMRGB
        aColor = Color( static_cast<sal_uInt8>(nColorCode), static_cast<sal_uInt8>( nColorCode >> 8 ), static_cast<sal_uInt8>( nColorCode >> 16 ) );
    return aColor;
}

void SvxMSDffManager::ReadObjText( SvStream& rStream, SdrObject* pObj )
{
    DffRecordHeader aRecHd;
    if (!ReadDffRecordHeader(rStream, aRecHd))
        return;
    if( aRecHd.nRecType != DFF_msofbtClientTextbox && aRecHd.nRecType != 0x1022 )
        return;

    while (rStream.good() && rStream.Tell() < aRecHd.GetRecEndFilePos())
    {
        DffRecordHeader aHd;
        if (!ReadDffRecordHeader(rStream, aHd))
            break;
        switch( aHd.nRecType )
        {
            case DFF_PST_TextBytesAtom:
            case DFF_PST_TextCharsAtom:
                {
                    bool bUniCode = ( aHd.nRecType == DFF_PST_TextCharsAtom );
                    sal_uInt32 nBytes = aHd.nRecLen;
                    OUString aStr = MSDFFReadZString( rStream, nBytes, bUniCode );
                    ReadObjText( aStr, pObj );
                }
                break;
            default:
                break;
        }
        bool bSeekSuccess = aHd.SeekToEndOfRecord(rStream);
        if (!bSeekSuccess)
            break;
    }
}

// sj: I just want to set a string for a text object that may contain multiple
// paragraphs. If I now take a look at the following code I get the impression that
// our outliner is too complicate to be used properly,
void SvxMSDffManager::ReadObjText( const OUString& rText, SdrObject* pObj )
{
    SdrTextObj* pText = DynCastSdrTextObj( pObj  );
    if ( !pText )
        return;

    SdrOutliner& rOutliner = pText->ImpGetDrawOutliner();
    rOutliner.Init( OutlinerMode::TextObject );

    bool bOldUpdateMode = rOutliner.SetUpdateLayout( false );
    rOutliner.SetVertical( pText->IsVerticalWriting() );

    sal_Int32 nParaIndex = 0;
    sal_Int32 nParaSize;
    const sal_Unicode* pBuf = rText.getStr();
    const sal_Unicode* pEnd = rText.getStr() + rText.getLength();

    while( pBuf < pEnd )
    {
        const sal_Unicode* pCurrent = pBuf;

        for ( nParaSize = 0; pBuf < pEnd; )
        {
            sal_Unicode nChar = *pBuf++;
            if ( nChar == 0xa )
            {
                if ( ( pBuf < pEnd ) && ( *pBuf == 0xd ) )
                    pBuf++;
                break;
            }
            else if ( nChar == 0xd )
            {
                if ( ( pBuf < pEnd ) && ( *pBuf == 0xa ) )
                    pBuf++;
                break;
            }
            else
                ++nParaSize;
        }
        ESelection aSelection(nParaIndex, 0);
        OUString aParagraph( pCurrent, nParaSize );
        if ( !nParaIndex && aParagraph.isEmpty() )              // SJ: we are crashing if the first paragraph is empty ?
            aParagraph += " ";                   // otherwise these two lines can be removed.
        rOutliner.Insert( aParagraph, nParaIndex );
        rOutliner.SetParaAttribs( nParaIndex, rOutliner.GetEmptyItemSet() );

        SfxItemSet aParagraphAttribs( rOutliner.GetEmptyItemSet() );
        if (!aSelection.start.nIndex)
            aParagraphAttribs.Put( SfxBoolItem( EE_PARA_BULLETSTATE, false ) );
        aSelection.start.nIndex = 0;
        rOutliner.QuickSetAttribs( aParagraphAttribs, aSelection );
        nParaIndex++;
    }
    std::optional<OutlinerParaObject> pNewText = rOutliner.CreateParaObject();
    rOutliner.Clear();
    rOutliner.SetUpdateLayout( bOldUpdateMode );
    pText->SetOutlinerParaObject( std::move(pNewText) );
    // tdf#143315: restore stylesheet applied to Outliner's nodes when SdrTextObj initializes
    // its attributes, but removed by Outliner::Init, which calls Outliner::Clear.
    pText->SetStyleSheet(pText->GetStyleSheet(), true);
}

//static
OUString SvxMSDffManager::MSDFFReadZString(SvStream& rIn,
    sal_uInt32 nLen, bool bUniCode)
{
    if (!nLen)
        return OUString();

    OUString sBuf;

    if( bUniCode )
        sBuf = read_uInt16s_ToOUString(rIn, nLen/2);
    else
        sBuf = read_uInt8s_ToOUString(rIn, nLen, RTL_TEXTENCODING_MS_1252);

    return comphelper::string::stripEnd(sBuf, 0);
}

static Size lcl_GetPrefSize(const Graphic& rGraf, const MapMode& aWanted)
{
    MapMode aPrefMapMode(rGraf.GetPrefMapMode());
    if (aPrefMapMode == aWanted)
        return rGraf.GetPrefSize();
    Size aRetSize;
    if (aPrefMapMode.GetMapUnit() == MapUnit::MapPixel)
    {
        aRetSize = Application::GetDefaultDevice()->PixelToLogic(
            rGraf.GetPrefSize(), aWanted);
    }
    else
    {
        aRetSize = OutputDevice::LogicToLogic(
            rGraf.GetPrefSize(), rGraf.GetPrefMapMode(), aWanted);
    }
    return aRetSize;
}

// sj: if the parameter pSet is null, then the resulting crop bitmap will be stored in rGraf,
// otherwise rGraf is untouched and pSet is used to store the corresponding SdrGrafCropItem
static void lcl_ApplyCropping( const DffPropSet& rPropSet, SfxItemSet* pSet, Graphic& rGraf )
{
    sal_Int32 nCropTop      = static_cast<sal_Int32>(rPropSet.GetPropertyValue( DFF_Prop_cropFromTop, 0 ));
    sal_Int32 nCropBottom   = static_cast<sal_Int32>(rPropSet.GetPropertyValue( DFF_Prop_cropFromBottom, 0 ));
    sal_Int32 nCropLeft     = static_cast<sal_Int32>(rPropSet.GetPropertyValue( DFF_Prop_cropFromLeft, 0 ));
    sal_Int32 nCropRight    = static_cast<sal_Int32>(rPropSet.GetPropertyValue( DFF_Prop_cropFromRight, 0 ));

    if( !(nCropTop || nCropBottom || nCropLeft || nCropRight) )
        return;

    double      fFactor;
    Size        aCropSize;
    BitmapEx    aCropBitmap;
    sal_uInt32  nTop( 0 ),  nBottom( 0 ), nLeft( 0 ), nRight( 0 );

    // Cropping has to be applied on a loaded graphic.
    rGraf.makeAvailable();

    if ( pSet ) // use crop attributes ?
        aCropSize = lcl_GetPrefSize(rGraf, MapMode(MapUnit::Map100thMM));
    else
    {
        aCropBitmap = rGraf.GetBitmapEx();
        aCropSize = aCropBitmap.GetSizePixel();
    }
    if ( nCropTop )
    {
        fFactor = static_cast<double>(nCropTop) / 65536.0;
        nTop = static_cast<sal_uInt32>( ( static_cast<double>( aCropSize.Height() + 1 ) * fFactor ) + 0.5 );
    }
    if ( nCropBottom )
    {
        fFactor = static_cast<double>(nCropBottom) / 65536.0;
        nBottom = static_cast<sal_uInt32>( ( static_cast<double>( aCropSize.Height() + 1 ) * fFactor ) + 0.5 );
    }
    if ( nCropLeft )
    {
        fFactor = static_cast<double>(nCropLeft) / 65536.0;
        nLeft = static_cast<sal_uInt32>( ( static_cast<double>( aCropSize.Width() + 1 ) * fFactor ) + 0.5 );
    }
    if ( nCropRight )
    {
        fFactor = static_cast<double>(nCropRight) / 65536.0;
        nRight = static_cast<sal_uInt32>( ( static_cast<double>( aCropSize.Width() + 1 ) * fFactor ) + 0.5 );
    }
    if ( pSet ) // use crop attributes ?
        pSet->Put( SdrGrafCropItem( nLeft, nTop, nRight, nBottom ) );
    else
    {
        tools::Rectangle aCropRect( nLeft, nTop, aCropSize.Width() - nRight, aCropSize.Height() - nBottom );
        aCropBitmap.Crop( aCropRect );
        rGraf = aCropBitmap;
    }
}

rtl::Reference<SdrObject> SvxMSDffManager::ImportGraphic( SvStream& rSt, SfxItemSet& rSet, const DffObjData& rObjData )
{
    rtl::Reference<SdrObject> pRet;
    OUString    aLinkFileName;
    tools::Rectangle   aVisArea;

    auto eFlags = GetPropertyValue(DFF_Prop_pibFlags, mso_blipflagDefault);
    sal_uInt32 nBlipId = GetPropertyValue( DFF_Prop_pib, 0 );
    bool bGrfRead = false,

    // Graphic linked
    bLinkGrf = 0 != ( eFlags & mso_blipflagLinkToFile );
    {
        OUString aFileName;
        Graphic aGraf;  // be sure this graphic is deleted before swapping out
        if( SeekToContent( DFF_Prop_pibName, rSt ) )
            aFileName = MSDFFReadZString( rSt, GetPropertyValue( DFF_Prop_pibName, 0 ), true );

        //   AND, OR the following:
        if( !( eFlags & mso_blipflagDoNotSave ) ) // Graphic embedded
        {
            bGrfRead = GetBLIP( nBlipId, aGraf, &aVisArea );
            if ( !bGrfRead )
            {
                /*
                Still no luck, let's look at the end of this record for a FBSE pool,
                this fallback is a specific case for how word does it sometimes
                */
                bool bOk = rObjData.rSpHd.SeekToEndOfRecord( rSt );
                DffRecordHeader aHd;
                if (bOk)
                {
                    bOk = ReadDffRecordHeader(rSt, aHd);
                }
                if (bOk && DFF_msofbtBSE == aHd.nRecType)
                {
                    const sal_uInt8 nSkipBLIPLen = 20;
                    const sal_uInt8 nSkipShapePos = 4;
                    const sal_uInt8 nSkipBLIP = 4;
                    const sal_uLong nSkip =
                        nSkipBLIPLen + 4 + nSkipShapePos + 4 + nSkipBLIP;

                    if (nSkip <= aHd.nRecLen)
                    {
                        rSt.SeekRel(nSkip);
                        if (ERRCODE_NONE == rSt.GetError())
                            bGrfRead = GetBLIPDirect( rSt, aGraf, &aVisArea );
                    }
                }
            }
        }
        if ( bGrfRead )
        {
            // the writer is doing its own cropping, so this part affects only impress and calc,
            // unless we're inside a group, in which case writer doesn't crop either
            if (( GetSvxMSDffSettings() & SVXMSDFF_SETTINGS_CROP_BITMAPS ) || rObjData.nCalledByGroup != 0 )
                lcl_ApplyCropping( *this, !bool( rObjData.nSpFlags & ShapeFlag::OLEShape ) ? &rSet : nullptr, aGraf );

            if ( IsProperty( DFF_Prop_pictureTransparent ) )
            {
                sal_uInt32 nTransColor = GetPropertyValue( DFF_Prop_pictureTransparent, 0 );

                if ( aGraf.GetType() == GraphicType::Bitmap )
                {
                    BitmapEx aBitmapEx( aGraf.GetBitmapEx() );
                    aBitmapEx.CombineMaskOr( MSO_CLR_ToColor( nTransColor, DFF_Prop_pictureTransparent ), 9 );
                    aGraf = aBitmapEx;
                }
            }

            sal_Int32 nContrast = GetPropertyValue( DFF_Prop_pictureContrast, 0x10000 );
            /*
            0x10000 is msoffice 50%
            < 0x10000 is in units of 1/50th of 0x10000 per 1%
            > 0x10000 is in units where
            a msoffice x% is stored as 50/(100-x) * 0x10000

            plus, a (ui) microsoft % ranges from 0 to 100, OOO
            from -100 to 100, so also normalize into that range
            */
            if ( nContrast > 0x10000 )
            {
                double fX = nContrast;
                fX /= 0x10000;
                fX /= 51;   // 50 + 1 to round
                fX = 1/fX;
                nContrast = static_cast<sal_Int32>(fX);
                nContrast -= 100;
                nContrast = -nContrast;
                nContrast = (nContrast-50)*2;
            }
            else if ( nContrast == 0x10000 )
                nContrast = 0;
            else
            {
                if (o3tl::checked_multiply<sal_Int32>(nContrast, 101, nContrast))  //100 + 1 to round
                {
                    SAL_WARN("filter.ms", "bad Contrast value:" << nContrast);
                    nContrast = 0;
                }
                else
                {
                    nContrast /= 0x10000;
                    nContrast -= 100;
                }
            }
            sal_Int16   nBrightness     = static_cast<sal_Int16>( static_cast<sal_Int32>(GetPropertyValue( DFF_Prop_pictureBrightness, 0 )) / 327 );
            sal_Int32   nGamma          = GetPropertyValue( DFF_Prop_pictureGamma, 0x10000 );
            GraphicDrawMode eDrawMode   = GraphicDrawMode::Standard;
            switch ( GetPropertyValue( DFF_Prop_pictureActive, 0 ) & 6 )
            {
                case 4 : eDrawMode = GraphicDrawMode::Greys; break;
                case 6 : eDrawMode = GraphicDrawMode::Mono; break;
                case 0 :
                {
                    //office considers the converted values of (in OOo) 70 to be the
                    //"watermark" values, which can vary slightly due to rounding from the
                    //above values
                    if (( nContrast == -70 ) && ( nBrightness == 70 ))
                    {
                        nContrast = 0;
                        nBrightness = 0;
                        eDrawMode = GraphicDrawMode::Watermark;
                    };
                }
                break;
            }

            if ( nContrast || nBrightness || ( nGamma != 0x10000 ) || ( eDrawMode != GraphicDrawMode::Standard ) )
            {
                // MSO uses a different algorithm for contrast+brightness, LO applies contrast before brightness,
                // while MSO apparently applies half of brightness before contrast and half after. So if only
                // contrast or brightness need to be altered, the result is the same, but if both are involved,
                // there's no way to map that, so just force a conversion of the image.
                bool needsConversion = nContrast != 0 && nBrightness != 0;
                if ( !bool(rObjData.nSpFlags & ShapeFlag::OLEShape) && !needsConversion )
                {
                    if ( nBrightness )
                        rSet.Put( SdrGrafLuminanceItem( nBrightness ) );
                    if ( nContrast )
                        rSet.Put( SdrGrafContrastItem( static_cast<sal_Int16>(nContrast) ) );
                    if ( nGamma != 0x10000 )
                        rSet.Put( SdrGrafGamma100Item( nGamma / 655 ) );
                    if ( eDrawMode != GraphicDrawMode::Standard )
                        rSet.Put( SdrGrafModeItem( eDrawMode ) );
                }
                else
                {
                    if ( eDrawMode == GraphicDrawMode::Watermark )
                    {
                        nContrast = 60;
                        nBrightness = 70;
                        eDrawMode = GraphicDrawMode::Standard;
                    }
                    switch ( aGraf.GetType() )
                    {
                        case GraphicType::Bitmap :
                        {
                            BitmapEx    aBitmapEx( aGraf.GetBitmapEx() );
                            if ( nBrightness || nContrast || ( nGamma != 0x10000 ) )
                                aBitmapEx.Adjust( nBrightness, static_cast<sal_Int16>(nContrast), 0, 0, 0, static_cast<double>(nGamma) / 0x10000, false, true );
                            if ( eDrawMode == GraphicDrawMode::Greys )
                                aBitmapEx.Convert( BmpConversion::N8BitGreys );
                            else if ( eDrawMode == GraphicDrawMode::Mono )
                                aBitmapEx.Convert( BmpConversion::N1BitThreshold );
                            aGraf = aBitmapEx;

                        }
                        break;

                        case GraphicType::GdiMetafile :
                        {
                            GDIMetaFile aGdiMetaFile( aGraf.GetGDIMetaFile() );
                            if ( nBrightness || nContrast || ( nGamma != 0x10000 ) )
                                aGdiMetaFile.Adjust( nBrightness, static_cast<sal_Int16>(nContrast), 0, 0, 0, static_cast<double>(nGamma) / 0x10000, false, true );
                            if ( eDrawMode == GraphicDrawMode::Greys )
                                aGdiMetaFile.Convert( MtfConversion::N8BitGreys );
                            else if ( eDrawMode == GraphicDrawMode::Mono )
                                aGdiMetaFile.Convert( MtfConversion::N1BitThreshold );
                            aGraf = aGdiMetaFile;
                        }
                        break;
                        default: break;
                    }
                }
            }
        }

        // should it be an OLE object?
        if( bGrfRead && !bLinkGrf && IsProperty( DFF_Prop_pictureId ) )
        {
            // TODO/LATER: in future probably the correct aspect should be provided here
            // #i32596# - pass <nCalledByGroup> to method
            pRet = ImportOLE( GetPropertyValue( DFF_Prop_pictureId, 0 ), aGraf, rObjData.aBoundRect, aVisArea, rObjData.nCalledByGroup );
        }
        if( !pRet )
        {
            pRet = new SdrGrafObj(*pSdrModel);
            if( bGrfRead )
                static_cast<SdrGrafObj*>(pRet.get())->SetGraphic( aGraf );

            if( bLinkGrf && !bGrfRead )     // sj: #i55484# if the graphic was embedded ( bGrfRead == true ) then
            {                               // we do not need to set a link. TODO: not to lose the information where the graphic is linked from
                INetURLObject aAbsURL;
                if ( !INetURLObject( maBaseURL ).GetNewAbsURL( aFileName, &aAbsURL ) )
                {
                    OUString aValidURL;
                    if( osl::FileBase::getFileURLFromSystemPath( aFileName, aValidURL ) == osl::FileBase::E_None )
                        aAbsURL = INetURLObject( aValidURL );
                }
                if( aAbsURL.GetProtocol() != INetProtocol::NotValid )
                {
                    aLinkFileName = aAbsURL.GetMainURL( INetURLObject::DecodeMechanism::ToIUri );
                }
                else
                    aLinkFileName = aFileName;
            }
        }

        // set the size from BLIP if there is one
        if ( bGrfRead && !aVisArea.IsEmpty() )
            pRet->SetBLIPSizeRectangle( aVisArea );

        if (pRet->GetName().isEmpty())                   // SJ 22.02.00 : PPT OLE IMPORT:
        {                                                // name is already set in ImportOLE !!
            // JP 01.12.99: SetName before SetModel - because in the other order the Bug 70098 is active
            if ( ( eFlags & mso_blipflagType ) != mso_blipflagComment )
            {
                INetURLObject aURL;
                aURL.SetSmartURL( aFileName );
                pRet->SetName( aURL.getBase() );
            }
            else
                pRet->SetName( aFileName );
        }
    }
    pRet->NbcSetLogicRect( rObjData.aBoundRect );

    if (SdrGrafObj* pGrafObj = dynamic_cast<SdrGrafObj*>(pRet.get()))
    {
        if( aLinkFileName.getLength() )
        {
            pGrafObj->SetGraphicLink( aLinkFileName );
            Graphic aGraphic(pGrafObj->GetGraphic());
            aGraphic.setOriginURL(aLinkFileName);
        }

        if ( bLinkGrf && !bGrfRead )
        {
            Graphic aGraf(pGrafObj->GetGraphic());
            lcl_ApplyCropping( *this, &rSet, aGraf );
        }
    }

    return pRet;
}

// PptSlidePersistEntry& rPersistEntry, SdPage* pPage
rtl::Reference<SdrObject> SvxMSDffManager::ImportObj( SvStream& rSt, SvxMSDffClientData& rClientData,
    tools::Rectangle& rClientRect, const tools::Rectangle& rGlobalChildRect, int nCalledByGroup, sal_Int32* pShapeId )
{
    rtl::Reference<SdrObject> pRet;
    DffRecordHeader aObjHd;
    bool bOk = ReadDffRecordHeader(rSt, aObjHd);
    if (bOk && aObjHd.nRecType == DFF_msofbtSpgrContainer)
    {
        pRet = ImportGroup( aObjHd, rSt, rClientData, rClientRect, rGlobalChildRect, nCalledByGroup, pShapeId );
    }
    else if (bOk && aObjHd.nRecType == DFF_msofbtSpContainer)
    {
        pRet = ImportShape( aObjHd, rSt, rClientData, rClientRect, rGlobalChildRect, nCalledByGroup, pShapeId );
    }
    aObjHd.SeekToBegOfRecord( rSt );    // restore FilePos
    return pRet;
}

rtl::Reference<SdrObject> SvxMSDffManager::ImportGroup( const DffRecordHeader& rHd, SvStream& rSt, SvxMSDffClientData& rClientData,
                                            tools::Rectangle& rClientRect, const tools::Rectangle& rGlobalChildRect,
                                                int nCalledByGroup, sal_Int32* pShapeId )
{
    if( pShapeId )
        *pShapeId = 0;

    if (!rHd.SeekToContent(rSt))
        return nullptr;

    rtl::Reference<SdrObject> xRet;

    DffRecordHeader aRecHd;     // the first atom has to be the SpContainer for the GroupObject
    bool bOk = ReadDffRecordHeader(rSt, aRecHd);
    if (bOk && aRecHd.nRecType == DFF_msofbtSpContainer)
    {
        mnFix16Angle = 0_deg100;
        if (!aRecHd.SeekToBegOfRecord(rSt))
            return xRet;
        xRet = ImportObj(rSt, rClientData, rClientRect, rGlobalChildRect, nCalledByGroup + 1, pShapeId);
        if (xRet)
        {
            Degree100 nGroupRotateAngle(0);
            ShapeFlag nSpFlags = nGroupShapeFlags;
            nGroupRotateAngle = mnFix16Angle;

            tools::Rectangle aClientRect( rClientRect );

            tools::Rectangle aGlobalChildRect;
            if ( !nCalledByGroup || rGlobalChildRect.IsEmpty() )
                aGlobalChildRect = GetGlobalChildAnchor( rHd, rSt, aClientRect );
            else
                aGlobalChildRect = rGlobalChildRect;

            if ( ( nGroupRotateAngle > 4500_deg100 && nGroupRotateAngle <= 13500_deg100 )
                || ( nGroupRotateAngle > 22500_deg100 && nGroupRotateAngle <= 31500_deg100 ) )
            {
                sal_Int32 nHalfWidth = ( aClientRect.GetWidth() + 1 ) >> 1;
                sal_Int32 nHalfHeight = ( aClientRect.GetHeight() + 1 ) >> 1;
                Point aTopLeft( aClientRect.Left() + nHalfWidth - nHalfHeight,
                                aClientRect.Top() + nHalfHeight - nHalfWidth );
                const tools::Long nRotatedWidth = aClientRect.GetHeight();
                const tools::Long nRotatedHeight = aClientRect.GetWidth();
                Size aNewSize(nRotatedWidth, nRotatedHeight);
                tools::Rectangle aNewRect( aTopLeft, aNewSize );
                aClientRect = aNewRect;
            }

            // now importing the inner objects of the group
            if (!aRecHd.SeekToEndOfRecord(rSt))
                return xRet;

            while (rSt.good() && ( rSt.Tell() < rHd.GetRecEndFilePos()))
            {
                DffRecordHeader aRecHd2;
                if (!ReadDffRecordHeader(rSt, aRecHd2))
                    break;
                if ( aRecHd2.nRecType == DFF_msofbtSpgrContainer )
                {
                    tools::Rectangle aGroupClientAnchor, aGroupChildAnchor;
                    GetGroupAnchors( aRecHd2, rSt, aGroupClientAnchor, aGroupChildAnchor, aClientRect, aGlobalChildRect );
                    if (!aRecHd2.SeekToBegOfRecord(rSt))
                        return xRet;
                    sal_Int32 nShapeId;
                    rtl::Reference<SdrObject> pTmp = ImportGroup( aRecHd2, rSt, rClientData, aGroupClientAnchor, aGroupChildAnchor, nCalledByGroup + 1, &nShapeId );
                    if (pTmp)
                    {
                        SdrObjGroup* pGroup = dynamic_cast<SdrObjGroup*>(xRet.get());
                        if (pGroup && pGroup->GetSubList())
                        {
                            pGroup->GetSubList()->NbcInsertObject(pTmp.get());
                            if (nShapeId)
                                insertShapeId(nShapeId, pTmp.get());
                        }
                        else
                            FreeObj(rClientData, pTmp.get());
                    }
                }
                else if ( aRecHd2.nRecType == DFF_msofbtSpContainer )
                {
                    if (!aRecHd2.SeekToBegOfRecord(rSt))
                        return xRet;
                    sal_Int32 nShapeId;
                    rtl::Reference<SdrObject> pTmp = ImportShape( aRecHd2, rSt, rClientData, aClientRect, aGlobalChildRect, nCalledByGroup + 1, &nShapeId );
                    if (pTmp)
                    {
                        SdrObjGroup* pGroup = dynamic_cast<SdrObjGroup*>(xRet.get());
                        if (pGroup && pGroup->GetSubList())
                        {
                            pGroup->GetSubList()->NbcInsertObject(pTmp.get());
                            if (nShapeId)
                                insertShapeId(nShapeId, pTmp.get());
                        }
                        else
                            FreeObj(rClientData, pTmp.get());
                    }
                }
                if (!aRecHd2.SeekToEndOfRecord(rSt))
                    return xRet;
            }

            if ( nGroupRotateAngle )
                xRet->NbcRotate( aClientRect.Center(), nGroupRotateAngle );
            if ( nSpFlags & ShapeFlag::FlipV )
            {   // BoundRect in aBoundRect
                Point aLeft( aClientRect.Left(), ( aClientRect.Top() + aClientRect.Bottom() ) >> 1 );
                Point aRight( aLeft.X() + 1000, aLeft.Y() );
                xRet->NbcMirror( aLeft, aRight );
            }
            if ( nSpFlags & ShapeFlag::FlipH )
            {   // BoundRect in aBoundRect
                Point aTop( ( aClientRect.Left() + aClientRect.Right() ) >> 1, aClientRect.Top() );
                Point aBottom( aTop.X(), aTop.Y() + 1000 );
                xRet->NbcMirror( aTop, aBottom );
            }
        }
    }
    if (o3tl::make_unsigned(nCalledByGroup) < maPendingGroupData.size())
    {
        // finalization for this group is pending, do it now
        xRet = FinalizeObj(maPendingGroupData.back().first, xRet.get());
        maPendingGroupData.pop_back();
    }
    return xRet;
}

rtl::Reference<SdrObject> SvxMSDffManager::ImportShape( const DffRecordHeader& rHd, SvStream& rSt, SvxMSDffClientData& rClientData,
                                            tools::Rectangle& rClientRect, const tools::Rectangle& rGlobalChildRect,
                                            int nCalledByGroup, sal_Int32* pShapeId )
{
    if( pShapeId )
        *pShapeId = 0;

    if (!rHd.SeekToBegOfRecord(rSt))
        return nullptr;

    DffObjData aObjData( rHd, rClientRect, nCalledByGroup );

    aObjData.bRotateTextWithShape = ( GetSvxMSDffSettings() & SVXMSDFF_SETTINGS_IMPORT_EXCEL ) == 0;
    maShapeRecords.Consume( rSt );
    if( maShapeRecords.SeekToContent( rSt,
        DFF_msofbtUDefProp ) )
    {
        sal_uInt32  nBytesLeft = maShapeRecords.Current()->nRecLen;
        while( 5 < nBytesLeft )
        {
            sal_uInt16 nPID(0);
            rSt.ReadUInt16(nPID);
            if (!rSt.good())
                break;
            sal_uInt32 nUDData(0);
            rSt.ReadUInt32(nUDData);
            if (!rSt.good())
                break;
            if (nPID == 447)
            {
                mbRotateGranientFillWithAngle = nUDData & 0x20;
                break;
            }
            nBytesLeft  -= 6;
        }
    }
    aObjData.bShapeType = maShapeRecords.SeekToContent( rSt, DFF_msofbtSp );
    if ( aObjData.bShapeType )
    {
        sal_uInt32 temp(0);
        rSt.ReadUInt32( aObjData.nShapeId )
           .ReadUInt32( temp );
        aObjData.nSpFlags = ShapeFlag(temp);
        aObjData.eShapeType = static_cast<MSO_SPT>(maShapeRecords.Current()->nRecInstance);
    }
    else
    {
        aObjData.nShapeId = 0;
        aObjData.nSpFlags = ShapeFlag::NONE;
        aObjData.eShapeType = mso_sptNil;
    }

    if( pShapeId )
        *pShapeId = aObjData.nShapeId;

    aObjData.bOpt = maShapeRecords.SeekToContent( rSt, DFF_msofbtOPT, SEEK_FROM_CURRENT_AND_RESTART );
    if ( aObjData.bOpt )
    {
        if (!maShapeRecords.Current()->SeekToBegOfRecord(rSt))
            return nullptr;
#ifdef DBG_AUTOSHAPE
        ReadPropSet( rSt, &rClientData, (sal_uInt32)aObjData.eShapeType );
#else
        ReadPropSet( rSt, &rClientData );
#endif
    }
    else
    {
        InitializePropSet( DFF_msofbtOPT ); // get the default PropSet
        static_cast<DffPropertyReader*>(this)->mnFix16Angle = 0_deg100;
    }

    aObjData.bOpt2 = maShapeRecords.SeekToContent( rSt, DFF_msofbtUDefProp, SEEK_FROM_CURRENT_AND_RESTART );
    if ( aObjData.bOpt2 )
    {
        maShapeRecords.Current()->SeekToBegOfRecord( rSt );
        pSecPropSet.reset( new DffPropertyReader( *this ) );
        pSecPropSet->ReadPropSet( rSt, nullptr );
    }

    aObjData.bChildAnchor = maShapeRecords.SeekToContent( rSt, DFF_msofbtChildAnchor, SEEK_FROM_CURRENT_AND_RESTART );
    if ( aObjData.bChildAnchor )
    {
        sal_Int32 l(0), o(0), r(0), u(0);
        rSt.ReadInt32( l ).ReadInt32( o ).ReadInt32( r ).ReadInt32( u );
        Scale( l );
        Scale( o );
        Scale( r );
        Scale( u );
        aObjData.aChildAnchor = tools::Rectangle( l, o, r, u );
        sal_Int32 nWidth, nHeight;
        if (!rGlobalChildRect.IsEmpty() && !rClientRect.IsEmpty() && rGlobalChildRect.GetWidth() && rGlobalChildRect.GetHeight() &&
            !o3tl::checked_sub(r, l, nWidth) && !o3tl::checked_sub(u, o, nHeight))
        {
            double fXScale = static_cast<double>(rClientRect.GetWidth()) / static_cast<double>(rGlobalChildRect.GetWidth());
            double fYScale = static_cast<double>(rClientRect.GetHeight()) / static_cast<double>(rGlobalChildRect.GetHeight());
            double fl = ( ( l - rGlobalChildRect.Left() ) * fXScale ) + rClientRect.Left();
            double fo = ( ( o - rGlobalChildRect.Top()  ) * fYScale ) + rClientRect.Top();
            double fWidth = nWidth * fXScale;
            double fHeight = nHeight * fYScale;
            aObjData.aChildAnchor = tools::Rectangle( Point( fl, fo ), Size( fWidth + 1, fHeight + 1 ) );
        }
    }

    aObjData.bClientAnchor = maShapeRecords.SeekToContent( rSt, DFF_msofbtClientAnchor, SEEK_FROM_CURRENT_AND_RESTART );
    if ( aObjData.bClientAnchor )
        ProcessClientAnchor2( rSt, *maShapeRecords.Current(), aObjData );

    if ( aObjData.bChildAnchor )
        aObjData.aBoundRect = aObjData.aChildAnchor;

    if ( aObjData.nSpFlags & ShapeFlag::Background )
        aObjData.aBoundRect = tools::Rectangle( Point(), Size( 1, 1 ) );

    rtl::Reference<SdrObject> xRet;

    tools::Rectangle aTextRect;
    if ( !aObjData.aBoundRect.IsEmpty() )
    {   // apply rotation to the BoundingBox BEFORE an object has been generated
        if( mnFix16Angle )
        {
            Degree100 nAngle = mnFix16Angle;
            if ( ( nAngle > 4500_deg100 && nAngle <= 13500_deg100 ) || ( nAngle > 22500_deg100 && nAngle <= 31500_deg100 ) )
            {
                sal_Int32 nHalfWidth = ( aObjData.aBoundRect.GetWidth() + 1 ) >> 1;
                sal_Int32 nHalfHeight = ( aObjData.aBoundRect.GetHeight() + 1 ) >> 1;
                Point aTopLeft( aObjData.aBoundRect.Left() + nHalfWidth - nHalfHeight,
                                aObjData.aBoundRect.Top() + nHalfHeight - nHalfWidth );
                Size aNewSize( aObjData.aBoundRect.GetHeight(), aObjData.aBoundRect.GetWidth() );
                tools::Rectangle aNewRect( aTopLeft, aNewSize );
                aObjData.aBoundRect = aNewRect;
            }
        }
        aTextRect = aObjData.aBoundRect;
        bool bGraphic = IsProperty( DFF_Prop_pib ) ||
                            IsProperty( DFF_Prop_pibName ) ||
                            IsProperty( DFF_Prop_pibFlags );

        if ( aObjData.nSpFlags & ShapeFlag::Group )
        {
            xRet = new SdrObjGroup(*pSdrModel);
            /*  After CWS aw033 has been integrated, an empty group object
                cannot store its resulting bounding rectangle anymore. We have
                to return this rectangle via rClientRect now, but only, if
                caller has not passed an own bounding ractangle. */
            if ( rClientRect.IsEmpty() )
                 rClientRect = aObjData.aBoundRect;
            nGroupShapeFlags = aObjData.nSpFlags;
        }
        else if ( ( aObjData.eShapeType != mso_sptNil ) || IsProperty( DFF_Prop_pVertices ) || bGraphic )
        {
            SfxItemSet  aSet( pSdrModel->GetItemPool() );

            bool    bIsConnector = ( ( aObjData.eShapeType >= mso_sptStraightConnector1 ) && ( aObjData.eShapeType <= mso_sptCurvedConnector5 ) );
            Degree100   nObjectRotation = mnFix16Angle;
            ShapeFlag   nSpFlags = aObjData.nSpFlags;

            if ( bGraphic )
            {
                if (!mbSkipImages) {
                    xRet = ImportGraphic(rSt, aSet, aObjData);        // SJ: #68396# is no longer true (fixed in ppt2000)
                    ApplyAttributes( rSt, aSet, aObjData );
                    xRet->SetMergedItemSet(aSet);
                }
            }
            else if ( aObjData.eShapeType == mso_sptLine && !( GetPropertyValue( DFF_Prop_fc3DLightFace, 0 ) & 8 ) )
            {
                basegfx::B2DPolygon aPoly;
                aPoly.append(basegfx::B2DPoint(aObjData.aBoundRect.Left(), aObjData.aBoundRect.Top()));
                aPoly.append(basegfx::B2DPoint(aObjData.aBoundRect.Right(), aObjData.aBoundRect.Bottom()));
                xRet = new SdrPathObj(
                    *pSdrModel,
                    SdrObjKind::Line,
                    basegfx::B2DPolyPolygon(aPoly));
                ApplyAttributes( rSt, aSet, aObjData );
                xRet->SetMergedItemSet(aSet);
            }
            else
            {
                if ( GetCustomShapeContent( aObjData.eShapeType ) || IsProperty( DFF_Prop_pVertices ) )
                {

                    ApplyAttributes( rSt, aSet, aObjData );

                    xRet = new SdrObjCustomShape(*pSdrModel);

                    sal_uInt32 ngtextFStrikethrough = GetPropertyValue( DFF_Prop_gtextFStrikethrough, 0 );
                    bool bIsFontwork = ( ngtextFStrikethrough & 0x4000 ) != 0;

                    // in case of a FontWork, the text is set by the escher import
                    if ( bIsFontwork )
                    {
                        OUString            aObjectText;
                        OUString            aFontName;

                        if ( SeekToContent( DFF_Prop_gtextFont, rSt ) )
                        {
                            SvxFontItem aLatin(EE_CHAR_FONTINFO), aAsian(EE_CHAR_FONTINFO_CJK), aComplex(EE_CHAR_FONTINFO_CTL);
                            GetDefaultFonts( aLatin, aAsian, aComplex );

                            aFontName = MSDFFReadZString( rSt, GetPropertyValue( DFF_Prop_gtextFont, 0 ), true );
                            aSet.Put( SvxFontItem( aLatin.GetFamily(), aFontName, aLatin.GetStyleName(),
                                        PITCH_DONTKNOW, RTL_TEXTENCODING_DONTKNOW, EE_CHAR_FONTINFO ));
                            aSet.Put( SvxFontItem( aLatin.GetFamily(), aFontName, aLatin.GetStyleName(),
                                        PITCH_DONTKNOW, RTL_TEXTENCODING_DONTKNOW, EE_CHAR_FONTINFO_CJK ) );
                            aSet.Put( SvxFontItem( aLatin.GetFamily(), aFontName, aLatin.GetStyleName(),
                                        PITCH_DONTKNOW, RTL_TEXTENCODING_DONTKNOW, EE_CHAR_FONTINFO_CTL ) );
                        }

                        // SJ: applying fontattributes for Fontwork :
                        if ( IsHardAttribute( DFF_Prop_gtextFItalic ) )
                            aSet.Put( SvxPostureItem( ( ngtextFStrikethrough & 0x0010 ) != 0 ? ITALIC_NORMAL : ITALIC_NONE, EE_CHAR_ITALIC ) );

                        if ( IsHardAttribute( DFF_Prop_gtextFBold ) )
                            aSet.Put( SvxWeightItem( ( ngtextFStrikethrough & 0x0020 ) != 0 ? WEIGHT_BOLD : WEIGHT_NORMAL, EE_CHAR_WEIGHT ) );

                        // SJ TODO: Vertical Writing is not correct, instead
                        // this should be replaced through "CharacterRotation"
                        // by 90 degrees, therefore a new Item has to be
                        // supported by svx core, api and xml file format
                        static_cast<SdrObjCustomShape*>(xRet.get())->SetVerticalWriting( ( ngtextFStrikethrough & 0x2000 ) != 0 );

                        if ( SeekToContent( DFF_Prop_gtextUNICODE, rSt ) )
                        {
                            aObjectText = MSDFFReadZString( rSt, GetPropertyValue( DFF_Prop_gtextUNICODE, 0 ), true );
                            ReadObjText(aObjectText, xRet.get());
                        }

                        auto eGeoTextAlign = GetPropertyValue(DFF_Prop_gtextAlign, mso_alignTextCenter);
                        {
                            SdrTextHorzAdjust eHorzAdjust;
                            switch( eGeoTextAlign )
                            {
                                case mso_alignTextLetterJust :
                                case mso_alignTextWordJust :
                                case mso_alignTextStretch : eHorzAdjust = SDRTEXTHORZADJUST_BLOCK; break;
                                default:
                                case mso_alignTextInvalid :
                                case mso_alignTextCenter : eHorzAdjust = SDRTEXTHORZADJUST_CENTER; break;
                                case mso_alignTextLeft : eHorzAdjust = SDRTEXTHORZADJUST_LEFT; break;
                                case mso_alignTextRight : eHorzAdjust = SDRTEXTHORZADJUST_RIGHT; break;
                            }
                            aSet.Put( SdrTextHorzAdjustItem( eHorzAdjust ) );

                            drawing::TextFitToSizeType eFTS = drawing::TextFitToSizeType_NONE;
                            if ( eGeoTextAlign == mso_alignTextStretch )
                                eFTS = drawing::TextFitToSizeType_ALLLINES;
                            aSet.Put( SdrTextFitToSizeTypeItem( eFTS ) );
                        }
                        if ( IsProperty( DFF_Prop_gtextSpacing ) )
                        {
                            sal_Int32 nTextWidth = GetPropertyValue( DFF_Prop_gtextSpacing, 1 << 16 ) / 655;
                            if ( nTextWidth != 100 )
                                aSet.Put( SvxCharScaleWidthItem( static_cast<sal_uInt16>(nTextWidth), EE_CHAR_FONTWIDTH ) );
                        }
                        if ( ngtextFStrikethrough & 0x1000 ) // SJ: Font Kerning On ?
                            aSet.Put( SvxKerningItem( 1, EE_CHAR_KERNING ) );

                        // #i119496# the resize autoshape to fit text attr of word art in MS PPT is always false
                        aSet.Put(makeSdrTextAutoGrowHeightItem(false));
                        aSet.Put(makeSdrTextAutoGrowWidthItem(false));

                        bool bWithPadding = !( ngtextFStrikethrough & use_gtextFBestFit
                                            && ngtextFStrikethrough & use_gtextFShrinkFit
                                            && ngtextFStrikethrough & use_gtextFStretch
                                            && ngtextFStrikethrough & gtextFBestFit
                                            && ngtextFStrikethrough & gtextFShrinkFit
                                            && ngtextFStrikethrough & gtextFStretch );

                        if ( bWithPadding )
                        {
                            // trim, remove additional space
                            VclPtr<VirtualDevice> pDevice = VclPtr<VirtualDevice>::Create();
                            vcl::Font aFont = pDevice->GetFont();
                            aFont.SetFamilyName( aFontName );
                            aFont.SetFontSize( Size( 0, 96 ) );
                            pDevice->SetFont( aFont );

                            auto nTextWidth = pDevice->GetTextWidth( aObjectText );
                            OUString aObjName = GetPropertyString( DFF_Prop_wzName, rSt );
                            if ( nTextWidth && aObjData.eShapeType == mso_sptTextPlainText
                                && aObjName.match( "PowerPlusWaterMarkObject" ) )
                            {
                                double fRatio = static_cast<double>(pDevice->GetTextHeight()) / nTextWidth;
                                sal_Int32 nNewHeight = fRatio * aObjData.aBoundRect.getOpenWidth();
                                sal_Int32 nPaddingY = aObjData.aBoundRect.getOpenHeight() - nNewHeight;

                                if ( nPaddingY > 0 )
                                    aObjData.aBoundRect.setHeight( nNewHeight );
                            }
                        }
                    }
                    xRet->SetMergedItemSet( aSet );

                    // sj: taking care of rtl, ltr. In case of fontwork mso. seems not to be able to set
                    // proper text directions, instead the text default is depending to the string.
                    // so we have to calculate the a text direction from string:
                    if ( bIsFontwork )
                    {
                        OutlinerParaObject* pParaObj = static_cast<SdrObjCustomShape*>(xRet.get())->GetOutlinerParaObject();
                        if ( pParaObj )
                        {
                            SdrOutliner& rOutliner = static_cast<SdrObjCustomShape*>(xRet.get())->ImpGetDrawOutliner();
                            rOutliner.SetStyleSheetPool(static_cast< SfxStyleSheetPool* >(xRet->getSdrModelFromSdrObject().GetStyleSheetPool()));
                            bool bOldUpdateMode = rOutliner.SetUpdateLayout( false );
                            rOutliner.SetText( *pParaObj );
                            ScopedVclPtrInstance< VirtualDevice > pVirDev(DeviceFormat::WITHOUT_ALPHA);
                            pVirDev->SetMapMode(MapMode(MapUnit::Map100thMM));
                            sal_Int32 i, nParagraphs = rOutliner.GetParagraphCount();
                            if ( nParagraphs )
                            {
                                bool bCreateNewParaObject = false;
                                for ( i = 0; i < nParagraphs; i++ )
                                {
                                    OUString aString(rOutliner.GetText(rOutliner.GetParagraph(i)));
                                    bool bIsRTL = pVirDev->GetTextIsRTL(aString, 0, aString.getLength());
                                    if ( bIsRTL )
                                    {
                                        SfxItemSet aSet2( rOutliner.GetParaAttribs( i ) );
                                        aSet2.Put( SvxFrameDirectionItem( SvxFrameDirection::Horizontal_RL_TB, EE_PARA_WRITINGDIR ) );
                                        rOutliner.SetParaAttribs( i, aSet2 );
                                        bCreateNewParaObject = true;
                                    }
                                }
                                if  ( bCreateNewParaObject )
                                {
                                    std::optional<OutlinerParaObject> pNewText = rOutliner.CreateParaObject();
                                    rOutliner.Init( OutlinerMode::TextObject );
                                    static_cast<SdrObjCustomShape*>(xRet.get())->NbcSetOutlinerParaObject( std::move(pNewText) );
                                }
                            }
                            rOutliner.Clear();
                            rOutliner.SetUpdateLayout( bOldUpdateMode );
                        }
                    }

                    // mso_sptArc special treating
                    // tdf#124029: A new custom shape is generated from prototype 'msoArc'. Values, which are
                    // read here, are adapted and merged. The shape type is changed, so this code
                    // applies only if importing arcs from MS Office.
                    if ( aObjData.eShapeType == mso_sptArc )
                    {
                        static constexpr OUString sAdjustmentValues( u"AdjustmentValues"_ustr );
                        static constexpr OUString sViewBox( u"ViewBox"_ustr );
                        static constexpr OUString sPath( u"Path"_ustr );
                        SdrCustomShapeGeometryItem aGeometryItem( static_cast<SdrObjCustomShape*>(xRet.get())->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY ) );
                        PropertyValue aPropVal;

                        // The default arc goes form -90deg to 0deg. Replace general defaults used
                        // when read from stream with this specific values.
                        double fStartAngle(-90.0);
                        double fEndAngle(0.0);
                        css::uno::Sequence< css::drawing::EnhancedCustomShapeAdjustmentValue > seqAdjustmentValues;
                        const uno::Any* pAny = aGeometryItem.GetPropertyValueByName(sAdjustmentValues);
                        if (pAny && (*pAny >>= seqAdjustmentValues) && seqAdjustmentValues.getLength() > 1)
                        {
                            auto pseqAdjustmentValues = seqAdjustmentValues.getArray();
                            if (seqAdjustmentValues[0].State == css::beans::PropertyState_DEFAULT_VALUE)
                            {
                                pseqAdjustmentValues[0].Value <<= -90.0;
                                pseqAdjustmentValues[0].State = css::beans::PropertyState_DIRECT_VALUE;
                            }
                            if (seqAdjustmentValues[1].State == css::beans::PropertyState_DEFAULT_VALUE)
                            {
                                pseqAdjustmentValues[1].Value <<= 0.0;
                                pseqAdjustmentValues[1].State = css::beans::PropertyState_DIRECT_VALUE;
                            }
                            seqAdjustmentValues[0].Value >>= fStartAngle;
                            seqAdjustmentValues[1].Value >>= fEndAngle;
                            aPropVal.Name = sAdjustmentValues;
                            aPropVal.Value <<= seqAdjustmentValues;
                            aGeometryItem.SetPropertyValue(aPropVal);
                        }

                        // arc first command is always wr -- clockwise arc
                        // the parameters are : (left,top),(right,bottom),start(x,y),end(x,y)
                        // The left/top vertex of the frame rectangle of the sector is the origin
                        // of the shape internal coordinate system in MS Office. The default arc
                        // has an ellipse frame rectangle with LT(-21600,0) and
                        // RB(21600,43200) in this coordinate system.
                        basegfx::B2DRectangle aEllipseRect_MS(-21600.0, 0.0, 21600.0, 43200.0);
                        css::uno::Sequence< css::drawing::EnhancedCustomShapeParameterPair> seqCoordinates;
                        pAny = aGeometryItem.GetPropertyValueByName( sPath, u"Coordinates"_ustr );
                        if (pAny && (*pAny >>= seqCoordinates) && (seqCoordinates.getLength() >= 2))
                        {
                            auto const nL
                                = *o3tl::doAccess<sal_Int32>(seqCoordinates[0].First.Value);
                            auto const nT
                                = *o3tl::doAccess<sal_Int32>(seqCoordinates[0].Second.Value);
                            auto const nR
                                = *o3tl::doAccess<sal_Int32>(seqCoordinates[1].First.Value);
                            auto const nB
                                = *o3tl::doAccess<sal_Int32>(seqCoordinates[1].Second.Value);
                            aEllipseRect_MS = basegfx::B2DRectangle(nL, nT, nR, nB);
                        }

                        // MS Office uses the pie frame rectangle as reference for outer position
                        // and size of the shape and for text in the shape. We can get this rectangle
                        // from imported viewBox or from the arc geometry.
                        basegfx::B2DRectangle aPieRect_MS(0.0 , 0.0, 21600.0, 21600.0);
                        pAny = aGeometryItem.GetPropertyValueByName(sPath,sViewBox);
                        css::awt::Rectangle aImportedViewBox;
                        if (pAny && (*pAny >>= aImportedViewBox))
                        {
                            aPieRect_MS = basegfx::B2DRectangle( aImportedViewBox.X,
                                                                aImportedViewBox.Y,
                                                      aImportedViewBox.X + aImportedViewBox.Width,
                                                      aImportedViewBox.Y + aImportedViewBox.Height);
                        }
                        else
                        {
                            double fRadStartAngle(basegfx::deg2rad(NormAngle360(fStartAngle)));
                            double fRadEndAngle(basegfx::deg2rad(NormAngle360(fEndAngle)));
                            basegfx::B2DPoint aCenter(aEllipseRect_MS.getCenter());
                            basegfx::B2DPolygon aTempPie(
                                    basegfx::utils::createPolygonFromEllipseSegment(
                                        aCenter,
                                        aEllipseRect_MS.getWidth() * 0.5,
                                        aEllipseRect_MS.getHeight() * 0.5,
                                        fRadStartAngle,
                                        fRadEndAngle));
                            aTempPie.append(aCenter);
                            aPieRect_MS = aTempPie.getB2DRange();
                        }

                        // MS Office uses for mso_sptArc a frame rectangle (=resize handles)
                        // which encloses only the sector, LibreOffice uses for custom shapes as
                        // default a frame rectangle, which encloses the entire ellipse. That would
                        // result in wrong positions in Writer and Calc, see tdf#124029.
                        // We workaround this problem, by setting a suitable viewBox.
                        bool bIsImportPPT(GetSvxMSDffSettings() & SVXMSDFF_SETTINGS_IMPORT_PPT);
                        if (bIsImportPPT || aPieRect_MS.getWidth() == 0 ||  aPieRect_MS.getHeight() == 0)
                        { // clear item, so that default from EnhancedCustomShapeGeometry is used
                            aGeometryItem.ClearPropertyValue(sViewBox);
                        }
                        else
                        {
                            double fX((aPieRect_MS.getMinX() - aEllipseRect_MS.getMinX()) / 2.0);
                            double fY((aPieRect_MS.getMinY() - aEllipseRect_MS.getMinY()) / 2.0);
                            css::awt::Rectangle aViewBox_LO; // in LO coordinate system
                            aViewBox_LO.X = static_cast<sal_Int32>(fX);
                            aViewBox_LO.Y = static_cast<sal_Int32>(fY);
                            aViewBox_LO.Width = static_cast<sal_Int32>(aPieRect_MS.getWidth() / 2.0);
                            aViewBox_LO.Height = static_cast<sal_Int32>(aPieRect_MS.getHeight() / 2.0);
                            aPropVal.Name = sViewBox;
                            aPropVal.Value <<= aViewBox_LO;
                            aGeometryItem.SetPropertyValue(aPropVal);
                        }

                        // aObjData.aBoundRect contains position and size of the sector in (outer)
                        // logic coordinates, e.g. for PPT in 1/100 mm, for Word in twips.
                        // For Impress the default viewBox is used, so adapt aObjData.aBoundRect.
                        tools::Rectangle aOldBoundRect(aObjData.aBoundRect); // backup, needed later on
                        if (bIsImportPPT)
                        {
                            double fLogicXOfs(0.0); // LogicLeft_LO = LogicLeft_MS + fXLogicOfs
                            double fLogicYOfs(0.0);
                            double fLogicPieWidth(aObjData.aBoundRect.getOpenWidth());
                            double fLogicPieHeight(aObjData.aBoundRect.getOpenHeight());
                            double fLogicEllipseWidth(0.0); // to be LogicWidth_LO
                            double fLogicEllipseHeight(0.0);
                            if (aPieRect_MS.getWidth())
                            {
                                // fXScale = ratio 'logic length' : 'shape internal length'
                                double fXScale = fLogicPieWidth / aPieRect_MS.getWidth();
                                if (nSpFlags & ShapeFlag::FlipH)
                                    fLogicXOfs = (aPieRect_MS.getMaxX() - aEllipseRect_MS.getMaxX()) * fXScale;
                                else
                                    fLogicXOfs = (aEllipseRect_MS.getMinX() - aPieRect_MS.getMinX()) * fXScale;
                                fLogicEllipseWidth = aEllipseRect_MS.getWidth() * fXScale;
                            }
                            if (aPieRect_MS.getHeight())
                            {
                                double fYScale = fLogicPieHeight / aPieRect_MS.getHeight();
                                if (nSpFlags & ShapeFlag::FlipV)
                                    fLogicYOfs = (aPieRect_MS.getMaxY() - aEllipseRect_MS.getMaxY()) * fYScale;
                                else
                                    fLogicYOfs = (aEllipseRect_MS.getMinY() - aPieRect_MS.getMinY()) * fYScale;
                                fLogicEllipseHeight = aEllipseRect_MS.getHeight() * fYScale;
                            }
                            aObjData.aBoundRect = tools::Rectangle(
                                                    Point(aOldBoundRect.Left() + static_cast<sal_Int32>(fLogicXOfs),
                                                          aOldBoundRect.Top() + static_cast<sal_Int32>(fLogicYOfs)),
                                                    Size(static_cast<sal_Int32>(fLogicEllipseWidth),
                                                         static_cast<sal_Int32>(fLogicEllipseHeight)));
                        }
                        // else nothing to do. aObjData.aBoundRect corresponds to changed viewBox.

                        // creating the text frame -> scaling into (0,0),(21600,21600) destination coordinate system
                        double fTextFrameScaleX = 0.0;
                        double fTextFrameScaleY = 0.0;
                        if (aEllipseRect_MS.getWidth())
                            fTextFrameScaleX = 21600.0 / aEllipseRect_MS.getWidth();
                        if (aEllipseRect_MS.getHeight())
                            fTextFrameScaleY = 21600.0 / aEllipseRect_MS.getHeight();

                        sal_Int32 nLeft  = static_cast<sal_Int32>((aPieRect_MS.getMinX() - aEllipseRect_MS.getMinX()) * fTextFrameScaleX );
                        sal_Int32 nTop   = static_cast<sal_Int32>((aPieRect_MS.getMinY() - aEllipseRect_MS.getMinY()) * fTextFrameScaleY );
                        sal_Int32 nRight = static_cast<sal_Int32>((aPieRect_MS.getMaxX() - aEllipseRect_MS.getMinX()) * fTextFrameScaleX );
                        sal_Int32 nBottom= static_cast<sal_Int32>((aPieRect_MS.getMaxY() - aEllipseRect_MS.getMinY()) * fTextFrameScaleY );
                        css::uno::Sequence< css::drawing::EnhancedCustomShapeTextFrame > aTextFrame( 1 );
                        auto pTextFrame = aTextFrame.getArray();
                        EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( pTextFrame[ 0 ].TopLeft.First,     nLeft );
                        EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( pTextFrame[ 0 ].TopLeft.Second,    nTop );
                        EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( pTextFrame[ 0 ].BottomRight.First, nRight );
                        EnhancedCustomShape2d::SetEnhancedCustomShapeParameter( pTextFrame[ 0 ].BottomRight.Second,nBottom );
                        PropertyValue aProp;
                        aProp.Name = "TextFrames";
                        aProp.Value <<= aTextFrame;
                        aGeometryItem.SetPropertyValue( sPath, aProp );

                        // sj: taking care of the different rotation points, since the new arc is having a bigger snaprect
                        if ( mnFix16Angle )
                        {
                            Degree100 nAngle = mnFix16Angle;
                            if ( nSpFlags & ShapeFlag::FlipH )
                                nAngle = 36000_deg100 - nAngle;
                            if ( nSpFlags & ShapeFlag::FlipV )
                                nAngle = -nAngle;
                            double a = toRadians(nAngle);
                            double ss = sin( a );
                            double cc = cos( a );
                            Point aP1( aOldBoundRect.TopLeft() );
                            Point aC1( aObjData.aBoundRect.Center() );
                            Point aP2( aOldBoundRect.TopLeft() );
                            Point aC2( aOldBoundRect.Center() );
                            RotatePoint( aP1, aC1, ss, cc );
                            RotatePoint( aP2, aC2, ss, cc );
                            aObjData.aBoundRect.Move( aP2.X() - aP1.X(), aP2.Y() - aP1.Y() );
                        }

                        // clearing items, so MergeDefaultAttributes will set the corresponding
                        // defaults from EnhancedCustomShapeGeometry
                        aGeometryItem.ClearPropertyValue( u"Handles"_ustr );
                        aGeometryItem.ClearPropertyValue( u"Equations"_ustr );
                        aGeometryItem.ClearPropertyValue( sPath );

                        static_cast<SdrObjCustomShape*>(xRet.get())->SetMergedItem( aGeometryItem );
                        static_cast<SdrObjCustomShape*>(xRet.get())->MergeDefaultAttributes();

                        // now setting a new name, so the above correction is only done once when importing from ms
                        SdrCustomShapeGeometryItem aGeoName( static_cast<SdrObjCustomShape*>(xRet.get())->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY ) );
                        aPropVal.Name = "Type";
                        aPropVal.Value <<= u"mso-spt100"_ustr;
                        aGeoName.SetPropertyValue( aPropVal );
                        static_cast<SdrObjCustomShape*>(xRet.get())->SetMergedItem( aGeoName );
                    }
                    else
                        static_cast<SdrObjCustomShape*>(xRet.get())->MergeDefaultAttributes();

                    xRet->SetSnapRect( aObjData.aBoundRect );
                    EnhancedCustomShape2d aCustomShape2d(static_cast<SdrObjCustomShape&>(*xRet));
                    aTextRect = aCustomShape2d.GetTextRect();

                    if( bIsConnector )
                    {
                        if( nObjectRotation )
                            xRet->NbcRotate( aObjData.aBoundRect.Center(), nObjectRotation );
                        // mirrored horizontally?
                        if ( nSpFlags & ShapeFlag::FlipH )
                        {
                            tools::Rectangle aBndRect(xRet->GetSnapRect());
                            Point aTop( ( aBndRect.Left() + aBndRect.Right() ) >> 1, aBndRect.Top() );
                            Point aBottom( aTop.X(), aTop.Y() + 1000 );
                            xRet->NbcMirror( aTop, aBottom );
                        }
                        // mirrored vertically?
                        if ( nSpFlags & ShapeFlag::FlipV )
                        {
                            tools::Rectangle aBndRect(xRet->GetSnapRect());
                            Point aLeft( aBndRect.Left(), ( aBndRect.Top() + aBndRect.Bottom() ) >> 1 );
                            Point aRight( aLeft.X() + 1000, aLeft.Y() );
                            xRet->NbcMirror( aLeft, aRight );
                        }
                        basegfx::B2DPolyPolygon aPoly( static_cast<SdrObjCustomShape*>(xRet.get())->GetLineGeometry( true ) );

                        xRet = new SdrEdgeObj(*pSdrModel);
                        ApplyAttributes( rSt, aSet, aObjData );
                        xRet->SetLogicRect( aObjData.aBoundRect );
                        xRet->SetMergedItemSet(aSet);

                        // connectors
                        auto eConnectorStyle = GetPropertyValue(DFF_Prop_cxstyle, mso_cxstyleStraight);

                        static_cast<SdrEdgeObj*>(xRet.get())->ConnectToNode(true, nullptr);
                        static_cast<SdrEdgeObj*>(xRet.get())->ConnectToNode(false, nullptr);

                        Point aPoint1( aObjData.aBoundRect.TopLeft() );
                        Point aPoint2( aObjData.aBoundRect.BottomRight() );

                        // pay attention to the rotations
                        if ( nObjectRotation )
                        {
                            double a = toRadians(nObjectRotation);
                            Point aCenter( aObjData.aBoundRect.Center() );
                            double ss = sin(a);
                            double cc = cos(a);

                            RotatePoint(aPoint1, aCenter, ss, cc);
                            RotatePoint(aPoint2, aCenter, ss, cc);

                            // #i120437# reset rotation, it is part of the path and shall not be applied again
                            nObjectRotation = 0_deg100;
                        }

                        // rotate/mirror line within the area as we need it
                        if ( nSpFlags & ShapeFlag::FlipH )
                        {
                            sal_Int32 n = aPoint1.X();
                            aPoint1.setX( aPoint2.X() );
                            aPoint2.setX( n );

                            // #i120437# reset hor flip
                            nSpFlags &= ~ShapeFlag::FlipH;
                        }
                        if ( nSpFlags & ShapeFlag::FlipV )
                        {
                            sal_Int32 n = aPoint1.Y();
                            aPoint1.setY( aPoint2.Y() );
                            aPoint2.setY( n );

                            // #i120437# reset ver flip
                            nSpFlags &= ~ShapeFlag::FlipV;
                        }

                        xRet->NbcSetPoint(aPoint1, 0); // start point
                        xRet->NbcSetPoint(aPoint2, 1); // endpoint

                        sal_Int32 n1HorzDist, n1VertDist, n2HorzDist, n2VertDist;
                        n1HorzDist = n1VertDist = n2HorzDist = n2VertDist = 0;
                        switch( eConnectorStyle )
                        {
                            case mso_cxstyleBent:
                            {
                                aSet.Put( SdrEdgeKindItem( SdrEdgeKind::OrthoLines ) );
                                n1HorzDist = n1VertDist = n2HorzDist = n2VertDist = 630;
                            }
                            break;
                            case mso_cxstyleCurved:
                                aSet.Put( SdrEdgeKindItem( SdrEdgeKind::Bezier ) );
                            break;
                            default: // mso_cxstyleStraight || mso_cxstyleNone
                                aSet.Put( SdrEdgeKindItem( SdrEdgeKind::OneLine ) );
                            break;
                        }
                        aSet.Put( SdrEdgeNode1HorzDistItem( n1HorzDist ) );
                        aSet.Put( SdrEdgeNode1VertDistItem( n1VertDist ) );
                        aSet.Put( SdrEdgeNode2HorzDistItem( n2HorzDist ) );
                        aSet.Put( SdrEdgeNode2VertDistItem( n2VertDist ) );

                        static_cast<SdrEdgeObj*>(xRet.get())->SetEdgeTrackPath( aPoly );
                        xRet->SetMergedItemSet(aSet);
                    }
                    if ( aObjData.eShapeType == mso_sptLine )
                    {
                        xRet->SetMergedItemSet(aSet);
                        static_cast<SdrObjCustomShape*>(xRet.get())->MergeDefaultAttributes();
                    }
                }
            }

            if (xRet)
            {
                if( nObjectRotation )
                    xRet->NbcRotate( aObjData.aBoundRect.Center(), nObjectRotation );
                // mirrored horizontally?
                if ( nSpFlags & ShapeFlag::FlipH )
                {
                    tools::Rectangle aBndRect(xRet->GetSnapRect());
                    Point aTop( ( aBndRect.Left() + aBndRect.Right() ) >> 1, aBndRect.Top() );
                    Point aBottom( aTop.X(), aTop.Y() + 1000 );
                    xRet->NbcMirror(aTop, aBottom);
                }
                // mirrored vertically?
                if ( nSpFlags & ShapeFlag::FlipV )
                {
                    tools::Rectangle aBndRect(xRet->GetSnapRect());
                    Point aLeft( aBndRect.Left(), ( aBndRect.Top() + aBndRect.Bottom() ) >> 1 );
                    Point aRight( aLeft.X() + 1000, aLeft.Y() );
                    xRet->NbcMirror(aLeft, aRight);
                }
            }
        }
    }

    // #i51348# #118052# name of the shape
    if (xRet)
    {
        OUString aObjName = GetPropertyString( DFF_Prop_wzName, rSt );
        if( !aObjName.isEmpty() )
            xRet->SetName(aObjName);
    }

    xRet = ProcessObj(rSt, aObjData, rClientData, aTextRect, xRet.get());

    if (xRet)
    {
        sal_Int32 nGroupProperties( GetPropertyValue( DFF_Prop_fPrint, 0 ) );
        const bool bVisible = ( ( nGroupProperties & 2 ) == 0 );
        xRet->SetVisible( bVisible );
        // In Excel hidden means not printed
        if ( !bVisible )
        {
            xRet->SetPrintable(false);
        }
        else
        {
            // This property isn't used in Excel anymore, leaving it for legacy reasons
            xRet->SetPrintable( ( nGroupProperties & 1 ) != 0 );
        }
    }

    //Import alt text as description
    if (xRet && SeekToContent(DFF_Prop_wzDescription, rSt))
    {
        OUString aAltText = MSDFFReadZString(rSt, GetPropertyValue(DFF_Prop_wzDescription, 0), true);
        xRet->SetDescription(aAltText);
    }

    // If this shape opens a new group, push back its object data because
    // finalization will be called when nested objects have been imported;
    // otherwise, just finalize here
    if (o3tl::make_unsigned(nCalledByGroup) > maPendingGroupData.size())
    {
        auto xHdClone = std::make_shared<DffRecordHeader>(aObjData.rSpHd);
        maPendingGroupData.emplace_back(DffObjData(xHdClone, aObjData), xHdClone );
    }
    else
    {
        xRet = FinalizeObj(aObjData, xRet.get());
    }
    return xRet;
}

tools::Rectangle SvxMSDffManager::GetGlobalChildAnchor( const DffRecordHeader& rHd, SvStream& rSt, tools::Rectangle& aClientRect )
{
    tools::Rectangle aChildAnchor;
    if (!rHd.SeekToContent(rSt))
        return aChildAnchor;

    bool bIsClientRectRead = false;
    while ( ( rSt.GetError() == ERRCODE_NONE ) && ( rSt.Tell() < rHd.GetRecEndFilePos() ) )
    {
        DffRecordHeader aShapeHd;
        if (!ReadDffRecordHeader(rSt, aShapeHd))
            break;
        if ( ( aShapeHd.nRecType == DFF_msofbtSpContainer ) ||
                ( aShapeHd.nRecType == DFF_msofbtSpgrContainer ) )
        {
            DffRecordHeader aShapeHd2( aShapeHd );
            if ( aShapeHd.nRecType == DFF_msofbtSpgrContainer )
                ReadDffRecordHeader( rSt, aShapeHd2 );
            while (rSt.good() && rSt.Tell() < aShapeHd2.GetRecEndFilePos())
            {
                DffRecordHeader aShapeAtom;
                if (!ReadDffRecordHeader(rSt, aShapeAtom))
                    break;

                if ( aShapeAtom.nRecType == DFF_msofbtClientAnchor )
                {
                    if ( GetSvxMSDffSettings() & SVXMSDFF_SETTINGS_IMPORT_PPT )
                    {
                        sal_Int32 l(0), t(0), r(0), b(0);
                        if ( aShapeAtom.nRecLen == 16 )
                        {
                            rSt.ReadInt32( l ).ReadInt32( t ).ReadInt32( r ).ReadInt32( b );
                        }
                        else
                        {
                            sal_Int16 ls(0), ts(0), rs(0), bs(0);
                            rSt.ReadInt16( ts ).ReadInt16( ls ).ReadInt16( rs ).ReadInt16( bs ); // the order of coordinates is a bit strange...
                            l = ls;
                            t = ts;
                            r = rs;
                            b = bs;
                        }
                        Scale( l );
                        Scale( t );
                        Scale( r );
                        Scale( b );
                        if ( bIsClientRectRead )
                        {
                            tools::Rectangle aChild( l, t, r, b );
                            aChildAnchor.Union( aChild );
                        }
                        else
                        {
                            aClientRect = tools::Rectangle( l, t, r, b );
                            bIsClientRectRead = true;
                        }
                    }
                    break;
                }
                else if ( aShapeAtom.nRecType == DFF_msofbtChildAnchor )
                {
                    sal_Int32 l(0), o(0), r(0), u(0);
                    rSt.ReadInt32( l ).ReadInt32( o ).ReadInt32( r ).ReadInt32( u );
                    Scale( l );
                    Scale( o );
                    Scale( r );
                    Scale( u );
                    tools::Rectangle aChild( l, o, r, u );
                    aChildAnchor.Union( aChild );
                    break;
                }
                if (!aShapeAtom.SeekToEndOfRecord(rSt))
                    break;
            }
        }
        if (!aShapeHd.SeekToEndOfRecord(rSt))
            break;
    }
    return aChildAnchor;
}

void SvxMSDffManager::GetGroupAnchors( const DffRecordHeader& rHd, SvStream& rSt,
                            tools::Rectangle& rGroupClientAnchor, tools::Rectangle& rGroupChildAnchor,
                                const tools::Rectangle& rClientRect, const tools::Rectangle& rGlobalChildRect )
{
    if (!rHd.SeekToContent(rSt))
        return;

    bool bFirst = true;
    DffRecordHeader aShapeHd;
    while (rSt.good() && rSt.Tell() < rHd.GetRecEndFilePos())
    {
        if (!ReadDffRecordHeader(rSt, aShapeHd))
            break;
        if ( ( aShapeHd.nRecType == DFF_msofbtSpContainer ) ||
                ( aShapeHd.nRecType == DFF_msofbtSpgrContainer ) )
        {
            DffRecordHeader aShapeHd2( aShapeHd );
            if ( aShapeHd.nRecType == DFF_msofbtSpgrContainer )
                ReadDffRecordHeader( rSt, aShapeHd2 );
            while (rSt.good() && rSt.Tell() < aShapeHd2.GetRecEndFilePos())
            {
                DffRecordHeader aShapeAtom;
                if (!ReadDffRecordHeader(rSt, aShapeAtom))
                    break;
                if ( aShapeAtom.nRecType == DFF_msofbtChildAnchor )
                {
                    sal_Int32 l(0), o(0), r(0), u(0);
                    rSt.ReadInt32( l ).ReadInt32( o ).ReadInt32( r ).ReadInt32( u );
                    Scale( l );
                    Scale( o );
                    Scale( r );
                    Scale( u );
                    tools::Rectangle aChild( l, o, r, u );

                    if ( bFirst )
                    {
                        if ( !rGlobalChildRect.IsEmpty() && !rClientRect.IsEmpty() && rGlobalChildRect.GetWidth() && rGlobalChildRect.GetHeight() )
                        {
                            double fWidth = o3tl::saturating_sub(r, l);
                            double fHeight= o3tl::saturating_sub(u, o);
                            double fXScale = static_cast<double>(rClientRect.GetWidth()) / static_cast<double>(rGlobalChildRect.GetWidth());
                            double fYScale = static_cast<double>(rClientRect.GetHeight()) / static_cast<double>(rGlobalChildRect.GetHeight());
                            double fl = ( ( l - rGlobalChildRect.Left() ) * fXScale ) + rClientRect.Left();
                            double fo = ( ( o - rGlobalChildRect.Top()  ) * fYScale ) + rClientRect.Top();
                            fWidth *= fXScale;
                            fHeight *= fYScale;
                            rGroupClientAnchor = tools::Rectangle( Point( static_cast<sal_Int32>(fl), static_cast<sal_Int32>(fo) ), Size( static_cast<sal_Int32>( fWidth + 1 ), static_cast<sal_Int32>( fHeight + 1 ) ) );
                        }
                        bFirst = false;
                    }
                    else
                        rGroupChildAnchor.Union( aChild );
                    break;
                }
                if (!aShapeAtom.SeekToEndOfRecord(rSt))
                    break;
            }
        }
        if (!aShapeHd.SeekToEndOfRecord(rSt))
            break;
    }
}

SvxMSDffImportRec* SvxMSDffImportData::find(const SdrObject* pObj)
{
    auto it = m_ObjToRecMap.find(pObj);
    if (it != m_ObjToRecMap.end())
        return it->second;
    return nullptr;
}

void SvxMSDffImportData::insert(std::unique_ptr<SvxMSDffImportRec> pImpRec)
{
    auto aRet = m_Records.insert(std::move(pImpRec));
    bool bSuccess = aRet.second;
    if (bSuccess)
    {
        SvxMSDffImportRec* pRec = aRet.first->get();
        m_ObjToRecMap[pRec->pObj.get()] = pRec;
    }
}

void SvxMSDffImportData::NotifyFreeObj(SdrObject* pObj)
{
    if (SvxMSDffImportRec* pRecord = find(pObj))
    {
        m_ObjToRecMap.erase(pObj);
        pRecord->pObj = nullptr;
    }
}

void SvxMSDffManager::NotifyFreeObj(SvxMSDffClientData& rData, SdrObject* pObj)
{
    if (SdrObjGroup* pGroup = dynamic_cast<SdrObjGroup*>(pObj))
    {
        SdrObjList* pSubList = pGroup->GetSubList();
        for (const rtl::Reference<SdrObject>& pChildObj : *pSubList)
            NotifyFreeObj(rData, pChildObj.get());
    }

    rData.NotifyFreeObj(pObj);
}

void SvxMSDffManager::FreeObj(SvxMSDffClientData& rData, SdrObject* pObj)
{
    NotifyFreeObj(rData, pObj);
}

rtl::Reference<SdrObject> SvxMSDffManager::ProcessObj(SvStream& rSt,
                                       DffObjData& rObjData,
                                       SvxMSDffClientData& rData,
                                       tools::Rectangle& rTextRect,
                                       SdrObject* pObj1
                                       )
{
    rtl::Reference<SdrObject> pObj = pObj1;
    if( !rTextRect.IsEmpty() )
    {
        SvxMSDffImportData& rImportData = static_cast<SvxMSDffImportData&>(rData);
        SvxMSDffImportRec* pImpRec = new SvxMSDffImportRec;
        bool bDeleteImpRec = true;
        SvxMSDffImportRec* pTextImpRec = pImpRec;
        bool bDeleteTextImpRec = false;

        // fill Import Record with data
        pImpRec->nShapeId   = rObjData.nShapeId;
        pImpRec->eShapeType = rObjData.eShapeType;

        auto eWrapMode = GetPropertyValue(DFF_Prop_WrapText, mso_wrapSquare);
        rObjData.bClientAnchor = maShapeRecords.SeekToContent( rSt,
                                            DFF_msofbtClientAnchor,
                                            SEEK_FROM_CURRENT_AND_RESTART );
        if( rObjData.bClientAnchor )
            ProcessClientAnchor( rSt,
                    maShapeRecords.Current()->nRecLen,
                    pImpRec->pClientAnchorBuffer, pImpRec->nClientAnchorLen );

        rObjData.bClientData = maShapeRecords.SeekToContent( rSt,
                                            DFF_msofbtClientData,
                                            SEEK_FROM_CURRENT_AND_RESTART );
        if( rObjData.bClientData )
            ProcessClientData( rSt,
                    maShapeRecords.Current()->nRecLen,
                    pImpRec->pClientDataBuffer, pImpRec->nClientDataLen );


        // process user (== Winword) defined parameters in 0xF122 record
        if(    maShapeRecords.SeekToContent( rSt,
                                             DFF_msofbtUDefProp,
                                             SEEK_FROM_CURRENT_AND_RESTART )
            && maShapeRecords.Current()->nRecLen )
        {
            sal_uInt32  nBytesLeft = maShapeRecords.Current()->nRecLen;
            while( 5 < nBytesLeft )
            {
                sal_uInt16 nPID(0);
                rSt.ReadUInt16(nPID);
                if (!rSt.good())
                    break;
                sal_uInt32 nUDData(0);
                rSt.ReadUInt32(nUDData);
                switch (nPID)
                {
                    case 0x038F: pImpRec->nXAlign = nUDData; break;
                    case 0x0390:
                        pImpRec->nXRelTo = nUDData;
                        break;
                    case 0x0391: pImpRec->nYAlign = nUDData; break;
                    case 0x0392:
                        pImpRec->nYRelTo = nUDData;
                        break;
                    case 0x03BF: pImpRec->nGroupShapeBooleanProperties = nUDData; break;
                    case 0x0393:
                    // This seems to correspond to o:hrpct from .docx (even including
                    // the difference that it's in 0.1% even though the .docx spec
                    // says it's in 1%).
                        pImpRec->relativeHorizontalWidth = nUDData;
                        break;
                    case 0x0394:
                    // And this is really just a guess, but a mere presence of this
                    // flag makes a horizontal rule be as wide as the page (unless
                    // overridden by something), so it probably matches o:hr from .docx.
                        pImpRec->isHorizontalRule = true;
                        break;
                }
                if (!rSt.good())
                    break;
                nBytesLeft  -= 6;
            }
        }

        //  text frame, also Title or Outline
        rtl::Reference<SdrObject> pOrgObj  = pObj;
        rtl::Reference<SdrRectObj> pTextObj;
        sal_uInt32 nTextId = GetPropertyValue( DFF_Prop_lTxid, 0 );
        if( nTextId )
        {
            SfxItemSet aSet( pSdrModel->GetItemPool() );

            //Originally anything that as a mso_sptTextBox was created as a
            //textbox, this was changed for #88277# to be created as a simple
            //rect to keep impress happy. For the rest of us we'd like to turn
            //it back into a textbox again.
            bool bTextFrame = (pImpRec->eShapeType == mso_sptTextBox);
            if (!bTextFrame)
            {
                //Either
                //a) it's a simple text object or
                //b) it's a rectangle with text and square wrapping.
                bTextFrame =
                (
                    (pImpRec->eShapeType == mso_sptTextSimple) ||
                    (
                        (pImpRec->eShapeType == mso_sptRectangle)
                        && (eWrapMode == mso_wrapSquare)
                        && ShapeHasText(pImpRec->nShapeId, rObjData.rSpHd.GetRecBegFilePos() )
                    )
                );
            }

            if (bTextFrame)
            {
                pObj = nullptr;
                pOrgObj = nullptr;
            }

            // Distance of Textbox to its surrounding Customshape
            sal_Int32 nTextLeft = GetPropertyValue( DFF_Prop_dxTextLeft, 91440L);
            sal_Int32 nTextRight = GetPropertyValue( DFF_Prop_dxTextRight, 91440L );
            sal_Int32 nTextTop = GetPropertyValue( DFF_Prop_dyTextTop, 45720L  );
            sal_Int32 nTextBottom = GetPropertyValue( DFF_Prop_dyTextBottom, 45720L );

            ScaleEmu( nTextLeft );
            ScaleEmu( nTextRight );
            ScaleEmu( nTextTop );
            ScaleEmu( nTextBottom );

            Degree100 nTextRotationAngle(0);
            bool bVerticalText = false;
            if ( IsProperty( DFF_Prop_txflTextFlow ) )
            {
                auto eTextFlow = GetPropertyValue(DFF_Prop_txflTextFlow, 0) & 0xFFFF;
                switch( eTextFlow )
                {
                    case mso_txflBtoT:
                        nTextRotationAngle = 9000_deg100;
                    break;
                    case mso_txflVertN:
                    case mso_txflTtoBN:
                        nTextRotationAngle = 27000_deg100;
                        break;
                    case mso_txflTtoBA:
                        bVerticalText = true;
                    break;
                    case mso_txflHorzA:
                        bVerticalText = true;
                        nTextRotationAngle = 9000_deg100;
                    break;
                    case mso_txflHorzN:
                    default :
                        break;
                }
            }

            if (nTextRotationAngle)
            {
                switch (nTextRotationAngle.get())
                {
                    case 9000:
                        {
                            tools::Long nWidth = rTextRect.GetWidth();
                            rTextRect.SetRight( rTextRect.Left() + rTextRect.GetHeight() );
                            rTextRect.SetBottom( rTextRect.Top() + nWidth );

                            sal_Int32 nOldTextLeft = nTextLeft;
                            sal_Int32 nOldTextRight = nTextRight;
                            sal_Int32 nOldTextTop = nTextTop;
                            sal_Int32 nOldTextBottom = nTextBottom;

                            nTextLeft = nOldTextBottom;
                            nTextRight = nOldTextTop;
                            nTextTop = nOldTextLeft;
                            nTextBottom = nOldTextRight;
                        }
                        break;
                    case 27000:
                        {
                            tools::Long nWidth = rTextRect.GetWidth();
                            rTextRect.SetRight( rTextRect.Left() + rTextRect.GetHeight() );
                            rTextRect.SetBottom( rTextRect.Top() + nWidth );

                            sal_Int32 nOldTextLeft = nTextLeft;
                            sal_Int32 nOldTextRight = nTextRight;
                            sal_Int32 nOldTextTop = nTextTop;
                            sal_Int32 nOldTextBottom = nTextBottom;

                            nTextLeft = nOldTextTop;
                            nTextRight = nOldTextBottom;
                            nTextTop = nOldTextRight;
                            nTextBottom = nOldTextLeft;
                        }
                        break;
                }
            }

            pTextObj = new SdrRectObj(*pSdrModel, rTextRect, SdrObjKind::Text);
            pTextImpRec = new SvxMSDffImportRec(*pImpRec);
            bDeleteTextImpRec = true;

            // the vertical paragraph indents are part of the BoundRect,
            // here we 'remove' them by calculating
            tools::Rectangle aNewRect(rTextRect);
            aNewRect.AdjustBottom( -(nTextTop + nTextBottom) );
            aNewRect.AdjustRight( -(nTextLeft + nTextRight) );

            // Only if it's a simple textbox may Writer replace
            // the object with a frame, otherwise
            if( bTextFrame )
            {
                auto const pTmpRec = std::make_shared<SvxMSDffShapeInfo>(0, pImpRec->nShapeId);

                SvxMSDffShapeInfos_ById::const_iterator const it =
                    m_xShapeInfosById->find(pTmpRec);
                if (it != m_xShapeInfosById->end())
                {
                    SvxMSDffShapeInfo& rInfo = **it;
                    pTextImpRec->bReplaceByFly   = rInfo.bReplaceByFly;
                }
            }

            if( !pObj )
                ApplyAttributes( rSt, aSet, rObjData );

            bool bFitText = false;
            if (GetPropertyValue(DFF_Prop_FitTextToShape, 0) & 2)
            {
                aSet.Put( makeSdrTextAutoGrowHeightItem( true ) );
                aSet.Put( makeSdrTextMinFrameHeightItem(
                    aNewRect.Bottom() - aNewRect.Top() ) );
                aSet.Put( makeSdrTextMinFrameWidthItem(
                    aNewRect.Right() - aNewRect.Left() ) );
                bFitText = true;
            }
            else
            {
                aSet.Put( makeSdrTextAutoGrowHeightItem( false ) );
                aSet.Put( makeSdrTextAutoGrowWidthItem( false ) );
            }

            switch (GetPropertyValue(DFF_Prop_WrapText, mso_wrapSquare))
            {
                case mso_wrapNone :
                    aSet.Put( makeSdrTextAutoGrowWidthItem( true ) );
                    if (bFitText)
                    {
                        //can't do autowidth in flys #i107184#
                        pTextImpRec->bReplaceByFly = false;
                    }
                break;
                case mso_wrapByPoints :
                    aSet.Put( makeSdrTextContourFrameItem( true ) );
                break;
                default: break;
            }

            // set margins at the border of the textbox
            aSet.Put( makeSdrTextLeftDistItem( nTextLeft ) );
            aSet.Put( makeSdrTextRightDistItem( nTextRight ) );
            aSet.Put( makeSdrTextUpperDistItem( nTextTop ) );
            aSet.Put( makeSdrTextLowerDistItem( nTextBottom ) );
            pTextImpRec->nDxTextLeft    = nTextLeft;
            pTextImpRec->nDyTextTop     = nTextTop;
            pTextImpRec->nDxTextRight   = nTextRight;
            pTextImpRec->nDyTextBottom  = nTextBottom;

            // read text anchor
            if ( IsProperty( DFF_Prop_anchorText ) )
            {
                auto eTextAnchor = GetPropertyValue(DFF_Prop_anchorText, 0);

                SdrTextVertAdjust eTVA = SDRTEXTVERTADJUST_CENTER;
                bool bTVASet(false);
                bool bTHASet(false);

                switch( eTextAnchor )
                {
                    case mso_anchorTop:
                    {
                        eTVA = SDRTEXTVERTADJUST_TOP;
                        bTVASet = true;
                    }
                    break;
                    case mso_anchorTopCentered:
                    {
                        eTVA = SDRTEXTVERTADJUST_TOP;
                        bTVASet = true;
                        bTHASet = true;
                    }
                    break;

                    case mso_anchorMiddle:
                        bTVASet = true;
                    break;
                    case mso_anchorMiddleCentered:
                    {
                        bTVASet = true;
                        bTHASet = true;
                    }
                    break;
                    case mso_anchorBottom:
                    {
                        eTVA = SDRTEXTVERTADJUST_BOTTOM;
                        bTVASet = true;
                    }
                    break;
                    case mso_anchorBottomCentered:
                    {
                        eTVA = SDRTEXTVERTADJUST_BOTTOM;
                        bTVASet = true;
                        bTHASet = true;
                    }
                    break;
                    default : break;
                }
                // insert
                if ( bTVASet )
                    aSet.Put( SdrTextVertAdjustItem( eTVA ) );
                if ( bTHASet )
                    aSet.Put( SdrTextHorzAdjustItem( SDRTEXTHORZADJUST_CENTER ) );
            }

            pTextObj->SetMergedItemSet(aSet);

            if (bVerticalText)
                pTextObj->SetVerticalWriting(true);

            if (nTextRotationAngle)
            {
                tools::Long nMinWH = rTextRect.GetWidth() < rTextRect.GetHeight() ?
                    rTextRect.GetWidth() : rTextRect.GetHeight();
                nMinWH /= 2;
                Point aPivot(rTextRect.TopLeft());
                aPivot.AdjustX(nMinWH );
                aPivot.AdjustY(nMinWH );
                pTextObj->SdrAttrObj::NbcRotate(aPivot, nTextRotationAngle);
            }

            // rotate text with shape?
            if ( mnFix16Angle )
            {
                double a = toRadians(mnFix16Angle);
                pTextObj->NbcRotate( rObjData.aBoundRect.Center(), mnFix16Angle,
                    sin( a ), cos( a ) );
            }

            if( !pObj )
            {
                pObj = pTextObj.get();
            }
            else
            {
                if( pTextObj.get() != pObj.get() )
                {
                    rtl::Reference<SdrObject> pGroup = new SdrObjGroup(*pSdrModel);
                    pGroup->GetSubList()->NbcInsertObject( pObj.get() );
                    pGroup->GetSubList()->NbcInsertObject( pTextObj.get() );
                    if (pOrgObj == pObj)
                        pOrgObj = pGroup;
                    else
                        pOrgObj = pObj;
                    pObj = pGroup.get();
                }
            }
        }
        else if( !pObj )
        {
            // simple rectangular objects are ignored by ImportObj()  :-(
            // this is OK for Draw but not for Calc and Writer
            // cause here these objects have a default border
            pObj = new SdrRectObj(
                *pSdrModel,
                rTextRect);

            pOrgObj = pObj;
            SfxItemSet aSet( pSdrModel->GetItemPool() );
            ApplyAttributes( rSt, aSet, rObjData );

            SfxItemState eState = aSet.GetItemState( XATTR_FILLCOLOR );
            if( SfxItemState::DEFAULT == eState )
                aSet.Put( XFillColorItem( OUString(), mnDefaultColor ) );
            pObj->SetMergedItemSet(aSet);
        }

        //Means that fBehindDocument is set
        if (GetPropertyValue(DFF_Prop_fPrint, 0) & 0x20)
            pImpRec->bDrawHell = true;
        else
            pImpRec->bDrawHell = false;
        if (GetPropertyValue(DFF_Prop_fPrint, 0) & 0x02)
            pImpRec->bHidden = true;
        pTextImpRec->bDrawHell  = pImpRec->bDrawHell;
        pTextImpRec->bHidden = pImpRec->bHidden;
        pImpRec->nNextShapeId   = GetPropertyValue( DFF_Prop_hspNext, 0 );
        pTextImpRec->nNextShapeId=pImpRec->nNextShapeId;

        if ( nTextId )
        {
            pTextImpRec->aTextId.nTxBxS = static_cast<sal_uInt16>( nTextId >> 16 );
            pTextImpRec->aTextId.nSequence = static_cast<sal_uInt16>(nTextId);
        }

        pTextImpRec->nDxWrapDistLeft = GetPropertyValue(
                                    DFF_Prop_dxWrapDistLeft, 114935L ) / 635L;
        pTextImpRec->nDyWrapDistTop = GetPropertyValue(
                                    DFF_Prop_dyWrapDistTop, 0 ) / 635L;
        pTextImpRec->nDxWrapDistRight = GetPropertyValue(
                                    DFF_Prop_dxWrapDistRight, 114935L ) / 635L;
        pTextImpRec->nDyWrapDistBottom = GetPropertyValue(
                                    DFF_Prop_dyWrapDistBottom, 0 ) / 635L;
        // 16.16 fraction times total image width or height, as appropriate.

        if (SeekToContent(DFF_Prop_pWrapPolygonVertices, rSt))
        {
            pTextImpRec->pWrapPolygon.reset();
            sal_uInt16 nNumElemVert(0), nNumElemMemVert(0), nElemSizeVert(8);
            rSt.ReadUInt16( nNumElemVert ).ReadUInt16( nNumElemMemVert ).ReadUInt16( nElemSizeVert );
            // If this value is 0xFFF0 then this record is an array of truncated 8 byte elements. Only the 4
            // low-order bytes are recorded
            if (nElemSizeVert == 0xFFF0)
                nElemSizeVert = 4;

            // sanity check that the stream is long enough to fulfill nNumElemVert * nElemSizeVert;
            bool bOk = nElemSizeVert && (rSt.remainingSize() / nElemSizeVert >= nNumElemVert);
            if (bOk)
            {
                pTextImpRec->pWrapPolygon = tools::Polygon(nNumElemVert);
                for (sal_uInt16 i = 0; i < nNumElemVert; ++i)
                {
                    sal_Int32 nX(0), nY(0);
                    if (nElemSizeVert == 8)
                        rSt.ReadInt32( nX ).ReadInt32( nY );
                    else
                    {
                        sal_Int16 nSmallX(0), nSmallY(0);
                        rSt.ReadInt16( nSmallX ).ReadInt16( nSmallY );
                        nX = nSmallX;
                        nY = nSmallY;
                    }
                    (*(pTextImpRec->pWrapPolygon))[i].setX( nX );
                    (*(pTextImpRec->pWrapPolygon))[i].setY( nY );
                }
            }
        }

        pImpRec->nCropFromTop = GetPropertyValue(
                                    DFF_Prop_cropFromTop, 0 );
        pImpRec->nCropFromBottom = GetPropertyValue(
                                    DFF_Prop_cropFromBottom, 0 );
        pImpRec->nCropFromLeft = GetPropertyValue(
                                    DFF_Prop_cropFromLeft, 0 );
        pImpRec->nCropFromRight = GetPropertyValue(
                                    DFF_Prop_cropFromRight, 0 );

        pImpRec->bVFlip = bool(rObjData.nSpFlags & ShapeFlag::FlipV);
        pImpRec->bHFlip = bool(rObjData.nSpFlags & ShapeFlag::FlipH);

        sal_uInt32 nLineFlags = GetPropertyValue( DFF_Prop_fNoLineDrawDash, 0 );
        pImpRec->eLineStyle = (nLineFlags & 8)
                            ? static_cast<MSO_LineStyle>(GetPropertyValue(
                                                DFF_Prop_lineStyle,
                                                mso_lineSimple ))
                            : MSO_LineStyle_NONE;
        pTextImpRec->eLineStyle = pImpRec->eLineStyle;

        pImpRec->eLineDashing = static_cast<MSO_LineDashing>(GetPropertyValue(
                DFF_Prop_lineDashing, mso_lineSolid ));
        pTextImpRec->eLineDashing = pImpRec->eLineDashing;

        if( pImpRec->nShapeId )
        {
            // amend the import record list
            if( pOrgObj )
            {
                pImpRec->pObj = pOrgObj.get();
                rImportData.insert(std::unique_ptr<SvxMSDffImportRec>(pImpRec));
                bDeleteImpRec = false;
                if (pImpRec == pTextImpRec)
                    bDeleteTextImpRec = false;
            }

            if( pTextObj && (pOrgObj != pTextObj) )
            {
                // Modify ShapeId (must be unique)
                pImpRec->nShapeId |= 0x8000000;
                pTextImpRec->pObj = pTextObj.get();
                rImportData.insert(std::unique_ptr<SvxMSDffImportRec>(pTextImpRec));
                bDeleteTextImpRec = false;
                if (pTextImpRec == pImpRec)
                    bDeleteImpRec = false;
            }

            // entry in the z-order-list in order to complement the pointer to this object
            /*Only store objects which are not deep inside the tree*/
            if( ( rObjData.nCalledByGroup == 0 )
                ||
                ( (rObjData.nSpFlags & ShapeFlag::Group)
                 && (rObjData.nCalledByGroup < 2) )
              )
                StoreShapeOrder( pImpRec->nShapeId,
                                ( static_cast<sal_uLong>(pImpRec->aTextId.nTxBxS) << 16 )
                                    + pImpRec->aTextId.nSequence, pObj.get() );
        }

        if (bDeleteImpRec)
            delete pImpRec;

        if (bDeleteTextImpRec)
            delete pTextImpRec;
    }

    return pObj;
};

SdrObject* SvxMSDffManager::FinalizeObj(DffObjData& /* rObjData */, SdrObject* pObj)
{
    return pObj;
}


void SvxMSDffManager::StoreShapeOrder(sal_uLong         nId,
                                      sal_uLong         nTxBx,
                                      SdrObject*    pObject,
                                      SwFlyFrameFormat*  pFly) const
{
    for (const auto& pOrder : m_aShapeOrders)
    {
        if (pOrder->nShapeId == nId)
        {
            pOrder->nTxBxComp = nTxBx;
            pOrder->pObj = pObject;
            pOrder->pFly = pFly;
        }
    }
}


void SvxMSDffManager::ExchangeInShapeOrder( SdrObject const * pOldObject,
                                            sal_uLong    nTxBx,
                                            SdrObject*   pObject) const
{
    for (const auto& pOrder : m_aShapeOrders)
    {
        if (pOrder->pObj == pOldObject)
        {
            pOrder->pFly = nullptr;
            pOrder->pObj = pObject;
            pOrder->nTxBxComp = nTxBx;
        }
    }
}


void SvxMSDffManager::RemoveFromShapeOrder( SdrObject const * pObject ) const
{
    for (const auto& pOrder : m_aShapeOrders)
    {
        if (pOrder->pObj == pObject)
        {
            pOrder->pObj = nullptr;
            pOrder->pFly = nullptr;
            pOrder->nTxBxComp = 0;
        }
    }
}


//  exported class: Public Methods

SvxMSDffManager::SvxMSDffManager(SvStream& rStCtrl_,
                                 OUString aBaseURL,
                                 sal_uInt32 nOffsDgg_,
                                 SvStream* pStData_,
                                 SdrModel* pSdrModel_,// see SetModel() below
                                 tools::Long      nApplicationScale,
                                 Color     mnDefaultColor_,
                                 SvStream* pStData2_,
                                 bool bSkipImages )
    :DffPropertyReader( *this ),
     m_xShapeInfosByTxBxComp( new SvxMSDffShapeInfos_ByTxBxComp ),
     nOffsDgg( nOffsDgg_ ),
     nBLIPCount(  USHRT_MAX ),              // initialize with error, since we first check if the
     nGroupShapeFlags(ShapeFlag::NONE),     // ensure initialization here, as some corrupted
                                            // files may yield to this being uninitialized
     maBaseURL(std::move( aBaseURL )),
     mnIdClusters(0),
     rStCtrl(  rStCtrl_  ),
     pStData(  pStData_  ),
     pStData2( pStData2_ ),
     nSvxMSDffSettings( 0 ),
     nSvxMSDffOLEConvFlags( 0 ),
     mnDefaultColor( mnDefaultColor_),
     mbSkipImages (bSkipImages)
{
    SetModel( pSdrModel_, nApplicationScale );

    // remember FilePos of the stream(s)
    sal_uInt64 nOldPosCtrl = rStCtrl.Tell();
    sal_uInt64 nOldPosData = pStData ? pStData->Tell() : nOldPosCtrl;

    // if no data stream is given we assume that the BLIPs
    // are in the control stream
    if( !pStData )
        pStData = &rStCtrl;

    SetDefaultPropSet( rStCtrl, nOffsDgg );

    // read control stream, if successful set nBLIPCount
    GetCtrlData( nOffsDgg );

    // check Text-Box-Story-Chain-Infos
    CheckTxBxStoryChain();

    // restore old FilePos of the stream(s)
    rStCtrl.Seek( nOldPosCtrl );
    if( &rStCtrl != pStData )
        pStData->Seek( nOldPosData );
}

SvxMSDffManager::SvxMSDffManager( SvStream& rStCtrl_, OUString aBaseURL )
    :DffPropertyReader( *this ),
     m_xShapeInfosByTxBxComp( new SvxMSDffShapeInfos_ByTxBxComp ),
     nOffsDgg( 0 ),
     nBLIPCount(  USHRT_MAX ),              // initialize with error, since we first have to check
     nGroupShapeFlags(ShapeFlag::NONE),
     maBaseURL(std::move( aBaseURL )),
     mnIdClusters(0),
     rStCtrl(  rStCtrl_  ),
     pStData( nullptr ),
     pStData2( nullptr ),
     nSvxMSDffSettings( 0 ),
     nSvxMSDffOLEConvFlags( 0 ),
     mnDefaultColor( COL_DEFAULT ),
     mbSkipImages(false)
{
    SetModel( nullptr, 0 );
}

SvxMSDffManager::~SvxMSDffManager()
{
}

void SvxMSDffManager::InitSvxMSDffManager( sal_uInt32 nOffsDgg_, SvStream* pStData_, sal_uInt32 nOleConvFlags )
{
    nOffsDgg = nOffsDgg_;
    pStData = pStData_;
    nSvxMSDffOLEConvFlags = nOleConvFlags;

    // remember FilePos of the stream(s)
    sal_uInt64 nOldPosCtrl = rStCtrl.Tell();

    SetDefaultPropSet( rStCtrl, nOffsDgg );

    // insert fidcl cluster table
    GetFidclData( nOffsDgg );

    // read control stream, if successful, set nBLIPCount
    GetCtrlData( nOffsDgg );

    // check Text-Box-Story-Chain-Infos
    CheckTxBxStoryChain();

    // restore old FilePos of the stream(s)
    rStCtrl.Seek( nOldPosCtrl );
}

void SvxMSDffManager::SetDgContainer( SvStream& rSt )
{
    sal_uInt64 nFilePos = rSt.Tell();
    DffRecordHeader aDgContHd;
    bool bOk = ReadDffRecordHeader(rSt, aDgContHd);
    // insert this container only if there is also a DggAtom
    if (bOk && SeekToRec(rSt, DFF_msofbtDg, aDgContHd.GetRecEndFilePos()))
    {
        DffRecordHeader aRecHd;
        if (ReadDffRecordHeader(rSt, aRecHd))
        {
            sal_uInt32 nDrawingId = aRecHd.nRecInstance;
            maDgOffsetTable[nDrawingId] = nFilePos;
        }
    }
    rSt.Seek(nFilePos);
}

void SvxMSDffManager::GetFidclData( sal_uInt32 nOffsDggL )
{
    if (!nOffsDggL)
        return;

    sal_uInt64 nOldPos = rStCtrl.Tell();

    if (nOffsDggL == rStCtrl.Seek(nOffsDggL))
    {
        DffRecordHeader aRecHd;
        bool bOk = ReadDffRecordHeader(rStCtrl, aRecHd);

        DffRecordHeader aDggAtomHd;
        if (bOk && SeekToRec(rStCtrl, DFF_msofbtDgg, aRecHd.GetRecEndFilePos(), &aDggAtomHd))
        {
            aDggAtomHd.SeekToContent( rStCtrl );
            sal_uInt32 nCurMaxShapeId;
            sal_uInt32 nDummy;
            rStCtrl.ReadUInt32( nCurMaxShapeId )
                   .ReadUInt32( mnIdClusters )
                   .ReadUInt32( nDummy )
                   .ReadUInt32( nDummy ); // nDrawingsSaved

            if ( mnIdClusters-- > 2 )
            {
                const std::size_t nFIDCLsize = sizeof(sal_uInt32) * 2;
                if ( aDggAtomHd.nRecLen == ( mnIdClusters * nFIDCLsize + 16 ) )
                {
                    sal_uInt64 nMaxEntriesPossible = rStCtrl.remainingSize() / nFIDCLsize;
                    SAL_WARN_IF(nMaxEntriesPossible < mnIdClusters,
                        "filter.ms", "FIDCL list longer than remaining bytes, ppt or parser is wrong");
                    mnIdClusters = std::min(nMaxEntriesPossible, static_cast<sal_uInt64>(mnIdClusters));

                    maFidcls.resize(mnIdClusters);
                    for (sal_uInt32 i = 0; i < mnIdClusters; ++i)
                    {
                        sal_uInt32  cspidCur;   ///< number of SPIDs used so far
                        rStCtrl.ReadUInt32( maFidcls[ i ].dgid )
                               .ReadUInt32( cspidCur );
                    }
                }
            }
        }
    }
    rStCtrl.Seek( nOldPos );
}

void SvxMSDffManager::CheckTxBxStoryChain()
{
    m_xShapeInfosById.reset(new SvxMSDffShapeInfos_ById);
    // mangle old Info array, sorted by nTxBxComp
    sal_uInt32 nChain = std::numeric_limits<sal_uInt32>::max();
    bool bSetReplaceFALSE = false;
    for (SvxMSDffShapeInfos_ByTxBxComp::iterator iter =
                m_xShapeInfosByTxBxComp->begin(),
            mark = m_xShapeInfosByTxBxComp->begin();
         iter != m_xShapeInfosByTxBxComp->end(); ++iter)
    {
        std::shared_ptr<SvxMSDffShapeInfo> const& pObj = *iter;
        if( pObj->nTxBxComp )
        {
            // group change?
            // the text id also contains an internal drawing container id
            // to distinguish between text id of drawing objects in different
            // drawing containers.
            if( nChain != pObj->nTxBxComp )
            {
                // reset mark and helper flag
                mark = iter;
                nChain = pObj->nTxBxComp;
                bSetReplaceFALSE = !pObj->bReplaceByFly;
            }
            else if( !pObj->bReplaceByFly )
            {
                // object that must NOT be replaced by frame?
                bSetReplaceFALSE = true;
                // maybe reset flags in start of group
                for (SvxMSDffShapeInfos_ByTxBxComp::iterator itemp = mark;
                        itemp != iter; ++itemp)
                {
                    (*itemp)->bReplaceByFly = false;
                }
            }

            if( bSetReplaceFALSE )
            {
                pObj->bReplaceByFly = false;
            }
        }
        // copy all Shape Info objects to m_xShapeInfosById, sorted by nShapeId
        pObj->nTxBxComp = pObj->nTxBxComp & 0xFFFF0000;
        m_xShapeInfosById->insert( pObj );
    }
    // free original array but don't free its elements
    m_xShapeInfosByTxBxComp.reset();
}


/*****************************************************************************

    Reading the Shape-Infos in the Ctor:
    ---------------------------------
    remembering the Shape-Ids and the associated Blip-Numbers and TextBox-Infos
                    =========                    ============     =============
    and remembering the File-Offsets for each Blip
                       ============
******************************************************************************/
void SvxMSDffManager::GetCtrlData(sal_uInt32 nOffsDggL)
{
    // position control stream
    if (!checkSeek(rStCtrl, nOffsDggL))
        return;

    sal_uInt8 nVer(0);
    sal_uInt16 nInst(0);
    sal_uInt16 nFbt(0);
    sal_uInt32 nLength(0);
    if( !ReadCommonRecordHeader( rStCtrl, nVer, nInst, nFbt, nLength ) ) return;

    sal_uInt64 nPos = nOffsDggL + DFF_COMMON_RECORD_HEADER_SIZE;

    // case A: first Drawing Group Container, then n times Drawing Container
    if( DFF_msofbtDggContainer != nFbt )
        return;

    bool bOk;
    GetDrawingGroupContainerData( rStCtrl, nLength );

    sal_uInt64 nMaxStrPos = rStCtrl.TellEnd();

    nPos += nLength;
    sal_uInt16 nDrawingContainerId = 1;
    do
    {
        if (!checkSeek(rStCtrl, nPos))
            break;

        bOk = ReadCommonRecordHeader( rStCtrl, nVer, nInst, nFbt, nLength ) && ( DFF_msofbtDgContainer == nFbt );

        if( !bOk )
        {
            nPos++;                // ????????? TODO: trying to get a one-hit wonder, this code should be rewritten...
            if (nPos != rStCtrl.Seek(nPos))
                break;
            bOk = ReadCommonRecordHeader( rStCtrl, nVer, nInst, nFbt, nLength )
                    && ( DFF_msofbtDgContainer == nFbt );
        }
        if( bOk )
        {
            GetDrawingContainerData( rStCtrl, nLength, nDrawingContainerId );
        }
        nPos += DFF_COMMON_RECORD_HEADER_SIZE + nLength;
        ++nDrawingContainerId;
    }
    while( ( rStCtrl.GetError() == ERRCODE_NONE ) && ( nPos < nMaxStrPos ) && bOk );
}


// from here on: Drawing Group Container  i.e. document-wide valid data

void SvxMSDffManager::GetDrawingGroupContainerData( SvStream& rSt, sal_uInt32 nLenDgg )
{
    sal_uInt8   nVer;
    sal_uInt16 nInst;
    sal_uInt16 nFbt;
    sal_uInt32 nLength;

    sal_uInt32 nLenBStoreCont = 0, nLenFBSE = 0;
    sal_uLong nRead = 0;

    // search for a  BStore Container
    bool bOk = true;
    do
    {
        if (!ReadCommonRecordHeader(rSt, nVer, nInst, nFbt, nLength))
            return;
        nRead += DFF_COMMON_RECORD_HEADER_SIZE + nLength;
        if (DFF_msofbtBstoreContainer == nFbt)
        {
            nLenBStoreCont = nLength;
            break;
        }
        bOk = checkSeek(rSt, rSt.Tell() + nLength);
    }
    while (bOk && nRead < nLenDgg);

    if (!bOk || !nLenBStoreCont)
        return;

    // Read all atoms of the containers from the BStore container and store all
    // relevant data of all contained FBSEs in out pointer array.
    // We also count all found FBSEs in member nBLIPCount.

    const sal_uInt8 nSkipBLIPLen = 20;  // skip to get to the nBLIPLen
    const sal_uInt8 nSkipBLIPPos =  4;  // thereafter skip up to nBLIPPos

    sal_uInt32 nBLIPLen = 0, nBLIPPos = 0;

    nRead = 0;
    do
    {
        if(!ReadCommonRecordHeader( rSt, nVer, nInst, nFbt, nLength)) return;
        nRead += DFF_COMMON_RECORD_HEADER_SIZE + nLength;
        if( DFF_msofbtBSE == nFbt && /* magic value from spec */ 0x2 == nVer )
        {
            nLenFBSE = nLength;
            // is FBSE big enough for our data
            bOk = ( nSkipBLIPLen + 4 + nSkipBLIPPos + 4 <= nLenFBSE );

            if (bOk)
            {
                rSt.SeekRel( nSkipBLIPLen );
                rSt.ReadUInt32( nBLIPLen );
                rSt.SeekRel( nSkipBLIPPos );
                rSt.ReadUInt32( nBLIPPos );
                bOk = rSt.GetError() == ERRCODE_NONE;

                nLength -= nSkipBLIPLen+ 4 + nSkipBLIPPos + 4;
            }

            if (bOk)
            {
                // specialty:
                // If nBLIPLen is less than nLenFBSE AND nBLIPPos is NULL,
                // then we assume, that the image is in FBSE!
                if( (!nBLIPPos) && (nBLIPLen < nLenFBSE) )
                    nBLIPPos = rSt.Tell() + 4;

                if( USHRT_MAX == nBLIPCount )
                    nBLIPCount = 1;
                else
                    nBLIPCount++;

                // now save the info for later access
                m_aBLIPOffsets.push_back(nBLIPPos);
            }
            if (!checkSeek(rSt, rSt.Tell() + nLength))
                return; // invalid offset
        }
        else return; // invalid input
    }
    while( nRead < nLenBStoreCont );
}


// from now on: Drawing Container  which means Pages (Sheet, Slide) - wide valid data
//                      =================               ======

void SvxMSDffManager::GetDrawingContainerData( SvStream& rSt, sal_uInt32 nLenDg,
                                               sal_uInt16 nDrawingContainerId )
{
    sal_uInt8 nVer;sal_uInt16 nInst;sal_uInt16 nFbt(0);sal_uInt32 nLength(0);

    sal_uLong nReadDg = 0;

    // We are now in a drawing container (one per each page) and
    // we now have to iterate through all contained shape group containers
    do
    {
        if (!ReadCommonRecordHeader(rSt, nVer, nInst, nFbt, nLength))
            return;
        nReadDg += DFF_COMMON_RECORD_HEADER_SIZE;
        // Patriarch found (the upmost shape group container) ?
        if (DFF_msofbtSpgrContainer == nFbt)
        {
            if (!GetShapeGroupContainerData(rSt, nLength, true, nDrawingContainerId))
                return;
        }
        // empty Shape Container ? (outside of shape group container)
        else if (DFF_msofbtSpContainer == nFbt)
        {
            if (!GetShapeContainerData(
                    rSt, nLength, std::numeric_limits<sal_uInt64>::max(), nDrawingContainerId))
                return;
        }
        else
        {
            if (!checkSeek(rSt, rSt.Tell() + nLength))
                return;
        }
        nReadDg += nLength;
    }
    while( nReadDg < nLenDg );
}

bool SvxMSDffManager::GetShapeGroupContainerData( SvStream& rSt,
                                                  sal_uInt32 nLenShapeGroupCont,
                                                  bool bPatriarch,
                                                  sal_uInt16 nDrawingContainerId )
{
    sal_uInt8 nVer;sal_uInt16 nInst;sal_uInt16 nFbt;sal_uInt32 nLength;
    sal_uInt64 nStartShapeGroupCont = rSt.Tell();
    // We are now in a shape group container (conditionally multiple per page)
    // and we now have to iterate through all contained shape containers
    bool  bFirst = !bPatriarch;
    sal_uLong nReadSpGrCont = 0;
    do
    {
        if( !ReadCommonRecordHeader( rSt, nVer, nInst, nFbt, nLength ) )
            return false;
        nReadSpGrCont += DFF_COMMON_RECORD_HEADER_SIZE;
        // Shape Container?
        if( DFF_msofbtSpContainer == nFbt )
        {
            sal_uInt64 nGroupOffs = bFirst ? nStartShapeGroupCont - DFF_COMMON_RECORD_HEADER_SIZE : std::numeric_limits<sal_uInt64>::max();
            if ( !GetShapeContainerData( rSt, nLength, nGroupOffs, nDrawingContainerId ) )
                return false;
            bFirst = false;
        }
        // nested shape group container ?
        else if( DFF_msofbtSpgrContainer == nFbt )
        {
            if ( !GetShapeGroupContainerData( rSt, nLength, false, nDrawingContainerId ) )
                return false;
        }
        else
        {
            if (!checkSeek(rSt, rSt.Tell() + nLength))
                return false;
        }
        nReadSpGrCont += nLength;
    }
    while( nReadSpGrCont < nLenShapeGroupCont );
    // position the stream correctly
    rSt.Seek( nStartShapeGroupCont + nLenShapeGroupCont );
    return true;
}

bool SvxMSDffManager::GetShapeContainerData( SvStream& rSt,
                                             sal_uInt32 nLenShapeCont,
                                             sal_uInt64 nPosGroup,
                                             sal_uInt16 nDrawingContainerId )
{
    sal_uInt8 nVer;sal_uInt16 nInst;sal_uInt16 nFbt;sal_uInt32 nLength;
    sal_uInt64 nStartShapeCont = rSt.Tell();

    // We are in a shape container (possibly more than one per shape group) and we now
    // have to fetch the shape id and file position (to be able to access them again later)
    // and the first BStore reference (if present).
    sal_uInt32 nLenShapePropTbl = 0;
    sal_uLong nReadSpCont = 0;

    // Store file offset of the shape containers or respectively the group(!).
    sal_uInt64 nStartOffs = (std::numeric_limits<sal_uInt64>::max() > nPosGroup) ?
                            nPosGroup : nStartShapeCont - DFF_COMMON_RECORD_HEADER_SIZE;
    SvxMSDffShapeInfo aInfo( nStartOffs );

    // Can the shape be replaced with a frame?
    // (provided that it is a TextBox and the text is not rotated)
    bool bCanBeReplaced = nPosGroup >= std::numeric_limits<sal_uInt64>::max();

    // we don't know yet whether it's a TextBox
    MSO_SPT         eShapeType      = mso_sptNil;

    // analyze Shape

    do
    {
        if(!ReadCommonRecordHeader( rSt, nVer, nInst, nFbt, nLength)) return false;
        nReadSpCont += DFF_COMMON_RECORD_HEADER_SIZE;
        // FSP ?
        if( ( DFF_msofbtSp == nFbt ) && ( 4 <= nLength ) )
        {
            // we've found the FSP: note Shape Type and Id!
            eShapeType = static_cast<MSO_SPT>(nInst);
            rSt.ReadUInt32( aInfo.nShapeId );
            rSt.SeekRel( nLength - 4 );
            nReadSpCont += nLength;
        }
        else if( DFF_msofbtOPT == nFbt ) // Shape Property Table ?
        {
            // We've found the Property Table:
            // search for the Blip Property!
            sal_uLong  nPropRead = 0;
            nLenShapePropTbl = nLength;
            auto nStartShapePropTbl = rSt.Tell();
            do
            {
                sal_uInt16 nPropId(0);
                sal_uInt32 nPropVal(0);

                rSt.ReadUInt16( nPropId )
                   .ReadUInt32( nPropVal );
                nPropRead += 6;

                switch( nPropId )
                {
                    case DFF_Prop_txflTextFlow :
                        //Writer can now handle vertical textflows in its
                        //native frames, to only need to do this for the
                        //other two formats

                        //Writer will handle all textflow except BtoT
                        if (GetSvxMSDffSettings() &
                            (SVXMSDFF_SETTINGS_IMPORT_PPT |
                             SVXMSDFF_SETTINGS_IMPORT_EXCEL))
                        {
                            if( 0 != nPropVal )
                                bCanBeReplaced = false;
                        }
                        else if (
                            (nPropVal != mso_txflHorzN) &&
                            (nPropVal != mso_txflTtoBA)
                                )
                        {
                            bCanBeReplaced = false;
                        }
                    break;
                    case DFF_Prop_cdirFont :
                        //Writer can now handle right to left and left
                        //to right in its native frames, so only do
                        //this for the other two formats.
                        if (GetSvxMSDffSettings() &
                            (SVXMSDFF_SETTINGS_IMPORT_PPT |
                             SVXMSDFF_SETTINGS_IMPORT_EXCEL))
                        {
                            if( 0 != nPropVal )
                                bCanBeReplaced = false;
                        }
                    break;
                    case DFF_Prop_Rotation :
                        if( 0 != nPropVal )
                            bCanBeReplaced = false;
                    break;

                    case DFF_Prop_gtextFStrikethrough :
                        if( ( 0x20002000 & nPropVal )  == 0x20002000 )
                            bCanBeReplaced = false;
                    break;

                    case DFF_Prop_fc3DLightFace :
                        if( ( 0x00080008 & nPropVal ) == 0x00080008 )
                            bCanBeReplaced = false;
                    break;

                    case DFF_Prop_WrapText :
                        //TODO: eWrapMode = (MSO_WrapMode)nPropVal;
                    break;

                    default:
                    {
                        // is the Bit set and valid?
                        if( 0x4000 == ( nPropId & 0xC000 ) )
                        {
                            // Blip Property found: remember BStore Idx!
                            nPropRead = nLenShapePropTbl;
                        }
                        else if( 0x8000 & nPropId )
                        {
                            // complex Prop found:
                            // Length is always 6. The length of the appended extra data
                            // after the actual prop table is of different size.
                        }
                    }
                    break;
                }
            }
            while (rSt.good() && nPropRead < nLenShapePropTbl);
            rSt.Seek( nStartShapePropTbl + nLenShapePropTbl );
            nReadSpCont += nLenShapePropTbl;
        }
        else if( ( DFF_msofbtClientTextbox == nFbt ) && ( 4 == nLength ) )  // Text-Box-Story-Entry found
        {
            rSt.ReadUInt32( aInfo.nTxBxComp );
            // Add internal drawing container id to text id.
            // Note: The text id uses the first two bytes, while the internal
            // drawing container id used the second two bytes.
            aInfo.nTxBxComp = ( aInfo.nTxBxComp & 0xFFFF0000 ) +
                              nDrawingContainerId;
            DBG_ASSERT( (aInfo.nTxBxComp & 0x0000FFFF) == nDrawingContainerId,
                        "<SvxMSDffManager::GetShapeContainerData(..)> - internal drawing container Id could not be correctly merged into DFF_msofbtClientTextbox value." );
        }
        else
        {
            if (!checkSeek(rSt, rSt.Tell() + nLength))
            {
                SAL_WARN("filter.ms", "remaining record longer than available data, ppt or parser is wrong");
                break;
            }
            nReadSpCont += nLength;
        }
    }
    while( nReadSpCont < nLenShapeCont );


    // Now possibly store the information for subsequent accesses to the shape

    if( aInfo.nShapeId )
    {
        // Possibly allow replacement of textboxes with frames
        if(     bCanBeReplaced
             && aInfo.nTxBxComp
             && (
                    ( eShapeType == mso_sptTextSimple )
                 || ( eShapeType == mso_sptTextBox    )
                 || ( eShapeType == mso_sptRectangle  )
                 || ( eShapeType == mso_sptRoundRectangle )
                ) )
        {
            aInfo.bReplaceByFly = true;
        }
        m_xShapeInfosByTxBxComp->insert(std::make_shared<SvxMSDffShapeInfo>(
                    aInfo));
        m_aShapeOrders.push_back(std::make_unique<SvxMSDffShapeOrder>(
                    aInfo.nShapeId ));
    }

    // and position the Stream correctly again
    rSt.Seek( nStartShapeCont + nLenShapeCont );
    return true;
}


/*****************************************************************************

    Access to a shape at runtime (via the Shape-Id)
    ----------------------------
******************************************************************************/
bool SvxMSDffManager::GetShape(sal_uLong nId, rtl::Reference<SdrObject>& rpShape,
                                          SvxMSDffImportData& rData)
{
    auto const pTmpRec = std::make_shared<SvxMSDffShapeInfo>(0, nId);

    SvxMSDffShapeInfos_ById::const_iterator const it =
        m_xShapeInfosById->find(pTmpRec);
    if (it == m_xShapeInfosById->end())
        return false;

    // Possibly delete old error flag.
    if( rStCtrl.GetError() )
        rStCtrl.ResetError();
    // store FilePos of the stream(s)
    sal_uInt64 nOldPosCtrl = rStCtrl.Tell();
    sal_uInt64 nOldPosData = pStData ? pStData->Tell() : nOldPosCtrl;
    // jump to the shape in the control stream
    sal_uInt64 const nFilePos((*it)->nFilePos);
    bool bSeeked = (nFilePos == rStCtrl.Seek(nFilePos));

    // if it failed, reset error statusF
    if (!bSeeked || rStCtrl.GetError())
        rStCtrl.ResetError();
    else
        rpShape = ImportObj( rStCtrl, rData, rData.aParentRect, rData.aParentRect, /*nCalledByGroup*/0, /*pShapeId*/nullptr );

    // restore old FilePos of the stream(s)
    rStCtrl.Seek( nOldPosCtrl );
    if( &rStCtrl != pStData && pStData )
        pStData->Seek( nOldPosData );
    return bool( rpShape );
}


/** Access to a BLIP at runtime (if the Blip-Number is already known)
 */
bool SvxMSDffManager::GetBLIP( sal_uLong nIdx_, Graphic& rGraphic, tools::Rectangle* pVisArea )
{
    if (!pStData)
        return false;

    bool bOk = false;       // initialize result variable

    // check if a graphic for this blipId is already imported
    if (nIdx_)
    {
        auto iter = aEscherBlipCache.find(nIdx_);

        if (iter != aEscherBlipCache.end())
        {
            /* if this entry is available */
            rGraphic = iter->second;
            if (rGraphic.GetType() != GraphicType::NONE)
                bOk = true;
            else
                aEscherBlipCache.erase(iter);
        }
    }

    if (!bOk)
    {
        sal_uInt16 nIdx = sal_uInt16( nIdx_ );
        if (!nIdx || (m_aBLIPOffsets.size() < nIdx))
            return false;

        // possibly delete old error flag(s)
        if( rStCtrl.GetError() )
            rStCtrl.ResetError();
        if(    ( &rStCtrl != pStData )
            && pStData->GetError() )
            pStData->ResetError();

        // remember FilePos of the stream(s)
        sal_uInt64 nOldPosCtrl = rStCtrl.Tell();
        sal_uInt64 nOldPosData = pStData->Tell();

        // fetch matching info struct out of the pointer array
        const sal_uInt32 nBlipFilePos = m_aBLIPOffsets[ nIdx-1 ];
        // jump to the BLIP atom in the data stream
        bOk = checkSeek(*pStData, nBlipFilePos);
        // possibly reset error status
        if (!bOk || pStData->GetError())
            pStData->ResetError();
        else
            bOk = GetBLIPDirect( *pStData, rGraphic, pVisArea );
        if( pStData2 && !bOk )
        {
            // Error, but the is a second chance: There is a second
            //         data stream in which the graphic could be stored!
            if( pStData2->GetError() )
                pStData2->ResetError();
            sal_uInt64 nOldPosData2 = pStData2->Tell();
            // jump to the BLIP atom in the second data stream
            bOk = checkSeek(*pStData2, nBlipFilePos);
            // reset error status if necessary
            if (!bOk || pStData2->GetError())
                pStData2->ResetError();
            else
                bOk = GetBLIPDirect( *pStData2, rGraphic, pVisArea );
            // restore of FilePos of the second data stream
            pStData2->Seek( nOldPosData2 );
        }
        // restore old FilePos of the stream(s)
        rStCtrl.Seek( nOldPosCtrl );
        if( &rStCtrl != pStData )
          pStData->Seek( nOldPosData );

        if (bOk)
        {
            // create new BlipCacheEntry for this graphic
            aEscherBlipCache.insert(std::make_pair(nIdx_, rGraphic));
        }
    }

    return bOk;
}

/*      access to a BLIP at runtime (with correctly positioned stream)
    ---------------------------------
******************************************************************************/
bool SvxMSDffManager::GetBLIPDirect( SvStream& rBLIPStream, Graphic& rData, tools::Rectangle* pVisArea )
{
    sal_uInt64 nOldPos = rBLIPStream.Tell();

    ErrCode nRes = ERRCODE_GRFILTER_OPENERROR;  // initialize error variable

    // check whether it's really a BLIP
    sal_uInt32 nLength;
    sal_uInt16 nInst, nFbt( 0 );
    sal_uInt8   nVer;
    if( ReadCommonRecordHeader( rBLIPStream, nVer, nInst, nFbt, nLength) && ( 0xF018 <= nFbt ) && ( 0xF117 >= nFbt ) )
    {
        Size        aMtfSize100;
        bool        bMtfBLIP = false;
        bool        bZCodecCompression = false;
        // now position it exactly at the beginning of the embedded graphic
        sal_uLong nSkip = (nInst & 0x0001) ? 32 : 16;
        const OfficeArtBlipRecInstance aRecInstanse = OfficeArtBlipRecInstance(nInst & 0xFFFE);
        switch (aRecInstanse)
        {
            case OfficeArtBlipRecInstance::EMF:
            case OfficeArtBlipRecInstance::WMF:
            case OfficeArtBlipRecInstance::PICT:
            {
                rBLIPStream.SeekRel(nSkip + 20);

                // read in size of metafile in English Metric Units (EMUs)
                sal_Int32 width(0), height(0);
                rBLIPStream.ReadInt32(width).ReadInt32(height);
                aMtfSize100.setWidth(width);
                aMtfSize100.setHeight(height);

                // 1 EMU = 1/360,000 of a centimeter
                // scale to 1/100mm
                aMtfSize100.setWidth(aMtfSize100.Width() / 360);
                aMtfSize100.setHeight(aMtfSize100.Height() / 360);

                if (pVisArea) // seem that we currently are skipping the visarea position
                    *pVisArea = tools::Rectangle(Point(), aMtfSize100);

                // skip rest of header
                nSkip = 6;
                bMtfBLIP = bZCodecCompression = true;
            }
            break;
            case OfficeArtBlipRecInstance::JPEG_RGB:
            case OfficeArtBlipRecInstance::JPEG_CMYK:
            case OfficeArtBlipRecInstance::PNG:
            case OfficeArtBlipRecInstance::DIB:
            case OfficeArtBlipRecInstance::TIFF:
                nSkip += 1; // Skip one byte tag
                break;
        }
        rBLIPStream.SeekRel( nSkip );

        SvStream* pGrStream = &rBLIPStream;
        std::unique_ptr<SvMemoryStream> xOut;
        if( bZCodecCompression )
        {
            xOut.reset(new SvMemoryStream( 0x8000, 0x4000 ));
            ZCodec aZCodec( 0x8000, 0x8000 );
            aZCodec.BeginCompression();
            aZCodec.Decompress( rBLIPStream, *xOut );
            aZCodec.EndCompression();
            xOut->Seek( STREAM_SEEK_TO_BEGIN );
            xOut->SetResizeOffset( 0 ); // sj: #i102257# setting ResizeOffset of 0 prevents from seeking
                                        // behind the stream end (allocating too much memory)
            pGrStream = xOut.get();
        }

#ifdef DEBUG_FILTER_MSDFFIMP
        // extract graphics from ole storage into "dbggfxNNN.*"
        static sal_Int32 nGrfCount;

        OUString aFileName = "dbggfx" + OUString::number(nGrfCount++);
        switch (aRecInstanse)
        {
            case OfficeArtBlipRecInstance::WMF:
                aFileName += ".wmf";
                break;
            case OfficeArtBlipRecInstance::EMF:
                aFileName += ".emf";
                break;
            case OfficeArtBlipRecInstance::PICT:
                aFileName += ".pct";
                break;
            case OfficeArtBlipRecInstance::JPEG_RGB:
            case OfficeArtBlipRecInstance::JPEG_CMYK:
                aFileName += ".jpg";
                break;
            case OfficeArtBlipRecInstance::PNG:
                aFileName += ".png";
                break;
            case OfficeArtBlipRecInstance::DIB:
                aFileName += ".bmp";
                break;
            case OfficeArtBlipRecInstance::TIFF:
                aFileName += ".tif";
                break;
        }


        OUString aURLStr;
        if( osl::FileBase::getFileURLFromSystemPath( Application::GetAppFileName(), aURLStr ) == osl::FileBase::E_None )
        {
            INetURLObject aURL( aURLStr );

            aURL.removeSegment();
            aURL.removeFinalSlash();
            aURL.Append( aFileName );

            aURLStr = aURL.GetMainURL( INetURLObject::DecodeMechanism::NONE );

            SAL_INFO("filter.ms", "dumping " << aURLStr);

            std::unique_ptr<SvStream> pDbgOut(::utl::UcbStreamHelper::CreateStream(aURLStr, StreamMode::TRUNC | StreamMode::WRITE));

            if( pDbgOut )
            {
                if ( bZCodecCompression )
                {
                    pDbgOut->WriteBytes(xOut->GetData(), xOut->TellEnd());
                    xOut->Seek(STREAM_SEEK_TO_BEGIN);
                }
                else
                {
                    sal_Int32 nDbgLen = nLength - nSkip;
                    if ( nDbgLen )
                    {
                        std::vector<char> aData(nDbgLen);
                        pGrStream->ReadBytes(aData.data(), nDbgLen);
                        pDbgOut->WriteBytes(aData.data(), nDbgLen);
                        pGrStream->SeekRel(-nDbgLen);
                    }
                }
            }
        }
#endif
        if (aRecInstanse == OfficeArtBlipRecInstance::DIB)
        {   // getting the DIBs immediately
            Bitmap aNew;
            if( ReadDIB(aNew, *pGrStream, false) )
            {
                rData = Graphic(BitmapEx(aNew));
                nRes = ERRCODE_NONE;
            }
        }
        else
        {   // and unleash our filter
            GraphicFilter& rGF = GraphicFilter::GetGraphicFilter();
            // ImportUnloadedGraphic() may simply read the entire rest of the stream,
            // which may be very large if the whole document is large. Limit the read
            // size to the size of this record.
            sal_uInt64 maxSize = pGrStream == &rBLIPStream ? nLength : 0;
            Graphic aGraphic;

            // Size available in metafile header.
            if (aMtfSize100.getWidth() && aMtfSize100.getHeight())
                aGraphic = rGF.ImportUnloadedGraphic(*pGrStream, maxSize, &aMtfSize100);
            else
                aGraphic = rGF.ImportUnloadedGraphic(*pGrStream, maxSize);

            if (!aGraphic.IsNone())
            {
                rData = std::move(aGraphic);
                nRes = ERRCODE_NONE;
            }
            else
                nRes = rGF.ImportGraphic( rData, u"", *pGrStream );

            // SJ: I40472, sometimes the aspect ratio (aMtfSize100) does not match and we get scaling problems,
            // then it is better to use the prefsize that is stored within the metafile. Bug #72846# for what the
            // scaling has been implemented does not happen anymore.
            //
            // For pict graphics we will furthermore scale the metafile, because font scaling leads to error if the
            // dxarray is empty (this has been solved in wmf/emf but not for pict)
            if (bMtfBLIP && (ERRCODE_NONE == nRes) && (rData.GetType() == GraphicType::GdiMetafile)
                && (aRecInstanse == OfficeArtBlipRecInstance::PICT))
            {
                if ( ( aMtfSize100.Width() >= 1000 ) && ( aMtfSize100.Height() >= 1000 ) )
                {   // #75956#, scaling does not work properly, if the graphic is less than 1cm
                    GDIMetaFile aMtf( rData.GetGDIMetaFile() );
                    const Size  aOldSize( aMtf.GetPrefSize() );

                    if( aOldSize.Width() && ( aOldSize.Width() != aMtfSize100.Width() ) &&
                        aOldSize.Height() && ( aOldSize.Height() != aMtfSize100.Height() ) )
                    {
                        aMtf.Scale( static_cast<double>(aMtfSize100.Width()) / aOldSize.Width(),
                                    static_cast<double>(aMtfSize100.Height()) / aOldSize.Height() );
                        aMtf.SetPrefSize( aMtfSize100 );
                        aMtf.SetPrefMapMode(MapMode(MapUnit::Map100thMM));
                        rData = aMtf;
                    }
                }
            }
        }
        // reset error status if necessary
        if ( ERRCODE_IO_PENDING == pGrStream->GetError() )
          pGrStream->ResetError();
    }
    rBLIPStream.Seek( nOldPos );    // restore old FilePos of the stream

    return ( ERRCODE_NONE == nRes ); // return result
}

/* also static */
bool SvxMSDffManager::ReadCommonRecordHeader(SvStream& rSt,
    sal_uInt8& rVer, sal_uInt16& rInst, sal_uInt16& rFbt, sal_uInt32& rLength)
{
    sal_uInt16 nTmp(0);
    rSt.ReadUInt16( nTmp ).ReadUInt16( rFbt ).ReadUInt32( rLength );
    rVer = sal::static_int_cast< sal_uInt8 >(nTmp & 15);
    rInst = nTmp >> 4;
    if (!rSt.good())
        return false;
    if (rLength > nMaxLegalDffRecordLength)
        return false;
    return true;
}

void SvxMSDffManager::ProcessClientAnchor(SvStream& rStData, sal_uInt32 nDatLen,
                                          std::unique_ptr<char[]>& rpBuff, sal_uInt32& rBuffLen )
{
    if( nDatLen )
    {
        rBuffLen = std::min(rStData.remainingSize(), static_cast<sal_uInt64>(nDatLen));
        rpBuff.reset( new char[rBuffLen] );
        rBuffLen = rStData.ReadBytes(rpBuff.get(), rBuffLen);
    }
}

void SvxMSDffManager::ProcessClientData(SvStream& rStData, sal_uInt32 nDatLen,
                                        std::unique_ptr<char[]>& rpBuff, sal_uInt32& rBuffLen )
{
    if( nDatLen )
    {
        rBuffLen = std::min(rStData.remainingSize(), static_cast<sal_uInt64>(nDatLen));
        rpBuff.reset( new char[rBuffLen] );
        rBuffLen = rStData.ReadBytes(rpBuff.get(), rBuffLen);
    }
}


void SvxMSDffManager::ProcessClientAnchor2( SvStream& /* rSt */, DffRecordHeader& /* rHd */ , DffObjData& /* rObj */ )
{
    // will be overridden by SJ in Draw
}

bool SvxMSDffManager::GetOLEStorageName( sal_uInt32, OUString&, rtl::Reference<SotStorage>&, uno::Reference < embed::XStorage >& ) const
{
    return false;
}

bool SvxMSDffManager::ShapeHasText( sal_uLong /* nShapeId */, sal_uLong /* nFilePos */ ) const
{
    return true;
}

// #i32596# - add new parameter <_nCalledByGroup>
rtl::Reference<SdrObject> SvxMSDffManager::ImportOLE( sal_uInt32 nOLEId,
                                       const Graphic& rGrf,
                                       const tools::Rectangle& rBoundRect,
                                       const tools::Rectangle& rVisArea,
                                       const int /* _nCalledByGroup */ ) const
{
    rtl::Reference<SdrObject> pRet;
    OUString sStorageName;
    rtl::Reference<SotStorage> xSrcStg;
    ErrCode nError = ERRCODE_NONE;
    uno::Reference < embed::XStorage > xDstStg;
    if( GetOLEStorageName( nOLEId, sStorageName, xSrcStg, xDstStg ))
        pRet = CreateSdrOLEFromStorage(
            *GetModel(),
            sStorageName,
            xSrcStg,
            xDstStg,
            rGrf,
            rBoundRect,
            rVisArea,
            pStData,
            nError,
            nSvxMSDffOLEConvFlags,
            embed::Aspects::MSOLE_CONTENT,
            maBaseURL);
    return pRet;
}

bool SvxMSDffManager::MakeContentStream( SotStorage * pStor, const GDIMetaFile & rMtf )
{
    rtl::Reference<SotStorageStream> xStm = pStor->OpenSotStream(SVEXT_PERSIST_STREAM);
    xStm->SetVersion( pStor->GetVersion() );
    xStm->SetBufferSize( 8192 );

    Impl_OlePres aEle;
    // Convert the size in 1/100 mm
    // If a not applicable MapUnit (device dependent) is used,
    // SV tries to guess a best match for the right value
    Size aSize = rMtf.GetPrefSize();
    const MapMode& aMMSrc = rMtf.GetPrefMapMode();
    MapMode aMMDst( MapUnit::Map100thMM );
    aSize = OutputDevice::LogicToLogic( aSize, aMMSrc, aMMDst );
    aEle.SetSize( aSize );
    aEle.SetAspect( ASPECT_CONTENT );
    aEle.SetAdviseFlags( 2 );
    aEle.SetMtf( rMtf );
    aEle.Write( *xStm );

    xStm->SetBufferSize( 0 );
    return xStm->GetError() == ERRCODE_NONE;
}

namespace {

struct ClsIDs {
    sal_uInt32  nId;
    const char* pSvrName;
    const char* pDspName;
};

}

const ClsIDs aClsIDs[] = {

    { 0x000212F0, "MSWordArt",          "Microsoft Word Art"            },
    { 0x000212F0, "MSWordArt.2",        "Microsoft Word Art 2.0"        },

    // MS Apps
    { 0x00030000, "ExcelWorksheet",     "Microsoft Excel Worksheet"     },
    { 0x00030001, "ExcelChart",         "Microsoft Excel Chart"         },
    { 0x00030002, "ExcelMacrosheet",    "Microsoft Excel Macro"         },
    { 0x00030003, "WordDocument",       "Microsoft Word Document"       },
    { 0x00030004, "MSPowerPoint",       "Microsoft PowerPoint"          },
    { 0x00030005, "MSPowerPointSho",    "Microsoft PowerPoint Slide Show"},
    { 0x00030006, "MSGraph",            "Microsoft Graph"               },
    { 0x00030007, "MSDraw",             "Microsoft Draw"                },
    { 0x00030008, "Note-It",            "Microsoft Note-It"             },
    { 0x00030009, "WordArt",            "Microsoft Word Art"            },
    { 0x0003000a, "PBrush",             "Microsoft PaintBrush Picture"  },
    { 0x0003000b, "Equation",           "Microsoft Equation Editor"     },
    { 0x0003000c, "Package",            "Package"                       },
    { 0x0003000d, "SoundRec",           "Sound"                         },
    { 0x0003000e, "MPlayer",            "Media Player"                  },
    // MS Demos
    { 0x0003000f, "ServerDemo",         "OLE 1.0 Server Demo"           },
    { 0x00030010, "Srtest",             "OLE 1.0 Test Demo"             },
    { 0x00030011, "SrtInv",             "OLE 1.0 Inv Demo"              },
    { 0x00030012, "OleDemo",            "OLE 1.0 Demo"                  },

    // Coromandel / Dorai Swamy / 718-793-7963
    { 0x00030013, "CoromandelIntegra",  "Coromandel Integra"            },
    { 0x00030014, "CoromandelObjServer","Coromandel Object Server"      },

    // 3-d Visions Corp / Peter Hirsch / 310-325-1339
    { 0x00030015, "StanfordGraphics",   "Stanford Graphics"             },

    // Deltapoint / Nigel Hearne / 408-648-4000
    { 0x00030016, "DGraphCHART",        "DeltaPoint Graph Chart"        },
    { 0x00030017, "DGraphDATA",         "DeltaPoint Graph Data"         },

    // Corel / Richard V. Woodend / 613-728-8200 x1153
    { 0x00030018, "PhotoPaint",         "Corel PhotoPaint"              },
    { 0x00030019, "CShow",              "Corel Show"                    },
    { 0x0003001a, "CorelChart",         "Corel Chart"                   },
    { 0x0003001b, "CDraw",              "Corel Draw"                    },

    // Inset Systems / Mark Skiba / 203-740-2400
    { 0x0003001c, "HJWIN1.0",           "Inset Systems"                 },

    // Mark V Systems / Mark McGraw / 818-995-7671
    { 0x0003001d, "ObjMakerOLE",        "MarkV Systems Object Maker"    },

    // IdentiTech / Mike Gilger / 407-951-9503
    { 0x0003001e, "FYI",                "IdentiTech FYI"                },
    { 0x0003001f, "FYIView",            "IdentiTech FYI Viewer"         },

    // Inventa Corporation / Balaji Varadarajan / 408-987-0220
    { 0x00030020, "Stickynote",         "Inventa Sticky Note"           },

    // ShapeWare Corp. / Lori Pearce / 206-467-6723
    { 0x00030021, "ShapewareVISIO10",   "Shapeware Visio 1.0"           },
    { 0x00030022, "ImportServer",       "Spaheware Import Server"       },

    // test app SrTest
    { 0x00030023, "SrvrTest",           "OLE 1.0 Server Test"           },

    // test app ClTest.  Doesn't really work as a server but is in reg db
    { 0x00030025, "Cltest",             "OLE 1.0 Client Test"           },

    // Microsoft ClipArt Gallery   Sherry Larsen-Holmes
    { 0x00030026, "MS_ClipArt_Gallery", "Microsoft ClipArt Gallery"     },
    // Microsoft Project  Cory Reina
    { 0x00030027, "MSProject",          "Microsoft Project"             },

    // Microsoft Works Chart
    { 0x00030028, "MSWorksChart",       "Microsoft Works Chart"         },

    // Microsoft Works Spreadsheet
    { 0x00030029, "MSWorksSpreadsheet", "Microsoft Works Spreadsheet"   },

    // AFX apps - Dean McCrory
    { 0x0003002A, "MinSvr",             "AFX Mini Server"               },
    { 0x0003002B, "HierarchyList",      "AFX Hierarchy List"            },
    { 0x0003002C, "BibRef",             "AFX BibRef"                    },
    { 0x0003002D, "MinSvrMI",           "AFX Mini Server MI"            },
    { 0x0003002E, "TestServ",           "AFX Test Server"               },

    // Ami Pro
    { 0x0003002F, "AmiProDocument",     "Ami Pro Document"              },

    // WordPerfect Presentations For Windows
    { 0x00030030, "WPGraphics",         "WordPerfect Presentation"      },
    { 0x00030031, "WPCharts",           "WordPerfect Chart"             },

    // MicroGrafx Charisma
    { 0x00030032, "Charisma",           "MicroGrafx Charisma"           },
    { 0x00030033, "Charisma_30",        "MicroGrafx Charisma 3.0"       },
    { 0x00030034, "CharPres_30",        "MicroGrafx Charisma 3.0 Pres"  },
    // MicroGrafx Draw
    { 0x00030035, "Draw",               "MicroGrafx Draw"               },
    // MicroGrafx Designer
    { 0x00030036, "Designer_40",        "MicroGrafx Designer 4.0"       },

    // STAR DIVISION
    { 0x00043AD2, "FontWork",           "Star FontWork"                 },

    { 0, "", "" } };


bool SvxMSDffManager::ConvertToOle2( SvStream& rStm, sal_uInt32 nReadLen,
                    const GDIMetaFile * pMtf, const rtl::Reference<SotStorage>& rDest )
{
    bool bMtfRead = false;
    rtl::Reference<SotStorageStream> xOle10Stm = rDest->OpenSotStream( u"\1Ole10Native"_ustr,
                                                    StreamMode::WRITE| StreamMode::SHARE_DENYALL );
    if( xOle10Stm->GetError() )
        return false;

    OUString   aSvrName;
    sal_uInt32 nDummy0;
    sal_uInt32 nDummy1;
    sal_uInt32 nBytesRead = 0;
    do
    {
        sal_uInt32 nType(0);
        sal_uInt32 nRecType(0);
        sal_uInt32 nStrLen(0);

        rStm.ReadUInt32( nType );
        rStm.ReadUInt32( nRecType );
        rStm.ReadUInt32( nStrLen );
        if( nStrLen )
        {
            if( 0x10000L > nStrLen )
            {
                std::unique_ptr<char[]> pBuf(new char[ nStrLen ]);
                rStm.ReadBytes(pBuf.get(), nStrLen);
                aSvrName = OUString( pBuf.get(), static_cast<sal_uInt16>(nStrLen)-1, osl_getThreadTextEncoding() );
            }
            else
                break;
        }
        rStm.ReadUInt32( nDummy0 );
        rStm.ReadUInt32( nDummy1 );
        sal_uInt32 nDataLen(0);
        rStm.ReadUInt32( nDataLen );

        nBytesRead += 6 * sizeof( sal_uInt32 ) + nStrLen + nDataLen;

        if (rStm.good() && nReadLen > nBytesRead && nDataLen)
        {
            if( xOle10Stm.is() )
            {
                std::unique_ptr<sal_uInt8[]> pData(new sal_uInt8[ nDataLen ]);
                rStm.ReadBytes(pData.get(), nDataLen);

                // write to ole10 stream
                xOle10Stm->WriteUInt32( nDataLen );
                xOle10Stm->WriteBytes(pData.get(), nDataLen);
                xOle10Stm.clear();

                // set the compobj stream
                const ClsIDs* pIds;
                for( pIds = aClsIDs; pIds->nId; pIds++ )
                {
                    if( aSvrName == OUString::createFromAscii(pIds->pSvrName) )
                        break;
                }

                if( pIds->nId )
                {
                    // found!
                    SotClipboardFormatId nCbFmt = SotExchange::RegisterFormatName( aSvrName );
                    rDest->SetClass( SvGlobalName( pIds->nId, 0, 0, 0xc0,0,0,0,0,0,0,0x46 ), nCbFmt,
                                    OUString::createFromAscii( pIds->pDspName ) );
                }
                else
                {
                    SotClipboardFormatId nCbFmt = SotExchange::RegisterFormatName( aSvrName );
                    rDest->SetClass( SvGlobalName(), nCbFmt, aSvrName );
                }
            }
            else if( nRecType == 5 && !pMtf )
            {
                sal_uInt64 nPos = rStm.Tell();
                sal_uInt16 sz[4];
                rStm.ReadBytes( sz, 8 );
                Graphic aGraphic;
                if( ERRCODE_NONE == GraphicConverter::Import( rStm, aGraphic ) && aGraphic.GetType() != GraphicType::NONE )
                {
                    const GDIMetaFile& rMtf = aGraphic.GetGDIMetaFile();
                    MakeContentStream( rDest.get(), rMtf );
                    bMtfRead = true;
                }
                // set behind the data
                rStm.Seek( nPos + nDataLen );
            }
            else
                rStm.SeekRel( nDataLen );
        }
    } while (rStm.good() && nReadLen >= nBytesRead);

    if( !bMtfRead && pMtf )
    {
        MakeContentStream( rDest.get(), *pMtf );
        return true;
    }

    return false;
}

static const char* GetInternalServerName_Impl( const SvGlobalName& aGlobName )
{
    if ( aGlobName == SvGlobalName( SO3_SW_OLE_EMBED_CLASSID_60 )
      || aGlobName == SvGlobalName( SO3_SW_OLE_EMBED_CLASSID_8 ) )
        return "swriter";
    else if ( aGlobName == SvGlobalName( SO3_SC_OLE_EMBED_CLASSID_60 )
      || aGlobName == SvGlobalName( SO3_SC_OLE_EMBED_CLASSID_8 ) )
        return "scalc";
    else if ( aGlobName == SvGlobalName( SO3_SIMPRESS_OLE_EMBED_CLASSID_60 )
      || aGlobName == SvGlobalName( SO3_SIMPRESS_OLE_EMBED_CLASSID_8 ) )
        return "simpress";
    else if ( aGlobName == SvGlobalName( SO3_SDRAW_OLE_EMBED_CLASSID_60 )
      || aGlobName == SvGlobalName( SO3_SDRAW_OLE_EMBED_CLASSID_8 ) )
        return "sdraw";
    else if ( aGlobName == SvGlobalName( SO3_SM_OLE_EMBED_CLASSID_60 )
      || aGlobName == SvGlobalName( SO3_SM_OLE_EMBED_CLASSID_8 ) )
        return "smath";
    else if ( aGlobName == SvGlobalName( SO3_SCH_OLE_EMBED_CLASSID_60 )
      || aGlobName == SvGlobalName( SO3_SCH_OLE_EMBED_CLASSID_8 ) )
        return "schart";
    return nullptr;
}

OUString SvxMSDffManager::GetFilterNameFromClassID( const SvGlobalName& aGlobName )
{
    if ( aGlobName == SvGlobalName( SO3_SW_OLE_EMBED_CLASSID_60 ) )
        return u"StarOffice XML (Writer)"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SW_OLE_EMBED_CLASSID_8 ) )
        return u"writer8"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SC_OLE_EMBED_CLASSID_60 ) )
        return u"StarOffice XML (Calc)"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SC_OLE_EMBED_CLASSID_8 ) )
        return u"calc8"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SIMPRESS_OLE_EMBED_CLASSID_60 ) )
        return u"StarOffice XML (Impress)"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SIMPRESS_OLE_EMBED_CLASSID_8 ) )
        return u"impress8"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SDRAW_OLE_EMBED_CLASSID_60 ) )
        return u"StarOffice XML (Draw)"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SDRAW_OLE_EMBED_CLASSID_8 ) )
        return u"draw8"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SM_OLE_EMBED_CLASSID_60 ) )
        return u"StarOffice XML (Math)"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SM_OLE_EMBED_CLASSID_8 ) )
        return u"math8"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SCH_OLE_EMBED_CLASSID_60 ) )
        return u"StarOffice XML (Chart)"_ustr;

    if ( aGlobName == SvGlobalName( SO3_SCH_OLE_EMBED_CLASSID_8 ) )
        return u"chart8"_ustr;

    return OUString();
}

void SvxMSDffManager::ExtractOwnStream(SotStorage& rSrcStg, SvMemoryStream& rMemStream)
{
    rtl::Reference<SotStorageStream> xStr
        = rSrcStg.OpenSotStream(u"package_stream"_ustr, StreamMode::STD_READ);
    xStr->ReadStream(rMemStream);
}

css::uno::Reference < css::embed::XEmbeddedObject >  SvxMSDffManager::CheckForConvertToSOObj( sal_uInt32 nConvertFlags,
                        SotStorage& rSrcStg, const uno::Reference < embed::XStorage >& rDestStorage,
                        const Graphic& rGrf,
                        const tools::Rectangle& rVisArea, OUString const& rBaseURL)
{
    uno::Reference < embed::XEmbeddedObject > xObj;
    SvGlobalName aStgNm = rSrcStg.GetClassName();
    const char* pName = GetInternalServerName_Impl( aStgNm );
    OUString sStarName;
    if ( pName )
        sStarName = OUString::createFromAscii( pName );
    else if ( nConvertFlags )
    {
        static constexpr struct ObjImpType
        {
            sal_uInt32 nFlag;
            OUString aFactoryNm;
            // GlobalNameId
            sal_uInt32 n1;
            sal_uInt16 n2, n3;
            sal_uInt8 b8, b9, b10, b11, b12, b13, b14, b15;
        } aArr[] {
            { OLE_MATHTYPE_2_STARMATH, u"smath"_ustr, MSO_EQUATION3_CLASSID },
            { OLE_MATHTYPE_2_STARMATH, u"smath"_ustr, MSO_EQUATION2_CLASSID },
            { OLE_WINWORD_2_STARWRITER, u"swriter"_ustr, MSO_WW8_CLASSID },
            // Excel table
            { OLE_EXCEL_2_STARCALC, u"scalc"_ustr, MSO_EXCEL5_CLASSID },
            { OLE_EXCEL_2_STARCALC, u"scalc"_ustr, MSO_EXCEL8_CLASSID },
            // 114465: additional Excel OLE chart classId to above.
            { OLE_EXCEL_2_STARCALC, u"scalc"_ustr, MSO_EXCEL8_CHART_CLASSID },
            // PowerPoint presentation
            { OLE_POWERPOINT_2_STARIMPRESS, u"simpress"_ustr, MSO_PPT8_CLASSID },
            // PowerPoint slide
            { OLE_POWERPOINT_2_STARIMPRESS, u"simpress"_ustr, MSO_PPT8_SLIDE_CLASSID }
        };

        for( const ObjImpType & rArr : aArr )
        {
            if( nConvertFlags & rArr.nFlag )
            {
                SvGlobalName aTypeName( rArr.n1, rArr.n2, rArr.n3,
                                rArr.b8, rArr.b9, rArr.b10, rArr.b11,
                                rArr.b12, rArr.b13, rArr.b14, rArr.b15 );

                if ( aStgNm == aTypeName )
                {
                    sStarName = rArr.aFactoryNm;
                    break;
                }
            }
        }
    }

    if ( sStarName.getLength() )
    {
        //TODO/MBA: check if (and when) storage and stream will be destroyed!
        std::shared_ptr<const SfxFilter> pFilter;
        SvMemoryStream aMemStream;
        if ( pName )
        {
            // TODO/LATER: perhaps we need to retrieve VisArea and Metafile from the storage also
            SvxMSDffManager::ExtractOwnStream(rSrcStg, aMemStream);
        }
        else
        {
            rtl::Reference<SotStorage> xStorage = new SotStorage(false, aMemStream);
            rSrcStg.CopyTo( xStorage.get() );
            xStorage->Commit();
            xStorage.clear();
            OUString aType = SfxFilter::GetTypeFromStorage( rSrcStg );
            if (aType.getLength() && !comphelper::IsFuzzing())
            {
                SfxFilterMatcher aMatch( sStarName );
                pFilter = aMatch.GetFilter4EA( aType );
            }
        }

#ifdef DEBUG_FILTER_MSFILTER
        // extract embedded ole streams into "/tmp/embedded_stream_NNN"
        static sal_Int32 nOleCount(0);
        OUString aTmpName("/tmp/embedded_stream_");
        aTmpName += OUString::number(nOleCount++);
        aTmpName += ".bin";
        SvFileStream aTmpStream(aTmpName,StreamMode::READ|StreamMode::WRITE|StreamMode::TRUNC);
        xMemStream->Seek(0);
        aTmpStream.WriteStream(*xMemStream);
        aTmpStream.Close();
#endif
        if ( pName || pFilter )
        {
            //Reuse current ole name
            OUString aDstStgName = MSO_OLE_Obj + OUString::number(nMSOleObjCntr);

            OUString aFilterName;
            if ( pFilter )
                aFilterName = pFilter->GetName();
            else
                aFilterName = SvxMSDffManager::GetFilterNameFromClassID( aStgNm );

            uno::Sequence<beans::PropertyValue> aMedium(aFilterName.isEmpty() ? 3 : 4);
            auto pMedium = aMedium.getArray();
            pMedium[0].Name = "InputStream";
            uno::Reference < io::XInputStream > xStream = new ::utl::OSeekableInputStreamWrapper( aMemStream );
            pMedium[0].Value <<= xStream;
            pMedium[1].Name = "URL";
            pMedium[1].Value <<= u"private:stream"_ustr;
            pMedium[2].Name = "DocumentBaseURL";
            pMedium[2].Value <<= rBaseURL;

            if ( !aFilterName.isEmpty() )
            {
                pMedium[3].Name = "FilterName";
                pMedium[3].Value <<= aFilterName;
            }

            OUString aName( aDstStgName );
            comphelper::EmbeddedObjectContainer aCnt( rDestStorage );
            xObj = aCnt.InsertEmbeddedObject(aMedium, aName, &rBaseURL);

            if ( !xObj.is() )
            {
                if( !aFilterName.isEmpty() )
                {
                    // throw the filter parameter away as workaround
                    aMedium.realloc( 2 );
                    xObj = aCnt.InsertEmbeddedObject(aMedium, aName, &rBaseURL);
                }

                if ( !xObj.is() )
                     return xObj;
            }

            // JP 26.10.2001: Bug 93374 / 91928 the writer
            // objects need the correct visarea needs the
            // correct visarea, but this is not true for
            // PowerPoint (see bugdoc 94908b)
            // SJ: 19.11.2001 bug 94908, also chart objects
            // needs the correct visarea

            // If pName is set this is an own embedded object, it should have the correct size internally
            // TODO/LATER: it might make sense in future to set the size stored in internal object
            if( !pName && ( sStarName == "swriter" || sStarName == "scalc" ) )
            {
                // TODO/LATER: ViewAspect must be passed from outside!
                sal_Int64 nViewAspect = embed::Aspects::MSOLE_CONTENT;
                MapMode aMapMode( VCLUnoHelper::UnoEmbed2VCLMapUnit( xObj->getMapUnit( nViewAspect ) ) );
                Size aSz;
                if ( rVisArea.IsEmpty() )
                    aSz = lcl_GetPrefSize(rGrf, aMapMode );
                else
                {
                    aSz = rVisArea.GetSize();
                    aSz = OutputDevice::LogicToLogic( aSz, MapMode( MapUnit::Map100thMM ), aMapMode );
                }

                // don't modify the object
                //TODO/LATER: remove those hacks, that needs to be done differently!
                //xIPObj->EnableSetModified( sal_False );
                awt::Size aSize;
                aSize.Width = aSz.Width();
                aSize.Height = aSz.Height();
                xObj->setVisualAreaSize( nViewAspect, aSize );
                //xIPObj->EnableSetModified( sal_True );
            }
            else if ( sStarName == "smath" )
            {   // SJ: force the object to recalc its visarea
                //TODO/LATER: wait for PrinterChangeNotification
                //xIPObj->OnDocumentPrinterChanged( NULL );
            }
        }
    }

    return xObj;
}

// TODO/MBA: code review and testing!
rtl::Reference<SdrOle2Obj> SvxMSDffManager::CreateSdrOLEFromStorage(
    SdrModel& rSdrModel,
    const OUString& rStorageName,
    rtl::Reference<SotStorage> const & rSrcStorage,
    const uno::Reference < embed::XStorage >& xDestStorage,
    const Graphic& rGrf,
    const tools::Rectangle& rBoundRect,
    const tools::Rectangle& rVisArea,
    SvStream* pDataStrm,
    ErrCode& rError,
    sal_uInt32 nConvertFlags,
    sal_Int64 nRecommendedAspect,
    OUString const& rBaseURL)
{
    sal_Int64 nAspect = nRecommendedAspect;
    rtl::Reference<SdrOle2Obj> pRet;
    if( rSrcStorage.is() && xDestStorage.is() && rStorageName.getLength() )
    {
        comphelper::EmbeddedObjectContainer aCnt( xDestStorage );
        // does the 01Ole-Stream exist at all?
        // (that's not the case for e.g. Fontwork )
        // If that's not the case -> include it as graphic
        bool bValidStorage = false;
        OUString aDstStgName = MSO_OLE_Obj + OUString::number( ++nMSOleObjCntr );

        {
            rtl::Reference<SotStorage> xObjStg = rSrcStorage->OpenSotStorage(rStorageName);
            if( xObjStg.is()  )
            {
                {
                    sal_uInt8 aTestA[10];   // exist the \1CompObj-Stream ?
                    rtl::Reference<SotStorageStream> xSrcTst = xObjStg->OpenSotStream(u"\1CompObj"_ustr);
                    bValidStorage = xSrcTst.is() && sizeof( aTestA ) ==
                                    xSrcTst->ReadBytes(aTestA, sizeof(aTestA));
                    if( !bValidStorage )
                    {
                        // or the \1Ole-Stream ?
                        xSrcTst = xObjStg->OpenSotStream( u"\1Ole"_ustr );
                        bValidStorage = xSrcTst.is() && sizeof(aTestA) ==
                                    xSrcTst->ReadBytes(aTestA, sizeof(aTestA));
                    }
                }

                if( bValidStorage )
                {
                    if ( nAspect != embed::Aspects::MSOLE_ICON )
                    {
                        // check whether the object is iconified one
                        // usually this information is already known, the only exception
                        // is a kind of embedded objects in Word documents
                        // TODO/LATER: should the caller be notified if the aspect changes in future?

                        rtl::Reference<SotStorageStream> xObjInfoSrc = xObjStg->OpenSotStream(
                            u"\3ObjInfo"_ustr, StreamMode::STD_READ );
                        if ( xObjInfoSrc.is() && !xObjInfoSrc->GetError() )
                        {
                            sal_uInt8 nByte = 0;
                            xObjInfoSrc->ReadUChar( nByte );
                            if ( ( nByte >> 4 ) & embed::Aspects::MSOLE_ICON )
                                nAspect = embed::Aspects::MSOLE_ICON;
                        }
                    }

                    uno::Reference < embed::XEmbeddedObject > xObj( CheckForConvertToSOObj(
                            nConvertFlags, *xObjStg, xDestStorage, rGrf,
                            rVisArea, rBaseURL));
                    if ( xObj.is() )
                    {
                        // remember file name to use in the title bar
                        INetURLObject aURL(rBaseURL);
                        xObj->setContainerName(aURL.GetLastName(INetURLObject::DecodeMechanism::WithCharset));

                        svt::EmbeddedObjectRef aObj( xObj, nAspect );

                        // TODO/LATER: need MediaType
                        aObj.SetGraphic( rGrf, OUString() );

                        // TODO/MBA: check setting of PersistName
                        pRet = new SdrOle2Obj(
                            rSdrModel,
                            aObj,
                            OUString(),
                            rBoundRect);

                        // we have the Object, don't create another
                        bValidStorage = false;
                    }
                }
            }
        }

        if( bValidStorage )
        {
            // object is not an own object
            rtl::Reference<SotStorage> xObjStor = SotStorage::OpenOLEStorage( xDestStorage, aDstStgName, StreamMode::READWRITE );

            if ( xObjStor.is() )
            {
                rtl::Reference<SotStorage> xSrcStor = rSrcStorage->OpenSotStorage( rStorageName, StreamMode::READ );
                xSrcStor->CopyTo( xObjStor.get() );

                if( !xObjStor->GetError() )
                    xObjStor->Commit();

                if( xObjStor->GetError() )
                {
                    rError = xObjStor->GetError();
                    bValidStorage = false;
                }
                else if( !xObjStor.is() )
                    bValidStorage = false;
            }
        }
        else if( pDataStrm )
        {
            sal_uInt32 nLen(0), nDummy(0);
            pDataStrm->ReadUInt32( nLen ).ReadUInt32( nDummy );
            if( ERRCODE_NONE != pDataStrm->GetError() ||
                // Id in BugDoc - exist there other Ids?
                // The ConvertToOle2 - does not check for consistent
                0x30008 != nDummy )
                bValidStorage = false;
            else
            {
                // or is it an OLE-1 Stream in the DataStream?
                rtl::Reference<SotStorage> xObjStor = SotStorage::OpenOLEStorage( xDestStorage, aDstStgName );
                //TODO/MBA: remove metafile conversion from ConvertToOle2
                //when is this code used?!
                GDIMetaFile aMtf;
                bValidStorage = ConvertToOle2( *pDataStrm, nLen, &aMtf, xObjStor );
                xObjStor->Commit();
            }
        }

        if( bValidStorage )
        {
            uno::Reference < embed::XEmbeddedObject > xObj = aCnt.GetEmbeddedObject( aDstStgName );
            if( xObj.is() )
            {
                // remember file name to use in the title bar
                INetURLObject aURL( rBaseURL );
                xObj->setContainerName( aURL.GetLastName( INetURLObject::DecodeMechanism::WithCharset ) );

                // the visual area must be retrieved from the metafile (object doesn't know it so far)

                if ( nAspect != embed::Aspects::MSOLE_ICON )
                {
                    // working with visual area can switch the object to running state
                    try
                    {
                        awt::Size aAwtSz;
                        // the provided visual area should be used, if there is any
                        if ( rVisArea.IsEmpty() )
                        {
                            MapUnit aMapUnit = VCLUnoHelper::UnoEmbed2VCLMapUnit( xObj->getMapUnit( nAspect ) );
                            Size aSz(lcl_GetPrefSize(rGrf, MapMode(aMapUnit)));
                            aAwtSz.Width = aSz.Width();
                            aAwtSz.Height = aSz.Height();
                        }
                        else
                        {
                            aAwtSz.Width = rVisArea.GetWidth();
                            aAwtSz.Height = rVisArea.GetHeight();
                        }
                        //xInplaceObj->EnableSetModified( sal_False );
                        xObj->setVisualAreaSize( nAspect, aAwtSz );
                        //xInplaceObj->EnableSetModified( sal_True );
                    }
                    catch( const uno::Exception& )
                    {
                        OSL_FAIL( "Could not set visual area of the object!" );
                    }
                }

                svt::EmbeddedObjectRef aObj( xObj, nAspect );

                // TODO/LATER: need MediaType
                aObj.SetGraphic( rGrf, OUString() );

                pRet = new SdrOle2Obj(
                    rSdrModel,
                    aObj,
                    aDstStgName,
                    rBoundRect);
            }
        }
    }

    return pRet;
}

bool SvxMSDffManager::SetPropValue( const uno::Any& rAny, const uno::Reference< css::beans::XPropertySet > & rXPropSet,
            const OUString& rPropName )
{
    bool bRetValue = false;
    try
    {
        uno::Reference< beans::XPropertySetInfo >
            aXPropSetInfo( rXPropSet->getPropertySetInfo() );
        if ( aXPropSetInfo.is() )
            bRetValue = aXPropSetInfo->hasPropertyByName( rPropName );
    }
    catch( const uno::Exception& )
    {
        bRetValue = false;
    }
    if ( bRetValue )
    {
        try
        {
            rXPropSet->setPropertyValue( rPropName, rAny );
            bRetValue = true;
        }
        catch( const uno::Exception& )
        {
            bRetValue = false;
        }
    }
    return bRetValue;
}

SvxMSDffImportRec::SvxMSDffImportRec()
    : nClientAnchorLen(  0 ),
      nClientDataLen(    0 ),
      nXAlign( 0 ), // position n cm from left
      nYAlign( 0 ), // position n cm below
      nGroupShapeBooleanProperties(0), // 16 settings: LayoutInCell/AllowOverlap/BehindDocument...
      nFlags( ShapeFlag::NONE ),
      nDxTextLeft( 144 ),
      nDyTextTop( 72 ),
      nDxTextRight( 144 ),
      nDyTextBottom( 72 ),
      nDxWrapDistLeft( 0 ),
      nDyWrapDistTop( 0 ),
      nDxWrapDistRight( 0 ),
      nDyWrapDistBottom(0 ),
      nCropFromTop( 0 ),
      nCropFromBottom( 0 ),
      nCropFromLeft( 0 ),
      nCropFromRight( 0 ),
      nNextShapeId( 0 ),
      nShapeId( 0 ),
      eShapeType( mso_sptNil ),
      relativeHorizontalWidth( -1 ),
      isHorizontalRule( false )
{
      eLineStyle      = mso_lineSimple; // GPF-Bug #66227#
      eLineDashing    = mso_lineSolid;
      bDrawHell       = false;
      bHidden         = false;

      bReplaceByFly   = false;
      bVFlip          = false;
      bHFlip          = false;
      bAutoWidth      = false;
}

SvxMSDffImportRec::SvxMSDffImportRec(const SvxMSDffImportRec& rCopy)
    : pObj( rCopy.pObj ),
      nXAlign( rCopy.nXAlign ),
      nXRelTo( rCopy.nXRelTo ),
      nYAlign( rCopy.nYAlign ),
      nYRelTo( rCopy.nYRelTo ),
      nGroupShapeBooleanProperties(rCopy.nGroupShapeBooleanProperties),
      nFlags( rCopy.nFlags ),
      nDxTextLeft( rCopy.nDxTextLeft    ),
      nDyTextTop( rCopy.nDyTextTop ),
      nDxTextRight( rCopy.nDxTextRight ),
      nDyTextBottom( rCopy.nDyTextBottom ),
      nDxWrapDistLeft( rCopy.nDxWrapDistLeft ),
      nDyWrapDistTop( rCopy.nDyWrapDistTop ),
      nDxWrapDistRight( rCopy.nDxWrapDistRight ),
      nDyWrapDistBottom(rCopy.nDyWrapDistBottom ),
      nCropFromTop( rCopy.nCropFromTop ),
      nCropFromBottom( rCopy.nCropFromBottom ),
      nCropFromLeft( rCopy.nCropFromLeft ),
      nCropFromRight( rCopy.nCropFromRight ),
      aTextId( rCopy.aTextId ),
      nNextShapeId( rCopy.nNextShapeId ),
      nShapeId( rCopy.nShapeId ),
      eShapeType( rCopy.eShapeType ),
      relativeHorizontalWidth( rCopy.relativeHorizontalWidth ),
      isHorizontalRule( rCopy.isHorizontalRule )
{
    eLineStyle       = rCopy.eLineStyle; // GPF-Bug #66227#
    eLineDashing     = rCopy.eLineDashing;
    bDrawHell        = rCopy.bDrawHell;
    bHidden          = rCopy.bHidden;
    bReplaceByFly    = rCopy.bReplaceByFly;
    bAutoWidth       = rCopy.bAutoWidth;
    bVFlip = rCopy.bVFlip;
    bHFlip = rCopy.bHFlip;
    nClientAnchorLen = rCopy.nClientAnchorLen;
    if( rCopy.nClientAnchorLen )
    {
        pClientAnchorBuffer.reset( new char[ nClientAnchorLen ] );
        memcpy( pClientAnchorBuffer.get(),
                rCopy.pClientAnchorBuffer.get(),
                nClientAnchorLen );
    }
    else
        pClientAnchorBuffer = nullptr;

    nClientDataLen = rCopy.nClientDataLen;
    if( rCopy.nClientDataLen )
    {
        pClientDataBuffer.reset( new char[ nClientDataLen ] );
        memcpy( pClientDataBuffer.get(),
                rCopy.pClientDataBuffer.get(),
                nClientDataLen );
    }
    else
        pClientDataBuffer = nullptr;

    if (rCopy.pWrapPolygon)
        pWrapPolygon = rCopy.pWrapPolygon;
}

SvxMSDffImportRec::~SvxMSDffImportRec()
{
}

void SvxMSDffManager::insertShapeId( sal_Int32 nShapeId, SdrObject* pShape )
{
    maShapeIdContainer[nShapeId] = pShape;
}

void SvxMSDffManager::removeShapeId( SdrObject const * pShape )
{
    SvxMSDffShapeIdContainer::iterator aIter = std::find_if(maShapeIdContainer.begin(), maShapeIdContainer.end(),
        [&pShape](const SvxMSDffShapeIdContainer::value_type& rEntry) { return rEntry.second == pShape; });
    if (aIter != maShapeIdContainer.end())
        maShapeIdContainer.erase( aIter );
}

SdrObject* SvxMSDffManager::getShapeForId( sal_Int32 nShapeId )
{
    SvxMSDffShapeIdContainer::iterator aIter( maShapeIdContainer.find(nShapeId) );
    return aIter != maShapeIdContainer.end() ? (*aIter).second : nullptr;
}

SvxMSDffImportData::SvxMSDffImportData(const tools::Rectangle& rParentRect)
    : aParentRect(rParentRect)
{
}

SvxMSDffImportData::~SvxMSDffImportData()
{
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
