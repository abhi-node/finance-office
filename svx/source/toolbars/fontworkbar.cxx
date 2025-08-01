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

#include <svx/svdundo.hxx>
#include <sfx2/request.hxx>
#include <sfx2/objface.hxx>
#include <sfx2/viewsh.hxx>
#include <svx/unoapi.hxx>
#include <com/sun/star/drawing/XShape.hpp>
#include <com/sun/star/drawing/XEnhancedCustomShapeDefaulter.hpp>
#include <sal/log.hxx>
#include <svx/dialmgr.hxx>
#include <svx/svdoashp.hxx>
#include <svx/strings.hrc>
#include <svx/svdpage.hxx>
#include <svx/svdview.hxx>
#include <svx/sdasitm.hxx>
#include <svx/gallery.hxx>
#include <svx/fmmodel.hxx>
#include <svx/sdtfsitm.hxx>
#include <svl/itempool.hxx>
#include <svl/stritem.hxx>
#include <sfx2/bindings.hxx>
#include <editeng/eeitem.hxx>
#include <editeng/charscaleitem.hxx>
#include <editeng/kernitem.hxx>

#include <svx/svxids.hrc>
#include <svx/fontworkbar.hxx>
#include <svx/fontworkgallery.hxx>


using namespace ::svx;
using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::uno;

static void SetAlignmentState( SdrView const * pSdrView, SfxItemSet& rSet )
{
    const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
    const size_t nCount = rMarkList.GetMarkCount();

    sal_Int32   nAlignment = -1;
    for( size_t i = 0; i < nCount; ++i )
    {
        SdrObject* pObj = rMarkList.GetMark( i )->GetMarkedSdrObj();
        if( dynamic_cast<const SdrObjCustomShape*>( pObj) !=  nullptr )
        {
            sal_Int32 nOldAlignment = nAlignment;
            const SdrTextHorzAdjustItem&      rTextHorzAdjustItem    = pObj->GetMergedItem( SDRATTR_TEXT_HORZADJUST );
            const SdrTextFitToSizeTypeItem&   rTextFitToSizeTypeItem = pObj->GetMergedItem( SDRATTR_TEXT_FITTOSIZE );
            switch ( rTextHorzAdjustItem.GetValue() )
            {
                case SDRTEXTHORZADJUST_LEFT   : nAlignment = 0; break;
                case SDRTEXTHORZADJUST_CENTER : nAlignment = 1; break;
                case SDRTEXTHORZADJUST_RIGHT  : nAlignment = 2; break;
                case SDRTEXTHORZADJUST_BLOCK  :
                {
                    auto const fit(rTextFitToSizeTypeItem.GetValue());
                    if (fit == drawing::TextFitToSizeType_NONE)
                    {
                        nAlignment = 3;
                    }
                    else if (fit == drawing::TextFitToSizeType_ALLLINES ||
                             fit == drawing::TextFitToSizeType_PROPORTIONAL)
                    {
                        nAlignment = 4;
                    }
                }
            }
            if ( ( nOldAlignment != -1 ) && ( nOldAlignment != nAlignment ) )
            {
                nAlignment = -1;
                break;
            }
        }
    }
    rSet.Put( SfxInt32Item( SID_FONTWORK_ALIGNMENT, nAlignment ) );
}

static void SetCharacterSpacingState( SdrView const * pSdrView, SfxItemSet& rSet )
{
    const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
    const size_t nCount = rMarkList.GetMarkCount();

    sal_Int32   nCharacterSpacing = -1;
    for( size_t i = 0; i < nCount; ++i )
    {
        SdrObject* pObj = rMarkList.GetMark( i )->GetMarkedSdrObj();
        if( dynamic_cast<const SdrObjCustomShape*>( pObj) !=  nullptr )
        {
            sal_Int32 nOldCharacterSpacing = nCharacterSpacing;
            const SvxCharScaleWidthItem& rCharScaleWidthItem = pObj->GetMergedItem( EE_CHAR_FONTWIDTH );
            nCharacterSpacing = rCharScaleWidthItem.GetValue();
            if ( ( nOldCharacterSpacing != -1 ) && ( nOldCharacterSpacing != nCharacterSpacing ) )
            {
                nCharacterSpacing = -1;
                break;
            }
        }
    }
    rSet.Put( SfxInt32Item( SID_FONTWORK_CHARACTER_SPACING, nCharacterSpacing ) );
}


static void SetKernCharacterPairsState( SdrView const * pSdrView, SfxItemSet& rSet )
{
    const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
    const size_t nCount = rMarkList.GetMarkCount();

    bool    bChecked = false;
    for( size_t i = 0; i < nCount; ++i )
    {
        SdrObject* pObj = rMarkList.GetMark( i )->GetMarkedSdrObj();
        if( dynamic_cast<const SdrObjCustomShape*>( pObj) !=  nullptr )
        {
            const SvxKerningItem& rKerningItem = pObj->GetMergedItem( EE_CHAR_KERNING );
            if ( rKerningItem.GetValue() )
                bChecked = true;
        }
    }
    rSet.Put( SfxBoolItem( SID_FONTWORK_KERN_CHARACTER_PAIRS, bChecked ) );
}

static void SetFontWorkShapeTypeState( SdrView const * pSdrView, SfxItemSet& rSet )
{
    const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
    const size_t nCount = rMarkList.GetMarkCount();

    OUString aFontWorkShapeType;

    for( size_t i = 0; i < nCount; ++i )
    {
        SdrObject* pObj = rMarkList.GetMark( i )->GetMarkedSdrObj();
        if( dynamic_cast<const SdrObjCustomShape*>( pObj) !=  nullptr )
        {
            const SdrCustomShapeGeometryItem & rGeometryItem( pObj->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY ) );
            const Any* pAny = rGeometryItem.GetPropertyValueByName( u"Type"_ustr );
            if( pAny )
            {
                OUString aType;
                if ( *pAny >>= aType )
                {
                    if ( !aFontWorkShapeType.isEmpty() )
                    {
                        if ( aFontWorkShapeType != aType )  // different FontWorkShapeTypes selected ?
                        {
                            aFontWorkShapeType.clear();
                            break;
                        }
                    }
                    aFontWorkShapeType = aType;
                }
            }
        }
    }
    rSet.Put( SfxStringItem( SID_FONTWORK_SHAPE_TYPE, aFontWorkShapeType ) );
}

// Declare the default interface. (The slotmap must not be empty, so
// we enter something which never occurs here (hopefully).)
constexpr SfxSlot aFontworkBarSlots_Impl[] =
{
    { 0, SfxGroupId::NONE, SfxSlotMode::NONE, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, 0, SfxDisableFlags::NONE, u""_ustr }
};

SFX_IMPL_INTERFACE(FontworkBar, SfxShell)

void FontworkBar::InitInterface_Impl()
{
    GetStaticInterface()->RegisterObjectBar(SFX_OBJECTBAR_OBJECT, SfxVisibilityFlags::Invisible, ToolbarId::Svx_Fontwork_Bar);
}


FontworkBar::FontworkBar(SfxViewShell* pViewShell )
: SfxShell(pViewShell)
{
    DBG_ASSERT( pViewShell, "svx::FontworkBar::FontworkBar(), I need a viewshell!" );
    if( pViewShell )
        SetPool(&pViewShell->GetPool());

    SetName( SvxResId( RID_SVX_FONTWORK_BAR ));
}

FontworkBar::~FontworkBar()
{
    SetRepeatTarget(nullptr);
}

namespace svx {
bool checkForFontWork( const SdrObject* pObj )
{
    static constexpr OUString sTextPath = u"TextPath"_ustr;
    bool bFound = false;

    if( dynamic_cast<const SdrObjCustomShape*>( pObj) !=  nullptr )
    {
        const SdrCustomShapeGeometryItem & rGeometryItem( pObj->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY ) );
        const Any* pAny = rGeometryItem.GetPropertyValueByName( sTextPath, sTextPath );
        if( pAny )
            *pAny >>= bFound;
    }

