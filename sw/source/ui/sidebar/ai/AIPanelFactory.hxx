/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AIPANELFACTORY_HXX
#define INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AIPANELFACTORY_HXX

#include <com/sun/star/ui/XUIElementFactory.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <comphelper/compbase.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <vcl/weld.hxx>

namespace sw::sidebar {

/**
 * Factory class for creating AI Panel instances in LibreOffice Writer sidebar
 * 
 * Implements the UNO XUIElementFactory interface to integrate with the LibreOffice
 * sidebar framework. Handles creation of AIPanel instances when requested by
 * the sidebar system.
 */
class AIPanelFactory final : public comphelper::WeakComponentImplHelper<
    css::ui::XUIElementFactory,
    css::lang::XServiceInfo>
{
public:
    /**
     * Constructor
     */
    AIPanelFactory();

    /**
     * Destructor
     */
    virtual ~AIPanelFactory() override;

    // XUIElementFactory interface
    /**
     * Create UI element (AI Panel) when requested by sidebar framework
     * @param rsResourceURL Resource URL identifying the panel type
     * @param rArguments Arguments containing parent window, frame, and bindings
     * @return XUIElement interface to the created panel
     */
    virtual css::uno::Reference<css::ui::XUIElement> SAL_CALL createUIElement(
        const OUString& rsResourceURL,
        const css::uno::Sequence<css::beans::PropertyValue>& rArguments) override;

    // XServiceInfo interface  
    /**
     * Get implementation name for UNO service registration
     * @return Implementation name string
     */
    virtual OUString SAL_CALL getImplementationName() override;

    /**
     * Check if service name is supported
     * @param rServiceName Service name to check
     * @return true if supported
     */
    virtual sal_Bool SAL_CALL supportsService(const OUString& rServiceName) override;

    /**
     * Get list of supported service names
     * @return Sequence of supported service names
     */
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;

    // Service registration constants
    static OUString getImplementationName_static();
    static css::uno::Sequence<OUString> getSupportedServiceNames_static();
    static css::uno::Reference<css::uno::XInterface> create(
        const css::uno::Reference<css::uno::XComponentContext>& rxContext);

private:
    // Prevent copy construction and assignment
    AIPanelFactory(const AIPanelFactory&) = delete;
    AIPanelFactory& operator=(const AIPanelFactory&) = delete;

    /**
     * Validate resource URL for AI panel requests
     * @param rsResourceURL Resource URL to validate
     * @return true if this is an AI panel request
     */
    bool IsAIPanelRequest(const OUString& rsResourceURL) const;

    /**
     * Extract arguments from property sequence for panel creation
     * @param rArguments Property sequence from createUIElement call
     * @param rxFrame Output frame reference
     * @param rpParent Output parent widget pointer
     * @return true if all required arguments were found
     */
    bool ExtractCreationArguments(
        const css::uno::Sequence<css::beans::PropertyValue>& rArguments,
        css::uno::Reference<css::frame::XFrame>& rxFrame,
        weld::Widget*& rpParent) const;
};

} // end of namespace sw::sidebar

#endif // INCLUDED_SW_SOURCE_UI_SIDEBAR_AI_AIPANELFACTORY_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */