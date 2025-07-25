#!/usr/bin/env python3
"""
Test configuration loading and validate API keys are properly loaded.
"""

from config import get_config

def test_config_loading():
    """Test that configuration loads properly."""
    print("🔧 Testing Configuration Loading...")
    
    try:
        config = get_config()
        print("✅ Configuration loaded successfully!")
        
        # Check API configuration
        print(f"\n🔑 API Configuration:")
        print(f"  OpenAI API Key: {'✅ Set' if config.api.openai_api_key else '❌ Not set'}")
        print(f"  Anthropic API Key: {'✅ Set' if config.api.anthropic_api_key else '❌ Not set'}")
        print(f"  Alpha Vantage Key: {'✅ Set' if config.api.alpha_vantage_key else '❌ Not set'}")
        print(f"  Default Model: {config.api.default_model}")
        
        # Check performance settings
        print(f"\n⚡ Performance Configuration:")
        print(f"  Simple Operation Target: {config.performance.simple_operation_target}s")
        print(f"  Moderate Operation Target: {config.performance.moderate_operation_target}s")
        print(f"  Complex Operation Target: {config.performance.complex_operation_target}s")
        print(f"  Max Memory: {config.performance.max_memory_mb}MB")
        print(f"  Cache Enabled: {config.performance.enable_analysis_cache}")
        
        # Check LibreOffice settings
        print(f"\n📄 LibreOffice Configuration:")
        print(f"  UNO Connection Timeout: {config.libreoffice.uno_connection_timeout}s")
        print(f"  Document Operation Timeout: {config.libreoffice.document_operation_timeout}s")  
        print(f"  Max Document Size: {config.libreoffice.max_document_size_mb}MB")
        
        # Check system paths
        print(f"\n📁 System Paths:")
        print(f"  Data Directory: {config.agent_data_dir}")
        print(f"  Log Directory: {config.log_dir}")
        print(f"  Cache Directory: {config.cache_dir}")
        print(f"  Log Level: {config.log_level}")
        
        # Validate that at least one AI API key is set
        if not config.api.openai_api_key and not config.api.anthropic_api_key:
            print("\n⚠️  WARNING: No AI API keys configured!")
            print("   Please set either OPENAI_API_KEY or ANTHROPIC_API_KEY in your .env file")
            return False
        
        print("\n🎉 Configuration is valid and ready to use!")
        return True
        
    except Exception as e:
        print(f"❌ Configuration loading failed: {e}")
        return False

if __name__ == "__main__":
    success = test_config_loading()
    
    if not success:
        print("\n💡 Setup instructions:")
        print("1. Copy .env.example to .env:")
        print("   cp .env.example .env")
        print("2. Edit .env with your API keys")
        print("3. Run this test again")
    
    exit(0 if success else 1)