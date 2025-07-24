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
#include <com/sun/star/beans/PropertyValue.hpp>
#include <rtl/ustrbuf.hxx>

#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <functional>
#include <mutex>
#include <atomic>

namespace sw::ai {

/**
 * AuthenticationManager - Secure credential management for AI backend communication
 * 
 * This class provides comprehensive authentication and authorization management
 * for the AI Agent system, including API key management, token refresh,
 * secure credential storage, and session management.
 * 
 * Design Principles:
 * - Security: Encrypted credential storage and secure transmission
 * - Flexibility: Support for multiple authentication schemes
 * - Reliability: Automatic token refresh and session recovery
 * - Compliance: GDPR-compliant credential handling
 * - Performance: Efficient credential caching and validation
 */
class AuthenticationManager
{
public:
    enum class AuthenticationType
    {
        NONE,               // No authentication
        API_KEY,            // Simple API key authentication
        BEARER_TOKEN,       // OAuth 2.0 Bearer tokens
        BASIC_AUTH,         // HTTP Basic authentication
        OAUTH2,             // Full OAuth 2.0 flow
        JWT,                // JWT token authentication
        CUSTOM              // Custom authentication scheme
    };

    enum class TokenStatus
    {
        VALID,              // Token is valid and active
        EXPIRED,            // Token has expired
        INVALID,            // Token is malformed or invalid
        REVOKED,            // Token has been revoked
        UNKNOWN             // Token status cannot be determined
    };

    enum class CredentialScope
    {
        GLOBAL,             // Global credentials for all services
        SERVICE_SPECIFIC,   // Credentials for specific service
        USER_SPECIFIC,      // User-specific credentials
        SESSION_SPECIFIC    // Session-specific credentials
    };

    struct AuthenticationCredentials
    {
        OUString sCredentialId;
        OUString sServiceName;
        AuthenticationType eType;
        CredentialScope eScope;
        
        OUString sApiKey;
        OUString sAccessToken;
        OUString sRefreshToken;
        OUString sClientId;
        OUString sClientSecret;
        OUString sUsername;
        OUString sPassword;
        
        std::chrono::steady_clock::time_point aExpiryTime;
        std::chrono::steady_clock::time_point aIssuedTime;
        std::chrono::steady_clock::time_point aLastUsed;
        
        sal_Int32 nRefreshThresholdSeconds;
        bool bAutoRefresh;
        bool bPersistent;
        
        std::map<OUString, OUString> aCustomHeaders;
        std::map<OUString, OUString> aCustomParameters;
        
        AuthenticationCredentials() = default;
        
        AuthenticationCredentials(const OUString& rsId, const OUString& rsService, 
                                AuthenticationType eAuthType = AuthenticationType::API_KEY)
            : sCredentialId(rsId), sServiceName(rsService), eType(eAuthType)
            , eScope(CredentialScope::SERVICE_SPECIFIC)
            , aExpiryTime(std::chrono::steady_clock::time_point::max())
            , aIssuedTime(std::chrono::steady_clock::now())
            , aLastUsed(std::chrono::steady_clock::now())
            , nRefreshThresholdSeconds(300), bAutoRefresh(true), bPersistent(false) {}
    };

    struct AuthenticationContext
    {
        OUString sRequestId;
        OUString sServiceName;
        OUString sUserId;
        OUString sSessionId;
        
        std::map<OUString, OUString> aRequestHeaders;
        std::map<OUString, OUString> aResponseHeaders;
        
        std::chrono::steady_clock::time_point aRequestTime;
        bool bAuthenticated;
        OUString sLastError;
        
        AuthenticationContext() = default;
        
        AuthenticationContext(const OUString& rsReqId, const OUString& rsService)
            : sRequestId(rsReqId), sServiceName(rsService)
            , aRequestTime(std::chrono::steady_clock::now())
            , bAuthenticated(false) {}
    };

    // Callback function types
    using TokenRefreshCallback = std::function<bool(AuthenticationCredentials&)>;
    using AuthenticationCallback = std::function<void(const AuthenticationContext&)>;
    using CredentialValidationCallback = std::function<bool(const AuthenticationCredentials&)>;

private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    
    // Thread safety
    mutable std::mutex m_aMutex;
    std::atomic<bool> m_bInitialized;
    std::atomic<bool> m_bSecureStorageEnabled;
    
    // Credential storage
    std::map<OUString, AuthenticationCredentials> m_aCredentials;    // Credential ID -> Credentials
    std::map<OUString, OUString> m_aServiceCredentialMap;           // Service -> Credential ID
    std::map<OUString, AuthenticationContext> m_aActiveContexts;    // Request ID -> Context
    
    // Configuration
    OUString m_sDefaultAuthType;
    OUString m_sCredentialStorePath;
    bool m_bAutoRefreshEnabled;
    sal_Int32 m_nDefaultRefreshThreshold;
    sal_Int32 m_nMaxRetryAttempts;
    sal_Int32 m_nTokenValidationTimeoutMs;
    
    // Security
    OUString m_sEncryptionKey;
    OUString m_sKeystorePassword;
    bool m_bEncryptCredentials;
    
    // Callbacks
    TokenRefreshCallback m_aTokenRefreshCallback;
    AuthenticationCallback m_aAuthCallback;
    CredentialValidationCallback m_aValidationCallback;
    
    // Statistics - separate atomic data from copyable snapshot
    struct AuthenticationStatisticsData
    {
        sal_Int32 nTotalRequests;
        sal_Int32 nSuccessfulAuths;
        sal_Int32 nFailedAuths;
        sal_Int32 nTokenRefreshes;
        sal_Int32 nCredentialRotations;
        std::chrono::steady_clock::time_point aLastReset;
    };
    
    struct AuthenticationStatistics
    {
        std::atomic<sal_Int32> nTotalRequests{0};
        std::atomic<sal_Int32> nSuccessfulAuths{0};
        std::atomic<sal_Int32> nFailedAuths{0};
        std::atomic<sal_Int32> nTokenRefreshes{0};
        std::atomic<sal_Int32> nCredentialRotations{0};
        std::chrono::steady_clock::time_point aLastReset;
        
        AuthenticationStatistics() : aLastReset(std::chrono::steady_clock::now()) {}
        
        void reset() {
            nTotalRequests = 0;
            nSuccessfulAuths = 0;
            nFailedAuths = 0;
            nTokenRefreshes = 0;
            nCredentialRotations = 0;
            aLastReset = std::chrono::steady_clock::now();
        }
        
        AuthenticationStatisticsData getData() const {
            return {
                nTotalRequests.load(),
                nSuccessfulAuths.load(),
                nFailedAuths.load(),
                nTokenRefreshes.load(),
                nCredentialRotations.load(),
                aLastReset
            };
        }
    };
    
