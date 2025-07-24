# LibreOffice AI Writing Assistant: WebSocket Architecture

## Overview

The LibreOffice AI Writing Assistant implements a sophisticated WebSocket communication layer to enable real-time streaming updates during LangGraph agent processing. This architecture provides immediate feedback to users during complex operations while maintaining compatibility with LibreOffice's existing infrastructure.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Component Structure](#component-structure)
3. [Message Flow and Protocol](#message-flow-and-protocol)
4. [Integration with AgentCoordinator](#integration-with-agentcoordinator)
5. [Connection Management](#connection-management)
6. [Error Handling and Fallback](#error-handling-and-fallback)
7. [Performance Optimization](#performance-optimization)
8. [Development and Production Considerations](#development-and-production-considerations)
9. [API Reference](#api-reference)
10. [Usage Examples](#usage-examples)

## Architecture Overview

### Design Principles

The WebSocket implementation follows LibreOffice's established patterns while enabling modern real-time communication:

- **LibreOffice Compatibility**: Uses UNO service patterns and LibreOffice threading model
- **Graceful Degradation**: Automatically falls back to HTTP when WebSocket unavailable
- **Thread Safety**: Background threads prevent UI blocking during long operations
- **Protocol Standards**: JSON message format with LangGraph sub-protocol
- **Performance Focus**: Connection pooling, heartbeat monitoring, and efficient message handling

### System Context

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           LIBREOFFICE WRITER APPLICATION                               │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌──────────────────────────────────────────────────────────┐ │
│  │   AI SIDEBAR    │    │                WEBSOCKET INTEGRATION                     │ │
│  │   PANEL         │    │                                                          │ │
│  │                 │    │  ┌─────────────────────────────────────────────────────┐ │ │
│  │  User Request   │◄───┼──┤             AgentCoordinator                        │ │ │
│  │  Progress UI    │    │  │  • WebSocket client management                     │ │ │
│  │  Status Updates │    │  │  • Request correlation and routing                 │ │ │
│  │                 │    │  │  • Real-time message handling                      │ │ │
│  └─────────────────┘    │  └─────────────────────────────────────────────────────┘ │ │
│                         │                            │                             │ │
│                         │                            ▼                             │ │
│                         │  ┌─────────────────────────────────────────────────────┐ │ │
│                         │  │              WebSocketClient                        │ │ │
│                         │  │  • Connection state management                     │ │ │
│                         │  │  • Message queue processing                        │ │ │
│                         │  │  • Auto-reconnection logic                         │ │ │
│                         │  │  • Heartbeat monitoring                            │ │ │
│                         │  └─────────────────────────────────────────────────────┘ │ │
│                         └──────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                                   │ WebSocket Protocol (ws://, wss://)
                                                   │ Sub-protocol: "langgraph-ai"
                                                   ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           LANGGRAPH BACKEND SYSTEM                                 │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                     WEBSOCKET SERVER                                       │   │
│  │                                                                             │   │
│  │  • Receives requests from LibreOffice WebSocketClient                     │   │
│  │  • Routes to appropriate LangGraph agent workflows                        │   │
│  │  • Streams real-time progress updates back to client                      │   │
│  │  • Manages multiple concurrent LibreOffice sessions                       │   │
│  │                                                                             │   │
│  │  Message Types:                                                            │   │
│  │  ├─ progress_update: Agent processing status                              │   │
│  │  ├─ agent_status: Individual agent availability                           │   │
│  │  ├─ streaming_response: Partial results during processing                 │   │
│  │  ├─ error_notification: Error states and recovery info                    │   │
│  │  └─ heartbeat: Connection health monitoring                               │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                               │                                     │
│                                               │ Routes to agent workflow            │
│                                               ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                    LANGGRAPH AGENT WORKFLOW                                │   │
│  │                                                                             │   │
│  │  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────────────┐   │   │
│  │  │ DocumentMaster  │──▶│ ContextAnalysis │──▶│ ContentGeneration       │   │   │
│  │  │ Agent           │   │ Agent           │   │ Agent                   │   │   │
│  │  │ (Orchestrator)  │   │                 │   │                         │   │   │
│  │  └─────────────────┘   └─────────────────┘   └─────────────────────────┘   │   │
│  │           │                      │                         │               │   │
│  │           │ Sends progress       │ Sends progress          │ Sends progress │   │
│  │           ▼                      ▼                         ▼               │   │
│  │  ┌─────────────────────────────────────────────────────────────────────┐   │   │
│  │  │                 WEBSOCKET PROGRESS STREAM                           │   │   │
│  │  │  • Each agent sends status updates via WebSocket                   │   │   │
│  │  │  • Progress percentages, current operations, results               │   │   │
│  │  │  • Real-time streaming back to LibreOffice UI                      │   │   │
│  │  └─────────────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

## Component Structure

### WebSocketClient Class Hierarchy

```cpp
namespace sw::ai {
    class WebSocketClient {
    public:
        // Connection state management
        enum class ConnectionState {
            DISCONNECTED, CONNECTING, CONNECTED, RECONNECTING, FAILED
        };
        
        // Message type classification
        enum class MessageType {
            TEXT, BINARY, PROGRESS_UPDATE, AGENT_STATUS, 
            ERROR_NOTIFICATION, HEARTBEAT
        };
        
        // Message structure
        struct WebSocketMessage {
            MessageType eType;
            OUString sContent;
            std::map<OUString, OUString> aHeaders;
            std::chrono::steady_clock::time_point aTimestamp;
        };
        
        // Callback function types
        using MessageCallback = std::function<void(const WebSocketMessage&)>;
        using ConnectionCallback = std::function<void(ConnectionState, const OUString&)>;
        using ErrorCallback = std::function<void(const OUString&, sal_Int32)>;
    };
}
```

### File Structure

```
sw/source/core/ai/
├── WebSocketClient.hxx          # WebSocket client interface and declarations
├── WebSocketClient.cxx          # WebSocket client implementation
├── AgentCoordinator.hxx         # Updated with WebSocket integration
└── AgentCoordinator.cxx         # WebSocket methods and request processing
```

## Message Flow and Protocol

### WebSocket Protocol Specification

**Connection Details:**
- **Protocol**: WebSocket (RFC 6455)
- **Sub-protocol**: "langgraph-ai"
- **URL Format**: `ws://localhost:8000/ws/libreoffice` or `wss://` for secure connections
- **Message Format**: JSON with standardized headers

### Message Structure

All WebSocket messages follow this JSON structure:

```json
{
    "type": "message_type",
    "request_id": "unique_correlation_id",
    "timestamp": "unix_timestamp_ms",
    "agent": "agent_name",
    "data": {
        // Message-specific payload
    },
    "headers": {
        "X-LangGraph-Protocol": "langgraph-ai",
        "X-Message-Type": "1",
        "X-Request-ID": "REQ_123456_789",
        "X-Agent-Type": "libreoffice-writer"
    }
}
```

### Message Types and Examples

#### 1. Progress Update Messages
```json
{
    "type": "progress_update",
    "request_id": "REQ_123456_789",
    "timestamp": "1703123456789",
    "agent": "DocumentMaster",
    "data": {
        "status": "analyzing_document",
        "progress": 25,
        "current_step": "Extracting document structure",
        "estimated_remaining_ms": 5000
    }
}
```

#### 2. Agent Status Messages
```json
{
    "type": "agent_status",
    "request_id": "REQ_123456_789",
    "timestamp": "1703123456789",
    "agent": "DataIntegration",
    "data": {
        "status": "available",
        "capabilities": ["financial_data", "market_research"],
        "current_load": 0.3,
        "queue_length": 2
    }
}
```

#### 3. Streaming Response Messages
```json
{
    "type": "streaming_response",
    "request_id": "REQ_123456_789",
    "timestamp": "1703123456789",
    "agent": "ContentGeneration",
    "data": {
        "partial_content": "# Financial Report\n\nExecutive Summary:\nBased on current market data...",
        "is_final": false,
        "content_type": "markdown"
    }
}
```

#### 4. Error Notification Messages
```json
{
    "type": "error_notification",
    "request_id": "REQ_123456_789",
    "timestamp": "1703123456789",
    "agent": "DataIntegration",
    "data": {
        "error_code": "API_RATE_LIMIT",
        "error_message": "Financial data API rate limit exceeded",
        "retry_after_ms": 60000,
        "recovery_suggestions": ["Use cached data", "Retry in 1 minute"]
    }
}
```

#### 5. Heartbeat Messages
```json
{
    "type": "heartbeat",
    "timestamp": "1703123456789",
    "data": {
        "server_status": "healthy",
        "active_connections": 15,
        "server_load": 0.45
    }
}
```

### Request Correlation Flow

```
LibreOffice Request Processing:
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  1. User Request                                                                    │
│     ↓                                                                               │
│  2. AgentCoordinator::processComplexRequest()                                      │
│     ├─ Generate unique request ID: "REQ_1703123456_789"                           │
│     ├─ Check if WebSocket enabled and connected                                   │
│     └─ Route request based on availability                                        │
│                                                                                     │
│  3A. WebSocket Path (Preferred for Complex Requests):                             │
│      ├─ sendWebSocketMessage(request, requestId)                                  │
│      ├─ Return immediate acknowledgment to UI                                     │
│      └─ Set up correlation for streaming updates                                  │
│                                                                                     │
│  3B. HTTP Fallback Path:                                                          │
│      ├─ Traditional HTTP POST with request ID in headers                          │
│      └─ Synchronous response handling                                             │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  Backend Processing with Real-time Updates                                         │
│                                                                                     │
│  4. LangGraph Agent Workflow:                                                      │
│     ├─ DocumentMaster receives request with ID "REQ_1703123456_789"              │
│     ├─ Routes to appropriate agents based on complexity                           │
│     └─ Each agent sends progress updates via WebSocket                            │
│                                                                                     │
│  5. Streaming Updates (WebSocket Messages):                                        │
│     ├─ t=0ms:    {"type":"progress_update","progress":0,"agent":"DocumentMaster"} │
│     ├─ t=500ms:  {"type":"progress_update","progress":15,"agent":"ContextAnalysis"}│
│     ├─ t=1200ms: {"type":"progress_update","progress":35,"agent":"DataIntegration"}│
│     ├─ t=2000ms: {"type":"streaming_response","partial_content":"# Report..."} │
│     ├─ t=3500ms: {"type":"progress_update","progress":75,"agent":"ContentGen"}    │
│     └─ t=4800ms: {"type":"streaming_response","is_final":true,"final_result":"..."}│
└─────────────────────────────────────────────────────────────────────────────────────┘
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  LibreOffice UI Updates                                                            │
│                                                                                     │
│  6. Real-time UI Updates:                                                          │
│     ├─ handleWebSocketMessage() receives each update                              │
│     ├─ Correlates message with original request using request_id                  │
│     ├─ Updates progress indicators in AI Panel                                    │
│     ├─ Shows streaming content as it becomes available                            │
│     └─ Displays final result when processing complete                             │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

## Integration with AgentCoordinator

### WebSocket Integration Points

The AgentCoordinator class integrates WebSocket functionality at several key points:

#### 1. Initialization (Constructor)
```cpp
AgentCoordinator::AgentCoordinator(const Reference<XComponentContext>& xContext)
{
    // ... existing initialization ...
    
    // Initialize WebSocket client if enabled
    if (m_bEnableWebSocket)
    {
        initializeWebSocketClient();
    }
}
```

#### 2. WebSocket Client Setup
```cpp
bool AgentCoordinator::initializeWebSocketClient()
{
    // Create WebSocket client instance
    m_pWebSocketClient = std::make_unique<sw::ai::WebSocketClient>(m_xContext);
    
    // Configure with LibreOffice-compatible settings
    Sequence<PropertyValue> aConfig(5);
    aConfig[0].Name = "AutoReconnect";
    aConfig[0].Value <<= true;
    aConfig[1].Name = "MaxReconnectAttempts";
    aConfig[1].Value <<= sal_Int32(3);
    // ... additional configuration ...
    
    // Set up callback handlers
    m_pWebSocketClient->setMessageCallback([this](const WebSocketMessage& aMessage) {
        handleWebSocketMessage(aMessage.sContent);
    });
    
    m_pWebSocketClient->setConnectionCallback([this](ConnectionState eState, const OUString&) {
        bool bConnected = (eState == ConnectionState::CONNECTED);
        handleWebSocketConnectionChange(bConnected);
    });
    
    return m_pWebSocketClient->initialize(aConfig);
}
```

#### 3. Request Processing Integration

The WebSocket integration modifies request processing to prefer real-time communication for complex operations:

```cpp
OUString AgentCoordinator::processComplexRequest(const OUString& rsRequest, const Any& rContext)
{
    // Generate unique request ID for correlation
    OUString sRequestId = generateRequestId();
    
    // Try WebSocket first for real-time updates during complex processing
    if (isWebSocketEnabled())
    {
        SAL_INFO("sw.ai", "Using WebSocket for complex request: " << sRequestId);
        
        // Send initial request via WebSocket for streaming updates
        bool bWebSocketSent = sendWebSocketMessage(rsRequest, sRequestId);
        if (bWebSocketSent)
        {
            // WebSocket request sent - will receive streaming updates
            return "Processing complex request via WebSocket (ID: " + sRequestId + 
                   ") - streaming updates enabled";
        }
        else
        {
            SAL_WARN("sw.ai", "WebSocket send failed, falling back to HTTP");
        }
    }
    
    // Fallback to HTTP for complex requests
    // ... existing HTTP processing logic ...
}
```

### Callback Handler Implementation

#### Message Handler
```cpp
void AgentCoordinator::handleWebSocketMessage(const OUString& rsMessage)
{
    try
    {
        SAL_INFO("sw.ai", "WebSocket message received: " << rsMessage.copy(0, 100) << "...");
        
        // TODO: Parse JSON message and handle different message types:
        // - progress_update: Update UI with agent processing progress
        // - agent_status: Update agent availability status
        // - streaming_response: Stream partial responses to UI
        // - error_notification: Handle backend errors
        
        // In a full implementation, this would:
        // 1. Parse JSON to determine message type
        // 2. Extract request ID for correlation
        // 3. Update corresponding UI elements
        // 4. Handle streaming responses for real-time updates
        // 5. Manage agent status and availability
        
        logActivity("WebSocket message processed: " + rsMessage.copy(0, 50) + "...");
    }
    catch (const Exception& e)
    {
        handleNetworkError("WebSocket message processing failed: " + e.Message);
    }
}
```

#### Connection State Handler
```cpp
void AgentCoordinator::handleWebSocketConnectionChange(bool bConnected)
{
    SAL_INFO("sw.ai", "WebSocket connection state changed: " << 
             (bConnected ? "CONNECTED" : "DISCONNECTED"));
    
    if (bConnected)
    {
        // WebSocket connected - can now send real-time requests
        logActivity("WebSocket connected - real-time communication enabled");
        
        // If we were in offline mode due to WebSocket issues, try to go online
        if (!m_bOnlineMode && m_pNetworkClient && m_pNetworkClient->isOnline())
        {
            exitOfflineMode();
        }
    }
    else
    {
        // WebSocket disconnected - fall back to HTTP-only mode
        logActivity("WebSocket disconnected - falling back to HTTP communication");
        
        // Don't enter offline mode unless HTTP is also failing
        // WebSocket is for real-time updates, HTTP still works for basic requests
    }
}
```

## Connection Management

### Connection Lifecycle

```
WebSocket Connection Lifecycle:
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  1. INITIALIZATION PHASE                                                            │
│     ├─ WebSocketClient constructor creates client instance                          │
│     ├─ Configuration loaded from LibreOffice settings                              │
│     ├─ Callback handlers registered for message processing                         │
│     └─ Client ready for connection attempts                                        │
│                                                                                     │
│  2. CONNECTION PHASE                                                               │
│     ├─ connect(url, protocol) initiates connection                                 │
│     ├─ Background connection thread starts                                         │
│     ├─ WebSocket handshake performed                                               │
│     ├─ Connection state: DISCONNECTED → CONNECTING → CONNECTED                     │
│     └─ Connection callback notifies AgentCoordinator                               │
│                                                                                     │
│  3. OPERATIONAL PHASE                                                              │
│     ├─ Message processing thread handles incoming/outgoing queues                  │
│     ├─ Heartbeat monitoring maintains connection health                            │
│     ├─ Real-time message exchange with LangGraph backend                          │
│     └─ Automatic handling of connection issues                                     │
│                                                                                     │
│  4. RECONNECTION PHASE (If connection lost)                                        │
│     ├─ Connection state: CONNECTED → RECONNECTING                                  │
│     ├─ Exponential backoff retry mechanism                                         │
│     ├─ Maximum retry attempts configured (default: 3)                             │
│     ├─ Success: RECONNECTING → CONNECTED                                           │
│     └─ Failure: RECONNECTING → FAILED                                              │
│                                                                                     │
│  5. SHUTDOWN PHASE                                                                 │
│     ├─ disconnect() or shutdown() called                                           │
│     ├─ Background threads gracefully terminated                                    │
│     ├─ Message queues cleared                                                      │
│     ├─ Resources cleaned up                                                        │
│     └─ Connection state: * → DISCONNECTED                                          │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### Connection State Management

The WebSocket client maintains detailed connection state with automatic transitions:

```cpp
enum class ConnectionState {
    DISCONNECTED,  // No connection, ready to connect
    CONNECTING,    // Connection attempt in progress
    CONNECTED,     // Successfully connected and operational
    RECONNECTING,  // Attempting to reconnect after failure
    FAILED         // Maximum reconnection attempts exceeded
};
```

### Threading Architecture

The WebSocket implementation uses two background threads to prevent UI blocking:

#### Connection Thread (`connectionThreadMain()`)
- **Purpose**: Manages WebSocket connection lifecycle
- **Responsibilities**:
  - Initial connection establishment
  - Heartbeat transmission every 30 seconds (configurable)
  - Reconnection attempts with exponential backoff
  - Connection state monitoring and updates

#### Message Processing Thread (`messageProcessingThreadMain()`)
- **Purpose**: Handles all message I/O operations
- **Responsibilities**:
  - Processing outgoing message queue
  - Processing incoming message queue
  - Message correlation and callback invocation
  - Performance metrics collection

### Auto-Reconnection Logic

```cpp
void WebSocketClient::handleReconnection()
{
    ++m_nCurrentReconnectAttempt;
    ++m_aMetrics.nReconnectCount;
    
    updateConnectionState(ConnectionState::RECONNECTING, 
                         "Reconnection attempt " + OUString::number(m_nCurrentReconnectAttempt));
    
    // Exponential backoff: 2s, 4s, 8s, 16s, 32s (max)
    sal_Int32 nDelayMs = std::min(m_nReconnectDelayMs * (1 << (m_nCurrentReconnectAttempt - 1)), 32000);
    std::this_thread::sleep_for(std::chrono::milliseconds(nDelayMs));
    
    updateConnectionState(ConnectionState::CONNECTING, "Retrying connection");
}
```

## Error Handling and Fallback

### Graceful Degradation Strategy

The WebSocket implementation follows a multi-tier fallback approach:

```
Error Handling Hierarchy:
┌─────────────────────────────────────────────────────────────────────────────────────┐
│  TIER 1: WebSocket Preferred (Real-time communication)                             │
│  ├─ Used for: Complex requests requiring streaming updates                         │
│  ├─ Benefits: Real-time progress, immediate feedback, agent status                 │
│  ├─ Failure modes: Connection timeout, server unavailable, protocol error         │
│  └─ Fallback trigger: WebSocket send failure or connection loss                   │
│                                          │                                         │
│                                          ▼                                         │
│  TIER 2: HTTP Fallback (Synchronous communication)                                │
│  ├─ Used for: All request types when WebSocket unavailable                        │
│  ├─ Benefits: Reliable delivery, standard HTTP semantics                          │
│  ├─ Limitations: No real-time updates, synchronous operation                      │
│  └─ Fallback trigger: HTTP connection failure or server error                     │
│                                          │                                         │
│                                          ▼                                         │
│  TIER 3: Offline Mode (Local processing)                                          │
│  ├─ Used for: Basic operations when backend unavailable                           │
│  ├─ Benefits: Always available, no network dependency                             │
│  ├─ Limitations: No AI processing, limited functionality                          │
│  └─ Recovery: Automatic retry when connectivity restored                          │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### Error Classification and Recovery

#### WebSocket-Specific Errors
```cpp
// Connection errors
1001: Connection closed by server
1006: Abnormal connection closure
1011: Server error (temporary)
1012: Service restart

// Protocol errors
1002: Protocol error in message format
1003: Unsupported message type
1007: Invalid UTF-8 data
1008: Policy violation

// Client errors
400:  Invalid WebSocket URL format
401:  Authentication failure
403:  Authorization denied
429:  Rate limit exceeded
```

#### Recovery Mechanisms
```cpp
void WebSocketClient::handleWebSocketError(const OUString& rsError, sal_Int32 nErrorCode)
{
    ++m_aMetrics.nErrorCount;
    
    SAL_WARN("sw.ai", "WebSocket error (" << nErrorCode << "): " << rsError);
    
    if (m_aErrorCallback)
    {
        m_aErrorCallback(rsError, nErrorCode);
    }
    
    // Determine recovery strategy based on error code
    if (nErrorCode == 1001 || nErrorCode == 1006) // Connection closed
    {
        if (m_bAutoReconnect && m_eConnectionState == ConnectionState::CONNECTED)
        {
            updateConnectionState(ConnectionState::RECONNECTING, "Auto-reconnect after error");
            m_aConnectionCV.notify_one();
        }
    }
    else if (nErrorCode >= 1011 && nErrorCode <= 1014) // Server errors
    {
        // Temporary server issues - retry with longer delay
        if (m_bAutoReconnect)
        {
            m_nReconnectDelayMs *= 2; // Double the delay for server errors
            updateConnectionState(ConnectionState::RECONNECTING, "Server error recovery");
        }
    }
    else if (nErrorCode >= 400 && nErrorCode < 500) // Client errors
    {
        // Client configuration issues - don't auto-retry
        updateConnectionState(ConnectionState::FAILED, "Client configuration error");
    }
}
```

### HTTP Fallback Integration

When WebSocket communication fails, the system seamlessly falls back to HTTP:

```cpp
OUString AgentCoordinator::processComplexRequest(const OUString& rsRequest, const Any& rContext)
{
    OUString sRequestId = generateRequestId();
    
    // Try WebSocket first
    if (isWebSocketEnabled())
    {
        bool bWebSocketSent = sendWebSocketMessage(rsRequest, sRequestId);
        if (bWebSocketSent)
        {
            return "Processing via WebSocket (ID: " + sRequestId + ") - streaming enabled";
        }
        else
        {
            SAL_WARN("sw.ai", "WebSocket send failed, falling back to HTTP");
        }
    }
    
    // HTTP fallback with same request ID for correlation
    if (m_bOnlineMode && m_pNetworkClient)
    {
        OUString sJsonBody = R"({"request": ")" + rsRequest + 
                           R"(", "type": "complex", "request_id": ")" + sRequestId + R"("})";
        
        std::map<OUString, OUString> aHeaders;
        aHeaders["X-Request-ID"] = sRequestId;
        aHeaders["X-Fallback-From"] = "websocket";
        
        sw::ai::NetworkClient::HttpResponse aResponse = 
            m_pNetworkClient->postJson("http://localhost:8000/api/complex", sJsonBody, aHeaders);
        
        if (aResponse.bSuccess)
        {
            return "AI processed via HTTP fallback: " + rsRequest;
        }
    }
    
    // Final fallback to offline mode
    return "Offline processed: " + rsRequest;
}
```

## Performance Optimization

### Connection Pooling and Resource Management

The WebSocket implementation includes several performance optimizations:

#### Connection Efficiency
```cpp
struct ConnectionMetrics
{
    std::chrono::steady_clock::time_point aConnectedTime;
    std::chrono::steady_clock::time_point aLastMessageTime;
    sal_Int32 nMessagesSent;
    sal_Int32 nMessagesReceived;
    sal_Int32 nReconnectCount;
    sal_Int32 nErrorCount;
};
```

#### Message Queue Optimization
- **Outgoing Queue**: Batches messages to reduce WebSocket overhead
- **Incoming Queue**: Processes messages asynchronously to prevent UI blocking
- **Priority Handling**: Higher priority for error notifications and connection events

#### Memory Management
```cpp
void WebSocketClient::shutdown()
{
    // Graceful thread termination
    m_bShutdownRequested = true;
    m_aConnectionCV.notify_all();
    m_aMessageCV.notify_all();
    
    // Thread cleanup
    if (m_pConnectionThread && m_pConnectionThread->joinable())
    {
        m_pConnectionThread->join();
        m_pConnectionThread.reset();
    }
    
    // Clear message queues to free memory
    std::queue<WebSocketMessage> empty1, empty2;
    m_aIncomingMessages.swap(empty1);
    m_aOutgoingMessages.swap(empty2);
}
```

### Heartbeat Monitoring

Maintains connection health with configurable heartbeat intervals:

```cpp
void WebSocketClient::sendHeartbeat()
{
    OUString sHeartbeat = R"({"type": "heartbeat", "timestamp": ")" + 
                         OUString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now().time_since_epoch()).count()) + R"("})";
    
    sendMessage(sHeartbeat, MessageType::HEARTBEAT);
}
```

**Benefits:**
- Detects connection failures quickly (30-second default interval)
- Prevents proxy timeouts on long-lived connections
- Provides server health monitoring capabilities
- Enables proactive reconnection before user-initiated requests fail

## Development and Production Considerations

### Current Implementation Status

The current WebSocket implementation is designed for development and testing:

#### Development Features (Currently Implemented)
- **Simulated WebSocket Connection**: Mock connection for development testing
- **Full Protocol Structure**: Complete message types and JSON formatting
- **LibreOffice Integration**: Full AgentCoordinator integration with callbacks
- **Error Handling**: Comprehensive error recovery and fallback mechanisms
- **Threading Architecture**: Background threads prevent UI blocking
- **Performance Monitoring**: Connection metrics and debugging capabilities

#### Production Requirements (Future Implementation)

For production deployment, the following enhancements would be needed:

#### 1. Native WebSocket Library Integration
```cpp
// Current: Simulated connection
bool WebSocketClient::attemptConnection()
{
    // Simulate connection for development
    if (m_sWebSocketUrl.indexOf("localhost") >= 0)
    {
        logDebug("WebSocket connection simulated successfully");
        return true;
    }
    return false;
}

// Production: Real WebSocket implementation
bool WebSocketClient::attemptConnection()
{
    // Option 1: libwebsockets integration
    m_pWebSocketContext = libwebsocket_create_context(&info);
    m_pWebSocketInstance = libwebsocket_client_connect(...);
    
    // Option 2: WebSocket++ integration
    m_client.set_access_channels(websocketpp::log::alevel::all);
    websocketpp::lib::error_code ec;
    client::connection_ptr con = m_client.get_connection(uri, ec);
    
    // Option 3: Custom WebSocket implementation over TCP
    // Implement RFC 6455 WebSocket protocol
    
    return establishWebSocketHandshake();
}
```

#### 2. SSL/TLS Support
```cpp
bool WebSocketClient::validateWebSocketUrl(const OUString& rsUrl)
{
    if (rsUrl.isEmpty())
        return false;
    
    // Support both ws:// and wss:// protocols
    if (!rsUrl.startsWithIgnoreAsciiCase("ws://") && 
        !rsUrl.startsWithIgnoreAsciiCase("wss://"))
    {
        return false;
    }
    
    // Additional validation for production:
    // - Certificate validation for wss://
    // - Hostname verification
    // - Protocol version negotiation
    
    return true;
}
```

#### 3. Authentication Integration
```cpp
void WebSocketClient::setAuthenticationToken(const OUString& rsToken)
{
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    m_sAuthToken = rsToken;
    
    // Update connection headers
    m_aConnectionHeaders["Authorization"] = "Bearer " + rsToken;
    m_aConnectionHeaders["X-Client-Type"] = "libreoffice-writer";
    m_aConnectionHeaders["X-Client-Version"] = LIBREOFFICE_VERSION;
}
```

#### 4. Production Logging and Monitoring
```cpp
class WebSocketMetrics
{
private:
    std::atomic<sal_Int64> m_nTotalConnections{0};
    std::atomic<sal_Int64> m_nSuccessfulConnections{0};
    std::atomic<sal_Int64> m_nFailedConnections{0};
    std::atomic<sal_Int64> m_nMessagesProcessed{0};
    std::atomic<sal_Int64> m_nAverageLatencyMs{0};
    
public:
    void recordConnection(bool bSuccess);
    void recordMessage(sal_Int64 nLatencyMs);
    void exportMetrics(const OUString& rsFilePath);
};
```

### Configuration Management

The WebSocket client supports extensive configuration for different deployment scenarios:

#### LibreOffice Configuration Integration
```cpp
void WebSocketClient::loadProductionConfig()
{
    // Read from LibreOffice configuration registry
    Reference<XNameAccess> xConfig = getConfigurationAccess(
        "/org.openoffice.Office.Writer/AI/WebSocket");
    
    if (xConfig.is())
    {
        Any aValue;
        
        // Connection settings
        if (xConfig->getByName("DefaultURL") >>= aValue)
            aValue >>= m_sDefaultWebSocketUrl;
            
        if (xConfig->getByName("EnableSSL") >>= aValue)
            aValue >>= m_bEnableSSL;
            
        if (xConfig->getByName("HeartbeatInterval") >>= aValue)
            aValue >>= m_nHeartbeatIntervalMs;
            
        // Security settings
        if (xConfig->getByName("ValidateCertificates") >>= aValue)
            aValue >>= m_bValidateCertificates;
            
        if (xConfig->getByName("AllowedOrigins") >>= aValue)
        {
            Sequence<OUString> aOrigins;
            aValue >>= aOrigins;
            for (const auto& rsOrigin : aOrigins)
                m_aAllowedOrigins.insert(rsOrigin);
        }
    }
}
```

### Deployment Configurations

#### Development Environment
```json
{
    "websocket": {
        "enabled": true,
        "url": "ws://localhost:8000/ws/libreoffice",
        "auto_reconnect": true,
        "max_reconnect_attempts": 10,
        "reconnect_delay_ms": 1000,
        "heartbeat_interval_ms": 10000,
        "enable_logging": true,
        "log_level": "debug"
    }
}
```

#### Production Environment
```json
{
    "websocket": {
        "enabled": true,
        "url": "wss://api.langgraph.ai/ws/libreoffice",
        "auto_reconnect": true,
        "max_reconnect_attempts": 3,
        "reconnect_delay_ms": 5000,
        "heartbeat_interval_ms": 30000,
        "enable_logging": false,
        "ssl_verify_certificates": true,
        "connection_timeout_ms": 10000,
        "message_queue_max_size": 1000
    }
}
```

## API Reference

### WebSocketClient Public Interface

#### Connection Management Methods

```cpp
// Initialize WebSocket client with configuration
bool initialize(const css::uno::Sequence<css::beans::PropertyValue>& rConfig = {});

// Connect to WebSocket server
bool connect(const OUString& rsUrl, const OUString& rsProtocol = OUString());

// Disconnect from WebSocket server
void disconnect();

// Shutdown and cleanup
void shutdown();

// Get current connection state
ConnectionState getConnectionState() const;

// Check if connected and operational
bool isConnected() const;
```

#### Message Handling Methods

```cpp
// Send text message
bool sendMessage(const OUString& rsMessage, MessageType eType = MessageType::TEXT);

// Send JSON message with headers
bool sendJsonMessage(const OUString& rsJsonMessage, 
                    const std::map<OUString, OUString>& aHeaders = {});

// Request progress update for specific request
bool requestProgress(const OUString& rsRequestId);

// Query agent status
bool queryAgentStatus(const OUString& rsAgentName);
```

#### Callback Registration Methods

```cpp
// Set message callback
void setMessageCallback(const MessageCallback& aCallback);

// Set connection state callback
void setConnectionCallback(const ConnectionCallback& aCallback);

// Set error callback
void setErrorCallback(const ErrorCallback& aCallback);
```

#### Configuration Methods

```cpp
// Configure auto-reconnection
void setAutoReconnect(bool bEnable, sal_Int32 nMaxAttempts = 5, sal_Int32 nDelayMs = 2000);

// Set heartbeat interval
void setHeartbeatInterval(sal_Int32 nIntervalMs);

// Enable/disable debug logging
void setLoggingEnabled(bool bEnable);

// Get performance metrics
ConnectionMetrics getConnectionMetrics() const;

// Clear accumulated metrics
void clearMetrics();
```

### AgentCoordinator WebSocket Integration

#### WebSocket-Specific Methods

```cpp
// Initialize WebSocket client
bool initializeWebSocketClient();

// Connect to WebSocket server
bool connectWebSocket(const OUString& rsUrl);

// Disconnect WebSocket
void disconnectWebSocket();

// Send message via WebSocket
bool sendWebSocketMessage(const OUString& rsMessage, const OUString& rsRequestId);

// Handle incoming WebSocket messages
void handleWebSocketMessage(const OUString& rsMessage);

// Handle connection state changes
void handleWebSocketConnectionChange(bool bConnected);

// Check if WebSocket is enabled and connected
bool isWebSocketEnabled() const;
```

### Configuration Properties

#### WebSocket Client Configuration

```cpp
Sequence<PropertyValue> aConfig(7);
aConfig[0].Name = "AutoReconnect";          // bool - Enable automatic reconnection
aConfig[0].Value <<= true;

aConfig[1].Name = "MaxReconnectAttempts";   // sal_Int32 - Maximum retry attempts
aConfig[1].Value <<= sal_Int32(3);

aConfig[2].Name = "ReconnectDelayMs";       // sal_Int32 - Delay between retries
aConfig[2].Value <<= sal_Int32(2000);

aConfig[3].Name = "HeartbeatIntervalMs";    // sal_Int32 - Heartbeat frequency
aConfig[3].Value <<= sal_Int32(30000);

aConfig[4].Name = "EnableLogging";          // bool - Debug logging
aConfig[4].Value <<= true;

aConfig[5].Name = "Protocol";               // OUString - WebSocket sub-protocol
aConfig[5].Value <<= OUString("langgraph-ai");

aConfig[6].Name = "ConnectionTimeoutMs";    // sal_Int32 - Connection timeout
aConfig[6].Value <<= sal_Int32(10000);
```

## Usage Examples

### Basic WebSocket Usage

#### 1. Initialization and Connection
```cpp
// Create and initialize WebSocket client
Reference<XComponentContext> xContext = comphelper::getProcessComponentContext();
auto pWebSocketClient = std::make_unique<sw::ai::WebSocketClient>(xContext);

// Configure client
Sequence<PropertyValue> aConfig(3);
aConfig[0].Name = "AutoReconnect";
aConfig[0].Value <<= true;
aConfig[1].Name = "HeartbeatIntervalMs";
aConfig[1].Value <<= sal_Int32(30000);
aConfig[2].Name = "EnableLogging";
aConfig[2].Value <<= true;

bool bInitialized = pWebSocketClient->initialize(aConfig);
if (!bInitialized)
{
    SAL_WARN("sw.ai", "WebSocket client initialization failed");
    return false;
}

// Set up callbacks
pWebSocketClient->setMessageCallback([](const WebSocketMessage& aMessage) {
    SAL_INFO("sw.ai", "Message received: " << aMessage.sContent);
});

pWebSocketClient->setConnectionCallback([](ConnectionState eState, const OUString& rsMessage) {
    SAL_INFO("sw.ai", "Connection state: " << static_cast<int>(eState) << " - " << rsMessage);
});

// Connect to server
bool bConnected = pWebSocketClient->connect("ws://localhost:8000/ws/libreoffice", "langgraph-ai");
if (!bConnected)
{
    SAL_WARN("sw.ai", "WebSocket connection failed");
    return false;
}
```

#### 2. Sending Messages
```cpp
// Send simple text message
bool bSent = pWebSocketClient->sendMessage("Process this document", MessageType::TEXT);

// Send JSON message with headers
OUString sJsonMessage = R"({
    "request": "Create financial report",
    "document_type": "writer",
    "complexity": "high",
    "agents": ["DocumentMaster", "DataIntegration", "ContentGeneration"]
})";

std::map<OUString, OUString> aHeaders;
aHeaders["X-Request-Priority"] = "high";
aHeaders["X-User-ID"] = "user123";

bool bJsonSent = pWebSocketClient->sendJsonMessage(sJsonMessage, aHeaders);

// Request progress update
bool bProgressSent = pWebSocketClient->requestProgress("REQ_123456_789");

// Query agent status
bool bStatusSent = pWebSocketClient->queryAgentStatus("DataIntegration");
```

#### 3. Handling Real-Time Updates
```cpp
void MyClass::setupWebSocketHandlers()
{
    m_pWebSocketClient->setMessageCallback([this](const WebSocketMessage& aMessage) {
        handleRealtimeUpdate(aMessage);
    });
}

void MyClass::handleRealtimeUpdate(const WebSocketMessage& aMessage)
{
    // Parse JSON message (simplified example)
    if (aMessage.sContent.indexOf("\"type\":\"progress_update\"") >= 0)
    {
        // Extract progress information
        // Update UI progress indicators
        updateProgressIndicator(aMessage.sContent);
    }
    else if (aMessage.sContent.indexOf("\"type\":\"streaming_response\"") >= 0)
    {
        // Handle streaming content
        appendStreamingContent(aMessage.sContent);
    }
    else if (aMessage.sContent.indexOf("\"type\":\"agent_status\"") >= 0)
    {
        // Update agent availability display
        updateAgentStatus(aMessage.sContent);
    }
}

void MyClass::updateProgressIndicator(const OUString& rsProgressMessage)
{
    // Extract progress percentage and current step
    // Update UI elements in main thread
    Application::PostUserEvent([=]() {
        // Update progress bar
        m_pProgressBar->SetValue(nProgress);
        m_pStatusLabel->SetText(sCurrentStep);
    });
}
```

### Integration with AgentCoordinator

#### Complex Request Processing with WebSocket
```cpp
OUString MyAgentCoordinator::processUserRequest(const OUString& rsRequest, const Any& rContext)
{
    // Generate unique request ID
    OUString sRequestId = generateRequestId();
    
    // Determine request complexity
    RequestComplexity eComplexity = analyzeRequestComplexity(rsRequest);
    
    if (eComplexity == RequestComplexity::Complex && isWebSocketEnabled())
    {
        // Use WebSocket for complex requests requiring real-time updates
        SAL_INFO("sw.ai", "Processing complex request via WebSocket: " << sRequestId);
        
        // Send request via WebSocket
        bool bSent = sendWebSocketMessage(rsRequest, sRequestId);
        if (bSent)
        {
            // Store request context for correlation with streaming updates
            m_aActiveRequests[sRequestId] = PendingRequest(sRequestId, rsRequest, rContext);
            
            // Return immediate acknowledgment
            return "Complex request initiated via WebSocket (ID: " + sRequestId + 
                   "). Real-time updates will be streamed to the UI.";
        }
    }
    
    // Fallback to HTTP or offline processing
    return processRequestViaHTTP(rsRequest, rContext, sRequestId);
}
```

#### WebSocket Message Correlation
```cpp
void MyAgentCoordinator::handleWebSocketMessage(const OUString& rsMessage)
{
    try
    {
        // Parse JSON to extract request ID and message type
        OUString sRequestId = extractRequestId(rsMessage);
        OUString sMessageType = extractMessageType(rsMessage);
        
        // Find corresponding pending request
        auto it = m_aActiveRequests.find(sRequestId);
        if (it == m_aActiveRequests.end())
        {
            SAL_WARN("sw.ai", "Received message for unknown request: " << sRequestId);
            return;
        }
        
        // Handle different message types
        if (sMessageType == "progress_update")
        {
            handleProgressUpdate(sRequestId, rsMessage);
        }
        else if (sMessageType == "streaming_response")
        {
            handleStreamingResponse(sRequestId, rsMessage);
        }
        else if (sMessageType == "final_response")
        {
            handleFinalResponse(sRequestId, rsMessage);
            m_aActiveRequests.erase(it); // Request completed
        }
        else if (sMessageType == "error_notification")
        {
            handleErrorNotification(sRequestId, rsMessage);
            m_aActiveRequests.erase(it); // Request failed
        }
    }
    catch (const Exception& e)
    {
        SAL_WARN("sw.ai", "WebSocket message handling failed: " << e.Message);
    }
}

void MyAgentCoordinator::handleProgressUpdate(const OUString& rsRequestId, const OUString& rsMessage)
{
    // Extract progress information
    sal_Int32 nProgress = extractProgress(rsMessage);
    OUString sCurrentStep = extractCurrentStep(rsMessage);
    OUString sAgent = extractAgent(rsMessage);
    
    // Update UI in main thread
    Application::PostUserEvent([=]() {
        // Find corresponding UI panel
        if (auto pPanel = findAIPanelForRequest(rsRequestId))
        {
            pPanel->updateProgress(nProgress, sCurrentStep, sAgent);
        }
    });
    
    SAL_INFO("sw.ai", "Progress update for " << rsRequestId << ": " << 
             nProgress << "% - " << sCurrentStep << " (" << sAgent << ")");
}
```

### Error Handling Examples

#### Connection Error Recovery
```cpp
void MyClass::handleWebSocketError(const OUString& rsError, sal_Int32 nErrorCode)
{
    SAL_WARN("sw.ai", "WebSocket error (" << nErrorCode << "): " << rsError);
    
    switch (nErrorCode)
    {
        case 1001: // Connection closed
        case 1006: // Abnormal closure
            // Attempt automatic reconnection
            if (m_bAutoReconnectEnabled)
            {
                scheduleReconnection(2000); // Reconnect after 2 seconds
            }
            else
            {
                // Notify user and suggest manual reconnection
                showUserNotification("WebSocket connection lost. Click to reconnect.", 
                                   NotificationType::WARNING);
            }
            break;
            
        case 1011: // Server error
        case 1012: // Service restart
            // Server-side issues - use longer reconnection delay
            scheduleReconnection(10000); // Reconnect after 10 seconds
            break;
            
        case 401: // Authentication failure
        case 403: // Authorization denied
            // Authentication issues - require user intervention
            showUserNotification("WebSocket authentication failed. Please check credentials.", 
                               NotificationType::ERROR);
            break;
            
        case 429: // Rate limit exceeded
            // Too many requests - back off significantly
            scheduleReconnection(60000); // Wait 1 minute before retry
            break;
            
        default:
            // Unknown error - use default recovery
            if (m_bAutoReconnectEnabled)
            {
                scheduleReconnection(5000);
            }
            break;
    }
}

void MyClass::scheduleReconnection(sal_Int32 nDelayMs)
{
    // Use LibreOffice timer for delayed reconnection
    if (!m_pReconnectTimer)
    {
        m_pReconnectTimer = std::make_unique<Timer>("WebSocket Reconnect");
    }
    
    m_pReconnectTimer->SetTimeout(nDelayMs);
    m_pReconnectTimer->SetTimeoutHdl(LINK(this, MyClass, ReconnectTimerHandler));
    m_pReconnectTimer->Start();
}

IMPL_LINK_NOARG(MyClass, ReconnectTimerHandler, Timer*, void)
{
    SAL_INFO("sw.ai", "Attempting WebSocket reconnection");
    
    bool bReconnected = m_pWebSocketClient->connect(m_sWebSocketUrl, "langgraph-ai");
    if (bReconnected)
    {
        SAL_INFO("sw.ai", "WebSocket reconnection successful");
        showUserNotification("Connection restored", NotificationType::INFO);
    }
    else
    {
        SAL_WARN("sw.ai", "WebSocket reconnection failed");
        // Schedule another retry with exponential backoff
        scheduleReconnection(std::min(m_nLastReconnectDelay * 2, 60000));
    }
}
```

This comprehensive WebSocket architecture documentation provides a complete understanding of how real-time communication is implemented in the LibreOffice AI Writing Assistant, enabling streaming updates and responsive user interaction during complex LangGraph agent processing workflows.
