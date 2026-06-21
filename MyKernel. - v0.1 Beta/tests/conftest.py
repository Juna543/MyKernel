"""
conftest.py — Adds src/ and src/mykernel/ to sys.path so both
`from mykernel import ...` AND `import mykernel_native` work when
running pytest from the project root, without needing an editable
install during early development.
"""

import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
SRC_DIR = PROJECT_ROOT / "src"
MYKERNEL_DIR = SRC_DIR / "mykernel"
TOOLS_DIR = PROJECT_ROOT / "tools"

for path in (SRC_DIR, MYKERNEL_DIR, TOOLS_DIR):
    if str(path) not in sys.path:
        sys.path.insert(0, str(path))
