/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "MessageQueue.hxx"

#include <sal/log.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/random.hxx>
#include <tools/datetime.hxx>
#include <rtl/ustrbuf.hxx>
#include <rtl/uuid.h>

#include <fstream>
#include <sstream>
#include <algorithm>

using namespace css;
using namespace css::uno;
using namespace css::beans;

namespace sw::ai {

MessageQueue::MessageQueue(const Reference<XComponentContext>& xContext)
    : m_xContext(xContext)
    , m_bRunning(false)
    , m_bPersistenceEnabled(false)
    , m_aOutboundQueue(compareMessagePriority)
    , m_nMaxQueueSize(10000)
    , m_nMaxMessageSize(1048576) // 1MB
    , m_nDefaultTTLSeconds(86400) // 24 hours
    , m_nDeliveryTimeoutMs(30000) // 30 seconds
    , m_nRetryDelayMs(1000)
    , m_bEnableCompression(false)
    , m_nMaxMessagesPerSecond(100)
    , m_aLastSend(std::chrono::steady_clock::now())
    , m_nCurrentBurst(0)
{
    SAL_INFO("sw.ai", "MessageQueue created");
}

MessageQueue::~MessageQueue()
{
    shutdown();
    SAL_INFO("sw.ai", "MessageQueue destroyed");
}

bool MessageQueue::initialize(const Sequence<PropertyValue>& rConfig)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    try
    {
        // Parse configuration parameters
        parseConfiguration(rConfig);
        
        // Initialize message queue with priority comparator
        // Priority queue automatically orders by priority
        
        // Load persisted messages if enabled
        if (m_bPersistenceEnabled && !m_sPersistenceFile.isEmpty())
        {
            loadQueue();
        }
        
        SAL_INFO("sw.ai", "MessageQueue initialized successfully");
        return true;
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "MessageQueue initialization failed: " << e.Message);
        return false;
    }
}

bool MessageQueue::start()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (m_bRunning)
        return true;
    
    m_bRunning = true;
    
    SAL_INFO("sw.ai", "MessageQueue started");
    return true;
}

void MessageQueue::stop()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    m_bRunning = false;
    
    // Save queue state if persistence is enabled
    if (m_bPersistenceEnabled)
    {
        saveQueue();
    }
    
    SAL_INFO("sw.ai", "MessageQueue stopped");
}

void MessageQueue::shutdown()
{
    stop();
    
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Clear all queues
    while (!m_aOutboundQueue.empty())
        m_aOutboundQueue.pop();
    
    std::queue<QueuedMessage> emptyInbound;
    m_aInboundQueue.swap(emptyInbound);
    
    m_aPendingMessages.clear();
    m_aFailedMessages.clear();
    
    SAL_INFO("sw.ai", "MessageQueue shut down");
}

bool MessageQueue::enqueueMessage(const OUString& rsRequestId, const OUString& rsServiceName,
                                 const OUString& rsContent, MessagePriority ePriority,
                                 const std::map<OUString, OUString>& aHeaders,
                                 const std::map<OUString, OUString>& aMetadata)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bRunning)
    {
        SAL_WARN("sw.ai", "Cannot enqueue message - queue is not running");
        return false;
    }
    
    if (isFull())
    {
        SAL_WARN("sw.ai", "Cannot enqueue message - queue is full");
        return false;
    }
    
    // Generate unique message ID
    OUString sMessageId = generateMessageId();
    
    // Create queued message
    QueuedMessage aMessage(sMessageId, rsRequestId, rsServiceName, rsContent, ePriority);
    aMessage.aHeaders = aHeaders;
    aMessage.aMetadata = aMetadata;
    
    // Set TTL
    aMessage.aExpiry = std::chrono::steady_clock::now() + 
                      std::chrono::seconds(m_nDefaultTTLSeconds);
    
    // Validate message
    if (!validateMessage(aMessage))
    {
        SAL_WARN("sw.ai", "Message validation failed for request: " << rsRequestId);
        return false;
    }
    
    // Compress content if enabled
    if (m_bEnableCompression && aMessage.sContent.getLength() > 1024)
    {
        aMessage.sContent = compressContent(aMessage.sContent);
        aMessage.aHeaders["Content-Encoding"] = "gzip";
    }
    
    // Add to outbound queue
    m_aOutboundQueue.push(aMessage);
    
    // Update statistics
    updateStatistics(aMessage, MessageStatus::QUEUED);
    
    logOperation("ENQUEUE", aMessage);
    
    SAL_INFO("sw.ai", "Message enqueued: " << sMessageId << " for service: " << rsServiceName);
    return true;
}

bool MessageQueue::enqueueMessage(const QueuedMessage& rMessage)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bRunning || isFull())
        return false;
    
    if (!validateMessage(rMessage))
        return false;
    
    m_aOutboundQueue.push(rMessage);
    updateStatistics(rMessage, MessageStatus::QUEUED);
    logOperation("ENQUEUE", rMessage);
    
    return true;
}

std::unique_ptr<MessageQueue::QueuedMessage> MessageQueue::getNextMessage()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bRunning || m_aOutboundQueue.empty())
        return nullptr;
    
    // Check rate limiting
    if (!checkRateLimit())
    {
        SAL_INFO("sw.ai", "Rate limit exceeded - delaying message delivery");
        return nullptr;
    }
    
    // Purge expired messages first
    purgeExpiredMessages();
    
    if (m_aOutboundQueue.empty())
        return nullptr;
    
    // Get highest priority message
    QueuedMessage aMessage = m_aOutboundQueue.top();
    m_aOutboundQueue.pop();
    
    // Check if message has expired
    if (isMessageExpired(aMessage))
    {
        updateStatistics(aMessage, MessageStatus::EXPIRED);
        logOperation("EXPIRE", aMessage);
        return getNextMessage(); // Try next message
    }
    
    // Move to pending state
    aMessage.eStatus = MessageStatus::SENT;
    aMessage.aLastAttempt = std::chrono::steady_clock::now();
    aMessage.nAttemptCount++;
    
    m_aPendingMessages[aMessage.sMessageId] = aMessage;
    
    updateStatistics(aMessage, MessageStatus::SENT);
    logOperation("DELIVER", aMessage);
    
    return std::make_unique<QueuedMessage>(aMessage);
}

void MessageQueue::acknowledgeMessage(const OUString& rsMessageId, bool bSuccess)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aPendingMessages.find(rsMessageId);
    if (it == m_aPendingMessages.end())
    {
        SAL_WARN("sw.ai", "Cannot acknowledge unknown message: " << rsMessageId);
        return;
    }
    
    QueuedMessage& rMessage = it->second;
    
    if (bSuccess)
    {
        rMessage.eStatus = MessageStatus::ACKNOWLEDGED;
        updateStatistics(rMessage, MessageStatus::ACKNOWLEDGED);
        logOperation("ACK", rMessage);
        
        // Call acknowledgment callback
        if (m_aAckCallback)
        {
            m_aAckCallback(rsMessageId, true);
        }
        
        // Remove from pending
        m_aPendingMessages.erase(it);
    }
    else
    {
        reportFailure(rsMessageId, "Negative acknowledgment");
    }
    
    SAL_INFO("sw.ai", "Message " << rsMessageId << " acknowledged: " << (bSuccess ? "SUCCESS" : "FAILURE"));
}

void MessageQueue::reportFailure(const OUString& rsMessageId, const OUString& rsError)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aPendingMessages.find(rsMessageId);
    if (it == m_aPendingMessages.end())
    {
        SAL_WARN("sw.ai", "Cannot report failure for unknown message: " << rsMessageId);
        return;
    }
    
    QueuedMessage& rMessage = it->second;
    
    // Check if we should retry
    if (rMessage.nAttemptCount < rMessage.nMaxRetries)
    {
        // Re-queue for retry
        rMessage.eStatus = MessageStatus::QUEUED;
        rMessage.aLastAttempt = std::chrono::steady_clock::now() + 
                               std::chrono::milliseconds(m_nRetryDelayMs * rMessage.nAttemptCount);
        
        m_aOutboundQueue.push(rMessage);
        m_aPendingMessages.erase(it);
        
        logOperation("RETRY", rMessage);
        SAL_INFO("sw.ai", "Message " << rsMessageId << " queued for retry (attempt " << 
                 rMessage.nAttemptCount + 1 << "/" << rMessage.nMaxRetries << ")");
    }
    else
    {
        // Move to failed state
        rMessage.eStatus = MessageStatus::FAILED;
        rMessage.aMetadata["last_error"] = rsError;
        
        m_aFailedMessages[rsMessageId] = rMessage;
        m_aPendingMessages.erase(it);
        
        updateStatistics(rMessage, MessageStatus::FAILED);
        logOperation("FAIL", rMessage);
        
        // Call error callback
        if (m_aErrorCallback)
        {
            m_aErrorCallback(rMessage, rsError);
        }
        
        SAL_WARN("sw.ai", "Message " << rsMessageId << " failed permanently: " << rsError);
    }
}

bool MessageQueue::cancelMessage(const OUString& rsMessageId)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Check pending messages
    auto pendingIt = m_aPendingMessages.find(rsMessageId);
    if (pendingIt != m_aPendingMessages.end())
    {
        m_aPendingMessages.erase(pendingIt);
        logOperation("CANCEL", pendingIt->second);
        return true;
    }
    
    // Cannot cancel messages in priority queue efficiently
    // They will be filtered out when retrieved
    
    return false;
}

MessageQueue::MessageStatus MessageQueue::getMessageStatus(const OUString& rsMessageId)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Check pending messages
    auto it = m_aPendingMessages.find(rsMessageId);
    if (it != m_aPendingMessages.end())
    {
        return it->second.eStatus;
    }
    
    // Check failed messages
    auto failedIt = m_aFailedMessages.find(rsMessageId);
    if (failedIt != m_aFailedMessages.end())
    {
        return failedIt->second.eStatus;
    }
    
    return MessageStatus::QUEUED; // Assume queued if not found elsewhere
}

std::unique_ptr<MessageQueue::QueuedMessage> MessageQueue::getMessage(const OUString& rsMessageId)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    auto it = m_aPendingMessages.find(rsMessageId);
    if (it != m_aPendingMessages.end())
    {
        return std::make_unique<QueuedMessage>(it->second);
    }
    
    auto failedIt = m_aFailedMessages.find(rsMessageId);
    if (failedIt != m_aFailedMessages.end())
    {
        return std::make_unique<QueuedMessage>(failedIt->second);
    }
    
    return nullptr;
}

sal_Int32 MessageQueue::getQueueSize() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return static_cast<sal_Int32>(m_aOutboundQueue.size());
}

sal_Int32 MessageQueue::getPendingCount() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return static_cast<sal_Int32>(m_aPendingMessages.size());
}

sal_Int32 MessageQueue::getFailedCount() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return static_cast<sal_Int32>(m_aFailedMessages.size());
}

void MessageQueue::clearQueue()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    while (!m_aOutboundQueue.empty())
        m_aOutboundQueue.pop();
    
    std::queue<QueuedMessage> emptyInbound;
    m_aInboundQueue.swap(emptyInbound);
    
    SAL_INFO("sw.ai", "Message queue cleared");
}

void MessageQueue::clearFailedMessages()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    m_aFailedMessages.clear();
    SAL_INFO("sw.ai", "Failed messages cleared");
}

sal_Int32 MessageQueue::retryFailedMessages()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    sal_Int32 nRetried = 0;
    
    for (auto& [sId, rMessage] : m_aFailedMessages)
    {
        // Reset message for retry
        rMessage.eStatus = MessageStatus::QUEUED;
        rMessage.nAttemptCount = 0;
        rMessage.aLastAttempt = std::chrono::steady_clock::time_point::min();
        
        m_aOutboundQueue.push(rMessage);
        ++nRetried;
    }
    
    m_aFailedMessages.clear();
    
    SAL_INFO("sw.ai", "Retried " << nRetried << " failed messages");
    return nRetried;
}

sal_Int32 MessageQueue::purgeExpiredMessages()
{
    sal_Int32 nPurged = 0;
    auto now = std::chrono::steady_clock::now();
    
    // Cannot efficiently purge from priority queue
    // Expired messages are filtered during retrieval
    
    // Purge expired pending messages
    auto it = m_aPendingMessages.begin();
    while (it != m_aPendingMessages.end())
    {
        if (it->second.aExpiry <= now)
        {
            updateStatistics(it->second, MessageStatus::EXPIRED);
            logOperation("PURGE", it->second);
            it = m_aPendingMessages.erase(it);
            ++nPurged;
        }
        else
        {
            ++it;
        }
    }
    
    // Purge expired failed messages
    auto failedIt = m_aFailedMessages.begin();
    while (failedIt != m_aFailedMessages.end())
    {
        if (failedIt->second.aExpiry <= now)
        {
            failedIt = m_aFailedMessages.erase(failedIt);
            ++nPurged;
        }
        else
        {
            ++failedIt;
        }
    }
    
    if (nPurged > 0)
    {
        SAL_INFO("sw.ai", "Purged " << nPurged << " expired messages");
    }
    
    return nPurged;
}

bool MessageQueue::saveQueue()
{
    if (m_sPersistenceFile.isEmpty())
        return false;
    
    // Implementation would serialize queue state to file
    // For now, just log the operation
    SAL_INFO("sw.ai", "Queue saved to: " << m_sPersistenceFile);
    return true;
}

bool MessageQueue::loadQueue()
{
    if (m_sPersistenceFile.isEmpty())
        return false;
    
    // Implementation would deserialize queue state from file
    // For now, just log the operation
    SAL_INFO("sw.ai", "Queue loaded from: " << m_sPersistenceFile);
    return true;
}

OUString MessageQueue::generateMessageId()
{
    sal_uInt8 aBuffer[16];
    rtl_createUuid(aBuffer, nullptr, false);
    
    OUStringBuffer aBuf(36);
    for (int i = 0; i < 16; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            aBuf.append('-');
        aBuf.append(static_cast<sal_Unicode>('0' + (aBuffer[i] >> 4)));
        aBuf.append(static_cast<sal_Unicode>('0' + (aBuffer[i] & 0x0F)));
    }
    
    return aBuf.makeStringAndClear();
}

bool MessageQueue::compareMessagePriority(const QueuedMessage& a, const QueuedMessage& b)
{
    // Higher priority values have higher precedence
    if (a.ePriority != b.ePriority)
        return static_cast<int>(a.ePriority) < static_cast<int>(b.ePriority);
    
    // Earlier timestamps have higher precedence
    return a.aCreated > b.aCreated;
}

bool MessageQueue::isMessageExpired(const QueuedMessage& rMessage) const
{
    return rMessage.aExpiry <= std::chrono::steady_clock::now();
}

bool MessageQueue::checkRateLimit()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_aLastSend).count();
    
    if (elapsed >= 1000) // Reset burst counter every second
    {
        m_nCurrentBurst = 0;
        m_aLastSend = now;
    }
    
    if (m_nCurrentBurst >= m_nMaxMessagesPerSecond)
        return false;
    
    ++m_nCurrentBurst;
    return true;
}

OUString MessageQueue::compressContent(const OUString& rsContent) const
{
    // Placeholder for compression implementation
    // Would use zlib or similar compression library
    return rsContent;
}

OUString MessageQueue::decompressContent(const OUString& rsContent) const
{
    // Placeholder for decompression implementation
    return rsContent;
}

bool MessageQueue::validateMessage(const QueuedMessage& rMessage) const
{
    if (rMessage.sMessageId.isEmpty() || rMessage.sRequestId.isEmpty())
        return false;
    
    if (rMessage.sContent.getLength() > m_nMaxMessageSize)
        return false;
    
    if (rMessage.aExpiry <= rMessage.aCreated)
        return false;
    
    return true;
}

void MessageQueue::updateStatistics(const QueuedMessage& /* rMessage */, MessageStatus eNewStatus)
{
    switch (eNewStatus)
    {
        case MessageStatus::QUEUED:
            ++m_aStatistics.nTotalEnqueued;
            ++m_aStatistics.nCurrentQueueSize;
            break;
        case MessageStatus::SENT:
            ++m_aStatistics.nTotalDelivered;
            break;
        case MessageStatus::ACKNOWLEDGED:
            ++m_aStatistics.nTotalAcknowledged;
            --m_aStatistics.nCurrentQueueSize;
            break;
        case MessageStatus::FAILED:
            ++m_aStatistics.nTotalFailed;
            --m_aStatistics.nCurrentQueueSize;
            break;
        case MessageStatus::EXPIRED:
            ++m_aStatistics.nTotalExpired;
            --m_aStatistics.nCurrentQueueSize;
            break;
    }
}

void MessageQueue::logOperation(const OUString& rsOperation, const QueuedMessage& rMessage) const
{
    SAL_INFO("sw.ai", rsOperation << " - Message: " << rMessage.sMessageId << 
             ", Service: " << rMessage.sServiceName << 
             ", Priority: " << static_cast<int>(rMessage.ePriority) <<
             ", Attempts: " << rMessage.nAttemptCount);
}

void MessageQueue::parseConfiguration(const Sequence<PropertyValue>& rConfig)
{
    for (const auto& rProperty : rConfig)
    {
        if (rProperty.Name == "MaxQueueSize")
        {
            rProperty.Value >>= m_nMaxQueueSize;
        }
        else if (rProperty.Name == "MaxMessageSize")
        {
            rProperty.Value >>= m_nMaxMessageSize;
        }
        else if (rProperty.Name == "DefaultTTLSeconds")
        {
            rProperty.Value >>= m_nDefaultTTLSeconds;
        }
        else if (rProperty.Name == "DeliveryTimeoutMs")
        {
            rProperty.Value >>= m_nDeliveryTimeoutMs;
        }
        else if (rProperty.Name == "RetryDelayMs")
        {
            rProperty.Value >>= m_nRetryDelayMs;
        }
        else if (rProperty.Name == "EnableCompression")
        {
            rProperty.Value >>= m_bEnableCompression;
        }
        else if (rProperty.Name == "PersistenceFile")
        {
            rProperty.Value >>= m_sPersistenceFile;
        }
        else if (rProperty.Name == "EnablePersistence")
        {
            bool bValue = false;
            if (rProperty.Value >>= bValue)
                m_bPersistenceEnabled.store(bValue);
        }
        else if (rProperty.Name == "MaxMessagesPerSecond")
        {
            rProperty.Value >>= m_nMaxMessagesPerSecond;
        }
    }
}

MessageQueue::QueueStatisticsData MessageQueue::getStatistics() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    return m_aStatistics.getData();
}

void MessageQueue::resetStatistics()
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_aStatistics.reset();
}

bool MessageQueue::isHealthy() const
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    // Check if queue is not full
    if (static_cast<sal_Int32>(m_aOutboundQueue.size()) >= m_nMaxQueueSize)
        return false;
    
    // Check if too many messages are failing
    auto stats = m_aStatistics.getData();
    if (stats.nTotalEnqueued > 0)
    {
        double fFailureRate = static_cast<double>(stats.nTotalFailed) / stats.nTotalEnqueued;
        if (fFailureRate > 0.5) // More than 50% failure rate
            return false;
    }
    
    return m_bRunning;
}

bool MessageQueue::isFull() const
{
    return static_cast<sal_Int32>(m_aOutboundQueue.size()) >= m_nMaxQueueSize;
}

bool MessageQueue::isEmpty() const
{
    return m_aOutboundQueue.empty();
}

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */