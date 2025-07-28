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

#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/io/XInputStream.hpp>
#include <com/sun/star/io/XOutputStream.hpp>
#include <com/sun/star/beans/PropertyValue.hpp>
#include <tools/stream.hxx>
#include <rtl/ustrbuf.hxx>

#include <memory>
#include <string>
#include <map>
#include <chrono>

namespace sw::ai {

/**
 * NetworkClient - HTTP client implementation using LibreOffice UCB system
 * 
 * This class provides HTTP communication capabilities for the AI Agent system,
 * built on top of LibreOffice's Universal Content Broker (UCB) for maximum
 * compatibility with existing network infrastructure.
 * 
 * Design Principles:
 * - Uses LibreOffice's native UCB system for network operations
 * - Compatible with LibreOffice proxy settings and security policies
 * - Supports both synchronous and asynchronous HTTP operations
 * - JSON request/response handling for LangGraph backend communication
 * - Connection pooling and timeout management
 */
class NetworkClient
{
public:
    enum class HttpMethod
    {
        GET,
        POST,
        PUT,
        DELETE
    };

    enum class ContentType
    {
        JSON,
        TEXT,
        FORM_URLENCODED,
        MULTIPART_FORM
    };

    struct HttpRequest
    {
        OUString sUrl;
        HttpMethod eMethod;
        ContentType eContentType;
        OUString sBody;
        std::map<OUString, OUString> aHeaders;
        sal_Int32 nTimeoutMs;
        
        HttpRequest(const OUString& rsUrl, HttpMethod eM = HttpMethod::GET)
            : sUrl(rsUrl), eMethod(eM), eContentType(ContentType::JSON)
            , nTimeoutMs(30000) {}
    };

    struct HttpResponse
    {
        sal_Int32 nStatusCode;
        OUString sStatusText;
        OUString sBody;
        std::map<OUString, OUString> aHeaders;
        bool bSuccess;
        OUString sErrorMessage;
        
        HttpResponse() : nStatusCode(0), bSuccess(false) {}
    };

private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    
    // Connection management
    mutable std::mutex m_aMutex;
    bool m_bInitialized;
    OUString m_sUserAgent;
    sal_Int32 m_nDefaultTimeoutMs;
    sal_Int32 m_nMaxConnections;
    
    // Request tracking for debugging and metrics
    struct RequestMetrics
    {
        std::chrono::steady_clock::time_point aStartTime;
        std::chrono::steady_clock::time_point aEndTime;
        sal_Int32 nRequestSize;
        sal_Int32 nResponseSize;
        bool bFromCache;
        
        RequestMetrics() : nRequestSize(0), nResponseSize(0), bFromCache(false) {}
    };
    
    std::map<OUString, RequestMetrics> m_aRequestMetrics;
    sal_Int32 m_nRequestCounter;

public:
    explicit NetworkClient(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    ~NetworkClient();

    // Main HTTP API
    
    /**
     * Perform synchronous HTTP request using LibreOffice UCB
     * 
     * @param rRequest HTTP request parameters including URL, method, headers, body
     * @returns HttpResponse with status code, headers, body, and success flag
     */
    HttpResponse executeRequest(const HttpRequest& rRequest);
    
    /**
     * Convenience method for JSON POST requests to LangGraph backend
     * 
     * @param rsUrl Target URL for the request
     * @param rsJsonBody JSON request body
     * @param rHeaders Additional HTTP headers
     * @returns HttpResponse with parsed JSON response
     */
    HttpResponse postJson(const OUString& rsUrl, const OUString& rsJsonBody,
                         const std::map<OUString, OUString>& rHeaders = {});
    
    /**
     * Convenience method for JSON GET requests
     * 
     * @param rsUrl Target URL for the request
     * @param rHeaders Additional HTTP headers
     * @returns HttpResponse with parsed JSON response
     */
    HttpResponse getJson(const OUString& rsUrl,
                        const std::map<OUString, OUString>& rHeaders = {});

    // Configuration and lifecycle management
    
    /**
     * Initialize network client with configuration
     * 
     * @param rConfig Configuration parameters including timeouts, user agent, etc.
     * @returns true if initialization successful
     */
    bool initialize(const css::uno::Sequence<css::beans::PropertyValue>& rConfig = {});
    
    /**
     * Shutdown network client and cleanup resources
     */
    void shutdown();
    
    /**
     * Check if network connectivity is available
     * 
     * @param rsTestUrl Optional URL to test connectivity (defaults to reliable test endpoint)
     * @returns true if network connectivity is available
     */
    bool isOnline(const OUString& rsTestUrl = OUString()) const;
    
    /**
     * Set default timeout for all requests
     * 
     * @param nTimeoutMs Timeout in milliseconds
     */
    void setDefaultTimeout(sal_Int32 nTimeoutMs);
    
    /**
     * Set custom user agent string
     * 
     * @param rsUserAgent User agent string to use for requests
     */
    void setUserAgent(const OUString& rsUserAgent);

    // Request metrics and debugging
    
    /**
     * Get performance metrics for debugging and optimization
     * 
     * @returns Map of request IDs to performance metrics
     */
    std::map<OUString, RequestMetrics> getRequestMetrics() const;
    
    /**
     * Clear accumulated request metrics
     */
    void clearMetrics();

private:
    // Internal implementation methods
    
    /**
     * Execute HTTP GET request using UCB
     * 
     * @param rRequest HTTP request parameters
     * @param rsRequestId Unique request identifier
     * @param rMetrics Request metrics for tracking
     * @returns HTTP response with status and body
     */
    HttpResponse executeGetRequest(const HttpRequest& rRequest, const OUString& rsRequestId, RequestMetrics& rMetrics);
    
    /**
     * Execute HTTP POST request using UCB
     * 
     * @param rRequest HTTP request parameters
     * @param rsRequestId Unique request identifier
     * @param rMetrics Request metrics for tracking
     * @returns HTTP response with status and body
     */
    HttpResponse executePostRequest(const HttpRequest& rRequest, const OUString& rsRequestId, RequestMetrics& rMetrics);
    
    /**
     * Create UCB content for HTTP request
     * 
     * @param rRequest HTTP request parameters
     * @returns UCB content reference for the request
     */
    css::uno::Reference<css::io::XInputStream> createHttpStream(const HttpRequest& rRequest);
    
    /**
     * Parse HTTP response from UCB stream
     * 
     * @param xStream Input stream from UCB
     * @param rRequest Original request for context
     * @returns Parsed HTTP response
     */
    HttpResponse parseHttpResponse(const css::uno::Reference<css::io::XInputStream>& xStream,
                                  const HttpRequest& rRequest);
    
    /**
     * Build HTTP request headers
     * 
     * @param rRequest Request parameters
     * @returns Complete header map including default and custom headers
     */
    std::map<OUString, OUString> buildRequestHeaders(const HttpRequest& rRequest) const;
    
    /**
     * Generate content type header value
     * 
     * @param eContentType Content type enum
     * @returns Proper HTTP content-type header value
     */
    OUString getContentTypeHeader(ContentType eContentType) const;
    
    /**
     * Generate unique request ID for tracking
     * 
     * @returns Unique request identifier
     */
    OUString generateRequestId();
    
    /**
     * Log request details for debugging
     * 
     * @param rsRequestId Request identifier
     * @param rRequest Request parameters
     * @param rResponse Response data (optional)
     */
    void logRequest(const OUString& rsRequestId, const HttpRequest& rRequest,
                   const HttpResponse* pResponse = nullptr) const;
    
    /**
     * Handle network errors with appropriate error codes
     * 
     * @param rsOperation Operation that failed
     * @param rException Exception details
     * @returns Error response with appropriate status code
     */
    HttpResponse handleNetworkError(const OUString& rsOperation,
                                   const css::uno::Exception& rException) const;
    
    /**
     * Apply LibreOffice proxy settings to request
     * 
     * @param rRequest Request to configure
     * @returns true if proxy configuration applied successfully
     */
    bool applyProxySettings(HttpRequest& rRequest) const;
    
    /**
     * Validate URL and request parameters
     * 
     * @param rRequest Request to validate
     * @returns true if request is valid
     */
    bool validateRequest(const HttpRequest& rRequest) const;
    
    /**
     * Make HTTP POST request to localhost using basic socket implementation
     * 
     * @param rsHost Target hostname (typically "localhost")
     * @param nPort Target port number
     * @param rsPath Request path (e.g., "/api/agent")
     * @param rsBody JSON request body
     * @param rHeaders HTTP headers map
     * @returns HTTP response with status and body
     */
    HttpResponse makeHttpPostRequest(const OUString& rsHost, sal_Int32 nPort, 
                                   const OUString& rsPath, const OUString& rsBody,
                                   const std::map<OUString, OUString>& rHeaders);
};

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */