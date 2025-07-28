/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <sal/config.h>

#include <cppuhelper/implbase.hxx>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XInitialization.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/uno/Sequence.hxx>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/text/XTextTable.hpp>
#include <com/sun/star/drawing/XShape.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>

#include <memory>
#include <mutex>
#include <vector>
#include <map>
#include <string>

// Forward declarations
class SwWrtShell;
class SwEditShell;
class SwDoc;
class SwView;

namespace sw::core::ai::operations {


/**
 * DocumentOperations - Simplified UNO service for 4 core document operations
 * 
 * This class provides a streamlined interface for agent-driven document manipulation:
 * 1. Insert Text - Add text at specified position
 * 2. Format Selected Text - Apply formatting to currently selected text
 * 3. Insert Basic Table - Create and populate simple tables
 * 4. Insert Basic Chart - Create basic charts with data
 */
class DocumentOperations final : public cppu::WeakImplHelper<
    css::lang::XServiceInfo,
    css::lang::XInitialization>
{
private:
    // Core LibreOffice integration
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    css::uno::Reference<css::frame::XFrame> m_xFrame;
    css::uno::Reference<css::text::XTextDocument> m_xTextDocument;
    
    // Thread safety and state management
    mutable std::mutex m_aMutex;
    bool m_bInitialized;
    
    // Writer-specific interfaces
    SwWrtShell* m_pWrtShell;
    SwEditShell* m_pEditShell;
    SwDoc* m_pDoc;
    SwView* m_pView;
    
    // Operation tracking
    sal_Int32 m_nOperationCounter;

public:
    explicit DocumentOperations(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    virtual ~DocumentOperations() override;
    
    // XInitialization interface - LibreOffice service initialization
    virtual void SAL_CALL initialize(const css::uno::Sequence<css::uno::Any>& rArguments) override;
    
    // XServiceInfo interface - LibreOffice service registration
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& rServiceName) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;
    
    // ========================================
    // 4 CORE AGENT OPERATIONS - Simplified
    // ========================================
    
    /**
     * Insert text at the end of the document
     * @param rsContent Text content to insert
     * @return Operation result message
     */
    OUString insertAgentText(const OUString& rsContent);
    
    /**
     * Format currently selected text
     * @param rsFormattingJson JSON object containing formatting properties
     * @return Operation result message
     */
    OUString formatAgentText(const OUString& rsFormattingJson);
    
    /**
     * Insert table at the end of the document
     * @param nRows Number of rows
     * @param nColumns Number of columns
     * @return Operation result message
     */
    OUString insertAgentTable(sal_Int32 nRows, sal_Int32 nColumns);
    
    /**
     * Insert chart at the end of the document
     * @param rsChartType Chart type (bar, line, pie, column)
     * @return Operation result message
     */
    OUString insertAgentChart(const OUString& rsChartType);
    

    
    // Additional lifecycle management
    void SAL_CALL initializeWithFrame(const css::uno::Reference<css::frame::XFrame>& xFrame);
    void SAL_CALL shutdown();

private:
    // Internal implementation methods
    
    /**
     * Document access management
     */
    bool ensureDocumentAccess();
    void releaseDocumentAccess();
    

    
    /**
     * Utility methods
     */
    OUString generateOperationId();
    
    // Static factory method for UNO service creation
public:
    static css::uno::Reference<css::uno::XInterface> SAL_CALL create(
        const css::uno::Reference<css::uno::XComponentContext>& xContext);
        
    // Service implementation constants
    static constexpr OUStringLiteral IMPLEMENTATION_NAME = u"com.sun.star.ai.DocumentOperations";
    static constexpr OUStringLiteral SERVICE_NAME = u"com.sun.star.ai.DocumentOperations";
};

} // namespace sw::core::ai::operations

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */