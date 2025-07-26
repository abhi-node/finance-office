#!/usr/bin/env python3
"""Debug script to isolate the 'bool' object is not callable error."""

import asyncio
import traceback

# Test basic component creation
def test_component_creation():
    print("Testing component creation...")
    
    try:
        from agents.operation_error_handler import create_operation_error_handler
        print("✓ Import successful")
        
        config = {}
        handler = create_operation_error_handler(config)
        print("✓ Handler created successfully")
        
        print(f"State manager type: {type(handler.state_manager)}")
        print(f"State manager methods: {[m for m in dir(handler.state_manager) if not m.startswith('_')]}")
        
        # Test create_snapshot
        if hasattr(handler.state_manager, 'create_snapshot'):
            print(f"create_snapshot callable: {callable(handler.state_manager.create_snapshot)}")
            if callable(handler.state_manager.create_snapshot):
                snapshot = handler.state_manager.create_snapshot()
                print(f"✓ Snapshot created: {snapshot}")
            else:
                print(f"✗ create_snapshot exists but is not callable: {handler.state_manager.create_snapshot}")
        else:
            print("✗ create_snapshot method missing")
        
    except Exception as e:
        print(f"✗ Error: {e}")
        traceback.print_exc()

async def test_simple_operation():
    """Test a simple operation execution."""
    print("\nTesting simple operation...")
    
    try:
        from agents.operation_error_handler import create_operation_error_handler, OperationContext, OperationType
        
        config = {}
        handler = create_operation_error_handler(config)
        
        operation_context = OperationContext(
            operation_type=OperationType.TEXT_INSERTION,
            operation_name="Test Operation"
        )
        
        async def simple_operation():
            return {"result": "success"}
        
        print("Starting operation...")
        result = await handler.execute_operation_with_error_handling(
            operation_context=operation_context,
            operation_function=simple_operation
        )
        
        print(f"✓ Operation completed: {result.success}")
        
    except Exception as e:
        print(f"✗ Operation failed: {e}")
        traceback.print_exc()

if __name__ == "__main__":
    test_component_creation()
    asyncio.run(test_simple_operation())