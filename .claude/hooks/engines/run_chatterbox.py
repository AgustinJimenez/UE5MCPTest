"""Chatterbox multilingual voice clone."""
import sys, winsound
import soundfile as sf
from chatterbox.mtl_tts import ChatterboxMultilingualTTS

text, ref_audio, out_path = sys.argv[1], sys.argv[2], sys.argv[3]

model = ChatterboxMultilingualTTS.from_pretrained(device="cuda")
wav = model.generate(text, audio_prompt_path=ref_audio, language_id="es")
# wav is a tensor [1, samples], convert to numpy
audio_np = wav.squeeze(0).cpu().numpy()
sf.write(out_path, audio_np, model.sr, subtype="PCM_16")
winsound.PlaySound(out_path, winsound.SND_FILENAME)
