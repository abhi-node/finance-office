/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "NetworkClient.hxx"

#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <ucbhelper/content.hxx>
#include <ucbhelper/commandenvironment.hxx>
#include <unotools/ucbstreamhelper.hxx>
#include <tools/stream.hxx>
#include <tools/datetime.hxx>
#include <rtl/ustrbuf.hxx>
#include <rtl/uri.hxx>
#include <comphelper/random.hxx>

#include <com/sun/star/ucb/SimpleFileAccess.hpp>
#include <com/sun/star/ucb/XCommandEnvironment.hpp>
#include <com/sun/star/ucb/CommandAbortedException.hpp>
#include <com/sun/star/ucb/ContentCreationException.hpp>
#include <com/sun/star/io/XSeekable.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>

using namespace css;
using namespace css::uno;
using namespace css::io;
using namespace css::ucb;
using namespace css::beans;

namespace sw::ai {

NetworkClient::NetworkClient(const Reference<XComponentContext>& xContext)
    : m_xContext(xContext)
    , m_bInitialized(false)
    , m_sUserAgent("LibreOffice-AI-Agent/1.0")
    , m_nDefaultTimeoutMs(30000)
    , m_nMaxConnections(10)
    , m_nRequestCounter(0)
{
    SAL_INFO("sw.ai", "NetworkClient created");
}

NetworkClient::~NetworkClient()
{
    shutdown();
    SAL_INFO("sw.ai", "NetworkClient destroyed");
}

bool NetworkClient::initialize(const Sequence<PropertyValue>& rConfig)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (m_bInitialized)
    {
        SAL_WARN("sw.ai", "NetworkClient already initialized");
        return true;
    }
    
    try
    {
        // Process configuration parameters
        for (const auto& rProperty : rConfig)
        {
            if (rProperty.Name == "DefaultTimeout")
            {
                rProperty.Value >>= m_nDefaultTimeoutMs;
            }
            else if (rProperty.Name == "UserAgent")
            {
                rProperty.Value >>= m_sUserAgent;
            }
            else if (rProperty.Name == "MaxConnections")
            {
                rProperty.Value >>= m_nMaxConnections;
            }
        }
        
        // Validate configuration
        if (m_nDefaultTimeoutMs <= 0)
            m_nDefaultTimeoutMs = 30000;
        
        if (m_sUserAgent.isEmpty())
            m_sUserAgent = "LibreOffice-AI-Agent/1.0";
        
        m_bInitialized = true;
        SAL_INFO("sw.ai", "NetworkClient initialized successfully");
        return true;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "NetworkClient initialization failed: " << e.Message);
        return false;
    }
}

void NetworkClient::shutdown()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bInitialized)
        return;
    
    // Clear metrics and cleanup
    m_aRequestMetrics.clear();
    m_bInitialized = false;
    
    SAL_INFO("sw.ai", "NetworkClient shut down");
}

NetworkClient::HttpResponse NetworkClient::executeRequest(const HttpRequest& rRequest)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bInitialized)
    {
        HttpResponse aResponse;
        aResponse.nStatusCode = 500;
        aResponse.sErrorMessage = "NetworkClient not initialized";
        aResponse.bSuccess = false;
        return aResponse;
    }
    
    // Validate request
    if (!validateRequest(rRequest))
    {
        HttpResponse aResponse;
        aResponse.nStatusCode = 400;
        aResponse.sErrorMessage = "Invalid request parameters";
        aResponse.bSuccess = false;
        return aResponse;
    }
    
    OUString sRequestId = generateRequestId();
    RequestMetrics aMetrics;
    aMetrics.aStartTime = std::chrono::steady_clock::now();
    
    SAL_INFO("sw.ai", "Executing HTTP " << static_cast<int>(rRequest.eMethod) 
             << " request " << sRequestId << " to " << rRequest.sUrl);
    
    try
    {
        // For POST/PUT requests with body, we need to use a different approach
        if (rRequest.eMethod == HttpMethod::POST || rRequest.eMethod == HttpMethod::PUT)
        {
            return executePostRequest(rRequest, sRequestId, aMetrics);
        }
        else
        {
            return executeGetRequest(rRequest, sRequestId, aMetrics);
        }
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "HTTP request " << sRequestId << " failed: " << e.Message);
        return handleNetworkError("executeRequest", e);
    }
}

NetworkClient::HttpResponse NetworkClient::executeGetRequest(
    const HttpRequest& rRequest, const OUString& rsRequestId, RequestMetrics& rMetrics)
{
    HttpResponse aResponse;
    
    try
    {
        // Create UCB content for the URL
        ucbhelper::Content aContent(rRequest.sUrl, nullptr, m_xContext);
        
        // For HTTP URLs, UCB can handle GET requests directly
        std::unique_ptr<SvStream> pStream = utl::UcbStreamHelper::CreateStream(
            rRequest.sUrl, StreamMode::READ);
        
        if (!pStream || pStream->GetError() != ERRCODE_NONE)
        {
            aResponse.nStatusCode = 404;
            aResponse.sErrorMessage = "Failed to create stream for URL: " + rRequest.sUrl;
            aResponse.bSuccess = false;
            return aResponse;
        }
        
        // Read response body
        pStream->Seek(STREAM_SEEK_TO_END);
        sal_uInt64 nSize = pStream->Tell();
        pStream->Seek(STREAM_SEEK_TO_BEGIN);
        
        if (nSize > 0)
        {
            std::vector<char> aBuffer(nSize);
            sal_uInt64 nBytesRead = pStream->ReadBytes(aBuffer.data(), nSize);
            
            if (nBytesRead > 0)
            {
                aResponse.sBody = OUString(aBuffer.data(), nBytesRead, RTL_TEXTENCODING_UTF8);
            }
        }
        
        // UCB streams don't provide HTTP status codes directly, assume success if we got here
        aResponse.nStatusCode = 200;
        aResponse.sStatusText = "OK";
        aResponse.bSuccess = true;
        
        // Record metrics
        rMetrics.aEndTime = std::chrono::steady_clock::now();
        rMetrics.nResponseSize = static_cast<sal_Int32>(nSize);
        m_aRequestMetrics[rsRequestId] = rMetrics;
        
        logRequest(rsRequestId, rRequest, &aResponse);
        return aResponse;
    }
    catch (const ContentCreationException& e)
    {
        SAL_WARN("sw.ai", "ContentCreationException in GET request: " << e.Message);
        aResponse.nStatusCode = 400;
        aResponse.sErrorMessage = "Invalid URL or content creation failed";
        aResponse.bSuccess = false;
        return aResponse;
    }
    catch (const Exception& e)
    {
        return handleNetworkError("GET request", e);
    }
}

NetworkClient::HttpResponse NetworkClient::executePostRequest(
    const HttpRequest& rRequest, const OUString& rsRequestId, RequestMetrics& rMetrics)
{
    HttpResponse aResponse;
    
    // Note: LibreOffice's UCB system has limited support for HTTP POST with custom bodies
    // For a production implementation, this would need to use:
    // 1. Native HTTP libraries (if available)
    // 2. External HTTP client integration
    // 3. Or extended UCB functionality
    
    // For now, we'll simulate the POST functionality for development purposes
    SAL_INFO("sw.ai", "POST request simulation - body length: " << rRequest.sBody.getLength());
    
    // In a real implementation, this would:
    // 1. Create appropriate UCB content with POST method
    // 2. Set up request headers including Content-Type
    // 3. Write request body to output stream
    // 4. Execute the request and read response
    
    // Simulated response for development
    if (rRequest.sUrl.indexOf("langraph") >= 0 || rRequest.sUrl.indexOf("localhost") >= 0)
    {
        // Simulate successful response from LangGraph backend
        aResponse.nStatusCode = 200;
        aResponse.sStatusText = "OK";
        aResponse.sBody = R"({"status": "success", "message": "Request processed successfully", "response": "Simulated agent response"})";
        aResponse.bSuccess = true;
        aResponse.aHeaders["Content-Type"] = "application/json";
    }
    else
    {
        // Simulate connection error for unknown endpoints
        aResponse.nStatusCode = 503;
        aResponse.sErrorMessage = "Service unavailable - LangGraph backend not implemented yet";
        aResponse.bSuccess = false;
    }
    
    // Record metrics
    rMetrics.aEndTime = std::chrono::steady_clock::now();
    rMetrics.nRequestSize = rRequest.sBody.getLength();
    rMetrics.nResponseSize = aResponse.sBody.getLength();
    m_aRequestMetrics[rsRequestId] = rMetrics;
    
    logRequest(rsRequestId, rRequest, &aResponse);
    return aResponse;
}

NetworkClient::HttpResponse NetworkClient::postJson(const OUString& rsUrl, const OUString& rsJsonBody,
                                                   const std::map<OUString, OUString>& rHeaders)
{
    HttpRequest aRequest(rsUrl, HttpMethod::POST);
    aRequest.eContentType = ContentType::JSON;
    aRequest.sBody = rsJsonBody;
    aRequest.nTimeoutMs = m_nDefaultTimeoutMs;
    
    // Add custom headers
    for (const auto& rHeader : rHeaders)
    {
        aRequest.aHeaders[rHeader.first] = rHeader.second;
    }
    
    // Ensure JSON content type
    aRequest.aHeaders["Content-Type"] = "application/json";
    aRequest.aHeaders["Accept"] = "application/json";
    
    return executeRequest(aRequest);
}

NetworkClient::HttpResponse NetworkClient::getJson(const OUString& rsUrl,
                                                  const std::map<OUString, OUString>& rHeaders)
{
    HttpRequest aRequest(rsUrl, HttpMethod::GET);
    aRequest.eContentType = ContentType::JSON;
    aRequest.nTimeoutMs = m_nDefaultTimeoutMs;
    
    // Add custom headers
    for (const auto& rHeader : rHeaders)
    {
        aRequest.aHeaders[rHeader.first] = rHeader.second;
    }
    
    // Ensure JSON accept header
    aRequest.aHeaders["Accept"] = "application/json";
    
    return executeRequest(aRequest);
}

bool NetworkClient::isOnline(const OUString& rsTestUrl) const
{
    // Simple connectivity test using a reliable endpoint
    OUString sTestUrl = rsTestUrl.isEmpty() ? "http://www.libreoffice.org" : rsTestUrl;
    
    try
    {
        std::unique_ptr<SvStream> pStream = utl::UcbStreamHelper::CreateStream(
            sTestUrl, StreamMode::READ);
        
        return pStream && pStream->GetError() == ERRCODE_NONE;
    }
    catch (const Exception&)
    {
        return false;
    }
}

void NetworkClient::setDefaultTimeout(sal_Int32 nTimeoutMs)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_nDefaultTimeoutMs = std::max(nTimeoutMs, 1000); // Minimum 1 second
}

void NetworkClient::setUserAgent(const OUString& rsUserAgent)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_sUserAgent = rsUserAgent.isEmpty() ? "LibreOffice-AI-Agent/1.0" : rsUserAgent;
}

std::map<OUString, NetworkClient::RequestMetrics> NetworkClient::getRequestMetrics() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_aRequestMetrics;
}

void NetworkClient::clearMetrics()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aRequestMetrics.clear();
}

std::map<OUString, OUString> NetworkClient::buildRequestHeaders(const HttpRequest& rRequest) const
{
    std::map<OUString, OUString> aHeaders;
    
    // Add default headers
    aHeaders["User-Agent"] = m_sUserAgent;
    aHeaders["Accept"] = "application/json, text/plain, */*";
    
    // Add content type for requests with body
    if (rRequest.eMethod == HttpMethod::POST || rRequest.eMethod == HttpMethod::PUT)
    {
        aHeaders["Content-Type"] = getContentTypeHeader(rRequest.eContentType);
        if (!rRequest.sBody.isEmpty())
        {
            aHeaders["Content-Length"] = OUString::number(rRequest.sBody.getLength());
        }
    }
    
    // Add custom headers (these override defaults)
    for (const auto& rHeader : rRequest.aHeaders)
    {
        aHeaders[rHeader.first] = rHeader.second;
    }
    
    return aHeaders;
}

OUString NetworkClient::getContentTypeHeader(ContentType eContentType) const
{
    switch (eContentType)
    {
        case ContentType::JSON:
            return "application/json";
        case ContentType::TEXT:
            return "text/plain";
        case ContentType::FORM_URLENCODED:
            return "application/x-www-form-urlencoded";
        case ContentType::MULTIPART_FORM:
            return "multipart/form-data";
        default:
            return "application/json";
    }
}

OUString NetworkClient::generateRequestId()
{
    tools::Time aTime(tools::Time::SYSTEM);
    sal_uInt32 nRandom = comphelper::rng::uniform_uint_distribution(0, 0xFFFFFFFF);
    
    return "HTTP_" + OUString::number(++m_nRequestCounter) + "_" 
           + OUString::number(aTime.GetTime()) + "_" + OUString::number(nRandom);
}

void NetworkClient::logRequest(const OUString& rsRequestId, const HttpRequest& rRequest,
                              const HttpResponse* pResponse) const
{
    SAL_INFO("sw.ai", "Request " << rsRequestId << ": " 
             << static_cast<int>(rRequest.eMethod) << " " << rRequest.sUrl);
    
    if (pResponse)
    {
        SAL_INFO("sw.ai", "Response " << rsRequestId << ": " 
                 << pResponse->nStatusCode << " " << pResponse->sStatusText
                 << " (body length: " << pResponse->sBody.getLength() << ")");
    }
}

NetworkClient::HttpResponse NetworkClient::handleNetworkError(const OUString& rsOperation,
                                                             const Exception& rException) const
{
    HttpResponse aResponse;
    aResponse.nStatusCode = 500;
    aResponse.sStatusText = "Internal Server Error";
    aResponse.sErrorMessage = rsOperation + " failed: " + rException.Message;
    aResponse.bSuccess = false;
    
    SAL_WARN("sw.ai", "Network error in " << rsOperation << ": " << rException.Message);
    return aResponse;
}

bool NetworkClient::applyProxySettings(HttpRequest& /*rRequest*/) const
{
    // TODO: Integrate with LibreOffice proxy configuration
    // This would read proxy settings from LibreOffice configuration
    // and apply them to the request
    return true;
}

bool NetworkClient::validateRequest(const HttpRequest& rRequest) const
{
    if (rRequest.sUrl.isEmpty())
    {
        SAL_WARN("sw.ai", "Invalid request: empty URL");
        return false;
    }
    
    if (!rRequest.sUrl.startsWithIgnoreAsciiCase("http://") && 
        !rRequest.sUrl.startsWithIgnoreAsciiCase("https://"))
    {
        SAL_WARN("sw.ai", "Invalid request: URL must use HTTP or HTTPS protocol");
        return false;
    }
    
    if (rRequest.nTimeoutMs <= 0)
    {
        SAL_WARN("sw.ai", "Invalid request: timeout must be positive");
        return false;
    }
    
    return true;
}

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */