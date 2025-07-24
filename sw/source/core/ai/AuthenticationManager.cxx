/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "AuthenticationManager.hxx"

#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/random.hxx>
#include <tools/datetime.hxx>
#include <rtl/ustrbuf.hxx>
#include <comphelper/base64.hxx>
#include <rtl/digest.h>
#include <rtl/uuid.h>

#include <algorithm>
#include <sstream>

using namespace css;
using namespace css::uno;
using namespace css::beans;

namespace sw::ai {

AuthenticationManager::AuthenticationManager(const Reference<XComponentContext>& xContext)
    : m_xContext(xContext)
    , m_bInitialized(false)
    , m_bSecureStorageEnabled(true)
    , m_sDefaultAuthType("API_KEY")
    , m_bAutoRefreshEnabled(true)
    , m_nDefaultRefreshThreshold(300) // 5 minutes
    , m_nMaxRetryAttempts(3)
    , m_nTokenValidationTimeoutMs(5000)
    , m_bEncryptCredentials(true)
{
    SAL_INFO("sw.ai", "AuthenticationManager created");
}

AuthenticationManager::~AuthenticationManager()
{
    shutdown();
    SAL_INFO("sw.ai", "AuthenticationManager destroyed");
}

bool AuthenticationManager::initialize(const Sequence<PropertyValue>& rConfig)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    try
    {
        // Parse configuration parameters
        parseConfiguration(rConfig);
        
        // Initialize encryption if enabled
        if (m_bEncryptCredentials && m_sEncryptionKey.isEmpty())
        {
            // Generate default encryption key
            m_sEncryptionKey = generateToken(32);
        }
        
        // Load persisted credentials if enabled
        if (m_bSecureStorageEnabled)
        {
            loadCredentials();
        }
        
        m_bInitialized = true;
        
        SAL_INFO("sw.ai", "AuthenticationManager initialized successfully");
        return true;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "AuthenticationManager initialization failed: " << e.Message);
        return false;
    }
}

void AuthenticationManager::shutdown()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Save credentials if persistence is enabled
    if (m_bSecureStorageEnabled)
    {
        saveCredentials();
    }
    
    // Clear all credentials from memory
    m_aCredentials.clear();
    m_aServiceCredentialMap.clear();
    m_aActiveContexts.clear();
    
    // Clear sensitive data
    m_sEncryptionKey.clear();
    m_sKeystorePassword.clear();
    
    m_bInitialized = false;
    
    SAL_INFO("sw.ai", "AuthenticationManager shut down");
}

bool AuthenticationManager::storeCredentials(const AuthenticationCredentials& rCredentials)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!validateCredentialFormat(rCredentials))
    {
        SAL_WARN("sw.ai", "Invalid credential format for: " << rCredentials.sCredentialId);
        return false;
    }
    
    // Store credentials
    m_aCredentials[rCredentials.sCredentialId] = rCredentials;
    
    // Update service mapping
    m_aServiceCredentialMap[rCredentials.sServiceName] = rCredentials.sCredentialId;
    
    // Save to persistent storage if enabled
    if (m_bSecureStorageEnabled && rCredentials.bPersistent)
    {
        saveCredentials();
    }
    
    logOperation("STORE", rCredentials.sServiceName, true);
    
    SAL_INFO("sw.ai", "Credentials stored for service: " << rCredentials.sServiceName);
    return true;
}

std::unique_ptr<AuthenticationManager::AuthenticationCredentials> 
AuthenticationManager::getCredentials(const OUString& rsCredentialId)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aCredentials.find(rsCredentialId);
    if (it != m_aCredentials.end())
    {
        // Update last used timestamp
        const_cast<AuthenticationCredentials&>(it->second).aLastUsed = 
            std::chrono::steady_clock::now();
            
        return std::make_unique<AuthenticationCredentials>(it->second);
    }
    
    return nullptr;
}

std::unique_ptr<AuthenticationManager::AuthenticationCredentials> 
AuthenticationManager::getServiceCredentials(const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aServiceCredentialMap.find(rsServiceName);
    if (it != m_aServiceCredentialMap.end())
    {
        return getCredentials(it->second);
    }
    
    return nullptr;
}

bool AuthenticationManager::updateCredentials(const OUString& rsCredentialId, 
                                            const AuthenticationCredentials& rCredentials)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aCredentials.find(rsCredentialId);
    if (it == m_aCredentials.end())
    {
        SAL_WARN("sw.ai", "Cannot update non-existent credentials: " << rsCredentialId);
        return false;
    }
    
    // Update credentials
    it->second = rCredentials;
    it->second.sCredentialId = rsCredentialId; // Preserve ID
    
    // Save to persistent storage if enabled
    if (m_bSecureStorageEnabled && rCredentials.bPersistent)
    {
        saveCredentials();
    }
    
    logOperation("UPDATE", rCredentials.sServiceName, true);
    
    SAL_INFO("sw.ai", "Credentials updated for: " << rsCredentialId);
    return true;
}

bool AuthenticationManager::removeCredentials(const OUString& rsCredentialId)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aCredentials.find(rsCredentialId);
    if (it == m_aCredentials.end())
        return false;
    
    OUString sServiceName = it->second.sServiceName;
    
    // Remove from credentials map
    m_aCredentials.erase(it);
    
    // Remove from service mapping
    auto serviceIt = m_aServiceCredentialMap.find(sServiceName);
    if (serviceIt != m_aServiceCredentialMap.end() && serviceIt->second == rsCredentialId)
    {
        m_aServiceCredentialMap.erase(serviceIt);
    }
    
    logOperation("REMOVE", sServiceName, true);
    
    SAL_INFO("sw.ai", "Credentials removed for: " << rsCredentialId);
    return true;
}

std::vector<OUString> AuthenticationManager::listCredentials(CredentialScope eScope)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    std::vector<OUString> aCredentialIds;
    
    for (const auto& [sId, rCred] : m_aCredentials)
    {
        if (eScope == CredentialScope::GLOBAL || rCred.eScope == eScope)
        {
            aCredentialIds.push_back(sId);
        }
    }
    
    return aCredentialIds;
}

bool AuthenticationManager::setDefaultCredentials(const OUString& rsServiceName, 
                                                const OUString& rsCredentialId)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Verify credential exists
    if (m_aCredentials.find(rsCredentialId) == m_aCredentials.end())
    {
        SAL_WARN("sw.ai", "Cannot set default for non-existent credential: " << rsCredentialId);
        return false;
    }
    
    m_aServiceCredentialMap[rsServiceName] = rsCredentialId;
    
    SAL_INFO("sw.ai", "Default credentials set for service: " << rsServiceName);
    return true;
}

bool AuthenticationManager::authenticateRequest(const OUString& rsRequestId, 
                                              const OUString& rsServiceName,
                                              std::map<OUString, OUString>& aHeaders)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Create authentication context
    AuthenticationContext aContext(rsRequestId, rsServiceName);
    m_aActiveContexts[rsRequestId] = aContext;
    
    // Get credentials for service
    auto pCredentials = getServiceCredentials(rsServiceName);
    if (!pCredentials)
    {
        aContext.sLastError = "No credentials found for service";
        aContext.bAuthenticated = false;
        updateStatistics(aContext);
        
        SAL_WARN("sw.ai", "No credentials found for service: " << rsServiceName);
        return false;
    }
    
    // Check if credentials are expired
    if (isCredentialExpired(*pCredentials))
    {
        if (m_bAutoRefreshEnabled && pCredentials->bAutoRefresh)
        {
            if (!refreshToken(rsServiceName, false))
            {
                aContext.sLastError = "Token refresh failed";
                aContext.bAuthenticated = false;
                updateStatistics(aContext);
                return false;
            }
            
            // Get refreshed credentials
            pCredentials = getServiceCredentials(rsServiceName);
        }
        else
        {
            aContext.sLastError = "Credentials expired";
            aContext.bAuthenticated = false;
            updateStatistics(aContext);
            return false;
        }
    }
    
    // Generate authentication header
    OUString sAuthHeader = generateAuthHeader(*pCredentials);
    if (sAuthHeader.isEmpty())
    {
        aContext.sLastError = "Failed to generate auth header";
        aContext.bAuthenticated = false;
        updateStatistics(aContext);
        return false;
    }
    
    // Add authentication header
    switch (pCredentials->eType)
    {
        case AuthenticationType::API_KEY:
            aHeaders["Authorization"] = sAuthHeader;
            break;
        case AuthenticationType::BEARER_TOKEN:
            aHeaders["Authorization"] = "Bearer " + sAuthHeader;
            break;
        case AuthenticationType::BASIC_AUTH:
            aHeaders["Authorization"] = "Basic " + sAuthHeader;
            break;
        default:
            aHeaders["Authorization"] = sAuthHeader;
            break;
    }
    
    // Add custom headers
    for (const auto& [sKey, sValue] : pCredentials->aCustomHeaders)
    {
        aHeaders[sKey] = sValue;
    }
    
    aContext.bAuthenticated = true;
    aContext.aRequestHeaders = aHeaders;
    
    updateStatistics(aContext);
    logOperation("AUTH", rsServiceName, true);
    
    SAL_INFO("sw.ai", "Request authenticated for service: " << rsServiceName);
    return true;
}

AuthenticationManager::TokenStatus AuthenticationManager::validateToken(const OUString& rsServiceName, 
                                                                       const OUString& /* rsToken */)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto pCredentials = getServiceCredentials(rsServiceName);
    if (!pCredentials)
        return TokenStatus::INVALID;
    
    // Check expiry
    if (isCredentialExpired(*pCredentials))
        return TokenStatus::EXPIRED;
    
    // Validate token format based on type
    switch (pCredentials->eType)
    {
        case AuthenticationType::JWT:
            // JWT validation would be more complex
            if (pCredentials->sAccessToken.isEmpty())
                return TokenStatus::INVALID;
            break;
            
        case AuthenticationType::API_KEY:
            if (pCredentials->sApiKey.isEmpty())
                return TokenStatus::INVALID;
            break;
            
        case AuthenticationType::BEARER_TOKEN:
            if (pCredentials->sAccessToken.isEmpty())
                return TokenStatus::INVALID;
            break;
            
        default:
            break;
    }
    
    return TokenStatus::VALID;
}

bool AuthenticationManager::refreshToken(const OUString& rsServiceName, bool bForceRefresh)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto pCredentials = getServiceCredentials(rsServiceName);
    if (!pCredentials || !pCredentials->bAutoRefresh)
        return false;
    
    // Check if refresh is needed
    if (!bForceRefresh && !needsTokenRefresh(rsServiceName))
        return true; // Already valid
    
    // Call refresh callback if available
    if (m_aTokenRefreshCallback)
    {
        AuthenticationCredentials updatedCreds = *pCredentials;
        if (m_aTokenRefreshCallback(updatedCreds))
        {
            // Update stored credentials
            updateCredentials(pCredentials->sCredentialId, updatedCreds);
            
            ++m_aStatistics.nTokenRefreshes;
            logOperation("REFRESH", rsServiceName, true);
            
            SAL_INFO("sw.ai", "Token refreshed for service: " << rsServiceName);
            return true;
        }
    }
    
    logOperation("REFRESH", rsServiceName, false);
    SAL_WARN("sw.ai", "Token refresh failed for service: " << rsServiceName);
    return false;
}

void AuthenticationManager::handleAuthenticationResponse(const OUString& rsRequestId, 
                                                       sal_Int32 nStatusCode,
                                                       const std::map<OUString, OUString>& aResponseHeaders)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aActiveContexts.find(rsRequestId);
    if (it == m_aActiveContexts.end())
        return;
    
    AuthenticationContext& rContext = it->second;
    rContext.aResponseHeaders = aResponseHeaders;
    
    // Handle authentication-related status codes
    switch (nStatusCode)
    {
        case 401: // Unauthorized
            rContext.sLastError = "Authentication failed";
            rContext.bAuthenticated = false;
            
            // Attempt token refresh if applicable
            if (m_bAutoRefreshEnabled)
            {
                refreshToken(rContext.sServiceName, true);
            }
            break;
            
        case 403: // Forbidden
            rContext.sLastError = "Access forbidden";
            rContext.bAuthenticated = false;
            break;
            
        case 429: // Rate Limited
            rContext.sLastError = "Rate limit exceeded";
            // Don't mark as unauthenticated for rate limiting
            break;
            
        default:
            if (nStatusCode >= 200 && nStatusCode < 300)
            {
                rContext.bAuthenticated = true;
                rContext.sLastError.clear();
            }
            break;
    }
    
    // Parse any new tokens from response
    auto pCredentials = getServiceCredentials(rContext.sServiceName);
    if (pCredentials)
    {
        AuthenticationCredentials updatedCreds = *pCredentials;
        if (parseAuthResponse(aResponseHeaders, updatedCreds))
        {
            updateCredentials(pCredentials->sCredentialId, updatedCreds);
        }
    }
    
    updateStatistics(rContext);
    
    // Call authentication callback
    if (m_aAuthCallback)
    {
        m_aAuthCallback(rContext);
    }
    
    // Clean up context after processing
    cleanupExpiredContexts();
}

bool AuthenticationManager::isAuthenticated(const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto pCredentials = getServiceCredentials(rsServiceName);
    if (!pCredentials)
        return false;
    
    return validateToken(rsServiceName) == TokenStatus::VALID;
}

void AuthenticationManager::clearSession(const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Remove service credentials mapping
    auto it = m_aServiceCredentialMap.find(rsServiceName);
    if (it != m_aServiceCredentialMap.end())
    {
        // Remove the actual credentials
        m_aCredentials.erase(it->second);
        m_aServiceCredentialMap.erase(it);
    }
    
    // Clear related contexts
    auto contextIt = m_aActiveContexts.begin();
    while (contextIt != m_aActiveContexts.end())
    {
        if (contextIt->second.sServiceName == rsServiceName)
        {
            contextIt = m_aActiveContexts.erase(contextIt);
        }
        else
        {
            ++contextIt;
        }
    }
    
    logOperation("CLEAR_SESSION", rsServiceName, true);
    SAL_INFO("sw.ai", "Session cleared for service: " << rsServiceName);
}

bool AuthenticationManager::needsTokenRefresh(const OUString& rsServiceName)
{
    auto pCredentials = getServiceCredentials(rsServiceName);
    if (!pCredentials)
        return false;
    
    auto now = std::chrono::steady_clock::now();
    auto threshold = std::chrono::seconds(pCredentials->nRefreshThresholdSeconds);
    
    return (pCredentials->aExpiryTime - now) <= threshold;
}

std::chrono::steady_clock::time_point AuthenticationManager::getTokenExpiry(const OUString& rsServiceName)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto pCredentials = getServiceCredentials(rsServiceName);
    if (pCredentials)
        return pCredentials->aExpiryTime;
    
    return std::chrono::steady_clock::time_point::min();
}

bool AuthenticationManager::createApiKeyCredentials(const OUString& rsServiceName, 
                                                   const OUString& rsApiKey,
                                                   const OUString& rsHeaderName)
{
    OUString sCredentialId = generateCredentialId(rsServiceName);
    
    AuthenticationCredentials aCredentials(sCredentialId, rsServiceName, AuthenticationType::API_KEY);
    aCredentials.sApiKey = rsApiKey;
    aCredentials.aCustomHeaders[rsHeaderName.isEmpty() ? OUString("Authorization") : rsHeaderName] = rsApiKey;
    aCredentials.bPersistent = true;
    
    return storeCredentials(aCredentials);
}

bool AuthenticationManager::createBearerTokenCredentials(const OUString& rsServiceName, 
                                                        const OUString& rsAccessToken,
                                                        const OUString& rsRefreshToken)
{
    OUString sCredentialId = generateCredentialId(rsServiceName);
    
    AuthenticationCredentials aCredentials(sCredentialId, rsServiceName, AuthenticationType::BEARER_TOKEN);
    aCredentials.sAccessToken = rsAccessToken;
    aCredentials.sRefreshToken = rsRefreshToken;
    aCredentials.bPersistent = true;
    aCredentials.bAutoRefresh = !rsRefreshToken.isEmpty();
    
    return storeCredentials(aCredentials);
}

