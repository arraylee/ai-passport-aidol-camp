#!/bin/zsh
set -e
TOOL_DIR="${0:A:h}"
for tool_python in "$TOOL_DIR/.venv/bin/python" "$HOME/Documents/ChatGPT/AI PASSPORT/work/hideseek-radio-ble/.venv/bin/python" /opt/homebrew/bin/python3 /usr/local/bin/python3 /usr/bin/python3; do
 if [[ -x "$tool_python" ]] && "$tool_python" -c 'import bleak' >/dev/null 2>&1; then
  exec "$tool_python" "$TOOL_DIR/server.py" "$@"
 fi
done
print '首次使用需要安装蓝牙运行环境，正在准备…'
/usr/bin/python3 -m venv "$TOOL_DIR/.venv"
"$TOOL_DIR/.venv/bin/python" -m pip install -r "$TOOL_DIR/requirements.txt"
exec "$TOOL_DIR/.venv/bin/python" "$TOOL_DIR/server.py" "$@"
