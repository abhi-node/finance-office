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
#include <sstream>

// Socket includes for HTTP communication
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

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
    
    SAL_INFO("sw.ai", "ExecutePostRequest - URL: " << rRequest.sUrl);
    SAL_INFO("sw.ai", "ExecutePostRequest - Body length: " << rRequest.sBody.getLength());
    SAL_INFO("sw.ai", "ExecutePostRequest - Body content: " << rRequest.sBody);
    
    try
    {
        // Convert OUString to std::string for simpler handling
        OString aUrlUtf8 = OUStringToOString(rRequest.sUrl, RTL_TEXTENCODING_UTF8);
        OString aBodyUtf8 = OUStringToOString(rRequest.sBody, RTL_TEXTENCODING_UTF8);
        
        // For localhost development, use simplified HTTP implementation
        if (rRequest.sUrl.indexOf("localhost") >= 0)
        {
            SAL_INFO("sw.ai", "ExecutePostRequest - Making real HTTP POST to localhost");
            
            // Parse URL to extract host, port, and path
            OUString sHost = "localhost";
            sal_Int32 nPort = 8000;
            OUString sPath = "/api/agent";
            
            // Extract port from URL if specified
            sal_Int32 nPortStart = rRequest.sUrl.indexOf(":", 7); // after "http://"
            if (nPortStart >= 0)
            {
                sal_Int32 nPortEnd = rRequest.sUrl.indexOf("/", nPortStart);
                if (nPortEnd >= 0)
                {
                    OUString sPortStr = rRequest.sUrl.copy(nPortStart + 1, nPortEnd - nPortStart - 1);
                    nPort = sPortStr.toInt32();
                    sPath = rRequest.sUrl.copy(nPortEnd);
                }
            }
            
            // Make actual HTTP request using basic socket approach
            aResponse = makeHttpPostRequest(sHost, nPort, sPath, rRequest.sBody, rRequest.aHeaders);
        }
        else
        {
            // For non-localhost URLs, return error as before
            SAL_WARN("sw.ai", "ExecutePostRequest - Non-localhost URLs not supported: " << rRequest.sUrl);
            aResponse.nStatusCode = 503;
            aResponse.sErrorMessage = "Only localhost connections supported for HTTP POST";
            aResponse.bSuccess = false;
        }
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "ExecutePostRequest - Exception: " << e.Message);
        aResponse.nStatusCode = 500;
        aResponse.sErrorMessage = "Internal error: " + e.Message;
        aResponse.bSuccess = false;
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "ExecutePostRequest - std::exception: " << e.what());
        aResponse.nStatusCode = 500;
        aResponse.sErrorMessage = "Internal error: " + OUString::createFromAscii(e.what());
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

NetworkClient::HttpResponse NetworkClient::makeHttpPostRequest(const OUString& rsHost, sal_Int32 nPort, 
                                                              const OUString& rsPath, const OUString& rsBody,
                                                              const std::map<OUString, OUString>& rHeaders)
{
    HttpResponse aResponse;
    
    SAL_INFO("sw.ai", "makeHttpPostRequest - Host: " << rsHost << ", Port: " << nPort << ", Path: " << rsPath);
    SAL_INFO("sw.ai", "makeHttpPostRequest - Body length: " << rsBody.getLength());
    SAL_INFO("sw.ai", "makeHttpPostRequest - Body content: " << rsBody);
    
    try
    {
        // Convert OUString parameters to std::string for socket operations
        std::string sHost = OUStringToOString(rsHost, RTL_TEXTENCODING_UTF8).getStr();
        std::string sPath = OUStringToOString(rsPath, RTL_TEXTENCODING_UTF8).getStr();
        std::string sBody = OUStringToOString(rsBody, RTL_TEXTENCODING_UTF8).getStr();
        
        // Initialize Winsock on Windows
        #ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) 
        {
            SAL_WARN("sw.ai", "WSAStartup failed: " << result);
            aResponse.nStatusCode = 500;
            aResponse.sErrorMessage = "Failed to initialize Winsock";
            aResponse.bSuccess = false;
            return aResponse;
        }
        #endif
        
        // Create socket
        SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET)
        {
            SAL_WARN("sw.ai", "Failed to create socket");
            #ifdef _WIN32
            WSACleanup();
            #endif
            aResponse.nStatusCode = 500;
            aResponse.sErrorMessage = "Failed to create socket";
            aResponse.bSuccess = false;
            return aResponse;
        }
        
        // Setup server address
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(static_cast<unsigned short>(nPort));
        
        // Convert hostname to IP address
        if (sHost == "localhost" || sHost == "127.0.0.1")
        {
            serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        }
        else
        {
            struct hostent* host = gethostbyname(sHost.c_str());
            if (host == nullptr)
            {
                SAL_WARN("sw.ai", "Failed to resolve hostname: " << sHost.c_str());
                closesocket(sock);
                #ifdef _WIN32
                WSACleanup();
                #endif
                aResponse.nStatusCode = 500;
                aResponse.sErrorMessage = "Failed to resolve hostname";
                aResponse.bSuccess = false;
                return aResponse;
            }
            memcpy(&serverAddr.sin_addr, host->h_addr_list[0], host->h_length);
        }
        
        // Connect to server
        SAL_INFO("sw.ai", "makeHttpPostRequest - Connecting to " << sHost.c_str() << ":" << nPort);
        if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            SAL_WARN("sw.ai", "Failed to connect to server");
            closesocket(sock);
            #ifdef _WIN32
            WSACleanup();
            #endif
            aResponse.nStatusCode = 503;
            aResponse.sErrorMessage = "Connection failed - Is the server running?";
            aResponse.bSuccess = false;
            return aResponse;
        }
        
        SAL_INFO("sw.ai", "makeHttpPostRequest - Connected successfully");
        
        // Build HTTP request
        std::ostringstream request;
        request << "POST " << sPath << " HTTP/1.1\r\n";
        request << "Host: " << sHost << ":" << nPort << "\r\n";
        request << "Content-Type: application/json\r\n";
        request << "Content-Length: " << sBody.length() << "\r\n";
        request << "Accept: application/json\r\n";
        request << "User-Agent: LibreOffice-AI-Agent/1.0\r\n";
        request << "Connection: close\r\n";
        
        // Add custom headers
        for (const auto& header : rHeaders)
        {
            std::string sHeaderName = OUStringToOString(header.first, RTL_TEXTENCODING_UTF8).getStr();
            std::string sHeaderValue = OUStringToOString(header.second, RTL_TEXTENCODING_UTF8).getStr();
            request << sHeaderName << ": " << sHeaderValue << "\r\n";
        }
        
        request << "\r\n"; // End headers
        request << sBody;   // Add body
        
        std::string httpRequest = request.str();
        SAL_INFO("sw.ai", "makeHttpPostRequest - Sending HTTP request, length: " << httpRequest.length());
        
        // Send HTTP request
        int bytesSent = send(sock, httpRequest.c_str(), static_cast<int>(httpRequest.length()), 0);
        if (bytesSent == SOCKET_ERROR)
        {
            SAL_WARN("sw.ai", "Failed to send HTTP request");
            closesocket(sock);
            #ifdef _WIN32
            WSACleanup();
            #endif
            aResponse.nStatusCode = 500;
            aResponse.sErrorMessage = "Failed to send request";
            aResponse.bSuccess = false;
            return aResponse;
        }
        
        SAL_INFO("sw.ai", "makeHttpPostRequest - Request sent, " << bytesSent << " bytes");
        
        // Receive HTTP response
        std::string response;
        char buffer[4096];
        int bytesReceived;
        
        while ((bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0)
        {
            buffer[bytesReceived] = '\0';
            response += buffer;
        }
        
        closesocket(sock);
        #ifdef _WIN32
        WSACleanup();
        #endif
        
        SAL_INFO("sw.ai", "makeHttpPostRequest - Response received, length: " << response.length());
        SAL_INFO("sw.ai", "makeHttpPostRequest - Response preview: " << response.substr(0, 500).c_str());
        
        // Parse HTTP response
        size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
        {
            SAL_WARN("sw.ai", "Invalid HTTP response format");
            aResponse.nStatusCode = 500;
            aResponse.sErrorMessage = "Invalid response format";
            aResponse.bSuccess = false;
            return aResponse;
        }
        
        std::string headers = response.substr(0, headerEnd);
        std::string body = response.substr(headerEnd + 4);
        
        // Parse status line
        size_t firstSpace = headers.find(' ');
        size_t secondSpace = headers.find(' ', firstSpace + 1);
        if (firstSpace != std::string::npos && secondSpace != std::string::npos)
        {
            std::string statusCodeStr = headers.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            aResponse.nStatusCode = std::stoi(statusCodeStr);
            aResponse.sStatusText = OUString::createFromAscii(headers.substr(secondSpace + 1, headers.find('\r') - secondSpace - 1).c_str());
        }
        else
        {
            aResponse.nStatusCode = 200; // Assume OK if we can't parse
            aResponse.sStatusText = "OK";
        }
        
        // Convert body to OUString
        aResponse.sBody = OUString::createFromAscii(body.c_str());
        aResponse.bSuccess = (aResponse.nStatusCode >= 200 && aResponse.nStatusCode < 300);
        
        // Parse Content-Type header
        size_t contentTypePos = headers.find("Content-Type:");
        if (contentTypePos != std::string::npos)
        {
            size_t lineEnd = headers.find('\r', contentTypePos);
            std::string contentType = headers.substr(contentTypePos + 13, lineEnd - contentTypePos - 13);
            // Trim whitespace
            contentType.erase(0, contentType.find_first_not_of(" \t"));
            contentType.erase(contentType.find_last_not_of(" \t") + 1);
            aResponse.aHeaders["Content-Type"] = OUString::createFromAscii(contentType.c_str());
        }
        
        SAL_INFO("sw.ai", "makeHttpPostRequest - Parsed response: Status=" << aResponse.nStatusCode << ", Body length=" << aResponse.sBody.getLength());
        
    }
    catch (const std::exception& e)
    {
        SAL_WARN("sw.ai", "makeHttpPostRequest - Exception: " << e.what());
        aResponse.nStatusCode = 500;
        aResponse.sErrorMessage = "Internal error in HTTP POST: " + OUString::createFromAscii(e.what());
        aResponse.bSuccess = false;
    }
    
    return aResponse;
}

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */