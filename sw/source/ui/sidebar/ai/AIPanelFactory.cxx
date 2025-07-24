/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include "AIPanelFactory.hxx"
#include "AIPanel.hxx"

#include <com/sun/star/ui/XUIElement.hpp>
#include <com/sun/star/awt/XWindow.hpp>
#include <comphelper/namedvaluecollection.hxx>
#include <sfx2/sidebar/SidebarPanelBase.hxx>
#include <vcl/weldutils.hxx>
#include <sal/log.hxx>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/uno/RuntimeException.hpp>

using namespace css;
using namespace css::uno;

namespace sw::sidebar {

// Service registration constants
constexpr OUStringLiteral AI_PANEL_FACTORY_IMPLEMENTATION_NAME = u"org.libreoffice.comp.Writer.sidebar.AIPanelFactory";
constexpr OUStringLiteral AI_PANEL_FACTORY_SERVICE_NAME = u"com.sun.star.ui.UIElementFactory";

AIPanelFactory::AIPanelFactory()
{
    SAL_INFO("sw.ui", "AIPanelFactory constructor");
}

AIPanelFactory::~AIPanelFactory()
{
    SAL_INFO("sw.ui", "AIPanelFactory destructor");
}

// XUIElementFactory implementation
Reference<ui::XUIElement> SAL_CALL AIPanelFactory::createUIElement(
    const OUString& rsResourceURL,
    const Sequence<beans::PropertyValue>& rArguments)
{
    SAL_INFO("sw.ui", "AIPanelFactory::createUIElement called with URL: " << rsResourceURL);
    
    Reference<ui::XUIElement> xElement;
    
    try
    {
        // Validate input parameters
        if (rsResourceURL.isEmpty())
        {
            SAL_WARN("sw.ui", "AIPanelFactory received empty resource URL");
            throw lang::IllegalArgumentException(
                u"Empty resource URL provided"_ustr, 
                static_cast<cppu::OWeakObject*>(this), 0);
        }
        
        // Check if this is an AI panel request
        if (!IsAIPanelRequest(rsResourceURL))
        {
            SAL_WARN("sw.ui", "AIPanelFactory received non-AI panel request: " << rsResourceURL);
            // Return null element for non-AI requests (not an error)
            return xElement;
        }
        
        // Extract required arguments with comprehensive validation
        Reference<frame::XFrame> xFrame;
        weld::Widget* pParent = nullptr;
        
        if (!ExtractCreationArguments(rArguments, xFrame, pParent))
        {
            SAL_WARN("sw.ui", "AIPanelFactory failed to extract creation arguments");
            throw lang::IllegalArgumentException(
                u"Required arguments (Frame, ParentWindow) not provided"_ustr,
                static_cast<cppu::OWeakObject*>(this), 1);
        }
        
        // Additional validation of extracted arguments
        if (!xFrame.is())
        {
            throw uno::RuntimeException(
                u"Frame reference is null after extraction"_ustr,
                static_cast<cppu::OWeakObject*>(this));
        }
        
        if (!pParent)
        {
            throw uno::RuntimeException(
                u"Parent widget is null after extraction"_ustr,
                static_cast<cppu::OWeakObject*>(this));
        }
        
        // Create AI Panel instance with error handling
        std::unique_ptr<PanelLayout> xPanel;
        try
        {
            xPanel = AIPanel::Create(pParent, xFrame);
        }
        catch (const std::exception& e)
        {
            SAL_WARN("sw.ui", "AIPanelFactory failed to create AIPanel: " << e.what());
            throw uno::RuntimeException(
                u"Failed to create AI Panel instance"_ustr,
                static_cast<cppu::OWeakObject*>(this));
        }
        catch (...)
        {
            SAL_WARN("sw.ui", "AIPanelFactory failed to create AIPanel: unknown exception");
            throw uno::RuntimeException(
                u"Failed to create AI Panel instance (unknown error)"_ustr,
                static_cast<cppu::OWeakObject*>(this));
        }
        
        if (!xPanel)
        {
            SAL_WARN("sw.ui", "AIPanelFactory: AIPanel::Create returned null");
            throw uno::RuntimeException(
                u"AI Panel creation returned null pointer"_ustr,
                static_cast<cppu::OWeakObject*>(this));
        }
        
        // Wrap in SidebarPanelBase for framework integration
        try
        {
            xElement = sfx2::sidebar::SidebarPanelBase::Create(
                rsResourceURL,
                xFrame,
                std::move(xPanel),
                ui::LayoutSize(-1, -1, -1));
        }
        catch (const Exception& e)
        {
            SAL_WARN("sw.ui", "AIPanelFactory failed to wrap panel: " << e.Message);
            throw uno::RuntimeException(
                u"Failed to wrap AI Panel in SidebarPanelBase"_ustr,
                static_cast<cppu::OWeakObject*>(this));
        }
        
        if (!xElement.is())
        {
            throw uno::RuntimeException(
                u"SidebarPanelBase::Create returned null element"_ustr,
                static_cast<cppu::OWeakObject*>(this));
        }
        
        SAL_INFO("sw.ui", "AIPanelFactory successfully created AI panel");
    }
    catch (const lang::IllegalArgumentException&)
    {
        // Re-throw IllegalArgumentException as-is
        throw;
    }
    catch (const uno::RuntimeException&)
    {
        // Re-throw RuntimeException as-is
        throw;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ui", "AIPanelFactory::createUIElement UNO exception: " << e.Message);
        throw uno::RuntimeException(
            u"Unexpected UNO exception in createUIElement: "_ustr + e.Message,
            static_cast<cppu::OWeakObject*>(this));
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ui", "AIPanelFactory::createUIElement std exception: " << e.what());
        throw uno::RuntimeException(
            u"Unexpected std exception in createUIElement"_ustr,
            static_cast<cppu::OWeakObject*>(this));
    }
    catch (...)
    {
        SAL_WARN("sw.ui", "AIPanelFactory::createUIElement unknown exception");
        throw uno::RuntimeException(
            u"Unknown exception in createUIElement"_ustr,
            static_cast<cppu::OWeakObject*>(this));
    }
    
    return xElement;
}

// XServiceInfo implementation
OUString SAL_CALL AIPanelFactory::getImplementationName()
{
    return getImplementationName_static();
}

sal_Bool SAL_CALL AIPanelFactory::supportsService(const OUString& rServiceName)
{
    return cppu::supportsService(this, rServiceName);
}

Sequence<OUString> SAL_CALL AIPanelFactory::getSupportedServiceNames()
{
    return getSupportedServiceNames_static();
}

// Static service registration methods
OUString AIPanelFactory::getImplementationName_static()
{
    return AI_PANEL_FACTORY_IMPLEMENTATION_NAME;
}

Sequence<OUString> AIPanelFactory::getSupportedServiceNames_static()
{
    return { OUString(AI_PANEL_FACTORY_SERVICE_NAME) };
}

Reference<XInterface> AIPanelFactory::create(
    const Reference<XComponentContext>& /*rxContext*/)
{
    return static_cast<cppu::OWeakObject*>(new AIPanelFactory());
}

// Private helper methods
bool AIPanelFactory::IsAIPanelRequest(const OUString& rsResourceURL) const
{
    // Check for AI panel resource URL pattern
    return rsResourceURL.endsWith("/AIPanel") || 
           rsResourceURL.indexOf("SwAIPanel") >= 0;
}

bool AIPanelFactory::ExtractCreationArguments(
    const Sequence<beans::PropertyValue>& rArguments,
    Reference<frame::XFrame>& rxFrame,
    weld::Widget*& rpParent) const
{
    try
    {
        const ::comphelper::NamedValueCollection aArguments(rArguments);
        
        // Extract frame reference
        rxFrame = aArguments.getOrDefault(u"Frame"_ustr, Reference<frame::XFrame>());
        if (!rxFrame.is())
        {
            SAL_WARN("sw.ui", "AIPanelFactory: No Frame provided in arguments");
            return false;
        }
        
        // Extract parent window
        Reference<awt::XWindow> xParentWindow = 
            aArguments.getOrDefault(u"ParentWindow"_ustr, Reference<awt::XWindow>());
        if (!xParentWindow.is())
        {
            SAL_WARN("sw.ui", "AIPanelFactory: No ParentWindow provided in arguments");
            return false;
        }
        
        // Convert XWindow to weld::Widget
        if (weld::TransportAsXWindow* pTunnel = 
            dynamic_cast<weld::TransportAsXWindow*>(xParentWindow.get()))
        {
            rpParent = pTunnel->getWidget();
        }
        
        if (!rpParent)
        {
            SAL_WARN("sw.ui", "AIPanelFactory: Failed to get parent widget from XWindow");
            return false;
        }
        
        SAL_INFO("sw.ui", "AIPanelFactory: Successfully extracted creation arguments");
        return true;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ui", "AIPanelFactory::ExtractCreationArguments exception: " << e.Message);
        return false;
    }
}

} // end of namespace sw::sidebar

// UNO service registration implementation
extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
Writer_AIPanelFactory_get_implementation(
    css::uno::XComponentContext* /*context*/,
    css::uno::Sequence<css::uno::Any> const& /*args*/)
{
    return cppu::acquire(new sw::sidebar::AIPanelFactory());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */