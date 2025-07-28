/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "DocumentOperations.hxx"

#include <sal/log.hxx>
#include <comphelper/servicehelper.hxx>
#include <cppuhelper/supportsservice.hxx>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/lang/IllegalArgumentException.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/table/XCell.hpp>
#include <rtl/ustrbuf.hxx>
#include <com/sun/star/uno/Exception.hpp>
#include <com/sun/star/text/XTextTable.hpp>
#include <com/sun/star/text/XTextTablesSupplier.hpp>
#include <com/sun/star/text/XText.hpp>
#include <com/sun/star/text/XTextViewCursor.hpp>
#include <com/sun/star/text/XTextViewCursorSupplier.hpp>
#include <com/sun/star/text/XTextCursor.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/drawing/XShape.hpp>
#include <com/sun/star/chart2/XChartDocument.hpp>
#include <com/sun/star/embed/XEmbeddedObject.hpp>
#include <com/sun/star/embed/EmbedStates.hpp>
#include <com/sun/star/document/XEmbeddedObjectSupplier.hpp>
#include <com/sun/star/awt/Size.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/container/XIndexAccess.hpp>

// LibreOffice Writer core includes
#include <wrtsh.hxx>
#include <editsh.hxx>
#include <doc.hxx>
#include <view.hxx>
#include <docsh.hxx>
#include <swdtflvr.hxx>
#include <ndtxt.hxx>
#include <txatbase.hxx>
#include <fmtanchr.hxx>
#include <frmfmt.hxx>
#include <IDocumentUndoRedo.hxx>
#include <swabstdlg.hxx>
#include <tablemgr.hxx>
#include <comphelper/classids.hxx>
#include <svtools/embedhlp.hxx>
#include <swtable.hxx>
#include <itabenum.hxx>

#include <chrono>
#include <algorithm>
#include <sstream>

using namespace css;
using namespace css::uno;
using namespace css::lang;
using namespace css::beans;
using namespace css::text;
using namespace css::frame;
using namespace css::embed;
using namespace css::document;
using namespace css::chart2;

namespace sw::core::ai::operations {

//=============================================================================
// DocumentOperations Implementation
//=============================================================================

DocumentOperations::DocumentOperations(const Reference<XComponentContext>& xContext)
    : m_xContext(xContext)
    , m_bInitialized(false)
    , m_pWrtShell(nullptr)
    , m_pEditShell(nullptr)
    , m_pDoc(nullptr)
    , m_pView(nullptr)
    , m_nOperationCounter(0)
{
    SAL_INFO("sw.ai", "DocumentOperations created");
}

DocumentOperations::~DocumentOperations()
{
    shutdown();
}

// XInitialization interface
void SAL_CALL DocumentOperations::initialize(const Sequence<Any>& rArguments)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (m_bInitialized)
        return;
        
    if (rArguments.getLength() > 0)
    {
        Reference<XFrame> xFrame;
        rArguments[0] >>= xFrame;
        if (xFrame.is())
        {
            m_xFrame = xFrame;
            Reference<XModel> xModel = xFrame->getController()->getModel();
            m_xTextDocument.set(xModel, UNO_QUERY);
        }
    }
    
    m_bInitialized = true;
    SAL_INFO("sw.ai", "DocumentOperations initialized");
}

void SAL_CALL DocumentOperations::initializeWithFrame(const Reference<XFrame>& xFrame)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    m_xFrame = xFrame;
    if (xFrame.is())
    {
        Reference<XModel> xModel = xFrame->getController()->getModel();
        m_xTextDocument.set(xModel, UNO_QUERY);
    }
    
    m_bInitialized = true;
    SAL_INFO("sw.ai", "DocumentOperations initialized with frame");
}

void SAL_CALL DocumentOperations::shutdown()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    releaseDocumentAccess();
    m_xTextDocument.clear();
    m_xFrame.clear();
    m_bInitialized = false;
    
    SAL_INFO("sw.ai", "DocumentOperations shutdown");
}

// XServiceInfo interface
OUString SAL_CALL DocumentOperations::getImplementationName()
{
    return IMPLEMENTATION_NAME;
}

sal_Bool SAL_CALL DocumentOperations::supportsService(const OUString& rServiceName)
{
    return cppu::supportsService(this, rServiceName);
}

Sequence<OUString> SAL_CALL DocumentOperations::getSupportedServiceNames()
{
    return { SERVICE_NAME };
}

//=============================================================================
// 4 CORE AGENT OPERATIONS - Simplified Implementation
//=============================================================================

