"""CosyVoice cross-lingual voice clone."""
import sys, os, winsound
sys.path.insert(0, "E:/repo/CosyVoice")
sys.path.insert(0, "E:/repo/CosyVoice/third_party/Matcha-TTS")
import torch, torchaudio
from cosyvoice.cli.cosyvoice import AutoModel

text, ref_audio, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
ref_text = "La rosquilla que guarde en mi escritorio ha desaparecido, seguro fue ese ladron invisible que marge dice que no existe"

model = AutoModel(model_dir="iic/CosyVoice2-0.5B", load_jit=False, fp16=True)
audio_chunks = []
for chunk in model.inference_zero_shot(text, ref_text, ref_audio):
    audio_chunks.append(chunk["tts_speech"])
full_audio = torch.cat(audio_chunks, dim=1)
torchaudio.save(out_path, full_audio, model.sample_rate)
winsound.PlaySound(out_path, winsound.SND_FILENAME)
