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

#ifndef INCLUDED_VCL_VIRDEV_HXX
#define INCLUDED_VCL_VIRDEV_HXX

#include <vcl/dllapi.h>
#include <vcl/outdev.hxx>
#include <vcl/salgtype.hxx>
#include <memory>

class SalVirtualDevice;
struct SystemGraphicsData;
typedef struct _cairo_surface cairo_surface_t;

class SAL_WARN_UNUSED VCL_DLLPUBLIC VirtualDevice : public OutputDevice
{
    friend class Application;
    friend class ::OutputDevice;
    friend class Printer;
    friend cairo_surface_t* get_underlying_cairo_surface(const VirtualDevice&);
public:
    // reference device modes for different compatibility levels
    enum class RefDevMode { NONE = 0,
                            Dpi600 = 1,      // 600 dpi
                            MSO1 = 3,
                            PDF1 = 4,
                            Custom = 5
                          };

private:
    std::unique_ptr<SalVirtualDevice> mpVirDev;
    VclPtr<VirtualDevice>  mpPrev;
    VclPtr<VirtualDevice>  mpNext;
    sal_uInt16          mnBitCount;
    bool                mbScreenComp;
    const DeviceFormat meFormatAndAlpha;
    RefDevMode          meRefDevMode;
    bool                mbForceZeroExtleadBug;

    SAL_DLLPRIVATE void ImplInitVirDev( const OutputDevice* pOutDev, tools::Long nDX, tools::Long nDY, const SystemGraphicsData *pData = nullptr );
    SAL_DLLPRIVATE bool InnerImplSetOutputSizePixel( const Size& rNewSize, bool bErase, bool bAlphaMaskTransparent );

    VirtualDevice (const VirtualDevice &) = delete;
    VirtualDevice & operator= (const VirtualDevice &) = delete;

    /** Used for alpha VDev, to set areas to opaque

        @since \#i32109#
     */
    SAL_DLLPRIVATE void ImplFillOpaqueRectangle( const tools::Rectangle& rRect );

protected:
    virtual bool AcquireGraphics() const override;
    virtual void ReleaseGraphics( bool bRelease = true ) override;

    /** Create a virtual device of size 1x1

        @param pCompDev
        The generated vdev will be compatible to this device.
        If it's the nullptr, it uses Application::GetDefaultDevice().

        @param eFormat
        Device format of the generated virtual device.

        @param eOutDevType
        This real virtual output device type.
     */
    explicit VirtualDevice(const OutputDevice* pCompDev, DeviceFormat eFormat,
                           OutDevType eOutDevType);

public:

    /** Create a virtual device of size 1x1

        @param eFormat
        Device format of the generated virtual device.
     */
    explicit VirtualDevice(DeviceFormat eFormat = DeviceFormat::WITHOUT_ALPHA)
        : VirtualDevice(nullptr, eFormat, OUTDEV_VIRDEV) {}

    /** Create a virtual device of size 1x1

        @param rCompDev
        The generated vdev will be compatible to this device.

        @param eFormat
        Device format of the generated virtual device.
     */
    explicit VirtualDevice(const OutputDevice& rCompDev,
                           DeviceFormat eFormat = DeviceFormat::WITHOUT_ALPHA)
        : VirtualDevice(&rCompDev, eFormat, OUTDEV_VIRDEV) {}

    /** Create a virtual device using an existing system dependent device or graphics context
        Any rendering will happen directly on the context and not on any intermediate bitmap.
        Note: This might not be supported on all platforms !
     */
    explicit VirtualDevice(const SystemGraphicsData& rData, const Size &rSize,
                           DeviceFormat eFormat);

    virtual             ~VirtualDevice() override;
    virtual void        dispose() override;

    bool                CanEnableNativeWidget() const override;

    bool                CanAnimate() const override { return false; }

    virtual void        EnableRTL( bool bEnable = true ) override;

    bool                SetOutputSizePixel( const Size& rNewSize, bool bErase = true, bool bAlphaMaskTransparent = false );
    bool                SetOutputSizePixelScaleOffsetAndLOKBuffer( const Size& rNewSize,
                                                                   const Fraction& rScale,
                                                                   const Point& rNewOffset,
                                                                   sal_uInt8* pBuffer);

    bool                SetOutputSize( const Size& rNewSize )
                            { return SetOutputSizePixel( LogicToPixel( rNewSize ) ); }

    void                SetReferenceDevice( RefDevMode );

    void                Compat_ZeroExtleadBug(); // enable workaround for #i60495#

    void                SetReferenceDevice( sal_Int32 i_nDPIX, sal_Int32 i_nDPIY );

    virtual sal_uInt16  GetBitCount() const override;

    bool IsVirtual() const override;

    bool                IsScreenComp() const override { return mbScreenComp; }

    bool IsWithoutAlpha() const { return meFormatAndAlpha == DeviceFormat::WITHOUT_ALPHA; }

    virtual bool HasAlpha() const override { return meFormatAndAlpha != DeviceFormat::WITHOUT_ALPHA; }

private:
    SAL_DLLPRIVATE void ImplSetReferenceDevice( RefDevMode, sal_Int32 i_nDPIX, sal_Int32 i_nDPIY );

protected:
    virtual bool        UsePolyPolygonForComplexGradient() override;

    virtual tools::Long        GetFontExtLeading() const override;

};

#endif // INCLUDED_VCL_VIRDEV_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
