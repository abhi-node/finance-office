/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
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

#include <QtInstance.hxx>
#include <QtInstance.moc>

#include <com/sun/star/lang/IllegalArgumentException.hpp>

#include <QtBitmap.hxx>
#include <QtClipboard.hxx>
#include <QtData.hxx>
#include <QtDragAndDrop.hxx>
#include <QtFilePicker.hxx>
#include <QtFrame.hxx>
#include <QtInstanceBuilder.hxx>
#include <QtMenu.hxx>
#include <QtObject.hxx>
#include <QtOpenGLContext.hxx>
#include "QtSvpVirtualDevice.hxx"
#include <QtSystem.hxx>
#include <QtTimer.hxx>
#include <QtVirtualDevice.hxx>
#include <QtInstanceWidget.hxx>
#include <QtInstanceMessageDialog.hxx>

#include <headless/svpvd.hxx>
#include <salvtables.hxx>

#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QLibraryInfo>
#include <QtCore/QThread>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QMessageBox>

#include <vclpluginapi.h>
#include <tools/debug.hxx>
#include <comphelper/emscriptenthreading.hxx>
#include <config_emscripten.h>
#include <config_vclplug.h>
#include <vcl/qt/QtUtils.hxx>
#include <vcl/sysdata.hxx>
#include <sal/log.hxx>
#include <o3tl/unreachable.hxx>
#include <o3tl/temporary.hxx>
#include <osl/process.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && ENABLE_GSTREAMER_1_0 && QT5_HAVE_GOBJECT
#include <unx/gstsink.hxx>
#endif
#include <headless/svpbmp.hxx>

#include <mutex>
#include <condition_variable>

#ifdef EMSCRIPTEN
#include <QtCore/QtPlugin>
Q_IMPORT_PLUGIN(QWasmIntegrationPlugin)
#if defined DISABLE_DYNLOADING && ENABLE_QT6
#include <QtCore/QtResource>
#endif
#endif

#if defined EMSCRIPTEN && ENABLE_QT6 && HAVE_EMSCRIPTEN_JSPI && !HAVE_EMSCRIPTEN_PROXY_TO_PTHREAD
#include <emscripten/promise.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <pthread.h>
#endif

namespace
{
/// TODO: not much Qt specific here? could be generalised, esp. for OSX...
/// this subclass allows for the transfer of a closure for running on the main
/// thread, to handle all the thread affine stuff in Qt; the SolarMutex is
/// "loaned" to the main thread for the execution of the closure.
/// @note it doesn't work to just use "emit" and signals/slots to move calls to
/// the main thread, because the other thread has the SolarMutex; the other
/// thread (typically) cannot release SolarMutex, because then the main thread
/// will handle all sorts of events and whatnot; this design ensures that the
/// main thread only runs the passed closure (unless the closure releases
/// SolarMutex itself, which should probably be avoided).
class QtYieldMutex : public SalYieldMutex
{
public:
    /// flag only accessed on main thread:
    /// main thread has "borrowed" SolarMutex from another thread
    bool m_bNoYieldLock = false;
    /// members for communication from non-main thread to main thread
    std::mutex m_RunInMainMutex;
    std::condition_variable m_InMainCondition;
    bool m_isWakeUpMain = false;
    std::function<void()> m_Closure; ///< code for main thread to run
    /// members for communication from main thread to non-main thread
    std::condition_variable m_ResultCondition;
    bool m_isResultReady = false;

    virtual bool IsCurrentThread() const override;
    virtual void doAcquire(sal_uInt32 nLockCount) override;
    virtual sal_uInt32 doRelease(bool const bUnlockAll) override;
};
}

bool QtYieldMutex::IsCurrentThread() const
{
    QtInstance& rQtInstance = GetQtInstance();
    if (rQtInstance.IsMainThread() && m_bNoYieldLock)
    {
        return true; // main thread has borrowed SolarMutex
    }
    return SalYieldMutex::IsCurrentThread();
}

void QtYieldMutex::doAcquire(sal_uInt32 nLockCount)
{
    QtInstance& rQtInstance = GetQtInstance();
    if (!rQtInstance.IsMainThread())
    {
        SalYieldMutex::doAcquire(nLockCount);
        return;
    }
    if (m_bNoYieldLock)
    {
        return; // special case for main thread: borrowed from other thread
    }
    do // main thread acquire...
    {
        std::function<void()> func; // copy of closure on thread stack
        {
            std::unique_lock<std::mutex> g(m_RunInMainMutex);
            if (m_aMutex.tryToAcquire())
            {
                // if there's a closure, the other thread holds m_aMutex
                assert(!m_Closure);
                m_isWakeUpMain = false;
                --nLockCount; // have acquired once!
                ++m_nCount;
                break;
            }
            m_InMainCondition.wait(g, [this]() { return m_isWakeUpMain; });
            m_isWakeUpMain = false;
            std::swap(func, m_Closure);
        }
        if (func)
        {
            assert(!m_bNoYieldLock);
            m_bNoYieldLock = true; // execute closure with borrowed SolarMutex
            func();
            m_bNoYieldLock = false;
            std::scoped_lock<std::mutex> g(m_RunInMainMutex);
            assert(!m_isResultReady);
            m_isResultReady = true;
            m_ResultCondition.notify_all(); // unblock other thread
        }
    } while (true);
    SalYieldMutex::doAcquire(nLockCount);
}

sal_uInt32 QtYieldMutex::doRelease(bool const bUnlockAll)
{
    QtInstance& rQtInstance = GetQtInstance();
    if (rQtInstance.IsMainThread() && m_bNoYieldLock)
    {
        return 1; // dummy value
    }

    std::scoped_lock<std::mutex> g(m_RunInMainMutex);
    // read m_nCount before doRelease (it's guarded by m_aMutex)
    bool const isReleased(bUnlockAll || m_nCount == 1);
    sal_uInt32 nCount = SalYieldMutex::doRelease(bUnlockAll);
    if (isReleased && !rQtInstance.IsMainThread())
    {
        m_isWakeUpMain = true;
        m_InMainCondition.notify_all(); // unblock main thread
    }
    return nCount;
}

// this could be abstracted to be independent of Qt by passing in the
// event-trigger as another function parameter...
// it could also be a template of the return type, then it could return the
// result of func... but then how to handle the result in doAcquire?
void QtInstance::RunInMainThread(std::function<void()> func)
{
    DBG_TESTSOLARMUTEX();
    if (IsMainThread())
    {
        func();
        return;
    }
#if defined EMSCRIPTEN && ENABLE_QT6 && HAVE_EMSCRIPTEN_JSPI && !HAVE_EMSCRIPTEN_PROXY_TO_PTHREAD
    if (pthread_self() == m_emscriptenThreadingData->eventHandlerThread)
    {
        EmscriptenLightweightRunInMainThread(func);
        return;
    }
#endif

    QtYieldMutex* const pMutex(static_cast<QtYieldMutex*>(GetYieldMutex()));
    {
        std::scoped_lock<std::mutex> g(pMutex->m_RunInMainMutex);
        assert(!pMutex->m_Closure);
        pMutex->m_Closure = func;
        // unblock main thread in case it is blocked on condition
        pMutex->m_isWakeUpMain = true;
        pMutex->m_InMainCondition.notify_all();
    }

    TriggerUserEventProcessing();
    {
        std::unique_lock<std::mutex> g(pMutex->m_RunInMainMutex);
        pMutex->m_ResultCondition.wait(g, [pMutex]() { return pMutex->m_isResultReady; });
        pMutex->m_isResultReady = false;
    }
}

void QtInstance::EmscriptenLightweightRunInMainThread_(std::function<void()> func)
{
#if defined EMSCRIPTEN && ENABLE_QT6 && HAVE_EMSCRIPTEN_JSPI && !HAVE_EMSCRIPTEN_PROXY_TO_PTHREAD
    if (pthread_self() != emscripten_main_runtime_thread_id())
    {
        SolarMutexReleaser release;
        emscripten_sync_run_in_main_runtime_thread(
            EM_FUNC_SIG_RETURN_VALUE_V | EM_FUNC_SIG_WITH_N_PARAMETERS(1)
                | EM_FUNC_SIG_SET_PARAM(0, EM_FUNC_SIG_PARAM_P),
            +[](void* pf) {
                DBG_TESTNOTSOLARMUTEX();
                SolarMutexGuard g;
                (*static_cast<std::function<void()>*>(pf))();
            },
            &func);
        return;
    }
#endif
    func();
}

OUString QtInstance::constructToolkitID(std::u16string_view sTKname)
{
    OUString sID(sTKname + OUString::Concat(u" ("));
    if (m_bUseCairo)
        sID += "cairo+";
    else
        sID += "qfont+";
    sID += toOUString(QGuiApplication::platformName()) + OUString::Concat(u")");
    return sID;
}

QtInstance::QtInstance(std::unique_ptr<QApplication>& pQApp)
    : SalGenericInstance(std::make_unique<QtYieldMutex>())
    , m_bUseCairo(nullptr == getenv("SAL_VCL_QT_USE_QFONT"))
    , m_pTimer(nullptr)
    , m_bSleeping(false)
    , m_pQApplication(std::move(pQApp))
    , m_aUpdateStyleTimer("vcl::qt5 m_aUpdateStyleTimer")
    , m_bUpdateFonts(false)
    , m_pActivePopup(nullptr)
{
#if defined EMSCRIPTEN && ENABLE_QT6 && HAVE_EMSCRIPTEN_JSPI && !HAVE_EMSCRIPTEN_PROXY_TO_PTHREAD
    m_emscriptenThreadingData = &comphelper::emscriptenthreading::getData();
#endif

    ImplSVData* pSVData = ImplGetSVData();
    const OUString sToolkit = "qt" + OUString::number(QT_VERSION_MAJOR);
    pSVData->maAppData.mxToolkitName = constructToolkitID(sToolkit);

    // this one needs to be blocking, so that the handling in main thread
    // is processed before the thread emitting the signal continues
    connect(this, &QtInstance::ImplYieldSignal, this, &QtInstance::ImplYield,
            Qt::BlockingQueuedConnection);

    // this one needs to be queued non-blocking
    // in order to have this event arriving to correct event processing loop
    connect(this, &QtInstance::deleteObjectLaterSignal, this,
            [](QObject* pObject) { QtInstance::deleteObjectLater(pObject); }, Qt::QueuedConnection);

    m_aUpdateStyleTimer.SetTimeout(50);
    m_aUpdateStyleTimer.SetInvokeHandler(LINK(this, QtInstance, updateStyleHdl));

    QAbstractEventDispatcher* dispatcher = QAbstractEventDispatcher::instance(qApp->thread());
    connect(dispatcher, &QAbstractEventDispatcher::awake, this, [this]() { m_bSleeping = false; });
    connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this,
            [this]() { m_bSleeping = true; });

    connect(QGuiApplication::inputMethod(), &QInputMethod::localeChanged, this,
            &QtInstance::localeChanged);

    for (const QScreen* pCurScreen : QApplication::screens())
        connectQScreenSignals(pCurScreen);
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, &QtInstance::primaryScreenChanged);
    connect(qApp, &QGuiApplication::screenAdded, this, &QtInstance::screenAdded);
    connect(qApp, &QGuiApplication::screenRemoved, this, &QtInstance::screenRemoved);

#ifndef EMSCRIPTEN
    m_bSupportsOpenGL = true;
#elif !HAVE_EMSCRIPTEN_JSPI
    ImplGetSVData()->maAppData.m_bUseSystemLoop = true;
#endif
}

QtInstance::~QtInstance()
{
    // force freeing the QApplication before freeing the arguments,
    // as it uses references to the provided arguments!
    m_pQApplication.reset();

#if defined EMSCRIPTEN && ENABLE_QT6 && HAVE_EMSCRIPTEN_JSPI && !HAVE_EMSCRIPTEN_PROXY_TO_PTHREAD
    comphelper::emscriptenthreading::tearDown();
#endif
}

void QtInstance::AfterAppInit()
{
    // set the default application icon via desktop file just on Wayland,
    // as this otherwise overrides the individual desktop icons on X11.
    if (QGuiApplication::platformName() == "wayland")
        QGuiApplication::setDesktopFileName(QStringLiteral("libreoffice-startcenter"));
    QGuiApplication::setLayoutDirection(AllSettings::GetLayoutRTL() ? Qt::RightToLeft
                                                                    : Qt::LeftToRight);
}

void QtInstance::localeChanged()
{
    SolarMutexGuard aGuard;
    const vcl::Window* pFocusWindow = Application::GetFocusWindow();
    SalFrame* const pFocusFrame = pFocusWindow ? pFocusWindow->ImplGetFrame() : nullptr;
    if (!pFocusFrame)
        return;

    const LanguageTag aTag(
        toOUString(QGuiApplication::inputMethod()->locale().name().replace("_", "-")));
    static_cast<QtFrame*>(pFocusFrame)->setInputLanguage(aTag.getLanguageType());
}

void QtInstance::deleteObjectLater(QObject* pObject) { pObject->deleteLater(); }

SalFrame* QtInstance::CreateChildFrame(SystemParentData* /*pParent*/, SalFrameStyleFlags nStyle)
{
    SolarMutexGuard aGuard;
    SalFrame* pRet(nullptr);
    RunInMainThread([&, this]() { pRet = new QtFrame(nullptr, nStyle, useCairo()); });
    assert(pRet);
    return pRet;
}

SalFrame* QtInstance::CreateFrame(SalFrame* pParent, SalFrameStyleFlags nStyle)
{
    SolarMutexGuard aGuard;

    assert(!pParent || dynamic_cast<QtFrame*>(pParent));

    SalFrame* pRet(nullptr);
    RunInMainThread(
        [&, this]() { pRet = new QtFrame(static_cast<QtFrame*>(pParent), nStyle, useCairo()); });
    assert(pRet);
    return pRet;
}

void QtInstance::DestroyFrame(SalFrame* pFrame)
{
    if (pFrame)
    {
        assert(dynamic_cast<QtFrame*>(pFrame));
        Q_EMIT deleteObjectLaterSignal(static_cast<QtFrame*>(pFrame));
    }
}

SalObject* QtInstance::CreateObject(SalFrame* pParent, SystemWindowData*, bool bShow)
{
    SolarMutexGuard aGuard;

    assert(!pParent || dynamic_cast<QtFrame*>(pParent));

    SalObject* pRet(nullptr);
    RunInMainThread([&]() { pRet = new QtObject(static_cast<QtFrame*>(pParent), bShow); });
    assert(pRet);
    return pRet;
}

void QtInstance::DestroyObject(SalObject* pObject)
{
    if (pObject)
    {
        assert(dynamic_cast<QtObject*>(pObject));
        Q_EMIT deleteObjectLaterSignal(static_cast<QtObject*>(pObject));
    }
}

std::unique_ptr<SalVirtualDevice> QtInstance::CreateVirtualDevice(SalGraphics& rGraphics,
                                                                  tools::Long nDX, tools::Long nDY,
                                                                  DeviceFormat /*eFormat*/,
                                                                  bool bAlphaMaskTransparent)
{
    if (m_bUseCairo)
    {
        SvpSalGraphics* pSvpSalGraphics = dynamic_cast<QtSvpGraphics*>(&rGraphics);
        assert(pSvpSalGraphics);
        // tdf#127529 see SvpSalInstance::CreateVirtualDevice for the rare case of a non-null pPreExistingTarget
        std::unique_ptr<SalVirtualDevice> pVD(
            new QtSvpVirtualDevice(pSvpSalGraphics->getSurface(), /*pPreExistingTarget*/ nullptr));
        pVD->SetSize(nDX, nDY, bAlphaMaskTransparent);
        return pVD;
    }
    else
    {
        std::unique_ptr<SalVirtualDevice> pVD(new QtVirtualDevice(/*scale*/ 1));
        pVD->SetSize(nDX, nDY, bAlphaMaskTransparent);
        return pVD;
    }
}

std::unique_ptr<SalVirtualDevice>
QtInstance::CreateVirtualDevice(SalGraphics& rGraphics, tools::Long& nDX, tools::Long& nDY,
                                DeviceFormat /*eFormat*/, const SystemGraphicsData& rGd)
{
    if (m_bUseCairo)
    {
        SvpSalGraphics* pSvpSalGraphics = dynamic_cast<QtSvpGraphics*>(&rGraphics);
        assert(pSvpSalGraphics);
        // tdf#127529 see SvpSalInstance::CreateVirtualDevice for the rare case of a non-null pPreExistingTarget
        cairo_surface_t* pPreExistingTarget = static_cast<cairo_surface_t*>(rGd.pSurface);
        std::unique_ptr<SalVirtualDevice> pVD(
            new QtSvpVirtualDevice(pSvpSalGraphics->getSurface(), pPreExistingTarget));
        pVD->SetSize(nDX, nDY, /*bAlphaMaskTransparent*/ false);
        return pVD;
    }
    else
    {
        std::unique_ptr<SalVirtualDevice> pVD(new QtVirtualDevice(/*scale*/ 1));
        pVD->SetSize(nDX, nDY, /*bAlphaMaskTransparent*/ false);
        return pVD;
    }
}

std::unique_ptr<SalMenu> QtInstance::CreateMenu(bool bMenuBar, Menu* pVCLMenu)
{
    SolarMutexGuard aGuard;
    std::unique_ptr<SalMenu> pRet;
    RunInMainThread([&pRet, bMenuBar, pVCLMenu]() {
        QtMenu* pSalMenu = new QtMenu(bMenuBar);
        pRet.reset(pSalMenu);
        pSalMenu->SetMenu(pVCLMenu);
    });
    assert(pRet);
    return pRet;
}

std::unique_ptr<SalMenuItem> QtInstance::CreateMenuItem(const SalItemParams& rItemData)
{
    return std::unique_ptr<SalMenuItem>(new QtMenuItem(&rItemData));
}

SalTimer* QtInstance::CreateSalTimer()
{
    m_pTimer = new QtTimer();
    return m_pTimer;
}

SalSystem* QtInstance::CreateSalSystem() { return new QtSystem; }

std::shared_ptr<SalBitmap> QtInstance::CreateSalBitmap()
{
    if (m_bUseCairo)
        return std::make_shared<SvpSalBitmap>();
    else
        return std::make_shared<QtBitmap>();
}

bool QtInstance::ImplYield(bool bWait, bool bHandleAllCurrentEvents)
{
    // Re-acquire the guard for user events when called via Q_EMIT ImplYieldSignal
    SolarMutexGuard aGuard;
    bool wasEvent = DispatchUserEvents(bHandleAllCurrentEvents);
    if (!bHandleAllCurrentEvents && wasEvent)
        return true;

    /**
     * Quoting the Qt docs: [QAbstractEventDispatcher::processEvents] processes
     * pending events that match flags until there are no more events to process.
     */
    SolarMutexReleaser aReleaser;
    QAbstractEventDispatcher* dispatcher = QAbstractEventDispatcher::instance(qApp->thread());
    if (bWait && !wasEvent)
        wasEvent = dispatcher->processEvents(QEventLoop::WaitForMoreEvents);
    else
        wasEvent = dispatcher->processEvents(QEventLoop::AllEvents) || wasEvent;
    return wasEvent;
}

bool QtInstance::DoYield(bool bWait, bool bHandleAllCurrentEvents)
{
    bool bWasEvent = false;
    if (qApp->thread() == QThread::currentThread())
    {
        bWasEvent = ImplYield(bWait, bHandleAllCurrentEvents);
        if (bWasEvent)
            m_aWaitingYieldCond.set();
    }
#if defined EMSCRIPTEN && ENABLE_QT6 && HAVE_EMSCRIPTEN_JSPI && !HAVE_EMSCRIPTEN_PROXY_TO_PTHREAD
    else if (pthread_self() == m_emscriptenThreadingData->eventHandlerThread)
    {
        SolarMutexReleaser release;
        struct Args
        {
            QtInstance* This;
            bool bWait;
            bool bHandleAllCurrentEvents;
            bool& bWasEvent;
        };
        (void)emscripten_promise_await(emscripten_proxy_promise(
            m_emscriptenThreadingData->proxyingQueue.queue, emscripten_main_runtime_thread_id(),
            [](void* p) {
                Args const& args = *static_cast<Args*>(p);
                args.bWasEvent = args.This->DoYield(args.bWait, args.bHandleAllCurrentEvents);
            },
            &o3tl::temporary<Args>({ this, bWait, bHandleAllCurrentEvents, bWasEvent })));
    }
#endif
    else
    {
        {
            SolarMutexReleaser aReleaser;
            bWasEvent = Q_EMIT ImplYieldSignal(false, bHandleAllCurrentEvents);
        }
        if (!bWasEvent && bWait)
        {
            m_aWaitingYieldCond.reset();
            SolarMutexReleaser aReleaser;
            m_aWaitingYieldCond.wait();
            bWasEvent = true;
        }
    }
    return bWasEvent;
}

bool QtInstance::AnyInput(VclInputFlags nType)
{
    bool bResult = false;
    if (nType & VclInputFlags::TIMER)
        bResult |= (m_pTimer && m_pTimer->remainingTime() == 0);
    if (nType & VclInputFlags::OTHER)
        bResult |= !m_bSleeping;
    return bResult;
}

void QtInstance::AddToRecentDocumentList(const OUString&, const OUString&, const OUString&) {}

#ifndef EMSCRIPTEN
OpenGLContext* QtInstance::CreateOpenGLContext() { return new QtOpenGLContext; }
#endif

bool QtInstance::IsMainThread() const
{
    return !qApp || (qApp->thread() == QThread::currentThread());
}

void QtInstance::TriggerUserEventProcessing()
{
    QAbstractEventDispatcher* dispatcher = QAbstractEventDispatcher::instance(qApp->thread());
    dispatcher->wakeUp();
}

void QtInstance::ProcessEvent(SalUserEvent aEvent)
{
#if defined EMSCRIPTEN && ENABLE_QT6 && HAVE_EMSCRIPTEN_JSPI && !HAVE_EMSCRIPTEN_PROXY_TO_PTHREAD
    SolarMutexReleaser release;
    (void)emscripten_promise_await(
        emscripten_proxy_promise(m_emscriptenThreadingData->proxyingQueue.queue,
                                 m_emscriptenThreadingData->eventHandlerThread,
                                 [](void* p) {
                                     auto& aEvent = *static_cast<SalUserEvent*>(p);
                                     SolarMutexGuard g;
                                     aEvent.m_pFrame->CallCallback(aEvent.m_nEvent, aEvent.m_pData);
                                 },
                                 &aEvent));
#else
    aEvent.m_pFrame->CallCallback(aEvent.m_nEvent, aEvent.m_pData);
#endif
}

rtl::Reference<QtFilePicker>
QtInstance::createPicker(css::uno::Reference<css::uno::XComponentContext> const& context,
                         QFileDialog::FileMode eMode)
{
    if (!IsMainThread())
    {
        SolarMutexGuard g;
        rtl::Reference<QtFilePicker> pPicker;
        RunInMainThread([&, this]() { pPicker = createPicker(context, eMode); });
        assert(pPicker);
        return pPicker;
    }

    return new QtFilePicker(context, eMode);
}

css::uno::Reference<css::ui::dialogs::XFilePicker2>
QtInstance::createFilePicker(const css::uno::Reference<css::uno::XComponentContext>& context)
{
    return css::uno::Reference<css::ui::dialogs::XFilePicker2>(
        createPicker(context, QFileDialog::ExistingFile));
}

css::uno::Reference<css::ui::dialogs::XFolderPicker2>
QtInstance::createFolderPicker(const css::uno::Reference<css::uno::XComponentContext>& context)
{
    return css::uno::Reference<css::ui::dialogs::XFolderPicker2>(
        createPicker(context, QFileDialog::Directory));
}

css::uno::Reference<css::datatransfer::clipboard::XClipboard>
QtInstance::CreateClipboard(const css::uno::Sequence<css::uno::Any>& arguments)
{
    OUString sel;
    if (arguments.getLength() == 0)
    {
        sel = "CLIPBOARD";
    }
    else if (arguments.getLength() != 1 || !(arguments[0] >>= sel))
    {
        throw css::lang::IllegalArgumentException(u"bad QtInstance::CreateClipboard arguments"_ustr,
                                                  css::uno::Reference<css::uno::XInterface>(), -1);
    }

    // This could also use RunInMain, but SolarMutexGuard is enough
    // since at this point we're not accessing the clipboard, just get the
    // accessor to the clipboard.
    SolarMutexGuard aGuard;

    auto it = m_aClipboards.find(sel);
    if (it != m_aClipboards.end())
        return it->second;

    css::uno::Reference<css::datatransfer::clipboard::XClipboard> xClipboard
        = EmscriptenLightweightRunInMainThread([&sel] { return QtClipboard::create(sel); });
    if (xClipboard.is())
        m_aClipboards[sel] = xClipboard;

    return xClipboard;
}

css::uno::Reference<css::datatransfer::dnd::XDragSource>
QtInstance::ImplCreateDragSource(const SystemEnvData& rSysEnv)
{
    return new QtDragSource(static_cast<QtFrame*>(rSysEnv.pSalFrame));
}

css::uno::Reference<css::datatransfer::dnd::XDropTarget>
QtInstance::ImplCreateDropTarget(const SystemEnvData& rSysEnv)
{
    rtl::Reference<QtDropTarget> pDropTarget = new QtDropTarget;
    QtFrame* pFrame = static_cast<QtFrame*>(rSysEnv.pSalFrame);
    pFrame->registerDropTarget(pDropTarget.get());
    pDropTarget->setActive(true);

    return pDropTarget;
}

const cairo_font_options_t* QtInstance::GetCairoFontOptions()
{
    static cairo_font_options_t* gOptions = cairo_font_options_create();
    return gOptions;
}

IMPL_LINK_NOARG(QtInstance, updateStyleHdl, Timer*, void)
{
    SolarMutexGuard aGuard;
    SalFrame* pFrame = anyFrame();
    if (pFrame)
    {
        pFrame->CallCallback(SalEvent::SettingsChanged, nullptr);
        if (m_bUpdateFonts)
        {
            pFrame->CallCallback(SalEvent::FontChanged, nullptr);
            m_bUpdateFonts = false;
        }
    }
}

void QtInstance::UpdateStyle(bool bFontsChanged)
{
    if (bFontsChanged)
        m_bUpdateFonts = true;
    if (!m_aUpdateStyleTimer.IsActive())
        m_aUpdateStyleTimer.Start();
}

void* QtInstance::CreateGStreamerSink(const SystemChildWindow* pWindow)
{
// As of 2021-09, qt-gstreamer is unmaintained and there is no Qt 6 video sink
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && ENABLE_GSTREAMER_1_0 && QT5_HAVE_GOBJECT
    auto pSymbol = gstElementFactoryNameSymbol();
    if (!pSymbol)
        return nullptr;

    const SystemEnvData* pEnvData = pWindow->GetSystemData();
    if (!pEnvData)
        return nullptr;

    if (pEnvData->platform != SystemEnvData::Platform::Wayland)
        return nullptr;

    GstElement* pVideosink = pSymbol("qwidget5videosink", "qwidget5videosink");
    if (pVideosink)
    {
        QWidget* pQWidget = static_cast<QWidget*>(pEnvData->pWidget);
        g_object_set(G_OBJECT(pVideosink), "widget", pQWidget, nullptr);
    }
    else
    {
        SAL_WARN("vcl.qt", "Couldn't initialize qwidget5videosink."
                           " Video playback might not work as expected."
                           " Please install Qt5 packages for QtGStreamer.");
        // with no videosink explicitly set, GStreamer will open its own (misplaced) window(s) to display video
    }

    return pVideosink;
#else
    Q_UNUSED(pWindow);
    return nullptr;
#endif
}

void QtInstance::connectQScreenSignals(const QScreen* pScreen)
{
    connect(pScreen, &QScreen::orientationChanged, this, &QtInstance::orientationChanged);
    connect(pScreen, &QScreen::virtualGeometryChanged, this, &QtInstance::virtualGeometryChanged);
}

void QtInstance::notifyDisplayChanged()
{
    SolarMutexGuard aGuard;
    SalFrame* pAnyFrame = anyFrame();
    if (pAnyFrame)
        pAnyFrame->CallCallback(SalEvent::DisplayChanged, nullptr);
}

void QtInstance::orientationChanged(Qt::ScreenOrientation) { notifyDisplayChanged(); }

void QtInstance::primaryScreenChanged(QScreen*) { notifyDisplayChanged(); }

void QtInstance::screenAdded(QScreen* pScreen)
{
    connectQScreenSignals(pScreen);
    if (QApplication::screens().size() == 1)
        notifyDisplayChanged();
}

void QtInstance::screenRemoved(QScreen*) { notifyDisplayChanged(); }

void QtInstance::virtualGeometryChanged(const QRect&) { notifyDisplayChanged(); }

void QtInstance::AllocFakeCmdlineArgs(std::unique_ptr<char* []>& rFakeArgv,
                                      std::unique_ptr<int>& rFakeArgc,
                                      std::vector<FreeableCStr>& rFakeArgvFreeable)
{
    OString aVersion(qVersion());
    SAL_INFO("vcl.qt", "qt version string is " << aVersion);

    const sal_uInt32 nParams = osl_getCommandArgCount();
    sal_uInt32 nDisplayValueIdx = 0;
    OUString aParam, aBin;

    for (sal_uInt32 nIdx = 0; nIdx < nParams; ++nIdx)
    {
        osl_getCommandArg(nIdx, &aParam.pData);
        if (aParam != "-display")
            continue;
        ++nIdx;
        nDisplayValueIdx = nIdx;
    }

    osl_getExecutableFile(&aParam.pData);
    osl_getSystemPathFromFileURL(aParam.pData, &aBin.pData);
    OString aExec = OUStringToOString(aBin, osl_getThreadTextEncoding());

    std::vector<FreeableCStr> aFakeArgvFreeable;
    aFakeArgvFreeable.reserve(4);
    aFakeArgvFreeable.emplace_back(strdup(aExec.getStr()));
    aFakeArgvFreeable.emplace_back(strdup("--nocrashhandler"));
    if (nDisplayValueIdx)
    {
        aFakeArgvFreeable.emplace_back(strdup("-display"));
        osl_getCommandArg(nDisplayValueIdx, &aParam.pData);
        OString aDisplay = OUStringToOString(aParam, osl_getThreadTextEncoding());
        aFakeArgvFreeable.emplace_back(strdup(aDisplay.getStr()));
    }
    rFakeArgvFreeable.swap(aFakeArgvFreeable);

    const int nFakeArgc = rFakeArgvFreeable.size();
    rFakeArgv.reset(new char*[nFakeArgc]);
    for (int i = 0; i < nFakeArgc; i++)
        rFakeArgv[i] = rFakeArgvFreeable[i].get();

    rFakeArgc.reset(new int);
    *rFakeArgc = nFakeArgc;
}

void QtInstance::MoveFakeCmdlineArgs(std::unique_ptr<char* []>& rFakeArgv,
                                     std::unique_ptr<int>& rFakeArgc,
                                     std::vector<FreeableCStr>& rFakeArgvFreeable)
{
    m_pFakeArgv = std::move(rFakeArgv);
    m_pFakeArgc = std::move(rFakeArgc);
    m_pFakeArgvFreeable.swap(rFakeArgvFreeable);
}

std::unique_ptr<QApplication> QtInstance::CreateQApplication(int& nArgc, char** pArgv)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // for Qt 6, setting Qt::AA_EnableHighDpiScaling and Qt::AA_UseHighDpiPixmaps
    // is deprecated, they're always enabled
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // for scaled icons in the native menus
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    // force Qt::HighDpiScaleFactorRoundingPolicy::Round, which is the Qt 5 default
    // policy and prevents incorrect rendering with the Qt 6 default policy
    // Qt::HighDpiScaleFactorRoundingPolicy::PassThrough (tdf#159915)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::Round);

    FreeableCStr session_manager;
    if (getenv("SESSION_MANAGER") != nullptr)
    {
        session_manager.reset(strdup(getenv("SESSION_MANAGER")));
        unsetenv("SESSION_MANAGER");
    }

    std::unique_ptr<QApplication> pQApp = std::make_unique<QApplication>(nArgc, pArgv);

    if (session_manager != nullptr)
    {
        // coverity[tainted_string] - trusted source for setenv
        setenv("SESSION_MANAGER", session_manager.get(), 1);
    }

    QApplication::setQuitOnLastWindowClosed(false);
    return pQApp;
}

bool QtInstance::DoExecute(int& nExitCode)
{
    const bool bIsUseSystemEventLoop = Application::IsUseSystemEventLoop();
    if (bIsUseSystemEventLoop)
    {
#if defined EMSCRIPTEN
        // For Emscripten, QApplication::exec() will unwind the stack by throwing a JavaScript
        // exception, so we need to manually undo the call of AcquireYieldMutex() done in InitVCL:
        ReleaseYieldMutex(false);
#endif
        nExitCode = QApplication::exec();
#if defined EMSCRIPTEN
        O3TL_UNREACHABLE;
#endif
    }
    return bIsUseSystemEventLoop;
}

void QtInstance::DoQuit()
{
    if (Application::IsUseSystemEventLoop())
        QApplication::quit();
}

void QtInstance::setActivePopup(QtFrame* pFrame)
{
    assert(!pFrame || pFrame->isPopup());
    m_pActivePopup = pFrame;
}

QWidget* QtInstance::GetNativeParentFromWeldParent(weld::Widget* pParent)
{
    if (!pParent)
        return nullptr;

    if (QtInstanceWidget* pQtInstanceWidget = dynamic_cast<QtInstanceWidget*>(pParent))
        return pQtInstanceWidget->getQWidget();

    // the parent is not welded/not a native Qt widget; get QWidget via frame
    if (SalInstanceWidget* pSalWidget = dynamic_cast<SalInstanceWidget*>(pParent))
    {
        if (vcl::Window* pWindow = pSalWidget->getWidget())
        {
            if (QtFrame* pFrame = static_cast<QtFrame*>(pWindow->ImplGetFrame()))
                return pFrame->GetQWidget();
        }
    }

    return nullptr;
}

std::unique_ptr<weld::Builder>
QtInstance::CreateBuilder(weld::Widget* pParent, const OUString& rUIRoot, const OUString& rUIFile)
{
    // for now, require explicitly enabling use of QtInstanceBuilder via SAL_VCL_QT_USE_WELDED_WIDGETS
    static const bool bUseWeldedWidgets = (getenv("SAL_VCL_QT_USE_WELDED_WIDGETS") != nullptr);
    if (bUseWeldedWidgets && !QtData::noWeldedWidgets()
        && QtInstanceBuilder::IsUIFileSupported(rUIFile, pParent))
    {
        QWidget* pQtParent = GetNativeParentFromWeldParent(pParent);
        return std::make_unique<QtInstanceBuilder>(pQtParent, rUIRoot, rUIFile);
    }
    else
    {
        return SalInstance::CreateBuilder(pParent, rUIRoot, rUIFile);
    }
}

weld::MessageDialog* QtInstance::CreateMessageDialog(weld::Widget* pParent,
                                                     VclMessageType eMessageType,
                                                     VclButtonsType eButtonsType,
                                                     const OUString& rPrimaryMessage)
{
    SolarMutexGuard g;
    if (!IsMainThread())
    {
        weld::MessageDialog* pDialog;
        RunInMainThread([&] {
            pDialog = CreateMessageDialog(pParent, eMessageType, eButtonsType, rPrimaryMessage);
        });
        return pDialog;
    }

    if (QtData::noWeldedWidgets())
    {
        return SalInstance::CreateMessageDialog(pParent, eMessageType, eButtonsType,
                                                rPrimaryMessage);
    }
    else
    {
        QWidget* pQtParent = GetNativeParentFromWeldParent(pParent);
        QMessageBox* pMessageBox = new QMessageBox(pQtParent);
        pMessageBox->setText(toQString(rPrimaryMessage));
        pMessageBox->setIcon(vclMessageTypeToQtIcon(eMessageType));
        pMessageBox->setWindowTitle(vclMessageTypeToQtTitle(eMessageType));
        QtInstanceMessageDialog* pDialog = new QtInstanceMessageDialog(pMessageBox);
        pDialog->addStandardButtons(eButtonsType);
        return pDialog;
    }
}

static void initResources()
{
#if defined EMSCRIPTEN && defined DISABLE_DYNLOADING && ENABLE_QT6
    // Make sure the resources from Qt6's plugins/platforms/libqwasm.a are not stripped out of a
    // statically linked binary (and this code cannot be directly in extern "C" create_SalInstance,
    // as the expansion of Q_INIT_RESOURCE contains extern function declarations that would then
    // erroneously be C function declarations):
    Q_INIT_RESOURCE(wasmfonts);
    Q_INIT_RESOURCE(wasmwindow);
#endif
}

extern "C" {
VCLPLUG_QT_PUBLIC SalInstance* create_SalInstance()
{
    initResources();

    std::unique_ptr<char* []> pFakeArgv;
    std::unique_ptr<int> pFakeArgc;
    std::vector<FreeableCStr> aFakeArgvFreeable;
    QtInstance::AllocFakeCmdlineArgs(pFakeArgv, pFakeArgc, aFakeArgvFreeable);

    std::unique_ptr<QApplication> pQApp
        = QtInstance::CreateQApplication(*pFakeArgc, pFakeArgv.get());

    QtInstance* pInstance = new QtInstance(pQApp);
    pInstance->MoveFakeCmdlineArgs(pFakeArgv, pFakeArgc, aFakeArgvFreeable);

    new QtData();

    return pInstance;
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
