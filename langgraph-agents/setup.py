#!/usr/bin/env python3
"""
Setup configuration for LibreOffice AI Writing Assistant LangGraph Multi-Agent System
"""

from setuptools import setup, find_packages
from pathlib import Path

# Read requirements
requirements_path = Path(__file__).parent / "requirements.txt"
with open(requirements_path, 'r') as f:
    requirements = [
        line.strip() 
        for line in f 
        if line.strip() and not line.startswith('#')
    ]

# Read README for long description
readme_path = Path(__file__).parent / "README.md"
with open(readme_path, 'r', encoding='utf-8') as f:
    long_description = f.read()

setup(
    name="libreoffice-ai-agents",
    version="1.0.0",
    author="LibreOffice AI Integration Team",
    author_email="ai-team@libreoffice.org",
    description="LangGraph Multi-Agent System for LibreOffice Writer AI Assistant",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/LibreOffice/core",
    packages=find_packages(),
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Office/Business :: Office Suites",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "License :: OSI Approved :: Mozilla Public License 2.0 (MPL 2.0)",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9", 
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Operating System :: OS Independent",
    ],
    python_requires=">=3.8",
    install_requires=requirements,
    extras_require={
        "dev": [
            "pytest>=7.4.0",
            "pytest-asyncio>=0.21.0",
            "pytest-mock>=3.11.0",
            "pytest-cov>=4.1.0",
            "black>=23.0.0",
            "isort>=5.12.0",
            "mypy>=1.5.0",
            "pre-commit>=3.3.0",
        ],
        "performance": [
            "prometheus-client>=0.16.0",
            "psutil>=5.9.0",
        ],
        "financial": [
            "alpha-vantage>=2.3.1",
            "yfinance>=0.2.18",
        ]
    },
    entry_points={
        "console_scripts": [
            "libreoffice-agents=langgraph_agents.cli:main",
        ],
    },
    include_package_data=True,
    package_data={
        "langgraph_agents": [
            "config/*.json",
            "templates/*.xml",
            "tests/fixtures/*.json",
        ],
    },
    zip_safe=False,
    keywords="libreoffice ai agents langgraph nlp document-processing",
    project_urls={
        "Bug Reports": "https://bugs.documentfoundation.org/",
        "Source": "https://gerrit.libreoffice.org/",
        "Documentation": "https://docs.libreoffice.org/",
    },
)