OUString DocumentOperations::insertAgentText(const OUString& rsContent)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    SAL_INFO("sw.ai", "INSERT_TEXT: Starting text insertion");
    
    try
    {
        if (!m_xTextDocument.is())
        {
            SAL_INFO("sw.ai", "INSERT_TEXT: ERROR - No text document available");
            return "ERROR: No text document available";
        }
        
        // Validate content before insertion
        if (rsContent.isEmpty())
        {
            SAL_INFO("sw.ai", "INSERT_TEXT: WARNING - Empty content, nothing to insert");
            return "SUCCESS: No content to insert";
        }
        
        // Check for problematic characters and clean the content
        OUStringBuffer aCleanContent;
        for (sal_Int32 i = 0; i < rsContent.getLength(); ++i)
        {
            sal_Unicode ch = rsContent[i];
            // Allow normal characters, newlines, tabs, and common punctuation
            if (ch < 0x20 && ch != '\r' && ch != '\n' && ch != '\t')
            {
                SAL_WARN("sw.ai", "INSERT_TEXT: Skipping control character at position " << i << ": " << static_cast<sal_uInt32>(ch));
                // Skip control characters
            }
            else
            {
                aCleanContent.append(ch);
            }
        }
        OUString sCleanContent = aCleanContent.makeStringAndClear();
        
        SAL_INFO("sw.ai", "INSERT_TEXT: Content validated, original length: " << rsContent.getLength() << ", cleaned length: " << sCleanContent.getLength());
        
        // Get the text object from the document
        Reference<XText> xText = m_xTextDocument->getText();
        if (!xText.is())
        {
            SAL_INFO("sw.ai", "INSERT_TEXT: ERROR - No text object available");
            return "ERROR: No text object available";
        }
        
        // Get the end cursor and insert text
        Reference<XTextCursor> xCursor = xText->createTextCursor();
        if (!xCursor.is())
        {
            SAL_INFO("sw.ai", "INSERT_TEXT: ERROR - Cannot create cursor");
            return "ERROR: Cannot create cursor";
        }
        
        SAL_INFO("sw.ai", "INSERT_TEXT: Moving cursor to end of document");
        xCursor->gotoEnd(false);
        
        SAL_INFO("sw.ai", "INSERT_TEXT: About to insert content into document");
        
        // Insert the text - this is where the assertion might be happening
        try
        {
            xText->insertString(xCursor, sCleanContent, false);
            SAL_INFO("sw.ai", "INSERT_TEXT: xText->insertString completed successfully");
        }
        catch (const css::uno::Exception& e)
        {
            SAL_WARN("sw.ai", "INSERT_TEXT: Exception during insertString: " << e.Message);
            throw;
        }
        
        SAL_INFO("sw.ai", "INSERT_TEXT: Operation completed successfully");
        return "SUCCESS: Text inserted at end of document";
    }
    catch (const css::uno::Exception& e)
    {
        SAL_INFO("sw.ai", "INSERT_TEXT: UNO Exception - " << e.Message);
        return "ERROR: UNO Exception - " + e.Message;
    }
    catch (const std::exception& e)
    {
        SAL_INFO("sw.ai", "INSERT_TEXT: std::exception - " << e.what());
        return "ERROR: std::exception - " + OUString::createFromAscii(e.what());
    }
    catch (...)
    {
        SAL_INFO("sw.ai", "INSERT_TEXT: Unknown exception occurred");
        return "ERROR: Unknown exception";
    }
}

