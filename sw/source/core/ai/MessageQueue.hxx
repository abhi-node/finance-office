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
#include <queue>
#include <chrono>
#include <functional>
#include <mutex>
#include <atomic>
#include <map>

namespace sw::ai {

/**
 * MessageQueue - Reliable message queuing system for AI backend communication
 * 
 * This class provides persistent message queuing capabilities for the AI Agent system,
 * ensuring reliable delivery of messages even during network outages or service
 * interruptions. Supports both in-memory and disk-based persistence.
 * 
 * Design Principles:
 * - Reliability: Messages are persisted until acknowledged
 * - Performance: In-memory queuing with optional disk persistence
 * - Recovery: Automatic queue restoration on startup
 * - Ordering: FIFO message delivery with priority support
 * - Throttling: Rate limiting and congestion control
 */
class MessageQueue
{
public:
    enum class MessagePriority
    {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        CRITICAL = 3
    };

    enum class MessageStatus
    {
        QUEUED,          // Waiting in queue
        SENT,            // Transmitted to backend
        ACKNOWLEDGED,    // Confirmed by backend
        FAILED,          // Failed after max retries
        EXPIRED          // Expired due to TTL
    };

    enum class DeliveryMode
    {
        AT_MOST_ONCE,    // Fire and forget (no guarantees)
        AT_LEAST_ONCE,   // Guaranteed delivery (may duplicate)
        EXACTLY_ONCE     // Guaranteed delivery exactly once
    };

    struct QueuedMessage
    {
        OUString sMessageId;
        OUString sRequestId;
        OUString sServiceName;       // "http", "websocket", "langgraph"
        OUString sContent;
        OUString sContentType;       // "application/json", "text/plain"
        MessagePriority ePriority;
        MessageStatus eStatus;
        DeliveryMode eDeliveryMode;
        
        std::chrono::steady_clock::time_point aCreated;
        std::chrono::steady_clock::time_point aExpiry;
        std::chrono::steady_clock::time_point aLastAttempt;
        
        sal_Int32 nAttemptCount;
        sal_Int32 nMaxRetries;
        std::map<OUString, OUString> aHeaders;
        std::map<OUString, OUString> aMetadata;
        
        QueuedMessage(const OUString& rsId, const OUString& rsReqId, const OUString& rsService, 
                     const OUString& rsContent, MessagePriority ePrio = MessagePriority::NORMAL)
            : sMessageId(rsId), sRequestId(rsReqId), sServiceName(rsService), sContent(rsContent)
            , sContentType("application/json"), ePriority(ePrio), eStatus(MessageStatus::QUEUED)
            , eDeliveryMode(DeliveryMode::AT_LEAST_ONCE)
            , aCreated(std::chrono::steady_clock::now())
            , aExpiry(std::chrono::steady_clock::now() + std::chrono::hours(24))
            , aLastAttempt(std::chrono::steady_clock::time_point::min())
            , nAttemptCount(0), nMaxRetries(3) {}
    };

    // Callback function types
    using DeliveryCallback = std::function<bool(const QueuedMessage&)>;
    using AcknowledgmentCallback = std::function<void(const OUString&, bool)>;
    using ErrorCallback = std::function<void(const QueuedMessage&, const OUString&)>;

private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
    
    // Thread safety
    mutable std::mutex m_aMutex;
    std::atomic<bool> m_bRunning;
    std::atomic<bool> m_bPersistenceEnabled;
    
    // Message storage
    std::priority_queue<QueuedMessage, std::vector<QueuedMessage>, 
                       std::function<bool(const QueuedMessage&, const QueuedMessage&)>> m_aOutboundQueue;
    std::queue<QueuedMessage> m_aInboundQueue;
    std::map<OUString, QueuedMessage> m_aPendingMessages;    // Message ID -> Message
    std::map<OUString, QueuedMessage> m_aFailedMessages;     // Message ID -> Message
    
    // Configuration
    sal_Int32 m_nMaxQueueSize;
    sal_Int32 m_nMaxMessageSize;
    sal_Int32 m_nDefaultTTLSeconds;
    sal_Int32 m_nDeliveryTimeoutMs;
    sal_Int32 m_nRetryDelayMs;
    bool m_bEnableCompression;
    OUString m_sPersistenceFile;
    
    // Rate limiting
    sal_Int32 m_nMaxMessagesPerSecond;
    std::chrono::steady_clock::time_point m_aLastSend;
    sal_Int32 m_nCurrentBurst;
    
    // Callbacks
    DeliveryCallback m_aDeliveryCallback;
    AcknowledgmentCallback m_aAckCallback;
    ErrorCallback m_aErrorCallback;
    
    // Statistics
    struct QueueStatistics
    {
        std::atomic<sal_Int32> nTotalEnqueued{0};
        std::atomic<sal_Int32> nTotalDelivered{0};
        std::atomic<sal_Int32> nTotalAcknowledged{0};
        std::atomic<sal_Int32> nTotalFailed{0};
        std::atomic<sal_Int32> nTotalExpired{0};
        std::atomic<sal_Int32> nCurrentQueueSize{0};
        std::chrono::steady_clock::time_point aLastReset;
        
        QueueStatistics() : aLastReset(std::chrono::steady_clock::now()) {}
    };
    
    QueueStatistics m_aStatistics;

public:
    explicit MessageQueue(const css::uno::Reference<css::uno::XComponentContext>& xContext);
    ~MessageQueue();

    // Lifecycle management
    
    /**
     * Initialize message queue with configuration
     */
    bool initialize(const css::uno::Sequence<css::beans::PropertyValue>& rConfig = {});
    
    /**
     * Start message processing
     */
    bool start();
    
    /**
     * Stop message processing and cleanup
     */
    void stop();
    
    /**
     * Shutdown message queue
     */
    void shutdown();

    // Message operations
    
    /**
     * Enqueue message for delivery
     */
    bool enqueueMessage(const OUString& rsRequestId, const OUString& rsServiceName,
                       const OUString& rsContent, MessagePriority ePriority = MessagePriority::NORMAL,
                       const std::map<OUString, OUString>& aHeaders = {},
                       const std::map<OUString, OUString>& aMetadata = {});
    
    /**
     * Enqueue message with custom settings
     */
    bool enqueueMessage(const QueuedMessage& rMessage);
    
    /**
     * Get next message for delivery
     */
    std::unique_ptr<QueuedMessage> getNextMessage();
    
    /**
     * Acknowledge successful message delivery
     */
    void acknowledgeMessage(const OUString& rsMessageId, bool bSuccess = true);
    
    /**
     * Report message delivery failure
     */
    void reportFailure(const OUString& rsMessageId, const OUString& rsError);
    
    /**
     * Cancel pending message
     */
    bool cancelMessage(const OUString& rsMessageId);
    
    /**
     * Get message status
     */
    MessageStatus getMessageStatus(const OUString& rsMessageId);
    
    /**
     * Get pending message by ID
     */
    std::unique_ptr<QueuedMessage> getMessage(const OUString& rsMessageId);

    // Queue management
    
    /**
     * Get current queue size
     */
    sal_Int32 getQueueSize() const;
    
    /**
     * Get pending messages count
     */
    sal_Int32 getPendingCount() const;
    
    /**
     * Get failed messages count
     */
    sal_Int32 getFailedCount() const;
    
    /**
     * Clear all queued messages
     */
    void clearQueue();
    
    /**
     * Clear failed messages
     */
    void clearFailedMessages();
    
    /**
     * Retry failed messages
     */
    sal_Int32 retryFailedMessages();
    
    /**
     * Purge expired messages
     */
    sal_Int32 purgeExpiredMessages();

    // Persistence
    
    /**
     * Enable/disable disk persistence
     */
    void setPersistenceEnabled(bool bEnabled);
    
    /**
     * Set persistence file path
     */
    void setPersistenceFile(const OUString& rsFilePath);
    
    /**
     * Save queue to disk
     */
    bool saveQueue();
    
    /**
     * Load queue from disk
     */
    bool loadQueue();

    // Configuration
    
    /**
     * Set maximum queue size
     */
    void setMaxQueueSize(sal_Int32 nMaxSize);
    
    /**
     * Set message TTL in seconds
     */
    void setDefaultTTL(sal_Int32 nTTLSeconds);
    
    /**
     * Set rate limiting
     */
    void setRateLimit(sal_Int32 nMessagesPerSecond);
    
    /**
     * Enable/disable message compression
     */
    void setCompressionEnabled(bool bEnabled);

    // Callbacks
    
    /**
     * Set delivery callback
     */
    void setDeliveryCallback(const DeliveryCallback& aCallback);
    
    /**
     * Set acknowledgment callback
     */
    void setAcknowledgmentCallback(const AcknowledgmentCallback& aCallback);
    
    /**
     * Set error callback
     */
    void setErrorCallback(const ErrorCallback& aCallback);

    // Statistics and monitoring
    
    /**
     * Get queue statistics
     */
    QueueStatistics getStatistics() const;
    
    /**
     * Reset statistics
     */
    void resetStatistics();
    
    /**
     * Get queue health status
     */
    bool isHealthy() const;

    // Utility methods
    
    /**
     * Generate unique message ID
     */
    static OUString generateMessageId();
    
    /**
     * Check if queue is full
     */
    bool isFull() const;
    
    /**
     * Check if queue is empty
     */
    bool isEmpty() const;

private:
    // Internal implementation methods
    
    /**
     * Priority comparison function for message queue
     */
    static bool compareMessagePriority(const QueuedMessage& a, const QueuedMessage& b);
    
    /**
     * Check if message has expired
     */
    bool isMessageExpired(const QueuedMessage& rMessage) const;
    
    /**
     * Apply rate limiting
     */
    bool checkRateLimit();
    
    /**
     * Compress message content
     */
    OUString compressContent(const OUString& rsContent) const;
    
    /**
     * Decompress message content
     */
    OUString decompressContent(const OUString& rsContent) const;
    
    /**
     * Serialize message to JSON
     */
    OUString serializeMessage(const QueuedMessage& rMessage) const;
    
    /**
     * Deserialize message from JSON
     */
    std::unique_ptr<QueuedMessage> deserializeMessage(const OUString& rsJson) const;
    
    /**
     * Parse configuration parameters
     */
    void parseConfiguration(const css::uno::Sequence<css::beans::PropertyValue>& rConfig);
    
    /**
     * Validate message content
     */
    bool validateMessage(const QueuedMessage& rMessage) const;
    
    /**
     * Update message statistics
     */
    void updateStatistics(const QueuedMessage& rMessage, MessageStatus eNewStatus);
    
    /**
     * Log queue operation
     */
    void logOperation(const OUString& rsOperation, const QueuedMessage& rMessage) const;
};

} // namespace sw::ai

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */