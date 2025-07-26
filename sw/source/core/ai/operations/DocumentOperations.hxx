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
        
            bool bSuccess;
        OUString sErrorMessage;
        sal_Int32 nRetryCount;
        
        OperationRecord(const OUString& rId, const OUString& rType)
            : sOperationId(rId), sOperationType(rType)
            , aTimestamp(std::chrono::steady_clock::now())
            , bCanUndo(false), bCanRedo(false), bSuccess(true)
            , nRetryCount(0) {}
    };
    
    // Enhanced operation tracking with checkpoints
    struct OperationCheckpoint
    {
        OUString sCheckpointId;
        OUString sOperationId;
        std::chrono::steady_clock::time_point aTimestamp;
        css::uno::Any aDocumentState;
        css::uno::Any aCursorState;
        css::uno::Any aSelectionState;
        bool bCanRollback;
        
        OperationCheckpoint(const OUString& rCheckpointId, const OUString& rOperationId)
            : sCheckpointId(rCheckpointId), sOperationId(rOperationId)
            , aTimestamp(std::chrono::steady_clock::now())
            , bCanRollback(true) {}
    };
    
    std::vector<OperationRecord> m_aOperationHistory;
    std::vector<OperationCheckpoint> m_aCheckpoints;
    sal_Int32 m_nMaxHistorySize;
    sal_Int32 m_nMaxCheckpoints;
    
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
    
    // Error handling and recovery integration
    struct ErrorContext
    {
        OUString sErrorCode;
        OUString sErrorMessage;
        OUString sOperationId;
        OUString sOperationType;
        std::chrono::steady_clock::time_point aTimestamp;
        sal_Int32 nSeverityLevel; // 1=Critical, 2=High, 3=Medium, 4=Low
        bool bRollbackRequired;
        bool bRetryAllowed;
        
        ErrorContext(const OUString& rCode, const OUString& rMessage, const OUString& rOpId)
            : sErrorCode(rCode), sErrorMessage(rMessage), sOperationId(rOpId)
            , aTimestamp(std::chrono::steady_clock::now())
            , nSeverityLevel(3), bRollbackRequired(false), bRetryAllowed(true) {}
    };
    
    std::vector<ErrorContext> m_aErrorHistory;
    sal_Int32 m_nMaxErrorHistory;
    
    // Phase 8.3: Cancellation and Progress Tracking Support
    struct CancellationToken
    {
        OUString sTokenId;
        OUString sOperationId;
        std::chrono::steady_clock::time_point aCreationTime;
        bool bCancelled;
        OUString sCancellationReason;
        OUString sCancelledBy;
        std::chrono::steady_clock::time_point aCancellationTime;
        
        CancellationToken(const OUString& rTokenId, const OUString& rOperationId)
            : sTokenId(rTokenId), sOperationId(rOperationId)
            , aCreationTime(std::chrono::steady_clock::now())
            , bCancelled(false) {}
    };
    
    struct ProgressInfo
    {
        OUString sOperationId;
        sal_Int32 nProgressPercentage;
        OUString sProgressMessage;
        std::chrono::steady_clock::time_point aLastUpdate;
        css::uno::Any aProgressMetadata;
        bool bCompleted;
        
        ProgressInfo(const OUString& rOperationId)
            : sOperationId(rOperationId), nProgressPercentage(0)
            , aLastUpdate(std::chrono::steady_clock::now())
            , bCompleted(false) {}
    };
    
    std::vector<CancellationToken> m_aCancellationTokens;
    std::vector<ProgressInfo> m_aProgressTracking;
    sal_Int32 m_nMaxCancellationTokens;
    sal_Int32 m_nMaxProgressEntries;
    
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
    
    // Enhanced error handling and rollback operations - Phase 8
    virtual OUString SAL_CALL createOperationCheckpoint(
        const OUString& rsOperationId,
        const css::uno::Sequence<css::beans::PropertyValue>& rCheckpointOptions);
        
    virtual sal_Bool SAL_CALL rollbackToCheckpoint(
        const OUString& rsCheckpointId,
        const css::uno::Sequence<css::beans::PropertyValue>& rRollbackOptions);
        
    virtual css::uno::Any SAL_CALL getOperationStatus(
        const OUString& rsOperationId);
        
    virtual css::uno::Sequence<css::beans::PropertyValue> SAL_CALL getErrorHistory(
        sal_Int32 nMaxEntries);
        
    virtual sal_Bool SAL_CALL canRecoverFromError(
        const OUString& rsErrorCode,
        const OUString& rsOperationId);
        
    virtual OUString SAL_CALL performErrorRecovery(
        const OUString& rsErrorCode,
        const OUString& rsOperationId,
        const css::uno::Sequence<css::beans::PropertyValue>& rRecoveryOptions);
    
    // Phase 8.3: Cancellation and Progress Tracking Support
    virtual OUString SAL_CALL createCancellationToken(
        const OUString& rsOperationId,
        const css::uno::Sequence<css::beans::PropertyValue>& rCancellationOptions);
        
    virtual sal_Bool SAL_CALL cancelOperation(
        const OUString& rsOperationId,
        const OUString& rsCancellationReason,
        const css::uno::Sequence<css::beans::PropertyValue>& rCancellationScope);
        
    virtual sal_Bool SAL_CALL isOperationCancelled(
        const OUString& rsOperationId);
        
    virtual css::uno::Any SAL_CALL getOperationProgress(
        const OUString& rsOperationId);
        
    virtual sal_Bool SAL_CALL updateOperationProgress(
        const OUString& rsOperationId,
        sal_Int32 nProgressPercentage,
        const OUString& rsProgressMessage,
        const css::uno::Sequence<css::beans::PropertyValue>& rProgressMetadata);
        
    virtual css::uno::Sequence<OUString> SAL_CALL getActiveCancellationTokens();
    
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
    
    // Enhanced error handling and rollback support - Phase 8 implementation
    OUString createCheckpointInternal(const OUString& rsOperationId);
    bool rollbackToCheckpointInternal(const OUString& rsCheckpointId);
    void recordOperationError(const OUString& rsOperationId, const OUString& rsErrorCode, 
                            const OUString& rsErrorMessage, sal_Int32 nSeverity);
    bool canPerformRollback(const OUString& rsOperationId) const;
    css::uno::Any captureDocumentState() const;
    bool restoreDocumentState(const css::uno::Any& rDocumentState);
    void cleanupExpiredCheckpoints();
    void notifyErrorHandlingSystem(const ErrorContext& rErrorContext) const;
    bool shouldRetryOperation(const OUString& rsOperationId, const OUString& rsErrorCode) const;
    css::uno::Sequence<css::beans::PropertyValue> buildErrorResponse(
        const OUString& rsErrorCode, const OUString& rsErrorMessage, 
        const OUString& rsOperationId, bool bCanRetry, bool bCanRollback) const;
    
    /**
     * Utility methods
     */
    OUString generateOperationId() const;
    css::beans::PropertyValue makePropertyValue(const OUString& rsName, const css::uno::Any& rValue) const;
    bool isValidPosition(const css::uno::Any& rPosition) const;
    bool hasWriteAccess() const;
    
    // Error context and recovery helpers
    ErrorContext createErrorContext(const OUString& rsErrorCode, const OUString& rsErrorMessage, 
                                  const OUString& rsOperationId) const;
    bool isRecoverableError(const OUString& rsErrorCode) const;
    sal_Int32 getErrorSeverity(const OUString& rsErrorCode) const;
    OUString generateCheckpointId() const;
    void validateOperationParameters(const OUString& rsOperationType, 
                                   const css::uno::Sequence<css::beans::PropertyValue>& rParameters) const;
    
    // Phase 8.3: Cancellation and progress tracking helpers
    OUString generateCancellationTokenId() const;
    CancellationToken* findCancellationToken(const OUString& rsOperationId);
    ProgressInfo* findProgressInfo(const OUString& rsOperationId);
    void cleanupExpiredTokensAndProgress();
    bool checkCancellationBeforeOperation(const OUString& rsOperationId) const;
    void notifyProgressToCoordinator(const OUString& rsOperationId, sal_Int32 nProgress, 
                                   const OUString& rsMessage) const;
    
    // Static factory method for UNO service creation
public:
    static css::uno::Reference<css::uno::XInterface> SAL_CALL create(
        const css::uno::Reference<css::uno::XComponentContext>& xContext);
        
    // Service implementation constants
    static constexpr OUStringLiteral IMPLEMENTATION_NAME = u"com.sun.star.ai.DocumentOperations";
    static constexpr OUStringLiteral SERVICE_NAME = u"com.sun.star.ai.XAIDocumentOperations";
    
    // Error handling constants - Phase 8
    static constexpr sal_Int32 DEFAULT_MAX_CHECKPOINTS = 20;
    static constexpr sal_Int32 DEFAULT_MAX_ERROR_HISTORY = 100;
    static constexpr sal_Int32 CHECKPOINT_CLEANUP_INTERVAL_MS = 300000; // 5 minutes
    
    // Error severity levels
    enum ErrorSeverity {
        CRITICAL = 1,
        HIGH = 2,
        MEDIUM = 3,
        LOW = 4
    };
    
    // Error codes for consistent error handling
    static constexpr const char* ERROR_INVALID_PARAMETERS = "INVALID_PARAMETERS";
    static constexpr const char* ERROR_DOCUMENT_ACCESS = "DOCUMENT_ACCESS";
    static constexpr const char* ERROR_OPERATION_FAILED = "OPERATION_FAILED";
    static constexpr const char* ERROR_ROLLBACK_FAILED = "ROLLBACK_FAILED";
    static constexpr const char* ERROR_CHECKPOINT_CREATION = "CHECKPOINT_CREATION";
    static constexpr const char* ERROR_RESOURCE_UNAVAILABLE = "RESOURCE_UNAVAILABLE";
    static constexpr const char* ERROR_TIMEOUT = "TIMEOUT";
    static constexpr const char* ERROR_PERMISSION_DENIED = "PERMISSION_DENIED";
    static constexpr const char* ERROR_MEMORY_EXHAUSTED = "MEMORY_EXHAUSTED";
    static constexpr const char* ERROR_NETWORK_FAILURE = "NETWORK_FAILURE";
};

} // namespace sw::core::ai::operations

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */