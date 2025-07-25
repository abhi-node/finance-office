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
#include <com/sun/star/ai/XAIDocumentOperations.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/lang/XInitialization.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/uno/Sequence.hxx>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/text/XText.hpp>
#include <com/sun/star/text/XTextCursor.hpp>
#include <com/sun/star/text/XTextTable.hpp>
#include <com/sun/star/drawing/XShape.hpp>

#include <memory>
#include <mutex>
#include <vector>
#include <string>

// Forward declarations
class SwWrtShell;
class SwEditShell;
class SwDoc;
class SwView;

namespace sw::core::ai::operations {

/**
 * DocumentOperations - Core UNO service for document manipulation from Python agents
 * 
 * This class provides the foundational UNO service framework for all document
 * operations requested by the LangGraph multi-agent system. It serves as the
 * bridge between Python agents and LibreOffice's native document manipulation
 * capabilities through SwEditShell, SwWrtShell, and document model hierarchies.
 * 
 * Design Principles:
 * - Direct Integration: Uses SwEditShell/SwWrtShell for immediate document access
 * - Atomic Operations: All operations support undo/redo and can be rolled back
 * - Thread Safety: All operations are thread-safe and resource-managed
 * - Performance: Optimized for 1-5 second response times per specification
 * - Extensibility: Designed for easy addition of new operation types
 */
class DocumentOperations final : public cppu::WeakImplHelper<
    css::ai::XAIDocumentOperations,
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
    
    // Operation tracking and undo/redo integration
    struct OperationRecord
    {
        OUString sOperationId;
        OUString sOperationType;
        std::chrono::steady_clock::time_point aTimestamp;
        css::uno::Any aUndoData;
        bool bCanUndo;
        bool bCanRedo;
        
        OperationRecord(const OUString& rId, const OUString& rType)
            : sOperationId(rId), sOperationType(rType)
            , aTimestamp(std::chrono::steady_clock::now())
            , bCanUndo(false), bCanRedo(false) {}
    };
    
    std::vector<OperationRecord> m_aOperationHistory;
    sal_Int32 m_nMaxHistorySize;
    
    // Resource management
    struct ResourceLock
    {
        std::mutex aMutex;
        bool bInUse;
        std::chrono::steady_clock::time_point aLastAccess;
        
        ResourceLock() : bInUse(false), aLastAccess(std::chrono::steady_clock::now()) {}
    };
    
    std::unique_ptr<ResourceLock> m_pDocumentLock;
    std::unique_ptr<ResourceLock> m_pShellLock;
    
public:
    explicit DocumentOperations(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    virtual ~DocumentOperations() override;
    
    // XAIDocumentOperations interface - Core document manipulation API
    
    /**
     * Text insertion and formatting operations
     * Provides comprehensive text manipulation with proper cursor management
     */
    virtual OUString SAL_CALL insertText(
        const OUString& rsText,
        const css::uno::Any& rPosition,
        const css::uno::Sequence<css::beans::PropertyValue>& rFormatting) override;
        
    virtual OUString SAL_CALL formatText(
        const css::uno::Any& rTextRange,
        const css::uno::Sequence<css::beans::PropertyValue>& rFormatting) override;
        
    virtual OUString SAL_CALL applyStyle(
        const css::uno::Any& rTarget,
        const OUString& rsStyleName,
        const css::uno::Sequence<css::beans::PropertyValue>& rStyleProperties) override;
    
    /**
     * Table creation and manipulation operations
     * Advanced table operations with proper formatting and data population
     */
    virtual OUString SAL_CALL createTable(
        sal_Int32 nRows,
        sal_Int32 nColumns,
        const css::uno::Any& rPosition,
        const css::uno::Sequence<css::beans::PropertyValue>& rTableProperties) override;
        
    virtual OUString SAL_CALL populateTable(
        const css::uno::Any& rTableReference,
        const css::uno::Sequence<css::uno::Sequence<OUString>>& rData,
        const css::uno::Sequence<css::beans::PropertyValue>& rFormatting) override;
        
    virtual OUString SAL_CALL modifyTableStructure(
        const css::uno::Any& rTableReference,
        const OUString& rsOperation,
        const css::uno::Sequence<css::beans::PropertyValue>& rParameters) override;
    
    /**
     * Chart and graphics operations
     * Professional chart creation with data binding and styling
     */
    virtual OUString SAL_CALL insertChart(
        const css::uno::Any& rChartData,
        const OUString& rsChartType,
        const css::uno::Any& rPosition,
        const css::uno::Sequence<css::beans::PropertyValue>& rChartProperties) override;
        
    virtual OUString SAL_CALL insertGraphic(
        const css::uno::Any& rGraphicData,
        const css::uno::Any& rPosition,
        const css::uno::Sequence<css::beans::PropertyValue>& rGraphicProperties) override;
    
    /**
     * Financial data integration operations
     * Specialized operations for financial document creation
     */
    virtual OUString SAL_CALL insertFinancialTable(
        const css::uno::Sequence<css::beans::PropertyValue>& rFinancialData,
        const css::uno::Any& rPosition,
        const css::uno::Sequence<css::beans::PropertyValue>& rFormatting) override;
        
    virtual OUString SAL_CALL createFinancialChart(
        const css::uno::Sequence<css::beans::PropertyValue>& rMarketData,
        const OUString& rsChartType,
        const css::uno::Any& rPosition) override;
        
    virtual OUString SAL_CALL insertMarketSummary(
        const css::uno::Sequence<css::beans::PropertyValue>& rSummaryData,
        const css::uno::Any& rPosition,
        const css::uno::Sequence<css::beans::PropertyValue>& rFormatting) override;
    
    /**
     * Document structure operations
     * Page breaks, sections, headers/footers, and document organization
     */
    virtual OUString SAL_CALL insertPageBreak(
        const css::uno::Any& rPosition,
        const OUString& rsBreakType) override;
        
    virtual OUString SAL_CALL createSection(
        const OUString& rsSectionName,
        const css::uno::Any& rPosition,
        const css::uno::Sequence<css::beans::PropertyValue>& rSectionProperties) override;
        
    virtual OUString SAL_CALL modifyHeaderFooter(
        const OUString& rsType,
        const OUString& rsContent,
        const css::uno::Sequence<css::beans::PropertyValue>& rFormatting) override;
    
    /**
     * Operation management and undo/redo system
     * Comprehensive operation tracking with full rollback capabilities
     */
    virtual css::uno::Sequence<OUString> SAL_CALL getOperationHistory() override;
    
    virtual sal_Bool SAL_CALL canUndo() override;
    virtual sal_Bool SAL_CALL canRedo() override;
    
    virtual OUString SAL_CALL undoLastOperation() override;
    virtual OUString SAL_CALL redoLastOperation() override;
    
    virtual OUString SAL_CALL undoOperation(const OUString& rsOperationId) override;
    
    /**
     * Document context and analysis
     * Provides document information for agent decision making
     */
    virtual css::uno::Any SAL_CALL getDocumentContext() override;
    virtual css::uno::Any SAL_CALL getCursorPosition() override;
    virtual OUString SAL_CALL getSelectedText() override;
    virtual css::uno::Any SAL_CALL getDocumentStructure() override;
    
    // XServiceInfo interface - LibreOffice service registration
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& rServiceName) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;
    
    // XInitialization interface - Proper UNO service initialization
    virtual void SAL_CALL initialize(const css::uno::Sequence<css::uno::Any>& rArguments) override;
    
    // Additional lifecycle management
    void SAL_CALL initializeWithFrame(const css::uno::Reference<css::frame::XFrame>& xFrame);
    void SAL_CALL shutdown();
    
private:
    // Internal implementation methods
    
    /**
     * Writer shell access and management
     * Thread-safe access to LibreOffice editing interfaces
     */
    SwWrtShell* getWriterShell() const;
    SwEditShell* getEditShell() const;
    SwDoc* getDocument() const;
    SwView* getView() const;
    
    bool ensureDocumentAccess();
    void releaseDocumentAccess();
    
    /**
     * Cursor and position management
     * Handles cursor positioning for all operations
     */
    bool setCursorPosition(const css::uno::Any& rPosition);
    css::uno::Any getCurrentCursorPosition() const;
    css::uno::Reference<css::text::XTextCursor> createTextCursor(const css::uno::Any& rPosition);
    
    /**
     * Formatting and style operations
     * Internal formatting logic with proper error handling
     */
    bool applyTextFormatting(const css::uno::Reference<css::text::XTextCursor>& xCursor,
                           const css::uno::Sequence<css::beans::PropertyValue>& rFormatting);
    bool applyParagraphFormatting(const css::uno::Reference<css::text::XTextCursor>& xCursor,
                               const css::uno::Sequence<css::beans::PropertyValue>& rFormatting);
    bool applyCharacterFormatting(const css::uno::Reference<css::text::XTextCursor>& xCursor,
                                const css::uno::Sequence<css::beans::PropertyValue>& rFormatting);
    
    /**
     * Table operations implementation
     * Advanced table creation and manipulation
     */
    css::uno::Reference<css::text::XTextTable> createTextTable(
        sal_Int32 nRows, sal_Int32 nColumns, const css::uno::Any& rPosition);
    bool populateTextTable(const css::uno::Reference<css::text::XTextTable>& xTable,
                         const css::uno::Sequence<css::uno::Sequence<OUString>>& rData);
    bool formatTextTable(const css::uno::Reference<css::text::XTextTable>& xTable,
                       const css::uno::Sequence<css::beans::PropertyValue>& rFormatting);
    
    /**
     * Chart and graphics implementation
     * Professional chart creation with LibreOffice chart engine
     */
    css::uno::Reference<css::drawing::XShape> createChartObject(
        const css::uno::Any& rChartData, const OUString& rsChartType, const css::uno::Any& rPosition);
    bool configureChart(const css::uno::Reference<css::drawing::XShape>& xChart,
                       const css::uno::Sequence<css::beans::PropertyValue>& rProperties);
    
    /**
     * Financial operations implementation
     * Specialized financial document operations
     */
    bool createFinancialTableStructure(
        const css::uno::Reference<css::text::XTextTable>& xTable,
        const css::uno::Sequence<css::beans::PropertyValue>& rFinancialData);
    bool populateFinancialData(
        const css::uno::Reference<css::text::XTextTable>& xTable,
        const css::uno::Sequence<css::beans::PropertyValue>& rFinancialData);
    bool applyFinancialFormatting(
        const css::uno::Reference<css::text::XTextTable>& xTable);
    
    /**
     * Undo/Redo system integration
     * Proper integration with LibreOffice's undo system
     */
    OUString recordOperation(const OUString& rsOperationType, const css::uno::Any& rUndoData);
    bool executeUndo(const OUString& rsOperationId);
    bool executeRedo(const OUString& rsOperationId);
    void clearOperationHistory();
    
    /**
     * Error handling and resource management
     * Comprehensive error handling with proper cleanup
     */
    bool validateOperation(const OUString& rsOperationType, const css::uno::Sequence<css::uno::Any>& rParameters);
    void handleOperationError(const OUString& rsOperationId, const OUString& rsError);
    void logOperationActivity(const OUString& rsOperationId, const OUString& rsMessage) const;
    
    /**
     * Utility methods
     */
    OUString generateOperationId() const;
    css::beans::PropertyValue makePropertyValue(const OUString& rsName, const css::uno::Any& rValue) const;
    bool isValidPosition(const css::uno::Any& rPosition) const;
    bool hasWriteAccess() const;
    
    // Static factory method for UNO service creation
public:
    static css::uno::Reference<css::uno::XInterface> SAL_CALL create(
        const css::uno::Reference<css::uno::XComponentContext>& xContext);
        
    // Service implementation constants
    static constexpr OUStringLiteral IMPLEMENTATION_NAME = u"com.sun.star.ai.DocumentOperations";
    static constexpr OUStringLiteral SERVICE_NAME = u"com.sun.star.ai.XAIDocumentOperations";
};

} // namespace sw::core::ai::operations

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */