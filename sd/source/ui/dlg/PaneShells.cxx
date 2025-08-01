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

#include <PaneShells.hxx>

#include <PaneChildWindows.hxx>

#include <sfx2/msg.hxx>
#include <sfx2/objface.hxx>

namespace sd {

//===== LeftImpressPaneShell ==================================================

const SfxSlot aLeftImpressPaneShellSlots_Impl[] =
{
    { 0, SfxGroupId::NONE, SfxSlotMode::NONE, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, 0, SfxDisableFlags::NONE, u""_ustr }
};

SFX_IMPL_INTERFACE(LeftImpressPaneShell, SfxShell)

void LeftImpressPaneShell::InitInterface_Impl()
{
    GetStaticInterface()->RegisterChildWindow(::sd::LeftPaneImpressChildWindow::GetChildWindowId());
}


LeftImpressPaneShell::LeftImpressPaneShell()
{
    SetName(u"LeftImpressPane"_ustr);
}

LeftImpressPaneShell::~LeftImpressPaneShell()
{
}

//===== BottomImpressPaneShell ==================================================

const SfxSlot aBottomImpressPaneShellSlots_Impl[]
    = { { 0, SfxGroupId::NONE, SfxSlotMode::NONE, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
          0, SfxDisableFlags::NONE, u""_ustr } };

SFX_IMPL_INTERFACE(BottomImpressPaneShell, SfxShell)

void BottomImpressPaneShell::InitInterface_Impl()
{
    GetStaticInterface()->RegisterChildWindow(::sd::BottomPaneImpressChildWindow::GetChildWindowId());
}

BottomImpressPaneShell::BottomImpressPaneShell() { SetName(u"BottomImpressPane"_ustr); }

BottomImpressPaneShell::~BottomImpressPaneShell() {}

//===== LeftDrawPaneShell =====================================================

const SfxSlot aLeftDrawPaneShellSlots_Impl[] =
{
    { 0, SfxGroupId::NONE, SfxSlotMode::NONE, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, 0, SfxDisableFlags::NONE, u""_ustr }
};

SFX_IMPL_INTERFACE(LeftDrawPaneShell, SfxShell)

void LeftDrawPaneShell::InitInterface_Impl()
{
    GetStaticInterface()->RegisterChildWindow(::sd::LeftPaneDrawChildWindow::GetChildWindowId());
}


LeftDrawPaneShell::LeftDrawPaneShell()
{
    SetName(u"LeftDrawPane"_ustr);
}

LeftDrawPaneShell::~LeftDrawPaneShell()
{
}

} // end of namespace ::sd

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
