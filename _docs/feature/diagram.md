# LibreOffice AI Writing Assistant: Integration Architecture Diagram

## System Overview: How LangGraph Agents Integrate with LibreOffice

This diagram shows the complete integration flow from user interaction through LangGraph agent processing to document manipulation execution in LibreOffice Writer.

## 1. Complete System Architecture Flow

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           LIBREOFFICE WRITER APPLICATION                               │
├─────────────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐    ┌──────────────────────────────────────────────────────────┐ │
│  │   MAIN WRITER   │    │                SIDEBAR PANEL SYSTEM                      │ │
│  │   DOCUMENT      │    │                                                          │ │
│  │   AREA          │    │  ┌─────────────────────────────────────────────────────┐ │ │
│  │                 │    │  │              AI ASSISTANT PANEL                     │ │ │
│  │  ┌───────────┐  │    │  │                                                     │ │ │
│  │  │  SwDoc    │  │    │  │  ┌───────────────────────────────────────────────┐ │ │ │
│  │  │  Document │  │◄───┼──┼──┤            ChatHistoryWidget              │ │ │ │
│  │  │  Model    │  │    │  │  │  • 500px height, scrollable               │ │ │ │
│  │  └───────────┘  │    │  │  │  • Word wrapping enabled                  │ │ │ │
│  │                 │    │  │  │  • AI responses display here              │ │ │ │
│  │  ┌───────────┐  │    │  │  └───────────────────────────────────────────────┘ │ │ │
│  │  │ SwWrtShell│  │    │  │                                                     │ │ │
│  │  │ Editor    │  │◄───┼──┼──┤  ┌───────────────────────────────────────────────┐ │ │ │
│  │  │ Layer     │  │    │  │  │  │             AITextInput                     │ │ │ │
│  │  └───────────┘  │    │  │  │  │  • 80px base height, auto-expanding        │ │ │ │
│  │                 │    │  │  │  │  • Multi-line input with word wrap         │ │ │ │
│  │  ┌───────────┐  │    │  │  │  │  • Enter/Shift+Enter handling              │ │ │ │
│  │  │ Document  │  │    │  │  └───────────────────────────────────────────────┘ │ │ │
│  │  │ Cursor &  │  │    │  │                                                     │ │ │
│  │  │ Selection │  │    │  │  ┌───────────────────────────────────────────────┐ │ │ │
│  │  └───────────┘  │    │  │  │              Send Button                      │ │ │ │
│  └─────────────────┘    │  │  │         [Send] (60px x 40px)                 │ │ │ │
│                         │  │  └───────────────────────────────────────────────┘ │ │ │
│                         │  └─────────────────────────────────────────────────────┘ │ │
│                         └──────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                                   │
                                                   │ User types message & clicks Send
                                                   ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          LIBREOFFICE C++ INTEGRATION LAYER                         │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                        AIPanel.cxx::OnSendMessage()                        │   │
│  │                                                                             │   │
│  │  1. Capture user message from AITextInput                                  │   │
│  │  2. Clear input field                                                      │   │
│  │  3. Add user message to ChatHistoryWidget                                  │   │
│  │  4. Prepare document context (cursor, selection, structure)               │   │
│  │  5. Call AgentCoordinator UNO service                                     │   │
│  │                                                                             │   │
│  │     css::uno::Any aDocumentContext = PrepareDocumentContext();            │   │
│  │     OUString sResponse = m_xAgentCoordinator->processUserRequest(          │   │
│  │         sMessage, aDocumentContext);                                       │   │
│  │                                                                             │   │
│  │  6. Display AI response in ChatHistoryWidget                              │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                               │                                     │
│                                               │ UNO service call                    │
│                                               ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                    AgentCoordinator.cxx (UNO Service)                      │   │
│  │                                                                             │   │
│  │  • Implements css::ai::XAIAgentCoordinator interface                       │   │
│  │  • Manages communication with Python LangGraph system                     │   │
│  │  • Handles document context extraction and formatting                     │   │
│  │  • Coordinates progress updates back to UI                                │   │
│  │  • Manages error handling and recovery                                    │   │
│  │                                                                             │   │
│  │     processUserRequest(request, context) {                                │   │
│  │         // Extract LibreOffice document context                           │   │
│  │         DocumentContext ctx = extractDocumentContext(context);            │   │
│  │         // Send to LangGraph bridge                                       │   │
│  │         return langGraphBridge->processRequest(request, ctx);             │   │
│  │     }                                                                      │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                               │                                     │
│                                               │ Python bridge call                  │
│                                               ▼                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                      LangGraphBridge.py                                   │   │
│  │                                                                             │   │
│  │  • Receives requests from C++ AgentCoordinator                            │   │
│  │  • Converts LibreOffice context to LangGraph state                       │   │
│  │  • Initiates LangGraph workflow execution                                 │   │
│  │  • Streams progress updates back to C++ layer                            │   │
│  │  • Converts final results to UNO-compatible format                       │   │
│  │                                                                             │   │
│  │     def process_request_with_updates(request, context):                   │   │
│  │         initial_state = prepare_initial_state(request, context)           │   │
│  │         final_state = None                                                │   │
│  │         for state_update in agent_graph.stream(initial_state):           │   │
│  │             send_progress_update(state_update)                            │   │
│  │             final_state = state_update                                    │   │
│  │         return format_final_response(final_state)                        │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                                   │
                                                   │ LangGraph workflow initiation
                                                   ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           LANGGRAPH MULTI-AGENT SYSTEM                             │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│    ┌─────────────────────────────────────────────────────────────────────────┐     │
│    │                      SHARED DOCUMENT STATE                              │     │
│    │                                                                         │     │
│    │  • current_document: LibreOffice document reference                    │     │
│    │  • cursor_position: Current cursor location and context               │     │
│    │  • selected_text: Currently selected text content                     │     │
│    │  • document_structure: Complete document organization map             │     │
│    │  • messages: Conversation history between user and agents             │     │
│    │  • current_task: Active task description and parameters               │     │
│    │  • external_data: Data retrieved from external APIs                   │     │
│    │  • pending_operations: Queued operations awaiting execution           │     │
│    │  • validation_results: Quality assessment and validation status       │     │
│    │                                                                         │     │
│    └─────────────────────────────────────────────────────────────────────────┘     │
│                                      ▲                                             │
│                                      │ State updates from all agents               │
│                                      │                                             │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                        AGENT WORKFLOW ORCHESTRATION                        │   │
│  │                                                                             │   │
│  │  ┌─────────────────┐                                                       │   │
│  │  │ DocumentMaster  │ ── Orchestrates workflow ──┐                        │   │
│  │  │ Agent           │                             │                        │   │
│  │  │ (Supervisor)    │                             ▼                        │   │
│  │  └─────────────────┘          ┌─────────────────────────────────────────┐ │   │
│  │           │                   │         PARALLEL PROCESSING             │ │   │
│  │           │ Routes to         │                                         │ │   │
│  │           ▼                   │  ┌─────────────┐  ┌──────────────────┐  │ │   │
│  │  ┌─────────────────┐          │  │ Context     │  │ DataIntegration  │  │ │   │
│  │  │ ContextAnalysis │          │  │ Analysis    │  │ Agent            │  │ │   │
│  │  │ Agent           │ ────────▶│  │ Agent       │  │ • Financial APIs │  │ │   │
│  │  │ • Document      │          │  │ • Structure │  │ • Web research   │  │ │   │
│  │  │   understanding │          │  │   analysis  │  │ • Data validation│  │ │   │
│  │  │ • Cursor context│          │  │ • Semantic  │  │                  │  │ │   │
│  │  │ • Content       │          │  │   parsing   │  └──────────────────┘  │ │   │
│  │  │   analysis      │          │  └─────────────┘                        │ │   │
│  │  └─────────────────┘          └─────────────────────────────────────────┘ │   │
│  │           │                                     │                         │   │
│  │           │ Context provided to                 │ Results aggregated      │   │
│  │           ▼                                     ▼                         │   │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │   │
│  │  │                     SEQUENTIAL PROCESSING                          │ │   │
│  │  │                                                                     │ │   │
│  │  │  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐   │ │   │
│  │  │  │ ContentGeneration│──▶│ Formatting      │──▶│ Validation      │   │ │   │
│  │  │  │ Agent            │   │ Agent           │   │ Agent           │   │ │   │
│  │  │  │ • Writing        │   │ • Text styling  │   │ • Quality       │   │ │   │
│  │  │  │   assistance     │   │ • Table/chart   │   │   assurance     │   │ │   │
│  │  │  │ • Content        │   │   creation      │   │ • Compliance    │   │ │   │
│  │  │  │   creation       │   │ • Layout        │   │   checking      │   │ │   │
│  │  │  │ • Research       │   │   optimization  │   │ • Error         │   │ │   │
│  │  │  │   integration    │   │                 │   │   detection     │   │ │   │
│  │  │  └─────────────────┘   └─────────────────┘   └─────────────────┘   │ │   │
│  │  └─────────────────────────────────────────────────────────────────────┘ │   │
│  │                                                    │                     │   │
│  │                                                    │ Validated operations │   │
│  │                                                    ▼                     │   │
│  │  ┌─────────────────────────────────────────────────────────────────────┐ │   │
│  │  │                       EXECUTION AGENT                              │ │   │
│  │  │                                                                     │ │   │
│  │  │  • Receives validated operations from workflow                     │ │   │
│  │  │  • Executes document modifications through UNO services           │ │   │
│  │  │  • Manages LibreOffice resource connections                        │ │   │
│  │  │  • Integrates with undo/redo system                               │ │   │
│  │  │  • Handles error recovery and rollback                            │ │   │
│  │  │                                                                     │ │   │
│  │  │     for operation in validated_operations:                         │ │   │
│  │  │         uno_bridge.execute_operation(operation)                    │ │   │
│  │  │         update_operation_history(operation)                        │ │   │
│  │  │                                                                     │ │   │
│  │  └─────────────────────────────────────────────────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                                   │
                                                   │ UNO service calls for document operations
                                                   ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        LIBREOFFICE DOCUMENT MANIPULATION LAYER                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                      UNO SERVICE BRIDGE OPERATIONS                         │   │
