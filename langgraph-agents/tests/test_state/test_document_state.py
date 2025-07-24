"""
Comprehensive tests for DocumentState management system.

This test suite validates the core state management functionality according to
Task 11.2 requirements including thread safety, serialization, state validation,
and state history tracking for debugging and rollback capabilities.
"""

import pytest
import json
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from unittest.mock import Mock, patch

# Import state management components
import sys
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from state.document_state import (
    DocumentState,
    DocumentStateManager,
    DocumentReference,
    CursorPosition,
    DocumentStructure,
    FormattingState,
    PendingOperation,
    CompletedOperation,
    ValidationResult,
    StateSnapshot,
    AgentStatus,
    OperationStatus
)

from state.state_manager import (
    EnhancedDocumentStateManager,
    StatePersistenceManager,
    StateMonitor,
    StateChangeEvent,
    create_state_manager
)

class TestDocumentStateStructure:
    """Test the DocumentState structure and data classes."""
    
    def test_document_reference_creation(self):
        """Test DocumentReference creation with proper defaults."""
        doc_ref = DocumentReference()
        assert doc_ref.title == ""
        assert doc_ref.path is None
        assert doc_ref.document_type == "text"
        assert doc_ref.size_bytes == 0
        assert doc_ref.page_count == 1
        assert doc_ref.language == "en-US"
        
        # Test with custom values
        doc_ref = DocumentReference(
            title="Test Document",
            path="/tmp/test.odt",
            size_bytes=1024,
            page_count=3
        )
        assert doc_ref.title == "Test Document"
        assert doc_ref.path == "/tmp/test.odt"
        assert doc_ref.size_bytes == 1024
        assert doc_ref.page_count == 3
    
    def test_cursor_position_creation(self):
        """Test CursorPosition creation and properties."""
        cursor = CursorPosition()
        assert cursor.paragraph == 0
        assert cursor.character == 0
        assert cursor.page == 1
        assert cursor.section is None
        assert cursor.is_in_table == False
        assert cursor.is_in_header == False
        assert cursor.is_in_footer == False
        
        # Test with custom values
        cursor = CursorPosition(
            paragraph=5,
            character=25,
            page=2,
            is_in_table=True,
            section="Introduction"
        )
        assert cursor.paragraph == 5
        assert cursor.character == 25
        assert cursor.page == 2
        assert cursor.is_in_table == True
        assert cursor.section == "Introduction"
    
    def test_document_structure_creation(self):
        """Test DocumentStructure creation with proper list initialization."""
        structure = DocumentStructure()
        assert structure.paragraphs == 0
        assert isinstance(structure.sections, list)
        assert isinstance(structure.tables, list)
        assert isinstance(structure.images, list)
        assert isinstance(structure.charts, list)
        assert len(structure.sections) == 0
        
        # Test that lists are independent for different instances
        structure1 = DocumentStructure()
        structure2 = DocumentStructure()
        structure1.sections.append({"name": "Section 1"})
        
        assert len(structure1.sections) == 1
        assert len(structure2.sections) == 0
    
    def test_pending_operation_creation(self):
        """Test PendingOperation creation with UUID generation."""
        operation = PendingOperation(
            operation_type="insert_text",
            parameters={"text": "Hello world", "position": 0},
            agent_id="content_generator"
        )
        
        assert operation.operation_type == "insert_text"
        assert operation.parameters["text"] == "Hello world"
        assert operation.agent_id == "content_generator"
        assert operation.status == OperationStatus.PENDING
        assert len(operation.operation_id) > 0  # UUID should be generated
        assert operation.priority == 0
        assert isinstance(operation.dependencies, list)
    
    def test_agent_status_enum(self):
        """Test AgentStatus enum values."""
        assert AgentStatus.IDLE.value == "idle"
        assert AgentStatus.PROCESSING.value == "processing"
        assert AgentStatus.COMPLETED.value == "completed"
        assert AgentStatus.ERROR.value == "error"
        assert AgentStatus.CANCELLED.value == "cancelled"
    
    def test_operation_status_enum(self):
        """Test OperationStatus enum values."""
        assert OperationStatus.PENDING.value == "pending"
        assert OperationStatus.IN_PROGRESS.value == "in_progress"
        assert OperationStatus.COMPLETED.value == "completed"
        assert OperationStatus.FAILED.value == "failed"
        assert OperationStatus.CANCELLED.value == "cancelled"

class TestDocumentStateManager:
    """Test the core DocumentStateManager functionality."""
    
    def test_state_manager_initialization(self):
        """Test state manager initialization with default state."""
        manager = DocumentStateManager()
        state = manager.get_state()
        
        # Verify all required keys are present
        required_keys = [
            "current_document", "cursor_position", "selected_text",
            "document_structure", "formatting_state", "messages",
            "current_task", "task_history", "agent_status",
            "content_analysis", "generated_content", "content_suggestions",
            "external_data", "research_citations", "api_usage",
            "pending_operations", "completed_operations", "validation_results",
            "last_error", "retry_count", "error_recovery", "rollback_points",
            "user_preferences", "interaction_history", "approval_required",
            "performance_metrics", "resource_utilization", "optimization_recommendations"
        ]
        
        for key in required_keys:
            assert key in state, f"Required key '{key}' missing from state"
        
        # Verify initial values
        assert state["selected_text"] == ""
        assert state["current_task"] == ""
        assert isinstance(state["messages"], list)
        assert len(state["messages"]) == 0
        assert state["retry_count"] == 0
        assert state["last_error"] is None
    
    def test_state_manager_with_initial_state(self):
        """Test state manager initialization with custom initial state."""
        initial_state = DocumentState(
            current_document={"title": "Test Doc"},
            cursor_position={"paragraph": 1, "character": 5},
            selected_text="test text",
            document_structure={},
            formatting_state={},
            messages=[],
            current_task="test task",
            task_history=[],
            agent_status={},
            content_analysis={},
            generated_content=[],
            content_suggestions=[],
            external_data={},
            research_citations=[],
            api_usage={},
            pending_operations=[],
            completed_operations=[],
            validation_results={},
            last_error=None,
            retry_count=0,
            error_recovery={},
            rollback_points=[],
            user_preferences={},
            interaction_history=[],
            approval_required=[],
            performance_metrics={},
            resource_utilization={},
            optimization_recommendations=[]
        )
        
        manager = DocumentStateManager(initial_state)
        state = manager.get_state()
        
        assert state["current_task"] == "test task"
        assert state["selected_text"] == "test text"
        assert state["current_document"]["title"] == "Test Doc"
    
    def test_basic_state_update(self):
        """Test basic state update functionality."""
        manager = DocumentStateManager()
        
        # Test simple update
        updates = {
            "current_task": "New task",
            "selected_text": "Updated text"
        }
        
        success = manager.update_state(updates)
        assert success == True
        
        state = manager.get_state()
        assert state["current_task"] == "New task"
        assert state["selected_text"] == "Updated text"
    
    def test_additive_list_updates(self):
        """Test LangGraph's additive pattern for list updates."""
        manager = DocumentStateManager()
        
        # Add first message
        update1 = {"messages": [{"type": "human", "content": "Hello"}]}
        success1 = manager.update_state(update1)
        assert success1 == True
        
        state1 = manager.get_state()
        assert len(state1["messages"]) == 1
        assert state1["messages"][0]["content"] == "Hello"
        
        # Add second message (should append, not replace)
        update2 = {"messages": [{"type": "ai", "content": "Hi there"}]}
        success2 = manager.update_state(update2)
        assert success2 == True
        
        state2 = manager.get_state()
        assert len(state2["messages"]) == 2
        assert state2["messages"][0]["content"] == "Hello"
        assert state2["messages"][1]["content"] == "Hi there"
    
    def test_additive_dict_updates(self):
        """Test LangGraph's additive pattern for dictionary updates."""
        manager = DocumentStateManager()
        
        # Add first agent status
        update1 = {"agent_status": {"agent1": "processing"}}
        success1 = manager.update_state(update1)
        assert success1 == True
        
        state1 = manager.get_state()
        assert state1["agent_status"]["agent1"] == "processing"
        
        # Add second agent status (should merge, not replace)
        update2 = {"agent_status": {"agent2": "completed"}}
        success2 = manager.update_state(update2)
        assert success2 == True
        
        state2 = manager.get_state()
        assert len(state2["agent_status"]) == 2
        assert state2["agent_status"]["agent1"] == "processing"
        assert state2["agent_status"]["agent2"] == "completed"
    
    def test_message_helper_method(self):
        """Test the add_message helper method."""
        manager = DocumentStateManager()
        
        message = {"type": "human", "content": "Test message"}
        success = manager.add_message(message)
        assert success == True
        
        state = manager.get_state()
        assert len(state["messages"]) == 1
        assert state["messages"][0]["content"] == "Test message"
    
    def test_pending_operation_helper_methods(self):
        """Test pending operation helper methods."""
        manager = DocumentStateManager()
        
        # Add pending operation
        operation = PendingOperation(
            operation_type="format_text",
            parameters={"style": "bold"},
            agent_id="formatting_agent"
        )
        
        success = manager.add_pending_operation(operation)
        assert success == True
        
        state = manager.get_state()
        assert len(state["pending_operations"]) == 1
        assert state["pending_operations"][0]["operation_type"] == "format_text"
        
        # Complete the operation
        operation_id = state["pending_operations"][0]["operation_id"]
        result = {"applied": True, "elements_affected": 5}
        
        success = manager.complete_operation(
            operation_id, result, execution_time=0.5, success=True
        )
        assert success == True
        
        updated_state = manager.get_state()
        assert len(updated_state["pending_operations"]) == 0
        assert len(updated_state["completed_operations"]) == 1
        assert updated_state["completed_operations"][0]["operation_id"] == operation_id
        assert updated_state["completed_operations"][0]["result"] == result
        assert updated_state["completed_operations"][0]["execution_time"] == 0.5
    
    def test_agent_status_helper_method(self):
        """Test agent status update helper method."""
        manager = DocumentStateManager()
        
        success = manager.update_agent_status("test_agent", AgentStatus.PROCESSING)
        assert success == True
        
        state = manager.get_state()
        assert state["agent_status"]["test_agent"] == "processing"
        
        # Update to completed
        success = manager.update_agent_status("test_agent", AgentStatus.COMPLETED)
        assert success == True
        
        state = manager.get_state()
        assert state["agent_status"]["test_agent"] == "completed"

@pytest.mark.unit
class TestStateSerialization:
    """Test state serialization and deserialization."""
    
    def test_state_serialization_basic(self):
        """Test basic state serialization to JSON."""
        manager = DocumentStateManager()
        
        # Add some data to serialize
        updates = {
            "current_task": "Test serialization",
            "selected_text": "Sample text",
            "agent_status": {"agent1": "processing"}
        }
        manager.update_state(updates)
        
        # Serialize
        json_str = manager.serialize_state()
        assert isinstance(json_str, str)
        assert len(json_str) > 0
        
        # Verify it's valid JSON
        parsed = json.loads(json_str)
        assert parsed["current_task"] == "Test serialization"
        assert parsed["selected_text"] == "Sample text"
        assert parsed["agent_status"]["agent1"] == "processing"
    
    def test_state_deserialization_basic(self):
        """Test basic state deserialization from JSON."""
        manager = DocumentStateManager()
        
        # Create test state JSON
        test_state = {
            "current_document": {"title": "Test"},
            "cursor_position": {"paragraph": 1},
            "selected_text": "test",
            "document_structure": {},
            "formatting_state": {},
            "messages": [],
            "current_task": "deserialization test",
            "task_history": [],
            "agent_status": {"agent1": "idle"},
            "content_analysis": {},
            "generated_content": [],
            "content_suggestions": [],
            "external_data": {},
            "research_citations": [],
            "api_usage": {},
            "pending_operations": [],
            "completed_operations": [],
            "validation_results": {},
            "last_error": None,
            "retry_count": 0,
            "error_recovery": {},
            "rollback_points": [],
            "user_preferences": {},
            "interaction_history": [],
            "approval_required": [],
            "performance_metrics": {},
            "resource_utilization": {},
            "optimization_recommendations": []
        }
        
        json_str = json.dumps(test_state)
        success = manager.deserialize_state(json_str)
        assert success == True
        
        state = manager.get_state()
        assert state["current_task"] == "deserialization test"
        assert state["agent_status"]["agent1"] == "idle"
    
    def test_serialization_with_complex_data(self):
        """Test serialization with complex nested data structures."""
        manager = DocumentStateManager()
        
        # Add complex data
        updates = {
            "document_structure": {
                "paragraphs": 10,
                "tables": [
                    {"id": "table1", "rows": 5, "cols": 3},
                    {"id": "table2", "rows": 2, "cols": 4}
                ]
            },
            "pending_operations": [
                {
                    "operation_id": "op1",
                    "operation_type": "insert_table",
                    "parameters": {"rows": 3, "cols": 2},
                    "created_timestamp": "2024-01-01T12:00:00Z"
                }
            ]
        }
        manager.update_state(updates)
        
        # Serialize and deserialize
        json_str = manager.serialize_state()
        new_manager = DocumentStateManager()
        success = new_manager.deserialize_state(json_str)
        assert success == True
        
        new_state = new_manager.get_state()
        assert new_state["document_structure"]["paragraphs"] == 10
        assert len(new_state["document_structure"]["tables"]) == 2
        assert new_state["document_structure"]["tables"][0]["id"] == "table1"
        assert len(new_state["pending_operations"]) == 1
        assert new_state["pending_operations"][0]["operation_type"] == "insert_table"

@pytest.mark.unit
class TestStateHistory:
    """Test state history and rollback functionality."""
    
    def test_state_history_creation(self):
        """Test that state history is created on updates."""
        manager = DocumentStateManager()
        
        # Initial state should create one history entry
        history = manager.get_state_history()
        assert len(history) == 1
        assert history[0]["description"] == "Initial state"
        
        # Update should create another history entry
        manager.update_state({"current_task": "History test"})
        
        history = manager.get_state_history()
        assert len(history) == 2
        assert "Before update" in history[1]["description"]
    
    def test_state_rollback(self):
        """Test state rollback to previous snapshot."""
        manager = DocumentStateManager()
        
        # Make initial update
        manager.update_state({"current_task": "Initial task"})
        state1 = manager.get_state()
        
        # Get snapshot ID
        history = manager.get_state_history()
        snapshot_id = history[-1]["snapshot_id"]
        
        # Make another update
        manager.update_state({"current_task": "Updated task"})
        state2 = manager.get_state()
        assert state2["current_task"] == "Updated task"
        
        # Rollback to previous snapshot
        success = manager.rollback_to_snapshot(snapshot_id)
        assert success == True
        
        state3 = manager.get_state()
        assert state3["current_task"] == "Initial task"
    
    def test_rollback_invalid_snapshot(self):
        """Test rollback with invalid snapshot ID."""
        manager = DocumentStateManager()
        
        success = manager.rollback_to_snapshot("invalid_id")
        assert success == False
    
    def test_history_size_limit(self):
        """Test that history maintains size limit."""
        manager = DocumentStateManager()
        
        # Make many updates to exceed history limit
        for i in range(55):  # More than max_history_size (50)
            manager.update_state({"current_task": f"Task {i}"})
        
        history = manager.get_state_history()
        assert len(history) <= 50  # Should be limited by max_history_size

