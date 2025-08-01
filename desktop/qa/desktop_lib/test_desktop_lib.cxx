/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config_oox.h>
#include <memory>
#include <string_view>

#include <com/sun/star/lang/XComponent.hpp>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/awt/Key.hpp>
#include <com/sun/star/awt/XReschedule.hpp>
#include <com/sun/star/awt/Toolkit.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <com/sun/star/text/TextContentAnchorType.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/util/XCloseable.hpp>

#include <vcl/scheduler.hxx>
#include <vcl/svapp.hxx>
#include <vcl/syswin.hxx>
#include <vcl/window.hxx>
#include <vcl/ctrl.hxx>
#include <vcl/uitest/uiobject.hxx>
#include <comphelper/processfactory.hxx>
#include <rtl/math.hxx>
#include <sfx2/childwin.hxx>
#include <sfx2/lokhelper.hxx>
#include <test/unoapi_test.hxx>
#include <comphelper/lok.hxx>
#include <comphelper/propertysequence.hxx>
#include <osl/conditn.hxx>
#include <svl/srchitem.hxx>
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <unotools/tempfile.hxx>
#include <sfx2/viewsh.hxx>
#include <sfx2/viewfrm.hxx>
#include <sfx2/bindings.hxx>
#include <unotools/datetime.hxx>
#include <unotools/syslocaleoptions.hxx>
#include <comphelper/string.hxx>
#include <comphelper/scopeguard.hxx>
#include <cairo.h>
#include <config_fonts.h>
#include <config_mpl.h>
#include <tools/json_writer.hxx>
#include <o3tl/unit_conversion.hxx>
#include <o3tl/string_view.hxx>

#include <lib/init.hxx>
#include <svx/svxids.hrc>

#include <cppunit/TestAssert.h>
#include <vcl/BitmapTools.hxx>
#include <vcl/filter/PngImageWriter.hxx>
#include <vcl/filter/PDFiumLibrary.hxx>
#include <svtools/colorcfg.hxx>
#include <sal/types.h>
#include <test/lokcallback.hxx>

#if USE_TLS_NSS
#include <nss.h>
#endif

using namespace com::sun::star;
using namespace desktop;

static LibreOfficeKitDocumentType getDocumentTypeFromName(std::string_view name)
{
    CPPUNIT_ASSERT_MESSAGE("Document name must include extension.", name.size() > 4);

    const auto it = name.rfind('.');
    CPPUNIT_ASSERT_MESSAGE("Document name must include extension.", it != std::string::npos);
    const std::string_view ext = name.substr(it);

    if (ext == ".ods")
        return LOK_DOCTYPE_SPREADSHEET;

    if (ext == ".odp")
        return LOK_DOCTYPE_PRESENTATION;

    return LOK_DOCTYPE_TEXT;
}

class DesktopLOKTest : public UnoApiTest
{
public:
    DesktopLOKTest() : UnoApiTest(u"/desktop/qa/data/"_ustr),
    m_nSelectionBeforeSearchResult(0),
    m_nSelectionAfterSearchResult(0),
    m_bModified(false),
    m_nTrackChanges(0)
    {
    }
    ~DesktopLOKTest();

    void readFileIntoByteVector(
        std::u16string_view sFilename, std::vector<sal_uInt8> & rByteVector);

    virtual void setUp() override
    {
        comphelper::LibreOfficeKit::setActive(true);

        UnoApiTest::setUp();
    }

    virtual void tearDown() override
    {
        closeDoc();

        // documents are already closed, no need to call UnoApiTest::tearDown
        test::BootstrapFixture::tearDown();

        comphelper::LibreOfficeKit::setActive(false);
    }

    std::unique_ptr<LibLODocument_Impl>
    loadDocImpl(const char* pName, LibreOfficeKitDocumentType eType);

private:
    std::unique_ptr<LibLODocument_Impl>
    loadDocImpl(const char* pName);

public:
    std::unique_ptr<LibLODocument_Impl>
    loadDocUrlImpl(const OUString& rFileURL, LibreOfficeKitDocumentType eType);

    LibLODocument_Impl* loadDocUrl(const OUString& rFileURL, LibreOfficeKitDocumentType eType);
    LibLODocument_Impl* loadDoc(const char* pName, LibreOfficeKitDocumentType eType);
    LibLODocument_Impl* loadDoc(const char* pName)
    {
        return loadDoc(pName, getDocumentTypeFromName(pName));
    }

    void closeDoc(std::unique_ptr<LibLODocument_Impl>& loDocument);
    void closeDoc() { closeDoc(m_pDocument); }
    static void callback(int nType, const char* pPayload, void* pData);
    void callbackImpl(int nType, const char* pPayload);

    void testGetStyles();
    void testGetFonts();
    void testCreateView();
    void testGetFilterTypes();
    void testGetPartPageRectangles();
    void testSearchCalc();
    void testSearchAllNotificationsCalc();
    void testPaintTile();
    void testSaveAs();
    void testSaveAsJsonOptions();
    void testSaveAsCalc();
    void testPasteWriter();
    void testPasteWriterJPEG();
    void testUndoWriter();
    void testRowColumnHeaders();
    void testHiddenRowHeaders();
    void testCellCursor();
    void testCommandResult();
    void testWriterComments();
    void testSheetOperations();
    void testSheetSelections();
    void testSheetDragDrop();
    void testContextMenuCalc();
    void testContextMenuWriter();
    void testContextMenuImpress();
    void testNotificationCompression();
    void testTileInvalidationCompression();
    void testPartInInvalidation();
    void testBinaryCallback();
    void testOmitInvalidate();
    void test2ViewsOmitInvalidate();
    void testPaintTileOmitInvalidate();
    void testCreateViewOmitInvalidate();
    void testInput();
    void testRedlineWriter();
    void testRedlineCalc();
    void testPaintPartTile();
    void testPaintPartTileDifferentSchemes();
#if HAVE_MORE_FONTS
    void testGetFontSubset();
#endif
    void testCommentsWriter();
    void testCommentsCalc();
    void testCommentsImpress();
    void testCommentsCallbacksWriter();
    void testCommentsAddEditDeleteDraw();
    void testCommentsInReadOnlyMode();
    void testRedlinesInReadOnlyMode();
    void testCalcValidityDropdown();
    void testCalcValidityDropdownInReadonlyMode();
    void testRunMacro();
    void testExtractParameter();
    void testGetSignatureState_NonSigned();
    void testGetSignatureState_Signed();
#if 0 // broken with system nss on RHEL 7
    void testInsertCertificate_DER_ODT();
    void testInsertCertificate_PEM_ODT();
    void testInsertCertificate_PEM_DOCX();
#endif
    void testSignDocument_PEM_PDF();
    void testTextSelectionHandles();
    void testComplexSelection();
    void testSpellcheckerMultiView();
    void testDialogPaste();
    void testCalcSaveAs();
    void testControlState();
    void testMetricField();
    void testMultiDocuments();
    void testJumpCursor();
    void testRenderSearchResult_WriterNode();
    void testRenderSearchResult_CommonNode();
    void testNoDuplicateTableSelection();
    void testMultiViewTableSelection();
    void testColorPaletteCallback();
    void testABI();

    CPPUNIT_TEST_SUITE(DesktopLOKTest);
    CPPUNIT_TEST(testGetStyles);
    CPPUNIT_TEST(testGetFonts);
    CPPUNIT_TEST(testCreateView);
    CPPUNIT_TEST(testGetFilterTypes);
    CPPUNIT_TEST(testGetPartPageRectangles);
    CPPUNIT_TEST(testSearchCalc);
    CPPUNIT_TEST(testSearchAllNotificationsCalc);
    CPPUNIT_TEST(testPaintTile);
    CPPUNIT_TEST(testSaveAs);
    CPPUNIT_TEST(testSaveAsJsonOptions);
    CPPUNIT_TEST(testSaveAsCalc);
    CPPUNIT_TEST(testPasteWriter);
    CPPUNIT_TEST(testPasteWriterJPEG);
    CPPUNIT_TEST(testUndoWriter);
    CPPUNIT_TEST(testRowColumnHeaders);
    CPPUNIT_TEST(testHiddenRowHeaders);
    CPPUNIT_TEST(testCellCursor);
    CPPUNIT_TEST(testCommandResult);
    CPPUNIT_TEST(testWriterComments);
    CPPUNIT_TEST(testSheetOperations);
    CPPUNIT_TEST(testSheetSelections);
    CPPUNIT_TEST(testSheetDragDrop);
    CPPUNIT_TEST(testContextMenuCalc);
    CPPUNIT_TEST(testContextMenuWriter);
    CPPUNIT_TEST(testContextMenuImpress);
    CPPUNIT_TEST(testNotificationCompression);
    CPPUNIT_TEST(testTileInvalidationCompression);
    CPPUNIT_TEST(testPartInInvalidation);
    CPPUNIT_TEST(testBinaryCallback);
    CPPUNIT_TEST(testOmitInvalidate);
    CPPUNIT_TEST(test2ViewsOmitInvalidate);
    CPPUNIT_TEST(testPaintTileOmitInvalidate);
    CPPUNIT_TEST(testCreateViewOmitInvalidate);
    CPPUNIT_TEST(testInput);
    CPPUNIT_TEST(testRedlineWriter);
    CPPUNIT_TEST(testRedlineCalc);
    CPPUNIT_TEST(testPaintPartTile);
    CPPUNIT_TEST(testPaintPartTileDifferentSchemes);
#if HAVE_MORE_FONTS
    CPPUNIT_TEST(testGetFontSubset);
#endif
    CPPUNIT_TEST(testCommentsWriter);
    CPPUNIT_TEST(testCommentsCalc);
    CPPUNIT_TEST(testCommentsImpress);
    CPPUNIT_TEST(testCommentsCallbacksWriter);
    CPPUNIT_TEST(testCommentsAddEditDeleteDraw);
    CPPUNIT_TEST(testCommentsInReadOnlyMode);
    CPPUNIT_TEST(testRedlinesInReadOnlyMode);
    CPPUNIT_TEST(testCalcValidityDropdown);
    CPPUNIT_TEST(testCalcValidityDropdownInReadonlyMode);
    CPPUNIT_TEST(testRunMacro);
    CPPUNIT_TEST(testExtractParameter);
    CPPUNIT_TEST(testGetSignatureState_Signed);
    CPPUNIT_TEST(testGetSignatureState_NonSigned);
#if !MPL_HAVE_SUBSET
#if 0 // broken with system nss on RHEL 7
    CPPUNIT_TEST(testInsertCertificate_DER_ODT);
    CPPUNIT_TEST(testInsertCertificate_PEM_ODT);
    CPPUNIT_TEST(testInsertCertificate_PEM_DOCX);
#endif
    CPPUNIT_TEST(testSignDocument_PEM_PDF);
#endif
    CPPUNIT_TEST(testTextSelectionHandles);
    CPPUNIT_TEST(testComplexSelection);
    CPPUNIT_TEST(testSpellcheckerMultiView);
    CPPUNIT_TEST(testDialogPaste);
    CPPUNIT_TEST(testCalcSaveAs);
    CPPUNIT_TEST(testControlState);
    CPPUNIT_TEST(testMetricField);
    CPPUNIT_TEST(testMultiDocuments);
    CPPUNIT_TEST(testJumpCursor);
    CPPUNIT_TEST(testRenderSearchResult_WriterNode);
    CPPUNIT_TEST(testRenderSearchResult_CommonNode);
    CPPUNIT_TEST(testNoDuplicateTableSelection);
    CPPUNIT_TEST(testMultiViewTableSelection);
    CPPUNIT_TEST(testColorPaletteCallback);
    CPPUNIT_TEST(testABI);
    CPPUNIT_TEST_SUITE_END();

    OString m_aTextSelection;
    OString m_aTextSelectionStart;
    OString m_aTextSelectionEnd;
    std::vector<OString> m_aSearchResultSelection;
    std::vector<int> m_aSearchResultPart;
    int m_nSelectionBeforeSearchResult;
    int m_nSelectionAfterSearchResult;

    // for testCommandResult
    osl::Condition m_aCommandResultCondition;
    OString m_aCommandResult;

    // for testModifiedStatus
    osl::Condition m_aStateChangedCondition;
    bool m_bModified;
    int m_nTrackChanges;

    // for testContextMenu{Calc, Writer}
    osl::Condition m_aContextMenuCondition;
    boost::property_tree::ptree m_aContextMenuResult;

    std::unique_ptr<LibLODocument_Impl> m_pDocument;
};

DesktopLOKTest::~DesktopLOKTest()
{
#if USE_TLS_NSS
    NSS_Shutdown();
#endif
}

static Control* GetFocusControl(vcl::Window const * pParent)
{
    sal_uInt16 nChildren = pParent->GetChildCount();
    for (sal_uInt16 nChild = 0; nChild < nChildren; ++nChild)
    {
        vcl::Window* pChild = pParent->GetChild( nChild );
        Control* pCtrl = dynamic_cast<Control*>(pChild);
        if (pCtrl && pCtrl->HasControlFocus())
            return pCtrl;

        Control* pSubCtrl = GetFocusControl( pChild );
        if (pSubCtrl)
            return pSubCtrl;
    }
    return nullptr;
}

std::unique_ptr<LibLODocument_Impl>
DesktopLOKTest::loadDocUrlImpl(const OUString& rFileURL, LibreOfficeKitDocumentType eType)
{
    OUString aService;
    switch (eType)
    {
    case LOK_DOCTYPE_TEXT:
        aService = "com.sun.star.text.TextDocument";
        break;
    case LOK_DOCTYPE_SPREADSHEET:
        aService = "com.sun.star.sheet.SpreadsheetDocument";
        break;
    case LOK_DOCTYPE_PRESENTATION:
        aService = "com.sun.star.presentation.PresentationDocument";
        break;
    default:
        CPPUNIT_ASSERT(false);
        break;
    }

    static int nDocumentIdCounter = 0;
    SfxViewShell::SetCurrentDocId(ViewShellDocId(nDocumentIdCounter));
    mxComponent = loadFromDesktop(rFileURL, aService);

    std::unique_ptr<LibLODocument_Impl> pDocument(new LibLODocument_Impl(mxComponent, nDocumentIdCounter));
    ++nDocumentIdCounter;

    return pDocument;
}

std::unique_ptr<LibLODocument_Impl>
DesktopLOKTest::loadDocImpl(const char* pName, LibreOfficeKitDocumentType eType)
{
    OUString aFileURL = createFileURL(OUString::createFromAscii(pName));
    return loadDocUrlImpl(aFileURL, eType);
}

std::unique_ptr<LibLODocument_Impl>
DesktopLOKTest::loadDocImpl(const char* pName)
{
    return loadDocImpl(pName, getDocumentTypeFromName(pName));
}

LibLODocument_Impl* DesktopLOKTest::loadDocUrl(const OUString& rFileURL, LibreOfficeKitDocumentType eType)
{
    m_pDocument = loadDocUrlImpl(rFileURL, eType);
    return m_pDocument.get();
}

LibLODocument_Impl* DesktopLOKTest::loadDoc(const char* pName, LibreOfficeKitDocumentType eType)
{
    m_pDocument = loadDocImpl(pName, eType);
    return m_pDocument.get();
}

void DesktopLOKTest::closeDoc(std::unique_ptr<LibLODocument_Impl>& pDocument)
{
    if (pDocument)
    {
        pDocument->pClass->registerCallback(pDocument.get(), nullptr, nullptr);
        pDocument.reset();
    }

    if (mxComponent.is())
    {
        css::uno::Reference<util::XCloseable> xCloseable(mxComponent, css::uno::UNO_QUERY_THROW);
        xCloseable->close(false);
        mxComponent.clear();
    }
}

void DesktopLOKTest::callback(int nType, const char* pPayload, void* pData)
{
    static_cast<DesktopLOKTest*>(pData)->callbackImpl(nType, pPayload);
}

void DesktopLOKTest::callbackImpl(int nType, const char* pPayload)
{
    switch (nType)
    {
    case LOK_CALLBACK_TEXT_SELECTION:
    {
        m_aTextSelection = pPayload;
        if (m_aSearchResultSelection.empty())
            ++m_nSelectionBeforeSearchResult;
        else
            ++m_nSelectionAfterSearchResult;
    }
    break;
    case LOK_CALLBACK_TEXT_SELECTION_START:
        m_aTextSelectionStart = pPayload;
    break;
    case LOK_CALLBACK_TEXT_SELECTION_END:
        m_aTextSelectionEnd = pPayload;
    break;
    case LOK_CALLBACK_SEARCH_RESULT_SELECTION:
    {
        m_aSearchResultSelection.clear();
        boost::property_tree::ptree aTree;
        std::stringstream aStream(pPayload);
        boost::property_tree::read_json(aStream, aTree);
        for (const boost::property_tree::ptree::value_type& rValue : aTree.get_child("searchResultSelection"))
        {
            m_aSearchResultSelection.emplace_back(rValue.second.get<std::string>("rectangles").c_str());
            m_aSearchResultPart.push_back(std::atoi(rValue.second.get<std::string>("part").c_str()));
        }
    }
    break;
    case LOK_CALLBACK_UNO_COMMAND_RESULT:
    {
        m_aCommandResult = pPayload;
        m_aCommandResultCondition.set();
    }
    break;
    case LOK_CALLBACK_STATE_CHANGED:
    {
        OString aPayload(pPayload);
        OString aPrefix(".uno:ModifiedStatus="_ostr);
        if (aPayload.startsWith(aPrefix))
        {
            m_bModified = aPayload.copy(aPrefix.getLength()).toBoolean();
            m_aStateChangedCondition.set();
        }
        else if (aPayload.startsWith(".uno:TrackChanges=") && aPayload.endsWith("=true"))
            ++m_nTrackChanges;
    }
    break;
    case LOK_CALLBACK_CONTEXT_MENU:
    {
        m_aContextMenuResult.clear();
        std::stringstream aStream(pPayload);
        boost::property_tree::read_json(aStream, m_aContextMenuResult);
        m_aContextMenuCondition.set();
    }
    break;
    }
}

void DesktopLOKTest::testGetStyles()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:StyleApply");
    std::stringstream aStream(pJSON);
    boost::property_tree::read_json(aStream, aTree);
    CPPUNIT_ASSERT( !aTree.empty() );
    CPPUNIT_ASSERT_EQUAL( std::string(".uno:StyleApply"), aTree.get_child("commandName").get_value<std::string>()  );

    boost::property_tree::ptree aValues = aTree.get_child("commandValues");
    CPPUNIT_ASSERT( !aValues.empty() );
    for (const auto& rPair : aValues)
    {
        if( rPair.first != "ClearStyle")
        {
            CPPUNIT_ASSERT( !rPair.second.empty());
        }
        if (rPair.first != "CharacterStyles" &&
            rPair.first != "ParagraphStyles" &&
            rPair.first != "FrameStyles" &&
            rPair.first != "PageStyles" &&
            rPair.first != "NumberingStyles" &&
            rPair.first != "CellStyles" &&
            rPair.first != "ShapeStyles" &&
            rPair.first != "TableStyles" &&
            rPair.first != "HeaderFooter" &&
            rPair.first != "Commands")
        {
            CPPUNIT_FAIL("Unknown style family: " + rPair.first);
        }
    }
    free(pJSON);
}

void DesktopLOKTest::testGetFonts()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_presentation.odp");
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:CharFontName");
    std::stringstream aStream(pJSON);
    boost::property_tree::read_json(aStream, aTree);
    CPPUNIT_ASSERT( !aTree.empty() );
    CPPUNIT_ASSERT_EQUAL( std::string(".uno:CharFontName"), aTree.get_child("commandName").get_value<std::string>() );

    boost::property_tree::ptree aValues = aTree.get_child("commandValues");
    CPPUNIT_ASSERT( !aValues.empty() );
    for (const auto& rPair : aValues)
    {
        // check that we have font sizes available for each font
        CPPUNIT_ASSERT( !rPair.second.empty());
    }
    free(pJSON);
}

void DesktopLOKTest::testCreateView()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    CPPUNIT_ASSERT_EQUAL(1, pDocument->m_pDocumentClass->getViewsCount(pDocument));

    int nId0 = pDocument->m_pDocumentClass->getView(pDocument);
    int nId1 = pDocument->m_pDocumentClass->createView(pDocument);
    CPPUNIT_ASSERT_EQUAL(2, pDocument->m_pDocumentClass->getViewsCount(pDocument));

    // Test getViewIds().
    std::vector<int> aViewIds(2);
    CPPUNIT_ASSERT(pDocument->m_pDocumentClass->getViewIds(pDocument, aViewIds.data(), aViewIds.size()));
    CPPUNIT_ASSERT_EQUAL(nId0, aViewIds[0]);
    CPPUNIT_ASSERT_EQUAL(nId1, aViewIds[1]);

    // Make sure the created view is the active one, then switch to the old
    // one.
    CPPUNIT_ASSERT_EQUAL(nId1, pDocument->m_pDocumentClass->getView(pDocument));
    pDocument->m_pDocumentClass->setView(pDocument, nId0);
    CPPUNIT_ASSERT_EQUAL(nId0, pDocument->m_pDocumentClass->getView(pDocument));

    pDocument->m_pDocumentClass->destroyView(pDocument, nId1);
    CPPUNIT_ASSERT_EQUAL(1, pDocument->m_pDocumentClass->getViewsCount(pDocument));
}

void DesktopLOKTest::testGetPartPageRectangles()
{
    // Test that we get as many page rectangles as expected: blank document is
    // one page.
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    char* pRectangles = pDocument->pClass->getPartPageRectangles(pDocument);
    OUString sRectangles = OUString::fromUtf8(pRectangles);

    std::vector<OUString> aRectangles;
    sal_Int32 nIndex = 0;
    do
    {
        OUString aRectangle = sRectangles.getToken(0, ';', nIndex);
        if (!aRectangle.isEmpty())
            aRectangles.push_back(aRectangle);
    }
    while (nIndex >= 0);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aRectangles.size());

    free(pRectangles);
}

void DesktopLOKTest::testGetFilterTypes()
{
    LibLibreOffice_Impl aOffice;
    char* pJSON = aOffice.m_pOfficeClass->getFilterTypes(&aOffice);

    std::stringstream aStream(pJSON);
    boost::property_tree::ptree aTree;
    boost::property_tree::read_json(aStream, aTree);

    CPPUNIT_ASSERT(!aTree.empty());
    CPPUNIT_ASSERT_EQUAL(std::string("application/vnd.oasis.opendocument.text"), aTree.get_child("writer8").get_child("MediaType").get_value<std::string>());
    free(pJSON);
}

void DesktopLOKTest::testSearchCalc()
{
    LibLibreOffice_Impl aOffice;
    LibLODocument_Impl* pDocument = loadDoc("search.ods");
    pDocument->pClass->initializeForRendering(pDocument, nullptr);
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    uno::Sequence<beans::PropertyValue> aPropertyValues(comphelper::InitPropertySequence(
    {
        {"SearchItem.SearchString", uno::Any(u"foo"_ustr)},
        {"SearchItem.Backward", uno::Any(false)},
        {"SearchItem.Command", uno::Any(static_cast<sal_uInt16>(SvxSearchCmd::FIND_ALL))},
    }));
    dispatchCommand(mxComponent, u".uno:ExecuteSearch"_ustr, aPropertyValues);

    std::vector<OString> aSelections;
    sal_Int32 nIndex = 0;
    do
    {
        OString aToken = m_aTextSelection.getToken(0, ';', nIndex);
        aSelections.push_back(aToken);
    } while (nIndex >= 0);
    // This was 1, find-all only found one match.
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aSelections.size());
    // Make sure that we get exactly as many rectangle lists as matches.
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), m_aSearchResultSelection.size());
    // Result is on the first sheet.
    CPPUNIT_ASSERT_EQUAL(0, m_aSearchResultPart[0]);
}

void DesktopLOKTest::testSearchAllNotificationsCalc()
{
    LibLibreOffice_Impl aOffice;
    LibLODocument_Impl* pDocument = loadDoc("search.ods");
    pDocument->pClass->initializeForRendering(pDocument, nullptr);
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    uno::Sequence<beans::PropertyValue> aPropertyValues(comphelper::InitPropertySequence(
    {
        {"SearchItem.SearchString", uno::Any(u"foo"_ustr)},
        {"SearchItem.Backward", uno::Any(false)},
        {"SearchItem.Command", uno::Any(static_cast<sal_uInt16>(SvxSearchCmd::FIND_ALL))},
    }));
    dispatchCommand(mxComponent, u".uno:ExecuteSearch"_ustr, aPropertyValues);

    // This was 1, make sure that we get no notifications about selection changes during search.
    CPPUNIT_ASSERT_EQUAL(0, m_nSelectionBeforeSearchResult);
    // But we do get the selection afterwards.
    CPPUNIT_ASSERT(m_nSelectionAfterSearchResult > 0);
}

void DesktopLOKTest::testPaintTile()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    int nCanvasWidth = 100;
    int nCanvasHeight = 300;
    sal_Int32 nStride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, nCanvasWidth);
    std::vector<unsigned char> aBuffer(nStride * nCanvasHeight);
    int nTilePosX = 0;
    int nTilePosY = 0;
    int nTileWidth = 1000;
    int nTileHeight = 3000;

    // This used to crash: paintTile() implementation did not handle
    // nCanvasWidth != nCanvasHeight correctly, as usually both are just always
    // 256.
    pDocument->pClass->paintTile(pDocument, aBuffer.data(), nCanvasWidth, nCanvasHeight, nTilePosX, nTilePosY, nTileWidth, nTileHeight);

    // This crashed in OutputDevice::DrawDeviceAlphaBitmap().
    nCanvasWidth = 200;
    nCanvasHeight = 200;
    nTileWidth = 4000;
    nTileHeight = 4000;
    aBuffer.resize(nCanvasWidth * nCanvasHeight * 4);
    pDocument->pClass->paintTile(pDocument, aBuffer.data(), nCanvasWidth, nCanvasHeight, nTilePosX, nTilePosY, nTileWidth, nTileHeight);
}

void DesktopLOKTest::testSaveAs()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    CPPUNIT_ASSERT(pDocument->pClass->saveAs(pDocument, maTempFile.GetURL().toUtf8().getStr(), "png", nullptr));
}

void DesktopLOKTest::testSaveAsJsonOptions()
{
    // Given a document with 3 pages:
    LibLODocument_Impl* pDocument = loadDoc("3page.odg");

    // When exporting that document to PDF, skipping the first page:
    OString aOptions("{\"PageRange\":{\"type\":\"string\",\"value\":\"2-\"}}"_ostr);
    CPPUNIT_ASSERT(pDocument->pClass->saveAs(pDocument, maTempFile.GetURL().toUtf8().getStr(), "pdf", aOptions.getStr()));

    std::shared_ptr<vcl::pdf::PDFium> pPDFium = vcl::pdf::PDFiumLibrary::get();
    if (!pPDFium)
        return;

    // Then make sure the resulting PDF has 2 pages:
    std::unique_ptr<vcl::pdf::PDFiumDocument> pPdfDocument
        = parsePDFExport();
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 2
    // - Actual  : 3
    // i.e. FilterOptions was ignored.
    CPPUNIT_ASSERT_EQUAL(2, pPdfDocument->getPageCount());
}

void DesktopLOKTest::testSaveAsCalc()
{
    LibLODocument_Impl* pDocument = loadDoc("search.ods");
    CPPUNIT_ASSERT(pDocument->pClass->saveAs(pDocument, maTempFile.GetURL().toUtf8().getStr(), "png", nullptr));
}

void DesktopLOKTest::testPasteWriter()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    OString aText("hello"_ostr);

    CPPUNIT_ASSERT(pDocument->pClass->paste(pDocument, "text/plain;charset=utf-8", aText.getStr(), aText.getLength()));

    pDocument->pClass->postUnoCommand(pDocument, ".uno:SelectAll", nullptr, false);
    Scheduler::ProcessEventsToIdle();
    char* pText = pDocument->pClass->getTextSelection(pDocument, "text/plain;charset=utf-8", nullptr);
    CPPUNIT_ASSERT_EQUAL("hello"_ostr, OString(pText));
    free(pText);

    // textt/plain should be rejected.
    CPPUNIT_ASSERT(!pDocument->pClass->paste(pDocument, "textt/plain;charset=utf-8", aText.getStr(), aText.getLength()));
    // Writer is expected to support text/html.
    CPPUNIT_ASSERT(pDocument->pClass->paste(pDocument, "text/html", aText.getStr(), aText.getLength()));

    // Overwrite doc contents with a HTML paste.
    pDocument->pClass->postUnoCommand(pDocument, ".uno:SelectAll", nullptr, false);
    Scheduler::ProcessEventsToIdle();
    OString aComment("foo <!-- bar --> baz"_ostr);
    CPPUNIT_ASSERT(pDocument->pClass->paste(pDocument, "text/html", aComment.getStr(), aComment.getLength()));

    // Check if we have a comment.
    uno::Reference<text::XTextDocument> xTextDocument(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XEnumerationAccess> xParagraphEnumerationAccess(xTextDocument->getText(), uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xParagraphEnumeration = xParagraphEnumerationAccess->createEnumeration();
    uno::Reference<container::XEnumerationAccess> xParagraph(xParagraphEnumeration->nextElement(), uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xTextPortionEnumeration = xParagraph->createEnumeration();
    uno::Reference<beans::XPropertySet> xTextPortion(xTextPortionEnumeration->nextElement(), uno::UNO_QUERY);
    CPPUNIT_ASSERT_EQUAL(u"Text"_ustr, xTextPortion->getPropertyValue(u"TextPortionType"_ustr).get<OUString>());
    // Without the accompanying fix in place, this test would have failed, as we had a comment
    // between "foo" and "baz".
    CPPUNIT_ASSERT(!xTextPortionEnumeration->hasMoreElements());
}

void DesktopLOKTest::testPasteWriterJPEG()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");

    OUString aFileURL = createFileURL(u"paste.jpg");
    std::ifstream aImageStream(aFileURL.toUtf8().copy(strlen("file://")).getStr());
    std::vector<char> aImageContents((std::istreambuf_iterator<char>(aImageStream)), std::istreambuf_iterator<char>());

    CPPUNIT_ASSERT(pDocument->pClass->paste(pDocument, "image/jpeg", aImageContents.data(), aImageContents.size()));

    uno::Reference<drawing::XDrawPageSupplier> xDrawPageSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<drawing::XDrawPage> xDrawPage = xDrawPageSupplier->getDrawPage();
    // This was 0, JPEG was not handled as a format for clipboard paste.
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(1), xDrawPage->getCount());

    uno::Reference<beans::XPropertySet> xShape(xDrawPage->getByIndex(0), uno::UNO_QUERY);
    // This was text::TextContentAnchorType_AT_PARAGRAPH.
    CPPUNIT_ASSERT_EQUAL(text::TextContentAnchorType_AS_CHARACTER, xShape->getPropertyValue(u"AnchorType"_ustr).get<text::TextContentAnchorType>());

    // Delete the pasted picture, and paste again with a custom anchor type.
    uno::Reference<lang::XComponent>(xShape, uno::UNO_QUERY_THROW)->dispose();
    uno::Sequence<beans::PropertyValue> aPropertyValues(comphelper::InitPropertySequence(
    {
        {"AnchorType", uno::Any(static_cast<sal_uInt16>(text::TextContentAnchorType_AT_CHARACTER))},
    }));
    dispatchCommand(mxComponent, u".uno:Paste"_ustr, aPropertyValues);
    xShape.set(xDrawPage->getByIndex(0), uno::UNO_QUERY);
    // This was text::TextContentAnchorType_AS_CHARACTER, AnchorType argument was ignored.
    CPPUNIT_ASSERT_EQUAL(text::TextContentAnchorType_AT_CHARACTER, xShape->getPropertyValue(u"AnchorType"_ustr).get<text::TextContentAnchorType>());
}

void DesktopLOKTest::testUndoWriter()
{
    // Load a Writer document and press a key.
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 't', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 't', 0);
    Scheduler::ProcessEventsToIdle();
    // Get undo info.
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:Undo");
    std::stringstream aStream(pJSON);
    free(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());
    boost::property_tree::read_json(aStream, aTree);
    // Make sure that pressing a key creates exactly one undo action.
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aTree.get_child("actions").size());
}

void DesktopLOKTest::testRowColumnHeaders()
{
    /*
     * Payload example:
     *
     * {
     *     "rows": [
     *         {
     *             "size": "254.987250637468",
     *             "text": "1"
     *         },
     *         {
     *             "size": "509.974501274936",
     *             "text": "2"
     *         }
     *     ],
     *     "columns": [
     *         {
     *             "size": "1274.93625318734",
     *             "text": "A"
     *         },
     *         {
     *             "size": "2549.87250637468",
     *             "text": "B"
     *         }
     *     ]
     * }
     *
     * "size" defines the bottom/right boundary of a row/column in twips (size between 0 and boundary)
     * "text" has the header label in UTF-8
     */
    LibLODocument_Impl* pDocument = loadDoc("search.ods");

    pDocument->pClass->initializeForRendering(pDocument, nullptr);

    tools::Long nWidth = 0;
    tools::Long nHeight = 0;
    pDocument->m_pDocumentClass->getDocumentSize(pDocument, &nWidth, &nHeight);
    tools::Long nX = rtl::math::round(nWidth / 4.0);
    tools::Long nY = rtl::math::round(nHeight / 4.0);
    nWidth = rtl::math::round(nWidth / 2.0);
    nHeight = rtl::math::round(nHeight / 2.0);

    std::stringstream aPayload;
    aPayload << ".uno:ViewRowColumnHeaders?x=" << nX << "&y=" << nY << "&width=" << nWidth << "&height=" << nHeight;

    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, aPayload.str().c_str());
    std::stringstream aStream(pJSON);
    free(pJSON);

    CPPUNIT_ASSERT(!aStream.str().empty());

    boost::property_tree::read_json(aStream, aTree);
    sal_Int32 nPrevious = 0;
    bool bFirstHeader = true;
    bool bNotEnoughHeaders = true;
    for (const boost::property_tree::ptree::value_type& rValue : aTree.get_child("rows"))
    {
        sal_Int32 nSize = o3tl::toInt32(rValue.second.get<std::string>("size"));
        nSize = o3tl::convert(nSize, o3tl::Length::px, o3tl::Length::twip);
        OString aText(rValue.second.get<std::string>("text"));

        if (bFirstHeader)
        {
            CPPUNIT_ASSERT(nSize <= nY);
            CPPUNIT_ASSERT_EQUAL("10"_ostr, aText);
            bFirstHeader = false;
        }
        else
        {
            CPPUNIT_ASSERT(nSize > 0);
            CPPUNIT_ASSERT(nPrevious < nSize);
            if (nSize > nY + nHeight)
            {
                bNotEnoughHeaders = false;
                break;
            }
        }
        nPrevious = nSize;
    }
    CPPUNIT_ASSERT(!bNotEnoughHeaders);

    nPrevious = 0;
    bFirstHeader = true;
    bNotEnoughHeaders = true;
    for (const boost::property_tree::ptree::value_type& rValue : aTree.get_child("columns"))
    {
        sal_Int32 nSize = o3tl::toInt32(rValue.second.get<std::string>("size"));
        nSize = o3tl::convert(nSize, o3tl::Length::px, o3tl::Length::twip);
        OString aText(rValue.second.get<std::string>("text"));
        if (bFirstHeader)
        {
            CPPUNIT_ASSERT(nSize <= nX);
            CPPUNIT_ASSERT_EQUAL("3"_ostr, aText);
            bFirstHeader = false;
        }
        else
        {
            CPPUNIT_ASSERT(nSize > 0);
            CPPUNIT_ASSERT(nPrevious < nSize);
            if (nSize > nX + nWidth)
            {
                bNotEnoughHeaders = false;
                break;
            }
        }
        nPrevious = nSize;
    }
    CPPUNIT_ASSERT(!bNotEnoughHeaders);
}

void DesktopLOKTest::testHiddenRowHeaders()
{
    LibLODocument_Impl* pDocument = loadDoc("hidden-row.ods");

    pDocument->pClass->initializeForRendering(pDocument, nullptr);

    tools::Long const nX = 0;
    tools::Long const nY = 0;
    tools::Long nWidth = 0;
    tools::Long nHeight = 0;
    pDocument->m_pDocumentClass->getDocumentSize(pDocument, &nWidth, &nHeight);

    std::stringstream aPayload;
    aPayload << ".uno:ViewRowColumnHeaders?x=" << nX << "&y=" << nY << "&width=" << nWidth << "&height=" << nHeight;

    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, aPayload.str().c_str());
    std::stringstream aStream(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());

    boost::property_tree::read_json(aStream, aTree);
    free(pJSON);
    sal_Int32 nPrevious = 0;
    sal_Int32 nIndex = 0;
    for (const boost::property_tree::ptree::value_type& rValue : aTree.get_child("rows"))
    {
        sal_Int32 nSize = o3tl::toInt32(rValue.second.get<std::string>("size"));

        if (nIndex++ == 2)
        {
            // nSize was 510, nPrevious was 255, i.e. hidden row wasn't reported as 0 height.
            CPPUNIT_ASSERT_EQUAL(nPrevious, nSize);
            break;
        }
        nPrevious = nSize;
    }
}

void DesktopLOKTest::testCellCursor()
{
    LibLODocument_Impl* pDocument = loadDoc("search.ods");

    boost::property_tree::ptree aTree;

    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:CellCursor?tileWidth=1&tileHeight=1&outputWidth=1&outputHeight=1");

    std::stringstream aStream(pJSON);
    free(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());

    boost::property_tree::read_json(aStream, aTree);

    OString aRectangle(aTree.get<std::string>("commandValues"));
    // cell cursor geometry + col + row
    CPPUNIT_ASSERT_EQUAL("0, 0, 1274, 254, 0, 0"_ostr, aRectangle);
}

void DesktopLOKTest::testCommandResult()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");

    // the postUnoCommand() is supposed to be async, let's test it safely
    // [no idea if it is async in reality - most probably we are operating
    // under some solar mutex or something anyway ;-) - but...]
    TimeValue aTimeValue = { 2 , 0 }; // 2 seconds max

    // nothing is triggered when we have no callback yet, we just time out on
    // the condition var.
    m_aCommandResultCondition.reset();
    pDocument->pClass->postUnoCommand(pDocument, ".uno:Bold", nullptr, true);
    Scheduler::ProcessEventsToIdle();
    m_aCommandResultCondition.wait(aTimeValue);

    CPPUNIT_ASSERT(m_aCommandResult.isEmpty());

    // but we get some real values when the callback is set up
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    m_aCommandResultCondition.reset();
    pDocument->pClass->postUnoCommand(pDocument, ".uno:Bold", nullptr, true);
    Scheduler::ProcessEventsToIdle();
    m_aCommandResultCondition.wait(aTimeValue);

    boost::property_tree::ptree aTree;
    std::stringstream aStream((std::string(m_aCommandResult)));
    boost::property_tree::read_json(aStream, aTree);

    CPPUNIT_ASSERT_EQUAL(std::string(".uno:Bold"), aTree.get_child("commandName").get_value<std::string>());
    CPPUNIT_ASSERT_EQUAL(true, aTree.get_child("success").get_value<bool>());
}

void DesktopLOKTest::testWriterComments()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);
    uno::Reference<awt::XReschedule> xToolkit = css::awt::Toolkit::create(comphelper::getProcessComponentContext());

    // Insert a comment at the beginning of the document and wait till the main
    // loop grabs the focus, so characters end up in the annotation window.
    TimeValue const aTimeValue = {2 , 0}; // 2 seconds max
    m_aCommandResultCondition.reset();
    pDocument->pClass->postUnoCommand(pDocument, ".uno:InsertAnnotation", nullptr, true);
    Scheduler::ProcessEventsToIdle();
    m_aCommandResultCondition.wait(aTimeValue);
    CPPUNIT_ASSERT(!m_aCommandResult.isEmpty());
    xToolkit->reschedule();

    // Test that we have a comment.
    uno::Reference<text::XTextDocument> xTextDocument(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XEnumerationAccess> xParagraphEnumerationAccess(xTextDocument->getText(), uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xParagraphEnumeration = xParagraphEnumerationAccess->createEnumeration();
    uno::Reference<container::XEnumerationAccess> xParagraph(xParagraphEnumeration->nextElement(), uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xTextPortionEnumeration = xParagraph->createEnumeration();
    uno::Reference<beans::XPropertySet> xTextPortion(xTextPortionEnumeration->nextElement(), uno::UNO_QUERY);
    CPPUNIT_ASSERT_EQUAL(u"Annotation"_ustr, xTextPortion->getPropertyValue(u"TextPortionType"_ustr).get<OUString>());

    // Type "test" and finish editing.
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 't', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'e', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 's', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 't', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, css::awt::Key::ESCAPE);
    Scheduler::ProcessEventsToIdle();

    // Test that the typed characters ended up in the right window.
    auto xTextField = xTextPortion->getPropertyValue(u"TextField"_ustr).get< uno::Reference<beans::XPropertySet> >();
    // This was empty, typed characters ended up in the body text.
    CPPUNIT_ASSERT_EQUAL(u"test"_ustr, xTextField->getPropertyValue(u"Content"_ustr).get<OUString>());
}

void DesktopLOKTest::testSheetOperations()
{
    LibLODocument_Impl* pDocument = loadDoc("sheets.ods");

    // insert the last sheet
    pDocument->pClass->postUnoCommand(pDocument, ".uno:Insert",
          "{ \"Name\": { \"type\": \"string\", \"value\": \"LastSheet\" }, \"Index\": { \"type\": \"long\", \"value\": 0 } }", false);

    // insert the first sheet
    pDocument->pClass->postUnoCommand(pDocument, ".uno:Insert",
          "{ \"Name\": { \"type\": \"string\", \"value\": \"FirstSheet\" }, \"Index\": { \"type\": \"long\", \"value\": 1 } }", false);

    // rename the \"Sheet1\" (2nd now) to \"Renamed\"
    pDocument->pClass->postUnoCommand(pDocument, ".uno:Name",
          "{ \"Name\": { \"type\": \"string\", \"value\": \"Renamed\" }, \"Index\": { \"type\": \"long\", \"value\": 2 } }", false);

    // delete the \"Sheet2\" (3rd)
    pDocument->pClass->postUnoCommand(pDocument, ".uno:Remove",
          "{ \"Index\": { \"type\": \"long\", \"value\": 3 } }", false);

    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(6, pDocument->pClass->getParts(pDocument));

    std::vector<OString> aExpected = { "FirstSheet"_ostr, "Renamed"_ostr, "Sheet3"_ostr, "Sheet4"_ostr, "Sheet5"_ostr, "LastSheet"_ostr };
    for (int i = 0; i < 6; ++i)
    {
        char* pPartName = pDocument->pClass->getPartName(pDocument, i);
        CPPUNIT_ASSERT_EQUAL(aExpected[i], OString(pPartName));
        free(pPartName);
    }
}

void DesktopLOKTest::testSheetSelections()
{
    LibLODocument_Impl* pDocument = loadDoc("sheets.ods", LOK_DOCTYPE_SPREADSHEET);
    pDocument->pClass->initializeForRendering(pDocument, nullptr);
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    /*
     * Check if selection data is correct
     */
    // Values in twips
    int row5 = 1150;
    int col1 = 1100;
    int const col2 = 2200;
    int const col3 = 3300;
    int col4 = 4400;
    int col5 = 5500;

    // Select row 5 from column 1 through column 5
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONDOWN,
                                      col1, row5,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEMOVE,
                                      col2, row5,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEMOVE,
                                      col3, row5,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEMOVE,
                                      col4, row5,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEMOVE,
                                      col5, row5,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONUP,
                                      col5, row5,
                                      1, 1, 0);
    Scheduler::ProcessEventsToIdle();

    // Copy the contents and check if matches expected data
    {
        char* pUsedMimeType = nullptr;
        char* pCopiedContent = pDocument->pClass->getTextSelection(pDocument, nullptr, &pUsedMimeType);
        std::vector<long> aExpected = {5, 6, 7, 8, 9};
        std::istringstream iss(pCopiedContent);
        for (const long nIndex : aExpected)
        {
            std::string token;
            iss >> token;
            CPPUNIT_ASSERT_EQUAL(nIndex, strtol(token.c_str(), nullptr, 10));
        }

        free(pUsedMimeType);
        free(pCopiedContent);
    }

    /*
     * Check if clicking inside the selection deselects the whole selection
     */

    // Click at row5, col4
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONDOWN,
                                      col4, row5,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONUP,
                                      col4, row5,
                                      1, 1, 0);
    Scheduler::ProcessEventsToIdle();

    // Selected text should get deselected and copying should give us
    // content of only one cell, now
    {
        char* pUsedMimeType  = nullptr;
        char* pCopiedContent = pDocument->pClass->getTextSelection(pDocument, nullptr, &pUsedMimeType);
        std::vector<long> aExpected = { 8 };
        std::istringstream iss(pCopiedContent);
        for (const long nIndex : aExpected)
        {
            std::string token;
            iss >> token;
            CPPUNIT_ASSERT_EQUAL(nIndex, strtol(token.c_str(), nullptr, 10));
        }

        free(pUsedMimeType);
        free(pCopiedContent);
    }
}

void DesktopLOKTest::testSheetDragDrop()
{
    LibLODocument_Impl* pDocument = loadDoc("sheets.ods", LOK_DOCTYPE_SPREADSHEET);
    pDocument->pClass->initializeForRendering(pDocument, nullptr);
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    int row01 = 100;
    int col01 = 1100;
    int col02 = 2200;
    int col03 = 3300;
    int col05 = 5500;
    int col07 = 5700;

    // Select row 01 from column 01 through column 05
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONDOWN,
                                      col01, row01,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEMOVE,
                                      col02, row01,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEMOVE,
                                      col05, row01,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONUP,
                                      col05, row01,
                                      1, 1, 0);

    Scheduler::ProcessEventsToIdle();
    {
        SfxViewShell* pViewShell = SfxViewShell::Current();
        SfxViewFrame& rViewFrame = pViewShell->GetViewFrame();

        OUString sValue;
        css::uno::Any aValue;
        css::util::URL aURL;
        std::unique_ptr<SfxPoolItem> pState;

        aURL.Protocol = ".uno:";
        aURL.Complete = ".uno:Address";
        aURL.Path = "Address";
        aURL.Main = ".uno:Address";

        rViewFrame.GetBindings().QueryState(rViewFrame.GetBindings().QuerySlotId(aURL), pState);
        pState->QueryValue(aValue);
        aValue >>= sValue;
        CPPUNIT_ASSERT_EQUAL(u"Sheet5.A1:E1"_ustr, sValue);
    }

    // Check selection content
    {
        char* pMimeType = nullptr;
        char* pContent = pDocument->pClass->getTextSelection(pDocument, nullptr, &pMimeType);
        std::vector<long> aExpected = {1, 2, 3, 4, 5};
        std::istringstream aContent(pContent);
        std::string token;
        for (const long nIndex : aExpected)
        {
            aContent >> token;
            CPPUNIT_ASSERT_EQUAL(nIndex, strtol(token.c_str(), nullptr, 10));
        }

        free(pMimeType);
        free(pContent);
    }

    // drag and drop
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONDOWN,
                                      col01, row01,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEMOVE,
                                      col02, row01,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEMOVE,
                                      col03, row01,
                                      1, 1, 0);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONUP,
                                      col07, row01,
                                      1, 1, 0);

    Scheduler::ProcessEventsToIdle();
    {
        SfxViewShell* pViewShell = SfxViewShell::Current();
        SfxViewFrame& rViewFrame = pViewShell->GetViewFrame();

        OUString sValue;
        css::uno::Any aValue;
        css::util::URL aURL;
        std::unique_ptr<SfxPoolItem> pState;

        aURL.Protocol = ".uno:";
        aURL.Complete = ".uno:Address";
        aURL.Path = "Address";
        aURL.Main = ".uno:Address";

        rViewFrame.GetBindings().QueryState(rViewFrame.GetBindings().QuerySlotId(aURL), pState);
        pState->QueryValue(aValue);
        aValue >>= sValue;
        CPPUNIT_ASSERT_EQUAL(u"Sheet5.D1:H1"_ustr, sValue);
    }

    // Check selection content
    {
        char* pMimeType = nullptr;
        char* pContent = pDocument->pClass->getTextSelection(pDocument, nullptr, &pMimeType);
        std::vector<long> aExpected = {1, 2, 3, 4, 5};
        std::istringstream aContent(pContent);
        std::string token;
        for (const long nIndex : aExpected)
        {
            aContent >> token;
            CPPUNIT_ASSERT_EQUAL(nIndex, strtol(token.c_str(), nullptr, 10));
        }

        free(pMimeType);
        free(pContent);
    }
}

namespace {

    void verifyContextMenuStructure(boost::property_tree::ptree& aRoot)
    {
        for (const auto& aItemPair: aRoot)
        {
            // This is an array, so no key
            CPPUNIT_ASSERT_EQUAL(aItemPair.first, std::string(""));

            boost::property_tree::ptree aItemValue = aItemPair.second;
            boost::optional<boost::property_tree::ptree&> aText = aItemValue.get_child_optional("text");
            boost::optional<boost::property_tree::ptree&> aType = aItemValue.get_child_optional("type");
            boost::optional<boost::property_tree::ptree&> aCommand = aItemValue.get_child_optional("command");
            boost::optional<boost::property_tree::ptree&> aSubmenu = aItemValue.get_child_optional("menu");
            boost::optional<boost::property_tree::ptree&> aEnabled = aItemValue.get_child_optional("enabled");
            boost::optional<boost::property_tree::ptree&> aChecktype = aItemValue.get_child_optional("checktype");
            boost::optional<boost::property_tree::ptree&> aChecked = aItemValue.get_child_optional("checked");

            // type is omnipresent
            CPPUNIT_ASSERT( aType );

            // separator doesn't have any other attribs
            if ( aType.get().data() == "separator" )
            {
                CPPUNIT_ASSERT( !aText );
                CPPUNIT_ASSERT( !aCommand );
                CPPUNIT_ASSERT( !aSubmenu );
                CPPUNIT_ASSERT( !aEnabled );
                CPPUNIT_ASSERT( !aChecktype );
                CPPUNIT_ASSERT( !aChecked );
            }
            else if ( aType.get().data() == "command" )
            {
                CPPUNIT_ASSERT( aCommand );
                CPPUNIT_ASSERT( aText );
            }
            else if ( aType.get().data() == "menu")
            {
                CPPUNIT_ASSERT( aSubmenu );
                CPPUNIT_ASSERT( aText );
                verifyContextMenuStructure( aSubmenu.get() );
            }

            if ( aChecktype )
            {
                CPPUNIT_ASSERT( aChecktype.get().data() == "radio" ||
                                aChecktype.get().data() == "checkmark" ||
                                aChecktype.get().data() == "auto" );

                CPPUNIT_ASSERT( aChecked );
                CPPUNIT_ASSERT( aChecked.get().data() == "true" || aChecked.get().data() == "false" );
            }
        }

    }

    boost::optional<boost::property_tree::ptree>
    getContextMenuItem(boost::property_tree::ptree& aMenu, std::string const & unoSelector)
    {
        boost::optional<boost::property_tree::ptree> aMenuItem;
        for (const auto& aItemPair: aMenu)
        {
            boost::property_tree::ptree aItemValue = aItemPair.second;

            boost::optional<boost::property_tree::ptree&> aCommand = aItemValue.get_child_optional("command");
            if (aCommand && aCommand.get().data() == unoSelector )
            {
                aMenuItem = aItemValue;
                break;
            }
        }

        return aMenuItem;
    }

} // end anonymous namespace

void DesktopLOKTest::testContextMenuCalc()
{
    LibLODocument_Impl* pDocument = loadDoc("sheet_with_image.ods", LOK_DOCTYPE_SPREADSHEET);
    pDocument->pClass->initializeForRendering(pDocument, nullptr);
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    // Values in twips
    Point aPointOnImage(1150, 1100);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONDOWN,
                                      aPointOnImage.X(), aPointOnImage.Y(),
                                      1, 4, 0);
    Scheduler::ProcessEventsToIdle();

    TimeValue const aTimeValue = {2 , 0}; // 2 seconds max
    m_aContextMenuCondition.wait(aTimeValue);

    CPPUNIT_ASSERT( !m_aContextMenuResult.empty() );
    boost::optional<boost::property_tree::ptree&> aMenu = m_aContextMenuResult.get_child_optional("menu");
    CPPUNIT_ASSERT( aMenu );
    verifyContextMenuStructure( aMenu.get() );

    // tests for calc specific context menu
    // Cut is enabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Cut");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("true"));
    }

    // Copy is enabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Copy");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("true"));
    }

    // Paste is enabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Paste");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("true"));
    }

    // Remove hyperlink is disabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:RemoveHyperlink");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("false"));
    }

    // open hyperlink is disabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:OpenHyperlinkOnCursor");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("false"));
    }

    // checkbutton tests
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:AnchorMenu");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aSubmenu = aMenuItem.get().get_child_optional("menu");
        CPPUNIT_ASSERT(aSubmenu);

        boost::optional<boost::property_tree::ptree> aMenuItemToPage = getContextMenuItem(aSubmenu.get(), ".uno:SetAnchorToPage");
        CPPUNIT_ASSERT(aMenuItemToPage);

        boost::optional<boost::property_tree::ptree> aMenuItemToCell = getContextMenuItem(aSubmenu.get(), ".uno:SetAnchorToCell");
        CPPUNIT_ASSERT(aMenuItemToCell);

        // these are radio buttons
        boost::optional<boost::property_tree::ptree&> aChecktypeToPage = aMenuItemToPage.get().get_child_optional("checktype");
        CPPUNIT_ASSERT(aChecktypeToPage);
        CPPUNIT_ASSERT_EQUAL(aChecktypeToPage.get().data(), std::string("radio"));

        boost::optional<boost::property_tree::ptree&> aChecktypeToCell = aMenuItemToCell.get().get_child_optional("checktype");
        CPPUNIT_ASSERT(aChecktypeToCell);
        CPPUNIT_ASSERT_EQUAL(aChecktypeToCell.get().data(), std::string("radio"));

        // ToPage is checked
        boost::optional<boost::property_tree::ptree&> aCheckedToPage = aMenuItemToPage.get().get_child_optional("checked");
        CPPUNIT_ASSERT(aCheckedToPage);
        CPPUNIT_ASSERT_EQUAL(aCheckedToPage.get().data(), std::string("true"));

        // ToCell is unchecked
        boost::optional<boost::property_tree::ptree&> aCheckedToCell = aMenuItemToCell.get().get_child_optional("checked");
        CPPUNIT_ASSERT(aCheckedToCell);
        CPPUNIT_ASSERT_EQUAL(aCheckedToCell.get().data(), std::string("false"));
    }
}

void DesktopLOKTest::testContextMenuWriter()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    pDocument->pClass->initializeForRendering(pDocument, nullptr);
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    Point aRandomPoint(1150, 1100);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONDOWN,
                                      aRandomPoint.X(), aRandomPoint.Y(),
                                      1, 4, 0);
    Scheduler::ProcessEventsToIdle();

    TimeValue const aTimeValue = {2 , 0}; // 2 seconds max
    m_aContextMenuCondition.wait(aTimeValue);

    CPPUNIT_ASSERT( !m_aContextMenuResult.empty() );
    boost::optional<boost::property_tree::ptree&> aMenu = m_aContextMenuResult.get_child_optional("menu");
    CPPUNIT_ASSERT( aMenu );
    verifyContextMenuStructure( aMenu.get() );

    // tests for writer specific context menu
   // Cut is disabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Cut");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("false"));
    }

    // Copy is disabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Copy");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("false"));
    }

    // Paste is enabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Paste");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("true"));
    }
}

void DesktopLOKTest::testContextMenuImpress()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_presentation.odp", LOK_DOCTYPE_PRESENTATION);
    pDocument->pClass->initializeForRendering(pDocument, nullptr);
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    // random point where we don't hit an underlying comment or text box
    Point aRandomPoint(10, 1150);
    pDocument->pClass->postMouseEvent(pDocument,
                                      LOK_MOUSEEVENT_MOUSEBUTTONDOWN,
                                      aRandomPoint.X(), aRandomPoint.Y(),
                                      1, 4, 0);
    Scheduler::ProcessEventsToIdle();

    TimeValue const aTimeValue = {2 , 0}; // 2 seconds max
    m_aContextMenuCondition.wait(aTimeValue);

    CPPUNIT_ASSERT( !m_aContextMenuResult.empty() );
    boost::optional<boost::property_tree::ptree&> aMenu = m_aContextMenuResult.get_child_optional("menu");
    CPPUNIT_ASSERT( aMenu );
    verifyContextMenuStructure( aMenu.get() );

    // tests for impress specific context menu
    // Cut is disabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Cut");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("false"));
    }

    // Copy is disabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Copy");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("false"));
    }

    // Paste is enabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:Paste");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("true"));
    }

    // SaveBackground is disabled
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:SaveBackground");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aEnabled = aMenuItem.get().get_child_optional("enabled");
        CPPUNIT_ASSERT(aEnabled);
        CPPUNIT_ASSERT_EQUAL(aEnabled.get().data(), std::string("false"));
    }

    // checkbutton tests
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:ShowRuler");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aChecktype = aMenuItem.get().get_child_optional("checktype");
        CPPUNIT_ASSERT(aChecktype);
        CPPUNIT_ASSERT_EQUAL(aChecktype.get().data(), std::string("checkmark"));

        boost::optional<boost::property_tree::ptree&> aChecked = aMenuItem.get().get_child_optional("checked");
        CPPUNIT_ASSERT(aChecked);
        CPPUNIT_ASSERT_EQUAL(aChecked.get().data(), std::string("false"));
    }

    // Checkbutton tests inside SnapLines submenu
    {
        boost::optional<boost::property_tree::ptree> aMenuItem = getContextMenuItem(aMenu.get(), ".uno:SnapLinesMenu");
        CPPUNIT_ASSERT(aMenuItem);

        boost::optional<boost::property_tree::ptree&> aSubmenu = aMenuItem.get().get_child_optional("menu");
        CPPUNIT_ASSERT(aSubmenu);

        boost::optional<boost::property_tree::ptree> aMenuItemHelpVis = getContextMenuItem(aSubmenu.get(), ".uno:HelplinesVisible");
        CPPUNIT_ASSERT(aMenuItemHelpVis);

        boost::optional<boost::property_tree::ptree> aMenuItemHelpUse = getContextMenuItem(aSubmenu.get(), ".uno:HelplinesUse");
        CPPUNIT_ASSERT(aMenuItemHelpUse);

        boost::optional<boost::property_tree::ptree> aMenuItemHelpFront = getContextMenuItem(aSubmenu.get(), ".uno:HelplinesFront");
        CPPUNIT_ASSERT(aMenuItemHelpFront);

        // these are checkmarks
        boost::optional<boost::property_tree::ptree&> aChecktypeHelpVis = aMenuItemHelpVis.get().get_child_optional("checktype");
        CPPUNIT_ASSERT(aChecktypeHelpVis);
        CPPUNIT_ASSERT_EQUAL(aChecktypeHelpVis.get().data(), std::string("checkmark"));

        boost::optional<boost::property_tree::ptree&> aChecktypeHelpUse = aMenuItemHelpUse.get().get_child_optional("checktype");
        CPPUNIT_ASSERT(aChecktypeHelpUse);
        CPPUNIT_ASSERT_EQUAL(aChecktypeHelpUse.get().data(), std::string("checkmark"));

        boost::optional<boost::property_tree::ptree&> aChecktypeHelpFront = aMenuItemHelpFront.get().get_child_optional("checktype");
        CPPUNIT_ASSERT(aChecktypeHelpFront);
        CPPUNIT_ASSERT_EQUAL(aChecktypeHelpFront.get().data(), std::string("checkmark"));

        // HelplineVisible is unchecked
        boost::optional<boost::property_tree::ptree&> aCheckedHelpVis = aMenuItemHelpVis.get().get_child_optional("checked");
        CPPUNIT_ASSERT(aCheckedHelpVis);
        CPPUNIT_ASSERT_EQUAL(aCheckedHelpVis.get().data(), std::string("false"));

        // HelplineUse is checked
        boost::optional<boost::property_tree::ptree&> aCheckedHelpUse = aMenuItemHelpUse.get().get_child_optional("checked");
        CPPUNIT_ASSERT(aCheckedHelpUse);
        CPPUNIT_ASSERT_EQUAL(aCheckedHelpUse.get().data(), std::string("true"));

        // HelplineFront is checked
        boost::optional<boost::property_tree::ptree&> aCheckedHelpFront = aMenuItemHelpFront.get().get_child_optional("checked");
        CPPUNIT_ASSERT(aCheckedHelpFront);
        CPPUNIT_ASSERT_EQUAL(aCheckedHelpFront.get().data(), std::string("true"));
    }
}

static void callbackCompressionTest(const int type, const char* payload, void* data)
{
    std::vector<std::tuple<int, std::string>>* notifs = static_cast<std::vector<std::tuple<int, std::string>>*>(data);
    notifs->emplace_back(type, std::string(payload ? payload : "(nil)"));
}

void DesktopLOKTest::testNotificationCompression()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    std::vector<std::tuple<int, std::string>> notifs;
    std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
    handler->setViewId(SfxLokHelper::getView());

    handler->queue(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR, ""_ostr); // 0
    handler->queue(LOK_CALLBACK_TEXT_SELECTION, "15, 25, 15, 10"_ostr); // Superseded.
    handler->queue(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR, ""_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "15, 25, 15, 10"_ostr); // 1
    handler->queue(LOK_CALLBACK_TEXT_SELECTION, "15, 25, 15, 10"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_TEXT_SELECTION, ""_ostr); // Superseded.
    handler->queue(LOK_CALLBACK_STATE_CHANGED, ""_ostr); // 2
    handler->queue(LOK_CALLBACK_STATE_CHANGED, ".uno:Bold"_ostr); // 3
    handler->queue(LOK_CALLBACK_STATE_CHANGED, ""_ostr); // 4
    handler->queue(LOK_CALLBACK_MOUSE_POINTER, "text"_ostr); // 5
    handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "15, 25, 15, 10"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "15, 25, 15, 10"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_MOUSE_POINTER, "text"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_TEXT_SELECTION_START, "15, 25, 15, 10"_ostr); // Superseded.
    handler->queue(LOK_CALLBACK_TEXT_SELECTION_END, "15, 25, 15, 10"_ostr); // Superseded.
    handler->queue(LOK_CALLBACK_TEXT_SELECTION, "15, 25, 15, 10"_ostr); // Superseded.
    handler->queue(LOK_CALLBACK_TEXT_SELECTION_START, "15, 25, 15, 10"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_TEXT_SELECTION_END, "15, 25, 15, 10"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_TEXT_SELECTION, ""_ostr); // 7
    handler->queue(LOK_CALLBACK_TEXT_SELECTION_START, "15, 25, 15, 10"_ostr); // 8
    handler->queue(LOK_CALLBACK_TEXT_SELECTION_END, "15, 25, 15, 10"_ostr); // 9
    handler->queue(LOK_CALLBACK_CELL_CURSOR, "15, 25, 15, 10"_ostr); // 10
    handler->queue(LOK_CALLBACK_CURSOR_VISIBLE, ""_ostr); // 11
    handler->queue(LOK_CALLBACK_CELL_CURSOR, "15, 25, 15, 10"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_CELL_FORMULA, "blah"_ostr); // 12
    handler->queue(LOK_CALLBACK_SET_PART, "1"_ostr); // 13
    handler->queue(LOK_CALLBACK_STATE_CHANGED, ".uno:AssignLayout=20"_ostr); // Superseded
    handler->queue(LOK_CALLBACK_CURSOR_VISIBLE, ""_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_CELL_FORMULA, "blah"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_SET_PART, "1"_ostr); // Should be dropped.
    handler->queue(LOK_CALLBACK_STATE_CHANGED, ".uno:AssignLayout=1"_ostr); // 14

    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(14), notifs.size());

    size_t i = 0;
    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string(""), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string("15, 25, 15, 10"), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_STATE_CHANGED), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string(""), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_STATE_CHANGED), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string(".uno:Bold"), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_STATE_CHANGED), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string(""), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_MOUSE_POINTER), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string("text"), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_TEXT_SELECTION), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string(""), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_TEXT_SELECTION_START), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string("15, 25, 15, 10"), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_TEXT_SELECTION_END), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string("15, 25, 15, 10"), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_CELL_CURSOR), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string("15, 25, 15, 10"), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_CURSOR_VISIBLE), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string(""), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_CELL_FORMULA), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string("blah"), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_SET_PART), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string("1"), std::get<1>(notifs[i++]));

    CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_STATE_CHANGED), std::get<0>(notifs[i]));
    CPPUNIT_ASSERT_EQUAL(std::string(".uno:AssignLayout=1"), std::get<1>(notifs[i++]));
}

void DesktopLOKTest::testTileInvalidationCompression()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");

    comphelper::LibreOfficeKit::setPartInInvalidation(true);
    comphelper::ScopeGuard aGuard([]()
    {
        comphelper::LibreOfficeKit::setPartInInvalidation(false);
    });

    // Single part merging
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 239, 239, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 239, 239, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "-100, -50, 500, 650, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, -32767, -32767, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "100, 100, 200, 200, 0, 0"_ostr);

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), notifs.size());

        size_t i = 0;
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 400, 600, 0, 0"), std::get<1>(notifs[i++]));
    }

    // Part Number
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 239, 239, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 200, 200, 1, 0"_ostr); // Different part
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 0, 0, 2, 0"_ostr); // Invalid
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "-121, -121, 200, 200, 0, 0"_ostr); // Inside first
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, -32767, -32767, 1, 0"_ostr); // Invalid

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), notifs.size());

        size_t i = 0;
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 200, 200, 1, 0"), std::get<1>(notifs[i++]));

        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 239, 239, 0, 0"), std::get<1>(notifs[i++]));
    }

    // All Parts
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 239, 239, 0, 0"_ostr); // 0
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 200, 200, 1, 0"_ostr); // 1: Different part
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 0, 0, -1, 0"_ostr); // Invalid
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "-121, -121, 200, 200, -1, 0"_ostr); // 0: All parts
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, -32767, -32767, -1, 0"_ostr); // Invalid
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "-100, -100, 1200, 1200, -1, 0"_ostr); // 0: All parts
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 239, 239, 3, 0"_ostr); // Overlapped
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "1000, 1000, 1239, 1239, 2, 0"_ostr); // 1: Unique region

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), notifs.size());

        size_t i = 0;
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 1100, 1100, -1, 0"), std::get<1>(notifs[i++]));

        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("1000, 1000, 1239, 1239, 2, 0"), std::get<1>(notifs[i++]));
    }

    // All Parts (partial)
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 200, 200, 0, 0"_ostr); // 0
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 100, 100, 1, 0"_ostr); // 1: Different part
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 0, 0, -1, 0"_ostr); // Invalid
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "150, 150, 50, 50, -1, 0"_ostr); // 2: All-parts
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, -32767, -32767, -1, 0"_ostr); // Invalid
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "150, 150, 40, 40, 3, 0"_ostr); // Overlapped w/ 2
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 200, 200, 4, 0"_ostr); // 3: Unique
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "1000, 1000, 1239, 1239, 1, 0"_ostr); // 4: Unique

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), notifs.size());

        size_t i = 0;
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 200, 200, 0, 0"), std::get<1>(notifs[i++]));

        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 100, 100, 1, 0"), std::get<1>(notifs[i++]));

        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("150, 150, 50, 50, -1, 0"), std::get<1>(notifs[i++]));

        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 200, 200, 4, 0"), std::get<1>(notifs[i++]));

        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("1000, 1000, 1239, 1239, 1, 0"), std::get<1>(notifs[i++]));
    }

    // Merge with "EMPTY"
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 239, 239, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "EMPTY, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, 239, 240, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "-121, -121, 300, 300, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "0, 0, -32767, -32767, 0, 0"_ostr);

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), notifs.size());

        size_t i = 0;
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[i]));
        CPPUNIT_ASSERT_EQUAL(std::string("EMPTY, 0, 0"), std::get<1>(notifs[i++]));
    }
}

void DesktopLOKTest::testPartInInvalidation()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    // No part in invalidation: merge.
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "10, 10, 20, 10"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "20, 10, 20, 10"_ostr);

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), notifs.size());

        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[0]));
        CPPUNIT_ASSERT_EQUAL(std::string("10, 10, 30, 10"), std::get<1>(notifs[0]));
    }
    // No part in invalidation: don't merge.
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "10, 10, 20, 10"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "40, 10, 20, 10"_ostr);

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), notifs.size());
    }

    // Part in invalidation, intersection and parts match -> merge.
    {
        comphelper::LibreOfficeKit::setPartInInvalidation(true);
        comphelper::ScopeGuard aGuard([]()
        {
            comphelper::LibreOfficeKit::setPartInInvalidation(false);
        });

        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "10, 10, 20, 10, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "20, 10, 20, 10, 0, 0"_ostr);

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), notifs.size());
    }
    // Part in invalidation, intersection and parts don't match -> don't merge.
    {
        comphelper::LibreOfficeKit::setPartInInvalidation(true);
        comphelper::ScopeGuard aGuard([]()
        {
            comphelper::LibreOfficeKit::setPartInInvalidation(false);
        });

        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackCompressionTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "10, 10, 20, 10, 0, 0"_ostr);
        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, "20, 10, 20, 10, 1, 0"_ostr);

        Scheduler::ProcessEventsToIdle();

        // This failed as RectangleAndPart::Create() always assumed no part in
        // payload, so this was merged -> it was 1.
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), notifs.size());
    }
}

static void callbackBinaryCallbackTest(const int type, const char* payload, void* data)
{
    std::vector<std::tuple<int, std::string>>* notifs = static_cast<std::vector<std::tuple<int, std::string>>*>(data);
    notifs->emplace_back(type, std::string(payload ? payload : "(nil)"));
}

void DesktopLOKTest::testBinaryCallback()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");

    const tools::Rectangle rect1(Point(10,15),Size(20,25));
    const std::string rect1String(rect1.toString());
    // Verify that using queue() and libreOfficeKitViewInvalidateTilesCallback() has the same result.
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackBinaryCallbackTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->queue(LOK_CALLBACK_INVALIDATE_TILES, OString(rect1String));

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), notifs.size());
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[0]));
        CPPUNIT_ASSERT_EQUAL(rect1String, std::get<1>(notifs[0]));
    }
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackBinaryCallbackTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->tilePainted(/*nPart=*/INT_MIN, /*nMode=*/0, rect1);
        handler->libreOfficeKitViewInvalidateTilesCallback(&rect1, INT_MIN, 0);

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), notifs.size());
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[0]));
        CPPUNIT_ASSERT_EQUAL(rect1String, std::get<1>(notifs[0]));
    }
    // Verify that the "EMPTY" invalidation gets converted properly.
    {
        std::vector<std::tuple<int, std::string>> notifs;
        std::unique_ptr<CallbackFlushHandler> handler(new CallbackFlushHandler(pDocument, callbackBinaryCallbackTest, &notifs));
        handler->setViewId(SfxLokHelper::getView());

        handler->tilePainted(/*nPart=*/INT_MIN, /*nMode=*/0, rect1);
        handler->libreOfficeKitViewInvalidateTilesCallback(nullptr, INT_MIN, 0);

        Scheduler::ProcessEventsToIdle();

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), notifs.size());
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(notifs[0]));
        CPPUNIT_ASSERT_EQUAL(std::string("EMPTY"), std::get<1>(notifs[0]));
    }
}

void DesktopLOKTest::testOmitInvalidate()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    tools::Rectangle aRectangle{Point(0, 0), Size(10, 10)};

    {
        // Given a clean state:
        std::vector<std::tuple<int, std::string>> aCallbacks;
        std::unique_ptr<CallbackFlushHandler> pHandler(new CallbackFlushHandler(pDocument, callbackBinaryCallbackTest, &aCallbacks));
        pHandler->setViewId(0);

        // When emitting just an invalidation:
        pHandler->libreOfficeKitViewInvalidateTilesCallback(&aRectangle, /*nPart=*/0, /*nMode=*/0);

        // Then make sure that's filtered out:
        Scheduler::ProcessEventsToIdle();
        // Without the accompanying fix in place, this test would have failed with:
        // - Expected: 0
        // - Actual  : 1
        // i.e. invalidation was emitted when we haven't rendered any tiles yet.
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), aCallbacks.size());
    }

    {
        // Given a clean state:
        std::vector<std::tuple<int, std::string>> aCallbacks;
        std::unique_ptr<CallbackFlushHandler> pHandler(new CallbackFlushHandler(pDocument, callbackBinaryCallbackTest, &aCallbacks));
        pHandler->setViewId(0);

        // When emitting an invalidation outside the painted area:
        pHandler->tilePainted(/*nPart=*/0, /*nMode=*/0, aRectangle);
        tools::Rectangle aElsewhere{Point(20, 20), Size(10, 10)};
        pHandler->libreOfficeKitViewInvalidateTilesCallback(&aElsewhere, /*nPart=*/0, /*nMode=*/0);

        // Then make sure that's filtered out:
        Scheduler::ProcessEventsToIdle();
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), aCallbacks.size());
    }

    {
        // Given a clean state:
        std::vector<std::tuple<int, std::string>> aCallbacks;
        std::unique_ptr<CallbackFlushHandler> pHandler(new CallbackFlushHandler(pDocument, callbackBinaryCallbackTest, &aCallbacks));
        pHandler->setViewId(0);

        // When emitting an invalidation partly outside the painted area:
        pHandler->tilePainted(/*nPart=*/0, /*nMode=*/0, aRectangle);
        tools::Rectangle aLarger{Point(0, 0), Size(20, 10)};
        pHandler->libreOfficeKitViewInvalidateTilesCallback(&aLarger, /*nPart=*/0, /*nMode=*/0);

        // Then make sure that's cropped:
        Scheduler::ProcessEventsToIdle();
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aCallbacks.size());
        CPPUNIT_ASSERT_EQUAL(int(LOK_CALLBACK_INVALIDATE_TILES), std::get<0>(aCallbacks[0]));
        // x, y, w, h, part, mode; so this is cropped.
        CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 9, 9, 0, 0"), std::get<1>(aCallbacks[0]));
    }
}

void DesktopLOKTest::test2ViewsOmitInvalidate()
{
    // Given a document with 2 views:
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    std::vector<std::tuple<int, std::string>> aCallbacks1;
    std::shared_ptr<CallbackFlushHandler> pHandler1(new CallbackFlushHandler(pDocument, callbackBinaryCallbackTest, &aCallbacks1));
    pHandler1->setViewId(0);
    pDocument->mpCallbackFlushHandlers[0] = pHandler1;
    std::vector<std::tuple<int, std::string>> aCallbacks2;
    std::shared_ptr<CallbackFlushHandler> pHandler2(new CallbackFlushHandler(pDocument, callbackBinaryCallbackTest, &aCallbacks2));
    pHandler2->setViewId(1);
    pDocument->mpCallbackFlushHandlers[1] = pHandler2;

    // When painting a tile for a larger area, and then 2 invalidates: the first view gets a smaller
    // invalidate, the second view gets a larger invalidate:
    tools::Rectangle aPaint{Point(0, 0), Size(20, 10)};
    pDocument->updateViewsForPaintedTile(/*nOrigViewId=*/0, /*nPart=*/0, /*nMode=*/0, aPaint);
    tools::Rectangle aSmaller{Point(0, 0), Size(10, 10)};
    pHandler1->libreOfficeKitViewInvalidateTilesCallback(&aSmaller, /*nPart=*/0, /*nMode=*/0);
    tools::Rectangle aLarger{Point(0, 0), Size(20, 10)};
    pHandler2->libreOfficeKitViewInvalidateTilesCallback(&aLarger, /*nPart=*/0, /*nMode=*/0);

    // Then make sure this larger invalidate for the 2nd view is not lost:
    Scheduler::ProcessEventsToIdle();
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 1
    // - Actual  : 0
    // i.e. the 2nd view's (larger) invalidate was lost.
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aCallbacks2.size());
    CPPUNIT_ASSERT_EQUAL(std::string("0, 0, 19, 9, 0, 0"), std::get<1>(aCallbacks2[0]));
}

void DesktopLOKTest::testInput()
{
    // Load a Writer document, enable change recording and press a key.
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");

    Scheduler::ProcessEventsToIdle(); // Get focus & other bits setup.

    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT, "far");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT_END, "far");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT, " ");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT_END, " ");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT, "beyond");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT_END, "beyond");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT, " ");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT_END, " ");
    // Mis-spelled ...
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT, "kovely");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT_END, "kovely");
    // Remove it again
    pDocument->pClass->removeTextContext(pDocument, 0, 6, 0);
    // Replace it with lovely
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT, "lovely");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT_END, "lovely");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT, " ");
    pDocument->pClass->postWindowExtTextInputEvent(pDocument, 0, LOK_EXT_TEXTINPUT_END, " ");

    // get the text ...
    pDocument->pClass->postUnoCommand(pDocument, ".uno:SelectAll", nullptr, false);
    Scheduler::ProcessEventsToIdle();
    char* pText = pDocument->pClass->getTextSelection(pDocument, "text/plain;charset=utf-8", nullptr);
    CPPUNIT_ASSERT(pText != nullptr);
    CPPUNIT_ASSERT_EQUAL("far beyond lovely "_ostr, OString(pText));
    free(pText);
}

void DesktopLOKTest::testRedlineWriter()
{
    // Load a Writer document, enable change recording and press a key.
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    uno::Reference<beans::XPropertySet> xPropertySet(mxComponent, uno::UNO_QUERY);
    xPropertySet->setPropertyValue(u"RecordChanges"_ustr, uno::Any(true));
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 't', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 't', 0);
    Scheduler::ProcessEventsToIdle();

    // Get redline info.
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:AcceptTrackedChanges");
    std::stringstream aStream(pJSON);
    free(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());
    boost::property_tree::read_json(aStream, aTree);
    // Make sure that pressing a key creates exactly one redline.
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aTree.get_child("redlines").size());

    for (const boost::property_tree::ptree::value_type& rRedline : aTree.get_child("redlines"))
        // This failed with boost::property_tree::ptree_bad_path, as there were no description field.
        CPPUNIT_ASSERT_EQUAL(std::string("Insert \xE2\x80\x9Ct\xE2\x80\x9D"), rRedline.second.get<std::string>("description"));
            // U+201C LEFT DOUBLE QUOTATION MARK, U+201D RIGHT DOUBLE QUOTATION
            // MARK
}

void DesktopLOKTest::testRedlineCalc()
{
    // Load a Writer document, enable change recording and press a key.
    LibLODocument_Impl* pDocument = loadDoc("sheets.ods");
    uno::Reference<beans::XPropertySet> xPropertySet(mxComponent, uno::UNO_QUERY);
    xPropertySet->setPropertyValue(u"RecordChanges"_ustr, uno::Any(true));
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 't', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 't', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, KEY_RETURN);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 0, KEY_RETURN);
    Scheduler::ProcessEventsToIdle();

    // Get redline info.
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:AcceptTrackedChanges");
    std::stringstream aStream(pJSON);
    free(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());
    boost::property_tree::read_json(aStream, aTree);
    // Make sure that pressing a key creates exactly one redline.
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aTree.get_child("redlines").size());

    for (const boost::property_tree::ptree::value_type& rRedline : aTree.get_child("redlines"))
        // This failed with boost::property_tree::ptree_bad_path, as there were no description field.
        CPPUNIT_ASSERT_EQUAL(std::string("Cell B4 changed from '5' to 't'"), rRedline.second.get<std::string>("description"));
}

namespace {

struct RedlineInfo
{
    std::string action;
    std::string index;
    std::string author;
    std::string type;
    std::string comment;
    std::string description;
    std::string dateTime;
};

std::vector<RedlineInfo> getRedlineInfo(const boost::property_tree::ptree& redlineNode)
{
    std::vector<RedlineInfo> result;
    result.reserve(redlineNode.size());
    for (const auto& redline : redlineNode)
    {
        result.emplace_back();
        result.back().index = redline.second.get<std::string>("index");
        result.back().author = redline.second.get<std::string>("author");
        result.back().type = redline.second.get<std::string>("type");
        result.back().comment = redline.second.get<std::string>("comment");
        result.back().description = redline.second.get<std::string>("description");
        result.back().dateTime = redline.second.get<std::string>("dateTime");
        if (auto oAction = redline.second.get_optional<std::string>("action"))
            result.back().action = *oAction;
    }

    return result;
}

std::vector<RedlineInfo> getRedlineInfo(LibLODocument_Impl* pDocument)
{
    char* json
        = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:AcceptTrackedChanges");
    std::stringstream stream(json);
    free(json);
    CPPUNIT_ASSERT(!stream.str().empty());
    boost::property_tree::ptree tree;
    boost::property_tree::read_json(stream, tree);
    return getRedlineInfo(tree.get_child("redlines"));
}

class ViewCallback
{
    LibLODocument_Impl* mpDocument;
    int mnView;
public:
    OString m_aCellFormula;
    boost::property_tree::ptree m_JSONDialog;
    int m_nTableSelectionCount;
    int m_nColorPaletteCallbackCount = 0;
    bool m_bEmptyTableSelection;
    bool m_bTilesInvalidated;
    bool m_bZeroCursor;
    tools::Rectangle m_aOwnCursor;
    boost::property_tree::ptree m_aCommentCallbackResult;
    boost::property_tree::ptree m_aColorPaletteCallbackResult;
    RedlineInfo m_aLastRedlineInfo;

    ViewCallback(LibLODocument_Impl* pDocument)
        : mpDocument(pDocument),
          m_nTableSelectionCount(0),
          m_bEmptyTableSelection(false),
          m_bTilesInvalidated(false),
          m_bZeroCursor(false)
    {
        mnView = SfxLokHelper::getView();
        mpDocument->m_pDocumentClass->registerCallback(pDocument, &ViewCallback::callback, this);
    }

    ~ViewCallback()
    {
        mpDocument->m_pDocumentClass->setView(mpDocument, mnView);
        mpDocument->m_pDocumentClass->registerCallback(mpDocument, nullptr, nullptr);
    }

    static void callback(int nType, const char* pPayload, void* pData)
    {
        static_cast<ViewCallback*>(pData)->callbackImpl(nType, pPayload);
    }

    void callbackImpl(int nType, const char* pPayload)
    {
        OString aPayload(pPayload);
        switch (nType)
        {
        case LOK_CALLBACK_INVALIDATE_TILES:
        {
            m_bTilesInvalidated = true;
        }
        break;
        case LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR:
        {
            uno::Sequence<OUString> aSeq = comphelper::string::convertCommaSeparated(OUString::fromUtf8(aPayload));
            if (std::string_view("EMPTY") == pPayload)
                return;
            CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(4), aSeq.getLength());
            m_aOwnCursor.SetLeft(aSeq[0].toInt32());
            m_aOwnCursor.SetTop(aSeq[1].toInt32());
            m_aOwnCursor.setWidth(aSeq[2].toInt32());
            m_aOwnCursor.setHeight(aSeq[3].toInt32());

            if (m_aOwnCursor.Left() == 0 && m_aOwnCursor.Top() == 0)
                m_bZeroCursor = true;
        }
        break;
        case LOK_CALLBACK_COMMENT:
        {
            m_aCommentCallbackResult.clear();
            std::stringstream aStream(pPayload);
            boost::property_tree::read_json(aStream, m_aCommentCallbackResult);
            m_aCommentCallbackResult = m_aCommentCallbackResult.get_child("comment");
        }
        break;
        break;
        case LOK_CALLBACK_CELL_FORMULA:
        {
            m_aCellFormula = aPayload;
        }
        break;
        case LOK_CALLBACK_TABLE_SELECTED:
        {
            m_bEmptyTableSelection = (std::string(pPayload).compare("{ }") == 0);
            ++m_nTableSelectionCount;
        }
        break;
        case LOK_CALLBACK_COLOR_PALETTES:
        {
            m_aColorPaletteCallbackResult.clear();
            std::stringstream aStream(pPayload);
            boost::property_tree::read_json(aStream, m_aColorPaletteCallbackResult);
            ++m_nColorPaletteCallbackCount;
        }
        break;
        case LOK_CALLBACK_JSDIALOG:
        {
            m_JSONDialog.clear();
            std::stringstream aStream(pPayload);
            boost::property_tree::read_json(aStream, m_JSONDialog);
        }
        break;
        case LOK_CALLBACK_REDLINE_TABLE_SIZE_CHANGED:
        case LOK_CALLBACK_REDLINE_TABLE_ENTRY_MODIFIED:
        {
            std::stringstream aStream(pPayload);
            boost::property_tree::ptree tree;
            boost::property_tree::read_json(aStream, tree);
            auto redlines = getRedlineInfo(tree);
            CPPUNIT_ASSERT_EQUAL(size_t(1), redlines.size());
            m_aLastRedlineInfo = redlines[0];
        }
        break;
        }
    }
};

}

void DesktopLOKTest::testPaintPartTile()
{
    // Load an impress doc of 2 slides.
//    ViewCallback aView1;
//    ViewCallback aView2;
    LibLODocument_Impl* pDocument = loadDoc("2slides.odp");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
//    pDocument->m_pDocumentClass->registerCallback(pDocument, &ViewCallback::callback, &aView1);
    int nView1 = pDocument->m_pDocumentClass->getView(pDocument);

    // Create a second view.
    pDocument->m_pDocumentClass->createView(pDocument);
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
//    pDocument->m_pDocumentClass->registerCallback(pDocument, &ViewCallback::callback, &aView2);

    // Go to the second slide in the second view.
    pDocument->m_pDocumentClass->setPart(pDocument, 1);

    // Switch back to the first view and start typing.
    pDocument->m_pDocumentClass->setView(pDocument, nView1);
    pDocument->m_pDocumentClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, awt::Key::TAB);
    pDocument->m_pDocumentClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 0, awt::Key::TAB);
    pDocument->m_pDocumentClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'x', 0);
    pDocument->m_pDocumentClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 'x', 0);
    Scheduler::ProcessEventsToIdle();

    // Call paintPartTile() to paint the second part (in whichever view it finds suitable for this).
    unsigned char pPixels[256 * 256 * 4];
    pDocument->m_pDocumentClass->paintPartTile(pDocument, pPixels, 1, 0, 256, 256, 0, 0, 256, 256);

    // Type again.
    Scheduler::ProcessEventsToIdle();
//    aView1.m_bTilesInvalidated = false;
    pDocument->m_pDocumentClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'x', 0);
    pDocument->m_pDocumentClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 'x', 0);
    Scheduler::ProcessEventsToIdle();
    // This failed: paintPartTile() (as a side-effect) ended the text edit of
    // the first view, so there were no invalidations.
    //CPPUNIT_ASSERT(aView1.m_bTilesInvalidated);
}

void DesktopLOKTest::testPaintTileOmitInvalidate()
{
    // Given a painted tile:
    comphelper::LibreOfficeKit::setPartInInvalidation(true);
    comphelper::ScopeGuard aGuard([]()
    {
        comphelper::LibreOfficeKit::setPartInInvalidation(false);
    });
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    ViewCallback aView(pDocument);
    const int nCanvasWidth = 256;
    const int nCanvasHeight = 256;
    std::array<sal_uInt8, nCanvasWidth * nCanvasHeight * 4> aPixels;
    pDocument->m_pDocumentClass->paintTile(pDocument, aPixels.data(), nCanvasWidth, nCanvasHeight, 0, 0, 3840, 3840);
    Scheduler::ProcessEventsToIdle();
    aView.m_bTilesInvalidated = false;

    // When pressing a key:
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'x', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 'x', 0);
    Scheduler::ProcessEventsToIdle();

    // Then make sure we get an invalidation:
    CPPUNIT_ASSERT(aView.m_bTilesInvalidated);
}

void DesktopLOKTest::testCreateViewOmitInvalidate()
{
    // Given a document with 2 views: view 1 renders sheet One, then view 2 gets created and finally
    // view 1 switches to sheet Two:
    comphelper::LibreOfficeKit::setPartInInvalidation(true);
    comphelper::ScopeGuard aGuard([]()
    {
        comphelper::LibreOfficeKit::setPartInInvalidation(false);
    });
    LibLODocument_Impl* pDocument = loadDoc("create-view-omit-invalidate.ods");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, nullptr);
    ViewCallback aView1(pDocument);
    int nView1 = pDocument->m_pDocumentClass->getView(pDocument);
    const int nCanvasWidth = 256;
    const int nCanvasHeight = 256;
    std::array<sal_uInt8, nCanvasWidth * nCanvasHeight * 4> aPixels;
    pDocument->m_pDocumentClass->paintTile(pDocument, aPixels.data(), nCanvasWidth, nCanvasHeight, 0, 0, 3840, 3840);
    pDocument->m_pDocumentClass->createView(pDocument);
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, nullptr);
    ViewCallback aView2(pDocument);
    pDocument->m_pDocumentClass->setView(pDocument, nView1);
    pDocument->m_pDocumentClass->setPart(pDocument, 1);
    Scheduler::ProcessEventsToIdle();
    aView1.m_bTilesInvalidated = false;
    aView2.m_bTilesInvalidated = false;

    // When pressing a key in view 1, on sheet Two:
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'x', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 'x', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, KEY_RETURN);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 0, KEY_RETURN);
    Scheduler::ProcessEventsToIdle();

    // Then make sure that both views are invalidated:
    CPPUNIT_ASSERT(aView1.m_bTilesInvalidated);
    // Without the accompanying fix in place, this test would have failed, the 2nd view was not
    // invalidated when it was created after a paintTile().
    CPPUNIT_ASSERT(aView2.m_bTilesInvalidated);
}

void DesktopLOKTest::testPaintPartTileDifferentSchemes()
{
    Color aDarkColor(0x1c, 0x1c, 0x1c);

    // Add a minimal dark scheme
    {
        svtools::EditableColorConfig aColorConfig;
        svtools::ColorConfigValue aValue;
        aValue.bIsVisible = true;
        aValue.nColor = aDarkColor;
        aColorConfig.SetColorValue(svtools::DOCCOLOR, aValue);
        aColorConfig.AddScheme(u"Dark"_ustr);
    }

    // Add a minimal light scheme
    {
        svtools::EditableColorConfig aColorConfig;
        svtools::ColorConfigValue aValue;
        aValue.bIsVisible = true;
        aValue.nColor = COL_WHITE;
        aColorConfig.SetColorValue(svtools::DOCCOLOR, aValue);
        aColorConfig.AddScheme(u"Light"_ustr);
    }

    // This view will default to light scheme
    LibLODocument_Impl* pDocument = loadDoc("2slides.odp");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    int nView1 = pDocument->m_pDocumentClass->getView(pDocument);

    // Create a second view
    pDocument->m_pDocumentClass->createView(pDocument);
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");

    // Go to the second slide in the second view
    pDocument->m_pDocumentClass->setPart(pDocument, 1);

    // Set to dark scheme
    {
        uno::Sequence<beans::PropertyValue> aPropertyValues = comphelper::InitPropertySequence(
            {
                { "NewTheme", uno::Any(u"Dark"_ustr) },
            }
        );
        dispatchCommand(mxComponent, u".uno:ChangeTheme"_ustr, aPropertyValues);
    }

    constexpr int nCanvasWidth = 256;
    constexpr int nCanvasHeight = 256;

    // Just a random pixel in the middle of the canvas
    constexpr int nPixelX = 128;
    constexpr int nPixelY = 128 * nCanvasWidth;

    // this is BGRA format data
    std::array<sal_uInt8, nCanvasWidth * nCanvasHeight * 4> aPixels;

    // Both parts should be painted with dark scheme
    pDocument->m_pDocumentClass->paintPartTile(pDocument, aPixels.data(), 0, 0, nCanvasWidth, nCanvasHeight, 0, 0, nCanvasWidth, nCanvasHeight);
    Color aPixel(aPixels[nPixelX + nPixelY + 0], aPixels[nPixelX + nPixelY + 1], aPixels[nPixelX + nPixelY + 2]);
    CPPUNIT_ASSERT_EQUAL(aDarkColor, aPixel);

    pDocument->m_pDocumentClass->paintPartTile(pDocument, aPixels.data(), 0, 0, nCanvasWidth, nCanvasHeight, 0, 0, nCanvasWidth, nCanvasHeight);
    aPixel = Color(aPixels[nPixelX + nPixelY + 0], aPixels[nPixelX + nPixelY + 1], aPixels[nPixelX + nPixelY + 2]);
    CPPUNIT_ASSERT_EQUAL(aDarkColor, aPixel);

    // Switch back to first view
    pDocument->m_pDocumentClass->setView(pDocument, nView1);

    // Both parts should be painted with light scheme
    pDocument->m_pDocumentClass->paintPartTile(pDocument, aPixels.data(), 0, 0, nCanvasWidth, nCanvasHeight, 0, 0, nCanvasWidth, nCanvasHeight);
    aPixel = Color(aPixels[nPixelX + nPixelY + 0], aPixels[nPixelX + nPixelY + 1], aPixels[nPixelX + nPixelY + 2]);
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, aPixel);

    pDocument->m_pDocumentClass->paintPartTile(pDocument, aPixels.data(), 0, 0, nCanvasWidth, nCanvasHeight, 0, 0, nCanvasWidth, nCanvasHeight);
    aPixel = Color(aPixels[nPixelX + nPixelY + 0], aPixels[nPixelX + nPixelY + 1], aPixels[nPixelX + nPixelY + 2]);
    CPPUNIT_ASSERT_EQUAL(COL_WHITE, aPixel);
}

#if HAVE_MORE_FONTS
#include <rtl/uri.hxx>
void DesktopLOKTest::testGetFontSubset()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    OUString aFontName = rtl::Uri::encode(
        u"Liberation Sans"_ustr,
        rtl_UriCharClassRelSegment,
        rtl_UriEncodeKeepEscapes,
        RTL_TEXTENCODING_UTF8
    );
    OString aCommand = ".uno:FontSubset&name=" + OUStringToOString(aFontName, RTL_TEXTENCODING_UTF8);
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, aCommand.getStr());
    std::stringstream aStream(pJSON);
    boost::property_tree::read_json(aStream, aTree);
    CPPUNIT_ASSERT( !aTree.empty() );
    CPPUNIT_ASSERT_EQUAL( std::string(".uno:FontSubset"), aTree.get_child("commandName").get_value<std::string>() );
    boost::property_tree::ptree aValues = aTree.get_child("commandValues");
    CPPUNIT_ASSERT( !aValues.empty() );
    free(pJSON);
}
#endif

void DesktopLOKTest::testCommentsWriter()
{
    // Disable tiled rendering for comments
    comphelper::LibreOfficeKit::setTiledAnnotations(false);

    LibLODocument_Impl* pDocument = loadDoc("comments.odt");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, nullptr);
    tools::Long nWidth, nHeight;
    pDocument->m_pDocumentClass->getDocumentSize(pDocument, &nWidth, &nHeight);

    // Document width alongwith without sidebar comes to be < 13000
    CPPUNIT_ASSERT( nWidth < 13000 );

    // Can we get all the comments using .uno:ViewAnnotations command ?
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:ViewAnnotations");
    std::stringstream aStream(pJSON);
    free(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());
    boost::property_tree::read_json(aStream, aTree);
    // There are 3 comments in the document already
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), aTree.get_child("comments").size());

    int nComment2Id = 0;
    // Check if all comment fields have valid data
    for (const auto& rComment : aTree.get_child("comments"))
    {
        CPPUNIT_ASSERT(rComment.second.get<int>("id") > 0);
        CPPUNIT_ASSERT(!rComment.second.get<std::string>("author").empty());
        CPPUNIT_ASSERT(!rComment.second.get<std::string>("html").empty());
        // Has a valid iso 8601 date time string
        css::util::DateTime aDateTime;
        OUString aDateTimeString = OUString::createFromAscii(rComment.second.get<std::string>("dateTime"));
        CPPUNIT_ASSERT(utl::ISO8601parseDateTime(aDateTimeString, aDateTime));

        // This comment has a marked text range
        if (rComment.second.get<std::string>("html") == "<div>Comment 2</div>")
        {
            CPPUNIT_ASSERT(!rComment.second.get<std::string>("textRange").empty());
            nComment2Id = rComment.second.get<int>("id");
        }
        // This is a reply comment
        else if (rComment.second.get<std::string>("html") == "<div>Reply to Comment 2</div>")
        {
            CPPUNIT_ASSERT_EQUAL(nComment2Id, rComment.second.get<int>("parentId"));
        }
    }

    comphelper::LibreOfficeKit::setTiledAnnotations(true);
}


void DesktopLOKTest::testCommentsCalc()
{
    // Disable tiled rendering for comments
    comphelper::LibreOfficeKit::setTiledAnnotations(false);

    LibLODocument_Impl* pDocument = loadDoc("sheets.ods");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, nullptr);

    // Can we get all the comments using .uno:ViewAnnotations command ?
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:ViewAnnotations");
    std::stringstream aStream(pJSON);
    free(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());
    boost::property_tree::read_json(aStream, aTree);
    // There are 2 comments in the document already
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aTree.get_child("comments").size());

    // Check if all comment fields have valid data
    int nIdx = 0;
    for (const auto& rComment : aTree.get_child("comments"))
    {
        switch(nIdx)
        {
            case 0:
            {
                CPPUNIT_ASSERT_EQUAL(std::string("4"), rComment.second.get<std::string>("tab"));
                CPPUNIT_ASSERT_EQUAL(std::string("Comment1"), rComment.second.get<std::string>("text"));
                CPPUNIT_ASSERT_EQUAL(std::string("6 14 6 14"), rComment.second.get<std::string>("cellRange"));
            }
            break;
            case 1:
            {
                CPPUNIT_ASSERT_EQUAL(std::string("4"), rComment.second.get<std::string>("tab"));
                CPPUNIT_ASSERT_EQUAL(std::string("Comment2"), rComment.second.get<std::string>("text"));
                CPPUNIT_ASSERT_EQUAL(std::string("7 17 7 17"), rComment.second.get<std::string>("cellRange"));
            }
            break;
        }

        ++nIdx;
    }

    // We checked all the comments
    CPPUNIT_ASSERT_EQUAL(2, nIdx);

    comphelper::LibreOfficeKit::setTiledAnnotations(true);
}


void DesktopLOKTest::testCommentsImpress()
{
    // Disable tiled rendering for comments
    comphelper::LibreOfficeKit::setTiledAnnotations(false);

    LibLODocument_Impl* pDocument = loadDoc("blank_presentation.odp");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, nullptr);

    // Can we get all the comments using .uno:ViewAnnotations command ?
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:ViewAnnotations");
    std::stringstream aStream(pJSON);
    free(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());
    boost::property_tree::read_json(aStream, aTree);
    // There are 2 comments in the document already
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aTree.get_child("comments").size());

    // Check if all comment fields have valid data
    int nIdx = 0;
    for (const auto& rComment : aTree.get_child("comments"))
    {
        switch(nIdx)
        {
            case 0:
            {
                CPPUNIT_ASSERT(rComment.second.get<int>("id") > 0);
                CPPUNIT_ASSERT_EQUAL(std::string("This is comment1"), rComment.second.get<std::string>("text"));
                CPPUNIT_ASSERT_EQUAL(std::string("LOK User1"), rComment.second.get<std::string>("author"));
                css::util::DateTime aDateTime;
                OUString aDateTimeString = OUString::createFromAscii(rComment.second.get<std::string>("dateTime"));
                CPPUNIT_ASSERT(utl::ISO8601parseDateTime(aDateTimeString, aDateTime));
            }
            break;
            case 1:
            {
                CPPUNIT_ASSERT(rComment.second.get<int>("id") > 0);
                CPPUNIT_ASSERT_EQUAL(std::string("This is comment2"), rComment.second.get<std::string>("text"));
                CPPUNIT_ASSERT_EQUAL(std::string("LOK User2"), rComment.second.get<std::string>("author"));
                css::util::DateTime aDateTime;
                OUString aDateTimeString = OUString::createFromAscii(rComment.second.get<std::string>("dateTime"));
                CPPUNIT_ASSERT(utl::ISO8601parseDateTime(aDateTimeString, aDateTime));
            }
            break;
        }

        ++nIdx;
    }

    // We checked all the comments
    CPPUNIT_ASSERT_EQUAL(2, nIdx);

    comphelper::LibreOfficeKit::setTiledAnnotations(true);
}

void DesktopLOKTest::testCommentsCallbacksWriter()
{
    // Comments callback are emitted only if tiled annotations are off
    comphelper::LibreOfficeKit::setTiledAnnotations(false);
    LibLODocument_Impl* pDocument = loadDoc("comments.odt");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aView1(pDocument);
    pDocument->m_pDocumentClass->createView(pDocument);
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aView2(pDocument);

    // Add a new comment
    OString aCommandArgs("{ \"Text\": { \"type\": \"string\", \"value\": \"Additional comment\" }, \"Author\": { \"type\": \"string\", \"value\": \"LOK User1\" } }"_ostr);
    pDocument->pClass->postUnoCommand(pDocument, ".uno:InsertAnnotation", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Add' action
    CPPUNIT_ASSERT_EQUAL(std::string("Add"), aView1.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(std::string("Add"), aView2.m_aCommentCallbackResult.get<std::string>("action"));
    int nCommentId1 = aView1.m_aCommentCallbackResult.get<int>("id");

    // Reply to a comment just added
    aCommandArgs = "{ \"Id\": { \"type\": \"string\", \"value\": \"" + OString::number(nCommentId1) + "\" }, \"Text\": { \"type\": \"string\", \"value\": \"Reply comment\" } }";
    pDocument->pClass->postUnoCommand(pDocument, ".uno:ReplyComment", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Add' action and linked to its parent comment
    CPPUNIT_ASSERT_EQUAL(std::string("Add"), aView1.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(std::string("Add"), aView2.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(nCommentId1, aView1.m_aCommentCallbackResult.get<int>("parentId"));
    CPPUNIT_ASSERT_EQUAL(nCommentId1, aView2.m_aCommentCallbackResult.get<int>("parentId"));
    CPPUNIT_ASSERT_EQUAL(std::string("<div>Reply comment</div>"), aView1.m_aCommentCallbackResult.get<std::string>("html"));
    CPPUNIT_ASSERT_EQUAL(std::string("<div>Reply comment</div>"), aView2.m_aCommentCallbackResult.get<std::string>("html"));
    int nCommentId2 = aView1.m_aCommentCallbackResult.get<int>("id");

    // Edit the previously added comment
    aCommandArgs = "{ \"Id\": { \"type\": \"string\", \"value\": \"" + OString::number(nCommentId2) + "\" }, \"Text\": { \"type\": \"string\", \"value\": \"Edited comment\" } }";
    pDocument->pClass->postUnoCommand(pDocument, ".uno:EditAnnotation", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Modify' action
    CPPUNIT_ASSERT_EQUAL(std::string("Modify"), aView1.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(std::string("Modify"), aView2.m_aCommentCallbackResult.get<std::string>("action"));
    // parent is unchanged still
    CPPUNIT_ASSERT_EQUAL(nCommentId1, aView1.m_aCommentCallbackResult.get<int>("parentId"));
    CPPUNIT_ASSERT_EQUAL(nCommentId1, aView2.m_aCommentCallbackResult.get<int>("parentId"));
    CPPUNIT_ASSERT_EQUAL(std::string("<div>Edited comment</div>"), aView1.m_aCommentCallbackResult.get<std::string>("html"));
    CPPUNIT_ASSERT_EQUAL(std::string("<div>Edited comment</div>"), aView2.m_aCommentCallbackResult.get<std::string>("html"));

    // Delete the reply comment just added
    aCommandArgs = "{ \"Id\": { \"type\": \"string\", \"value\":  \"" + OString::number(nCommentId2) + "\" } }";
    pDocument->pClass->postUnoCommand(pDocument, ".uno:DeleteComment", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Remove' action
    CPPUNIT_ASSERT_EQUAL(std::string("Remove"), aView1.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(std::string("Remove"), aView2.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(nCommentId2, aView1.m_aCommentCallbackResult.get<int>("id"));
    CPPUNIT_ASSERT_EQUAL(nCommentId2, aView2.m_aCommentCallbackResult.get<int>("id"));

    // Reply to nCommentId1 again
    aCommandArgs = "{ \"Id\": { \"type\": \"string\", \"value\": \"" + OString::number(nCommentId1) + "\" }, \"Html\": { \"type\": \"string\", \"value\": \"Reply comment again\" } }";
    pDocument->pClass->postUnoCommand(pDocument, ".uno:ReplyComment", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Add' action and linked to its parent comment
    CPPUNIT_ASSERT_EQUAL(std::string("Add"), aView1.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(std::string("Add"), aView2.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(nCommentId1, aView1.m_aCommentCallbackResult.get<int>("parentId"));
    CPPUNIT_ASSERT_EQUAL(nCommentId1, aView2.m_aCommentCallbackResult.get<int>("parentId"));
    CPPUNIT_ASSERT_EQUAL(std::string("<div>Reply comment again</div>"), aView1.m_aCommentCallbackResult.get<std::string>("html"));
    CPPUNIT_ASSERT_EQUAL(std::string("<div>Reply comment again</div>"), aView2.m_aCommentCallbackResult.get<std::string>("html"));

    // Ensure that an undo and redo restores the html contents
    aView1.m_aCommentCallbackResult.clear();
    aView2.m_aCommentCallbackResult.clear();
    pDocument->pClass->postUnoCommand(pDocument, ".uno:Undo", "", false);
    pDocument->pClass->postUnoCommand(pDocument, ".uno:Redo", "", false);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(std::string("<div>Reply comment again</div>"), aView1.m_aCommentCallbackResult.get<std::string>("html"));
    CPPUNIT_ASSERT_EQUAL(std::string("<div>Reply comment again</div>"), aView2.m_aCommentCallbackResult.get<std::string>("html"));

    // .uno:ViewAnnotations returns total of 5 comments
    boost::property_tree::ptree aTree;
    char* pJSON = pDocument->m_pDocumentClass->getCommandValues(pDocument, ".uno:ViewAnnotations");
    std::stringstream aStream(pJSON);
    free(pJSON);
    CPPUNIT_ASSERT(!aStream.str().empty());
    boost::property_tree::read_json(aStream, aTree);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), aTree.get_child("comments").size());
}

namespace
{

void addParameter(tools::JsonWriter& rJson, const char* sName, std::string_view type, std::string_view value)
{
    auto testNode = rJson.startNode(sName);
    rJson.put("type", type);
    rJson.put("value", value);
}

}

void DesktopLOKTest::testCommentsAddEditDeleteDraw()
{
    // Comments callback are emitted only if tiled annotations are off
    comphelper::LibreOfficeKit::setTiledAnnotations(false);
    LibLODocument_Impl* pDocument = loadDoc("BlankDrawDocument.odg");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aView1(pDocument);

    // Add a new comment
    OString aCommandArgs;
    {
        tools::JsonWriter aJson;
        addParameter(aJson, "Text", "string", "Comment");
        addParameter(aJson, "Author", "string", "LOK User1");
        aCommandArgs = aJson.finishAndGetAsOString();
    }

    pDocument->pClass->postUnoCommand(pDocument, ".uno:InsertAnnotation", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Add' action
    CPPUNIT_ASSERT_EQUAL(std::string("Add"), aView1.m_aCommentCallbackResult.get<std::string>("action"));
    int nCommentId1 = aView1.m_aCommentCallbackResult.get<int>("id");

    // Edit the previously added comment
    {
        tools::JsonWriter aJson;
        addParameter(aJson, "Id", "string", OString::number(nCommentId1));
        addParameter(aJson, "Text", "string", "Edited comment");
        aCommandArgs = aJson.finishAndGetAsOString();
    }

    pDocument->pClass->postUnoCommand(pDocument, ".uno:EditAnnotation", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Modify' action
    CPPUNIT_ASSERT_EQUAL(std::string("Modify"), aView1.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(nCommentId1, aView1.m_aCommentCallbackResult.get<int>("id"));

    // Delete Comment
    {
        tools::JsonWriter aJson;
        addParameter(aJson, "Id", "string", OString::number(nCommentId1));
        aCommandArgs = aJson.finishAndGetAsOString();
    }
    pDocument->pClass->postUnoCommand(pDocument, ".uno:DeleteAnnotation", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Remove' action
    CPPUNIT_ASSERT_EQUAL(std::string("Remove"), aView1.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(nCommentId1, aView1.m_aCommentCallbackResult.get<int>("id"));
}

void DesktopLOKTest::testCommentsInReadOnlyMode()
{
    // Comments callback are emitted only if tiled annotations are off
    comphelper::LibreOfficeKit::setTiledAnnotations(false);
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");

    int viewId = pDocument->m_pDocumentClass->createView(pDocument);
    pDocument->m_pDocumentClass->setView(pDocument, viewId);

    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{\".uno:Author\":{\"type\":\"string\",\"value\":\"LOK User1\"}}");

    SfxLokHelper::setViewReadOnly(viewId, true);
    SfxLokHelper::setAllowChangeComments(viewId, true);

    Scheduler::ProcessEventsToIdle();

    ViewCallback aView(pDocument);

    // Add a new comment
    OString aCommandArgs;
    {
        tools::JsonWriter aJson;
        addParameter(aJson, "Text", "string", "Comment");
        addParameter(aJson, "Author", "string", "LOK User1");
        aCommandArgs = aJson.finishAndGetAsOString();
    }

    pDocument->pClass->postUnoCommand(pDocument, ".uno:InsertAnnotation", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Add' action
    CPPUNIT_ASSERT_EQUAL(std::string("Add"), aView.m_aCommentCallbackResult.get<std::string>("action"));
    int nCommentId = aView.m_aCommentCallbackResult.get<int>("id");

    // Edit the previously added comment
    {
        tools::JsonWriter aJson;
        addParameter(aJson, "Id", "string", OString::number(nCommentId));
        addParameter(aJson, "Text", "string", "Edited comment");
        aCommandArgs = aJson.finishAndGetAsOString();
    }

    pDocument->pClass->postUnoCommand(pDocument, ".uno:EditAnnotation", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Modify' action
    CPPUNIT_ASSERT_EQUAL(std::string("Modify"), aView.m_aCommentCallbackResult.get<std::string>("action"));
    CPPUNIT_ASSERT_EQUAL(nCommentId, aView.m_aCommentCallbackResult.get<int>("id"));

    // Delete Comment
    {
        tools::JsonWriter aJson;
        addParameter(aJson, "Id", "string", OString::number(nCommentId));
        aCommandArgs = aJson.finishAndGetAsOString();
    }
    pDocument->pClass->postUnoCommand(pDocument, ".uno:DeleteAnnotation", aCommandArgs.getStr(), false);
    Scheduler::ProcessEventsToIdle();

    // Result is not sent for delete operation for some reason. But it is sent when debugging with online.
    // TODO: Enable below 2 checks.

    // We received a LOK_CALLBACK_COMMENT callback with comment 'Remove' action
    //CPPUNIT_ASSERT_EQUAL(std::string("Remove"), aView.m_aCommentCallbackResult.get<std::string>("action"));
    //CPPUNIT_ASSERT_EQUAL(nCommentId, aView.m_aCommentCallbackResult.get<int>("id"));
}

void DesktopLOKTest::testRedlinesInReadOnlyMode()
{
    // In AllowManageRedlines mode, it must be possible to perform redline editing commands,
    // even in read-only mode.

    using namespace std::string_literals;

    LibLODocument_Impl* pDocument = loadDoc("three-changes.fodt");

    int viewId = pDocument->m_pDocumentClass->createView(pDocument);
    pDocument->m_pDocumentClass->setView(pDocument, viewId);
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aCallback(pDocument);
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(size_t(3), getRedlineInfo(pDocument).size());

    // Activate read-only mode
    SfxLokHelper::setViewReadOnly(viewId, true);

    // Go to the 1st tracked change: "Delete “Donec”"
    pDocument->pClass->postUnoCommand(pDocument, ".uno:NextTrackedChange", {}, false);
    Scheduler::ProcessEventsToIdle();

    // Check that redline management commands don't work in pure read-only
    // Try to reject current redline
    pDocument->pClass->postUnoCommand(pDocument, ".uno:RejectTrackedChange", {}, false);
    Scheduler::ProcessEventsToIdle();
    // Nothing happened
    CPPUNIT_ASSERT_EQUAL(size_t(3), getRedlineInfo(pDocument).size());
    CPPUNIT_ASSERT_EQUAL(""s, aCallback.m_aLastRedlineInfo.action);
    CPPUNIT_ASSERT_EQUAL(""s, aCallback.m_aLastRedlineInfo.author);
    CPPUNIT_ASSERT_EQUAL(""s, aCallback.m_aLastRedlineInfo.type);
    CPPUNIT_ASSERT_EQUAL(""s, aCallback.m_aLastRedlineInfo.comment);
    CPPUNIT_ASSERT_EQUAL(""s, aCallback.m_aLastRedlineInfo.description);
    CPPUNIT_ASSERT_EQUAL(""s, aCallback.m_aLastRedlineInfo.dateTime);

    // Activate the AllowManageRedlines mode
    SfxLokHelper::setAllowManageRedlines(viewId, true);

    // Try to reject current redline
    pDocument->pClass->postUnoCommand(pDocument, ".uno:RejectTrackedChange", {}, false);
    Scheduler::ProcessEventsToIdle();
    // One change gone; it is recorded "Remove"d in aCallback.m_aLastRedlineInfo
    CPPUNIT_ASSERT_EQUAL(size_t(2), getRedlineInfo(pDocument).size());
    CPPUNIT_ASSERT_EQUAL("Remove"s, aCallback.m_aLastRedlineInfo.action);
    CPPUNIT_ASSERT_EQUAL("Mike"s, aCallback.m_aLastRedlineInfo.author);
    CPPUNIT_ASSERT_EQUAL("Delete"s, aCallback.m_aLastRedlineInfo.type);
    CPPUNIT_ASSERT_EQUAL(""s, aCallback.m_aLastRedlineInfo.comment);
    CPPUNIT_ASSERT_EQUAL("Delete “Donec”"s, aCallback.m_aLastRedlineInfo.description);
    CPPUNIT_ASSERT_EQUAL("2025-06-16T14:08:27"s, aCallback.m_aLastRedlineInfo.dateTime);

    // Go to the 2nd tracked change: "Attributes changed"
    pDocument->pClass->postUnoCommand(pDocument, ".uno:NextTrackedChange", {}, false);
    Scheduler::ProcessEventsToIdle();

    // Comment on it
    pDocument->pClass->postUnoCommand(pDocument, ".uno:CommentChangeTracking",
                                      R"({"Text":{"type":"string","value":"Some comment"}})",
                                      false);
    Scheduler::ProcessEventsToIdle();
    // One change got a comment; it is recorded "Modify"ed in aCallback.m_aLastRedlineInfo
    CPPUNIT_ASSERT_EQUAL(size_t(2), getRedlineInfo(pDocument).size());
    CPPUNIT_ASSERT_EQUAL("Modify"s, aCallback.m_aLastRedlineInfo.action);
    CPPUNIT_ASSERT_EQUAL("Mike"s, aCallback.m_aLastRedlineInfo.author);
    CPPUNIT_ASSERT_EQUAL("Format"s, aCallback.m_aLastRedlineInfo.type);
    CPPUNIT_ASSERT_EQUAL("Some comment"s, aCallback.m_aLastRedlineInfo.comment);
    CPPUNIT_ASSERT_EQUAL("Attributes changed"s, aCallback.m_aLastRedlineInfo.description);
    CPPUNIT_ASSERT_EQUAL("2025-06-17T12:41:00"s, aCallback.m_aLastRedlineInfo.dateTime);

    // Go to the 3rd tracked change: "Insert “ Sapienti sat.”"
    pDocument->pClass->postUnoCommand(pDocument, ".uno:NextTrackedChange", {}, false);
    Scheduler::ProcessEventsToIdle();

    // Accept it
    pDocument->pClass->postUnoCommand(pDocument, ".uno:AcceptTrackedChange", {}, false);
    Scheduler::ProcessEventsToIdle();
    // One change gone; it is recorded "Remove"d in aCallback.m_aLastRedlineInfo
    CPPUNIT_ASSERT_EQUAL(size_t(1), getRedlineInfo(pDocument).size());
    CPPUNIT_ASSERT_EQUAL("Remove"s, aCallback.m_aLastRedlineInfo.action);
    CPPUNIT_ASSERT_EQUAL("Mike"s, aCallback.m_aLastRedlineInfo.author);
    CPPUNIT_ASSERT_EQUAL("Insert"s, aCallback.m_aLastRedlineInfo.type);
    CPPUNIT_ASSERT_EQUAL(""s, aCallback.m_aLastRedlineInfo.comment);
    CPPUNIT_ASSERT_EQUAL("Insert “ Sapienti sat.”"s, aCallback.m_aLastRedlineInfo.description);
    CPPUNIT_ASSERT_EQUAL("2025-06-17T12:41:19"s, aCallback.m_aLastRedlineInfo.dateTime);

    // Make sure that another (unrelated to redline management) editing command is not working
    pDocument->pClass->postUnoCommand(pDocument, ".uno:InsertAnnotation",
                                      R"({"Text":{"type":"string","value":"Comment"}})",
                                      false);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT(aCallback.m_aCommentCallbackResult.empty());

    // Check that the same command would succeed in AllowChangeComments mode
    SfxLokHelper::setAllowChangeComments(viewId, true);
    pDocument->pClass->postUnoCommand(pDocument, ".uno:InsertAnnotation",
                                      R"({"Text":{"type":"string","value":"Comment"}})",
                                      false);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT(!aCallback.m_aCommentCallbackResult.empty());
}

void DesktopLOKTest::testCalcValidityDropdown()
{
    LibLODocument_Impl* pDocument = loadDoc("validity.ods");
    Scheduler::ProcessEventsToIdle();
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    Scheduler::ProcessEventsToIdle();

    ViewCallback aView(pDocument);
    Scheduler::ProcessEventsToIdle();

    // Select row 1 from column 1.
    pDocument->pClass->postMouseEvent(pDocument, LOK_MOUSEEVENT_MOUSEBUTTONDOWN, 1000, 150, 1, 1, 0);
    Scheduler::ProcessEventsToIdle();
    pDocument->pClass->postMouseEvent(pDocument, LOK_MOUSEEVENT_MOUSEBUTTONUP, 1000, 150, 1, 1, 0);
    Scheduler::ProcessEventsToIdle();

    // Open dropdown.
    pDocument->pClass->postMouseEvent(pDocument, LOK_MOUSEEVENT_MOUSEBUTTONDOWN, 1380, 150, 1, 1, 0);
    Scheduler::ProcessEventsToIdle();
    pDocument->pClass->postMouseEvent(pDocument, LOK_MOUSEEVENT_MOUSEBUTTONUP, 1380, 150, 1, 1, 0);
    Scheduler::ProcessEventsToIdle();

    // Select some value from dropdown.
    pDocument->pClass->sendDialogEvent(pDocument, aView.m_JSONDialog.get_child("id").get_value<int>(), "{\"id\":\"list\", \"cmd\": \"select\", \"data\": \"3\", \"type\": \"treeview\"}");
    Scheduler::ProcessEventsToIdle();

    // Activate the selected value.
    pDocument->pClass->sendDialogEvent(pDocument, aView.m_JSONDialog.get_child("id").get_value<int>(), "{\"id\":\"list\", \"cmd\": \"activate\", \"data\": \"3\", \"type\": \"treeview\"}");
    Scheduler::ProcessEventsToIdle();

    // Check the content of the current cell. The selected value of the dropdown was 1. It should be 4 now.
    char* pCellContent = pDocument->pClass->getTextSelection(pDocument, "text/plain;charset=utf-8", nullptr);
    CPPUNIT_ASSERT_EQUAL("4"_ostr, OString(pCellContent));
    free(pCellContent);
}

void DesktopLOKTest::testCalcValidityDropdownInReadonlyMode()
{
    LibLODocument_Impl* pDocument = loadDoc("validity.ods");
    Scheduler::ProcessEventsToIdle();
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    Scheduler::ProcessEventsToIdle();

    ViewCallback aView(pDocument);
    Scheduler::ProcessEventsToIdle();

    int viewId = pDocument->m_pDocumentClass->getView(pDocument);
    SfxLokHelper::setViewReadOnly(viewId, true);
    Scheduler::ProcessEventsToIdle();

    // Select row 1 from column 1.
    pDocument->pClass->postMouseEvent(pDocument, LOK_MOUSEEVENT_MOUSEBUTTONDOWN, 1000, 150, 1, 1, 0);
    Scheduler::ProcessEventsToIdle();
    pDocument->pClass->postMouseEvent(pDocument, LOK_MOUSEEVENT_MOUSEBUTTONUP, 1000, 150, 1, 1, 0);
    Scheduler::ProcessEventsToIdle();

    // Attempt to open dropdown.
    pDocument->pClass->postMouseEvent(pDocument, LOK_MOUSEEVENT_MOUSEBUTTONDOWN, 1380, 150, 1, 1, 0);
    Scheduler::ProcessEventsToIdle();
    pDocument->pClass->postMouseEvent(pDocument, LOK_MOUSEEVENT_MOUSEBUTTONUP, 1380, 150, 1, 1, 0);
    Scheduler::ProcessEventsToIdle();

    // Dropdown should not open in readonly mode.
    CPPUNIT_ASSERT_EQUAL(true, aView.m_JSONDialog.empty());
}

void DesktopLOKTest::testRunMacro()
{
    LibLibreOffice_Impl aOffice;
    bool bGoodMacro, bNonExistentMacro;

    // Tools macros come pre-installed in system share/basic folder,
    bGoodMacro = aOffice.m_pOfficeClass->runMacro(&aOffice, "macro:///Tools.Debug.ActivateReadOnlyFlag()");
    CPPUNIT_ASSERT(bGoodMacro);

    bNonExistentMacro = aOffice.m_pOfficeClass->runMacro(&aOffice, "macro:///I.Am.Not(There)");
    CPPUNIT_ASSERT(!bNonExistentMacro);
}

void DesktopLOKTest::testExtractParameter()
{
    OUString aOptions(u"Language=de-DE"_ustr);
    OUString aValue = extractParameter(aOptions, u"Language");
    CPPUNIT_ASSERT_EQUAL(u"de-DE"_ustr, aValue);
    CPPUNIT_ASSERT_EQUAL(OUString(), aOptions);

    aOptions = "Language=en-US,Something";
    aValue = extractParameter(aOptions, u"Language");
    CPPUNIT_ASSERT_EQUAL(u"en-US"_ustr, aValue);
    CPPUNIT_ASSERT_EQUAL(u"Something"_ustr, aOptions);

    aOptions = "SomethingElse,Language=cs-CZ";
    aValue = extractParameter(aOptions, u"Language");
    CPPUNIT_ASSERT_EQUAL(u"cs-CZ"_ustr, aValue);
    CPPUNIT_ASSERT_EQUAL(u"SomethingElse"_ustr, aOptions);

    aOptions = "Something1,Language=hu-HU,Something2";
    aValue = extractParameter(aOptions, u"Language");
    CPPUNIT_ASSERT_EQUAL(u"hu-HU"_ustr, aValue);
    CPPUNIT_ASSERT_EQUAL(u"Something1,Something2"_ustr, aOptions);

    aOptions = "Something1,Something2=blah,Something3";
    aValue = extractParameter(aOptions, u"Language");
    CPPUNIT_ASSERT_EQUAL(OUString(), aValue);
    CPPUNIT_ASSERT_EQUAL(u"Something1,Something2=blah,Something3"_ustr, aOptions);
}

void DesktopLOKTest::readFileIntoByteVector(std::u16string_view sFilename, std::vector<unsigned char> & rByteVector)
{
    rByteVector.clear();
    OUString aURL = createFileURL(sFilename);
    SvFileStream aStream(aURL, StreamMode::READ);
    rByteVector.resize(aStream.remainingSize());
    aStream.ReadBytes(rByteVector.data(), aStream.remainingSize());
}

void DesktopLOKTest::testGetSignatureState_Signed()
{
    LibLODocument_Impl* pDocument = loadDoc("signed.odt");
    Scheduler::ProcessEventsToIdle();
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    int nState = pDocument->m_pDocumentClass->getSignatureState(pDocument);
    if (nState == 1)
    {
        // Already SignatureState::OK, then can't test the effect of trusting new CAs.
        return;
    }

    CPPUNIT_ASSERT_EQUAL(int(4), nState);

    std::vector<unsigned char> aCertificate;
    {
        readFileIntoByteVector(u"rootCA.der", aCertificate);
        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"intermediateRootCA.der", aCertificate);
        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    nState = pDocument->m_pDocumentClass->getSignatureState(pDocument);
    CPPUNIT_ASSERT_EQUAL(int(1), nState);
}

void DesktopLOKTest::testGetSignatureState_NonSigned()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    Scheduler::ProcessEventsToIdle();
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    int nState = pDocument->m_pDocumentClass->getSignatureState(pDocument);
    CPPUNIT_ASSERT_EQUAL(int(0), nState);
}

#if 0 // broken with system nss on RHEL 7
void DesktopLOKTest::testInsertCertificate_DER_ODT()
{
    // Load the document, save it into a temp file and load that file again
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    CPPUNIT_ASSERT(pDocument->pClass->saveAs(pDocument, maTempFile.GetURL().toUtf8().getStr(), "odt", nullptr));
    closeDoc();

    pDocument = loadDocUrl(maTempFile.GetURL(), LOK_DOCTYPE_TEXT);

    Scheduler::ProcessEventsToIdle();
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    Scheduler::ProcessEventsToIdle();

    std::vector<unsigned char> aCertificate;
    std::vector<unsigned char> aPrivateKey;

    {
        readFileIntoByteVector(u"rootCA.der", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"intermediateRootCA.der", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"certificate.der", aCertificate);
        readFileIntoByteVector(u"certificatePrivateKey.der", aPrivateKey);

        bool bResult = pDocument->m_pDocumentClass->insertCertificate(pDocument,
                            aCertificate.data(), int(aCertificate.size()),
                            aPrivateKey.data(), int(aPrivateKey.size()));
        CPPUNIT_ASSERT(bResult);
    }

    int nState = pDocument->m_pDocumentClass->getSignatureState(pDocument);
    CPPUNIT_ASSERT_EQUAL(int(1), nState);
}


void DesktopLOKTest::testInsertCertificate_PEM_ODT()
{
    // Load the document, save it into a temp file and load that file again
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    CPPUNIT_ASSERT(pDocument->pClass->saveAs(pDocument, maTempFile.GetURL().toUtf8().getStr(), "odt", nullptr));
    closeDoc();

    pDocument = loadDocUrl(maTempFile.GetURL(), LOK_DOCTYPE_TEXT);

    Scheduler::ProcessEventsToIdle();
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    Scheduler::ProcessEventsToIdle();

    std::vector<unsigned char> aCertificate;
    std::vector<unsigned char> aPrivateKey;

    {
        readFileIntoByteVector(u"test-cert-chain-1.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"test-cert-chain-2.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"test-cert-chain-3.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"test-cert-signing.pem", aCertificate);
        readFileIntoByteVector(u"test-PK-signing.pem", aPrivateKey);

        bool bResult = pDocument->m_pDocumentClass->insertCertificate(pDocument,
                            aCertificate.data(), int(aCertificate.size()),
                            aPrivateKey.data(), int(aPrivateKey.size()));
        CPPUNIT_ASSERT(bResult);
    }

    int nState = pDocument->m_pDocumentClass->getSignatureState(pDocument);
    CPPUNIT_ASSERT_EQUAL(int(1), nState);
}

void DesktopLOKTest::testInsertCertificate_PEM_DOCX()
{
    // Load the document, save it into a temp file and load that file again
    LibLODocument_Impl* pDocument = loadDoc("blank_text.docx");
    CPPUNIT_ASSERT(pDocument->pClass->saveAs(pDocument, maTempFile.GetURL().toUtf8().getStr(), "docx", nullptr));
    closeDoc();

    pDocument = loadDocUrl(maTempFile.GetURL(), LOK_DOCTYPE_TEXT);

    Scheduler::ProcessEventsToIdle();
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    Scheduler::ProcessEventsToIdle();

    std::vector<unsigned char> aCertificate;
    std::vector<unsigned char> aPrivateKey;

    {
        readFileIntoByteVector(u"test-cert-chain-1.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"test-cert-chain-2.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"test-cert-chain-3.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"test-cert-signing.pem", aCertificate);
        readFileIntoByteVector(u"test-PK-signing.pem", aPrivateKey);

        bool bResult = pDocument->m_pDocumentClass->insertCertificate(pDocument,
                            aCertificate.data(), int(aCertificate.size()),
                            aPrivateKey.data(), int(aPrivateKey.size()));
        CPPUNIT_ASSERT(bResult);
    }

    int nState = pDocument->m_pDocumentClass->getSignatureState(pDocument);
    CPPUNIT_ASSERT_EQUAL(int(5), nState);
}
#endif

void DesktopLOKTest::testSignDocument_PEM_PDF()
{
    // Load the document, save it into a temp file and load that file again
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");

    Scheduler::ProcessEventsToIdle();
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    Scheduler::ProcessEventsToIdle();

    std::vector<unsigned char> aCertificate;
    std::vector<unsigned char> aPrivateKey;

    {
        readFileIntoByteVector(u"test-cert-chain-1.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"test-cert-chain-2.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    {
        readFileIntoByteVector(u"test-cert-chain-3.pem", aCertificate);

        bool bResult = pDocument->m_pDocumentClass->addCertificate(
                            pDocument, aCertificate.data(), int(aCertificate.size()));
        CPPUNIT_ASSERT(bResult);
    }

    CPPUNIT_ASSERT(pDocument->pClass->saveAs(pDocument, maTempFile.GetURL().toUtf8().getStr(), "pdf", nullptr));

    closeDoc();

    Scheduler::ProcessEventsToIdle();

    readFileIntoByteVector(u"test-cert-signing.pem", aCertificate);
    readFileIntoByteVector(u"test-PK-signing.pem", aPrivateKey);

    LibLibreOffice_Impl aOffice;
    bool bResult = aOffice.m_pOfficeClass->signDocument(&aOffice, maTempFile.GetURL().toUtf8().getStr(),
                                         aCertificate.data(), int(aCertificate.size()),
                                         aPrivateKey.data(), int(aPrivateKey.size()));

    CPPUNIT_ASSERT(bResult);
}

void DesktopLOKTest::testTextSelectionHandles()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    OString aText("hello"_ostr);
    CPPUNIT_ASSERT(pDocument->pClass->paste(pDocument, "text/plain;charset=utf-8", aText.getStr(), aText.getLength()));

    // select the inserted text
    pDocument->pClass->postUnoCommand(pDocument, ".uno:SelectAll", nullptr, false);
    Scheduler::ProcessEventsToIdle();
    char* pText = pDocument->pClass->getTextSelection(pDocument, "text/plain;charset=utf-8", nullptr);
    CPPUNIT_ASSERT_EQUAL(aText, OString(pText));
    free(pText);
    CPPUNIT_ASSERT_EQUAL("1418, 1418, 0, 275"_ostr, m_aTextSelectionStart);
    CPPUNIT_ASSERT_EQUAL("1897, 1418, 0, 275"_ostr, m_aTextSelectionEnd);

    // deselect & check
    m_aTextSelectionStart = ""_ostr;
    m_aTextSelectionEnd = ""_ostr;
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, css::awt::Key::ESCAPE);
    Scheduler::ProcessEventsToIdle();
    pText = pDocument->pClass->getTextSelection(pDocument, "text/plain;charset=utf-8", nullptr);
    CPPUNIT_ASSERT_EQUAL(static_cast<char *>(nullptr), pText);
    free(pText);
    CPPUNIT_ASSERT_EQUAL(OString(), m_aTextSelectionStart);
    CPPUNIT_ASSERT_EQUAL(OString(), m_aTextSelectionEnd);

    // select again; the positions of the selection handles have to be sent
    // again
    pDocument->pClass->postUnoCommand(pDocument, ".uno:SelectAll", nullptr, false);
    Scheduler::ProcessEventsToIdle();
    pText = pDocument->pClass->getTextSelection(pDocument, "text/plain;charset=utf-8", nullptr);
    CPPUNIT_ASSERT_EQUAL(aText, OString(pText));
    free(pText);
    CPPUNIT_ASSERT_EQUAL("1418, 1418, 0, 275"_ostr, m_aTextSelectionStart);
    CPPUNIT_ASSERT_EQUAL("1897, 1418, 0, 275"_ostr, m_aTextSelectionEnd);
}

void DesktopLOKTest::testDialogPaste()
{
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    pDocument->pClass->postUnoCommand(pDocument, ".uno:HyperlinkDialog", nullptr, false);
    Scheduler::ProcessEventsToIdle();

    SfxViewShell* pViewShell = SfxViewShell::Current();
    pViewShell->GetViewFrame().GetBindings().Update();

    VclPtr<vcl::Window> pWindow(Application::GetActiveTopWindow());
    CPPUNIT_ASSERT(pWindow);

    pDocument->pClass->postWindow(pDocument, pWindow->GetLOKWindowId(), LOK_WINDOW_PASTE,
            "{ \"MimeType\" : { \"type\" : \"string\", \"value\" : \"text/plain;charset=utf-8\" }, \"Data\" : { \"type\" : \"[]byte\", \"value\" : \"www.softwarelibre.org.bo\" } }");
    Scheduler::ProcessEventsToIdle();

    Control* pCtrlFocused = GetFocusControl(pWindow.get());
    CPPUNIT_ASSERT(pCtrlFocused);
    CPPUNIT_ASSERT_EQUAL(WindowType::COMBOBOX, pCtrlFocused->GetType());
    CPPUNIT_ASSERT_EQUAL(u"www.softwarelibre.org.bo"_ustr, pCtrlFocused->GetText());

    static_cast<SystemWindow*>(pWindow.get())->Close();
    Scheduler::ProcessEventsToIdle();
}

void DesktopLOKTest::testComplexSelection()
{
    // Start with a blank text file and add contents.
    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    static constexpr OString aText("hello world"_ostr);

    // Certainly not complex.
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(LOK_SELTYPE_NONE), pDocument->pClass->getSelectionType(pDocument));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(LOK_SELTYPE_NONE), pDocument->pClass->getSelectionTypeAndText(pDocument,
                                                                 "", nullptr, nullptr));

    // Paste text.
    CPPUNIT_ASSERT(pDocument->pClass->paste(pDocument, "text/plain;charset=utf-8", aText.getStr(), aText.getLength()));

    // No selection.
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(LOK_SELTYPE_NONE), pDocument->pClass->getSelectionType(pDocument));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(LOK_SELTYPE_NONE), pDocument->pClass->getSelectionTypeAndText(pDocument,
                                                                 "", nullptr, nullptr));

    // Paste an image.
    OUString aFileURL = createFileURL(u"paste.jpg");
    std::ifstream aImageStream(aFileURL.toUtf8().copy(strlen("file://")).getStr());
    std::vector<char> aImageContents((std::istreambuf_iterator<char>(aImageStream)), std::istreambuf_iterator<char>());
    CPPUNIT_ASSERT(pDocument->pClass->paste(pDocument, "image/jpeg", aImageContents.data(), aImageContents.size()));

    // Now select-all.
    pDocument->pClass->postUnoCommand(pDocument, ".uno:SelectAll", nullptr, false);
    Scheduler::ProcessEventsToIdle();

    // Export as plain text, we should get only the text part "hello".
    char* pText = pDocument->pClass->getTextSelection(pDocument, "text/plain;charset=utf-8", nullptr);
    CPPUNIT_ASSERT(pText != nullptr);
    CPPUNIT_ASSERT_EQUAL(aText, OString(pText));
    free(pText);

    // Export as rtf, we should also get the image.
    pText = pDocument->pClass->getTextSelection(pDocument, "text/rtf", nullptr);
    CPPUNIT_ASSERT(pText != nullptr);
    CPPUNIT_ASSERT(std::string(pText).find(aText.getStr()) != std::string::npos); // Must have the text.
    CPPUNIT_ASSERT(std::string(pText).find("pict{") != std::string::npos); // Must have the image as well.
    free(pText);

    // Export as html, we should also get the image.
    pText = pDocument->pClass->getTextSelection(pDocument, "text/html", nullptr);
    CPPUNIT_ASSERT(pText != nullptr);
    CPPUNIT_ASSERT(std::string(pText).find(aText.getStr()) != std::string::npos); // Must have the text.
    CPPUNIT_ASSERT(std::string(pText).find("<img") != std::string::npos); // Must have the image as well.
    free(pText);

    // We expect this to be complex.
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(LOK_SELTYPE_COMPLEX), pDocument->pClass->getSelectionType(pDocument));
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(LOK_SELTYPE_COMPLEX), pDocument->pClass->getSelectionTypeAndText(pDocument,
                                                                 "", nullptr, nullptr));
}

void DesktopLOKTest::testCalcSaveAs()
{
    LibLODocument_Impl* pDocument = loadDoc("sheets.ods");
    CPPUNIT_ASSERT(pDocument);

    // Enter some text, but don't commit.
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'X', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 'X', 0);
    Scheduler::ProcessEventsToIdle();

    // Save as a new file.
    pDocument->pClass->saveAs(pDocument, maTempFile.GetURL().toUtf8().getStr(), "ods", nullptr);
    closeDoc();

    // Load the new document and verify that the in-flight changes are saved.
    pDocument = loadDocUrl(maTempFile.GetURL(), LOK_DOCTYPE_SPREADSHEET);
    CPPUNIT_ASSERT(pDocument);

    ViewCallback aView(pDocument);
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    pDocument->m_pDocumentClass->registerCallback(pDocument, &ViewCallback::callback, &aView);

    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, KEY_RIGHT);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 0, KEY_RIGHT);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, KEY_LEFT);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, 0, KEY_LEFT);
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL("X"_ostr, aView.m_aCellFormula);
}

void DesktopLOKTest::testSpellcheckerMultiView()
{
    static constexpr OUString aLangISO(u"en-US"_ustr);
    SvtSysLocaleOptions aSysLocaleOptions;
    aSysLocaleOptions.SetLocaleConfigString(aLangISO);
    aSysLocaleOptions.SetUILocaleConfigString(aLangISO);
    comphelper::LibreOfficeKit::setLanguageTag(LanguageTag(aLangISO, true));

    auto aSavedSettings = Application::GetSettings();
    std::unique_ptr<Resetter> pResetter(
            new Resetter([&]() { Application::SetSettings(aSavedSettings); }));
    AllSettings aSettings(aSavedSettings);
    aSettings.SetLanguageTag(aLangISO, true);
    Application::SetSettings(aSettings);

    LibLODocument_Impl* pDocument = loadDoc("sheet_with_image.ods", LOK_DOCTYPE_SPREADSHEET);
    pDocument->pClass->setViewLanguage(pDocument, 0, "en-US"); // For spellchecking.
    pDocument->pClass->initializeForRendering(pDocument, nullptr);
    pDocument->pClass->registerCallback(pDocument, &DesktopLOKTest::callback, this);

    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'a', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'a', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'a', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, css::awt::Key::ESCAPE);

    // Start spellchecking.
    pDocument->pClass->postUnoCommand(pDocument, ".uno:SpellDialog", nullptr, false);

    // Uncommenting this will result in a deadlock.
    // Because the language configuration above is not effective, and no
    // language is actually set, the spell-dialog finds no misspelled
    // words, and displays a message box, which must be dismissed to
    // continue.
    // Need to fix the language configuration issue to enable this.
    // Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(1, pDocument->m_pDocumentClass->getViewsCount(pDocument));

    // Now create another view.
    const int nViewId = pDocument->m_pDocumentClass->createView(pDocument);
    CPPUNIT_ASSERT_EQUAL(2, pDocument->m_pDocumentClass->getViewsCount(pDocument));

    // And destroy it.
    pDocument->m_pDocumentClass->destroyView(pDocument, nViewId);

    // We should survive the destroyed view.
    CPPUNIT_ASSERT_EQUAL(1, pDocument->m_pDocumentClass->getViewsCount(pDocument));
}

void DesktopLOKTest::testMultiDocuments()
{
    for (int i = 0; i < 3; i++)
    {
        // Load a document.
        std::unique_ptr<LibLODocument_Impl> document1 = loadDocImpl("blank_text.odt");
        LibLODocument_Impl* pDocument1 = document1.get();
        CPPUNIT_ASSERT_EQUAL(1, pDocument1->m_pDocumentClass->getViewsCount(pDocument1));
        const int nDocId1 = pDocument1->mnDocumentId;

        const int nDoc1View0 = pDocument1->m_pDocumentClass->getView(pDocument1);
        CPPUNIT_ASSERT_EQUAL(nDocId1, SfxLokHelper::getDocumentIdOfView(nDoc1View0));
        const int nDoc1View1 = pDocument1->m_pDocumentClass->createView(pDocument1);
        CPPUNIT_ASSERT_EQUAL(nDoc1View1, pDocument1->m_pDocumentClass->getView(pDocument1));
        CPPUNIT_ASSERT_EQUAL(nDocId1, SfxLokHelper::getDocumentIdOfView(nDoc1View1));
        CPPUNIT_ASSERT_EQUAL(2, pDocument1->m_pDocumentClass->getViewsCount(pDocument1));

        // Validate the views of document 1.
        std::vector<int> aViewIdsDoc1(2);
        CPPUNIT_ASSERT(pDocument1->m_pDocumentClass->getViewIds(pDocument1, aViewIdsDoc1.data(), aViewIdsDoc1.size()));
        CPPUNIT_ASSERT_EQUAL(nDoc1View0, aViewIdsDoc1[0]);
        CPPUNIT_ASSERT_EQUAL(nDoc1View1, aViewIdsDoc1[1]);

        CPPUNIT_ASSERT_EQUAL(nDoc1View1, pDocument1->m_pDocumentClass->getView(pDocument1));
        CPPUNIT_ASSERT_EQUAL(nDocId1, SfxLokHelper::getDocumentIdOfView(nDoc1View1));
        pDocument1->m_pDocumentClass->setView(pDocument1, nDoc1View0);
        CPPUNIT_ASSERT_EQUAL(nDoc1View0, pDocument1->m_pDocumentClass->getView(pDocument1));
        CPPUNIT_ASSERT_EQUAL(nDocId1, SfxLokHelper::getDocumentIdOfView(nDoc1View0));
        pDocument1->m_pDocumentClass->setView(pDocument1, nDoc1View1);
        CPPUNIT_ASSERT_EQUAL(nDoc1View1, pDocument1->m_pDocumentClass->getView(pDocument1));
        CPPUNIT_ASSERT_EQUAL(nDocId1, SfxLokHelper::getDocumentIdOfView(nDoc1View1));
        CPPUNIT_ASSERT_EQUAL(2, pDocument1->m_pDocumentClass->getViewsCount(pDocument1));

        // Load another document.
        std::unique_ptr<LibLODocument_Impl> document2 = loadDocImpl("blank_presentation.odp");
        LibLODocument_Impl* pDocument2 = document2.get();
        CPPUNIT_ASSERT_EQUAL(1, pDocument2->m_pDocumentClass->getViewsCount(pDocument2));
        const int nDocId2 = pDocument2->mnDocumentId;

        const int nDoc2View0 = pDocument2->m_pDocumentClass->getView(pDocument2);
        CPPUNIT_ASSERT_EQUAL(nDocId2, SfxLokHelper::getDocumentIdOfView(nDoc2View0));
        const int nDoc2View1 = pDocument2->m_pDocumentClass->createView(pDocument2);
        CPPUNIT_ASSERT_EQUAL(nDoc2View1, pDocument2->m_pDocumentClass->getView(pDocument2));
        CPPUNIT_ASSERT_EQUAL(nDocId2, SfxLokHelper::getDocumentIdOfView(nDoc2View1));
        CPPUNIT_ASSERT_EQUAL(2, pDocument2->m_pDocumentClass->getViewsCount(pDocument2));

        // Validate the views of document 2.
        std::vector<int> aViewIdsDoc2(2);
        CPPUNIT_ASSERT(pDocument2->m_pDocumentClass->getViewIds(pDocument2, aViewIdsDoc2.data(), aViewIdsDoc2.size()));
        CPPUNIT_ASSERT_EQUAL(nDoc2View0, aViewIdsDoc2[0]);
        CPPUNIT_ASSERT_EQUAL(nDoc2View1, aViewIdsDoc2[1]);

        CPPUNIT_ASSERT_EQUAL(nDoc2View1, pDocument2->m_pDocumentClass->getView(pDocument2));
        CPPUNIT_ASSERT_EQUAL(nDocId2, SfxLokHelper::getDocumentIdOfView(nDoc2View1));
        pDocument2->m_pDocumentClass->setView(pDocument2, nDoc2View0);
        CPPUNIT_ASSERT_EQUAL(nDoc2View0, pDocument2->m_pDocumentClass->getView(pDocument2));
        CPPUNIT_ASSERT_EQUAL(nDocId2, SfxLokHelper::getDocumentIdOfView(nDoc2View0));
        pDocument2->m_pDocumentClass->setView(pDocument2, nDoc2View1);
        CPPUNIT_ASSERT_EQUAL(nDoc2View1, pDocument2->m_pDocumentClass->getView(pDocument2));
        CPPUNIT_ASSERT_EQUAL(nDocId2, SfxLokHelper::getDocumentIdOfView(nDoc2View1));
        CPPUNIT_ASSERT_EQUAL(2, pDocument2->m_pDocumentClass->getViewsCount(pDocument2));

        // The views of document1 should be unchanged.
        CPPUNIT_ASSERT(pDocument1->m_pDocumentClass->getViewIds(pDocument1, aViewIdsDoc1.data(), aViewIdsDoc1.size()));
        CPPUNIT_ASSERT_EQUAL(nDoc1View0, aViewIdsDoc1[0]);
        CPPUNIT_ASSERT_EQUAL(nDoc1View1, aViewIdsDoc1[1]);
        // Switch views in the first doc.
        CPPUNIT_ASSERT_EQUAL(nDocId1, SfxLokHelper::getDocumentIdOfView(nDoc1View0));
        pDocument1->m_pDocumentClass->setView(pDocument1, nDoc1View0);
        CPPUNIT_ASSERT_EQUAL(nDoc1View0, pDocument1->m_pDocumentClass->getView(pDocument1));
        CPPUNIT_ASSERT_EQUAL(nDocId1, SfxLokHelper::getDocumentIdOfView(nDoc1View1));
        pDocument1->m_pDocumentClass->destroyView(pDocument1, nDoc1View1);
        CPPUNIT_ASSERT_EQUAL(1, pDocument1->m_pDocumentClass->getViewsCount(pDocument1));

        // The views of document2 should be unchanged.
        CPPUNIT_ASSERT(pDocument2->m_pDocumentClass->getViewIds(pDocument2, aViewIdsDoc2.data(), aViewIdsDoc2.size()));
        CPPUNIT_ASSERT_EQUAL(nDoc2View0, aViewIdsDoc2[0]);
        CPPUNIT_ASSERT_EQUAL(nDoc2View1, aViewIdsDoc2[1]);
        // Switch views in the second doc.
        CPPUNIT_ASSERT_EQUAL(nDocId2, SfxLokHelper::getDocumentIdOfView(nDoc2View0));
        pDocument2->m_pDocumentClass->setView(pDocument2, nDoc2View0);
        CPPUNIT_ASSERT_EQUAL(nDoc2View0, pDocument2->m_pDocumentClass->getView(pDocument2));
        CPPUNIT_ASSERT_EQUAL(nDocId2, SfxLokHelper::getDocumentIdOfView(nDoc2View1));
        pDocument2->m_pDocumentClass->destroyView(pDocument2, nDoc2View1);
        CPPUNIT_ASSERT_EQUAL(1, pDocument2->m_pDocumentClass->getViewsCount(pDocument2));

        closeDoc(document2);

        closeDoc(document1);
    }
}

void DesktopLOKTest::testControlState()
{
    LibLODocument_Impl* pDocument = loadDoc("search.ods");
    pDocument->pClass->postUnoCommand(pDocument, ".uno:StarShapes", nullptr, false);
    TestLokCallbackWrapper::InitializeSidebar();
    Scheduler::ProcessEventsToIdle();

    boost::property_tree::ptree aState;
    SfxViewShell* pViewShell = SfxViewShell::Current();
    pViewShell->GetViewFrame().GetBindings().Update();
    pViewShell->GetViewFrame().GetBindings().QueryControlState(SID_ATTR_TRANSFORM_WIDTH, aState);
    CPPUNIT_ASSERT(!aState.empty());
}

void DesktopLOKTest::testMetricField()
{
    LibLODocument_Impl* pDocument = loadDoc("search.ods");
    pDocument->pClass->postUnoCommand(pDocument, ".uno:StarShapes", nullptr, false);
    SfxChildWindow* pSideBar = TestLokCallbackWrapper::InitializeSidebar();
    Scheduler::ProcessEventsToIdle();

    vcl::Window* pWin = pSideBar->GetWindow();
    CPPUNIT_ASSERT(pWin);

    WindowUIObject aWinUI(pWin);
    std::unique_ptr<UIObject> pUIWin(aWinUI.get_child(u"selectwidth"_ustr));
    CPPUNIT_ASSERT(pUIWin);

    StringMap aMap;
    aMap[u"VALUE"_ustr] = "75.06";
    pUIWin->execute(u"VALUE"_ustr, aMap);

    StringMap aRet = pUIWin->get_state();
    CPPUNIT_ASSERT_EQUAL(aMap[u"VALUE"_ustr], aRet[u"Value"_ustr]);
}

void DesktopLOKTest::testJumpCursor()
{
    comphelper::LibreOfficeKit::setTiledAnnotations(false);

    LibLODocument_Impl* pDocument = loadDoc("blank_text.odt");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");

    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'B', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'o', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'l', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'i', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'v', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'i', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 'a', 0);
    pDocument->pClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, 0, css::awt::Key::ESCAPE);
    Scheduler::ProcessEventsToIdle();

    // There is a cursor jump to (0, 0) due to
    // mpOutlinerView->SetOutputArea( PixelToLogic( tools::Rectangle(0,0,1,1) ) );
    // when creating a comment
    ViewCallback aView1(pDocument);

    pDocument->pClass->postUnoCommand(pDocument, ".uno:InsertAnnotation", nullptr, true);
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT(!aView1.m_bZeroCursor);

    comphelper::LibreOfficeKit::setTiledAnnotations(true);
}

void DesktopLOKTest::testRenderSearchResult_WriterNode()
{
    constexpr const bool bDumpBitmap = false;

    LibLODocument_Impl* pDocument = loadDoc("SearchIndexResultTest.odt");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");

    Scheduler::ProcessEventsToIdle();

    unsigned char* pBuffer = nullptr;
    OString aPayload =
    "<indexing>"
        "<paragraph node_type=\"writer\" index=\"19\">ABC</paragraph>"
    "</indexing>"_ostr;

    int nWidth = 0;
    int nHeight = 0;
    size_t nByteSize = 0;

    bool bResult = pDocument->m_pDocumentClass->renderSearchResult(pDocument, aPayload.getStr(), &pBuffer, &nWidth, &nHeight, &nByteSize);

    CPPUNIT_ASSERT(bResult);
    CPPUNIT_ASSERT(pBuffer);

    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(642, nWidth);
    CPPUNIT_ASSERT_EQUAL(561, nHeight);
    CPPUNIT_ASSERT_EQUAL(size_t(1440648), nByteSize);

    const sal_uInt8* pD = reinterpret_cast<const sal_uInt8*>(pBuffer);
    BitmapEx aBitmap = vcl::bitmap::CreateFromData(pD, nWidth, nHeight, nWidth * 4, /*nBitsPerPixel*/32, true, true);

    if (bDumpBitmap)
    {
        SvFileStream aStream(u"~/SearchResultBitmap.png"_ustr, StreamMode::WRITE | StreamMode::TRUNC);
        vcl::PngImageWriter aPNGWriter(aStream);
        aPNGWriter.write(aBitmap);
    }
    CPPUNIT_ASSERT_EQUAL(tools::Long(642), aBitmap.GetSizePixel().Width());
    CPPUNIT_ASSERT_EQUAL(tools::Long(561), aBitmap.GetSizePixel().Height());

    std::free(pBuffer);
}

void DesktopLOKTest::testRenderSearchResult_CommonNode()
{
    constexpr const bool bDumpBitmap = false;

    LibLODocument_Impl* pDocument = loadDoc("SearchIndexResultShapeTest.odt");
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");

    Scheduler::ProcessEventsToIdle();

    unsigned char* pBuffer = nullptr;
    OString aPayload =
    "<indexing>"
        "<paragraph node_type=\"common\" index=\"0\" object_name=\"Shape 1\" />"
    "</indexing>"_ostr;

    int nWidth = 0;
    int nHeight = 0;
    size_t nByteSize = 0;

    bool bResult = pDocument->m_pDocumentClass->renderSearchResult(pDocument, aPayload.getStr(), &pBuffer, &nWidth, &nHeight, &nByteSize);

    CPPUNIT_ASSERT(bResult);
    CPPUNIT_ASSERT(pBuffer);

    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(192, nWidth);
    CPPUNIT_ASSERT_EQUAL(96, nHeight);
    CPPUNIT_ASSERT_EQUAL(size_t(73728), nByteSize);

    const sal_uInt8* pD = reinterpret_cast<const sal_uInt8*>(pBuffer);
    BitmapEx aBitmap = vcl::bitmap::CreateFromData(pD, nWidth, nHeight, nWidth * 4, /*nBitsPerPixel*/32, true, true);

    if (bDumpBitmap)
    {
        SvFileStream aStream(u"~/SearchResultBitmap.png"_ustr, StreamMode::WRITE | StreamMode::TRUNC);
        vcl::PngImageWriter aPNGWriter(aStream);
        aPNGWriter.write(aBitmap);
    }
    CPPUNIT_ASSERT_EQUAL(tools::Long(192), aBitmap.GetSizePixel().Width());
    CPPUNIT_ASSERT_EQUAL(tools::Long(96), aBitmap.GetSizePixel().Height());

    std::free(pBuffer);
}

static void lcl_repeatKeyStroke(LibLODocument_Impl *pDocument, int nCharCode, int nKeyCode, size_t nCount)
{
    for (size_t nCtr = 0; nCtr < nCount; ++nCtr)
    {
        pDocument->m_pDocumentClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYINPUT, nCharCode, nKeyCode);
        pDocument->m_pDocumentClass->postKeyEvent(pDocument, LOK_KEYEVENT_KEYUP, nCharCode, nKeyCode);
    }
}

void DesktopLOKTest::testNoDuplicateTableSelection()
{
    LibLODocument_Impl* pDocument = loadDoc("table-selection.odt");

    // Create view 1.
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aView1(pDocument);

    lcl_repeatKeyStroke(pDocument, 0, KEY_DOWN, 1);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(1, aView1.m_nTableSelectionCount);
    CPPUNIT_ASSERT(aView1.m_bEmptyTableSelection);

    aView1.m_nTableSelectionCount = 0;
    // Go to Table1.
    lcl_repeatKeyStroke(pDocument, 0, KEY_DOWN, 1);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(1, aView1.m_nTableSelectionCount);
    CPPUNIT_ASSERT(!aView1.m_bEmptyTableSelection);

    aView1.m_nTableSelectionCount = 0;
    // Move to the last row in Table1.
    lcl_repeatKeyStroke(pDocument, 0, KEY_DOWN, 2);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(0, aView1.m_nTableSelectionCount);

    // Go outside Table1.
    lcl_repeatKeyStroke(pDocument, 0, KEY_DOWN, 1);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(1, aView1.m_nTableSelectionCount);
    CPPUNIT_ASSERT(aView1.m_bEmptyTableSelection);
}

void DesktopLOKTest::testMultiViewTableSelection()
{
    LibLODocument_Impl* pDocument = loadDoc("table-selection.odt");

    // Create view 1.
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aView1(pDocument);
    int nView1 = pDocument->m_pDocumentClass->getView(pDocument);

    // Create view 2.
    pDocument->m_pDocumentClass->createView(pDocument);
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aView2(pDocument);
    int nView2 = pDocument->m_pDocumentClass->getView(pDocument);

    // switch to view 1.
    pDocument->m_pDocumentClass->setView(pDocument, nView1);
    lcl_repeatKeyStroke(pDocument, 0, KEY_DOWN, 1);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(1, aView1.m_nTableSelectionCount);
    CPPUNIT_ASSERT_EQUAL(1, aView2.m_nTableSelectionCount);
    CPPUNIT_ASSERT(aView1.m_bEmptyTableSelection);
    CPPUNIT_ASSERT(aView2.m_bEmptyTableSelection);

    aView1.m_nTableSelectionCount = 0;
    aView2.m_nTableSelectionCount = 0;

    pDocument->m_pDocumentClass->setView(pDocument, nView1);
    // Go to Table1.
    lcl_repeatKeyStroke(pDocument, 0, KEY_DOWN, 1);
    Scheduler::ProcessEventsToIdle();
    CPPUNIT_ASSERT_EQUAL(1, aView1.m_nTableSelectionCount);
    CPPUNIT_ASSERT_EQUAL(0, aView2.m_nTableSelectionCount);

    aView1.m_nTableSelectionCount = 0;
    // Switch to view 2
    pDocument->m_pDocumentClass->setView(pDocument, nView2);
    // Go to Table2 in view 2.
    lcl_repeatKeyStroke(pDocument, 0, KEY_DOWN, 7);
    Scheduler::ProcessEventsToIdle();
    // View1 should not get any table selection messages.
    CPPUNIT_ASSERT_EQUAL(0, aView1.m_nTableSelectionCount);
    // View2 will first get table selection of Table1, then empty selection, and finally on 7th down arrow keypress,
    // it will get table-selection of Table2. So in total it should get 3 table selections.
    CPPUNIT_ASSERT_EQUAL(3, aView2.m_nTableSelectionCount);
    CPPUNIT_ASSERT(!aView2.m_bEmptyTableSelection);

    aView1.m_nTableSelectionCount = 0;
    aView2.m_nTableSelectionCount = 0;

    // Switch to view 1
    pDocument->m_pDocumentClass->setView(pDocument, nView1);
    // Go out of Table1 and re-enter..
    lcl_repeatKeyStroke(pDocument, 0, KEY_UP, 1);
    lcl_repeatKeyStroke(pDocument, 0, KEY_DOWN, 1);
    Scheduler::ProcessEventsToIdle();
    // View1 should get one empty table selection, then get Table1 selection.
    CPPUNIT_ASSERT_EQUAL(2, aView1.m_nTableSelectionCount);
    // View2 should not get any table selection.
    CPPUNIT_ASSERT_EQUAL(0, aView2.m_nTableSelectionCount);
    CPPUNIT_ASSERT(!aView1.m_bEmptyTableSelection);
}

void DesktopLOKTest::testColorPaletteCallback()
{
    LibLODocument_Impl* pDocument = loadDoc("ThemeDocument.docx");

    // Create view 1.
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aView1(pDocument);
    Scheduler::ProcessEventsToIdle();
    {
        CPPUNIT_ASSERT_EQUAL(1, aView1.m_nColorPaletteCallbackCount);
        boost::property_tree::ptree aValues = aView1.m_aColorPaletteCallbackResult.get_child("ThemeColors");
        CPPUNIT_ASSERT(!aValues.empty());
        CPPUNIT_ASSERT_EQUAL(size_t(6), aValues.size());
    }

    // Create view 2.
    pDocument->m_pDocumentClass->createView(pDocument);
    pDocument->m_pDocumentClass->initializeForRendering(pDocument, "{}");
    ViewCallback aView2(pDocument);
    Scheduler::ProcessEventsToIdle();
    {
        CPPUNIT_ASSERT_EQUAL(1, aView2.m_nColorPaletteCallbackCount);
        boost::property_tree::ptree aValues = aView1.m_aColorPaletteCallbackResult.get_child("ThemeColors");
        CPPUNIT_ASSERT(!aValues.empty());
        CPPUNIT_ASSERT_EQUAL(size_t(6), aValues.size());
    }
}

namespace {

constexpr size_t classOffset(int i)
{
    return sizeof(static_cast<struct _LibreOfficeKitClass*>(nullptr)->nSize) + i * sizeof(void*);
}

constexpr size_t documentClassOffset(int i)
{
    return sizeof(static_cast<struct _LibreOfficeKitDocumentClass*>(nullptr)->nSize) + i * sizeof(void*);
}

}

void DesktopLOKTest::testABI()
{
    // STABLE ABI, NEVER CHANGE (unless there's a very good reason, agreed by ESC, etc.)
    CPPUNIT_ASSERT_EQUAL(classOffset(0), offsetof(struct _LibreOfficeKitClass, destroy));
    CPPUNIT_ASSERT_EQUAL(classOffset(1), offsetof(struct _LibreOfficeKitClass, documentLoad));
    CPPUNIT_ASSERT_EQUAL(classOffset(2), offsetof(struct _LibreOfficeKitClass, getError));
    CPPUNIT_ASSERT_EQUAL(classOffset(3), offsetof(struct _LibreOfficeKitClass, documentLoadWithOptions));
    CPPUNIT_ASSERT_EQUAL(classOffset(4), offsetof(struct _LibreOfficeKitClass, freeError));
    CPPUNIT_ASSERT_EQUAL(classOffset(5), offsetof(struct _LibreOfficeKitClass, registerCallback));
    CPPUNIT_ASSERT_EQUAL(classOffset(6), offsetof(struct _LibreOfficeKitClass, getFilterTypes));
    CPPUNIT_ASSERT_EQUAL(classOffset(7), offsetof(struct _LibreOfficeKitClass, setOptionalFeatures));
    CPPUNIT_ASSERT_EQUAL(classOffset(8), offsetof(struct _LibreOfficeKitClass, setDocumentPassword));
    CPPUNIT_ASSERT_EQUAL(classOffset(9), offsetof(struct _LibreOfficeKitClass, getVersionInfo));
    CPPUNIT_ASSERT_EQUAL(classOffset(10), offsetof(struct _LibreOfficeKitClass, runMacro));
    CPPUNIT_ASSERT_EQUAL(classOffset(11), offsetof(struct _LibreOfficeKitClass, signDocument));
    CPPUNIT_ASSERT_EQUAL(classOffset(12), offsetof(struct _LibreOfficeKitClass, runLoop));
    CPPUNIT_ASSERT_EQUAL(classOffset(13), offsetof(struct _LibreOfficeKitClass, sendDialogEvent));
    CPPUNIT_ASSERT_EQUAL(classOffset(14), offsetof(struct _LibreOfficeKitClass, setOption));
    CPPUNIT_ASSERT_EQUAL(classOffset(15), offsetof(struct _LibreOfficeKitClass, dumpState));
    CPPUNIT_ASSERT_EQUAL(classOffset(16), offsetof(struct _LibreOfficeKitClass, extractRequest));
    CPPUNIT_ASSERT_EQUAL(classOffset(17), offsetof(struct _LibreOfficeKitClass, trimMemory));
    CPPUNIT_ASSERT_EQUAL(classOffset(18), offsetof(struct _LibreOfficeKitClass, startURP));
    CPPUNIT_ASSERT_EQUAL(classOffset(19), offsetof(struct _LibreOfficeKitClass, stopURP));
    CPPUNIT_ASSERT_EQUAL(classOffset(20), offsetof(struct _LibreOfficeKitClass, joinThreads));
    CPPUNIT_ASSERT_EQUAL(classOffset(21), offsetof(struct _LibreOfficeKitClass, startThreads));
    CPPUNIT_ASSERT_EQUAL(classOffset(22), offsetof(struct _LibreOfficeKitClass, setForkedChild));
    CPPUNIT_ASSERT_EQUAL(classOffset(23), offsetof(struct _LibreOfficeKitClass, extractDocumentStructureRequest));
    CPPUNIT_ASSERT_EQUAL(classOffset(24), offsetof(struct _LibreOfficeKitClass, registerAnyInputCallback));

    // When extending LibreOfficeKit with a new function pointer,  add new assert for the offsetof the
    // new function pointer and bump this assert for the size of the class.
    CPPUNIT_ASSERT_EQUAL(classOffset(25), sizeof(struct _LibreOfficeKitClass));

    CPPUNIT_ASSERT_EQUAL(documentClassOffset(0), offsetof(struct _LibreOfficeKitDocumentClass, destroy));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(1), offsetof(struct _LibreOfficeKitDocumentClass, saveAs));

    // Unstable ABI, but still think twice before changing this
    // Eg. can't you add your new member at the end of the struct instead of
    // in the middle?  The thing you are changing - is it already part of some
    // release?
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(2), offsetof(struct _LibreOfficeKitDocumentClass, getDocumentType));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(3), offsetof(struct _LibreOfficeKitDocumentClass, getParts));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(4), offsetof(struct _LibreOfficeKitDocumentClass, getPartPageRectangles));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(5), offsetof(struct _LibreOfficeKitDocumentClass, getPart));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(6), offsetof(struct _LibreOfficeKitDocumentClass, setPart));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(7), offsetof(struct _LibreOfficeKitDocumentClass, getPartName));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(8), offsetof(struct _LibreOfficeKitDocumentClass, setPartMode));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(9), offsetof(struct _LibreOfficeKitDocumentClass, paintTile));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(10), offsetof(struct _LibreOfficeKitDocumentClass, getTileMode));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(11), offsetof(struct _LibreOfficeKitDocumentClass, getDocumentSize));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(12), offsetof(struct _LibreOfficeKitDocumentClass, initializeForRendering));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(13), offsetof(struct _LibreOfficeKitDocumentClass, registerCallback));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(14), offsetof(struct _LibreOfficeKitDocumentClass, postKeyEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(15), offsetof(struct _LibreOfficeKitDocumentClass, postMouseEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(16), offsetof(struct _LibreOfficeKitDocumentClass, postUnoCommand));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(17), offsetof(struct _LibreOfficeKitDocumentClass, setTextSelection));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(18), offsetof(struct _LibreOfficeKitDocumentClass, getTextSelection));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(19), offsetof(struct _LibreOfficeKitDocumentClass, paste));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(20), offsetof(struct _LibreOfficeKitDocumentClass, setGraphicSelection));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(21), offsetof(struct _LibreOfficeKitDocumentClass, resetSelection));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(22), offsetof(struct _LibreOfficeKitDocumentClass, getCommandValues));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(23), offsetof(struct _LibreOfficeKitDocumentClass, setClientZoom));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(24), offsetof(struct _LibreOfficeKitDocumentClass, setClientVisibleArea));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(25), offsetof(struct _LibreOfficeKitDocumentClass, createView));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(26), offsetof(struct _LibreOfficeKitDocumentClass, destroyView));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(27), offsetof(struct _LibreOfficeKitDocumentClass, setView));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(28), offsetof(struct _LibreOfficeKitDocumentClass, getView));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(29), offsetof(struct _LibreOfficeKitDocumentClass, getViewsCount));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(30), offsetof(struct _LibreOfficeKitDocumentClass, renderFont));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(31), offsetof(struct _LibreOfficeKitDocumentClass, getPartHash));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(32), offsetof(struct _LibreOfficeKitDocumentClass, paintPartTile));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(33), offsetof(struct _LibreOfficeKitDocumentClass, getViewIds));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(34), offsetof(struct _LibreOfficeKitDocumentClass, setOutlineState));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(35), offsetof(struct _LibreOfficeKitDocumentClass, paintWindow));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(36), offsetof(struct _LibreOfficeKitDocumentClass, postWindow));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(37), offsetof(struct _LibreOfficeKitDocumentClass, postWindowKeyEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(38), offsetof(struct _LibreOfficeKitDocumentClass, postWindowMouseEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(39), offsetof(struct _LibreOfficeKitDocumentClass, setViewLanguage));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(40), offsetof(struct _LibreOfficeKitDocumentClass, postWindowExtTextInputEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(41), offsetof(struct _LibreOfficeKitDocumentClass, getPartInfo));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(42), offsetof(struct _LibreOfficeKitDocumentClass, paintWindowDPI));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(43), offsetof(struct _LibreOfficeKitDocumentClass, insertCertificate));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(44), offsetof(struct _LibreOfficeKitDocumentClass, addCertificate));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(45), offsetof(struct _LibreOfficeKitDocumentClass, getSignatureState));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(46), offsetof(struct _LibreOfficeKitDocumentClass, renderShapeSelection));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(47), offsetof(struct _LibreOfficeKitDocumentClass, postWindowGestureEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(48), offsetof(struct _LibreOfficeKitDocumentClass, createViewWithOptions));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(49), offsetof(struct _LibreOfficeKitDocumentClass, selectPart));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(50), offsetof(struct _LibreOfficeKitDocumentClass, moveSelectedParts));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(51), offsetof(struct _LibreOfficeKitDocumentClass, resizeWindow));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(52), offsetof(struct _LibreOfficeKitDocumentClass, getClipboard));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(53), offsetof(struct _LibreOfficeKitDocumentClass, setClipboard));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(54), offsetof(struct _LibreOfficeKitDocumentClass, getSelectionType));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(55), offsetof(struct _LibreOfficeKitDocumentClass, removeTextContext));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(56), offsetof(struct _LibreOfficeKitDocumentClass, sendDialogEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(57), offsetof(struct _LibreOfficeKitDocumentClass, renderFontOrientation));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(58), offsetof(struct _LibreOfficeKitDocumentClass, paintWindowForView));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(59), offsetof(struct _LibreOfficeKitDocumentClass, completeFunction));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(60), offsetof(struct _LibreOfficeKitDocumentClass, setWindowTextSelection));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(61), offsetof(struct _LibreOfficeKitDocumentClass, sendFormFieldEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(62), offsetof(struct _LibreOfficeKitDocumentClass, setBlockedCommandList));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(63), offsetof(struct _LibreOfficeKitDocumentClass, renderSearchResult));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(64), offsetof(struct _LibreOfficeKitDocumentClass, sendContentControlEvent));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(65), offsetof(struct _LibreOfficeKitDocumentClass, getSelectionTypeAndText));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(66), offsetof(struct _LibreOfficeKitDocumentClass, getDataArea));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(67), offsetof(struct _LibreOfficeKitDocumentClass, getEditMode));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(68), offsetof(struct _LibreOfficeKitDocumentClass, setViewTimezone));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(69), offsetof(struct _LibreOfficeKitDocumentClass, setAccessibilityState));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(70), offsetof(struct _LibreOfficeKitDocumentClass, getA11yFocusedParagraph));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(71), offsetof(struct _LibreOfficeKitDocumentClass, getA11yCaretPosition));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(72), offsetof(struct _LibreOfficeKitDocumentClass, setViewReadOnly));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(73), offsetof(struct _LibreOfficeKitDocumentClass, setAllowChangeComments));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(74), offsetof(struct _LibreOfficeKitDocumentClass, getPresentationInfo));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(75), offsetof(struct _LibreOfficeKitDocumentClass, createSlideRenderer));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(76), offsetof(struct _LibreOfficeKitDocumentClass, postSlideshowCleanup));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(77), offsetof(struct _LibreOfficeKitDocumentClass, renderNextSlideLayer));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(78), offsetof(struct _LibreOfficeKitDocumentClass, setViewOption));
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(79), offsetof(struct _LibreOfficeKitDocumentClass, setAllowManageRedlines));

    // As above
    CPPUNIT_ASSERT_EQUAL(documentClassOffset(80), sizeof(struct _LibreOfficeKitDocumentClass));
}

CPPUNIT_TEST_SUITE_REGISTRATION(DesktopLOKTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
