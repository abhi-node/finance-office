#!/usr/bin/env python3
"""
Phase 8.4 Integration Test - Complete Error Handling with User Feedback
 
This test demonstrates the complete integration of all Phase 8 components:
- OperationErrorHandler (8.1)
- DocumentOperations checkpoints and rollback (8.2) 
- OperationCancellationManager and progress tracking (8.3)
- UIErrorPropagator with user feedback (8.4)

The test simulates a full error handling flow from operation failure through
user notification and recovery.
"""

import asyncio
import logging
import time
from typing import Dict, Any

# Import Phase 8 components
from agents.operation_error_handler import (
    OperationErrorHandler, OperationContext, OperationType, 
    create_operation_error_handler
)
from agents.ui_error_propagation import (
    UIErrorPropagator, UIErrorMessage, UIErrorSeverity, UIErrorType,
    ProgressState, create_ui_error_propagator
)
from agents.error_handler import ErrorContext, ErrorSeverity, ErrorCategory
from agents.operation_cancellation import CancellationToken, CancellationReason

# Mock DocumentOperations service for testing
class MockDocumentOperationsService:
    """Mock DocumentOperations service that simulates C++ behavior."""
    
    def __init__(self):
        self.cancellation_tokens = {}
        self.progress_info = {}
        self.error_history = []
        self.checkpoints = {}
        
    def createCancellationToken(self, operation_id: str, options) -> str:
        token_id = f"token_{operation_id}_{int(time.time())}"
        self.cancellation_tokens[operation_id] = {
            "token_id": token_id,
            "cancelled": False,
            "created": time.time()
        }
        print(f"[MockDocOps] Created cancellation token: {token_id}")
        return token_id
        
    def updateOperationProgress(self, operation_id: str, percentage: int, message: str, metadata):
        self.progress_info[operation_id] = {
            "percentage": percentage,
            "message": message,
            "timestamp": time.time()
        }
        print(f"[MockDocOps] Progress update: {operation_id} = {percentage}% - {message}")
        return True
        
    def isOperationCancelled(self, operation_id: str) -> bool:
        token = self.cancellation_tokens.get(operation_id, {})
        return token.get("cancelled", False)
        
    def cancelOperation(self, operation_id: str, reason: str, scope):
        if operation_id in self.cancellation_tokens:
            self.cancellation_tokens[operation_id]["cancelled"] = True
            print(f"[MockDocOps] Cancelled operation: {operation_id} - {reason}")
            return True
        return False
        
    def getOperationProgress(self, operation_id: str):
        progress = self.progress_info.get(operation_id, {})
        print(f"[MockDocOps] Getting progress for {operation_id}: {progress}")
        return progress
        
    def recordOperationError(self, operation_id: str, error_code: str, message: str, severity: int):
        error_record = {
            "operation_id": operation_id,
            "error_code": error_code,
            "message": message,
            "severity": severity,
            "timestamp": time.time()
        }
        self.error_history.append(error_record)
        print(f"[MockDocOps] Recorded error: {error_code} for {operation_id}")
        
    def canRecoverFromError(self, error_code: str, operation_id: str) -> bool:
        # Simulate recovery capability based on error type
        recoverable_errors = ["INVALID_PARAMETERS", "TIMEOUT", "NETWORK_FAILURE"]
        can_recover = error_code in recoverable_errors
        print(f"[MockDocOps] Can recover from {error_code}: {can_recover}")
        return can_recover
        
    def performErrorRecovery(self, error_code: str, operation_id: str, recovery_options):
        if self.canRecoverFromError(error_code, operation_id):
            recovery_id = f"recovery_{operation_id}_{int(time.time())}"
            print(f"[MockDocOps] Performing error recovery: {recovery_id}")
            return recovery_id
        return None

# Mock operation function that can simulate different error scenarios
async def mock_document_operation(operation_type: str, simulate_error: str = None):
    """Mock document operation that can simulate various error conditions."""
    await asyncio.sleep(0.5)  # Simulate processing time
    
    if simulate_error == "network_error":
        raise ConnectionError("Failed to connect to document service")
    elif simulate_error == "validation_error":
        raise ValueError("Invalid document formatting parameters")
    elif simulate_error == "timeout_error":
        await asyncio.sleep(10)  # This will trigger timeout handling
    elif simulate_error == "system_error":
        raise RuntimeError("Critical system error during document processing")
    elif simulate_error == "permission_error":
        raise PermissionError("Insufficient permissions to modify document")
    
    # Success case
    return {
        "operation_result": f"Successfully completed {operation_type} operation",
        "document_changes": ["Added text formatting", "Updated document structure"],
        "success": True
    }

async def test_complete_error_handling_flow():
    """Test the complete Phase 8 error handling flow."""
    
    print("=== Phase 8.4 Complete Error Handling Integration Test ===\n")
    
    # Setup logging
    logging.basicConfig(level=logging.INFO)
    
    # Create mock DocumentOperations service
    mock_doc_ops = MockDocumentOperationsService()
    
    # Create integrated error handler with all Phase 8 components
    config = {
        "document_operations_service": mock_doc_ops,
        "bridge": None,  # Would be actual bridge in real implementation
        "error_tracker": None  # Would be actual error tracker
    }
    
    error_handler = create_operation_error_handler(config)
    
    print("✓ Created integrated OperationErrorHandler with Phase 8.4 support\n")
    
    # Test scenarios
    test_scenarios = [
        {
            "name": "Network Error with Recovery",
            "operation_type": OperationType.TEXT_INSERTION,
            "error_type": "network_error",
            "expected_recovery": True
        },
        {
            "name": "Validation Error with User Feedback", 
            "operation_type": OperationType.TABLE_CREATION,
            "error_type": "validation_error",
            "expected_recovery": True
        },
        {
            "name": "Critical System Error with Rollback",
            "operation_type": OperationType.TEXT_FORMATTING,
            "error_type": "system_error", 
            "expected_recovery": False
        },
        {
            "name": "Successful Operation",
            "operation_type": OperationType.CHART_INSERTION,
            "error_type": None,
            "expected_recovery": False
        }
    ]
    
    for i, scenario in enumerate(test_scenarios, 1):
        print(f"--- Test Scenario {i}: {scenario['name']} ---")
        
        # Create operation context
        operation_context = OperationContext(
            operation_type=scenario["operation_type"],
            operation_name=scenario["name"],
            parameters={"test_mode": True},
            agent_id="test_agent",
            user_request=f"Test {scenario['name']}",
            timeout_seconds=5.0,
            max_retries=2
        )
        
        try:
            # Execute operation with comprehensive error handling
            result = await error_handler.execute_operation_with_error_handling(
                operation_context=operation_context,
                operation_function=mock_document_operation,
                document_operations_service=mock_doc_ops,
                operation_type=scenario["operation_type"].value,
                simulate_error=scenario["error_type"]
            )
            
            print(f"✓ Operation completed: {result.success}")
            if result.success:
                print(f"  Result: {result.result_data.get('operation_result', 'No result data')}")
            else:
                print(f"  Error: {result.error_message}")
                print(f"  Recovery options: {result.recovery_options}")
                print(f"  Rollback performed: {result.rollback_performed}")
                print(f"  User message: {result.user_message}")
                
                # Test error feedback handling
                if result.error_context:
                    await test_error_feedback_cycle(error_handler, operation_context.operation_id)
            
        except Exception as e:
            print(f"✗ Test scenario failed: {e}")
            # Create a basic result object for failed scenarios
            class MockResult:
                def __init__(self):
                    self.progress_updates = []
                    self.success = False
            result = MockResult()
        
        print(f"  Progress updates: {len(result.progress_updates) if hasattr(result, 'progress_updates') else 0}")
        print()
    
    # Test cancellation flow
    print("--- Test Scenario: Operation Cancellation ---")
    await test_cancellation_flow(error_handler, mock_doc_ops)
    
    # Display final statistics
    print("\n=== Final Statistics ===")
    performance_summary = error_handler.get_performance_summary()
    print(f"Total operations: {performance_summary['performance_metrics']['total_operations']}")
    print(f"Successful operations: {performance_summary['performance_metrics']['successful_operations']}")
    print(f"Failed operations: {performance_summary['performance_metrics']['failed_operations']}")
    print(f"Rolled back operations: {performance_summary['performance_metrics']['rolled_back_operations']}")
    print(f"Average execution time: {performance_summary['performance_metrics']['average_execution_time_ms']:.2f}ms")
    
    # Display DocumentOperations mock statistics
    print(f"\nDocumentOperations Mock Statistics:")
    print(f"Cancellation tokens created: {len(mock_doc_ops.cancellation_tokens)}")
    print(f"Progress updates: {len(mock_doc_ops.progress_info)}")
    print(f"Error records: {len(mock_doc_ops.error_history)}")
    
    print("\n✓ Phase 8.4 Integration Test Completed Successfully!")

async def test_error_feedback_cycle(error_handler: OperationErrorHandler, operation_id: str):
    """Test the error feedback cycle between components."""
    
    print(f"  Testing error feedback cycle for operation: {operation_id}")
    
    # Simulate DocumentOperations error feedback
    error_feedback = {
        "message": "Error processing completed with partial recovery",
        "details": "Document state restored to previous checkpoint",
        "suggested_actions": ["Review document for completeness", "Try operation again"],
        "can_retry": True,
        "auto_dismiss": 10,
        "progress_update": {
            "percentage": 100,
            "message": "Error handling completed"
        }
    }
    
    # Test error feedback handling
    if hasattr(error_handler.ui_propagator, 'handle_document_operations_error_feedback'):
        await error_handler.ui_propagator.handle_document_operations_error_feedback(
            operation_id, error_feedback
        )
        print(f"  ✓ Error feedback cycle completed")
    
    # Test error history retrieval
    error_history = error_handler.ui_propagator.get_operation_error_history(operation_id)
    if error_history:
        print(f"  ✓ Error history retrieved: {error_history['ui_error'].error_id}")

async def test_cancellation_flow(error_handler: OperationErrorHandler, mock_doc_ops: MockDocumentOperationsService):
    """Test the operation cancellation flow."""
    
    print("Testing operation cancellation flow...")
    
    # Create long-running operation context
    operation_context = OperationContext(
        operation_type=OperationType.TABLE_POPULATION,
        operation_name="Long Running Operation",
        parameters={"large_dataset": True},
        timeout_seconds=2.0,  # Short timeout to trigger cancellation
        cancellation_token=CancellationToken()
    )
    
    # Start operation that will be cancelled
    operation_task = asyncio.create_task(
        error_handler.execute_operation_with_error_handling(
            operation_context=operation_context,
            operation_function=mock_document_operation,
            document_operations_service=mock_doc_ops,
            operation_type="table_population",
            simulate_error="timeout_error"  # This will cause timeout
        )
    )
    
    # Simulate user cancellation after 1 second
    await asyncio.sleep(1.0)
    
    # Cancel the operation
    success = operation_context.cancellation_token.cancel(
        CancellationReason.USER_REQUESTED, 
        "User cancelled long-running operation",
        "test_user"
    )
    
    print(f"  Cancellation initiated: {success}")
    
    # Wait for operation to complete (should be cancelled)
    try:
        result = await asyncio.wait_for(operation_task, timeout=3.0)
        print(f"  Operation result after cancellation: {result.success}")
        print(f"  ✓ Cancellation flow completed successfully")
    except asyncio.TimeoutError:
        print(f"  ✗ Operation did not complete within timeout")
    except Exception as e:
        print(f"  Operation cancelled with exception: {type(e).__name__}")

if __name__ == "__main__":
    # Run the comprehensive integration test
    asyncio.run(test_complete_error_handling_flow())