"""
Unified Claude Code hook handler. Dispatches TTS based on event type.

Usage (called by Claude Code hooks):
  python .claude/hooks/hook.py session
  python .claude/hooks/hook.py submit
  python .claude/hooks/hook.py notification
  python .claude/hooks/hook.py stop

Engine options (set TTS_ENGINE below):
  "kitten"      — KittenTTS Kiki (fast, CPU, English)
  "homer"       — Qwen3-TTS Homer voice clone (slow, GPU, Spanish)
  "f5"          — F5-TTS voice clone (fast, GPU, multilingual)
  "chatterbox"  — Chatterbox multilingual voice clone (fast, GPU)
  "cosyvoice"   — CosyVoice cross-lingual voice clone (fast, GPU)
"""
import sys
import json
import subprocess
import re
import os

HOOKS_DIR = os.path.dirname(os.path.abspath(__file__))

# ── Engine selection ──
# Change this to switch TTS engine for all hooks
TTS_ENGINE = "homer"  # "kitten" | "homer" | "f5" | "chatterbox" | "cosyvoice"

# Spanish engines (voice cloning with Homer reference audio)
SPANISH_ENGINES = {"homer", "f5", "chatterbox", "cosyvoice"}
use_spanish = TTS_ENGINE in SPANISH_ENGINES

event = sys.argv[1] if len(sys.argv) > 1 else "stop"
data = json.load(sys.stdin)

if event == "session":
    msg = "Listo para trabajar, que necesitas?" if use_spanish else "Ready to work, what do you need?"

elif event == "submit":
    # msg = "Entendido, ya me pongo a trabajar" if use_spanish else "Got it, working on it"
    sys.exit(0)

elif event == "notification":
    if use_spanish:
        msg = "Hey Agus, ven a revizar aqui un rato"
    else:
        msg = data.get("message", "Hey, I need your attention")

elif event == "stop":
    msg = data.get("last_assistant_message", "")
    # Strip code blocks and markdown formatting
    msg = re.sub(r"```[\s\S]*?```", "", msg)
    msg = re.sub(r"[`*#|\[\]{}()>_~]", "", msg)
    msg = re.sub(r"\s+", " ", msg).strip()
    if len(msg) > 200:
        msg = msg[:200].rsplit(" ", 1)[0]
    if not msg:
        msg = "Listo, esperando la siguiente tarea" if use_spanish else "Done, ready for next task"

else:
    msg = "Oye" if use_spanish else "Hey"

tts_args = [sys.executable, os.path.join(HOOKS_DIR, "tts.py")]
if TTS_ENGINE != "kitten":
    tts_args.append("--" + TTS_ENGINE)
tts_args.append(msg)

subprocess.Popen(tts_args)
