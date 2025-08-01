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
#include <DocumentFieldsManager.hxx>
#include <config_features.h>
#include <config_fuzzers.h>
#include <doc.hxx>
#include <IDocumentUndoRedo.hxx>
#include <IDocumentState.hxx>
#include <IDocumentRedlineAccess.hxx>
#include <redline.hxx>
#include <rootfrm.hxx>
#include <dbmgr.hxx>
#include <chpfld.hxx>
#include <dbfld.hxx>
#include <reffld.hxx>
#include <flddropdown.hxx>
#include <strings.hrc>
#include <SwUndoField.hxx>
#include <flddat.hxx>
#include <cntfrm.hxx>
#include <node2lay.hxx>
#include <section.hxx>
#include <docufld.hxx>
#include <calbck.hxx>
#include <cellatr.hxx>
#include <swtable.hxx>
#include <frmfmt.hxx>
#include <fmtfld.hxx>
#include <ndtxt.hxx>
#include <txtfld.hxx>
#include <docfld.hxx>
#include <hints.hxx>
#include <docary.hxx>
#include <fldbas.hxx>
#include <expfld.hxx>
#include <ddefld.hxx>
#include <authfld.hxx>
#include <usrfld.hxx>
#include <ndindex.hxx>
#include <pam.hxx>
#include <o3tl/deleter.hxx>
#include <osl/diagnose.h>
#include <unotools/transliterationwrapper.hxx>
#include <comphelper/scopeguard.hxx>
#include <com/sun/star/uno/Any.hxx>

using namespace ::com::sun::star::uno;

namespace sw
{
    bool IsFieldDeletedInModel(IDocumentRedlineAccess const& rIDRA,
            SwTextField const& rTextField)
    {
        SwRedlineTable::size_type tmp;
        SwPosition const pos(rTextField.GetTextNode(),
                rTextField.GetStart());
        SwRangeRedline const*const pRedline(rIDRA.GetRedline(pos, &tmp));
        return (pRedline
            && pRedline->GetType() == RedlineType::Delete
            && *pRedline->GetPoint() != *pRedline->GetMark());
    }
}

namespace
{
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS

    OUString lcl_GetDBVarName( SwDoc& rDoc, SwDBNameInfField& rDBField )
    {
        SwDBData aDBData( rDBField.GetDBData( &rDoc ));
        OUString sDBNumNm;
        SwDBData aDocData = rDoc.GetDBData();

        if( aDBData != aDocData )
        {
            sDBNumNm = aDBData.sDataSource + OUStringChar(DB_DELIM)
                + aDBData.sCommand + OUStringChar(DB_DELIM);
        }
        sDBNumNm += SwFieldType::GetTypeStr(SwFieldTypesEnum::DatabaseSetNumber);

        return sDBNumNm;
    }

#endif

    bool IsFieldDeleted(IDocumentRedlineAccess const& rIDRA,
            SwRootFrame const& rLayout, SwTextField const& rTextField)
    {
        SwTextNode const& rNode(rTextField.GetTextNode());
        bool const isInBody(
            rNode.GetNodes().GetEndOfExtras().GetIndex() < rNode.GetIndex());
        if (!isInBody && nullptr == rNode.getLayoutFrame(&rLayout))
        {   // see SwDocUpdateField::GetBodyNode() - fields in hidden sections
            // don't have layout frames but must be updated, so use the same
            // check as there, but do it again because GetBodyNode() checks
            // for *any* layout...
            return true;
        }
        return sw::IsFieldDeletedInModel(rIDRA, rTextField);
    }

    void lcl_CalcField( SwDoc& rDoc, SwCalc& rCalc, const SetGetExpField& rSGEField,
            SwDBManager* pMgr, SwRootFrame const*const pLayout)
    {
        const SwTextField* pTextField = rSGEField.GetTextField();
        if( !pTextField )
            return ;

        if (pLayout && pLayout->IsHideRedlines()
            && IsFieldDeleted(rDoc.getIDocumentRedlineAccess(), *pLayout, *pTextField))
        {
            return;
        }

        const SwField* pField = pTextField->GetFormatField().GetField();
        const SwFieldIds nFieldWhich = pField->GetTyp()->Which();

        if( SwFieldIds::SetExp == nFieldWhich )
        {
            auto pSetExpField = static_cast<const SwSetExpField*>(pField);
            SwSbxValue aValue;
            if( SwGetSetExpType::Expr & pSetExpField->GetSubType() )
                aValue.PutDouble( pSetExpField->GetValue(pLayout) );
            else
                // Extension to calculate with Strings
                aValue.PutString( pSetExpField->GetExpStr(pLayout) );

            // set the new value in Calculator
            rCalc.VarChange( pField->GetTyp()->GetName().toString(), aValue );
        }
        else if( pMgr )
        {
    #if !HAVE_FEATURE_DBCONNECTIVITY || ENABLE_FUZZERS
            (void) rDoc;
    #else
            switch( nFieldWhich )
            {
            case SwFieldIds::DbNumSet:
                {
                    SwDBNumSetField* pDBField = const_cast<SwDBNumSetField*>(static_cast<const SwDBNumSetField*>(pField));

                    SwDBData aDBData(pDBField->GetDBData(&rDoc));

                    if( pDBField->IsCondValid() &&
                        pMgr->OpenDataSource( aDBData.sDataSource, aDBData.sCommand ))
                        rCalc.VarChange( lcl_GetDBVarName( rDoc, *pDBField),
                                        pDBField->GetFormat() );
                }
                break;
            case SwFieldIds::DbNextSet:
                {
                    SwDBNextSetField* pDBField = const_cast<SwDBNextSetField*>(static_cast<const SwDBNextSetField*>(pField));
                    SwDBData aDBData(pDBField->GetDBData(&rDoc));
                    if( !pDBField->IsCondValid() ||
                        !pMgr->OpenDataSource( aDBData.sDataSource, aDBData.sCommand ))
                        break;

                    OUString sDBNumNm(lcl_GetDBVarName( rDoc, *pDBField));
                    SwCalcExp* pExp = rCalc.VarLook( sDBNumNm );
                    if( pExp )
                        rCalc.VarChange( sDBNumNm, pExp->nValue.GetLong() + 1 );
                }
                break;

            default: break;
            }
    #endif
        }
    }
}