    return bFound;
}

bool checkForSelectedFontWork( SdrView const * pSdrView )
{
    const SdrMarkList& rMarkList = pSdrView->GetMarkedObjectList();
    const size_t nCount = rMarkList.GetMarkCount();
    bool bFound = false;
    for(size_t i=0; (i<nCount) && !bFound ; ++i)
    {
        SdrObject* pObj = rMarkList.GetMark(i)->GetMarkedSdrObj();
        bFound = checkForFontWork(pObj);
    }
    return bFound;
}

static void impl_execute( SfxRequest const & rReq, SdrCustomShapeGeometryItem& rGeometryItem, SdrObject* pObj )
{
    sal_uInt16 nSID = rReq.GetSlot();
    switch( nSID )
    {
        case SID_FONTWORK_SAME_LETTER_HEIGHTS:
        {
            css::uno::Any* pAny = rGeometryItem.GetPropertyValueByName( u"TextPath"_ustr, u"SameLetterHeights"_ustr );

            bool bOn = false;
            if( pAny )
                (*pAny) >>= bOn;
            bOn = !bOn;
            css::beans::PropertyValue aPropValue;
            aPropValue.Name = "SameLetterHeights";
            aPropValue.Value <<= bOn;
            rGeometryItem.SetPropertyValue(u"TextPath"_ustr, aPropValue);
        }
        break;

        case SID_FONTWORK_ALIGNMENT:
        {
            const SfxInt32Item* pAlignItem = nullptr;
            if( rReq.GetArgs() && rReq.GetArgs()->GetItemState( SID_FONTWORK_ALIGNMENT, true, &pAlignItem ) == SfxItemState::SET )
            {
                sal_Int32 nValue = pAlignItem->GetValue();
                if ( ( nValue >= 0 ) && ( nValue < 5 ) )
                {
                    drawing::TextFitToSizeType eFTS = drawing::TextFitToSizeType_NONE;
                    SdrTextHorzAdjust eHorzAdjust;
                    switch ( nValue )
                    {
                        case 4 : eFTS = drawing::TextFitToSizeType_ALLLINES; [[fallthrough]];
                        case 3 : eHorzAdjust = SDRTEXTHORZADJUST_BLOCK; break;
                        default: eHorzAdjust = SDRTEXTHORZADJUST_LEFT; break;
                        case 1 : eHorzAdjust = SDRTEXTHORZADJUST_CENTER; break;
                        case 2 : eHorzAdjust = SDRTEXTHORZADJUST_RIGHT; break;
                    }
                    pObj->SetMergedItem( SdrTextHorzAdjustItem( eHorzAdjust ) );
                    pObj->SetMergedItem( SdrTextFitToSizeTypeItem( eFTS ) );
                    pObj->BroadcastObjectChange();
                }
            }
        }
        break;

        case SID_FONTWORK_CHARACTER_SPACING:
        {
            const SfxInt32Item* pSpacingItem = nullptr;
            if( rReq.GetArgs() && ( rReq.GetArgs()->GetItemState( SID_FONTWORK_CHARACTER_SPACING, true, &pSpacingItem ) == SfxItemState::SET ) )
            {
                sal_Int32 nCharSpacing = pSpacingItem->GetValue();
                pObj->SetMergedItem( SvxCharScaleWidthItem( static_cast<sal_uInt16>(nCharSpacing), EE_CHAR_FONTWIDTH ) );
                pObj->BroadcastObjectChange();
            }
        }
        break;

        case SID_FONTWORK_KERN_CHARACTER_PAIRS:
        {
            if( rReq.GetArgs() && ( rReq.GetArgs()->GetItemState( SID_FONTWORK_KERN_CHARACTER_PAIRS ) == SfxItemState::SET ) )
            {
                // sal_Bool bKernCharacterPairs = ((const SfxBoolItem*)rReq.GetArgs()->GetItem(SID_FONTWORK_KERN_CHARACTER_PAIRS))->GetValue();
//TODO:             pObj->SetMergedItem( SvxCharScaleWidthItem( (sal_uInt16)nCharSpacing, EE_CHAR_FONTWIDTH ) );
                pObj->BroadcastObjectChange();
            }
        }
        break;
    }
}

