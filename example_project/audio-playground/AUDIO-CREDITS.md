# Créditos de áudio do AudioPlayground

Os efeitos e as músicas abaixo foram processados para mono PCM de 48 kHz e 16 bits. Os recortes, fades, filtros e ajustes de ganho estão descritos por arquivo em `audio-provenance.json`.

| WAV ou grupo | Fonte | Licença declarada na fonte |
| --- | --- | --- |
| `step_wood_*.wav`, `step_concrete_*.wav`, `step_grass_*.wav` | Kenney, [Impact Sounds](https://kenney.nl/assets/impact-sounds), via [port para Godot](https://github.com/Boyquotes/kenney-impact-sounds-for-godot) | CC0 1.0 |
| `ui_click.wav` | Kenney, [UI Audio](https://kenney.nl/assets/ui-audio), via [port para Godot](https://github.com/Calinou/kenney-ui-audio) | CC0 1.0 |
| `emitter_3d_loop.wav` | [Acoustic Guitar Rpg Tune With Strings And Piccolo](https://opengameart.org/content/acoustic-guitar-rpg-tune-with-strings-and-piccolo) | CC0 1.0 |
| `music_explore.wav` | Tsorthan Grove, [Shades of a Forest](https://opengameart.org/content/shades-of-a-forest) | CC0 na página atual, com crédito preservado conforme a versão [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) indexada |
| `music_combat.wav` | [Determined Pursuit](https://opengameart.org/content/determined-pursuit-epic-orchestra-loop) | CC0 1.0 |
| `music_tension_layer.wav` | Wolfgang_, [Battle Theme](https://opengameart.org/content/battle-theme-0) | CC0 1.0 |
| `forest_bird_1.wav` | Jidanni, [Bird call with so-called separated chirps](https://commons.wikimedia.org/wiki/File:Bird_call_with_so-called_separated_chirps.ogg) | CC0 1.0 |
| `forest_bird_2.wav` | Jidanni, [Dirty bird call](https://commons.wikimedia.org/wiki/File:Dirty_bird_call.oga) | CC0 1.0 |
| `forest_wind_grain.wav` | [Howling wind](https://commons.wikimedia.org/wiki/File:Howling_wind.ogg) | CC0 1.0 |
| `forest_fire_crackle.wav` | PagDev, [Fireplace Sound loop](https://opengameart.org/content/fireplace-sound-loop) | CC0 1.0 |

`music_explore.wav` usa os primeiros 16 segundos de `shades_of_a_forest_loop.flac`, com fades, ajuste de ganho e conversão para mono.

As três locuções foram sintetizadas localmente com [Piper 1.8.0](https://github.com/OHF-Voice/piper1-gpl). Foram usadas as vozes [Faber](https://huggingface.co/rhasspy/piper-voices/blob/main/pt/pt_BR/faber/medium/MODEL_CARD) para boas-vindas e ducking e [Jeff](https://huggingface.co/rhasspy/piper-voices/blob/main/pt/pt_BR/jeff/medium/MODEL_CARD) para espacialização. O repositório dos modelos declara MIT e os cartões apontam para gravações de treinamento [CC0 da OHF-Voice](https://github.com/OHF-Voice/voice-datasets). `voice-provenance.json` registra a revisão dos modelos, as falas e os hashes. O player leva o áudio sintetizado. Piper e seus modelos não são necessários para jogar.