    AuthenticationStatistics m_aStatistics;

public:
    explicit AuthenticationManager(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    ~AuthenticationManager();

    // Lifecycle management
    
    /**
     * Initialize authentication manager with configuration
     */
    bool initialize(const css::uno::Sequence<css::beans::PropertyValue>& rConfig = {});
    
    /**
     * Shutdown authentication manager
     */
    void shutdown();

    // Credential management
    
    /**
     * Store authentication credentials
     */
    bool storeCredentials(const AuthenticationCredentials& rCredentials);
    
    /**
     * Retrieve authentication credentials by ID
     */
    std::unique_ptr<AuthenticationCredentials> getCredentials(const OUString& rsCredentialId);
    
    /**
     * Retrieve credentials for specific service
     */
    std::unique_ptr<AuthenticationCredentials> getServiceCredentials(const OUString& rsServiceName);
    
    /**
     * Update existing credentials
     */
    bool updateCredentials(const OUString& rsCredentialId, const AuthenticationCredentials& rCredentials);
    
    /**
     * Remove credentials
     */
    bool removeCredentials(const OUString& rsCredentialId);
    
    /**
     * List all stored credentials
     */
    std::vector<OUString> listCredentials(CredentialScope eScope = CredentialScope::GLOBAL);
    
    /**
     * Set default credentials for service
     */
    bool setDefaultCredentials(const OUString& rsServiceName, const OUString& rsCredentialId);

    // Authentication operations
    
    /**
     * Authenticate request and get headers
     */
    bool authenticateRequest(const OUString& rsRequestId, const OUString& rsServiceName,
                           std::map<OUString, OUString>& aHeaders);
    
    /**
     * Validate authentication token
     */
    TokenStatus validateToken(const OUString& rsServiceName, const OUString& rsToken = OUString());
    
    /**
     * Refresh authentication token
     */
    bool refreshToken(const OUString& rsServiceName, bool bForceRefresh = false);
    
    /**
     * Handle authentication response
     */
    void handleAuthenticationResponse(const OUString& rsRequestId, sal_Int32 nStatusCode,
                                    const std::map<OUString, OUString>& aResponseHeaders);
    
    /**
     * Check if service is authenticated
     */
    bool isAuthenticated(const OUString& rsServiceName);
    
    /**
     * Clear authentication session
     */
    void clearSession(const OUString& rsServiceName);

    // Token management
    
    /**
     * Check if token needs refresh
     */
    bool needsTokenRefresh(const OUString& rsServiceName);
    
    /**
     * Get token expiry time
     */
    std::chrono::steady_clock::time_point getTokenExpiry(const OUString& rsServiceName);
    
    /**
     * Set token refresh threshold
     */
    void setRefreshThreshold(const OUString& rsServiceName, sal_Int32 nThresholdSeconds);
    
    /**
     * Enable/disable auto refresh
     */
    void setAutoRefreshEnabled(const OUString& rsServiceName, bool bEnabled);

    // Secure storage
    
    /**
     * Enable/disable secure credential storage
     */
    void setSecureStorageEnabled(bool bEnabled);
    
    /**
     * Set encryption key for credential storage
     */
    bool setEncryptionKey(const OUString& rsKey);
    
    /**
     * Save all credentials to secure storage
     */
    bool saveCredentials();
    
    /**
     * Load credentials from secure storage
     */
    bool loadCredentials();
    
    /**
     * Clear all stored credentials
     */
    void clearCredentials();

    // Configuration
    
    /**
     * Set default authentication type
     */
    void setDefaultAuthenticationType(AuthenticationType eType);
    
    /**
     * Set credential storage path
     */
    void setCredentialStorePath(const OUString& rsPath);
    
    /**
     * Set maximum retry attempts
     */
    void setMaxRetryAttempts(sal_Int32 nAttempts);
    
    /**
     * Set token validation timeout
     */
    void setValidationTimeout(sal_Int32 nTimeoutMs);

    // Callbacks
    
    /**
     * Set token refresh callback
     */
    void setTokenRefreshCallback(const TokenRefreshCallback& aCallback);
    
    /**
     * Set authentication callback
     */
    void setAuthenticationCallback(const AuthenticationCallback& aCallback);
    
    /**
     * Set credential validation callback
     */
    void setValidationCallback(const CredentialValidationCallback& aCallback);

    // Statistics and monitoring
    
    /**
     * Get authentication statistics
     */
    AuthenticationStatisticsData getStatistics() const;
    
    /**
     * Reset statistics
     */
    void resetStatistics();
    
    /**
     * Get authentication health status
     */
    bool isHealthy() const;

    // Utility methods
    
    /**
     * Generate credential ID
     */
    static OUString generateCredentialId(const OUString& rsServiceName);
    
    /**
     * Hash password securely
     */
    static OUString hashPassword(const OUString& rsPassword, const OUString& rsSalt = OUString());
    
    /**
     * Generate secure random token
     */
    static OUString generateToken(sal_Int32 nLength = 32);
    
    /**
     * Validate credential format
     */
    static bool validateCredentialFormat(const AuthenticationCredentials& rCredentials);

    // Convenience methods for common authentication types
    
    /**
     * Create API key credentials
     */
    bool createApiKeyCredentials(const OUString& rsServiceName, const OUString& rsApiKey,
                               const OUString& rsHeaderName = "Authorization");
    
    /**
     * Create Bearer token credentials
     */
    bool createBearerTokenCredentials(const OUString& rsServiceName, const OUString& rsAccessToken,
                                    const OUString& rsRefreshToken = OUString());
    
    /**
     * Create Basic auth credentials
     */
    bool createBasicAuthCredentials(const OUString& rsServiceName, const OUString& rsUsername,
                                  const OUString& rsPassword);
    
    /**
     * Create OAuth2 credentials
     */
    bool createOAuth2Credentials(const OUString& rsServiceName, const OUString& rsClientId,
                               const OUString& rsClientSecret, const OUString& rsScope = OUString());

private:
    // Internal implementation methods
    
    /**
     * Encrypt credential data
     */
    OUString encryptData(const OUString& rsData) const;
    
    /**
     * Decrypt credential data
     */
    OUString decryptData(const OUString& rsEncryptedData) const;
    
    /**
     * Generate authentication header
     */
    OUString generateAuthHeader(const AuthenticationCredentials& rCredentials) const;
    
    /**
     * Parse authentication response
     */
    bool parseAuthResponse(const std::map<OUString, OUString>& aHeaders, 
                          AuthenticationCredentials& rCredentials);
    
    /**
     * Validate credential expiry
     */
    bool isCredentialExpired(const AuthenticationCredentials& rCredentials) const;
    
    /**
     * Serialize credentials to JSON
     */
    OUString serializeCredentials(const AuthenticationCredentials& rCredentials) const;
    
    /**
     * Deserialize credentials from JSON
     */
    std::unique_ptr<AuthenticationCredentials> deserializeCredentials(const OUString& rsJson) const;
    
    /**
     * Parse configuration parameters
     */
    void parseConfiguration(const css::uno::Sequence<css::beans::PropertyValue>& rConfig);
    
    /**
     * Update credential statistics
     */
    void updateStatistics(const AuthenticationContext& rContext);
    
    /**
     * Log authentication operation
     */
    void logOperation(const OUString& rsOperation, const OUString& rsServiceName, bool bSuccess) const;
    
    /**
     * Cleanup expired contexts
     */
    void cleanupExpiredContexts();
};

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */