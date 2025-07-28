# LangGraph Agent Orchestrator Schema

## Overview

This document describes the architecture and flow of the LangGraph-based AI agent orchestrator for the LibreOffice Finance Office project. The agent uses a multi-node workflow to process user requests, classify intent, perform operations, and generate responses with corresponding document operations.

## Agent Architecture

The agent is structured as a **standard orchestrator agent** with an enhanced workflow consisting of 7 main nodes, including a specialized Data Analyst Agent for financial data integration:

```
[User Request] → [Intent Classifier] ←→ [Data Analyst Agent] → [Operation Nodes] → [Response Creator] → [JSON Response]
                        ↓                      ↓
                 [Enhanced Context]    [Yahoo Finance API]
                        ↓
                  [Generate Content (Enhanced)]
                  [Classify Formatting]  
                  [Create Chart (Blank)]
                  [Create Table (Blank)]
```

## Node Definitions

### 1. Intent Classifier Node

**Purpose**: Analyzes the incoming user request to determine which operation should be performed and whether financial data is required.

**Input**: 
- Raw user request (natural language)
- Document context (cursor position, selected text, etc.)

**Process**:
- Uses LLM to analyze user intent
- Classifies request into one of four operation categories
- Determines if financial data is needed (for Generate Content operations)
- If financial data required, calls Data Analyst Agent
- Extracts relevant parameters for the chosen operation

**Output**:
- Intent classification result
- Operation type selection
- Enhanced context (with financial data if applicable)
- Extracted parameters

**Decision Logic**:
```python
def should_fetch_financial_data(user_request, intent):
    financial_keywords = ["stock", "price", "earnings", "market", "AAPL", "financial", "investment"]
    has_financial_terms = any(keyword in user_request.lower() for keyword in financial_keywords)
    
    return intent == "generate_content" and has_financial_terms
```

**LLM Prompt Strategy**:
```
Analyze the user request and classify the intent into one of these categories:
1. GENERATE_CONTENT - User wants to insert new text/content
2. CLASSIFY_FORMATTING - User wants to format existing selected text  
3. CREATE_CHART - User wants to insert a chart/graph
4. CREATE_TABLE - User wants to insert a table

Also determine if the request requires financial data (stock prices, market data, etc.)

User Request: {request}
Document Context: {context}
```

### 2. Data Analyst Agent (Sub-Agent)

**Purpose**: Specialized agent for fetching and processing financial data from external APIs, specifically Yahoo Finance.

**Triggered When**: Intent Classifier determines that financial data is required for Generate Content operations.

**Input**:
- User request (for parameter extraction)
- Financial data requirements identified by Intent Classifier

**Process**:
- Uses LLM to extract financial parameters (stock symbols, timeframes, data types)
- Makes API calls to Yahoo Finance for real-time financial data
- Processes and formats data for downstream consumption
- Creates human-readable financial summaries

**API Integration**:
```python
async def fetch_financial_data(request):
    # LLM extracts parameters
    params = extract_financial_params(request)  # e.g., "AAPL", "quarterly"
    
    # Yahoo Finance API calls
    stock_data = yfinance.Ticker(params["symbol"]).info
    price_data = yfinance.Ticker(params["symbol"]).history(period="1d")
    
    return {
        "stock_prices": {"current": price_data["Close"][-1], "change": "..."},
        "financial_metrics": {"P/E": stock_data["trailingPE"], "market_cap": "..."},
        "formatted_summary": "AAPL is trading at $185.23, up 2.1%..."
    }
```

**Output**:
```json
{
  "financial_data": {
    "stock_prices": {
      "symbol": "AAPL",
      "current_price": 185.23,
      "change_percent": "+2.1%",
      "currency": "USD"
    },
    "financial_metrics": {
      "market_cap": "2.9T",
      "pe_ratio": 28.5,
      "volume": "45.2M"
    },
    "formatted_summary": "Apple (AAPL) is trading at $185.23, up 2.1% today with a market cap of $2.9T",
    "data_source": "yahoo_finance",
    "timestamp": "2024-01-15T10:30:00Z"
  }
}
```

**Error Handling**:
- Graceful fallback when APIs are unavailable
- Validation of financial symbols and parameters
- Cached data for recent requests to avoid rate limiting

### 3. Operation Nodes

#### 2.1 Generate Content Node

**Purpose**: Creates content for text insertion operations.

**Triggered When**: Intent classifier determines `GENERATE_CONTENT`

**Input**:
- User request for content generation
- Document context
- Content specifications

**Process**:
- Uses LLM to generate appropriate text content
- Considers document context and user requirements
- Formats content for insertion

**Output**:
```json
{
  "operation_type": "insert",
  "content": "Generated text content here",
  "metadata": {
    "content_type": "text",
    "length": 150,
    "tone": "professional"
  }
}
```

#### 2.2 Classify Formatting Node

**Purpose**: Determines formatting specifications for selected text.

**Triggered When**: Intent classifier determines `CLASSIFY_FORMATTING`

**Input**:
- User formatting request
- Currently selected text
- Formatting preferences

**Process**:
- Analyzes user request for formatting intent
- Maps to specific formatting properties
- Validates formatting options

**Output**:
```json
{
  "operation_type": "format",
  "formatting": {
    "bold": "true|false",
    "italic": "true|false",
    "underline": "true|false",
    "font_size": "12",
    "color": "#000000"
  },
  "metadata": {
    "selected_text_length": 25,
    "formatting_scope": "selection"
  }
}
```

#### 2.3 Create Chart Node

**Purpose**: Generates chart specifications and configuration.

**Triggered When**: Intent classifier determines `CREATE_CHART`

**Input**:
- User chart request
- Data requirements
- Chart preferences

**Process**:
- Determines appropriate chart type
- Analyzes data requirements
- Generates chart configuration

**Output**:
```json
{
  "operation_type": "chart",
  "chart_type": "bar|line|pie|column",
  "configuration": {
    "title": "Chart Title",
    "data_source": "user_specified|generated",
    "dimensions": "default"
  },
  "metadata": {
    "chart_category": "financial",
    "complexity": "simple"
  }
}
```

#### 2.4 Create Table Node

**Purpose**: Generates table specifications and structure.

**Triggered When**: Intent classifier determines `CREATE_TABLE`

**Input**:
- User table request
- Table size requirements
- Data specifications

**Process**:
- Determines table dimensions
- Analyzes data structure needs
- Generates table configuration

**Output**:
```json
{
  "operation_type": "table",
  "rows": 3,
  "columns": 4,
  "configuration": {
    "headers": ["Col1", "Col2", "Col3", "Col4"],
    "data_type": "financial|general",
    "style": "default"
  },
  "metadata": {
    "table_purpose": "data_display",
    "estimated_size": "small"
  }
}
```

### 3. Response Creator Node

**Purpose**: Creates the final human-readable response and formats the complete JSON response for the server.

**Input**:
- Operation result from one of the four operation nodes
- Original user request
- Document context

**Process**:
- Generates human-readable response explaining what was done
- Combines operation data with response message
- Formats final JSON according to data models specification
- Validates JSON structure

**Output**:
```json
{
  "type": "insert|format|table|chart",
  "response": "I've successfully [action taken] as requested. [Additional details]",
  
  // Operation-specific fields based on type
  "content": "...",           // for insert operations
  "formatting": {...},        // for format operations  
  "rows": 3, "columns": 4,   // for table operations
  "chart_type": "bar",       // for chart operations
  
  "metadata": {
    "execution_time_ms": 150,
    "confidence": 0.95,
    "operation_id": "op_12345"
  }
}
```

## Workflow Execution

### Step 1: Request Processing
1. User request received with document context
2. Input validation and preprocessing
3. Context extraction (cursor position, selected text, etc.)

### Step 2: Intent Classification
1. LLM analyzes request and context
2. Intent classified into one of four categories
3. Relevant parameters extracted
4. Route determined for next node

### Step 3: Operation Execution
1. **Single operation executed** based on intent classification:
   - Generate Content → Create text content
   - Classify Formatting → Determine formatting specs
   - Create Chart → Generate chart configuration  
   - Create Table → Generate table structure
2. Operation produces structured output
3. Validation of operation results

### Step 4: Response Generation
1. Human-readable response created
2. Operation results combined with response
3. Final JSON formatted according to data models
4. Response validation and quality checks

### Step 5: Output Delivery
1. Complete JSON response sent to server
2. Server forwards to LibreOffice AgentCoordinator
3. AgentCoordinator processes operation and displays response

## Error Handling

### Intent Classification Errors
- Fallback to default "generate content" if classification unclear
- Request clarification for ambiguous intents
- Log classification confidence scores

### Operation Execution Errors
- Graceful degradation to simpler operations
- Error message generation with suggestions
- Retry logic with modified parameters

### Response Generation Errors
- Fallback to basic success/error messages
- Ensure valid JSON structure even in error cases
- Include error details in metadata

## Data Flow Integration

### Input from LibreOffice
```json
{
  "request": "User's natural language request",
  "request_id": "unique_request_id",
  "context": {
    "cursor_position": {...},
    "selected_text": "...",
    "document_info": {...}
  }
}
```

### Output to LibreOffice
```json
{
  "type": "insert|format|table|chart",
  "response": "Human readable response",
  // ... operation-specific fields as per data_models.md
}
```

## Performance Requirements

- **Response Time**: Target 1-3 seconds for simple operations
- **Accuracy**: Intent classification >90% accuracy
- **Reliability**: Error handling for all edge cases
- **Scalability**: Support for concurrent requests

## Extension Points

The architecture supports future enhancements:

1. **Additional Operation Types**: Image insertion, style application, etc.
2. **Enhanced Context**: Document analysis, user preferences
3. **Multi-Step Operations**: Complex workflows requiring multiple operations
4. **Learning Capabilities**: User feedback integration for improved classification

## LLM Integration

### Models Used
- **Intent Classification**: Fast, lightweight model (e.g., GPT-3.5-turbo)
- **Content Generation**: High-quality model (e.g., GPT-4, Claude)
- **Response Creation**: Balanced model for consistency

### Prompt Engineering
- Structured prompts with clear examples
- Context-aware prompting with document state
- Consistent formatting for reliable parsing

## Monitoring and Logging

- Request/response logging for debugging
- Performance metrics for each node
- Intent classification accuracy tracking
- Error rate monitoring by operation type

---

This schema provides the foundation for implementing a robust, extensible AI agent orchestrator that integrates seamlessly with the LibreOffice Finance Office AI system.