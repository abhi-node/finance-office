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

#include <vcl/sysdata.hxx>

#include <X11/Xlib.h>

#include <unx/saldisp.hxx>
#include <unx/salinst.h>
#include <unx/salgdi.h>
#include <unx/salvd.h>
#include <unx/x11/xlimits.hxx>

#include <config_features.h>
#include <vcl/skia/SkiaHelper.hxx>
#if HAVE_FEATURE_SKIA
#include <skia/x11/salvd.hxx>
#endif
#include <cairo-xlib.h>

std::unique_ptr<SalVirtualDevice> X11SalInstance::CreateX11VirtualDevice(const SalGraphics& rGraphics,
        tools::Long nDX, tools::Long nDY, DeviceFormat eFormat,
        std::unique_ptr<X11SalGraphics> pNewGraphics, bool bAlphaMaskTransparent)
{
    assert(!bAlphaMaskTransparent && "TODO"); (void)bAlphaMaskTransparent;
    assert(pNewGraphics);
#if HAVE_FEATURE_SKIA
    if (SkiaHelper::isVCLSkiaEnabled())
        return std::unique_ptr<SalVirtualDevice>(new X11SkiaSalVirtualDevice(rGraphics, nDX, nDY, std::move(pNewGraphics)));
    else
#endif
        return std::unique_ptr<SalVirtualDevice>(new X11SalVirtualDevice(rGraphics, nDX, nDY, eFormat, std::move(pNewGraphics)));
}

std::unique_ptr<SalVirtualDevice> X11SalInstance::CreateX11VirtualDevice(const SalGraphics& rGraphics,
        tools::Long &nDX, tools::Long &nDY, DeviceFormat eFormat, const SystemGraphicsData& rData,
        std::unique_ptr<X11SalGraphics> pNewGraphics)
{
    assert(pNewGraphics);
#if HAVE_FEATURE_SKIA
    if (SkiaHelper::isVCLSkiaEnabled())
        return std::unique_ptr<SalVirtualDevice>(new X11SkiaSalVirtualDevice(rGraphics, nDX, nDY, rData, std::move(pNewGraphics)));
    else
#endif
        return std::unique_ptr<SalVirtualDevice>(new X11SalVirtualDevice(rGraphics, nDX, nDY, eFormat, rData, std::move(pNewGraphics)));
}

std::unique_ptr<SalVirtualDevice> X11SalInstance::CreateVirtualDevice(SalGraphics& rGraphics,
        tools::Long nDX, tools::Long nDY, DeviceFormat eFormat, bool bAlphaMaskTransparent)
{
    return CreateX11VirtualDevice(rGraphics, nDX, nDY, eFormat, std::make_unique<X11SalGraphics>(), bAlphaMaskTransparent);
}

std::unique_ptr<SalVirtualDevice> X11SalInstance::CreateVirtualDevice(SalGraphics& rGraphics,
        tools::Long &nDX, tools::Long &nDY, DeviceFormat eFormat, const SystemGraphicsData& rData)
{
    return CreateX11VirtualDevice(rGraphics, nDX, nDY, eFormat, rData, std::make_unique<X11SalGraphics>());
}

void X11SalGraphics::Init(X11SalVirtualDevice *pDevice, SalColormap* pColormap, bool bDeleteColormap)
{
    SalDisplay *pDisplay  = pDevice->GetDisplay();
    m_nXScreen = pDevice->GetXScreenNumber();

    int nVisualDepth = pDisplay->GetColormap( m_nXScreen ).GetVisual().GetDepth();
    int nDeviceDepth = pDevice->GetDepth();

    if( pColormap )
    {
        maX11Common.m_pColormap = pColormap;
        if( bDeleteColormap )
            m_pDeleteColormap.reset(pColormap);
    }
    else if( nDeviceDepth == nVisualDepth )
        maX11Common.m_pColormap = &pDisplay->GetColormap( m_nXScreen );
    else if( nDeviceDepth == 1 )
    {
        m_pDeleteColormap.reset(new SalColormap());
        maX11Common.m_pColormap = m_pDeleteColormap.get();
    }

    m_pVDev      = pDevice;
    m_pFrame     = nullptr;

    SetDrawable(pDevice->GetDrawable(), pDevice->GetSurface(), m_nXScreen);
    mxImpl->UpdateX11GeometryProvider();
}

X11SalVirtualDevice::X11SalVirtualDevice(const SalGraphics& rGraphics, tools::Long nDX, tools::Long nDY,
                                         DeviceFormat /*eFormat*/,
                                         std::unique_ptr<X11SalGraphics> pNewGraphics) :
    pGraphics_(std::move(pNewGraphics)),
    m_nXScreen(0),
    bGraphics_(false)
{
    SalColormap* pColormap = nullptr;
    bool bDeleteColormap = false;

    sal_uInt16 nBitCount = rGraphics.GetBitCount();
    pDisplay_               = vcl_sal::getSalDisplay(GetGenericUnixSalData());
    nDepth_                 = nBitCount;

    nDX_ = nDX;
    nDY_ = nDY;
    m_nXScreen = static_cast<const X11SalGraphics&>(rGraphics).GetScreenNumber();
    hDrawable_ = limitXCreatePixmap( GetXDisplay(),
                                     pDisplay_->GetDrawable( m_nXScreen ),
                                     nDX_, nDY_,
                                     GetDepth() );
    bExternPixmap_ = false;

    if( nBitCount != pDisplay_->GetVisual( m_nXScreen ).GetDepth() )
    {
        pColormap = new SalColormap( nBitCount );
        bDeleteColormap = true;
    }

    pGraphics_->SetLayout( SalLayoutFlags::NONE ); // by default no! mirroring for VirtualDevices, can be enabled with EnableRTL()

    // tdf#127529 see SvpSalInstance::CreateVirtualDevice for the rare case of a non-null pPreExistingTarget
    m_bOwnsSurface = true;
    m_pSurface = cairo_xlib_surface_create(GetXDisplay(), hDrawable_,
                                           pDisplay_->GetColormap(m_nXScreen).GetVisual().visual,
                                           nDX_, nDY_);

    pGraphics_->Init(this, pColormap, bDeleteColormap);
}

X11SalVirtualDevice::X11SalVirtualDevice(const SalGraphics& rGraphics, tools::Long &nDX, tools::Long &nDY,
                                         DeviceFormat /*eFormat*/, const SystemGraphicsData& rData,
                                         std::unique_ptr<X11SalGraphics> pNewGraphics) :
    pGraphics_(std::move(pNewGraphics)),
    m_nXScreen(0),
    bGraphics_(false)
{
    SalColormap* pColormap = nullptr;
    bool bDeleteColormap = false;

    sal_uInt16 nBitCount = rGraphics.GetBitCount();
    pDisplay_               = vcl_sal::getSalDisplay(GetGenericUnixSalData());
    nDepth_                 = nBitCount;

    assert(rData.hDrawable != None);

    ::Window aRoot;
    int x, y;
    unsigned int w = 0, h = 0, bw, d;
    Display* pDisp = pDisplay_->GetDisplay();
    XGetGeometry( pDisp, rData.hDrawable,
                  &aRoot, &x, &y, &w, &h, &bw, &d );
    int nScreen = 0;
    while( nScreen < ScreenCount( pDisp ) )
    {
        if( RootWindow( pDisp, nScreen ) == aRoot )
            break;
        nScreen++;
    }
    nDX_ = static_cast<tools::Long>(w);
    nDY_ = static_cast<tools::Long>(h);
    nDX = nDX_;
    nDY = nDY_;
    m_nXScreen = SalX11Screen( nScreen );
    hDrawable_ = rData.hDrawable;
    bExternPixmap_ = true;

    if( nBitCount != pDisplay_->GetVisual( m_nXScreen ).GetDepth() )
    {
        pColormap = new SalColormap( nBitCount );
        bDeleteColormap = true;
    }

    pGraphics_->SetLayout( SalLayoutFlags::NONE ); // by default no! mirroring for VirtualDevices, can be enabled with EnableRTL()

    // tdf#127529 see SvpSalInstance::CreateVirtualDevice for the rare case of a non-null pPreExistingTarget
    cairo_surface_t* pPreExistingTarget = static_cast<cairo_surface_t*>(rData.pSurface);
    if (pPreExistingTarget)
    {
        m_bOwnsSurface = false;
        m_pSurface = pPreExistingTarget;
    }
    else
    {
        m_bOwnsSurface = true;
        m_pSurface = cairo_xlib_surface_create(GetXDisplay(), hDrawable_,
                                               pDisplay_->GetColormap(m_nXScreen).GetVisual().visual,
                                               nDX_, nDY_);
    }

    pGraphics_->Init(this, pColormap, bDeleteColormap);
}

X11SalVirtualDevice::~X11SalVirtualDevice()
{
    pGraphics_.reset();

    if (m_bOwnsSurface)
        cairo_surface_destroy(m_pSurface);

    if( GetDrawable() && !bExternPixmap_ )
        XFreePixmap( GetXDisplay(), GetDrawable() );
}

SalGraphics* X11SalVirtualDevice::AcquireGraphics()
{
    if( bGraphics_ )
        return nullptr;

    if( pGraphics_ )
        bGraphics_ = true;

    return pGraphics_.get();
}

void X11SalVirtualDevice::ReleaseGraphics( SalGraphics* )
{ bGraphics_ = false; }

bool X11SalVirtualDevice::SetSize( tools::Long nDX, tools::Long nDY, bool bAlphaMaskTransparent )
{
    assert(!bAlphaMaskTransparent && "TODO"); (void)bAlphaMaskTransparent;
    if( bExternPixmap_ )
        return false;

    if( !nDX ) nDX = 1;
    if( !nDY ) nDY = 1;

    if (m_bOwnsSurface)
        cairo_surface_destroy(m_pSurface);

    Pixmap h = limitXCreatePixmap( GetXDisplay(),
                              pDisplay_->GetDrawable( m_nXScreen ),
                              nDX, nDY, nDepth_ );

    if( !h )
    {
        if( !GetDrawable() )
        {
            hDrawable_ = limitXCreatePixmap( GetXDisplay(),
                                        pDisplay_->GetDrawable( m_nXScreen ),
                                        1, 1, nDepth_ );
            nDX_ = 1;
            nDY_ = 1;
        }

        if (m_bOwnsSurface)
        {
            m_pSurface = cairo_xlib_surface_create(GetXDisplay(), hDrawable_,
                                                   pDisplay_->GetColormap(m_nXScreen).GetVisual().visual,
                                                   nDX_, nDY_);
        }

        return false;
    }

    if( GetDrawable() )
        XFreePixmap( GetXDisplay(), GetDrawable() );
    hDrawable_ = h;

    nDX_ = nDX;
    nDY_ = nDY;

    if (m_bOwnsSurface)
    {
        m_pSurface = cairo_xlib_surface_create(GetXDisplay(), hDrawable_,
                                               pDisplay_->GetColormap(m_nXScreen).GetVisual().visual,
                                               nDX_, nDY_);
    }

    if( pGraphics_ )
        pGraphics_->Init( this );

    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
