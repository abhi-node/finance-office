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

#include <controller/SlsSelectionManager.hxx>

#include <SlideSorter.hxx>
#include <controller/SlideSorterController.hxx>
#include <controller/SlsCurrentSlideManager.hxx>
#include <controller/SlsFocusManager.hxx>
#include <controller/SlsPageSelector.hxx>
#include <controller/SlsSelectionObserver.hxx>
#include <model/SlideSorterModel.hxx>
#include <model/SlsPageEnumerationProvider.hxx>
#include <model/SlsPageDescriptor.hxx>
#include <view/SlideSorterView.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <drawdoc.hxx>
#include <sdpage.hxx>
#include <unomodel.hxx>
#include <drawview.hxx>
#include <DrawViewShell.hxx>
#include <ViewShellBase.hxx>
#include <svx/svxids.hrc>
#include <com/sun/star/drawing/XMasterPagesSupplier.hpp>
#include <com/sun/star/drawing/XDrawPagesSupplier.hpp>


#include <sdresid.hxx>
#include <strings.hrc>
#include <app.hrc>

using namespace ::com::sun::star;
using namespace ::com::sun::star::drawing;
using namespace ::com::sun::star::uno;
using namespace ::sd::slidesorter::model;
using namespace ::sd::slidesorter::view;
using namespace ::sd::slidesorter::controller;

namespace sd::slidesorter::controller {

SelectionManager::SelectionManager (SlideSorter& rSlideSorter)
    : mrSlideSorter(rSlideSorter),
      mrController(rSlideSorter.GetController()),
      mnInsertionPosition(-1),
      mpSelectionObserver(std::make_shared<SelectionObserver>(rSlideSorter))
{
}

SelectionManager::~SelectionManager()
{
}

void SelectionManager::DeleteSelectedPages (const bool bSelectFollowingPage)
{
    // Create some locks to prevent updates of the model, view, selection
    // state while modifying any of them.
    SlideSorterController::ModelChangeLock aLock (mrController);
    SlideSorterView::DrawLock aDrawLock (mrSlideSorter);
    PageSelector::UpdateLock aSelectionLock (mrSlideSorter);

    // Hide focus.
    bool bIsFocusShowing = mrController.GetFocusManager().IsFocusShowing();
    if (bIsFocusShowing)
        mrController.GetFocusManager().ToggleFocus();

    // Store pointers to all selected page descriptors.  This is necessary
    // because the pages get deselected when the first one is deleted.
    model::PageEnumeration aPageEnumeration (
        PageEnumerationProvider::CreateSelectedPagesEnumeration(mrSlideSorter.GetModel()));
    ::std::vector<SdPage*> aSelectedPages;
    sal_Int32 nNewCurrentSlide (-1);
    while (aPageEnumeration.HasMoreElements())
    {
        SharedPageDescriptor pDescriptor (aPageEnumeration.GetNextElement());
        aSelectedPages.push_back(pDescriptor->GetPage());
        if (bSelectFollowingPage || nNewCurrentSlide<0)
            nNewCurrentSlide = pDescriptor->GetPageIndex();
    }
    if (aSelectedPages.empty())
        return;

    // Determine the slide to select (and thereby make the current slide)
    // after the deletion.
    if (bSelectFollowingPage)
        nNewCurrentSlide -= aSelectedPages.size() - 1;
    else
        --nNewCurrentSlide;

    ViewShell& rViewShell = mrSlideSorter.GetViewShell();
    const auto pDrawViewShell = std::dynamic_pointer_cast<sd::DrawViewShell>(rViewShell.GetViewShellBase().GetMainViewShell());
    const auto pDrawView = pDrawViewShell ? pDrawViewShell->GetDrawView() : nullptr;

    if (pDrawView)
        pDrawView->BlockPageOrderChangedHint(true);

    // Proper naming for the undo action
    OUString sUndoComment(SdResId(STR_UNDO_DELETEPAGES));
    if (mrSlideSorter.GetView().GetDoc().GetDocumentType() == DocumentType::Draw)
        sUndoComment = SdResId(STR_UNDO_DELETEPAGES_DRAW);

    // The actual deletion of the selected pages is done in one of two
    // helper functions.  They are specialized for normal respectively for
    // master pages.
    mrSlideSorter.GetView().BegUndo (sUndoComment);
    if (mrSlideSorter.GetModel().GetEditMode() == EditMode::Page)
        DeleteSelectedNormalPages(aSelectedPages);
    else
        DeleteSelectedMasterPages(aSelectedPages);
    mrSlideSorter.GetView().EndUndo ();

    mrController.HandleModelChange();
    aLock.Release();
    if (pDrawView)
    {
        assert(pDrawViewShell);
        pDrawView->BlockPageOrderChangedHint(false);
        pDrawViewShell->ResetActualPage();
    }

    // Show focus and move it to next valid location.
    if (bIsFocusShowing)
        mrController.GetFocusManager().ToggleFocus();

    // Set the new current slide.
    if (nNewCurrentSlide < 0)
        nNewCurrentSlide = 0;
    else if (nNewCurrentSlide >= mrSlideSorter.GetModel().GetPageCount())
        nNewCurrentSlide = mrSlideSorter.GetModel().GetPageCount()-1;
    mrController.GetPageSelector().CountSelectedPages();
    mrController.GetPageSelector().SelectPage(nNewCurrentSlide);
    mrController.GetFocusManager().SetFocusedPage(nNewCurrentSlide);
}

void SelectionManager::DeleteSelectedNormalPages (const ::std::vector<SdPage*>& rSelectedPages)
{
    // Prepare the deletion via the UNO API.
    OSL_ASSERT(mrSlideSorter.GetModel().GetEditMode() == EditMode::Page);

    try
    {
        rtl::Reference<SdXImpressDocument> xDrawPagesSupplier( mrSlideSorter.GetModel().GetDocument()->getUnoModel() );
        if (!xDrawPagesSupplier)
            return;
        Reference<drawing::XDrawPages> xPages( xDrawPagesSupplier->getDrawPages(), UNO_SET_THROW );

        // Iterate over all pages that were selected when this method was called
        // and delete the draw page the notes page. The iteration is done in
        // reverse order so that when one slide is not deleted (to avoid an
        // empty document) the remaining slide is the first one.
        ::std::vector<SdPage*>::const_reverse_iterator aI;
        for (aI=rSelectedPages.rbegin(); aI!=rSelectedPages.rend(); ++aI)
        {
            // Do not delete the last slide in the document.
            if (xPages->getCount() <= 1)
                break;

            const sal_uInt16 nPage (model::FromCoreIndex((*aI)->GetPageNum()));

            Reference< XDrawPage > xPage( xPages->getByIndex( nPage ), UNO_QUERY_THROW );
            xPages->remove(xPage);
        }
    }
    catch( Exception& )
    {
        TOOLS_WARN_EXCEPTION( "sd", "SelectionManager::DeleteSelectedNormalPages()");
    }
}

void SelectionManager::DeleteSelectedMasterPages (const ::std::vector<SdPage*>& rSelectedPages)
{
    // Prepare the deletion via the UNO API.
    OSL_ASSERT(mrSlideSorter.GetModel().GetEditMode() == EditMode::MasterPage);

    try
    {
        rtl::Reference<SdXImpressDocument> xDrawPagesSupplier( mrSlideSorter.GetModel().GetDocument()->getUnoModel() );
        if (!xDrawPagesSupplier)
            return;
        Reference<drawing::XDrawPages> xPages( xDrawPagesSupplier->getMasterPages(), UNO_SET_THROW );

        // Iterate over all pages that were selected when this method was called
        // and delete the draw page the notes page. The iteration is done in
        // reverse order so that when one slide is not deleted (to avoid an
        // empty document) the remaining slide is the first one.
        ::std::vector<SdPage*>::const_reverse_iterator aI;
        for (aI=rSelectedPages.rbegin(); aI!=rSelectedPages.rend(); ++aI)
        {
            // Do not delete the last slide in the document.
            if (xPages->getCount() <= 1)
                break;

            const sal_uInt16 nPage (model::FromCoreIndex((*aI)->GetPageNum()));

            Reference< XDrawPage > xPage( xPages->getByIndex( nPage ), UNO_QUERY_THROW );
            xPages->remove(xPage);
        }
    }
    catch( Exception& )
    {
        TOOLS_WARN_EXCEPTION( "sd", "SelectionManager::DeleteSelectedMasterPages()");
    }
}

void SelectionManager::SelectionHasChanged ()
{
    ViewShell& rViewShell = mrSlideSorter.GetViewShell();

    rViewShell.Invalidate (SID_EXPAND_PAGE);
    rViewShell.Invalidate (SID_SUMMARY_PAGE);
    rViewShell.Invalidate(SID_SHOW_SLIDE);
    rViewShell.Invalidate(SID_HIDE_SLIDE);
    rViewShell.Invalidate(SID_DELETE_PAGE);
    rViewShell.Invalidate(SID_DELETE_MASTER_PAGE);
    rViewShell.Invalidate(SID_ASSIGN_LAYOUT);

    // StatusBar
    rViewShell.Invalidate (SID_STATUS_PAGE);
    rViewShell.Invalidate (SID_STATUS_LAYOUT);
    rViewShell.Invalidate (SID_SCALE);

    SharedPageDescriptor pDescriptor(mrController.GetCurrentSlideManager().GetCurrentSlide());
    if (pDescriptor)
        rViewShell.UpdatePreview(pDescriptor->GetPage());

    // Tell the selection change listeners that the selection has changed.
    for (const auto& rLink : maSelectionChangeListeners)
    {
        rLink.Call(nullptr);
    }

    // Reset the insertion position: until set again it is calculated from
    // the current selection.
    mnInsertionPosition = -1;
}

void SelectionManager::AddSelectionChangeListener (const Link<LinkParamNone*,void>& rListener)
{
    if (::std::find (
        maSelectionChangeListeners.begin(),
        maSelectionChangeListeners.end(),
        rListener) == maSelectionChangeListeners.end())
    {
        maSelectionChangeListeners.push_back (rListener);
    }
}

void SelectionManager::RemoveSelectionChangeListener(const Link<LinkParamNone*,void>& rListener)
{
    maSelectionChangeListeners.erase (
        ::std::find (
            maSelectionChangeListeners.begin(),
            maSelectionChangeListeners.end(),
            rListener));
}

sal_Int32 SelectionManager::GetInsertionPosition() const
{
    sal_Int32 nInsertionPosition (mnInsertionPosition);
    if (nInsertionPosition < 0)
    {
        model::PageEnumeration aSelectedPages
            (model::PageEnumerationProvider::CreateSelectedPagesEnumeration(
                mrSlideSorter.GetModel()));
        // Initialize (for the case of an empty selection) with the position
        // at the end of the document.
        nInsertionPosition = mrSlideSorter.GetModel().GetPageCount();
        while (aSelectedPages.HasMoreElements())
        {
            const sal_Int32 nPosition (aSelectedPages.GetNextElement()->GetPage()->GetPageNum());
            // Convert *2+1 index to straight index (n-1)/2 after the page
            // (+1).
            nInsertionPosition = model::FromCoreIndex(nPosition) + 1;
        }

    }
    return nInsertionPosition;
}

void SelectionManager::SetInsertionPosition (const sal_Int32 nInsertionPosition)
{
    if (nInsertionPosition < 0)
        mnInsertionPosition = -1;
    else if (nInsertionPosition > mrSlideSorter.GetModel().GetPageCount())
    {
        // Assert but then ignore invalid values.
        OSL_ASSERT(nInsertionPosition<=mrSlideSorter.GetModel().GetPageCount());
        return;
    }
    else
        mnInsertionPosition = nInsertionPosition;
}

} // end of namespace ::sd::slidesorter::controller

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
