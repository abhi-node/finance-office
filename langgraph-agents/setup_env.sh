#!/bin/bash
# Setup script for LibreOffice AI Writing Assistant environment configuration

echo "🚀 LibreOffice AI Writing Assistant - Environment Setup"
echo "======================================================"

# Check if .env already exists
if [ -f ".env" ]; then
    echo "⚠️  .env file already exists!"
    read -p "Do you want to overwrite it? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Setup cancelled."
        exit 1
    fi
fi

# Copy example file
if [ -f ".env.example" ]; then
    cp .env.example .env
    echo "✅ Copied .env.example to .env"
    echo ""
    echo "📝 Next steps:"
    echo "1. Edit .env file with your API keys:"
    echo "   - OPENAI_API_KEY (required)"
    echo "   - ANTHROPIC_API_KEY (alternative to OpenAI)"
    echo "   - Other optional API keys as needed"
    echo ""
    echo "2. You can edit the file with:"
    echo "   nano .env    (or your preferred editor)"
    echo ""
    echo "3. Test the configuration with:"
    echo "   python3 -c \"from config import get_config; print('Config loaded successfully!')\""
    echo ""
    echo "🔐 Important: Never commit the .env file to version control!"
    echo "   (It's already in .gitignore)"
else
    echo "❌ .env.example file not found!"
    echo "Make sure you're in the correct directory."
    exit 1
fi