namespace sw
{

DocumentFieldsManager::DocumentFieldsManager( SwDoc& i_rSwdoc ) : m_rDoc( i_rSwdoc ),
                                                                  mbNewFieldLst(true),
                                                                  mpUpdateFields(new SwDocUpdateField(m_rDoc)),
                                                                  mpFieldTypes( new SwFieldTypes ),
                                                                  mnLockExpField( 0 )
{
}

const SwFieldTypes* DocumentFieldsManager::GetFieldTypes() const
{
    return mpFieldTypes.get();
}

/** Insert field types
 *
 * @param rFieldTyp ???
 * @return Always returns a pointer to the type, if it's new or already added.
 */
SwFieldType* DocumentFieldsManager::InsertFieldType(const SwFieldType &rFieldTyp)
{
    const SwFieldTypes::size_type nSize = mpFieldTypes->size();
    const SwFieldIds nFieldWhich = rFieldTyp.Which();

    SwFieldTypes::size_type i = INIT_FLDTYPES;

    switch( nFieldWhich )
    {
    case SwFieldIds::SetExp:
            //JP 29.01.96: SequenceFields start at INIT_FLDTYPES - 3!!
            //             Or we get doubble number circles!!
            //MIB 14.03.95: From now on also the SW3-Reader relies on this, when
            //constructing string pools and when reading SetExp fields
            if( SwGetSetExpType::Sequence & static_cast<const SwSetExpFieldType&>(rFieldTyp).GetType() )
                i -= INIT_SEQ_FLDTYPES;
            [[fallthrough]];
    case SwFieldIds::Database:
    case SwFieldIds::User:
    case SwFieldIds::Dde:
        {
            const ::utl::TransliterationWrapper& rSCmp = GetAppCmpStrIgnore();
            OUString sFieldNm( rFieldTyp.GetName().toString() );
            for( ; i < nSize; ++i )
                if( nFieldWhich == (*mpFieldTypes)[i]->Which() &&
                    rSCmp.isEqual( sFieldNm, (*mpFieldTypes)[i]->GetName().toString() ))
                        return (*mpFieldTypes)[i].get();
        }
        break;

    case SwFieldIds::TableOfAuthorities:
        for( ; i < nSize; ++i )
            if( nFieldWhich == (*mpFieldTypes)[i]->Which() )
                return (*mpFieldTypes)[i].get();
        break;

    default:
        for( i = 0; i < nSize; ++i )
            if( nFieldWhich == (*mpFieldTypes)[i]->Which() )
                return (*mpFieldTypes)[i].get();
    }

    std::unique_ptr<SwFieldType> pNew = rFieldTyp.Copy();
    switch( nFieldWhich )
    {
    case SwFieldIds::Dde:
        static_cast<SwDDEFieldType*>(pNew.get())->SetDoc( &m_rDoc );
        break;

    case SwFieldIds::Database:
    case SwFieldIds::Table:
    case SwFieldIds::DateTime:
    case SwFieldIds::GetExp:
        static_cast<SwValueFieldType*>(pNew.get())->SetDoc( &m_rDoc );
        break;

    case SwFieldIds::User:
    case SwFieldIds::SetExp:
        static_cast<SwValueFieldType*>(pNew.get())->SetDoc( &m_rDoc );
        // JP 29.07.96: Optionally prepare FieldList for Calculator:
        mpUpdateFields->InsertFieldType( *pNew );
        break;
    case SwFieldIds::TableOfAuthorities :
        static_cast<SwAuthorityFieldType*>(pNew.get())->SetDoc( &m_rDoc );
        break;
    default: break;
    }

    mpFieldTypes->insert( mpFieldTypes->begin() + nSize, std::move(pNew) );
    m_rDoc.getIDocumentState().SetModified();

    return (*mpFieldTypes)[ nSize ].get();
}

/// @returns the field type of the Doc
SwFieldType *DocumentFieldsManager::GetSysFieldType( const SwFieldIds eWhich ) const
{
    for( SwFieldTypes::size_type i = 0; i < INIT_FLDTYPES; ++i )
        if( eWhich == (*mpFieldTypes)[i]->Which() )
            return (*mpFieldTypes)[i].get();
    return nullptr;
}

/// Find first type with ResId and name
SwFieldType* DocumentFieldsManager::GetFieldType(
    SwFieldIds nResId,
    const OUString& rName,
    bool bDbFieldMatching // used in some UNO calls for SwFieldIds::Database to use different string matching code #i51815#
    ) const
{
    const SwFieldTypes::size_type nSize = mpFieldTypes->size();
    SwFieldTypes::size_type i {0};
    const ::utl::TransliterationWrapper& rSCmp = GetAppCmpStrIgnore();

    switch( nResId )
    {
    case SwFieldIds::SetExp:
            //JP 29.01.96: SequenceFields start at INIT_FLDTYPES - 3!!
            //             Or we get doubble number circles!!
            //MIB 14.03.95: From now on also the SW3-Reader relies on this, when
            //constructing string pools and when reading SetExp fields
        i = INIT_FLDTYPES - INIT_SEQ_FLDTYPES;
        break;

    case SwFieldIds::Database:
    case SwFieldIds::User:
    case SwFieldIds::Dde:
    case SwFieldIds::TableOfAuthorities:
        i = INIT_FLDTYPES;
        break;
    default: break;
    }

    SwFieldType* pRet = nullptr;
    for( ; i < nSize; ++i )
    {
        SwFieldType* pFieldType = (*mpFieldTypes)[i].get();

        if (nResId == pFieldType->Which())
        {
            OUString aFieldName( pFieldType->GetName().toString() );
            if (bDbFieldMatching && nResId == SwFieldIds::Database)    // #i51815#
                aFieldName = aFieldName.replace(DB_DELIM, '.');

            if (rSCmp.isEqual( rName, aFieldName ))
            {
                pRet = pFieldType;
                break;
            }
        }
    }
    return pRet;
}

/// Remove field type
void DocumentFieldsManager::RemoveFieldType(size_t nField)
{
    OSL_ENSURE( INIT_FLDTYPES <= nField,  "don't remove InitFields" );
    /*
     * Dependent fields present -> ErrRaise
     */
    if(nField >= mpFieldTypes->size())
        return;

    SwFieldType* pTmp = (*mpFieldTypes)[nField].get();

    // JP 29.07.96: Optionally prepare FieldList for Calculator
    SwFieldIds nWhich = pTmp->Which();
    switch( nWhich )
    {
    case SwFieldIds::SetExp:
    case SwFieldIds::User:
        mpUpdateFields->RemoveFieldType( *pTmp );
        [[fallthrough]];
    case SwFieldIds::Dde:
        if( pTmp->HasWriterListeners() && !m_rDoc.IsUsed( *pTmp ) )
        {
            if( SwFieldIds::SetExp == nWhich )
                static_cast<SwSetExpFieldType*>(pTmp)->SetDeleted( true );
            else if( SwFieldIds::User == nWhich )
                static_cast<SwUserFieldType*>(pTmp)->SetDeleted( true );
            else
                static_cast<SwDDEFieldType*>(pTmp)->SetDeleted( true );
            nWhich = SwFieldIds::Database;
        }
        break;
    default: break;
    }

    if( nWhich != SwFieldIds::Database )
    {
        OSL_ENSURE( !pTmp->HasWriterListeners(), "Dependent fields present!" );
    }
    else
    {
        // coverity[leaked_storage] - at this point DB fields are ref-counted and delete themselves
        (*mpFieldTypes)[nField].release();
    }

    mpFieldTypes->erase( mpFieldTypes->begin() + nField );
    m_rDoc.getIDocumentState().SetModified();
}

// All have to be re-evaluated.
void DocumentFieldsManager::UpdateFields(bool bCloseDB, bool bSetModified)
{
    // Tell all types to update their fields
    for(auto const& pFieldType: *mpFieldTypes)
        pFieldType->UpdateFields();

    if(!IsExpFieldsLocked())
        UpdateExpFields(nullptr, false); // update expression fields

    // Tables
    UpdateTableFields(nullptr);

    // References
    UpdateRefFields();
    if(bCloseDB)
    {
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
        m_rDoc.GetDBManager()->CloseAll();
#endif
    }
    if (bSetModified)
    {
        // Only evaluate on full update
        m_rDoc.getIDocumentState().SetModified();
    }
}

void DocumentFieldsManager::InsDeletedFieldType( SwFieldType& rFieldTyp )
{
    // The FieldType was marked as deleted and removed from the array.
    // One has to look this up again, now.
    // - If it's not present, it can be re-inserted.
    // - If the same type is found, the deleted one has to be renamed.

    const SwFieldTypes::size_type nSize = mpFieldTypes->size();
    const SwFieldIds nFieldWhich = rFieldTyp.Which();

    OSL_ENSURE( SwFieldIds::SetExp == nFieldWhich ||
            SwFieldIds::User == nFieldWhich ||
            SwFieldIds::Dde == nFieldWhich, "Wrong FieldType" );

    const ::utl::TransliterationWrapper& rSCmp = GetAppCmpStrIgnore();
    const OUString aFieldNm = rFieldTyp.GetName().toString();

    for( SwFieldTypes::size_type i = INIT_FLDTYPES; i < nSize; ++i )
    {
        SwFieldType* pFnd = (*mpFieldTypes)[i].get();
        if( nFieldWhich == pFnd->Which() &&
            rSCmp.isEqual( aFieldNm, pFnd->GetName().toString() ) )
        {
            // find new name
            SwFieldTypes::size_type nNum = 1;
            do {
                OUString sSrch = aFieldNm + OUString::number( nNum );
                for( i = INIT_FLDTYPES; i < nSize; ++i )
                {
                    pFnd = (*mpFieldTypes)[i].get();
                    if( nFieldWhich == pFnd->Which() &&
                        rSCmp.isEqual( sSrch, pFnd->GetName().toString() ) )
                        break;
                }
                if( i >= nSize )        // not found
                {
                    const_cast<OUString&>(aFieldNm) = sSrch;
                    break;      // exit while loop
                }
                ++nNum;
            } while( true );
            break;
        }
    }

    // not found, so insert, and updated deleted flag
    mpFieldTypes->insert( mpFieldTypes->begin() + nSize, std::unique_ptr<SwFieldType>(&rFieldTyp) );
    switch( nFieldWhich )
    {
    case SwFieldIds::SetExp:
        static_cast<SwSetExpFieldType&>(rFieldTyp).SetDeleted( false );
        break;
    case SwFieldIds::User:
        static_cast<SwUserFieldType&>(rFieldTyp).SetDeleted( false );
        break;
    case SwFieldIds::Dde:
        static_cast<SwDDEFieldType&>(rFieldTyp).SetDeleted( false );
        break;
    default: break;
    }
}

void DocumentFieldsManager::PutValueToField(const SwPosition & rPos,
                            const Any& rVal, sal_uInt16 nWhich)
{
    Any aOldVal;
    SwField * pField = GetFieldAtPos(rPos);

    if (m_rDoc.GetIDocumentUndoRedo().DoesUndo() &&
        pField->QueryValue(aOldVal, nWhich))
    {
        m_rDoc.GetIDocumentUndoRedo().AppendUndo(
            std::make_unique<SwUndoFieldFromAPI>(rPos, aOldVal, rVal, nWhich));
    }

    pField->PutValue(rVal, nWhich);
}

bool DocumentFieldsManager::UpdateField(SwTextField* pDstTextField, SwField& rSrcField, bool bUpdateFields)
{
    //static const sw::RefmarkFieldUpdate aRefMarkHint;
    assert(pDstTextField && "no field to update!");

    bool bTableSelBreak = false;

    SwFormatField * pDstFormatField = const_cast<SwFormatField*>(&pDstTextField->GetFormatField());
    SwField * pDstField = pDstFormatField->GetField();
    SwFieldIds nFieldWhich = rSrcField.GetTyp()->Which();
    SwNodeIndex aTableNdIdx(pDstTextField->GetTextNode());

    if (pDstField->GetTyp()->Which() ==
        rSrcField.GetTyp()->Which())
    {
        if (m_rDoc.GetIDocumentUndoRedo().DoesUndo())
        {
            SwPosition aPosition( pDstTextField->GetTextNode(), pDstTextField->GetStart() );
            m_rDoc.GetIDocumentUndoRedo().AppendUndo(std::make_unique<SwUndoFieldFromDoc>(aPosition, *pDstField, rSrcField, bUpdateFields));
        }

        pDstFormatField->SetField(rSrcField.CopyField());
        SwField* pNewField = pDstFormatField->GetField();

        switch( nFieldWhich )
        {
        case SwFieldIds::SetExp:
        case SwFieldIds::GetExp:
        case SwFieldIds::HiddenText:
        case SwFieldIds::HiddenPara:
            UpdateExpFields( pDstTextField, true );
            break;

        case SwFieldIds::Table:
            {
                const SwTableNode* pTableNd =
                    SwDoc::IsIdxInTable(aTableNdIdx);
                if( pTableNd )
                {
                    if (bUpdateFields)
                        UpdateTableFields(&pTableNd->GetTable());
                    else
                        pNewField->GetTyp()->CallSwClientNotify(sw::LegacyModifyHint(nullptr, nullptr));

                    if (! bUpdateFields)
                        bTableSelBreak = true;
                }
            }
            break;

        case SwFieldIds::Macro:
            if( bUpdateFields && pDstTextField->GetpTextNode() )
                pDstTextField->GetpTextNode()->TriggerNodeUpdate(sw::LegacyModifyHint(nullptr, pDstFormatField));
            break;

        case SwFieldIds::DatabaseName:
        case SwFieldIds::DbNextSet:
        case SwFieldIds::DbNumSet:
        case SwFieldIds::DbSetNumber:
            m_rDoc.ChgDBData(static_cast<SwDBNameInfField*>( pNewField)->GetRealDBData());
            pNewField->GetTyp()->UpdateFields();

            break;

        case SwFieldIds::Database:
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
            {
                // JP 10.02.96: call ChgValue, so that the style change sets the
                // ContentString correctly
                SwDBField* pDBField = static_cast<SwDBField*>(pNewField);
                if (pDBField->IsInitialized())
                    pDBField->ChgValue( pDBField->GetValue(), true );

                pDBField->ClearInitialized();
                pDBField->InitContent();
            }
#endif
            [[fallthrough]];

        default:
            pDstFormatField->ForceUpdateTextNode();
        }

        // The fields we can calculate here are being triggered for an update
        // here explicitly.
        if( nFieldWhich == SwFieldIds::User )
            UpdateUsrFields();
    }

    return bTableSelBreak;
}

/// Update reference and table fields
void DocumentFieldsManager::UpdateRefFields()
{
    for(auto const& pFieldType: *mpFieldTypes)
        if(SwFieldIds::GetRef == pFieldType->Which())
            static_cast<SwGetRefFieldType*>(pFieldType.get())->UpdateGetReferences();
}

void DocumentFieldsManager::UpdateTableFields(const SwTable* pTable)
{
    auto pFieldType = GetFieldType( SwFieldIds::Table, OUString(), false );
    if(pFieldType)
    {
        std::vector<SwFormatField*> vFields;
        pFieldType->GatherFields(vFields);
        for(auto pFormatField : vFields)
        {
            if(!pFormatField->GetTextField()->GetTextNode().FindTableNode())
                continue;
            SwTableField* pField = static_cast<SwTableField*>(pFormatField->GetField());
            // re-set the value flag
            // JP 17.06.96: internal representation of all formulas
            //              (reference to other table!!!)
            if(pTable && SwTableFieldSubType::Command & pField->GetSubType())
                pField->PtrToBoxNm(pTable);
            else
                // reset the value flag for all
                pField->ChgValid(false);
        }
    }
    // process all table box formulas
    std::vector<SwTableBoxFormula*> aTableBoxFormulas;
    SwTable::GatherFormulas(m_rDoc, aTableBoxFormulas);
    for (SwTableBoxFormula* pBoxFormula : aTableBoxFormulas)
    {
        if(pBoxFormula->GetDefinedIn())
            pBoxFormula->ChangeState();
    }

    SwRootFrame const* pLayout(nullptr);
    for (SwRootFrame const*const pLay : m_rDoc.GetAllLayouts())
    {
        assert(!pLayout || pLay->IsHideRedlines() == pLayout->IsHideRedlines()); // TODO
        pLayout = pLay;
    }

    std::optional<SwCalc> oCalc;

    if( pFieldType )
    {
        std::vector<SwFormatField*> vFields;
        pFieldType->GatherFields(vFields);
        for(SwFormatField* pFormatField: vFields)
        {
            // start calculation at the end
            // new fields are inserted at the beginning of the modify chain
            // that gives faster calculation on import
            // mba: do we really need this "optimization"? Is it still valid?
            SwTableField *const pField(static_cast<SwTableField*>(pFormatField->GetField()));
            if (SwTableFieldSubType::Command & pField->GetSubType())
                continue;

            // needs to be recalculated
            if( !pField->IsValid() )
            {
                // table where this field is located
                const SwTextNode& rTextNd = pFormatField->GetTextField()->GetTextNode();
                const SwTableNode* pTableNd = rTextNd.FindTableNode();
                if( !pTableNd )
                    continue;

                // if this field is not in the to-be-updated table, skip it
                if(pTable && &pTableNd->GetTable() != pTable)
                    continue;

                if( !oCalc )
                    oCalc.emplace( m_rDoc );

                // get the values of all SetExpression fields that are valid
                // until the table
                SwFrame* pFrame = nullptr;
                if( pTableNd->GetIndex() < m_rDoc.GetNodes().GetEndOfExtras().GetIndex() )
                {
                    // is in the special section, that's expensive!
                    Point aPt;      // return the first frame of the layout - Tab.Headline!!
                    std::pair<Point, bool> const tmp(aPt, true);
                    pFrame = rTextNd.getLayoutFrame(pLayout, nullptr, &tmp);
                    if( pFrame )
                    {
                        SwPosition aPos( *pTableNd );
                        if( GetBodyTextNode( m_rDoc, aPos, *pFrame ) )
                        {
                            FieldsToCalc( *oCalc, SetGetExpField(
                                    aPos.GetNode(), pFormatField->GetTextField(),
                                    aPos.GetContentIndex(), pFrame->GetPhyPageNum()),
                                pLayout);
                        }
                        else
                            pFrame = nullptr;
                    }
                }
                if( !pFrame )
                {
                    // create index to determine the TextNode
                    SwFrame const*const pFrame2 = ::sw::FindNeighbourFrameForNode(rTextNd);
                    FieldsToCalc( *oCalc,
                        SetGetExpField(rTextNd, pFormatField->GetTextField(),
                            std::nullopt,
                            pFrame2 ? pFrame2->GetPhyPageNum() : 0),
                        pLayout);
                }

                SwTableCalcPara aPara(*oCalc, pTableNd->GetTable(), pLayout);
                pField->CalcField( aPara );
                if( aPara.IsStackOverflow() )
                {
                    bool const bResult = aPara.CalcWithStackOverflow();
                    if (bResult)
                    {
                        pField->CalcField( aPara );
                    }
                    OSL_ENSURE(bResult,
                            "the chained formula could no be calculated");
                }
                oCalc->SetCalcError( SwCalcError::NONE );
            }
            pFormatField->ForceUpdateTextNode();
        }
    }

    // calculate the formula at the boxes
    SwTable::GatherFormulas(m_rDoc, aTableBoxFormulas);
    for (SwTableBoxFormula* pItem : aTableBoxFormulas)
    {
        auto & rFormula = *pItem;
        if(!rFormula.GetDefinedIn() || rFormula.IsValid())
            continue;
        SwTableBox* pBox = rFormula.GetTableBox();
        if(!pBox || !pBox->GetSttNd() || !pBox->GetSttNd()->GetNodes().IsDocNodes())
            continue;
        const SwTableNode* pTableNd = pBox->GetSttNd()->FindTableNode();
        if(pTable && &pTableNd->GetTable() != pTable)
            continue;
        double nValue;
        if( !oCalc )
            oCalc.emplace( m_rDoc );

        // get the values of all SetExpression fields that are valid
        // until the table
        SwFrame* pFrame = nullptr;
        if( pTableNd->GetIndex() < m_rDoc.GetNodes().GetEndOfExtras().GetIndex() )
        {
            // is in the special section, that's expensive!
            SwNodeIndex aCNdIdx( *pTableNd, +2 );
            SwContentNode* pCNd = aCNdIdx.GetNode().GetContentNode();
            if( !pCNd )
                pCNd = SwNodes::GoNext(&aCNdIdx);

            if (pCNd)
            {
                Point aPt;      // return the first frame of the layout - Tab.Headline!!
                std::pair<Point, bool> const tmp(aPt, true);
                pFrame = pCNd->getLayoutFrame(pLayout, nullptr, &tmp);
                if( pFrame )
                {
                    SwPosition aPos( *pCNd );
                    if( GetBodyTextNode( m_rDoc, aPos, *pFrame ) )
                    {
                        FieldsToCalc(*oCalc, SetGetExpField(aPos.GetNode(),
                                nullptr, std::nullopt, pFrame->GetPhyPageNum()),
                            pLayout);
                    }
                    else
                        pFrame = nullptr;
                }
            }
        }
        if( !pFrame )
        {
            // create index to determine the TextNode
            SwFrame const*const pFrame2 = ::sw::FindNeighbourFrameForNode(*pTableNd);
            FieldsToCalc(*oCalc, SetGetExpField(*pTableNd, nullptr, std::nullopt,
                    pFrame2 ? pFrame2->GetPhyPageNum() : 0),
                pLayout);
        }

        SwTableCalcPara aPara(*oCalc, pTableNd->GetTable(), pLayout);
        rFormula.Calc( aPara, nValue );

        if( aPara.IsStackOverflow() )
        {
            bool const bResult = aPara.CalcWithStackOverflow();
            if (bResult)
            {
                rFormula.Calc( aPara, nValue );
            }
            OSL_ENSURE(bResult,
                    "the chained formula could no be calculated");
        }

        SwFrameFormat* pFormat = pBox->ClaimFrameFormat();
        SfxItemSetFixed<RES_BOXATR_BEGIN,RES_BOXATR_END-1> aTmp( m_rDoc.GetAttrPool() );

        if( oCalc->IsCalcError() )
            nValue = DBL_MAX;
        aTmp.Put( SwTableBoxValue( nValue ));
        if( SfxItemState::SET != pFormat->GetItemState( RES_BOXATR_FORMAT ))
            aTmp.Put( SwTableBoxNumFormat( 0 ));
        pFormat->SetFormatAttr( aTmp );

        oCalc->SetCalcError( SwCalcError::NONE );
    }
}

void DocumentFieldsManager::UpdateExpFields( SwTextField* pUpdateField, bool bUpdRefFields )
{
    if( IsExpFieldsLocked() || m_rDoc.IsInReading() )
        return;

    bool bOldInUpdateFields = mpUpdateFields->IsInUpdateFields();
    mpUpdateFields->SetInUpdateFields( true );

    mpUpdateFields->MakeFieldList( m_rDoc, true, GETFLD_ALL );
    mbNewFieldLst = false;

    if (mpUpdateFields->GetSortList()->empty())
    {
        if( bUpdRefFields )
            UpdateRefFields();

        mpUpdateFields->SetInUpdateFields( bOldInUpdateFields );
        mpUpdateFields->SetFieldsDirty( false );
        return ;
    }

    SwRootFrame const* pLayout(nullptr);
    SwRootFrame const* pLayoutRLHidden(nullptr);
    for (SwRootFrame const*const pLay : m_rDoc.GetAllLayouts())
    {
        if (pLay->IsHideRedlines())
        {
            pLayoutRLHidden = pLay;
        }
        else
        {
            pLayout = pLay;
        }
    }
    if (pLayout || !pLayoutRLHidden) // always calc *something*...
    {
        UpdateExpFieldsImpl(pUpdateField, pLayout);
    }
    if (pLayoutRLHidden)
    {
        UpdateExpFieldsImpl(pUpdateField, pLayoutRLHidden);
    }

    // update reference fields
    if( bUpdRefFields )
        UpdateRefFields();

    mpUpdateFields->SetInUpdateFields( bOldInUpdateFields );
    mpUpdateFields->SetFieldsDirty( false );
}

void DocumentFieldsManager::UpdateExpFieldsImpl(
        SwTextField * pUpdateField, SwRootFrame const*const pLayout)
{
    SwFieldIds nWhich;

    // Hash table for all string replacements is filled on-the-fly.
    std::unordered_map<OUString, OUString> aHashStrTable;

    {
        const SwFieldType* pFieldType;
        // process separately:
        for( auto n = mpFieldTypes->size(); n; )
        {
            pFieldType = (*mpFieldTypes)[ --n ].get();
            switch( pFieldType->Which() )
            {
            case SwFieldIds::User:
                {
                    // Entry present?
                    const OUString aNm = pFieldType->GetName().toString();
                    OUString sExpand(const_cast<SwUserFieldType*>(static_cast<const SwUserFieldType*>(pFieldType))->Expand(1, SwUserType::None, LANGUAGE_SYSTEM));
                    auto pFnd = aHashStrTable.find( aNm );
                    if( pFnd != aHashStrTable.end() )
                        // modify entry in the hash table
                        pFnd->second = sExpand;
                    else
                        // insert the new entry
                        aHashStrTable.insert( { aNm, sExpand } );
                }
                break;
            default: break;
            }
        }
    }

    // The array is filled with all fields; start calculation.
    SwCalc aCalc( m_rDoc );

#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
    OUString sDBNumNm( SwFieldType::GetTypeStr( SwFieldTypesEnum::DatabaseSetNumber ) );

    // already set the current record number
    SwDBManager* pMgr = m_rDoc.GetDBManager();
    pMgr->CloseAll( false );

    SvtSysLocale aSysLocale;
    const LocaleDataWrapper* pLclData = &aSysLocale.GetLocaleData();
    const LanguageType nLang = pLclData->getLanguageTag().getLanguageType();
    bool bCanFill = pMgr->FillCalcWithMergeData( m_rDoc.GetNumberFormatter(), nLang, aCalc );
#endif

    // Make sure we don't hide all content, which would lead to a crash. First, count how many visible sections we have.
    int nShownSections = 0;
    SwNodeOffset nContentStart = m_rDoc.GetNodes().GetEndOfContent().StartOfSectionIndex() + 1;
    SwNodeOffset nContentEnd = m_rDoc.GetNodes().GetEndOfContent().GetIndex();
    SwSectionFormats& rSectFormats = m_rDoc.GetSections();
    for( SwSectionFormats::size_type n = 0; n<rSectFormats.size(); ++n )
    {
        SwSectionFormat& rSectFormat = *rSectFormats[ n ];
        SwSectionNode* pSectionNode = rSectFormat.GetSectionNode();
        SwSection* pSect = rSectFormat.GetSection();

        // Usually some of the content is not in a section: count that as a virtual section, so that all real sections can be hidden.
        // Only look for section gaps at the lowest level, ignoring sub-sections.
        if ( pSectionNode && !rSectFormat.GetParent() )
        {
            SwNodeIndex aNextIdx( *pSectionNode->EndOfSectionNode(), 1 );
            if ( n == 0 && pSectionNode->GetIndex() != nContentStart )
                nShownSections++;  //document does not start with a section
            if ( n == rSectFormats.size() - 1 )
            {
                if ( aNextIdx.GetIndex() != nContentEnd )
                    nShownSections++;  //document does not end in a section
            }
            else if ( !aNextIdx.GetNode().IsSectionNode() )
                    nShownSections++; //section is not immediately followed by another section
        }

        // count only visible sections
        if ( pSect && !pSect->CalcHiddenFlag())
            nShownSections++;
    }

    IDocumentRedlineAccess const& rIDRA(m_rDoc.getIDocumentRedlineAccess());
    std::unordered_map<SwSetExpFieldType const*, SwTextNode const*> SetExpOutlineNodeMap;

    for (std::unique_ptr<SetGetExpField> const& it : *mpUpdateFields->GetSortList())
    {
        SwSection* pSect = const_cast<SwSection*>(it->GetSection());
        if( pSect )
        {
            SwSbxValue aValue = aCalc.Calculate(
                                        pSect->GetCondition() );
            if(!aValue.IsVoidValue())
            {
                // Do we want to hide this one?
                bool bHide = aValue.GetBool();
                if (bHide && !pSect->IsCondHidden())
                {
                    // This section will be hidden, but it wasn't before
                    if (nShownSections == 1)
                    {
                        // This would be the last section, so set its condition to false, and avoid hiding it.
                        pSect->SetCondition(u"0"_ustr);
                        bHide = false;
                    }
                    nShownSections--;
                }
                pSect->SetCondHidden( bHide );
            }
            continue;
        }
        ::sw::mark::Bookmark *const pBookmark(
                const_cast<::sw::mark::Bookmark *>(it->GetBookmark()));
        if (pBookmark)
        {
            SwSbxValue const aValue(aCalc.Calculate(pBookmark->GetHideCondition()));
            if (!aValue.IsVoidValue())
            {
                pBookmark->Hide(aValue.GetBool());
            }
            continue;
        }

        SwTextField* pTextField = const_cast<SwTextField*>(it->GetTextField());
        if( !pTextField )
        {
            OSL_ENSURE( false, "what's wrong now'" );
            continue;
        }

        if (pLayout && pLayout->IsHideRedlines()
            && IsFieldDeleted(rIDRA, *pLayout, *pTextField))
        {
            continue;
        }

        SwFormatField* pFormatField = const_cast<SwFormatField*>(&pTextField->GetFormatField());
        const SwField* pField = pFormatField->GetField();

        nWhich = pField->GetTyp()->Which();
        switch( nWhich )
        {
        case SwFieldIds::HiddenText:
        {
            SwHiddenTextField* pHField = const_cast<SwHiddenTextField*>(static_cast<const SwHiddenTextField*>(pField));
            SwSbxValue aValue = aCalc.Calculate( pHField->GetPar1() );
            bool bValue = !aValue.GetBool();
            if(!aValue.IsVoidValue())
            {
                pHField->SetValue( bValue );
                // evaluate field
                pHField->Evaluate(m_rDoc);
            }
        }
        break;
        case SwFieldIds::HiddenPara:
        {
            SwHiddenParaField* pHPField = const_cast<SwHiddenParaField*>(static_cast<const SwHiddenParaField*>(pField));
            SwSbxValue aValue = aCalc.Calculate( pHPField->GetPar1() );
            bool bValue = aValue.GetBool();
            if(!aValue.IsVoidValue())
                pHPField->SetHidden( bValue );
        }
        break;
        case SwFieldIds::DbSetNumber:
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
        {
            const_cast<SwDBSetNumberField*>(static_cast<const SwDBSetNumberField*>(pField))->Evaluate(m_rDoc);
            aCalc.VarChange( sDBNumNm, static_cast<const SwDBSetNumberField*>(pField)->GetSetNumber());
            pField->ExpandField(m_rDoc.IsClipBoard(), nullptr);
        }
#endif
        break;
        case SwFieldIds::DbNextSet:
        case SwFieldIds::DbNumSet:
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
        {
            UpdateDBNumFields( *const_cast<SwDBNameInfField*>(static_cast<const SwDBNameInfField*>(pField)), aCalc );
            if( bCanFill )
                bCanFill = pMgr->FillCalcWithMergeData( m_rDoc.GetNumberFormatter(), nLang, aCalc );
        }
#endif
        break;
        case SwFieldIds::Database:
        {
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
            // evaluate field
            const_cast<SwDBField*>(static_cast<const SwDBField*>(pField))->Evaluate();

            SwDBData aTmpDBData(static_cast<const SwDBField*>(pField)->GetDBData());

            if( pMgr->IsDataSourceOpen(aTmpDBData.sDataSource, aTmpDBData.sCommand, false))
                aCalc.VarChange( sDBNumNm, pMgr->GetSelectedRecordId(aTmpDBData.sDataSource, aTmpDBData.sCommand, aTmpDBData.nCommandType));

            const OUString aName = pField->GetTyp()->GetName().toString();

            // Add entry to hash table
            // Entry present?
            auto pFnd = aHashStrTable.find( aName );
            OUString const value(pField->ExpandField(m_rDoc.IsClipBoard(), nullptr));
            if( pFnd != aHashStrTable.end() )
            {
                // Modify entry in the hash table
                pFnd->second = value;
            }
            else
            {
                // insert new entry
               aHashStrTable.insert( { aName, value } );
            }
#endif
        }
        break;
        case SwFieldIds::GetExp:
        {
            SwGetExpField* pGField = const_cast<SwGetExpField*>(static_cast<const SwGetExpField*>(pField));
            if( SwGetSetExpType::String & pGField->GetSubType() )        // replace String
            {
                if( (!pUpdateField || pUpdateField == pTextField )
                    && pGField->IsInBodyText() )
                {
                    OUString aNew = LookString( aHashStrTable, pGField->GetFormula() );
                    pGField->ChgExpStr( aNew, pLayout );
                }
            }
            else            // recalculate formula
            {
                if( (!pUpdateField || pUpdateField == pTextField )
                    && pGField->IsInBodyText() )
                {
                    SwSbxValue aValue = aCalc.Calculate(
                                    pGField->GetFormula());
                    if(!aValue.IsVoidValue())
                        pGField->SetValue(aValue.GetDouble(), pLayout);
                }
            }
        }
        break;
        case SwFieldIds::SetExp:
        {
            SwSetExpField* pSField = const_cast<SwSetExpField*>(static_cast<const SwSetExpField*>(pField));
            if( SwGetSetExpType::String & pSField->GetSubType() )        // replace String
            {
                // is the "formula" a field?
                OUString aNew = LookString( aHashStrTable, pSField->GetFormula() );

                if( aNew.isEmpty() )               // nothing found then the formula is the new value
                    aNew = pSField->GetFormula();

                // only update one field
                if( !pUpdateField || pUpdateField == pTextField )
                    pSField->ChgExpStr( aNew, pLayout );

                // lookup the field's name
                aNew = static_cast<SwSetExpFieldType*>(pSField->GetTyp())->GetSetRefName().toString();
                // Entry present?
                auto pFnd = aHashStrTable.find( aNew );
                if( pFnd != aHashStrTable.end() )
                    // Modify entry in the hash table
                    pFnd->second = pSField->GetExpStr(pLayout);
                else
                    // insert new entry
                    pFnd = aHashStrTable.insert( { aNew, pSField->GetExpStr(pLayout) } ).first;

                // Extension for calculation with Strings
                SwSbxValue aValue;
                aValue.PutString( pFnd->second );
                aCalc.VarChange( aNew, aValue );
            }
            else            // recalculate formula
            {
                SwSetExpFieldType* pSFieldTyp = static_cast<SwSetExpFieldType*>(pField->GetTyp());
                OUString aNew = pSFieldTyp->GetName().toString();

                SwNode* pSeqNd = nullptr;

                if( pSField->IsSequenceField() )
                {
                    const sal_uInt8 nLvl = pSFieldTyp->GetOutlineLvl();
                    if( MAXLEVEL > nLvl )
                    {
                        // test if the Number needs to be updated
                        pSeqNd = m_rDoc.GetNodes()[ it->GetNode() ];

                        const SwTextNode* pOutlNd = pSeqNd->
                                FindOutlineNodeOfLevel(nLvl, pLayout);
                        auto const iter(SetExpOutlineNodeMap.find(pSFieldTyp));
                        if (iter == SetExpOutlineNodeMap.end()
                            || iter->second != pOutlNd)
                        {
                            SetExpOutlineNodeMap[pSFieldTyp] = pOutlNd;
                            aCalc.VarChange( aNew, 0 );
                        }
                    }
                }

                aNew += "=" + pSField->GetFormula();

                SwSbxValue aValue = aCalc.Calculate( aNew );
                if (!aCalc.IsCalcError())
                {
                    double nErg = aValue.GetDouble();
                    // only update one field
                    if( !aValue.IsVoidValue() && (!pUpdateField || pUpdateField == pTextField) )
                    {
                        pSField->SetValue(nErg, pLayout);

                        if( pSeqNd )
                            pSFieldTyp->SetChapter(*pSField, *pSeqNd, pLayout);
                    }
                }
            }
        }
        break;
        default: break;
        } // switch

        {
            // avoid calling ReplaceText() for input fields, it is pointless
            // here and moves the cursor if it's inside the field ...
            SwTextInputField *const pInputField(
                pUpdateField == pTextField // ... except once, when the dialog
                    ? nullptr // is used to change content via UpdateOneField()
                    : dynamic_cast<SwTextInputField *>(pTextField));
            if (pInputField)
            {
                bool const tmp = pInputField->LockNotifyContentChange();
                (void) tmp;
                assert(tmp && "should not be locked here?");
            }
            ::comphelper::ScopeGuard g([pInputField]()
                {
                    if (pInputField)
                    {
                        pInputField->UnlockNotifyContentChange();
                    }
                });
            pFormatField->ForceUpdateTextNode();
        }

        if (pUpdateField == pTextField) // if only this one is updated
        {
            if( SwFieldIds::GetExp == nWhich ||      // only GetField or
                SwFieldIds::HiddenText == nWhich ||   // HiddenText?
                SwFieldIds::HiddenPara == nWhich)    // HiddenParaField?
                break;                          // quit
            pUpdateField = nullptr;                       // update all from here on
        }
    }

#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
    pMgr->CloseAll(false);
#endif
}

/// Insert field type that was marked as deleted
void DocumentFieldsManager::UpdateUsrFields()
{
    SwCalc* pCalc = nullptr;
    for( SwFieldTypes::size_type i = INIT_FLDTYPES; i < mpFieldTypes->size(); ++i )
    {
        const SwFieldType* pFieldType = (*mpFieldTypes)[i].get();
        if( SwFieldIds::User == pFieldType->Which() )
        {
            if( !pCalc )
                pCalc = new SwCalc( m_rDoc );
            const_cast<SwUserFieldType*>(static_cast<const SwUserFieldType*>(pFieldType))->GetValue( *pCalc );
        }
    }

    if( pCalc )
    {
        delete pCalc;
        m_rDoc.getIDocumentState().SetModified();
    }
}

sal_Int32 DocumentFieldsManager::GetRecordsPerDocument() const
{
    sal_Int32 nRecords = 1;

    mpUpdateFields->MakeFieldList( m_rDoc, true, GETFLD_ALL );
    if (mpUpdateFields->GetSortList()->empty())
        return nRecords;

    for (std::unique_ptr<SetGetExpField> const& it : *mpUpdateFields->GetSortList())
    {
        const SwTextField *pTextField = it->GetTextField();
        if( !pTextField )
            continue;

        const SwFormatField &pFormatField = pTextField->GetFormatField();
        const SwField* pField = pFormatField.GetField();

        switch( pField->GetTyp()->Which() )
        {
        case SwFieldIds::DbNextSet:
        case SwFieldIds::DbNumSet:
            nRecords++;
            break;
        default:
            break;
        }
    }

    return nRecords;
}

void DocumentFieldsManager::UpdatePageFields(const SwTwips nDocPos)
{
    for(SwFieldTypes::size_type i = 0; i < INIT_FLDTYPES; ++i)
    {
        SwFieldType* pFieldType = (*mpFieldTypes)[i].get();
        switch(pFieldType->Which())
        {
        case SwFieldIds::PageNumber:
        case SwFieldIds::Chapter:
        case SwFieldIds::GetExp:
        case SwFieldIds::RefPageGet:
            pFieldType->UpdateDocPos(nDocPos);
            break;
        case SwFieldIds::DocStat:
        {
            pFieldType->CallSwClientNotify(sw::LegacyModifyHint(nullptr, nullptr));
            SwRootFrame const* pLayout(nullptr);
            for (SwRootFrame const*const pLay : m_rDoc.GetAllLayouts())
            {
                if (!pLay->IsHideRedlines())
                    pLayout = pLay;
            }
            static_cast<SwDocStatFieldType*>(pFieldType)->UpdateRangeFields(pLayout);
        }
        break;
        case SwFieldIds::GetRef:
            static_cast<SwGetRefFieldType*>(pFieldType)->UpdateStyleReferences();
            // Style references can vary across different pages (e.g. in header/footer)
            // so they must be updated when page fields are
            break;
        default: break;
        }
    }
    SetNewFieldLst(true);
}

void DocumentFieldsManager::LockExpFields()
{
    ++mnLockExpField;
}

void DocumentFieldsManager::UnlockExpFields()
{
    assert(mnLockExpField != 0);
    if( mnLockExpField )
        --mnLockExpField;
}

bool DocumentFieldsManager::IsExpFieldsLocked() const
{
    return 0 != mnLockExpField;
}

SwDocUpdateField& DocumentFieldsManager::GetUpdateFields() const
{
    return *mpUpdateFields;
}

bool DocumentFieldsManager::SetFieldsDirty( bool b, const SwNode* pChk, SwNodeOffset nLen )
{
    // See if the supplied nodes actually contain fields.
    // If they don't, the flag doesn't need to be changed.
    bool bFieldsFnd = false;
    if( b && pChk && !GetUpdateFields().IsFieldsDirty() && !m_rDoc.IsInDtor()
        // ?? what's up with Undo, this is also wanted there!
        /*&& &pChk->GetNodes() == &GetNodes()*/ )
    {
        b = false;
        if( !nLen )
            ++nLen;
        SwNodeOffset nStt = pChk->GetIndex();
        const SwNodes& rNds = pChk->GetNodes();
        while( nLen-- )
        {
            const SwTextNode* pTNd = rNds[ nStt++ ]->GetTextNode();
            if( pTNd )
            {
                if( pTNd->GetAttrOutlineLevel() != 0 )
                    // update chapter fields
                    b = true;
                else if( pTNd->GetpSwpHints() && pTNd->GetSwpHints().Count() )
                {
                    const size_t nEnd = pTNd->GetSwpHints().Count();
                    for( size_t n = 0 ; n < nEnd; ++n )
                    {
                        const SwTextAttr* pAttr = pTNd->GetSwpHints().Get(n);
                        if (   pAttr->Which() == RES_TXTATR_FIELD
                            || pAttr->Which() == RES_TXTATR_INPUTFIELD)
                        {
                            b = true;
                            break;
                        }
                    }
                }

                if( b )
                    break;
            }
        }
        bFieldsFnd = b;
    }
    GetUpdateFields().SetFieldsDirty( b );
    return bFieldsFnd;
}

void DocumentFieldsManager::SetFixFields( const DateTime* pNewDateTime )
{
    bool bIsModified = m_rDoc.getIDocumentState().IsModified();

    sal_Int32 nDate;
    sal_Int64 nTime;
    if( pNewDateTime )
    {
        nDate = pNewDateTime->GetDate();
        nTime = pNewDateTime->GetTime();
    }
    else
    {
        DateTime aDateTime( DateTime::SYSTEM );
        nDate = aDateTime.GetDate();
        nTime = aDateTime.GetTime();
    }

    SwFieldIds const aTypes[] {
        /*0*/   SwFieldIds::DocInfo,
        /*1*/   SwFieldIds::Author,
        /*2*/   SwFieldIds::ExtUser,
        /*3*/   SwFieldIds::Filename,
        /*4*/   SwFieldIds::DateTime };  // MUST be at the end!

    for(SwFieldIds aType : aTypes)
    {
        std::vector<SwFormatField*> vFields;
        GetSysFieldType(aType)->GatherFields(vFields);
        for(auto pFormatField: vFields)
        {
            if (pFormatField->GetTextField())
            {
                bool bChgd = false;
                switch( aType )
                {
                case SwFieldIds::DocInfo:
                    if( static_cast<SwDocInfoField*>(pFormatField->GetField())->IsFixed() )
                    {
                        bChgd = true;
                        SwDocInfoField* pDocInfField = static_cast<SwDocInfoField*>(pFormatField->GetField());
                        pDocInfField->SetExpansion( static_cast<SwDocInfoFieldType*>(
                                    pDocInfField->GetTyp())->Expand(
                                        pDocInfField->GetSubType(),
                                        pDocInfField->GetFormat(),
                                        pDocInfField->GetLanguage(),
                                        pDocInfField->GetName() ) );
                    }
                    break;

                case SwFieldIds::Author:
                    if( static_cast<SwAuthorField*>(pFormatField->GetField())->IsFixed() )
                    {
                        bChgd = true;
                        SwAuthorField* pAuthorField = static_cast<SwAuthorField*>(pFormatField->GetField());
                        pAuthorField->SetExpansion( SwAuthorFieldType::Expand( pAuthorField->GetFormat() ) );
                    }
                    break;

                case SwFieldIds::ExtUser:
                    if( static_cast<SwExtUserField*>(pFormatField->GetField())->IsFixed() )
                    {
                        bChgd = true;
                        SwExtUserField* pExtUserField = static_cast<SwExtUserField*>(pFormatField->GetField());
                        pExtUserField->SetExpansion( SwExtUserFieldType::Expand(pExtUserField->GetSubType()) );
                    }
                    break;

                case SwFieldIds::DateTime:
                    if( static_cast<SwDateTimeField*>(pFormatField->GetField())->IsFixed() )
                    {
                        bChgd = true;
                        static_cast<SwDateTimeField*>(pFormatField->GetField())->SetDateTime(
                                                    DateTime(Date(nDate), tools::Time::fromEncodedTime(nTime)) );
                    }
                    break;

                case SwFieldIds::Filename:
                    {
                        SwFileNameField* pFileNameField =
                                static_cast<SwFileNameField*>(pFormatField->GetField());
                        if( pFileNameField->IsFixed() )
                        {
                            bChgd = true;
                            pFileNameField->SetExpansion( static_cast<SwFileNameFieldType*>(
                                        pFileNameField->GetTyp())->Expand(
                                                pFileNameField->GetFormat() ) );
                        }
                    }
                    break;
                default: break;
                }

                // Trigger formatting
                if( bChgd )
                    pFormatField->ForceUpdateTextNode();
            }
        }
    }

    if( !bIsModified )
        m_rDoc.getIDocumentState().ResetModified();
}

void DocumentFieldsManager::FieldsToCalc(SwCalc& rCalc,
        const SetGetExpField& rToThisField, SwRootFrame const*const pLayout)
{
    // create the sorted list of all SetFields
    mpUpdateFields->MakeFieldList( m_rDoc, mbNewFieldLst, GETFLD_CALC );
    mbNewFieldLst = false;

#if !HAVE_FEATURE_DBCONNECTIVITY || ENABLE_FUZZERS
    SwDBManager* pMgr = NULL;
#else
    SwDBManager* pMgr = m_rDoc.GetDBManager();
    pMgr->CloseAll(false);
#endif

    if (!mpUpdateFields->GetSortList()->empty())
    {
        SetGetExpFields::const_iterator const itLast =
            mpUpdateFields->GetSortList()->upper_bound(
                &rToThisField);
        for (auto it = mpUpdateFields->GetSortList()->begin(); it != itLast; ++it)
        {
            lcl_CalcField(m_rDoc, rCalc, **it, pMgr, pLayout);
        }
    }
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
    pMgr->CloseAll(false);
#endif
}

void DocumentFieldsManager::FieldsToCalc(SwCalc& rCalc,
        SwNodeOffset const nLastNd, sal_Int32 const nLastCnt)
{
    // create the sorted list of all SetFields
    mpUpdateFields->MakeFieldList( m_rDoc, mbNewFieldLst, GETFLD_CALC );
    mbNewFieldLst = false;

#if !HAVE_FEATURE_DBCONNECTIVITY || ENABLE_FUZZERS
    SwDBManager* pMgr = NULL;
#else
    SwDBManager* pMgr = m_rDoc.GetDBManager();
    pMgr->CloseAll(false);
#endif

    SwRootFrame const* pLayout(nullptr);
    SwRootFrame const* pLayoutRLHidden(nullptr);
    for (SwRootFrame const*const pLay : m_rDoc.GetAllLayouts())
    {
        if (pLay->IsHideRedlines())
        {
            pLayoutRLHidden = pLay;
        }
        else
        {
            pLayout = pLay;
        }
    }

    // note this is not duplicate of the other FieldsToCalc because there is
    // (currently) no SetGetExpField that compares only a position
    for(auto it = mpUpdateFields->GetSortList()->begin();
        it != mpUpdateFields->GetSortList()->end() &&
        ( (*it)->GetNode() < nLastNd ||
          ( (*it)->GetNode() == nLastNd && (*it)->GetContent() <= nLastCnt )
        );
        ++it )
    {
        if (pLayout || !pLayoutRLHidden) // always calc *something*...
        {
            lcl_CalcField( m_rDoc, rCalc, **it, pMgr, pLayout );
        }
        if (pLayoutRLHidden)
        {
            lcl_CalcField( m_rDoc, rCalc, **it, pMgr, pLayoutRLHidden );
        }
    }

#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
    pMgr->CloseAll(false);
#endif
}

void DocumentFieldsManager::FieldsToExpand( std::unordered_map<OUString, OUString> & rHashTable,
        const SetGetExpField& rToThisField, SwRootFrame const& rLayout)
{
    // create the sorted list of all SetFields
    mpUpdateFields->MakeFieldList( m_rDoc, mbNewFieldLst, GETFLD_EXPAND );
    mbNewFieldLst = false;

    IDocumentRedlineAccess const& rIDRA(m_rDoc.getIDocumentRedlineAccess());

    SetGetExpFields::const_iterator const itLast =
        mpUpdateFields->GetSortList()->upper_bound(&rToThisField);

    for (auto it = mpUpdateFields->GetSortList()->begin(); it != itLast; ++it)
    {
        const SwTextField* pTextField = (*it)->GetTextField();
        if( !pTextField )
            continue;

        if (rLayout.IsHideRedlines()
            && IsFieldDeleted(rIDRA, rLayout, *pTextField))
        {
            continue;
        }

        const SwField* pField = pTextField->GetFormatField().GetField();
        switch( pField->GetTyp()->Which() )
        {
        case SwFieldIds::SetExp:
            {
                SwSetExpField* pSField = const_cast<SwSetExpField*>(static_cast<const SwSetExpField*>(pField));
                if( SwGetSetExpType::String & pSField->GetSubType() )
                {
                    // set the new value in the hash table
                    // is the formula a field?
                    OUString aNew = LookString( rHashTable, pSField->GetFormula() );

                    if( aNew.isEmpty() )               // nothing found, then the formula is
                        aNew = pSField->GetFormula(); // the new value

                    // #i3141# - update expression of field as in method
                    // <SwDoc::UpdateExpFields(..)> for string/text fields
                    pSField->ChgExpStr(aNew, &rLayout);

                    // look up the field's name
                    aNew = static_cast<SwSetExpFieldType*>(pSField->GetTyp())->GetSetRefName().toString();
                    // Entry present?
                    auto pFnd = rHashTable.find( aNew );
                    if( pFnd != rHashTable.end() )
                        // modify entry in the hash table
                        pFnd->second = pSField->GetExpStr(&rLayout);
                    else
                        // insert the new entry
                        rHashTable.insert( { aNew, pSField->GetExpStr(&rLayout) } );
                }
            }
            break;
        case SwFieldIds::Database:
            {
                const OUString aName = pField->GetTyp()->GetName().toString();

                // Insert entry in the hash table
                // Entry present?
                auto pFnd = rHashTable.find( aName );
                OUString const value(pField->ExpandField(m_rDoc.IsClipBoard(), nullptr));
                if( pFnd != rHashTable.end() )
                    // modify entry in the hash table
                    pFnd->second = value;
                else
                    // insert the new entry
                    rHashTable.insert( { aName, value } );
            }
            break;
        default: break;
        }
    }
}


bool DocumentFieldsManager::IsNewFieldLst() const
{
    return mbNewFieldLst;
}

void DocumentFieldsManager::SetNewFieldLst(bool bFlag)
{
    mbNewFieldLst = bFlag;
}

void DocumentFieldsManager::InsDelFieldInFieldLst( bool bIns, const SwTextField& rField )
{
    if (!mbNewFieldLst && !m_rDoc.IsInDtor())
        mpUpdateFields->InsDelFieldInFieldLst( bIns, rField );
}

SwField * DocumentFieldsManager::GetFieldAtPos(const SwPosition & rPos)
{
    SwTextField * const pAttr = GetTextFieldAtPos(rPos);

    return pAttr ? const_cast<SwField *>( pAttr->GetFormatField().GetField() ) : nullptr;
}

SwTextField * DocumentFieldsManager::GetTextFieldAtPos(const SwPosition & rPos)
{
    SwTextNode * const pNode = rPos.GetNode().GetTextNode();

    return (pNode != nullptr)
        ? pNode->GetFieldTextAttrAt(rPos.GetContentIndex(), ::sw::GetTextAttrMode::Default)
        : nullptr;
}

/// @note For simplicity assume that all field types have updatable contents so
///       optimization currently only available when no fields exist.
bool DocumentFieldsManager::containsUpdatableFields()
{
    std::vector<SwFormatField*> vFields;
    for (auto const& pFieldType: *mpFieldTypes)
    {
        pFieldType->GatherFields(vFields);
        if(vFields.size()>0)
            return true;
    }
    return false;
}

/// Remove all unreferenced field types of a document
void DocumentFieldsManager::GCFieldTypes()
{
    for( auto n = mpFieldTypes->size(); n > INIT_FLDTYPES; )
        if( !(*mpFieldTypes)[ --n ]->HasWriterListeners() )
            RemoveFieldType( n );
}

void DocumentFieldsManager::InitFieldTypes()       // is being called by the CTOR
{
    // Field types
    mpFieldTypes->emplace_back( new SwDateTimeFieldType(&m_rDoc) );
    mpFieldTypes->emplace_back( new SwChapterFieldType );
    mpFieldTypes->emplace_back( new SwPageNumberFieldType );
    mpFieldTypes->emplace_back( new SwAuthorFieldType );
    mpFieldTypes->emplace_back( new SwFileNameFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwDBNameFieldType(&m_rDoc) );
    mpFieldTypes->emplace_back( new SwGetExpFieldType(&m_rDoc) );
    mpFieldTypes->emplace_back( new SwGetRefFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwHiddenTextFieldType );
    mpFieldTypes->emplace_back( new SwPostItFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwDocStatFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwDocInfoFieldType(&m_rDoc) );
    mpFieldTypes->emplace_back( new SwInputFieldType( &m_rDoc ) );
    mpFieldTypes->emplace_back( new SwTableFieldType( &m_rDoc ) );
    mpFieldTypes->emplace_back( new SwMacroFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwHiddenParaFieldType );
    mpFieldTypes->emplace_back( new SwDBNextSetFieldType );
    mpFieldTypes->emplace_back( new SwDBNumSetFieldType );
    mpFieldTypes->emplace_back( new SwDBSetNumberFieldType );
    mpFieldTypes->emplace_back( new SwTemplNameFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwTemplNameFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwExtUserFieldType );
    mpFieldTypes->emplace_back( new SwRefPageSetFieldType );
    mpFieldTypes->emplace_back( new SwRefPageGetFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwJumpEditFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwScriptFieldType(m_rDoc) );
    mpFieldTypes->emplace_back( new SwCombinedCharFieldType );
    mpFieldTypes->emplace_back( new SwDropDownFieldType );

    // Types have to be at the end!
    // We expect this in the InsertFieldType!
    // MIB 14.04.95: In Sw3StringPool::Setup (sw3imp.cxx) and
    //               lcl_sw3io_InSetExpField (sw3field.cxx) now also
    mpFieldTypes->emplace_back( new SwSetExpFieldType(&m_rDoc,
                UIName(SwResId(STR_POOLCOLL_LABEL_ABB)), SwGetSetExpType::Sequence) );
    mpFieldTypes->emplace_back( new SwSetExpFieldType(&m_rDoc,
                UIName(SwResId(STR_POOLCOLL_LABEL_TABLE)), SwGetSetExpType::Sequence) );
    mpFieldTypes->emplace_back( new SwSetExpFieldType(&m_rDoc,
                UIName(SwResId(STR_POOLCOLL_LABEL_FRAME)), SwGetSetExpType::Sequence) );
    mpFieldTypes->emplace_back( new SwSetExpFieldType(&m_rDoc,
                UIName(SwResId(STR_POOLCOLL_LABEL_DRAWING)), SwGetSetExpType::Sequence) );
    mpFieldTypes->emplace_back( new SwSetExpFieldType(&m_rDoc,
                UIName(SwResId(STR_POOLCOLL_LABEL_FIGURE)), SwGetSetExpType::Sequence) );

    assert( mpFieldTypes->size() == INIT_FLDTYPES );
}

void DocumentFieldsManager::ClearFieldTypes()
{
    mpFieldTypes->erase( mpFieldTypes->begin() + INIT_FLDTYPES, mpFieldTypes->end() );
}

void DocumentFieldsManager::UpdateDBNumFields( SwDBNameInfField& rDBField, SwCalc& rCalc )
{
#if !HAVE_FEATURE_DBCONNECTIVITY || ENABLE_FUZZERS
    (void) rDBField;
    (void) rCalc;
#else
    SwDBManager* pMgr = m_rDoc.GetDBManager();

    SwFieldIds nFieldType = rDBField.Which();

    bool bPar1 = rCalc.Calculate( rDBField.GetPar1() ).GetBool();

    if( SwFieldIds::DbNextSet == nFieldType )
        static_cast<SwDBNextSetField&>(rDBField).SetCondValid( bPar1 );
    else
        static_cast<SwDBNumSetField&>(rDBField).SetCondValid( bPar1 );

    if( !rDBField.GetRealDBData().sDataSource.isEmpty() )
    {
        // Edit a certain database
        if( SwFieldIds::DbNextSet == nFieldType )
            static_cast<SwDBNextSetField&>(rDBField).Evaluate(m_rDoc);
        else
            static_cast<SwDBNumSetField&>(rDBField).Evaluate(m_rDoc);

        SwDBData aTmpDBData( rDBField.GetDBData(&m_rDoc) );

        if( pMgr->OpenDataSource( aTmpDBData.sDataSource, aTmpDBData.sCommand ))
            rCalc.VarChange( lcl_GetDBVarName( m_rDoc, rDBField),
                        pMgr->GetSelectedRecordId(aTmpDBData.sDataSource, aTmpDBData.sCommand, aTmpDBData.nCommandType) );
    }
    else
    {
        OSL_FAIL("TODO: what should happen with unnamed DBFields?");
    }
#endif
}

DocumentFieldsManager::~DocumentFieldsManager()
{
    mpUpdateFields.reset();
    mpFieldTypes.reset();
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