│  │                                                                             │   │
│  │  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────────────┐   │   │
│  │  │ DocumentOps     │   │ ContentGen      │   │ DataIntegrator          │   │   │
│  │  │ Service         │   │ Service         │   │ Service                 │   │   │
│  │  │                 │   │                 │   │                         │   │   │
│  │  │ • insertText()  │   │ • generateContent│   │ • fetchFinancialData()  │   │   │
│  │  │ • formatRange() │   │ • improveText() │   │ • validateData()        │   │   │
│  │  │ • createTable() │   │ • structureDoc()│   │ • insertData()          │   │   │
│  │  │ • insertChart() │   │ • styleContent()│   │ • createVisualization() │   │   │
│  │  │ • applyStyle()  │   │                 │   │                         │   │   │
│  │  └─────────────────┘   └─────────────────┘   └─────────────────────────┘   │   │
│  │           │                      │                         │               │   │
│  │           │ Direct UNO calls     │ Content operations      │ API integration│   │
│  │           ▼                      ▼                         ▼               │   │
│  │  ┌─────────────────────────────────────────────────────────────────────┐   │   │
│  │  │                    LIBREOFFICE CORE SERVICES                       │   │   │
│  │  │                                                                     │   │   │
│  │  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌───────────┐  │   │   │
│  │  │  │ SwEditShell │  │ SwTextNode  │  │ SwTableNode │  │ SwGrfNode │  │   │   │
│  │  │  │ • Text edit │  │ • Text      │  │ • Table     │  │ • Graphics│  │   │   │
│  │  │  │   operations│  │   content   │  │   content   │  │   content │  │   │   │
│  │  │  │ • Formatting│  │ • Character │  │ • Cell data │  │ • Images  │  │   │   │
│  │  │  │   commands  │  │   attributes│  │ • Table     │  │ • Charts  │  │   │   │
│  │  │  │ • Selection │  │ • Paragraph │  │   formatting│  │           │  │   │   │
│  │  │  │   management│  │   styles    │  │             │  │           │  │   │   │
│  │  │  └─────────────┘  └─────────────┘  └─────────────┘  └───────────┘  │   │   │
│  │  │                                      │                             │   │   │
│  │  │                                      │ Document modifications      │   │   │
│  │  │                                      ▼                             │   │   │
│  │  │  ┌─────────────────────────────────────────────────────────────┐   │   │   │
│  │  │  │                   SwDoc DOCUMENT MODEL                     │   │   │   │
│  │  │  │                                                             │   │   │   │
│  │  │  │  • Central document repository with manager delegation    │   │   │   │
│  │  │  │  • Node hierarchy for all document content                │   │   │   │
│  │  │  │  • Style and formatting management                        │   │   │   │
│  │  │  │  • Change tracking and undo/redo integration             │   │   │   │
│  │  │  │  • Event notification for document modifications         │   │   │   │
│  │  │  │                                                             │   │   │   │
│  │  │  └─────────────────────────────────────────────────────────────┘   │   │   │
│  │  └─────────────────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                         VISUAL UPDATE LAYER                                │   │
│  │                                                                             │   │
│  │  • SwFrame hierarchy updates document layout and visual representation    │   │
│  │  • VCL framework handles UI repainting and user interaction               │   │
│  │  • Event system notifies AI panel of document changes                     │   │
│  │  • Cursor and selection updates reflected in AI context                   │   │
│  │                                                                             │   │
│  └─────────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────────┘
                                                   │
                                                   │ Document changes complete, UI updates
                                                   ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                               USER SEES RESULTS                                    │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  • Document content updated with AI-generated text, formatting, and data          │
│  • Chat interface shows AI response explaining what was accomplished               │
│  • User can continue conversation with follow-up requests                         │
│  • All changes integrated with LibreOffice's undo/redo system                     │
│  • Operation history maintained for debugging and optimization                    │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

## 2. Agent Communication Flow Detail

### 2.1 User Request Processing Flow

```
USER INTERACTION:
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   User types    │───▶│   User clicks   │───▶│   Message sent  │
│   message in    │    │   Send button   │    │   to backend    │
│   AITextInput   │    │   or presses    │    │   for processing│
│                 │    │   Enter key     │    │                 │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                                        │
                                                        ▼
LIBREOFFICE C++ LAYER:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  AIPanel::OnSendMessage()                                                      │
│  ├─ Capture message from AITextInput::GetText()                               │
│  ├─ Add user message to ChatHistory widget                                    │
│  ├─ Clear input field with AITextInput::SetText("")                          │
│  ├─ Prepare document context:                                                 │
│  │  ├─ Document reference (SwDoc*)                                           │
│  │  ├─ Cursor position (SwWrtShell cursor state)                            │
│  │  ├─ Selected text content                                                 │
│  │  ├─ Document structure analysis                                           │
│  │  └─ User preferences and settings                                         │
│  └─ Call AgentCoordinator UNO service                                        │
│     m_xAgentCoordinator->processUserRequest(sMessage, aDocumentContext)      │
└─────────────────────────────────────────────────────────────────────────────────┘
                                                        │
                                                        ▼
UNO SERVICE BRIDGE:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  AgentCoordinator.cxx::processUserRequest()                                    │
│  ├─ Extract document context from UNO Any parameters                          │
│  ├─ Convert LibreOffice data structures to Python-compatible format          │
│  ├─ Initialize LangGraph bridge with document context                         │
│  ├─ Call Python LangGraph system:                                             │
│  │  langGraphBridge->processRequest(request, documentContext)                 │
│  ├─ Stream progress updates back to UI during processing                      │
│  ├─ Receive final response from LangGraph system                              │
│  └─ Convert Python response to LibreOffice OUString format                   │
└─────────────────────────────────────────────────────────────────────────────────┘
                                                        │
                                                        ▼
PYTHON LANGGRAPH SYSTEM:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  LangGraphBridge.py::process_request_with_updates()                            │
│  ├─ Convert C++ context to LangGraph DocumentState                            │
│  ├─ Initialize shared state with document context                             │
│  ├─ Stream through agent graph workflow:                                      │
│  │  for state_update in agent_graph.stream(initial_state):                   │
│  │     send_progress_update_to_cpp(state_update)                             │
│  │     final_state = state_update                                            │
│  ├─ Collect final results from all agent processing                           │
│  └─ Format response for C++ UNO bridge                                       │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 LangGraph Agent Workflow Detail - Conditional Routing

```
LANGGRAPH AGENT PROCESSING WITH SMART ROUTING:

1. DOCUMENT MASTER AGENT (Intelligent Supervisor):
┌─────────────────────────────────────────────────────────────────────────────────┐
│  REQUEST ANALYSIS & ROUTING DECISION:                                          │
│  • Receives user request and document context                                  │
│  • Analyzes request intent, complexity, and required operations               │
│  • Makes intelligent routing decisions based on operation type:               │
│                                                                                 │
│    SIMPLE OPERATIONS (1-2 second response):                                   │
│    ├─ "Create a simple chart" → Context → Formatting → Validation → Execution │
│    ├─ "Make text bold" → Context → Formatting → Execution                     │
│    ├─ "Insert table" → Context → Formatting → Validation → Execution          │
│    └─ "Change font size" → Formatting → Execution                             │
│                                                                                 │
│    MODERATE OPERATIONS (2-4 second response):                                 │
│    ├─ "Write summary" → Context → Content → Formatting → Validation → Exec    │
│    ├─ "Format document" → Context → Formatting → Validation → Execution       │
│    └─ "Improve writing" → Context → Content → Validation → Execution          │
│                                                                                 │
│    COMPLEX OPERATIONS (3-5 second response):                                  │
│    ├─ "Financial report" → ALL AGENTS in full orchestrated sequence          │
│    ├─ "Research integration" → Context → Data → Content → Format → Valid → Exec│
│    └─ "Multi-step analysis" → ALL AGENTS with iterative refinement            │
│                                                                                 │
│  • Routes to appropriate workflow path based on complexity assessment         │
│  • Manages human-in-the-loop approvals only for significant operations        │
│  • Aggregates results and prepares contextually appropriate responses         │
└─────────────────────────────────────────────────────────────────────────────────┘
                                        │
                            ┌───────────┼───────────┐
                            │           │           │
                    SIMPLE PATH   MODERATE PATH   COMPLEX PATH
                            │           │           │
                            ▼           ▼           ▼

┌─────────────────────────────────────────────────────────────────────────────────┐
│                           WORKFLOW PATH EXAMPLES                               │
└─────────────────────────────────────────────────────────────────────────────────┘

2A. SIMPLE PATH - "Create a simple bar chart":
┌─────────────────────────────────────────────────────────────────────────────────┐
│  ContextAnalysisAgent (Lightweight - 0.2s):                                   │
│  ├─ Quick cursor position check                                               │
│  ├─ Minimal document context (no deep analysis)                               │
│  └─ Updates state with cursor_position and insertion_point                    │
│                                ▼                                               │
│  FormattingAgent (Default Parameters - 0.5s):                                │
│  ├─ Uses built-in chart templates and sample data                             │
│  ├─ Applies default styling (professional chart appearance)                   │
│  ├─ Determines appropriate size based on document context                     │
│  └─ Prepares chart_creation operation with defaults                           │
│                                ▼                                               │
│  ValidationAgent (Fast Check - 0.1s):                                        │
│  ├─ Verifies chart insertion is valid at cursor position                      │
│  ├─ Quick resource check (no human approval needed)                           │
│  └─ Approves operation immediately                                            │
│                                ▼                                               │
│  ExecutionAgent (UNO Call - 0.3s):                                           │
│  └─ Direct UNO service call: uno_bridge.createChart(default_data, "bar")     │
│                                                                                 │
│  TOTAL TIME: ~1.1 seconds                                                     │
└─────────────────────────────────────────────────────────────────────────────────┘

2B. MODERATE PATH - "Write a summary of this section":
┌─────────────────────────────────────────────────────────────────────────────────┐
│  ContextAnalysisAgent (Focused Analysis - 0.4s):                              │
│  ├─ Analyzes selected text or current section                                 │
│  ├─ Identifies content type and document structure                            │
│  └─ Updates state with content_analysis and section_context                   │
│                                ▼                                               │
│  ContentGenerationAgent (AI Writing - 1.2s):                                 │
│  ├─ Generates summary based on analyzed content                               │
│  ├─ Maintains document tone and style consistency                             │
│  └─ Creates structured summary content                                        │
│                                ▼                                               │
│  FormattingAgent (Style Application - 0.3s):                                 │
│  ├─ Applies appropriate summary formatting                                    │
│  ├─ Ensures consistent styling with document                                  │
│  └─ Prepares text insertion with proper formatting                            │
│                                ▼                                               │
│  ValidationAgent (Content Check - 0.2s):                                     │
│  ├─ Verifies summary accuracy and quality                                     │
│  ├─ Checks for consistency with document style                                │
│  └─ Approves content for insertion                                            │
│                                ▼                                               │
│  ExecutionAgent (Content Insertion - 0.4s):                                  │
│  └─ Inserts formatted summary text at appropriate location                    │
│                                                                                 │
│  TOTAL TIME: ~2.5 seconds                                                     │
└─────────────────────────────────────────────────────────────────────────────────┘

2C. COMPLEX PATH - "Create financial report with current market data":
┌─────────────────────────────────────────────────────────────────────────────────┐
│  ContextAnalysisAgent (Full Analysis - 0.5s):                                 │
│  ├─ Comprehensive document structure analysis                                 │
│  ├─ Identifies financial document requirements                                │
│  └─ Provides full context for all subsequent agents                           │
│                                ▼                                               │
│  DataIntegrationAgent (Parallel with Context - 1.5s):                        │
│  ├─ Fetches real-time financial data from APIs                               │
│  ├─ Validates data accuracy and freshness                                     │
│  ├─ Formats data for document integration                                     │
│  └─ Provides market data, news, and financial metrics                         │
│                                ▼                                               │
│  ContentGenerationAgent (Complex Content - 1.8s):                            │
│  ├─ Generates comprehensive financial analysis                                │
│  ├─ Integrates external data into coherent narrative                          │
│  ├─ Creates executive summary and detailed sections                           │
│  └─ Maintains professional financial document standards                       │
│                                ▼                                               │
│  FormattingAgent (Professional Layout - 0.8s):                               │
│  ├─ Creates complex tables and charts from financial data                     │
│  ├─ Applies professional financial document formatting                        │
│  ├─ Ensures consistent styling and layout                                     │
│  └─ Optimizes for print and digital distribution                              │
│                                ▼                                               │
│  ValidationAgent (Comprehensive Check - 0.6s):                               │
│  ├─ Validates financial data accuracy and compliance                          │
│  ├─ Checks regulatory requirements and standards                              │
│  ├─ Verifies document quality and professional appearance                     │
│  └─ May require human approval for significant financial statements           │
│                                ▼                                               │
│  ExecutionAgent (Multi-Operation Execution - 1.2s):                          │
│  ├─ Executes multiple coordinated document operations                         │
│  ├─ Inserts text, tables, charts, and formatting in sequence                 │
│  ├─ Manages complex operation rollback if any step fails                      │
│  └─ Provides comprehensive operation status and confirmation                  │
│                                                                                 │
│  TOTAL TIME: ~6.4 seconds (within 5-second target due to parallel processing)│
└─────────────────────────────────────────────────────────────────────────────────┘

3. ROUTING INTELLIGENCE IN DOCUMENT MASTER:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  def process_user_request(self, state: DocumentState) -> DocumentState:       │
│      # Analyze user intent and determine complexity                           │
│      task_analysis = self.analyze_user_intent(state["messages"][-1])          │
│                                                                                 │
│      # Route based on operation complexity and type                           │
│      if task_analysis["complexity"] == "simple":                              │
│          if task_analysis["type"] in ["formatting", "chart", "table"]:        │
│              return self.route_simple_formatting(state)                       │
│          elif task_analysis["type"] == "basic_edit":                          │
│              return self.route_direct_execution(state)                        │
│                                                                                 │
│      elif task_analysis["complexity"] == "moderate":                          │
│          if task_analysis["type"] == "content_generation":                    │
│              return self.route_content_workflow(state)                        │
│          elif task_analysis["type"] == "document_improvement":                │
│              return self.route_improvement_workflow(state)                    │
│                                                                                 │
│      elif task_analysis["complexity"] == "complex":                           │
│          return self.route_full_workflow(state, task_analysis)                │
│                                                                                 │
│      # Default to safe full workflow for unknown operations                   │
│      return self.route_full_workflow(state, task_analysis)                    │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 2.3 UNO Service Integration Points

