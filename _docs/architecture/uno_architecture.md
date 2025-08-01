# UNO Architecture: Universal Network Objects Framework

## Overview

UNO (Universal Network Objects) is LibreOffice's foundational component architecture that enables language-independent, location-transparent communication between software components. UNO serves as the backbone for all LibreOffice functionality, providing a unified interface system that allows different programming languages, processes, and even remote systems to interact seamlessly with LibreOffice's core functionality.

## Architectural Philosophy

UNO operates on several key architectural principles that make it both powerful and flexible. The system emphasizes **interface-based programming** where functionality is accessed through well-defined contracts rather than direct implementation dependencies. This approach enables **language independence**, allowing components written in C++, Java, Python, and other languages to interact seamlessly.

The architecture provides **location transparency**, meaning that components can communicate whether they exist in the same process, different processes on the same machine, or even across network boundaries. **Service-oriented design** structures functionality as discoverable services that can be instantiated and configured dynamically. Finally, **reference counting and lifecycle management** ensure proper resource cleanup and prevent memory leaks across component boundaries.

## Core UNO Concepts

### Interface Definition Language (IDL)

UNO uses Interface Definition Language (IDL) files to define contracts between components. These IDL files are language-neutral specifications that get compiled into language-specific bindings for C++, Java, Python, and other supported languages.

**IDL Structure Pattern:**
```idl
module com { module sun { module star { module text {

published interface XTextDocument : com::sun::star::frame::XModel
{
    /** Access to the main text content of the document */
    com::sun::star::text::XText getText();
    
    /** Forces reformatting of the entire document */
    void reformat();
};

published service TextDocument
{
    /** Primary interface for document access */
    interface XTextDocument;
    
    /** Document model functionality */
    interface com::sun::star::frame::XModel;
    
    /** Component lifecycle management */
    interface com::sun::star::lang::XComponent;
    
    /** Service introspection */
    interface com::sun::star::lang::XServiceInfo;
};

}; }; }; };
```

**Key IDL Elements:**
- **module**: Namespace organization following reverse domain name convention
- **interface**: Contract definition with method signatures and documentation
- **service**: Collection of interfaces that define a component's capabilities
- **published**: Indicates stable API that external components can depend on
- **inheritance**: Interfaces can extend other interfaces to build functionality hierarchies

### Interface-Based Programming Model

UNO's interface-based approach separates contract definition from implementation, enabling flexible component architectures. Every UNO object implements the base `XInterface` which provides reference counting and interface querying capabilities.

**Interface Hierarchy:**
```
XInterface (base)
   XServiceInfo (service introspection)
   XComponent (lifecycle management)
   XMultiServiceFactory (service creation)
   Domain-specific interfaces
       XTextDocument (document access)
       XText (text manipulation)
       XTextCursor (text navigation)
       XPropertySet (property access)
```

**Interface Querying Pattern:**
```cpp
// Query for specific interfaces from a base reference
css::uno::Reference<css::uno::XInterface> xInterface = /* object reference */;

// Query for text document interface
css::uno::Reference<css::text::XTextDocument> xTextDoc(
    xInterface, css::uno::UNO_QUERY);

if (xTextDoc.is()) {
    // Use text document functionality
    css::uno::Reference<css::text::XText> xText = xTextDoc->getText();
}

// Query for property access
css::uno::Reference<css::beans::XPropertySet> xProps(
    xInterface, css::uno::UNO_QUERY);

if (xProps.is()) {
    // Access object properties
    css::uno::Any value = xProps->getPropertyValue(u"PropertyName");
}
```

### Service Architecture

UNO services are collections of interfaces that define the complete functionality of a component. Services can be instantiated by name through the service manager, providing a factory pattern for component creation.

**Service Registration Pattern:**
```xml
<!-- Component definition file -->
<component loader="com.sun.star.loader.SharedLibrary" 
           environment="@CPPU_ENV@"
           xmlns="http://openoffice.org/2010/uno-components">
    <implementation name="com.sun.star.comp.Writer.TextDocument"
                    constructor="Writer_SwTextDocument_get_implementation">
        <service name="com.sun.star.text.TextDocument"/>
        <service name="com.sun.star.frame.XModel"/>
    </implementation>
</component>
```

**Service Implementation Pattern:**
```cpp
// Modern constructor function pattern
extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
Writer_SwTextDocument_get_implementation(
    css::uno::XComponentContext* context,
    css::uno::Sequence<css::uno::Any> const& args)
{
    return cppu::acquire(new SwXTextDocument(context, args));
}

// Service implementation class
class SwXTextDocument : public cppu::WeakImplHelper<
    css::text::XTextDocument,
    css::frame::XModel,
    css::lang::XServiceInfo>
{
public:
    SwXTextDocument(css::uno::Reference<css::uno::XComponentContext> const& context,
                   css::uno::Sequence<css::uno::Any> const& args);
    
    // XTextDocument implementation
    virtual css::uno::Reference<css::text::XText> SAL_CALL getText() override;
    virtual void SAL_CALL reformat() override;
    
    // XServiceInfo implementation
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(OUString const& serviceName) override;
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override;
    
private:
    css::uno::Reference<css::uno::XComponentContext> m_xContext;
};
```

## Component Context and Service Manager

The Component Context serves as the dependency injection container for UNO services, providing access to the service manager, configuration, and other system services.

### Component Context Architecture

```cpp
// Get the global component context
css::uno::Reference<css::uno::XComponentContext> xContext = 
    comphelper::getProcessComponentContext();

// Access service manager through context
css::uno::Reference<css::lang::XMultiComponentFactory> xServiceManager = 
    xContext->getServiceManager();

// Create services with context support
css::uno::Reference<css::text::XTextDocument> xTextDoc = 
    css::uno::Reference<css::text::XTextDocument>(
        xServiceManager->createInstanceWithContext(
            "com.sun.star.text.TextDocument", xContext),
        css::uno::UNO_QUERY);
```

### Service Discovery and Instantiation

The service manager maintains a registry of available services and their implementations, enabling dynamic service discovery and instantiation.

**Service Creation Patterns:**
```cpp
// Simple service creation
css::uno::Reference<css::uno::XInterface> xService = 
    xServiceManager->createInstanceWithContext(
        "com.sun.star.configuration.ConfigurationProvider", xContext);

// Service creation with arguments
css::uno::Sequence<css::uno::Any> aArgs(1);
aArgs[0] <<= css::beans::PropertyValue(
    "nodepath", -1, 
    css::uno::Any(OUString("/org.openoffice.Setup")),
    css::beans::PropertyState_DIRECT_VALUE);

css::uno::Reference<css::container::XNameAccess> xConfigAccess = 
    css::uno::Reference<css::container::XNameAccess>(
        xServiceManager->createInstanceWithArgumentsAndContext(
            "com.sun.star.configuration.ConfigurationAccess", aArgs, xContext),
        css::uno::UNO_QUERY);
```

## UNO Type System

UNO provides a rich type system that enables type-safe communication across language and process boundaries.

### Basic Types

**Primitive Types:**
- `boolean` - Boolean values
- `byte` - 8-bit signed integer
- `short` - 16-bit signed integer  
- `long` - 32-bit signed integer
- `hyper` - 64-bit signed integer
- `float` - 32-bit floating point
- `double` - 64-bit floating point
- `char` - 16-bit Unicode character
- `string` - Unicode string
- `type` - Type information

**Complex Types:**
- `sequence<T>` - Dynamic arrays of type T
- `struct` - Value types with named fields
- `enum` - Enumerated constants
- `interface` - Reference types
- `union` - Discriminated unions
- `any` - Variant type that can hold any UNO value

### Type Conversion and Any Type

The `any` type serves as UNO's variant type, capable of holding any UNO value with full type information.

```cpp
// Store different types in any
css::uno::Any aValue;

aValue <<= OUString("Hello World");  // String
aValue <<= sal_Int32(42);           // Integer
aValue <<= true;                    // Boolean

// Extract with type checking
OUString sText;
if (aValue >>= sText) {
    // Successfully extracted as string
}

sal_Int32 nNumber;
if (aValue >>= nNumber) {
    // Successfully extracted as integer
}

// Get type information
css::uno::Type aType = aValue.getValueType();
if (aType == cppu::UnoType<OUString>::get()) {
    // Type is string
}
```

## Reference Counting and Memory Management

UNO uses reference counting for automatic memory management across component boundaries. All UNO objects inherit from `XInterface` which provides `acquire()` and `release()` methods.

### Reference Template

The `css::uno::Reference<T>` template provides smart pointer functionality with automatic reference counting:

```cpp
// Automatic reference management
css::uno::Reference<css::text::XTextDocument> xDoc = /* initialization */;

// Reference is automatically acquired during assignment
css::uno::Reference<css::text::XText> xText = xDoc->getText();

// Reference is automatically released when going out of scope
```

### Weak References

For breaking circular references, UNO provides weak reference support:

```cpp
// Create weak reference to avoid cycles
css::uno::WeakReference<css::text::XTextDocument> xWeakDoc = xDoc;

// Convert back to strong reference when needed
css::uno::Reference<css::text::XTextDocument> xStrongDoc = xWeakDoc.get();
if (xStrongDoc.is()) {
    // Object is still alive
}
```

## Inter-Process Communication

UNO supports communication across process and network boundaries through its bridge architecture.

### UNO Bridge Architecture

**Bridge Types:**
- **In-process**: Direct function calls within the same process
- **Inter-process**: Communication between processes on the same machine
- **Network**: Communication across network boundaries
- **Language bridges**: Translation between different programming languages

**Connection Establishment:**
```bash
# Start LibreOffice as UNO service
soffice --headless --accept="socket,host=localhost,port=2002;urp;"

# Connect from external application
css::uno::Reference<css::bridge::XBridge> xBridge = /* bridge creation */;
css::uno::Reference<css::uno::XInterface> xRemoteObject = 
    xBridge->getInstance("StarOffice.ServiceManager");
```

### Protocol Support

UNO supports multiple communication protocols:
- **URP (UNO Remote Protocol)**: Binary protocol for efficient communication
- **SOAP**: Web services integration  
- **CORBA**: Integration with CORBA systems
- **Java**: Native Java integration through JNI

## Configuration Integration

UNO integrates with LibreOffice's configuration system through the Configuration Provider service, enabling type-safe access to settings and preferences.

### Configuration Access Pattern

```cpp
// Create configuration provider
css::uno::Reference<css::lang::XMultiServiceFactory> xConfigProvider = 
    css::uno::Reference<css::lang::XMultiServiceFactory>(
        xServiceManager->createInstanceWithContext(
            "com.sun.star.configuration.ConfigurationProvider", xContext),
        css::uno::UNO_QUERY);

// Create configuration access for specific node
css::uno::Sequence<css::uno::Any> aArgs(1);
aArgs[0] <<= css::beans::PropertyValue(
    "nodepath", -1,
    css::uno::Any(OUString("/org.openoffice.Office.Writer/Content")),
    css::beans::PropertyState_DIRECT_VALUE);

css::uno::Reference<css::container::XNameAccess> xConfigAccess = 
    css::uno::Reference<css::container::XNameAccess>(
        xConfigProvider->createInstanceWithArguments(
            "com.sun.star.configuration.ConfigurationAccess", aArgs),
        css::uno::UNO_QUERY);

// Read configuration values
css::uno::Any aValue = xConfigAccess->getByName("Display/FieldName");
sal_Bool bShowFieldNames;
aValue >>= bShowFieldNames;
```

## Event and Listener Architecture

UNO provides a comprehensive event system for component communication and state synchronization.

### Listener Registration Pattern

```cpp
// Implement listener interface
class MyDocumentListener : public cppu::WeakImplHelper<
    css::document::XDocumentEventListener>
{
public:
    virtual void SAL_CALL documentEventOccured(
        const css::document::DocumentEvent& Event) override
    {
        if (Event.EventName == "OnSave") {
            // Handle document save event
        }
    }
    
    virtual void SAL_CALL disposing(
        const css::lang::EventObject& Source) override
    {
        // Handle component disposal
    }
};

// Register listener
css::uno::Reference<css::document::XDocumentEventBroadcaster> xBroadcaster(
    xDocument, css::uno::UNO_QUERY);

if (xBroadcaster.is()) {
    css::uno::Reference<css::document::XDocumentEventListener> xListener = 
        new MyDocumentListener();
    xBroadcaster->addDocumentEventListener(xListener);
}
```

## Extension and Add-in Development

UNO enables comprehensive extension development through its service architecture and interface system.

### Extension Service Pattern

```cpp
// Extension service implementation
class MyExtensionService : public cppu::WeakImplHelper<
    css::lang::XServiceInfo,
    css::lang::XInitialization,
    css::custom::XMyExtensionInterface>
{
public:
    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override {
        return "com.example.MyExtensionService";
    }
    
    virtual sal_Bool SAL_CALL supportsService(OUString const& serviceName) override {
        return cppu::supportsService(this, serviceName);
    }
    
    virtual css::uno::Sequence<OUString> SAL_CALL getSupportedServiceNames() override {
        return { "com.example.MyExtension" };
    }
    
    // XInitialization
    virtual void SAL_CALL initialize(
        const css::uno::Sequence<css::uno::Any>& arguments) override {
        // Initialize extension with provided arguments
    }
    
    // Custom interface implementation
    virtual void SAL_CALL doCustomOperation() override {
        // Extension-specific functionality
    }
};

// Constructor function for service registration
extern "C" SAL_DLLPUBLIC_EXPORT css::uno::XInterface*
MyExtension_get_implementation(
    css::uno::XComponentContext* context,
    css::uno::Sequence<css::uno::Any> const& args)
{
    return cppu::acquire(new MyExtensionService(context, args));
}
```

## Error Handling and Exception Management

UNO provides structured exception handling across language and process boundaries.

### Exception Hierarchy

```cpp
// UNO exception hierarchy
css::uno::Exception (base)
   css::uno::RuntimeException
   css::lang::IllegalArgumentException  
   css::io::IOException
   css::container::NoSuchElementException
   Domain-specific exceptions
```

### Exception Handling Pattern

```cpp
try {
    css::uno::Reference<css::text::XText> xText = xDocument->getText();
    xText->insertString(xText->getStart(), "Hello World", false);
}
catch (const css::lang::IllegalArgumentException& e) {
    // Handle invalid arguments
    SAL_WARN("extension.myext", "Invalid argument: " << e.Message);
}
catch (const css::uno::RuntimeException& e) {
    // Handle runtime errors
    SAL_WARN("extension.myext", "Runtime error: " << e.Message);
}
catch (const css::uno::Exception& e) {
    // Handle all other UNO exceptions
    SAL_WARN("extension.myext", "UNO exception: " << e.Message);
}
```

## Performance Considerations

UNO's architecture includes several performance optimization strategies:

### Connection Pooling and Caching

- **Service instance caching**: Frequently used services are cached to avoid repeated instantiation
- **Interface proxy caching**: Interface queries are cached to avoid repeated proxy creation
- **Connection reuse**: Network connections are pooled and reused for multiple requests

### Lazy Loading

- **Component loading**: UNO components are loaded only when first requested
- **Interface implementation**: Complex interfaces may use lazy initialization for expensive operations
- **Service discovery**: Service registry is built incrementally as services are requested

### Memory Management Optimization

- **Weak reference usage**: Breaking circular references prevents memory leaks
- **Automatic cleanup**: Reference counting ensures deterministic cleanup
- **Resource pooling**: Expensive resources like fonts and images are pooled and shared

## Integration with LibreOffice Applications

UNO serves as the foundation for all LibreOffice application integration, providing consistent interfaces across Writer, Calc, Impress, Draw, Math, and Base.

### Application-Specific Services

**Writer Integration:**
- `com.sun.star.text.TextDocument` - Main document service
- `com.sun.star.text.Text` - Text content manipulation
- `com.sun.star.text.TextCursor` - Text navigation and selection
- `com.sun.star.style.StyleFamilies` - Style management

**Calc Integration:**
- `com.sun.star.sheet.SpreadsheetDocument` - Spreadsheet document service
- `com.sun.star.sheet.Spreadsheet` - Sheet manipulation
- `com.sun.star.table.CellRange` - Cell range operations
- `com.sun.star.sheet.FormulaParser` - Formula processing

### Cross-Application Services

- `com.sun.star.frame.Desktop` - Application lifecycle management
- `com.sun.star.document.FilterFactory` - Import/export filters
- `com.sun.star.configuration.ConfigurationProvider` - Settings access
- `com.sun.star.ui.UIElementFactory` - UI component creation

## Advanced UNO Patterns

### Factory Pattern Implementation

```cpp
// Service factory for creating multiple related services
class MyServiceFactory : public cppu::WeakImplHelper<
    css::lang::XMultiServiceFactory,
    css::lang::XServiceInfo>
{
public:
    virtual css::uno::Reference<css::uno::XInterface> SAL_CALL 
    createInstance(const OUString& serviceName) override {
        if (serviceName == "com.example.TextProcessor") {
            return css::uno::Reference<css::uno::XInterface>(
                static_cast<css::uno::XInterface*>(new TextProcessor()));
        } else if (serviceName == "com.example.DataAnalyzer") {
            return css::uno::Reference<css::uno::XInterface>(
                static_cast<css::uno::XInterface*>(new DataAnalyzer()));
        }
        throw css::lang::IllegalArgumentException();
    }
};
```

### Observer Pattern with UNO Events

```cpp
// Event broadcaster implementation
class DocumentEventBroadcaster : public cppu::WeakImplHelper<
    css::document::XDocumentEventBroadcaster>
{
private:
    std::vector<css::uno::Reference<css::document::XDocumentEventListener>> m_listeners;
    
public:
    virtual void SAL_CALL addDocumentEventListener(
        const css::uno::Reference<css::document::XDocumentEventListener>& listener) override {
        m_listeners.push_back(listener);
    }
    
    virtual void SAL_CALL removeDocumentEventListener(
        const css::uno::Reference<css::document::XDocumentEventListener>& listener) override {
        m_listeners.erase(
            std::remove(m_listeners.begin(), m_listeners.end(), listener),
            m_listeners.end());
    }
    
    void fireDocumentEvent(const css::document::DocumentEvent& event) {
        for (auto& listener : m_listeners) {
            if (listener.is()) {
                listener->documentEventOccured(event);
            }
        }
    }
};
```

## Security Considerations

UNO includes security mechanisms for safe inter-process and network communication:

### Access Control

- **Permission checking**: Services can implement permission-based access control
- **Security context**: Component context includes security information
- **Capability-based security**: Interfaces provide capability-based access control

### Safe Communication

- **Type safety**: Strong typing prevents many categories of security vulnerabilities
- **Boundary validation**: All data crossing component boundaries is validated
- **Exception isolation**: Exceptions are properly marshaled across boundaries

This comprehensive UNO architecture provides the foundation for all LibreOffice functionality and enables sophisticated component-based applications that can span multiple processes, languages, and even network boundaries. The architecture's emphasis on interface-based programming, service-oriented design, and automatic resource management creates a robust platform for building complex office productivity applications while maintaining clean separation of concerns and enabling extensive customization and extension capabilities.