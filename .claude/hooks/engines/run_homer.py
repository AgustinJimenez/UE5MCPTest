"""Qwen3-TTS Homer Simpson voice clone (Spanish LATAM)."""
import sys, os
sys.path.insert(0, "E:/repo/Qwen3-TTS")
import torch, soundfile as sf, winsound
from qwen_tts import Qwen3TTSModel

text, ref_audio, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
ref_text = "La rosquilla que guarde en mi escritorio ha desaparecido, seguro fue ese ladron invisible que marge dice que no existe"

model = Qwen3TTSModel.from_pretrained("Qwen/Qwen3-TTS-12Hz-1.7B-Base", device_map="cuda:0", dtype=torch.bfloat16)
wavs, sr = model.generate_voice_clone(text=text, language="Spanish", ref_audio=ref_audio, ref_text=ref_text)
sf.write(out_path, wavs[0], sr)
winsound.PlaySound(out_path, winsound.SND_FILENAME)
