"""
Unified TTS dispatcher. Routes to the correct venv's python for each engine.

Engines:
  (default)    KittenTTS    — fast, CPU, Kiki voice
  --homer      Qwen3-TTS    — Homer Simpson voice clone (Spanish LATAM, GPU, slow)
  --f5         F5-TTS       — voice clone (GPU, fast)
  --chatterbox Chatterbox   — voice clone multilingual (GPU, fast)
  --cosyvoice  CosyVoice    — voice clone cross-lingual (GPU, fast)
"""
import os
import sys
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("text", nargs="*", default=["Hey, I need your attention over here."])
parser.add_argument("--homer", action="store_true", help="Qwen3-TTS Homer voice (Spanish)")
parser.add_argument("--f5", action="store_true", help="F5-TTS voice clone")
parser.add_argument("--chatterbox", action="store_true", help="Chatterbox multilingual voice clone")
parser.add_argument("--cosyvoice", action="store_true", help="CosyVoice cross-lingual voice clone")
parser.add_argument("--voice", default="Kiki", help="KittenTTS voice")
args = parser.parse_args()

text = " ".join(args.text)
script_dir = os.path.dirname(os.path.abspath(__file__))
engine_dir = os.path.join(script_dir, "engines")

# Each engine has its own script run with its venv's python
ENGINES = {
    "homer":      (sys.executable,                                os.path.join(engine_dir, "run_homer.py")),
    "f5":         ("E:/repo/F5-TTS/.venv/Scripts/python.exe",     os.path.join(engine_dir, "run_f5.py")),
    "chatterbox": ("E:/repo/Chatterbox/.venv/Scripts/python.exe", os.path.join(engine_dir, "run_chatterbox.py")),
    "cosyvoice":  ("E:/repo/CosyVoice/.venv/Scripts/python.exe",  os.path.join(engine_dir, "run_cosyvoice.py")),
}

# Determine which engine
engine = None
for name in ENGINES:
    if getattr(args, name, False):
        engine = name
        break

if engine:
    python_exe, script = ENGINES[engine]
    ref_audio = os.path.join(script_dir, "homer-simpson-spanish-latam-audio.wav")
    out_path = os.path.join(script_dir, "tts_output.wav")
    subprocess.run([python_exe, script, text, ref_audio, out_path], check=True)
else:
    # KittenTTS — runs in current python (has deps available)
    import winsound
    venv_packages = os.path.join("E:/repo/KittenTTS", ".venv", "Lib", "site-packages")
    for pkg in ["nvidia/cudnn/bin", "nvidia/cublas/bin"]:
        dll_dir = os.path.join(venv_packages, pkg)
        if os.path.isdir(dll_dir):
            os.add_dll_directory(dll_dir)
            os.environ["PATH"] = dll_dir + os.pathsep + os.environ.get("PATH", "")

    from kittentts import KittenTTS
    import soundfile as sf

    m = KittenTTS("KittenML/kitten-tts-mini-0.8")
    audio = m.generate(text, voice=args.voice)
    out_path = os.path.join(script_dir, "tts_output.wav")
    sf.write(out_path, audio, 24000, subtype="PCM_16")
    winsound.PlaySound(out_path, winsound.SND_FILENAME)