OUString DocumentOperations::formatAgentText(const OUString& /*rsFormattingJson*/)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    SAL_INFO("sw.ai", "FORMAT_TEXT: Starting text formatting");
    
    try
    {
        if (!m_xTextDocument.is())
        {
            SAL_INFO("sw.ai", "FORMAT_TEXT: ERROR - No text document available");
            return "ERROR: No text document available";
        }
        
        // Get the current controller and selection
        Reference<XModel> xModel(m_xTextDocument, UNO_QUERY);
        if (!xModel.is())
        {
            SAL_INFO("sw.ai", "FORMAT_TEXT: ERROR - Cannot get model interface");
            return "ERROR: Cannot get model interface";
        }
        
        Reference<XController> xController = xModel->getCurrentController();
        if (!xController.is())
        {
            SAL_INFO("sw.ai", "FORMAT_TEXT: ERROR - No controller available");
            return "ERROR: No controller available";
        }
        
        Reference<XTextViewCursor> xViewCursor;
        Reference<XTextViewCursorSupplier> xViewCursorSupplier(xController, UNO_QUERY);
        if (xViewCursorSupplier.is())
        {
            xViewCursor = xViewCursorSupplier->getViewCursor();
        }
        
        if (!xViewCursor.is())
        {
            SAL_INFO("sw.ai", "FORMAT_TEXT: ERROR - No view cursor available");
            return "ERROR: No view cursor available";
        }
        
        // Check if there's a selection
        Reference<XTextRange> xSelection = xViewCursor;
        if (!xViewCursor->isCollapsed())
        {
            SAL_INFO("sw.ai", "FORMAT_TEXT: Applying bold formatting to selected text");
            
            // Apply bold formatting using property
            Reference<XPropertySet> xProps(xSelection, UNO_QUERY);
            if (xProps.is())
            {
                // Use standard bold weight value (150.0 = bold in LibreOffice)
                xProps->setPropertyValue("CharWeight", Any(150.0f));
                SAL_INFO("sw.ai", "FORMAT_TEXT: Bold formatting applied successfully");
                return "SUCCESS: Bold formatting applied to selected text";
            }
            else
            {
                SAL_INFO("sw.ai", "FORMAT_TEXT: Cannot get property interface");
                return "ERROR: Cannot access text properties";
            }
        }
        else
        {
            SAL_INFO("sw.ai", "FORMAT_TEXT: No text selected");
            return "ERROR: No text selected for formatting";
        }
    }
    catch (const css::uno::Exception& e)
    {
        SAL_INFO("sw.ai", "FORMAT_TEXT: UNO Exception - " << e.Message);
        return "ERROR: UNO Exception - " + e.Message;
    }
    catch (const std::exception& e)
    {
        SAL_INFO("sw.ai", "FORMAT_TEXT: std::exception - " << e.what());
        return "ERROR: std::exception - " + OUString::createFromAscii(e.what());
    }
    catch (...)
    {
        SAL_INFO("sw.ai", "FORMAT_TEXT: Unknown exception occurred");
        return "ERROR: Unknown exception";
    }
}

OUString DocumentOperations::insertAgentTable(sal_Int32 nRows, sal_Int32 nColumns)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    SAL_INFO("sw.ai", "INSERT_TABLE: Starting table insertion");
    
    try
    {
        if (!m_xTextDocument.is())
        {
            SAL_INFO("sw.ai", "INSERT_TABLE: ERROR - No text document available");
            return "ERROR: No text document available";
        }
        
        // Get the text object from the document
        Reference<XText> xText = m_xTextDocument->getText();
        if (!xText.is())
        {
            SAL_INFO("sw.ai", "INSERT_TABLE: ERROR - No text object available");
            return "ERROR: No text object available";
        }
        
        // Create cursor at end of document
        Reference<XTextCursor> xCursor = xText->createTextCursor();
        if (!xCursor.is())
        {
            SAL_INFO("sw.ai", "INSERT_TABLE: ERROR - Cannot create cursor");
            return "ERROR: Cannot create cursor";
        }
        
        xCursor->gotoEnd(false);
        
        // Create table using UNO service
        Reference<XMultiServiceFactory> xMSF(m_xTextDocument, UNO_QUERY);
        if (!xMSF.is())
        {
            SAL_INFO("sw.ai", "INSERT_TABLE: ERROR - Cannot get service factory");
            return "ERROR: Cannot get service factory";
        }
        
        Reference<XTextTable> xTable(xMSF->createInstance("com.sun.star.text.TextTable"), UNO_QUERY);
        if (!xTable.is())
        {
            SAL_INFO("sw.ai", "INSERT_TABLE: ERROR - Cannot create text table");
            return "ERROR: Cannot create text table";
        }
        
        SAL_INFO("sw.ai", "INSERT_TABLE: Initializing table with " << nRows << " rows and " << nColumns << " columns");
        xTable->initialize(nRows, nColumns);
        
        SAL_INFO("sw.ai", "INSERT_TABLE: Inserting table into document");
        xText->insertTextContent(xCursor, xTable, false);
        
        SAL_INFO("sw.ai", "INSERT_TABLE: Operation completed successfully");
        return "SUCCESS: Table inserted at end of document";
    }
    catch (const css::uno::Exception& e)
    {
        SAL_INFO("sw.ai", "INSERT_TABLE: UNO Exception - " << e.Message);
        return "ERROR: UNO Exception - " + e.Message;
    }
    catch (const std::exception& e)
    {
        SAL_INFO("sw.ai", "INSERT_TABLE: std::exception - " << e.what());
        return "ERROR: std::exception - " + OUString::createFromAscii(e.what());
    }
    catch (...)
    {
        SAL_INFO("sw.ai", "INSERT_TABLE: Unknown exception occurred");
        return "ERROR: Unknown exception";
    }
}

