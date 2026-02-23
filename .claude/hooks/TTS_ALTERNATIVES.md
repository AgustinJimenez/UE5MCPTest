# Alternativas TTS con Voice Cloning para Homer Simpson (Spanish LATAM)

**Contexto**: Actualmente usamos Qwen3-TTS 1.7B-Base para clonar la voz de Homer Simpson en español LATAM desde un clip de ~10 segundos. Funciona pero es lento (~10x mas lento que tiempo real). Buscamos alternativas mas rapidas.

---

## Top 3 Recomendaciones

### 1. F5-TTS (Mejor opcion general)
- **Repo**: https://github.com/SWivid/F5-TTS
- **Modelo**: ~330M parametros
- **Velocidad**: RTF ~0.15 (6.7x mas rapido que tiempo real) — **~60-70x mas rapido que Qwen3-TTS**
- **Voice cloning**: Zero-shot desde 10-15s de audio referencia
- **Español**: Si, cross-lingual
- **VRAM**: 2-3 GB
- **Setup**: `pip install f5-tts` — el mas simple de todos
- **UI**: `f5-tts_infer-gradio` para probar rapido
- **Calidad**: Top tier en benchmarks de voice cloning open-source
- **Licencia**: CC-BY-NC-SA 4.0 (no comercial, pero para notificaciones personales no importa)
- **Veredicto**: Setup mas facil, excelente calidad, muy rapido, bajo VRAM. Para mensajes cortos de notificacion seria casi instantaneo.

### 2. Chatterbox Multilingual + Turbo (Resemble AI)
- **Repo**: https://github.com/resemble-ai/chatterbox
- **Modelo**: 350M (Turbo)
- **Velocidad**: RTF ~0.5 en RTX 4090, latencia sub-200ms
- **Voice cloning**: Zero-shot desde 5-10s de audio
- **Español**: Si, 23 idiomas
- **VRAM**: 4-8 GB
- **Setup**: `pip install chatterbox-tts`
- **Control de emocion**: Puede exagerar emociones (ideal para Homer)
- **Licencia**: Apache 2.0
- **Nota**: Variante multilingual es mas nueva, probar calidad en español antes de decidir

### 3. CosyVoice 3.0 (Alibaba)
- **Repo**: https://github.com/FunAudioLLM/CosyVoice
- **Modelo**: 500M
- **Velocidad**: ~150ms latencia con bi-streaming
- **Voice cloning**: Zero-shot cross-lingual (captura voz de audio español, genera español)
- **Español**: Si, 9 idiomas, diseñado especificamente para cross-lingual
- **VRAM**: 4-8 GB
- **Setup**: conda + pip (mas complejo que los otros)
- **Licencia**: Apache 2.0
- **Nota**: Documentacion principalmente en chino. Ideal para clonacion cross-lingual.

---

## Otras Alternativas Viables

### Fish Speech / OpenAudio S1-mini
- **Repo**: https://github.com/fishaudio/fish-speech
- **Modelo**: S1-mini = 500M (S1 full = 4B)
- **Velocidad**: RTF 1:5 en RTX 4060, 1:15 en RTX 4090
- **Español**: Si, 13+ idiomas
- **VRAM**: 5-12 GB
- **Calidad**: SOTA, entrenado en 720K horas de datos
- **Licencia**: Apache 2.0

### Orpheus TTS (Canopy AI)
- **Repo**: https://github.com/canopyai/Orpheus-TTS
- **Modelo**: 3B (main), 1B, 400M, 150M variantes
- **Velocidad**: ~200ms streaming
- **Español**: Si, 7 idiomas
- **Calidad**: Excelente control emocional (risa, suspiros — perfecto para Homer)
- **VRAM**: 8-12 GB para 3B
- **Setup**: `pip install orpheus-speech`
- **Nota**: 3B es pesado, probar variante 400M o 1B

### OpenVoice V2 (MyShell / MIT)
- **Repo**: https://github.com/myshell-ai/OpenVoice
- **Velocidad**: RTF ~0.08 (12x mas rapido que real-time) — el mas rapido
- **Español**: Si, 6 idiomas
- **Limitacion**: Aplana acentos/dialecto, Homer podria perder su caracter distintivo
- **Licencia**: MIT

### XTTS-v2 (Coqui TTS)
- **Repo**: https://huggingface.co/coqui/XTTS-v2
- **Modelo**: 467M
- **Velocidad**: RTF ~0.3 (3x real-time)
- **Español**: Si, 17 idiomas
- **Setup**: `pip install TTS`
- **Nota**: Coqui (empresa) cerro, mantenido por comunidad. Solo necesita 6s de referencia.

---

## Descartadas (sin soporte español)

| Modelo | Razon |
|--------|-------|
| Spark-TTS (500M) | Solo chino + ingles |
| Dia 1.6B (Nari Labs) | Solo ingles |
| StyleTTS2 | Solo ingles |
| MaskGCT | 6 idiomas pero NO español |
| Kokoro-82M | Sin voice cloning |

---

## Tabla Comparativa

| Modelo | Tamaño | Velocidad (RTF) | Español | VRAM | Setup | Calidad | Licencia |
|--------|--------|-----------------|---------|------|-------|---------|----------|
| **F5-TTS** | 330M | 0.15 (6.7x RT) | Si | 2-3 GB | `pip install` | Excelente | CC-BY-NC-SA |
| **Chatterbox Turbo** | 350M | 0.5 (2x RT) | Si (23) | 4-8 GB | `pip install` | Excelente | Apache 2.0 |
| **CosyVoice 3** | 500M | ~150ms lat | Si (9) | 4-8 GB | conda+pip | Muy buena | Apache 2.0 |
| **Fish S1-mini** | 500M | 0.14-0.2 | Si (13+) | 5-12 GB | clone+pip | Excelente | Apache 2.0 |
| **Orpheus 1B** | 1B | ~200ms stream | Si (7) | 8-12 GB | `pip install` | Muy buena | Apache 2.0 |
| **OpenVoice V2** | Chico | 0.08 (12x RT) | Si (6) | ~8 GB | conda+pip | Moderada | MIT |
| **XTTS-v2** | 467M | 0.3 (3x RT) | Si (17) | 6-8 GB | `pip install` | Buena | CPML |
| **Qwen3-TTS (actual)** | 1.7B | ~10 (0.1x RT) | Si | 8-12 GB | Manual | Muy buena | Apache 2.0 |

*RTF = Real-Time Factor. Menor = mas rapido. 0.15 = genera 1 segundo de audio en 0.15 segundos.*

---

## Plan de Prueba Rapida

1. **F5-TTS primero** — `pip install f5-tts && f5-tts_infer-gradio` y probar con el audio de Homer en la UI Gradio
2. **Chatterbox Turbo** — `pip install chatterbox-tts` y probar voice cloning
3. **Fish Speech S1-mini** — si los anteriores no capturan bien la voz de Homer, Fish tiene mas datos de entrenamiento (720K horas)

Las 3 se pueden probar en menos de 30 minutos cada una.