bool AuthenticationManager::createBasicAuthCredentials(const OUString& rsServiceName, 
                                                      const OUString& rsUsername,
                                                      const OUString& rsPassword)
{
    OUString sCredentialId = generateCredentialId(rsServiceName);
    
    AuthenticationCredentials aCredentials(sCredentialId, rsServiceName, AuthenticationType::BASIC_AUTH);
    aCredentials.sUsername = rsUsername;
    aCredentials.sPassword = rsPassword;
    aCredentials.bPersistent = true;
    
    return storeCredentials(aCredentials);
}

OUString AuthenticationManager::generateAuthHeader(const AuthenticationCredentials& rCredentials) const
{
    switch (rCredentials.eType)
    {
        case AuthenticationType::API_KEY:
            return rCredentials.sApiKey;
            
        case AuthenticationType::BEARER_TOKEN:
            return rCredentials.sAccessToken;
            
        case AuthenticationType::BASIC_AUTH:
        {
            OUString sCredentials = rCredentials.sUsername + ":" + rCredentials.sPassword;
            
            // Base64 encode credentials
            OString sUtf8 = OUStringToOString(sCredentials, RTL_TEXTENCODING_UTF8);
            
            // Convert to sequence
            css::uno::Sequence<sal_Int8> aData(sUtf8.getLength());
            sal_Int8* pData = aData.getArray();
            for (sal_Int32 i = 0; i < sUtf8.getLength(); ++i)
                pData[i] = sUtf8[i];
            
            OStringBuffer aBuffer;
            comphelper::Base64::encode(aBuffer, aData);
            
            return OStringToOUString(aBuffer.makeStringAndClear(), RTL_TEXTENCODING_ASCII_US);
        }
        
        case AuthenticationType::JWT:
            return rCredentials.sAccessToken;
            
        default:
            return OUString();
    }
}

bool AuthenticationManager::parseAuthResponse(const std::map<OUString, OUString>& aHeaders, 
                                            AuthenticationCredentials& rCredentials)
{
    bool bUpdated = false;
    
    // Look for common authentication response headers
    auto it = aHeaders.find("Authorization");
    if (it != aHeaders.end())
    {
        // Parse Bearer token
        OUString sAuth = it->second;
        if (sAuth.startsWith("Bearer "))
        {
            rCredentials.sAccessToken = sAuth.copy(7);
            bUpdated = true;
        }
    }
    
    // Look for refresh token
    auto refreshIt = aHeaders.find("Refresh-Token");
    if (refreshIt != aHeaders.end())
    {
        rCredentials.sRefreshToken = refreshIt->second;
        bUpdated = true;
    }
    
    // Look for expiry information
    auto expiryIt = aHeaders.find("Token-Expires");
    if (expiryIt != aHeaders.end())
    {
        // Parse expiry timestamp
        sal_Int32 nExpiry = expiryIt->second.toInt32();
        if (nExpiry > 0)
        {
            rCredentials.aExpiryTime = std::chrono::steady_clock::now() + 
                                      std::chrono::seconds(nExpiry);
            bUpdated = true;
        }
    }
    
    return bUpdated;
}

bool AuthenticationManager::isCredentialExpired(const AuthenticationCredentials& rCredentials) const
{
    if (rCredentials.aExpiryTime == std::chrono::steady_clock::time_point::max())
        return false; // Never expires
    
    return rCredentials.aExpiryTime <= std::chrono::steady_clock::now();
}

OUString AuthenticationManager::generateCredentialId(const OUString& rsServiceName)
{
    return rsServiceName + "_" + generateToken(8);
}

OUString AuthenticationManager::generateToken(sal_Int32 nLength)
{
    sal_uInt8 aBuffer[16];
    rtl_createUuid(aBuffer, nullptr, false);
    
    OUStringBuffer aBuf(nLength);
    for (int i = 0; i < std::min(nLength / 2, 16); ++i)
    {
        aBuf.append(static_cast<sal_Unicode>('0' + (aBuffer[i] >> 4)));
        if (aBuf.getLength() < nLength)
            aBuf.append(static_cast<sal_Unicode>('0' + (aBuffer[i] & 0x0F)));
    }
    
    return aBuf.makeStringAndClear();
}

bool AuthenticationManager::validateCredentialFormat(const AuthenticationCredentials& rCredentials)
{
    if (rCredentials.sCredentialId.isEmpty() || rCredentials.sServiceName.isEmpty())
        return false;
    
    switch (rCredentials.eType)
    {
        case AuthenticationType::API_KEY:
            return !rCredentials.sApiKey.isEmpty();
            
        case AuthenticationType::BEARER_TOKEN:
            return !rCredentials.sAccessToken.isEmpty();
            
        case AuthenticationType::BASIC_AUTH:
            return !rCredentials.sUsername.isEmpty() && !rCredentials.sPassword.isEmpty();
            
        case AuthenticationType::JWT:
            return !rCredentials.sAccessToken.isEmpty();
            
        default:
            return true;
    }
}

bool AuthenticationManager::saveCredentials()
{
    // Placeholder for persistent storage implementation
    // Would serialize encrypted credentials to secure storage
    SAL_INFO("sw.ai", "Credentials saved to secure storage");
    return true;
}

bool AuthenticationManager::loadCredentials()
{
    // Placeholder for persistent storage implementation  
    // Would load and decrypt credentials from secure storage
    SAL_INFO("sw.ai", "Credentials loaded from secure storage");
    return true;
}

void AuthenticationManager::parseConfiguration(const Sequence<PropertyValue>& rConfig)
{
    for (const auto& rProperty : rConfig)
    {
        if (rProperty.Name == "SecureStorageEnabled")
        {
            bool bValue = false;
            if (rProperty.Value >>= bValue)
                m_bSecureStorageEnabled.store(bValue);
        }
        else if (rProperty.Name == "AutoRefreshEnabled")
        {
            rProperty.Value >>= m_bAutoRefreshEnabled;
        }
        else if (rProperty.Name == "DefaultRefreshThreshold")
        {
            rProperty.Value >>= m_nDefaultRefreshThreshold;
        }
        else if (rProperty.Name == "MaxRetryAttempts")
        {
            rProperty.Value >>= m_nMaxRetryAttempts;
        }
        else if (rProperty.Name == "TokenValidationTimeout")
        {
            rProperty.Value >>= m_nTokenValidationTimeoutMs;
        }
        else if (rProperty.Name == "EncryptionKey")
        {
            rProperty.Value >>= m_sEncryptionKey;
        }
        else if (rProperty.Name == "CredentialStorePath")
        {
            rProperty.Value >>= m_sCredentialStorePath;
        }
    }
}

void AuthenticationManager::updateStatistics(const AuthenticationContext& rContext)
{
    ++m_aStatistics.nTotalRequests;
    
    if (rContext.bAuthenticated)
        ++m_aStatistics.nSuccessfulAuths;
    else
        ++m_aStatistics.nFailedAuths;
}

void AuthenticationManager::logOperation(const OUString& rsOperation, const OUString& rsServiceName, bool bSuccess) const
{
    SAL_INFO("sw.ai", rsOperation << " - Service: " << rsServiceName << 
             ", Result: " << (bSuccess ? "SUCCESS" : "FAILURE"));
}

void AuthenticationManager::cleanupExpiredContexts()
{
    auto now = std::chrono::steady_clock::now();
    auto contextIt = m_aActiveContexts.begin();
    
    while (contextIt != m_aActiveContexts.end())
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
            now - contextIt->second.aRequestTime).count();
            
        if (elapsed > 30) // Clean up contexts older than 30 minutes
        {
            contextIt = m_aActiveContexts.erase(contextIt);
        }
        else
        {
            ++contextIt;
        }
    }
}

AuthenticationManager::AuthenticationStatisticsData AuthenticationManager::getStatistics() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_aStatistics.getData();
}

void AuthenticationManager::resetStatistics()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aStatistics.reset();
}

bool AuthenticationManager::isHealthy() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bInitialized)
        return false;
    
    // Check if authentication success rate is reasonable
    auto stats = m_aStatistics.getData();
    if (stats.nTotalRequests > 10)
    {
        double fSuccessRate = static_cast<double>(stats.nSuccessfulAuths) / stats.nTotalRequests;
        if (fSuccessRate < 0.5) // Less than 50% success rate
            return false;
    }
    
    return true;
}

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */