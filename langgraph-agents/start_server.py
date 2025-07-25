#!/usr/bin/env python3
"""
LibreOffice LangGraph API Server Startup Script

This script starts the FastAPI server with proper configuration for LibreOffice integration.
Handles environment setup, logging configuration, and graceful shutdown.
"""

import asyncio
import logging
import os
import signal
import sys
from pathlib import Path

import uvicorn
from config import get_config

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler('logs/api_server.log', mode='a')
    ]
)

logger = logging.getLogger(__name__)

def setup_environment():
    """Set up environment and directories"""
    # Create necessary directories
    directories = ['logs', 'cache', 'data']
    for dir_name in directories:
        Path(dir_name).mkdir(exist_ok=True)
    
    # Load configuration
    config = get_config()
    logger.info(f"Configuration loaded - Log level: {config.log_level}")
    
    # Set environment variables if needed
    if not os.getenv("PYTHONPATH"):
        current_dir = Path(__file__).parent
        os.environ["PYTHONPATH"] = str(current_dir)
    
    return config

def create_server_config():
    """Create uvicorn server configuration"""
    return {
        "app": "api_server:app",
        "host": "0.0.0.0",
        "port": 8000,
        "log_level": "info",
        "access_log": True,
        "reload": False,  # Set to True for development
        "workers": 1,  # Single worker for WebSocket state management
        "loop": "asyncio",
        "http": "httptools",
        "lifespan": "on"
    }

def signal_handler(signum, frame):
    """Handle shutdown signals gracefully"""
    logger.info(f"Received signal {signum}, shutting down gracefully...")
    sys.exit(0)

def main():
    """Main server startup function"""
    print("🚀 LibreOffice LangGraph API Server")
    print("=" * 50)
    
    try:
        # Set up environment
        config = setup_environment()
        
        # Register signal handlers
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)
        
        # Create server configuration
        server_config = create_server_config()
        
        # Display startup information
        print(f"🌐 Server starting on http://localhost:{server_config['port']}")
        print(f"📊 Expected LibreOffice endpoints:")
        print(f"   • POST http://localhost:8000/api/simple")
        print(f"   • POST http://localhost:8000/api/moderate") 
        print(f"   • POST http://localhost:8000/api/complex")
        print(f"   • POST http://localhost:8000/api/process")
        print(f"   • WS   ws://localhost:8000/ws/libreoffice")
        print(f"📋 Utility endpoints:")
        print(f"   • GET  http://localhost:8000/health")
        print(f"   • GET  http://localhost:8000/status")
        print("=" * 50)
        print(f"🔧 Configuration:")
        print(f"   • Workers: {server_config['workers']}")
        print(f"   • Log Level: {server_config['log_level']}")
        print(f"   • Reload: {server_config['reload']}")
        print("=" * 50)
        print("🎯 Ready for LibreOffice connections!")
        print("   Press Ctrl+C to stop the server")
        print("=" * 50)
        
        # Start the server
        uvicorn.run(**server_config)
        
    except KeyboardInterrupt:
        logger.info("Server shutdown initiated by user")
    except Exception as e:
        logger.error(f"Failed to start server: {e}")
        sys.exit(1)
    finally:
        logger.info("Server shutdown complete")

if __name__ == "__main__":
    main()