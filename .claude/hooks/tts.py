"""
Unified TTS script. Default: KittenTTS (fast, CPU). Optional: Qwen3-TTS Homer voice clone.

Usage:
  python tts.py "message"                  # KittenTTS with Kiki voice
  python tts.py --homer "mensaje"          # Qwen3-TTS Homer Simpson (Spanish)
  python tts.py --voice Jasper "message"   # KittenTTS with specific voice
"""
import os
import sys
import argparse
import winsound

parser = argparse.ArgumentParser()
parser.add_argument("text", nargs="*", default=["Hey, I need your attention over here."])
parser.add_argument("--homer", action="store_true", help="Use Qwen3-TTS Homer voice (Spanish)")
parser.add_argument("--voice", default="Kiki", help="KittenTTS voice (Bella/Jasper/Luna/Bruno/Rosie/Hugo/Kiki/Leo)")
args = parser.parse_args()

text = " ".join(args.text)
script_dir = os.path.dirname(os.path.abspath(__file__))

if args.homer:
    sys.path.insert(0, "E:/repo/Qwen3-TTS")
    import torch
    import soundfile as sf
    from qwen_tts import Qwen3TTSModel

    model = Qwen3TTSModel.from_pretrained(
        "Qwen/Qwen3-TTS-12Hz-1.7B-Base",
        device_map="cuda:0",
        dtype=torch.bfloat16,
    )
    ref_audio = r"C:\Users\agus_\Downloads\homer-simpson-spanish-latam-audio.wav"
    ref_text = "La rosquilla que guarde en mi escritorio ha desaparecido, seguro fue ese ladron invisible que marge dice que no existe"

    wavs, sr = model.generate_voice_clone(
        text=text, language="Spanish", ref_audio=ref_audio, ref_text=ref_text,
    )
    out_path = os.path.join(script_dir, "tts_output.wav")
    sf.write(out_path, wavs[0], sr)
else:
    # Add CUDA DLL directories for onnxruntime
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