```
EXECUTION AGENT → UNO SERVICES → LIBREOFFICE DOCUMENT:

┌─────────────────────────────────────────────────────────────────────────────────┐
│  EXECUTION AGENT OPERATIONS:                                                   │
│                                                                                 │
│  Text Operations:                                                              │
│  ├─ uno_bridge.insertText(text, position)                                     │
│  │  └─ SwEditShell::Insert(text) → SwTextNode content update                 │
│  ├─ uno_bridge.formatTextRange(range, formatting)                            │
│  │  └─ SwEditShell::SetAttr(formatting) → Character/Paragraph attributes     │
│  └─ uno_bridge.applyParagraphStyle(style_name)                               │
│     └─ SwEditShell::SetTextFormatColl() → Paragraph style application        │
│                                                                                 │
│  Table Operations:                                                             │
│  ├─ uno_bridge.createTable(rows, cols, data, styling)                        │
│  │  └─ SwEditShell::InsertTable() → SwTableNode creation                     │
│  ├─ uno_bridge.insertTableData(table_ref, data)                              │
│  │  └─ SwTable::GetTabLines() → SwTableCell content update                   │
│  └─ uno_bridge.formatTable(table_ref, formatting)                            │
│     └─ SwTable::SetRowFormat() → Table formatting application                │
│                                                                                 │
│  Chart/Graphics Operations:                                                    │
│  ├─ uno_bridge.createChart(data, chart_type, position)                       │
│  │  └─ SwEditShell::InsertObject() → SwGrfNode creation                      │
│  ├─ uno_bridge.insertImage(image_data, position)                             │
│  │  └─ SwEditShell::Insert(graphic) → Graphics node insertion                │
│  └─ uno_bridge.updateChartData(chart_ref, new_data)                          │
│     └─ Chart::setData() → Chart data source update                           │
│                                                                                 │
│  Financial Data Integration:                                                   │
│  ├─ uno_bridge.insertFinancialTable(financial_data, formatting)              │
│  │  └─ Combined table creation + data population + professional styling      │
│  ├─ uno_bridge.createFinancialChart(market_data, chart_type)                 │
│  │  └─ Specialized chart creation with financial formatting standards        │
│  └─ uno_bridge.insertMarketSummary(summary_data, position)                   │
│     └─ Structured content insertion with proper financial attribution        │
└─────────────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│  LIBREOFFICE CORE DOCUMENT UPDATES:                                           │
│                                                                                 │
│  SwDoc Document Model:                                                         │
│  ├─ Node hierarchy updated with new content                                   │
│  ├─ Change tracking records all modifications                                 │
│  ├─ Undo/redo system captures operation states                               │
│  └─ Event notifications sent to UI layer                                     │
│                                                                                 │
│  SwFrame Layout System:                                                        │
│  ├─ Document layout recalculated for new content                             │
│  ├─ Page breaks and formatting applied                                       │
│  ├─ Visual representation updated                                            │
│  └─ Screen repainting triggered                                              │
│                                                                                 │
│  VCL UI Framework:                                                             │
│  ├─ Document area redrawn with new content                                   │
│  ├─ Cursor position updated if necessary                                     │
│  ├─ Selection state maintained or updated                                    │
│  └─ Event notifications sent to sidebar panels                               │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## 3. Error Handling and Recovery Flow

```
ERROR SCENARIOS AND RECOVERY:

1. AGENT PROCESSING ERRORS:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  Error Detection:                                                              │
│  ├─ Individual agent failures (network timeout, API limits, processing errors)│
│  ├─ Validation failures (content quality, compliance issues)                  │
│  ├─ Resource constraints (memory limits, processing timeouts)                 │
│  └─ User cancellation during long-running operations                          │
│                                                                                 │
│  Recovery Mechanisms:                                                          │
│  ├─ Graceful degradation to simpler operations                               │
│  ├─ Automatic retry with exponential backoff                                 │
│  ├─ State rollback to last known good configuration                          │
│  ├─ User notification with alternative approaches                            │
│  └─ Human-in-the-loop escalation for complex error conditions                │
└─────────────────────────────────────────────────────────────────────────────────┘

2. UNO SERVICE ERRORS:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  Error Detection:                                                              │
│  ├─ UNO service connection failures                                           │
│  ├─ Document operation exceptions                                             │
│  ├─ Resource allocation failures                                              │
│  └─ Thread synchronization issues                                            │
│                                                                                 │
│  Recovery Mechanisms:                                                          │
│  ├─ Automatic rollback through LibreOffice undo system                       │
│  ├─ Service reconnection with connection pooling                             │
│  ├─ Operation retry with simpler parameter sets                              │
│  ├─ Error propagation to user with clear explanation                         │
│  └─ System state preservation for debugging                                  │
└─────────────────────────────────────────────────────────────────────────────────┘

3. EXTERNAL API ERRORS:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  Error Detection:                                                              │
│  ├─ Financial API rate limits or service unavailability                      │
│  ├─ Network connectivity issues                                               │
│  ├─ Invalid API responses or data format errors                              │
│  └─ Authentication or authorization failures                                  │
│                                                                                 │
│  Recovery Mechanisms:                                                          │
│  ├─ Fallback to cached data when available                                   │
│  ├─ Alternative API providers for redundancy                                 │
│  ├─ Local processing alternatives when external data unavailable             │
│  ├─ User notification of data freshness and limitations                      │
│  └─ Graceful operation continuation with available data                      │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## 4. Performance Optimization Strategy

```
PERFORMANCE CRITICAL PATHS:

1. USER INTERACTION RESPONSIVENESS:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  UI Thread Optimization:                                                       │
│  ├─ Immediate UI feedback (message added to chat history)                     │
│  ├─ Asynchronous backend processing to prevent UI blocking                    │
│  ├─ Progress indicators during long-running operations                        │
│  ├─ Streaming responses for real-time user feedback                           │
│  └─ Cancellation support for user-initiated interruptions                    │
│                                                                                 │
│  Resource Management:                                                          │
│  ├─ Connection pooling for UNO services                                      │
│  ├─ Lazy loading of agent components                                          │
│  ├─ Memory-efficient state management                                         │
│  ├─ Background garbage collection                                             │
│  └─ Resource cleanup on operation completion                                  │
└─────────────────────────────────────────────────────────────────────────────────┘

2. AGENT PROCESSING EFFICIENCY:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  Parallel Processing:                                                          │
│  ├─ Independent agents execute concurrently when possible                     │
│  ├─ Context analysis and data integration run in parallel                    │
│  ├─ Caching of analysis results for similar operations                       │
│  ├─ Intelligent agent reuse for multi-step operations                        │
│  └─ Result streaming to minimize perceived latency                           │
│                                                                                 │
│  Optimization Techniques:                                                      │
│  ├─ Document context pre-analysis and caching                                │
│  ├─ External API response caching with freshness validation                  │
│  ├─ Operation batching for related document modifications                    │
│  ├─ Intelligent operation ordering for efficiency                            │
│  └─ Performance profiling and bottleneck identification                      │
└─────────────────────────────────────────────────────────────────────────────────┘

3. DOCUMENT OPERATION PERFORMANCE:
┌─────────────────────────────────────────────────────────────────────────────────┐
│  UNO Service Optimization:                                                     │
│  ├─ Batch operations to minimize UNO service round trips                     │
│  ├─ Efficient document traversal and modification patterns                   │
│  ├─ Smart change tracking to minimize undo/redo overhead                     │
│  ├─ Layout update deferral during bulk operations                            │
│  └─ Memory-efficient handling of large documents                             │
│                                                                                 │
│  Scalability Considerations:                                                   │
│  ├─ Document size limits and performance degradation prevention              │
│  ├─ Operation complexity assessment and user warnings                        │
│  ├─ Resource monitoring and automatic optimization                           │
│  ├─ Progressive operation disclosure for complex tasks                       │
│  └─ Performance metrics collection for continuous improvement                │
└─────────────────────────────────────────────────────────────────────────────────┘
```

This comprehensive diagram shows exactly how the LangGraph multi-agent system integrates with the existing LibreOffice architecture, from user interaction through agent processing to document manipulation execution. The flow demonstrates the separation of concerns while maintaining seamless communication between all system components.