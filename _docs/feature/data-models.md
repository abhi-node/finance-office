# Agent Operation Data Models & Handler Functions

## Overview

This document defines the 4 core operation types that agents will return to the LibreOffice backend and the functions that will handle each operation type. The agent system uses a simplified JSON-based approach where each operation is identified by a `type` field.

## Data Models

### Core Operation Structure

All operations share a common base structure:
```json
{
  "type": "operation_type"
  // Additional fields specific to operation type
}
```

### 1. Format Operation

**Purpose**: Apply text formatting to currently selected text

**JSON Structure**:
```json
{
  "type": "format",
  "response": "Applied bold and italic formatting to the selected text.",
  "formatting": {
    "bold": "true|false",
    "italic": "true|false", 
    "underline": "true|false",
    "strikethrough": "true|false",
    "font_size": "12",
    "color": "#000000"
  }
}
```

**Fields**:
- `type`: Always "format"
- `response`: Human readable response from the AI describing the action
- `formatting`: Object containing formatting properties
  - `bold`: Boolean as string - apply/remove bold
  - `italic`: Boolean as string - apply/remove italic
  - `underline`: Boolean as string - apply/remove underline
  - `strikethrough`: Boolean as string - apply/remove strikethrough
  - `font_size`: String number - font size in points
  - `color`: String hex color - text color

**Notes**: Operates only on currently selected text in the document.

### 2. Insert Text Operation

**Purpose**: Insert plain text at the current cursor position

**JSON Structure**:
```json
{
  "type": "insert",
  "response": "Inserted the requested text at the cursor position.",
  "content": "Text to be inserted"
}
```

**Fields**:
- `type`: Always "insert"
- `response`: Human readable response from the AI describing the action
- `content`: String containing the text to insert

**Notes**: Text is inserted at current cursor position without any formatting.

### 3. Insert Table Operation

**Purpose**: Insert a blank table at the current cursor position

**JSON Structure**:
```json
{
  "type": "table",
  "response": "Created a 3x4 table at the cursor position.",
  "rows": 3,
  "columns": 4
}
```

**Fields**:
- `type`: Always "table"
- `response`: Human readable response from the AI describing the action
- `rows`: Integer number of rows for the table
- `columns`: Integer number of columns for the table

**Notes**: Creates an empty table with the specified dimensions. Future versions may include data population and styling.

### 4. Insert Chart Operation

**Purpose**: Insert a blank chart at the current cursor position

**JSON Structure**:
```json
{
  "type": "chart",
  "response": "Inserted a bar chart at the cursor position.",
  "chart_type": "bar|line|pie|column"
}
```

**Fields**:
- `type`: Always "chart"
- `response`: Human readable response from the AI describing the action
- `chart_type`: String specifying chart type (bar, line, pie, column)

**Notes**: Creates an empty chart of the specified type. Future versions may include data population and configuration.

## Handler Functions

### executeSimplifiedOperations()

**Purpose**: Main router function that processes agent responses

**Functionality**:
- Receives the complete agent JSON response
- Parses the JSON to identify the operation type
- Routes to the appropriate handler function based on the `type` field
- Returns execution result/status

**Routing Logic**:
- `type: "format"` → calls `formatAgentText()`
- `type: "insert"` → calls `insertAgentText()`
- `type: "table"` → calls `insertAgentTable()`
- `type: "chart"` → calls `insertAgentChart()`

### formatAgentText()

**Purpose**: Handle text formatting operations

**Functionality**:
- Receives format operation JSON object
- Extracts formatting properties from the `formatting` object
- Applies formatting to currently selected text
- Supports: bold, italic, underline, strikethrough, font size, color
- Returns operation success/failure status

**Requirements**:
- Must check that text is currently selected
- Should handle individual formatting properties independently
- Must be distinct from existing formatting functions

### insertAgentText()

**Purpose**: Handle text insertion operations

**Functionality**:
- Receives insert operation JSON object
- Extracts text content from the `content` field
- Inserts text at current cursor position
- Returns operation success/failure status

**Requirements**:
- Inserts plain text without formatting
- Must be distinct from existing text insertion functions
- Should handle special characters and unicode properly

### insertAgentTable()

**Purpose**: Handle table insertion operations

**Functionality**:
- Receives table operation JSON object
- Extracts row and column counts
- Creates blank table at current cursor position
- Returns operation success/failure status

**Requirements**:
- Creates empty table with specified dimensions
- Uses default table styling
- Must be distinct from existing table creation functions
- Should handle reasonable size limits (e.g., max 50x50)

### insertAgentChart()

**Purpose**: Handle chart insertion operations

**Functionality**:
- Receives chart operation JSON object
- Extracts chart type specification
- Creates blank chart of specified type at current cursor position
- Returns operation success/failure status

**Requirements**:
- Creates empty chart with default configuration
- Supports basic chart types: bar, line, pie, column
- Must be distinct from existing chart creation functions
- Should handle unsupported chart types gracefully

## Implementation Notes

### Function Naming Convention
All agent handler functions use the prefix "Agent" to distinguish them from existing LibreOffice functions:
- `formatAgentText()` (not `formatText()`)
- `insertAgentText()` (not `insertText()`)
- `insertAgentTable()` (not `insertTable()`)
- `insertAgentChart()` (not `insertChart()`)

### Error Handling
Each handler function should:
- Validate input parameters
- Check document state (e.g., text selection for formatting)
- Return meaningful error messages
- Log operations for debugging

### Future Extensibility
The data model design allows for future enhancements:
- Additional formatting properties
- Table data population and styling
- Chart data and configuration
- New operation types (e.g., image insertion, style application)

### Integration Points
- AgentCoordinator calls `executeSimplifiedOperations()`
- DocumentOperations provides the 4 handler functions
- Each handler function operates on the current document state
- All operations work at current cursor position or selected text