@pytest.mark.unit
class TestThreadSafety:
    """Test thread safety of state management operations."""
    
    def test_concurrent_state_updates(self):
        """Test concurrent state updates from multiple threads."""
        manager = DocumentStateManager()
        results = []
        errors = []
        
        def update_worker(worker_id, num_updates):
            try:
                for i in range(num_updates):
                    success = manager.update_state({
                        "messages": [{
                            "worker": worker_id,
                            "update": i,
                            "content": f"Message from worker {worker_id}, update {i}"
                        }]
                    })
                    results.append((worker_id, i, success))
                    time.sleep(0.001)  # Small delay to increase chance of race conditions
            except Exception as e:
                errors.append((worker_id, e))
        
        # Start multiple worker threads
        threads = []
        num_workers = 5
        updates_per_worker = 10
        
        for worker_id in range(num_workers):
            thread = threading.Thread(target=update_worker, args=(worker_id, updates_per_worker))
            threads.append(thread)
            thread.start()
        
        # Wait for all threads to complete
        for thread in threads:
            thread.join()
        
        # Verify results
        assert len(errors) == 0, f"Errors occurred: {errors}"
        assert len(results) == num_workers * updates_per_worker
        
        # Verify all updates succeeded
        for worker_id, update_id, success in results:
            assert success == True, f"Update failed for worker {worker_id}, update {update_id}"
        
        # Verify final state has all messages
        final_state = manager.get_state()
        assert len(final_state["messages"]) == num_workers * updates_per_worker
    
    def test_concurrent_serialization(self):
        """Test concurrent serialization operations."""
        manager = DocumentStateManager()
        
        # Add some initial data
        manager.update_state({
            "current_task": "Concurrent serialization test",
            "agent_status": {"agent1": "processing"}
        })
        
        serialization_results = []
        errors = []
        
        def serialize_worker(worker_id, num_serializations):
            try:
                for i in range(num_serializations):
                    json_str = manager.serialize_state()
                    parsed = json.loads(json_str)
                    serialization_results.append((worker_id, i, len(json_str)))
            except Exception as e:
                errors.append((worker_id, e))
        
        # Start multiple serialization threads
        threads = []
        num_workers = 3
        serializations_per_worker = 5
        
        for worker_id in range(num_workers):
            thread = threading.Thread(target=serialize_worker, args=(worker_id, serializations_per_worker))
            threads.append(thread)
            thread.start()
        
        # Wait for completion
        for thread in threads:
            thread.join()
        
        # Verify results
        assert len(errors) == 0, f"Serialization errors: {errors}"
        assert len(serialization_results) == num_workers * serializations_per_worker

@pytest.mark.unit
class TestStateValidation:
    """Test state validation functionality."""
    
    def test_valid_update_validation(self):
        """Test validation of valid state updates."""
        manager = DocumentStateManager()
        
        valid_updates = {
            "current_task": "Valid task",
            "selected_text": "Valid text",
            "messages": [{"type": "human", "content": "Valid message"}]
        }
        
        success = manager.update_state(valid_updates)
        assert success == True
    
    def test_invalid_update_validation(self):
        """Test validation rejects invalid state updates."""
        manager = DocumentStateManager()
        
        # Test invalid message type
        invalid_updates = {
            "messages": "This should be a list, not a string"
        }
        
        success = manager.update_state(invalid_updates)
        assert success == False
        
        # Test invalid current_task type
        invalid_updates = {
            "current_task": 123  # Should be string
        }
        
        success = manager.update_state(invalid_updates)
        assert success == False
    
    def test_performance_summary(self):
        """Test performance summary generation."""
        manager = DocumentStateManager()
        
        # Add some operations to track
        operation = PendingOperation(
            operation_type="test_op",
            agent_id="test_agent"
        )
        manager.add_pending_operation(operation)
        manager.update_agent_status("test_agent", AgentStatus.PROCESSING)
        
        summary = manager.get_performance_summary()
        
        # Verify summary structure
        required_keys = [
            "memory_usage_mb", "cpu_usage_percent", "active_operations",
            "completed_operations", "active_agents", "last_update_time",
            "state_history_size"
        ]
        
        for key in required_keys:
            assert key in summary, f"Missing key in performance summary: {key}"
        
        assert summary["active_operations"] == 1
        assert summary["active_agents"] == 1
        assert summary["completed_operations"] == 0
        assert summary["state_history_size"] > 0

if __name__ == "__main__":
    # Run tests when executed directly
    pytest.main([__file__, "-v"])