# LibreOffice LangGraph FastAPI Server

This FastAPI server provides HTTP and WebSocket endpoints for LibreOffice C++ AgentCoordinator integration with the LangGraph multi-agent system.

## 🏗️ Architecture

The server acts as a bridge between LibreOffice C++ and the Python LangGraph agents:

```
LibreOffice C++ ←→ FastAPI Server ←→ bridge.py ←→ LangGraph Agents
```

## 📡 Endpoints

### HTTP API Endpoints

All endpoints expect JSON requests in LibreOffice-compatible format:

```json
{
  "request": "user message",
  "type": "simple|moderate|complex", 
  "complexity": "low|medium|high",
  "request_id": "unique_id",
  "context": {
    "document_path": "path/to/document.odt",
    "cursor_position": {"line": 1, "column": 5},
    "selected_text": "sample text",
    "document_type": "text"
  }
}
```

#### Core Processing Endpoints

- **POST /api/simple** - Fast processing for basic operations (target: <2s)
  - Bold/italic formatting, simple text operations
  - Headers: `X-Request-Type: simple`

- **POST /api/moderate** - Balanced workflow for medium complexity (target: <4s)  
  - Document structure, styling, moderate content generation
  - Headers: `X-Request-Type: moderate`, `X-Include-Context: true`

- **POST /api/complex** - Full agent workflow for complex operations (target: <5s)
  - Multi-agent coordination, data integration, comprehensive analysis
  - Headers: `X-Request-Type: complex`, `X-Agent-Workflow: complete`

- **POST /api/process** - General processing endpoint (legacy compatibility)
  - Auto-determines complexity level
  - Headers: `X-Request-Type: general`

#### Utility Endpoints

- **GET /health** - Server health check
- **GET /status** - Detailed server status and bridge information

### WebSocket Endpoint

- **WS /ws/libreoffice** - Real-time streaming communication
  - Protocol: `langgraph-ai`
  - Supports progress updates, cancellation, streaming responses
  - Used for complex operations requiring real-time feedback

## 🚀 Quick Start

### 1. Set Up Virtual Environment

```bash
# Create virtual environment
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate  # On Linux/macOS
# venv\Scripts\activate   # On Windows

# Upgrade pip
pip install --upgrade pip
```

### 2. Install Dependencies

```bash
pip install -r requirements.txt
```

**Note**: Some packages like `uno` and `pyuno` are commented out due to dependency conflicts. These will be available in the LibreOffice Python environment when running within LibreOffice.

### 3. Validate Implementation

```bash
python3 validate_implementation.py
```

### 4. Run Integration Tests

```bash
python3 test_integration.py
```

### 5. Start Server

```bash
python3 start_server.py
```

Server will start on `http://localhost:8000`

## 🧪 Testing

### Quick Integration Test

```bash
python3 test_integration.py
```

Tests core functionality without external dependencies.

### Comprehensive Test Suite

```bash
# Install test dependencies first
pip install pytest pytest-asyncio httpx websockets

# Run full test suite
python3 test_api_server.py
```

Includes end-to-end workflow testing, WebSocket functionality, and performance validation.

## 📋 Request/Response Format

### Request Headers (from LibreOffice C++)

```
Content-Type: application/json
X-Request-Type: simple|moderate|complex|general
X-Include-Context: true|full
X-Agent-Workflow: complete
Authorization: Bearer <token>
```

### Response Format

```json
{
  "request_id": "unique_request_id",
  "success": true,
  "result": {
    "operations": [...],
    "content": "...",
    "formatting": {...}
  },
  "error_message": null,
  "execution_time_ms": 1250.5,
  "agent_results": {
    "DocumentMasterAgent": {...},
    "ContextAnalysisAgent": {...},
    "ContentGenerationAgent": {...}
  },
  "final_state": {
    "document_state": {...},
    "cursor_position": {...}
  },
  "metadata": {
    "request_type": "complex",
    "processing_time_ms": 1250.5
  }
}
```

## 🔌 WebSocket Protocol

### Connection

```javascript
const ws = new WebSocket('ws://localhost:8000/ws/libreoffice', 'langgraph-ai');
```

### Message Types

#### Request Processing
```json
{
  "type": "request",
  "request_id": "unique_id", 
  "message": "Create financial analysis",
  "context": {...}
}
```

#### Progress Updates
```json
{
  "type": "progress",
  "request_id": "unique_id",
  "content": {
    "status": "agents_coordinating", 
    "message": "DocumentMasterAgent analyzing request",
    "progress": 50
  }
}
```

#### Final Response
```json
{
  "type": "response",
  "request_id": "unique_id",
  "content": {
    "success": true,
    "result": {...},
    "agent_results": {...}
  }
}
```

#### Cancellation
```json
{
  "type": "cancel",
  "request_id": "unique_id"
}
```

## 🔧 Configuration

### Environment Variables

```bash
# AI API Keys
OPENAI_API_KEY=your_openai_key
ANTHROPIC_API_KEY=your_anthropic_key

# Performance Tuning
AGENT_SIMPLE_TARGET=2.0
AGENT_MODERATE_TARGET=4.0  
AGENT_COMPLEX_TARGET=5.0
AGENT_MAX_MEMORY_MB=200
AGENT_MAX_CPU_PERCENT=10.0

# LibreOffice Integration
UNO_CONNECTION_TIMEOUT=10
DOC_OPERATION_TIMEOUT=30
MAX_DOCUMENT_SIZE_MB=50

# Logging
AGENT_LOG_LEVEL=INFO
```

### Server Configuration

Edit `start_server.py` to modify:
- Host/port binding
- Worker processes
- Logging configuration
- Development/production settings

## 🏃‍♂️ Development

### Running in Development Mode

```bash
# Enable auto-reload
uvicorn api_server:app --reload --host 0.0.0.0 --port 8000
```

### Adding New Endpoints

1. Add endpoint function to `api_server.py`
2. Update request/response models if needed
3. Add tests to `test_api_server.py`
4. Update this documentation

### WebSocket Debugging

```bash
# Test WebSocket connection
wscat -c ws://localhost:8000/ws/libreoffice -s langgraph-ai
```

## 🚨 Troubleshooting

### Common Issues

1. **Module not found errors**
   ```bash
   pip install -r requirements.txt
   ```

2. **Bridge initialization failed**
   - Check AI API keys are configured
   - Verify LangGraph agents are accessible
   - Check logs for specific error messages

3. **WebSocket connection refused**
   - Ensure server is running on port 8000
   - Check firewall settings
   - Verify subprotocol: `langgraph-ai`

4. **Performance issues**
   - Check resource limits in configuration
   - Monitor memory usage
   - Adjust timeout settings

### Debug Logging

```bash
AGENT_LOG_LEVEL=DEBUG python3 start_server.py
```

## 📊 Monitoring

### Health Checks

```bash
# Quick health check
curl http://localhost:8000/health

# Detailed status
curl http://localhost:8000/status
```

### Performance Metrics

The server tracks:
- Request processing times
- Agent coordination metrics  
- WebSocket connection counts
- Error rates and types

Access via `/status` endpoint or check logs.

## 🔗 Integration with LibreOffice

The server is designed to be a drop-in replacement for the expected LangGraph backend. LibreOffice C++ AgentCoordinator will automatically connect to:

- `http://localhost:8000/api/simple`
- `http://localhost:8000/api/moderate` 
- `http://localhost:8000/api/complex`
- `http://localhost:8000/api/process`
- `ws://localhost:8000/ws/libreoffice`

No LibreOffice configuration changes should be required.

## 📚 Additional Resources

- [LangGraph Agent Documentation](./README.md)
- [Bridge Implementation](./bridge.py)
- [Configuration Guide](./config.py)
- [Test Suite](./test_api_server.py)