static void GetGeometryForCustomShape( SdrCustomShapeGeometryItem& rGeometryItem, const OUString& rCustomShape )
{
    static constexpr OUString sType( u"Type"_ustr );

    css::beans::PropertyValue aPropVal;
    aPropVal.Name = sType;
    aPropVal.Value <<= rCustomShape;
    rGeometryItem.SetPropertyValue( aPropVal );

    static constexpr OUString sAdjustmentValues( u"AdjustmentValues"_ustr );
    static constexpr OUString sCoordinateOrigin( u"CoordinateOrigin"_ustr );
    static constexpr OUString sCoordinateSize( u"CoordinateSize"_ustr );
    static constexpr OUString sEquations( u"Equations"_ustr );
    static constexpr OUString sHandles( u"Handles"_ustr );
    static constexpr OUString sPath( u"Path"_ustr );
    rGeometryItem.ClearPropertyValue( sAdjustmentValues );
    rGeometryItem.ClearPropertyValue( sCoordinateOrigin );
    rGeometryItem.ClearPropertyValue( sCoordinateSize );
    rGeometryItem.ClearPropertyValue( sEquations );
    rGeometryItem.ClearPropertyValue( sHandles );
    rGeometryItem.ClearPropertyValue( sPath );

    /* SJ: CustomShapes that are available in the gallery are having the highest
       priority, so we will take a look there before taking the internal default */

    if ( !GalleryExplorer::GetSdrObjCount( GALLERY_THEME_POWERPOINT ) )
        return;

    std::vector< OUString > aObjList;
    if ( !GalleryExplorer::FillObjListTitle( GALLERY_THEME_POWERPOINT, aObjList ) )
        return;

    for ( std::vector<OUString>::size_type i = 0; i < aObjList.size(); i++ )
    {
        if ( aObjList[ i ].equalsIgnoreAsciiCase( rCustomShape ) )
        {
            FmFormModel aFormModel;

            if ( GalleryExplorer::GetSdrObj( GALLERY_THEME_POWERPOINT, i, &aFormModel ) )
            {
                const SdrObject* pSourceObj = nullptr;
                if (aFormModel.GetPageCount() > 0)
                    pSourceObj = aFormModel.GetPage( 0 )->GetObj( 0 );
                SAL_WARN_IF(!pSourceObj, "svx.form", "No content in gallery custom shape '" << rCustomShape << "'" );
                if( pSourceObj )
                {
                    PropertyValue aPropVal_;
                    const SdrCustomShapeGeometryItem& rSourceGeometry = pSourceObj->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY );
                    const css::uno::Any* pAny = rSourceGeometry.GetPropertyValueByName( sType );
                    if ( pAny )
                    {
                        aPropVal_.Name = sType;
                        aPropVal_.Value = *pAny;
                        rGeometryItem.SetPropertyValue( aPropVal_ );
                    }
                    pAny = rSourceGeometry.GetPropertyValueByName( sAdjustmentValues );
                    if ( pAny )
                    {
                        aPropVal_.Name = sAdjustmentValues;
                        aPropVal_.Value = *pAny;
                        rGeometryItem.SetPropertyValue( aPropVal_ );
                    }
                    pAny = rSourceGeometry.GetPropertyValueByName( sCoordinateOrigin );
                    if ( pAny )
                    {
                        aPropVal_.Name = sCoordinateOrigin;
                        aPropVal_.Value = *pAny;
                        rGeometryItem.SetPropertyValue( aPropVal_ );
                    }
                    pAny = rSourceGeometry.GetPropertyValueByName( sCoordinateSize );
                    if ( pAny )
                    {
                        aPropVal_.Name = sCoordinateSize;
                        aPropVal_.Value = *pAny;
                        rGeometryItem.SetPropertyValue( aPropVal_ );
                    }
                    pAny = rSourceGeometry.GetPropertyValueByName( sEquations );
                    if ( pAny )
                    {
                        aPropVal_.Name = sEquations;
                        aPropVal_.Value = *pAny;
                        rGeometryItem.SetPropertyValue( aPropVal_ );
                    }
                    pAny = rSourceGeometry.GetPropertyValueByName( sHandles );
                    if ( pAny )
                    {
                        aPropVal_.Name = sHandles;
                        aPropVal_.Value = *pAny;
                        rGeometryItem.SetPropertyValue( aPropVal_ );
                    }
                    pAny = rSourceGeometry.GetPropertyValueByName( sPath );
                    if ( pAny )
                    {
                        aPropVal_.Name = sPath;
                        aPropVal_.Value = *pAny;
                        rGeometryItem.SetPropertyValue( aPropVal_ );
                    }
                }
            }
        }
    }
}


void FontworkBar::execute( SdrView& rSdrView, SfxRequest const & rReq, SfxBindings& rBindings )
{
    TranslateId pStrResId;

    sal_uInt16 nSID = rReq.GetSlot();
    switch( nSID )
    {
        case SID_FONTWORK_GALLERY_FLOATER:
        {
            std::shared_ptr<FontWorkGalleryDialog> pDlg = std::make_shared<FontWorkGalleryDialog>(rReq.GetFrameWeld(), rSdrView, rBindings.GetActiveFrame());
            weld::DialogController::runAsync(pDlg, [](int){});
        }
        break;

        case SID_FONTWORK_SHAPE_TYPE:
        {
            OUString aCustomShape;
            const SfxItemSet* pArgs = rReq.GetArgs();
            if ( pArgs )
            {
                const SfxStringItem& rItm = static_cast<const SfxStringItem&>(pArgs->Get( rReq.GetSlot() ));
                aCustomShape = rItm.GetValue();
            }
            if ( !aCustomShape.isEmpty() )
            {
                const SdrMarkList& rMarkList = rSdrView.GetMarkedObjectList();
                const size_t nCount = rMarkList.GetMarkCount();
                for( size_t i = 0; i < nCount; ++i )
                {
                    SdrObject* pObj = rMarkList.GetMark( i )->GetMarkedSdrObj();
                    if( auto pCustomShape = dynamic_cast<SdrObjCustomShape*>( pObj) )
                    {
                        const bool bUndo = rSdrView.IsUndoEnabled();

                        if( bUndo )
                        {
                            OUString aStr( SvxResId( RID_SVXSTR_UNDO_APPLY_FONTWORK_SHAPE ) );
                            rSdrView.BegUndo(aStr);
                            rSdrView.AddUndo(rSdrView.GetModel().GetSdrUndoFactory().CreateUndoAttrObject(*pObj));
                        }
                        SdrCustomShapeGeometryItem aGeometryItem( pObj->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY ) );
                        GetGeometryForCustomShape( aGeometryItem, aCustomShape );
                        pObj->SetMergedItem( aGeometryItem );

                        Reference< drawing::XShape > aXShape = GetXShapeForSdrObject( pCustomShape );
                        if ( aXShape.is() )
                        {
                            Reference< drawing::XEnhancedCustomShapeDefaulter > xDefaulter( aXShape, UNO_QUERY );
                            if( xDefaulter.is() )
                                xDefaulter->createCustomShapeDefaults( aCustomShape );
                        }

                        pObj->BroadcastObjectChange();
                        if (bUndo)
                            rSdrView.EndUndo();
                        rSdrView.AdjustMarkHdl(); //HMH sal_True );
                        rBindings.Invalidate( SID_FONTWORK_SHAPE_TYPE );
                    }
                }
            }
        }
        break;

        case SID_FONTWORK_CHARACTER_SPACING_DIALOG :
        {
            const SfxInt32Item* pSpacingItem = nullptr;
            if( rReq.GetArgs() && ( rReq.GetArgs()->GetItemState( SID_FONTWORK_CHARACTER_SPACING, true, &pSpacingItem ) == SfxItemState::SET ) )
            {
                sal_Int32 nCharSpacing = pSpacingItem->GetValue();
                FontworkCharacterSpacingDialog aDlg(rReq.GetFrameWeld(), nCharSpacing);
                sal_uInt16 nRet = aDlg.run();
                if (nRet != RET_CANCEL)
                {
                    SfxInt32Item aItem(SID_FONTWORK_CHARACTER_SPACING, aDlg.getScale());
                    SfxPoolItem* aItems[] = { &aItem, nullptr };
                    rBindings.Execute( SID_FONTWORK_CHARACTER_SPACING, const_cast<const SfxPoolItem**>(aItems) );
                }
            }
        }
        break;

        case SID_FONTWORK_SHAPE:
        case SID_FONTWORK_ALIGNMENT:
        {
            if ( !pStrResId )
                pStrResId = RID_SVXSTR_UNDO_APPLY_FONTWORK_ALIGNMENT;
            [[fallthrough]];
        }
        case SID_FONTWORK_CHARACTER_SPACING:
        {
            if ( !pStrResId )
                pStrResId = RID_SVXSTR_UNDO_APPLY_FONTWORK_CHARACTER_SPACING;
            [[fallthrough]];
        }
        case SID_FONTWORK_KERN_CHARACTER_PAIRS:
        {
            if ( !pStrResId )
                pStrResId = RID_SVXSTR_UNDO_APPLY_FONTWORK_CHARACTER_SPACING;
            [[fallthrough]];
        }
        case SID_FONTWORK_SAME_LETTER_HEIGHTS:
        {
            if ( !pStrResId )
                pStrResId = RID_SVXSTR_UNDO_APPLY_FONTWORK_SAME_LETTER_HEIGHT;

            const SdrMarkList& rMarkList = rSdrView.GetMarkedObjectList();
            const size_t nCount = rMarkList.GetMarkCount();
            for( size_t i = 0; i < nCount; ++i )
            {
                SdrObject* pObj = rMarkList.GetMark(i)->GetMarkedSdrObj();
                if( dynamic_cast<const SdrObjCustomShape*>( pObj) !=  nullptr )
                {
                    const bool bUndo = rSdrView.IsUndoEnabled();
                    if( bUndo )
                    {
                        OUString aStr( SvxResId( pStrResId ) );
                        rSdrView.BegUndo(aStr);
                        rSdrView.AddUndo(rSdrView.GetModel().GetSdrUndoFactory().CreateUndoAttrObject(*pObj));
                    }
                    SdrCustomShapeGeometryItem aGeometryItem( pObj->GetMergedItem( SDRATTR_CUSTOMSHAPE_GEOMETRY ) );
                    impl_execute( rReq, aGeometryItem, pObj );
                    pObj->SetMergedItem( aGeometryItem );
                    pObj->BroadcastObjectChange();
                    if (bUndo)
                        rSdrView.EndUndo();
                }
            }
        }
        break;
    }
}

void FontworkBar::getState( SdrView const * pSdrView, SfxItemSet& rSet )
{
    if ( checkForSelectedFontWork( pSdrView ) )
    {
        SetAlignmentState( pSdrView, rSet );
        SetCharacterSpacingState( pSdrView, rSet );
        SetKernCharacterPairsState( pSdrView, rSet );
        SetFontWorkShapeTypeState( pSdrView, rSet );
    }
    else
    {
        rSet.DisableItem( SID_FONTWORK_ALIGNMENT_FLOATER );
        rSet.DisableItem( SID_FONTWORK_ALIGNMENT );
        rSet.DisableItem( SID_FONTWORK_CHARACTER_SPACING_FLOATER );
        rSet.DisableItem( SID_FONTWORK_CHARACTER_SPACING );
        rSet.DisableItem( SID_FONTWORK_KERN_CHARACTER_PAIRS );
        rSet.DisableItem( SID_FONTWORK_SAME_LETTER_HEIGHTS );
        rSet.DisableItem( SID_FONTWORK_SHAPE_TYPE );
    }
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
