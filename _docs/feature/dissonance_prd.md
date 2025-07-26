# LibreOffice AI Agent Integration: Production Ready Implementation

## Clear Objective

**Objective**: Complete the production-ready integration of LibreOffice AI Writing Assistant with LangGraph multi-agent system, enabling users to send natural language requests that result in both conversational responses and actual document modifications.

**Success Criteria**:
- Users can type AI requests in LibreOffice Writer sidebar chat
- AI agents analyze document context and generate contextually appropriate responses
- Document operations (text insertion, formatting, tables, etc.) are executed automatically
- Users see both chat responses and immediate document changes
- All operations are undoable and integrated with LibreOffice's native undo system
- Response times: Simple operations <2s, Moderate <4s, Complex <5s

## Context

### What's Already Built and Working

#### **Complete Infrastructure Components**
1. **AIPanel UI (`sw/source/ui/sidebar/ai/AIPanel.cxx`)**
   - Full chat interface with message queuing and display
   - User input validation and sanitization
   - Message state management and progress indicators
   - Background processing framework
   - **Status**: ✅ Complete and functional

2. **AgentCoordinator Framework (`sw/source/core/ai/AgentCoordinator.cxx`)**  
   - Complete UNO service with request routing (simple/moderate/complex)
   - Operation translation framework from JSON to UNO operations
   - Operation execution framework calling DocumentOperations service
   - Enhanced JSON response parsing with separate content and operations
   - **Status**: ✅ Framework complete, 4 implementation gaps

3. **Python Agent System (`langgraph-agents/`)**
   - Fully functional multi-agent system (DocumentMaster, Content, Research, Formatting, Execution)
   - Complete agent workflow with state management
   - FastAPI server with `/api/simple`, `/api/moderate`, `/api/complex` endpoints
   - Enhanced response format with separate `operations` and `response_content` fields
   - **Status**: ✅ Complete and tested

4. **DocumentOperations Service (`sw/source/core/ai/operations/DocumentOperations.cxx`)**
   - Complete UNO interface definitions for all operation types
   - Core text operations implemented (`insertText`, `formatText`)
   - SwWrtShell/SwEditShell integration for direct document manipulation
   - Undo/redo integration and operation tracking
   - **Status**: ✅ Core operations complete, some operations need verification

5. **Service Registration (`sw/util/sw.component`)**
   - Both AgentCoordinator and DocumentOperations services registered
   - UNO service factory implementations complete
   - **Status**: ✅ Complete

### Current Integration Flow (Partially Working)

```
User types "Make this text bold" in AIPanel
         ↓ ✅ WORKING
AIPanel.OnSendMessage() → queueMessage() → sendMessageToBackend()
         ↓ ✅ WORKING  
AIPanel.prepareDocumentContext() → creates basic PropertyValue sequence
         ↓ ✅ WORKING
AgentCoordinator.processUserRequest(message, context)
         ↓ ❌ GAP 1: extractDocumentContext() returns context unchanged
AgentCoordinator.extractDocumentContext() → should serialize to JSON
         ↓ ❌ GAP 2: HTTP client incomplete
NetworkClient.postJson() → HTTP POST to localhost:8000/api/moderate  
         ↓ ✅ WORKING
Python agents process request and return JSON with operations + content
         ↓ ✅ WORKING
AgentCoordinator.parseEnhancedJsonResponse() → extracts operations and content
         ↓ ✅ WORKING  
AgentCoordinator.translateOperationsToUno() → converts to C++ operations
         ↓ ✅ WORKING
AgentCoordinator.executeTranslatedOperations() → calls DocumentOperations
         ↓ ✅ WORKING (for implemented operations)
DocumentOperations.insertText()/formatText() → real UNO operations
         ↓ ✅ WORKING
Document is modified, operation recorded for undo
         ↓ ✅ WORKING
AIPanel.handleBackendResponse() → displays chat response + operation confirmations
```

### Architecture Advantages

#### **No PyUNO Dependency Required**
- Document context extraction: C++ native UNO APIs (`SwWrtShell`, `SwDoc`)
- Operation execution: C++ native UNO APIs (`SwWrtShell`, `SwEditShell`)  
- Python agents: Receive JSON context, return JSON operations
- Deployment: Standard LibreOffice build, no Python bridge configuration

#### **Clear Separation of Concerns**
- **C++ Layer**: UI, document access, context extraction, operation execution
- **Python Layer**: AI processing, content generation, operation planning
- **HTTP Interface**: Clean JSON communication between layers

## Constraints

### Technical Constraints

#### **LibreOffice Integration Requirements**
- Must use UNO service architecture for C++ components
- Must integrate with SwWrtShell/SwEditShell for Writer operations
- Must support LibreOffice's undo/redo system
- Must be thread-safe and handle concurrent operations
- Must follow LibreOffice coding standards and error handling patterns

#### **Network Communication Requirements**
- HTTP client must use LibreOffice's Universal Content Broker (UCB) system
- Must handle network failures, timeouts, and malformed responses gracefully
- Must support LibreOffice proxy settings and security policies
- JSON serialization must be compatible with Python agent system

#### **Performance Requirements**
- Simple operations must complete in <2 seconds
- Moderate operations must complete in <4 seconds  
- Complex operations must complete in <5 seconds
- Context extraction must not significantly impact UI responsiveness
- HTTP requests must be asynchronous to prevent UI blocking

#### **Deployment Constraints**
- Must work with standard LibreOffice installation
- Python agent server runs separately (localhost:8000)
- No additional Python dependencies in LibreOffice build
- Must be compatible with existing LibreOffice extension system

### Business Constraints

#### **User Experience Requirements**
- Chat interface must feel responsive and natural
- Document changes must be immediate and visible
- Error messages must be user-friendly and actionable
- All operations must be undoable through standard Ctrl+Z
- Must handle edge cases gracefully (empty documents, complex formatting)

#### **Reliability Requirements**
- Must handle network failures without crashing LibreOffice
- Must recover gracefully from agent server downtime
- Must validate all user input and agent responses
- Must prevent malicious operations or code injection

## Reasoning

### Why This Implementation Approach

#### **C++ Context Extraction vs PyUNO**
We extract document context in C++ rather than using PyUNO because:
- **Performance**: Direct SwWrtShell access is faster than Python bridge
- **Simplicity**: No PyUNO installation or configuration required
- **Reliability**: Eliminates Python bridge failure points
- **Maintenance**: Standard LibreOffice C++ patterns, easier to maintain

#### **HTTP Communication vs Direct Python Integration**
We use HTTP communication rather than embedded Python because:
- **Scalability**: Python agents can run on separate servers for load balancing
- **Development**: Python agent development independent of LibreOffice build
- **Deployment**: Agents can be updated without rebuilding LibreOffice
- **Testing**: Easier to test agents in isolation with mock data

#### **JSON Serialization vs UNO Objects**
We serialize context to JSON rather than passing UNO objects because:
- **Language Independence**: Python agents don't need UNO knowledge
- **Network Transport**: JSON travels cleanly over HTTP
- **Testing**: Easy to create mock contexts for agent testing
- **Debugging**: Human-readable request/response format

## Detailed Implementation Flow

### Phase 1: Core Communication Infrastructure (Days 1-2)

#### **Step 1.1: Complete NetworkClient HTTP Implementation**
**File**: `sw/source/core/ai/NetworkClient.cxx`
**Current State**: Interface exists, implementation incomplete

```cpp
// NetworkClient.cxx - Complete UCB-based HTTP implementation
class NetworkClient {
private:
    Reference<XComponentContext> m_xContext;
    
public:
    HttpResponse postJson(const OUString& rsUrl, const OUString& rsJsonBody, 
                         const std::map<OUString, OUString>& rHeaders) {
        HttpResponse aResponse;
        
        try {
            // Phase 1.1.1: Create UCB content for URL
            Reference<XContentProvider> xProvider = getContentProvider();
            Reference<XContent> xContent = xProvider->queryContent(
                Reference<XContentIdentifier>(createIdentifier(rsUrl), UNO_QUERY));
            
            // Phase 1.1.2: Prepare POST command with JSON body
            PostCommandArgument2 aPostArg;
            aPostArg.Source = createInputStreamFromString(rsJsonBody);
            aPostArg.Sink = Reference<XActiveDataSink>(createMemoryStream(), UNO_QUERY);
            
            // Phase 1.1.3: Set HTTP headers
            Sequence<PropertyValue> aProperties(rHeaders.size() + 2);
            aProperties[0].Name = "Content-Type";
            aProperties[0].Value <<= OUString("application/json");
            aProperties[1].Name = "Accept";
            aProperties[1].Value <<= OUString("application/json");
            
            // Add custom headers
            size_t nIndex = 2;
            for (const auto& header : rHeaders) {
                aProperties[nIndex].Name = header.first;
                aProperties[nIndex].Value <<= header.second;
                ++nIndex;
            }
            
            // Phase 1.1.4: Execute POST command
            Command aCommand;
            aCommand.Name = "post";
            aCommand.Handle = -1;
            aCommand.Argument <<= aPostArg;
            
            Any aResult = xContent->execute(aCommand, 0, createEnvironment(aProperties));
            
            // Phase 1.1.5: Extract response
            aResponse.bSuccess = true;
            aResponse.nStatusCode = 200; // UCB doesn't provide status codes directly
            aResponse.sBody = extractStringFromStream(aPostArg.Sink);
            
            return aResponse;
        }
        catch (const ContentCreationException& e) {
            aResponse.bSuccess = false;
            aResponse.sErrorMessage = "Failed to create content: " + e.Message;
        }
        catch (const CommandAbortedException& e) {
            aResponse.bSuccess = false;
            aResponse.sErrorMessage = "Request aborted: " + e.Message;
        }
        catch (const Exception& e) {
            aResponse.bSuccess = false;
            aResponse.sErrorMessage = "HTTP request failed: " + e.Message;
        }
        
        return aResponse;
    }
    
private:
    Reference<XInputStream> createInputStreamFromString(const OUString& rsString) {
        // Convert OUString to UTF-8 byte stream
        OString aUtf8String = OUStringToOString(rsString, RTL_TEXTENCODING_UTF8);
        Reference<XInputStream> xStream = new SequenceInputStream(
            Sequence<sal_Int8>(reinterpret_cast<const sal_Int8*>(aUtf8String.getStr()), 
                              aUtf8String.getLength()));
        return xStream;
    }
    
    OUString extractStringFromStream(const Reference<XActiveDataSink>& xSink) {
        Reference<XInputStream> xInputStream(xSink->getInputStream(), UNO_QUERY);
        if (!xInputStream.is()) return OUString();
        
        // Read entire stream into string
        Sequence<sal_Int8> aData;
        sal_Int32 nBytesRead = xInputStream->readBytes(aData, 65536);
        
        OString aUtf8Response(reinterpret_cast<const char*>(aData.getConstArray()), nBytesRead);
        return OStringToOUString(aUtf8Response, RTL_TEXTENCODING_UTF8);
    }
};
```

**Testing Phase 1.1**:
```cpp
// Test HTTP client with simple echo server
NetworkClient client(xContext);
std::map<OUString, OUString> headers;
headers["X-Test"] = "true";
HttpResponse response = client.postJson("http://localhost:8000/api/simple", 
                                       "{\"test\": \"message\"}", headers);
assert(response.bSuccess);
assert(response.sBody.indexOf("test") >= 0);
```

#### **Step 1.2: Basic Document Context Extraction**
**File**: `sw/source/core/ai/AgentCoordinator.cxx:602-607`
**Current State**: Stub returning input unchanged

```cpp
// AgentCoordinator.cxx - Basic JSON context serialization
Any AgentCoordinator::extractDocumentContext(const Any& rContext) const {
    try {
        // Phase 1.2.1: Extract PropertyValue sequence from AIPanel
        Sequence<PropertyValue> aContextProps;
        if (!(rContext >>= aContextProps)) {
            SAL_WARN("sw.ai", "Context is not a PropertyValue sequence");
            return Any(OUString("{}"));
        }
        
        // Phase 1.2.2: Extract document and frame references
        Reference<XTextDocument> xTextDoc;
        Reference<XFrame> xFrame;
        sal_Int64 nTimestamp = 0;
        
        for (const auto& prop : aContextProps) {
            if (prop.Name == "Document") {
                prop.Value >>= xTextDoc;
            } else if (prop.Name == "Frame") {
                prop.Value >>= xFrame;
            } else if (prop.Name == "Timestamp") {
                prop.Value >>= nTimestamp;
            }
        }
        
        // Phase 1.2.3: Build basic JSON context
        OUStringBuffer aJsonBuilder(1024);
        aJsonBuilder.append("{");
        
        // Basic document info
        aJsonBuilder.append("\"document\": {");
        if (xTextDoc.is()) {
            aJsonBuilder.append("\"type\": \"text_document\",");
            aJsonBuilder.append("\"has_content\": true");
        } else {
            aJsonBuilder.append("\"type\": \"unknown\",");
            aJsonBuilder.append("\"has_content\": false");
        }
        aJsonBuilder.append("},");
        
        // Timestamp
        aJsonBuilder.append("\"timestamp\": ");
        aJsonBuilder.append(OUString::number(nTimestamp));
        aJsonBuilder.append(",");
        
        // Placeholder for enhanced context (Phase 2)
        aJsonBuilder.append("\"cursor_position\": {},");
        aJsonBuilder.append("\"selected_text\": \"\",");
        aJsonBuilder.append("\"formatting_state\": {}");
        
        aJsonBuilder.append("}");
        
        OUString sJsonContext = aJsonBuilder.makeStringAndClear();
        SAL_INFO("sw.ai", "Extracted basic document context: " << sJsonContext);
        
        return Any(sJsonContext);
    }
    catch (const Exception& e) {
        SAL_WARN("sw.ai", "Error extracting document context: " << e.Message);
        return Any(OUString("{}"));
    }
}
```

**Testing Phase 1.2**:
```cpp
// Test context extraction with mock PropertyValue sequence
Sequence<PropertyValue> mockContext(2);
mockContext[0].Name = "Document";
mockContext[0].Value <<= xTextDoc;
mockContext[1].Name = "Timestamp";  
mockContext[1].Value <<= sal_Int64(1234567890);

Any result = coordinator.extractDocumentContext(Any(mockContext));
OUString jsonContext;
result >>= jsonContext;
assert(jsonContext.indexOf("text_document") >= 0);
assert(jsonContext.indexOf("1234567890") >= 0);
```

#### **Step 1.3: Integrate HTTP Client into AgentCoordinator**
**File**: `sw/source/core/ai/AgentCoordinator.cxx:processSimpleRequest()`
**Current State**: Framework exists, NetworkClient integration incomplete

```cpp
// AgentCoordinator.cxx - Complete HTTP integration
OUString AgentCoordinator::processSimpleRequest(const OUString& rsRequest, const Any& rContext) {
    try {
        // Phase 1.3.1: Extract JSON context
        Any aJsonContext = extractDocumentContext(rContext);
        OUString sJsonContext;
        aJsonContext >>= sJsonContext;
        
        // Phase 1.3.2: Build request JSON
        OUStringBuffer aRequestBuilder(2048);
        aRequestBuilder.append("{");
        aRequestBuilder.append("\"request\": \"");
        aRequestBuilder.append(rsRequest.replaceAll("\"", "\\\""));
        aRequestBuilder.append("\",");
        aRequestBuilder.append("\"type\": \"simple\",");
        aRequestBuilder.append("\"complexity\": \"low\",");
        aRequestBuilder.append("\"context\": ");
        aRequestBuilder.append(sJsonContext);
        aRequestBuilder.append("}");
        
        OUString sRequestJson = aRequestBuilder.makeStringAndClear();
        
        // Phase 1.3.3: Make HTTP request
        if (!m_pNetworkClient) {
            initializeNetworkClient();
        }
        
        std::map<OUString, OUString> aHeaders;
        aHeaders["X-Request-Type"] = "simple";
        aHeaders["X-LibreOffice-Version"] = "24.2"; // Get from version info
        
        OUString sBackendUrl = "http://localhost:8000/api/simple";
        NetworkClient::HttpResponse aResponse = m_pNetworkClient->postJson(sBackendUrl, sRequestJson, aHeaders);
        
        // Phase 1.3.4: Handle response
        if (aResponse.bSuccess) {
            SAL_INFO("sw.ai", "Received agent response: " << aResponse.sBody.copy(0, 200));
            
            // Parse and execute operations (existing framework)
            ParsedResponse aParsed = parseEnhancedJsonResponse(aResponse.sBody);
            if (aParsed.bSuccess) {
                return processAgentResponse(aParsed);
            } else {
                return "AI processed: " + rsRequest + " (response format error)";
            }
        } else {
            SAL_WARN("sw.ai", "HTTP request failed: " << aResponse.sErrorMessage);
            return "Error connecting to AI service: " + aResponse.sErrorMessage;
        }
    }
    catch (const Exception& e) {
        SAL_WARN("sw.ai", "Exception in simple request processing: " << e.Message);
        return "Error processing request: " + e.Message;
    }
}
```

**End-to-End Testing Phase 1**:
```bash
# 1. Start Python agent server
cd langgraph-agents
python start_server.py

# 2. Test basic flow in LibreOffice
# User types: "Hello AI" 
# Expected: HTTP POST to localhost:8000/api/simple
# Expected: Agent response displayed in chat
# Expected: No document modifications (simple request)
```

### Phase 2: Enhanced Context and Operation Execution (Days 3-4)

#### **Step 2.1: Complete Document Context Extraction**
**File**: `sw/source/core/ai/AgentCoordinator.cxx:extractDocumentContext()`
**Enhancement**: Add comprehensive document information

```cpp
// AgentCoordinator.cxx - Enhanced context extraction with SwWrtShell
Any AgentCoordinator::extractDocumentContext(const Any& rContext) const {
    try {
        Sequence<PropertyValue> aContextProps;
        if (!(rContext >>= aContextProps)) {
            return Any(OUString("{}"));
        }
        
        Reference<XTextDocument> xTextDoc;
        Reference<XFrame> xFrame;
        
        // Extract references
        for (const auto& prop : aContextProps) {
            if (prop.Name == "Document") {
                prop.Value >>= xTextDoc;
            } else if (prop.Name == "Frame") {
                prop.Value >>= xFrame;
            }
        }
        
        OUStringBuffer aJsonBuilder(4096);
        aJsonBuilder.append("{");
        
        // Phase 2.1.1: Enhanced document structure
        aJsonBuilder.append("\"document\": {");
        if (xTextDoc.is()) {
            Reference<XText> xText = xTextDoc->getText();
            if (xText.is()) {
                OUString sTextContent = xText->getString();
                aJsonBuilder.append("\"content_length\": ");
                aJsonBuilder.append(OUString::number(sTextContent.getLength()));
                aJsonBuilder.append(",");
                
                // Extract first 500 characters for context
                OUString sContentPreview = sTextContent.copy(0, std::min(500, sTextContent.getLength()));
                aJsonBuilder.append("\"content_preview\": \"");
                aJsonBuilder.append(sContentPreview.replaceAll("\"", "\\\"").replaceAll("\n", "\\n"));
                aJsonBuilder.append("\",");
            }
            aJsonBuilder.append("\"type\": \"text_document\"");
        }
        aJsonBuilder.append("},");
        
        // Phase 2.1.2: Cursor and selection information
        aJsonBuilder.append("\"cursor_position\": {");
        if (xFrame.is()) {
            // Access Writer shell through frame
            SwView* pView = GetActiveView(); // Need to implement this helper
            if (pView) {
                SwWrtShell& rWrtShell = pView->GetWrtShell();
                
                // Get cursor position
                Point aCursorPos = rWrtShell.GetCharRect().Pos();
                aJsonBuilder.append("\"x\": ");
                aJsonBuilder.append(OUString::number(aCursorPos.X()));
                aJsonBuilder.append(", \"y\": ");
                aJsonBuilder.append(OUString::number(aCursorPos.Y()));
                aJsonBuilder.append(",");
                
                // Get line/column numbers
                aJsonBuilder.append("\"line\": ");
                aJsonBuilder.append(OUString::number(rWrtShell.GetLineNum()));
                aJsonBuilder.append(", \"column\": ");
                aJsonBuilder.append(OUString::number(rWrtShell.GetColumnNum()));
            }
        }
        aJsonBuilder.append("},");
        
        // Phase 2.1.3: Selected text
        aJsonBuilder.append("\"selected_text\": \"");
        if (xFrame.is()) {
            SwView* pView = GetActiveView();
            if (pView) {
                SwWrtShell& rWrtShell = pView->GetWrtShell();
                OUString sSelectedText = rWrtShell.GetSelText();
                aJsonBuilder.append(sSelectedText.replaceAll("\"", "\\\"").replaceAll("\n", "\\n"));
            }
        }
        aJsonBuilder.append("\",");
        
        // Phase 2.1.4: Document structure
        aJsonBuilder.append("\"document_structure\": {");
        if (xFrame.is()) {
            SwView* pView = GetActiveView();
            if (pView) {
                SwDoc* pDoc = pView->GetDocShell()->GetDoc();
                if (pDoc) {
                    aJsonBuilder.append("\"paragraph_count\": ");
                    aJsonBuilder.append(OUString::number(pDoc->GetDocStat().nPara));
                    aJsonBuilder.append(",");
                    aJsonBuilder.append("\"word_count\": ");
                    aJsonBuilder.append(OUString::number(pDoc->GetDocStat().nWord));
                    aJsonBuilder.append(",");
                    aJsonBuilder.append("\"character_count\": ");
                    aJsonBuilder.append(OUString::number(pDoc->GetDocStat().nChar));
                }
            }
        }
        aJsonBuilder.append("},");
        
        // Phase 2.1.5: Current formatting state
        aJsonBuilder.append("\"formatting_state\": {");
        if (xFrame.is()) {
            SwView* pView = GetActiveView();
            if (pView) {
                SwWrtShell& rWrtShell = pView->GetWrtShell();
                
                // Get character formatting at cursor
                SfxItemSet aCoreSet(rWrtShell.GetDoc()->GetAttrPool(),
                                   svl::Items<RES_CHRATR_BEGIN, RES_CHRATR_END-1>{});
                rWrtShell.GetCurAttr(aCoreSet);
                
                // Extract font weight (bold)
                const SvxWeightItem& rWeightItem = aCoreSet.Get(RES_CHRATR_WEIGHT);
                aJsonBuilder.append("\"is_bold\": ");
                aJsonBuilder.append(rWeightItem.GetWeight() >= WEIGHT_BOLD ? "true" : "false");
                aJsonBuilder.append(",");
                
                // Extract font posture (italic)
                const SvxPostureItem& rPostureItem = aCoreSet.Get(RES_CHRATR_POSTURE);
                aJsonBuilder.append("\"is_italic\": ");
                aJsonBuilder.append(rPostureItem.GetPosture() != ITALIC_NONE ? "true" : "false");
            }
        }
        aJsonBuilder.append("}");
        
        aJsonBuilder.append("}");
        
        OUString sJsonContext = aJsonBuilder.makeStringAndClear();
        SAL_INFO("sw.ai", "Extracted enhanced context (" << sJsonContext.getLength() << " chars)");
        
        return Any(sJsonContext);
    }
    catch (const Exception& e) {
        SAL_WARN("sw.ai", "Error extracting enhanced context: " << e.Message);
        return Any(OUString("{}"));
    }
}

// Helper function to get active Writer view
SwView* AgentCoordinator::GetActiveView() const {
    if (!m_xFrame.is()) return nullptr;
    
    Reference<XController> xController = m_xFrame->getController();
    if (!xController.is()) return nullptr;
    
    // Cast to SwView (this requires proper includes)
    SwView* pView = dynamic_cast<SwView*>(xController.get());
    return pView;
}
```

#### **Step 2.2: Enhanced AIPanel Context Preparation**
**File**: `sw/source/ui/sidebar/ai/AIPanel.cxx:prepareDocumentContext()`
**Enhancement**: Provide more comprehensive context to AgentCoordinator

```cpp
// AIPanel.cxx - Enhanced context preparation
Any AIPanel::prepareDocumentContext() {
    try {
        if (!m_xFrame.is()) return Any();
        
        Reference<XController> xController = m_xFrame->getController();
        if (!xController.is()) return Any();
        
        Reference<XTextDocument> xTextDoc(xController->getModel(), UNO_QUERY);
        if (!xTextDoc.is()) return Any();
        
        // Phase 2.2.1: Expanded context with more information
        Sequence<PropertyValue> aContext(8);
        PropertyValue* pContext = aContext.getArray();
        
        // Basic references
        pContext[0].Name = "Document";
        pContext[0].Value = Any(xTextDoc);
        pContext[1].Name = "Frame";
        pContext[1].Value = Any(m_xFrame);
        
        // Phase 2.2.2: Current selection
        Reference<XSelectionSupplier> xSelSupplier(xController, UNO_QUERY);
        if (xSelSupplier.is()) {
            Any aSelection = xSelSupplier->getSelection();
            pContext[2].Name = "Selection";
            pContext[2].Value = aSelection;
        }
        
        // Phase 2.2.3: Document properties
        Reference<XDocumentPropertiesSupplier> xDocPropSupplier(xTextDoc, UNO_QUERY);
        if (xDocPropSupplier.is()) {
            Reference<XDocumentProperties> xDocProps = xDocPropSupplier->getDocumentProperties();
            pContext[3].Name = "DocumentProperties";
            pContext[3].Value = Any(xDocProps);
        }
        
        // Phase 2.2.4: Text content reference
        Reference<XText> xText = xTextDoc->getText();
        pContext[4].Name = "TextContent";
        pContext[4].Value = Any(xText);
        
        // Phase 2.2.5: View configuration
        Reference<XViewSettingsSupplier> xViewSettingsSupplier(xController, UNO_QUERY);
        if (xViewSettingsSupplier.is()) {
            Reference<XPropertySet> xViewSettings = xViewSettingsSupplier->getViewSettings();
            pContext[5].Name = "ViewSettings";
            pContext[5].Value = Any(xViewSettings);
        }
        
        // Phase 2.2.6: Current timestamp for request correlation
        pContext[6].Name = "Timestamp";
        pContext[6].Value = Any(sal_Int64(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));
        
        // Phase 2.2.7: User context (language, preferences)
        pContext[7].Name = "UserPreferences";
        pContext[7].Value = Any(OUString("en_US")); // TODO: Get actual user language
        
        return Any(aContext);
    }
    catch (const Exception& e) {
        SAL_WARN("sw.ai", "Error preparing enhanced document context: " << e.Message);
        return Any();
    }
}
```

