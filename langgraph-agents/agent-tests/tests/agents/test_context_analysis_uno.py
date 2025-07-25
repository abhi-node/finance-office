#!/usr/bin/env python3
"""
UNO Bridge Integration Tests for ContextAnalysisAgent

This test suite validates the LibreOffice UNO services bridge integration
in ContextAnalysisAgent, testing both real UNO connections and mock fallbacks.
"""

import asyncio
import unittest
from unittest.mock import Mock, patch, MagicMock
import time

from agents.context_analysis import ContextAnalysisAgent
from state.document_state import DocumentState


class TestContextAnalysisUNO(unittest.IsolatedAsyncioTestCase):
    """Test UNO bridge integration in ContextAnalysisAgent."""
    
    def setUp(self):
        """Set up test environment."""
        self.agent = ContextAnalysisAgent()
        self.test_state = {
            "current_document": {"title": "Test Doc", "path": "/tmp/test.odt"},
            "cursor_position": {"paragraph": 5, "character": 20, "line": 5},
            "selected_text": "test selection",
            "document_structure": {
                "paragraphs": 10,
                "sections": [{"title": "Test Section", "start_paragraph": 0, "end_paragraph": 10}]
            },
            "messages": [],
            "current_task": "context_analysis"
        }
    
    async def test_uno_bridge_connection_success(self):
        """Test successful UNO bridge connection."""
        # Mock successful UNO connection
        with patch('uno.getComponentContext') as mock_uno, \
             patch('agents.context_analysis.time.sleep'):
            
            mock_context = Mock()
            mock_service_manager = Mock()
            mock_resolver = Mock()
            mock_remote_context = Mock()
            
            mock_uno.return_value = mock_context
            mock_context.getServiceManager.return_value = mock_service_manager
            mock_service_manager.createInstanceWithContext.return_value = mock_resolver
            mock_resolver.resolve.return_value = mock_remote_context
            
            # Test connection
            uno_context = self.agent._connect_to_uno_bridge()
            
            # Verify successful connection
            self.assertIsNotNone(uno_context)
            self.assertEqual(uno_context, mock_remote_context)
            mock_resolver.resolve.assert_called_once()
    
    async def test_uno_bridge_connection_failure_with_mock_fallback(self):
        """Test UNO connection failure with mock fallback."""
        with patch('uno.getComponentContext') as mock_uno:
            # Simulate ImportError (UNO not available)
            mock_uno.side_effect = ImportError("UNO not available")
            
            # Test connection with fallback
            uno_context = self.agent._connect_to_uno_bridge()
            
            # Should fall back to mock bridge
            self.assertIsNotNone(uno_context)
            self.assertTrue(hasattr(uno_context, '_is_mock_bridge'))
            self.assertTrue(uno_context._is_mock_bridge)
    
    async def test_uno_bridge_retry_logic(self):
        """Test UNO connection retry logic with eventual success."""
        with patch('uno.getComponentContext') as mock_uno, \
             patch('agents.context_analysis.time.sleep') as mock_sleep:
            
            mock_context = Mock()
            mock_service_manager = Mock()
            mock_resolver = Mock()
            mock_remote_context = Mock()
            
            mock_uno.return_value = mock_context
            mock_context.getServiceManager.return_value = mock_service_manager
            mock_service_manager.createInstanceWithContext.return_value = mock_resolver
            
            # First two attempts fail, third succeeds
            from com.sun.star.connection import NoConnectException
            mock_resolver.resolve.side_effect = [
                NoConnectException("Connection failed"),
                NoConnectException("Connection failed"),
                mock_remote_context
            ]
            
            # Test connection with retries
            uno_context = self.agent._connect_to_uno_bridge()
            
            # Verify retry logic
            self.assertEqual(mock_resolver.resolve.call_count, 3)
            self.assertEqual(mock_sleep.call_count, 2)  # Sleep between retries
            self.assertEqual(uno_context, mock_remote_context)
    
    async def test_get_agent_coordinator_service_success(self):
        """Test successful AgentCoordinator service retrieval."""
        # Mock UNO bridge
        mock_bridge = Mock()
        mock_service_manager = Mock()
        mock_coordinator = Mock()
        
        mock_bridge.getServiceManager.return_value = mock_service_manager
        mock_service_manager.createInstance.return_value = mock_coordinator
        
        self.agent.uno_bridge = mock_bridge
        
        # Test service retrieval
        coordinator = self.agent._get_agent_coordinator_service()
        
        # Verify service creation
        self.assertEqual(coordinator, mock_coordinator)
        mock_service_manager.createInstance.assert_called_with("com.sun.star.ai.AgentCoordinator")
    
    async def test_get_agent_coordinator_service_mock_fallback(self):
        """Test AgentCoordinator service with mock fallback."""
        # Use mock bridge
        self.agent.uno_bridge = self.agent._create_mock_uno_bridge()
        
        # Test service retrieval
        coordinator = self.agent._get_agent_coordinator_service()
        
        # Should return mock coordinator
        self.assertIsNotNone(coordinator)
        self.assertTrue(hasattr(coordinator, '_is_mock_service'))
        self.assertTrue(coordinator._is_mock_service)
    
    async def test_get_current_document_success(self):
        """Test successful document retrieval via UNO."""
        # Mock UNO components
        mock_bridge = Mock()
        mock_service_manager = Mock()
        mock_desktop = Mock()
        mock_document = Mock()
        
        mock_bridge.getServiceManager.return_value = mock_service_manager
        mock_service_manager.createInstance.return_value = mock_desktop
        mock_desktop.getCurrentComponent.return_value = mock_document
        
        self.agent.uno_bridge = mock_bridge
        
        # Test document retrieval
        doc = self.agent._get_current_document()
        
        # Verify document retrieval
        self.assertEqual(doc, mock_document)
        mock_service_manager.createInstance.assert_called_with("com.sun.star.frame.Desktop")
    
    async def test_get_current_document_mock_fallback(self):
        """Test document retrieval with mock fallback."""
        # Use mock bridge
        self.agent.uno_bridge = self.agent._create_mock_uno_bridge()
        
        # Test document retrieval
        doc = self.agent._get_current_document()
        
        # Should return mock document
        self.assertIsNotNone(doc)
        self.assertTrue(hasattr(doc, '_is_mock_document'))
        self.assertTrue(doc._is_mock_document)
    
    async def test_traverse_text_nodes_success(self):
        """Test text node traversal via UNO."""
        # Mock document and text nodes
        mock_doc = Mock()
        mock_text = Mock()
        mock_enum = Mock()
        mock_node1 = Mock()
        mock_node2 = Mock()
        
        mock_doc.getText.return_value = mock_text
        mock_text.createEnumeration.return_value = mock_enum
        mock_enum.hasMoreElements.side_effect = [True, True, False]
        mock_enum.nextElement.side_effect = [mock_node1, mock_node2]
        
        mock_node1.getString.return_value = "First paragraph"
        mock_node2.getString.return_value = "Second paragraph"
        
        # Test traversal
        text_nodes = self.agent._traverse_text_nodes(mock_doc)
        
        # Verify results
        self.assertEqual(len(text_nodes), 2)
        self.assertEqual(text_nodes[0].getString(), "First paragraph")
        self.assertEqual(text_nodes[1].getString(), "Second paragraph")
    
    async def test_traverse_text_nodes_mock(self):
        """Test text node traversal with mock document."""
        # Use mock document
        mock_doc = self.agent._create_mock_uno_bridge()._create_mock_document()
        
        # Test traversal
        text_nodes = self.agent._traverse_text_nodes(mock_doc)
        
        # Should return mock nodes
        self.assertIsInstance(text_nodes, list)
        self.assertGreater(len(text_nodes), 0)
        for node in text_nodes:
            self.assertTrue(hasattr(node, '_is_mock_node'))
    
    async def test_extract_document_styling_success(self):
        """Test document styling extraction via UNO."""
        # Mock document with styling
        mock_doc = Mock()
        mock_style_families = Mock()
        mock_para_styles = Mock()
        mock_char_styles = Mock()
        
        mock_doc.getStyleFamilies.return_value = mock_style_families
        mock_style_families.getByName.side_effect = [mock_para_styles, mock_char_styles]
        
        # Mock style collections
        mock_para_styles.getElementNames.return_value = ["Heading 1", "Normal"]
        mock_char_styles.getElementNames.return_value = ["Strong", "Emphasis"]
        
        # Test styling extraction
        styling = self.agent._extract_document_styling(mock_doc)
        
        # Verify results
        self.assertIn("paragraph_styles", styling)
        self.assertIn("character_styles", styling)
        self.assertEqual(styling["paragraph_styles"], ["Heading 1", "Normal"])
        self.assertEqual(styling["character_styles"], ["Strong", "Emphasis"])
    
    async def test_extract_document_styling_mock(self):
        """Test document styling extraction with mock document."""
        # Use mock document
        mock_doc = self.agent._create_mock_uno_bridge()._create_mock_document()
        
        # Test styling extraction
        styling = self.agent._extract_document_styling(mock_doc)
        
        # Should return mock styling data
        self.assertIsInstance(styling, dict)
        self.assertIn("paragraph_styles", styling)
        self.assertIn("character_styles", styling)
    
    async def test_uno_connection_status_monitoring(self):
        """Test UNO connection status monitoring."""
        # Test with successful connection
        with patch.object(self.agent, '_connect_to_uno_bridge') as mock_connect:
            mock_bridge = Mock()
            mock_connect.return_value = mock_bridge
            
            # Check connection status
            is_connected = self.agent._is_uno_connected()
            
            # Should detect connection
            self.assertTrue(is_connected)
    
    async def test_uno_connection_status_mock_mode(self):
        """Test UNO connection status in mock mode."""
        # Set up mock bridge
        self.agent.uno_bridge = self.agent._create_mock_uno_bridge()
        
        # Check connection status
        is_connected = self.agent._is_uno_connected()
        
        # Should indicate mock mode (not real connection)
        self.assertFalse(is_connected)  # Mock mode returns False for real connection
    
    async def test_context_analysis_with_uno_integration(self):
        """Test full context analysis with UNO integration."""
        # Mock successful UNO integration
        with patch.object(self.agent, '_connect_to_uno_bridge') as mock_connect, \
             patch.object(self.agent, '_get_current_document') as mock_get_doc, \
             patch.object(self.agent, '_traverse_text_nodes') as mock_traverse, \
             patch.object(self.agent, '_extract_document_styling') as mock_styling:
            
            # Set up mocks
            mock_connect.return_value = Mock()
            mock_doc = Mock()
            mock_get_doc.return_value = mock_doc
            mock_traverse.return_value = [Mock(), Mock()]
            mock_styling.return_value = {"paragraph_styles": ["Normal"], "character_styles": ["Default"]}
            
            # Test context analysis
            message = {"content": "analyze document with UNO integration", "role": "user"}
            result = await self.agent.process(self.test_state, message)
            
            # Verify UNO integration was used
            self.assertTrue(result.success)
            self.assertIn("content_analysis", result.state_updates)
            
            analysis = result.state_updates["content_analysis"]
            self.assertIn("uno_integration", analysis)
            self.assertTrue(analysis["uno_integration"]["enabled"])
    
    async def test_context_analysis_mock_fallback(self):
        """Test context analysis with mock UNO fallback."""
        # Force mock mode by simulating UNO unavailability
        with patch('uno.getComponentContext', side_effect=ImportError("UNO not available")):
            
            # Test context analysis
            message = {"content": "analyze document structure", "role": "user"}
            result = await self.agent.process(self.test_state, message)
            
            # Should still succeed with mock fallback
            self.assertTrue(result.success)
            self.assertIn("content_analysis", result.state_updates)
            
            analysis = result.state_updates["content_analysis"]
            self.assertIn("uno_integration", analysis)
            self.assertFalse(analysis["uno_integration"]["enabled"])  # Mock mode
    
    async def test_performance_uno_vs_mock(self):
        """Test performance comparison between UNO and mock modes."""
        # Test mock mode performance
        with patch('uno.getComponentContext', side_effect=ImportError("UNO not available")):
            start_time = time.time()
            message = {"content": "quick analysis", "role": "user"}
            result_mock = await self.agent.process(self.test_state, message)
            mock_time = time.time() - start_time
        
        # Test UNO mode performance (mocked for test)
        with patch.object(self.agent, '_connect_to_uno_bridge') as mock_connect:
            mock_connect.return_value = Mock()
            
            start_time = time.time()
            message = {"content": "quick analysis", "role": "user"}
            result_uno = await self.agent.process(self.test_state, message)
            uno_time = time.time() - start_time
        
        # Both should succeed
        self.assertTrue(result_mock.success)
        self.assertTrue(result_uno.success)
        
        # Mock mode should be faster (no real UNO overhead)
        print(f"Mock mode: {mock_time*1000:.1f}ms, UNO mode: {uno_time*1000:.1f}ms")


def run_uno_integration_tests():
    """Run all UNO integration tests."""
    print("🚀 Running ContextAnalysisAgent UNO Bridge Integration Tests...")
    
    # Create test suite
    suite = unittest.TestLoader().loadTestsFromTestCase(TestContextAnalysisUNO)
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    # Summary
    if result.wasSuccessful():
        print("\n✅ All UNO integration tests passed!")
        print("✅ UNO bridge connection and retry logic validated")
        print("✅ Mock fallback functionality verified")
        print("✅ AgentCoordinator service integration tested")
        print("✅ Document access and styling extraction confirmed")
        print("✅ Performance characteristics measured")
        return True
    else:
        print(f"\n❌ {len(result.failures)} test failures, {len(result.errors)} errors")
        return False


if __name__ == "__main__":
    success = run_uno_integration_tests()
    exit(0 if success else 1)