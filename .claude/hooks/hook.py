"""
Unified Claude Code hook handler. Dispatches TTS based on event type.

Usage (called by Claude Code hooks):
  python .claude/hooks/hook.py session
  python .claude/hooks/hook.py submit
  python .claude/hooks/hook.py notification
  python .claude/hooks/hook.py stop
"""
import sys
import json
import subprocess
import re
import os

HOOKS_DIR = os.path.dirname(os.path.abspath(__file__))
USE_HOMER = True  # Toggle: True = Qwen3-TTS Homer (Spanish), False = KittenTTS Kiki (English)

event = sys.argv[1] if len(sys.argv) > 1 else "stop"
data = json.load(sys.stdin)

if event == "session":
    msg = "Listo para trabajar, que necesitas?" if USE_HOMER else "Ready to work, what do you need?"

elif event == "submit":
    # msg = "Entendido, ya me pongo a trabajar" if USE_HOMER else "Got it, working on it"
    sys.exit(0)

elif event == "notification":
    default = "Oye, necesito tu atencion por aqui" if USE_HOMER else "Hey, I need your attention"
    msg = data.get("message", default)

elif event == "stop":
    msg = data.get("last_assistant_message", "")
    # Strip code blocks and markdown formatting
    msg = re.sub(r"```[\s\S]*?```", "", msg)
    msg = re.sub(r"[`*#|\[\]{}()>_~]", "", msg)
    msg = re.sub(r"\s+", " ", msg).strip()
    if len(msg) > 200:
        msg = msg[:200].rsplit(" ", 1)[0]
    if not msg:
        msg = "Listo, esperando la siguiente tarea" if USE_HOMER else "Done, ready for next task"

else:
    msg = "Oye" if USE_HOMER else "Hey"

tts_args = [sys.executable, os.path.join(HOOKS_DIR, "tts.py")]
if USE_HOMER:
    tts_args.append("--homer")
tts_args.append(msg)

subprocess.Popen(tts_args)