#### **Step 2.3: Verify DocumentOperations Implementation**
**File**: `sw/source/core/ai/operations/DocumentOperations.cxx`
**Task**: Ensure all operation types are fully implemented

```cpp
// DocumentOperations.cxx - Verify key operations are complete

// Phase 2.3.1: Test insertText operation (already implemented)
OUString DocumentOperations::insertText(const OUString& rsText, const Any& rPosition, 
                                        const Sequence<PropertyValue>& rFormatting) {
    // Implementation verified: lines 190-253
    // Uses pWrtShell->Insert(rsText) 
    // Applies formatting via applyTextFormatting()
    // Records operation for undo
    // Status: ✅ COMPLETE
}

// Phase 2.3.2: Test formatText operation (already implemented)  
OUString DocumentOperations::formatText(const Any& rTextRange, 
                                       const Sequence<PropertyValue>& rFormatting) {
    // Implementation verified: lines 255-298
    // Creates text cursor for range
    // Applies formatting via applyTextFormatting()
    // Records operation for undo
    // Status: ✅ COMPLETE
}

// Phase 2.3.3: Verify createTable implementation
OUString DocumentOperations::createTable(sal_Int32 nRows, sal_Int32 nColumns, 
                                        const Any& rPosition, 
                                        const Sequence<PropertyValue>& rTableProperties) {
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    if (!m_bInitialized) {
        throw IllegalArgumentException("DocumentOperations not initialized");
    }
    
    try {
        OUString sOperationId = generateOperationId();
        logOperationActivity(sOperationId, "Creating table: " + 
                           OUString::number(nRows) + "x" + OUString::number(nColumns));
        
        // Set position if specified
        if (rPosition.hasValue() && !setCursorPosition(rPosition)) {
            throw IllegalArgumentException("Invalid position for table creation");
        }
        
        SwWrtShell* pWrtShell = getWriterShell();
        if (!pWrtShell) {
            throw RuntimeException("Writer shell not available");
        }
        
        // Record operation for undo
        recordOperation("createTable", getCurrentCursorPosition());
        
        // Create table using SwWrtShell
        SwInsertTableOptions aTableOpts(SwInsertTableFlags::All, 1);
        const SwTable* pTable = pWrtShell->InsertTable(aTableOpts, nRows, nColumns);
        
        if (!pTable) {
            throw RuntimeException("Failed to create table");
        }
        
        // Apply table properties if specified
        if (rTableProperties.hasElements()) {
            applyTableProperties(pTable, rTableProperties);
        }
        
        logOperationActivity(sOperationId, "Table creation completed successfully");
        return sOperationId;
    }
    catch (const Exception& e) {
        SAL_WARN("sw.ai", "Error creating table: " << e.Message);
        throw;
    }
}

// Phase 2.3.4: Helper method for table properties
bool DocumentOperations::applyTableProperties(const SwTable* pTable, 
                                             const Sequence<PropertyValue>& rProperties) {
    // Apply table-specific formatting
    for (const auto& prop : rProperties) {
        if (prop.Name == "BorderStyle") {
            // Apply border styling
        } else if (prop.Name == "CellPadding") {
            // Apply cell padding
        } else if (prop.Name == "TableWidth") {
            // Set table width
        }
        // Add more property handling as needed
    }
    return true;
}
```

**End-to-End Testing Phase 2**:
```bash
# Test enhanced context and operations
# User types: "Make this text bold and create a 2x3 table"
# Expected: Enhanced document context sent to agents
# Expected: Two operations returned: formatText + createTable
# Expected: Selected text becomes bold
# Expected: 2x3 table inserted at cursor
# Expected: Both operations undoable with Ctrl+Z
```

### Phase 3: Production Optimization and Testing (Days 5-6)

#### **Step 3.1: Error Handling and Recovery**
**Enhancement**: Comprehensive error handling throughout the flow

```cpp
// AgentCoordinator.cxx - Enhanced error handling
OUString AgentCoordinator::processUserRequest(const OUString& rsRequest, const Any& rContext) {
    std::lock_guard<std::mutex> aGuard(m_aMutex);
    
    try {
        // Phase 3.1.1: Input validation
        if (rsRequest.isEmpty()) {
            throw IllegalArgumentException("Request cannot be empty");
        }
        
        if (rsRequest.getLength() > 10000) {
            throw IllegalArgumentException("Request too long (max 10000 characters)");
        }
        
        // Phase 3.1.2: Service availability check
        if (!m_pNetworkClient) {
            initializeNetworkClient();
            if (!m_pNetworkClient) {
                return "AI service temporarily unavailable. Please try again.";
            }
        }
        
        // Phase 3.1.3: Context extraction with fallback
        Any aJsonContext;
        try {
            aJsonContext = extractDocumentContext(rContext);
        }
        catch (const Exception& e) {
            SAL_WARN("sw.ai", "Context extraction failed, using minimal context: " << e.Message);
            aJsonContext = Any(OUString("{\"document\": {\"type\": \"unknown\"}}"));
        }
        
        // Phase 3.1.4: Request processing with retry logic
        for (int nRetry = 0; nRetry < 3; ++nRetry) {
            try {
                OUString sResponse = performAgentRequest(rsRequest, aJsonContext);
                if (!sResponse.isEmpty()) {
                    return sResponse;
                }
            }
            catch (const Exception& e) {
                SAL_WARN("sw.ai", "Request attempt " << (nRetry + 1) << " failed: " << e.Message);
                if (nRetry == 2) {
                    return "Unable to process request after multiple attempts. Please check your connection.";
                }
                // Wait before retry
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 * (nRetry + 1)));
            }
        }
        
        return "Request processing failed. Please try again.";
    }
    catch (const IllegalArgumentException& e) {
        return "Invalid request: " + e.Message;
    }
    catch (const Exception& e) {
        SAL_WARN("sw.ai", "Unexpected error in processUserRequest: " << e.Message);
        return "An unexpected error occurred. Please try again.";
    }
}
```

#### **Step 3.2: Performance Optimization**
**Enhancement**: Async processing and response time optimization

```cpp
// AIPanel.cxx - Async processing implementation
void AIPanel::sendMessageToBackend(const QueuedMessage& rMessage) {
    try {
        // Phase 3.2.1: Show immediate feedback
        if (m_xChatHistory) {
            m_xChatHistory->UpdateMessageStatus(rMessage.nChatMessageId, 
                                               MessageStatus::PROCESSING, 
                                               "Processing your request...");
        }
        
        // Phase 3.2.2: Prepare context asynchronously
        std::future<Any> contextFuture = std::async(std::launch::async, [this]() {
            return prepareDocumentContext();
        });
        
        // Phase 3.2.3: Get context with timeout
        std::future_status status = contextFuture.wait_for(std::chrono::milliseconds(500));
        Any aDocumentContext;
        
        if (status == std::future_status::ready) {
            aDocumentContext = contextFuture.get();
        } else {
            SAL_WARN("sw.ai", "Context preparation timeout, using minimal context");
            aDocumentContext = Any(); // Minimal context
        }
        
        // Phase 3.2.4: Call AgentCoordinator with timeout handling
        auto startTime = std::chrono::steady_clock::now();
        
        OUString sResponse = m_xAgentCoordinator->processUserRequest(rMessage.sContent, aDocumentContext);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        SAL_INFO("sw.ai", "Request processed in " << duration.count() << "ms");
        
        // Phase 3.2.5: Update UI based on response time
        if (duration.count() > 5000) {
            // Slow response warning
            if (m_xChatHistory) {
                m_xChatHistory->AddSystemMessage("Note: Request took longer than usual (" + 
                                                OUString::number(duration.count() / 1000.0) + "s)");
            }
        }
        
        handleBackendResponse(rMessage.sMessageId, sResponse);
    }
    catch (const Exception& e) {
        handleBackendError(rMessage.sMessageId, e.Message);
    }
}
```

#### **Step 3.3: Comprehensive Testing Suite**
**Implementation**: Complete testing framework

```cpp
// Test suite for production validation
class AIIntegrationTest {
public:
    void testBasicFlow() {
        // Test: User message → Agent response → Display
        AIPanel panel(nullptr, xFrame);
        panel.OnSendMessage("Hello AI");
        
        // Verify: Message queued and processed
        assert(panel.getActiveMessageCount() > 0);
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Verify: Response received and displayed
        assert(panel.getLastResponse().indexOf("Hello") >= 0);
    }
    
    void testContextExtraction() {
        // Test: Document context extraction
        AgentCoordinator coordinator(xContext);
        
        // Create mock document context
        Sequence<PropertyValue> mockContext = createMockDocumentContext();
        Any result = coordinator.extractDocumentContext(Any(mockContext));
        
        // Verify: JSON context contains expected fields
        OUString jsonContext;
        result >>= jsonContext;
        assert(jsonContext.indexOf("document") >= 0);
        assert(jsonContext.indexOf("cursor_position") >= 0);
        assert(jsonContext.indexOf("selected_text") >= 0);
    }
    
    void testOperationExecution() {
        // Test: Operation execution and undo
        DocumentOperations docOps(xContext);
        docOps.initializeWithFrame(xFrame);
        
        // Test text insertion
        OUString operationId = docOps.insertText("Test text", Any(), Sequence<PropertyValue>());
        assert(!operationId.isEmpty());
        
        // Verify: Text was inserted
        Reference<XTextDocument> xDoc = getCurrentDocument();
        assert(xDoc->getText()->getString().indexOf("Test text") >= 0);
        
        // Test undo
        assert(docOps.canUndo());
        docOps.undoLastOperation();
        
        // Verify: Text was removed
        assert(xDoc->getText()->getString().indexOf("Test text") < 0);
    }
    
    void testErrorHandling() {
        // Test: Network failure handling
        AIPanel panel(nullptr, xFrame);
        
        // Simulate network failure
        stopAgentServer();
        
        panel.OnSendMessage("Test message");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Verify: Error message displayed
        assert(panel.getLastResponse().indexOf("error") >= 0 || 
               panel.getLastResponse().indexOf("unavailable") >= 0);
        
        // Restart server and verify recovery
        startAgentServer();
        panel.OnSendMessage("Test message 2");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // Verify: Normal operation resumed
        assert(panel.getLastResponse().indexOf("error") < 0);
    }
    
    void testPerformance() {
        // Test: Response time requirements
        AIPanel panel(nullptr, xFrame);
        
        auto startTime = std::chrono::steady_clock::now();
        panel.OnSendMessage("Make this text bold"); // Simple operation
        
        // Wait for completion
        while (panel.isProcessingActive()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Verify: Simple operation completed in <2 seconds
        assert(duration.count() < 2000);
    }
};
```

### Phase 4: Production Deployment (Day 7)

#### **Step 4.1: Service Registration Verification**
```xml
<!-- sw/util/sw.component - Verify service registration -->
<component loader="com.sun.star.loader.SharedLibrary" uri="vnd.sun.star.expand:$LO_LIB_DIR/libsw.so">
  <implementation name="com.sun.star.comp.Writer.AIAgentCoordinator">
    <service name="com.sun.star.ai.AIAgentCoordinator"/>
  </implementation>
  <implementation name="com.sun.star.ai.DocumentOperations">
    <service name="com.sun.star.ai.XAIDocumentOperations"/>
  </implementation>
</component>
```

#### **Step 4.2: Build System Integration**
```makefile
# sw/Library_sw.mk - Ensure all files are included
$(eval $(call gb_Library_add_exception_objects,sw,\
    sw/source/ui/sidebar/ai/AIPanel \
    sw/source/core/ai/AgentCoordinator \
    sw/source/core/ai/NetworkClient \
    sw/source/core/ai/operations/DocumentOperations \
))
```

#### **Step 4.3: Production Validation**
```bash
# Final integration test script
#!/bin/bash

# 1. Build LibreOffice with AI integration
make sw

# 2. Start Python agent server
cd langgraph-agents
python start_server.py &
AGENT_PID=$!

# 3. Start LibreOffice with AI panel
soffice --writer --show-sidebar &
LO_PID=$!

# 4. Run automated test suite
python test_integration.py

# 5. Manual validation checklist
echo "Manual validation:"
echo "1. Open Writer document"
echo "2. Open AI sidebar panel"
echo "3. Type: 'Make this text bold'"
echo "4. Verify: Selected text becomes bold"
echo "5. Type: 'Create a 2x3 table'"
echo "6. Verify: Table is inserted"
echo "7. Press Ctrl+Z twice"
echo "8. Verify: Both operations are undone"

# Cleanup
kill $AGENT_PID $LO_PID
```

This comprehensive implementation plan provides the exact detailed flow for completing the LibreOffice AI Agent integration, with specific code implementations, testing procedures, and validation criteria for each phase.

<function_calls>
<invoke name="TodoWrite">
<parameter name="todos">[{"content": "ANALYSIS: Identify gaps between current agent system and LibreOffice C++ integration layer", "status": "completed", "priority": "high", "id": "analysis-gaps"}, {"content": "PLAN: Create production integration roadmap for LibreOffice \u2192 Agent \u2192 Operations flow", "status": "completed", "priority": "high", "id": "plan-production"}, {"content": "GAP 1: Replace mock document context with real LibreOffice UNO context extraction in bridge.py", "status": "completed", "priority": "low", "id": "gap1-context"}, {"content": "GAP 2: Implement C++ AgentCoordinator HTTP client to communicate with Python agent system", "status": "pending", "priority": "high", "id": "gap2-coordinator"}, {"content": "GAP 3: Replace mock operations with real C++ DocumentOperations UNO execution", "status": "pending", "priority": "high", "id": "gap3-execution"}, {"content": "GAP 4: Connect AIPanel user input to AgentCoordinator.processUserRequest()", "status": "pending", "priority": "high", "id": "gap4-triggers"}, {"content": "INTEGRATION: Test complete flow with real LibreOffice document", "status": "pending", "priority": "high", "id": "integration-test"}, {"content": "RESEARCH: Deep analysis of implementation gaps for production integration", "status": "completed", "priority": "high", "id": "research-gaps"}, {"content": "DOCUMENT: Create comprehensive flow document _docs/feature/dissonance_plan.md", "status": "completed", "priority": "high", "id": "document-flow"}, {"content": "CLARIFY: Update dissonance_plan.md to reflect C++ context extraction instead of PyUNO", "status": "completed", "priority": "medium", "id": "clarify-context"}, {"content": "RESEARCH: Complete end-to-end flow analysis for dissonance_plan.md update", "status": "completed", "priority": "high", "id": "research-complete-flow"}, {"content": "CREATE: Convert dissonance_plan.md to PRD format as dissonance_prd.md", "status": "completed", "priority": "high", "id": "create-prd"}]