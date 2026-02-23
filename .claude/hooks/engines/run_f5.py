"""F5-TTS voice clone."""
import sys, winsound
import soundfile as sf
from f5_tts.api import F5TTS

text, ref_audio, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
ref_text = "La rosquilla que guarde en mi escritorio ha desaparecido, seguro fue ese ladron invisible que marge dice que no existe"

model = F5TTS(model="F5TTS_v1_Base", device="cuda")
wav, sr, _ = model.infer(ref_file=ref_audio, ref_text=ref_text, gen_text=text)
sf.write(out_path, wav, sr)
winsound.PlaySound(out_path, winsound.SND_FILENAME)
