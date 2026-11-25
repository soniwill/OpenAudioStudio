# Proposal

## Why

O motor nativo e o formato `.bank` já executam a lógica completa de timeline (`loop_region` com `until_key_off`, `transition_marker`, `transition_region` com `crossfade_ms` e quantização `immediate`/`beat`/`bar`, `sustain_point`, `tempo` e `Parameter Conditions`), mas a UI do OpenAudio Studio (`TimelineRuler` e `LogicConditionsDeck`) ainda possui lacunas importantes de autoria e manipulação visual direta na régua. Fechar essa paridade no Studio e migrar a especificação `timeline-logic` elimina atritos de fluxo e consolida o contrato canônico no OpenSpec.

## What Changes

- **Manipulação visual direta em `TimelineRuler`:**
  - Feedback visual ao vivo durante arraste (`mouseDrag`) de marcadores e regiões lógicas.
  - Alças laterais de redimensionamento (`resize`) para `loop_region` e `transition_region` diretamente na régua.
  - Rótulos informativos nos itens lógicos (destino `-> Marker`, quantização `Beat`/`Bar`, repetições `∞` / `Nx` / `KeyOff` e indicador de condição).
  - Menu de contexto completo na régua para criar `Marker`, `Tempo Marker`, `Loop Region`, `Transition Marker`, `Transition Region` e `Sustain Point`, além de `Delete` no item clicado e remoção via tecla `Delete`.
- **Autoria completa em `LogicConditionsDeck` e `AuthoringModel`:**
  - Criação e edição de `transition_region` (com `length_ms`, `target_marker`, `quantization`, `crossfade_ms` e condições).
  - Toggle `Until Key Off` em `loop_region` (ligado ao campo `until_key_off` já suportado pelo `.bank` e runtime).
  - Controle `Crossfade (ms)` em `transition_marker` e `transition_region`.
  - Edição de nome de `Marker` (com integridade referencial em `target_marker` ou atualização consistente) e edição de `Tempo Marker` (`bpm`, `ts_upper`, `ts_lower`).
  - Agrupamento de Undo/Redo por gesto (`begin_gesture`/`end_gesture`) nos sliders do `LogicConditionsDeck`.
- **Especificação canônica e documentação:**
  - Criação da capacidade `timeline-logic` no OpenSpec (migrando o contrato `T09`) e atualização da capacidade `studio-shell`.
  - Atualização do capítulo de Timeline/Lógica no manual offline do Studio e da memória operacional.

### Exclusões
- Alterações no formato binário `.bank` ou no runtime nativo de `EventInstance` (já suportam `until_key_off`, `crossfade_ms`, `transition_region`, `tempo` e `Parameter Conditions`).
- Automação/curvas de tempo contínuo (ritardando/accelerando); apenas marcadores discretos de `tempo` (`bpm`, `ts_upper`, `ts_lower`).

## Capabilities

### New Capabilities
- `timeline-logic`: Especificação normativa de marcadores e regiões lógicas de timeline (`marker`, `tempo`, `loop_region`, `sustain_point`, `transition_marker`, `transition_region`, quantização `immediate`/`beat`/`bar` e `Parameter Conditions`) na camada de autoria e runtime.

### Modified Capabilities
- `studio-shell`: Requisitos de UI/UX para edição visual na `TimelineRuler` (arraste ao vivo, resize de regiões, menu de contexto, remoção e rótulos ricos) e controles completos no `LogicConditionsDeck` (criação de `transition_region`, `until_key_off`, `crossfade_ms`, edição de `Marker`/`Tempo` e Undo por gesto).

## Impact

- **Studio UI (`studio/Source/UI/`):** `TimelineRuler.h/.cpp`, `LogicConditionsDeck.h/.cpp` (e ajuste eventual em `TimelinePanel.cpp` para repasse de teclado/ações).
- **Studio Model (`studio/Source/Model/`):** `AuthoringModel.h/.cpp` (helpers/validação para criação/edição de `transition_region`, renomeação segura de `marker` e edição de `tempo`).
- **Verificação e Docs:** `studio/Source/Main.cpp` (smoke test de UI/autoria para lógica de timeline), `studio/Resources/manual/` (capítulo de timeline/lógica), `docs/legacy/` (apontamento para spec canônico) e `docs/ai/memory.md`.