OUString DocumentOperations::insertAgentChart(const OUString& rsChartType)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    SAL_INFO("sw.ai", "INSERT_CHART: Starting chart insertion");
    
    try
    {
        if (!m_xTextDocument.is())
        {
            SAL_INFO("sw.ai", "INSERT_CHART: ERROR - No text document available");
            return "ERROR: No text document available";
        }
        
        // Get the text object from the document
        Reference<XText> xText = m_xTextDocument->getText();
        if (!xText.is())
        {
            SAL_INFO("sw.ai", "INSERT_CHART: ERROR - No text object available");
            return "ERROR: No text object available";
        }
        
        // Create cursor at end of document
        Reference<XTextCursor> xCursor = xText->createTextCursor();
        if (!xCursor.is())
        {
            SAL_INFO("sw.ai", "INSERT_CHART: ERROR - Cannot create cursor");
            return "ERROR: Cannot create cursor";
        }
        
        xCursor->gotoEnd(false);
        
        SAL_INFO("sw.ai", "INSERT_CHART: Inserting chart of type: " << rsChartType);
        
        // Get the service factory
        Reference<XMultiServiceFactory> xMSF(m_xTextDocument, UNO_QUERY);
        if (!xMSF.is())
        {
            SAL_INFO("sw.ai", "INSERT_CHART: ERROR - Cannot get service factory");
            return "ERROR: Cannot get service factory";
        }
        
        try
        {
            // Create an embedded object for the chart
            Reference<XTextContent> xTextContent(
                xMSF->createInstance("com.sun.star.text.TextEmbeddedObject"), 
                UNO_QUERY);
                
            if (!xTextContent.is())
            {
                SAL_INFO("sw.ai", "INSERT_CHART: ERROR - Cannot create embedded object");
                return "ERROR: Cannot create embedded object";
            }
            
            // Get the embedded object supplier interface
            Reference<XEmbeddedObjectSupplier> xEmbeddedObjSupplier(xTextContent, UNO_QUERY);
            if (!xEmbeddedObjSupplier.is())
            {
                SAL_INFO("sw.ai", "INSERT_CHART: ERROR - Cannot get embedded object supplier");
                return "ERROR: Cannot get embedded object supplier";
            }
            
            // Set the CLSID for chart
            Reference<XPropertySet> xProps(xTextContent, UNO_QUERY);
            if (xProps.is())
            {
                // Set the chart CLSID
                xProps->setPropertyValue("CLSID", Any(OUString("12dcae26-281f-416f-a234-c3086127382e")));
                
                // Set a reasonable default size for the chart (in 100th mm)
                awt::Size aSize;
                aSize.Width = 10000;  // 10 cm
                aSize.Height = 7500;  // 7.5 cm
                
                // Try to set the size
                try
                {
                    xProps->setPropertyValue("Width", Any(aSize.Width));
                    xProps->setPropertyValue("Height", Any(aSize.Height));
                }
                catch (...)
                {
                    SAL_INFO("sw.ai", "INSERT_CHART: Could not set chart size");
                }
            }
            
            // Insert the embedded object into the document
            xText->insertTextContent(xCursor, xTextContent, false);
            
            // Get the actual embedded object
            Reference<XComponent> xComponent = xEmbeddedObjSupplier->getEmbeddedObject();
            Reference<XEmbeddedObject> xEmbeddedObj(xComponent, UNO_QUERY);
            if (xEmbeddedObj.is())
            {
                SAL_INFO("sw.ai", "INSERT_CHART: Embedded object created successfully");
                
                // Try to initialize the chart with the requested type
                try
                {
                    // Switch to running state to access the chart
                    xEmbeddedObj->changeState(embed::EmbedStates::RUNNING);
                    
                    // Get the chart document
                    Reference<chart2::XChartDocument> xChartDoc(xEmbeddedObj->getComponent(), UNO_QUERY);
                    if (xChartDoc.is())
                    {
                        SAL_INFO("sw.ai", "INSERT_CHART: Got chart document, setting type to: " << rsChartType);
                        
                        try
                        {
                            // Simply try to set a title as a basic configuration
                            Reference<XPropertySet> xChartProps(xChartDoc, UNO_QUERY);
                            if (xChartProps.is())
                            {
                                // Try to set chart type through properties
                                OUString sChartTypeService;
                                if (rsChartType == "bar" || rsChartType == "column")
                                    sChartTypeService = "com.sun.star.chart.BarDiagram";
                                else if (rsChartType == "line")
                                    sChartTypeService = "com.sun.star.chart.LineDiagram";
                                else if (rsChartType == "pie")
                                    sChartTypeService = "com.sun.star.chart.PieDiagram";
                                else
                                    sChartTypeService = "com.sun.star.chart.BarDiagram";
                                
                                try
                                {
                                    xChartProps->setPropertyValue("DiagramType", Any(sChartTypeService));
                                    SAL_INFO("sw.ai", "INSERT_CHART: Set diagram type to: " << sChartTypeService);
                                }
                                catch (...)
                                {
                                    SAL_INFO("sw.ai", "INSERT_CHART: Could not set diagram type");
                                }
                                
                                // Try to set a title
                                try
                                {
                                    xChartProps->setPropertyValue("HasMainTitle", Any(true));
                                    
                                    Reference<XPropertySet> xTitleProps;
                                    xChartProps->getPropertyValue("Title") >>= xTitleProps;
                                    if (xTitleProps.is())
                                    {
                                        xTitleProps->setPropertyValue("String", Any(OUString("Sample Chart")));
                                    }
                                }
                                catch (...)
                                {
                                    SAL_INFO("sw.ai", "INSERT_CHART: Could not set chart title");
                                }
                            }
                        }
                        catch (const uno::Exception& e)
                        {
                            SAL_INFO("sw.ai", "INSERT_CHART: Could not configure chart: " << e.Message);
                            // Continue anyway - chart is inserted even if we couldn't configure it
                        }
                    }
                    
                    // Switch back to loaded state
                    xEmbeddedObj->changeState(embed::EmbedStates::LOADED);
                }
                catch (const uno::Exception& e)
                {
                    SAL_INFO("sw.ai", "INSERT_CHART: Could not configure chart type: " << e.Message);
                    // Continue anyway - chart is inserted even if we couldn't set the type
                }
            }
            
            SAL_INFO("sw.ai", "INSERT_CHART: Operation completed successfully");
            return "SUCCESS: Chart inserted at end of document";
        }
        catch (const uno::Exception& e)
        {
            SAL_INFO("sw.ai", "INSERT_CHART: Exception creating chart: " << e.Message);
            
            // Fallback: insert a placeholder
            OUString sPlaceholder = "\n[CHART: " + rsChartType + " - Chart creation failed]\n";
            xText->insertString(xCursor, sPlaceholder, false);
            
            return "ERROR: Chart creation failed - " + e.Message;
        }
    }
    catch (const css::uno::Exception& e)
    {
        SAL_INFO("sw.ai", "INSERT_CHART: UNO Exception - " << e.Message);
        return "ERROR: UNO Exception - " + e.Message;
    }
    catch (const std::exception& e)
    {
        SAL_INFO("sw.ai", "INSERT_CHART: std::exception - " << e.what());
        return "ERROR: std::exception - " + OUString::createFromAscii(e.what());
    }
    catch (...)
    {
        SAL_INFO("sw.ai", "INSERT_CHART: Unknown exception occurred");
        return "ERROR: Unknown exception";
    }
}



//=============================================================================
// Internal Implementation Methods
//=============================================================================

bool DocumentOperations::ensureDocumentAccess()
{
    if (!m_bInitialized)
    {
        SAL_INFO("sw.ai", "DocumentOperations not initialized");
        return false;
    }
    
    if (!m_xTextDocument.is())
    {
        SAL_INFO("sw.ai", "No text document available");
        return false;
    }
    
    return true;
}

void DocumentOperations::releaseDocumentAccess()
{
    // Clear any cached pointers - using UNO services now
    m_pWrtShell = nullptr;
    m_pEditShell = nullptr;
    m_pDoc = nullptr;
    m_pView = nullptr;
}

OUString DocumentOperations::generateOperationId()
{
    ++m_nOperationCounter;
    return "op_" + OUString::number(m_nOperationCounter);
}

// Static factory method
Reference<XInterface> SAL_CALL DocumentOperations::create(
    const Reference<XComponentContext>& xContext)
{
    return static_cast<cppu::OWeakObject*>(new DocumentOperations(xContext));
}

} // namespace sw::core::ai::operations

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */