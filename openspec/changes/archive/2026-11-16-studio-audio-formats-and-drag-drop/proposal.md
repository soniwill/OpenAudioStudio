# Proposal

## Why

Atualmente, o motor nativo e o compilador de bancos (`oae_bankc`) suportam nativamente arquivos WAV, OGG Vorbis, MP3 e FLAC. No entanto, a interface desktop do OpenAudio Studio apresenta gargalos críticos de usabilidade:
1. `WaveformCache` e `PlaylistDeck` registram exclusivamente formatos básicos da JUCE (`registerBasicFormats()`, cobrindo apenas WAV e AIFF). Arquivos `.ogg`, `.mp3` e `.flac` têm suas formas de onda renderizadas em branco na Timeline e aparecem com duração "Unreadable" no `PlaylistDeck`.
2. A interface do Studio não possui manipuladores de *drag-and-drop* (`FileDragAndDropTarget`) nem opções de menu acessíveis para importar áudio do sistema operacional, forçando manipulação manual do `project.json` ou testes headless.

Esta mudança elimina o atrito de entrada para sound designers, permitindo arrastar qualquer áudio suportado (.wav, .ogg, .mp3, .flac) diretamente para a Timeline ou para o navegador de Assets, com decodificação uniforme e visualização imediata da forma de onda.

## What Changes

- **Decodificação Uniforme na UI (`WaveformCache` e `PlaylistDeck`)**: Substituição da dependência estrita de `registerBasicFormats()` pelo leitor baseado em `ma_decoder` (já integrado via `miniaudio` no projeto), garantindo suporte a `.wav`, `.ogg`, `.mp3` e `.flac` com visualização de waveforms Peak/RMS e extração exata de duração.
- **Drag-and-Drop Direto na Timeline**: Implementação de `juce::FileDragAndDropTarget` em `TimelineTrackLane` / `TimelinePanel`. Ao soltar um ou mais arquivos de áudio válidos sobre uma pista, o Studio copia os arquivos para `assets/`, registra-os em `project.json` e cria imediatamente um `Single Instrument` posicionado no tempo do cursor com a duração real do áudio.
- **Drag-and-Drop no Navegador de Assets**: Implementação de `FileDragAndDropTarget` em `AuthoringTreeBrowser` (nó de Audio Assets) para permitir importação em lote para a pasta `assets/` e cadastro no catálogo do projeto sem criar clipes na Timeline.
- **Ação de Menu "Import Audio..."**: Adição de ação de importação com seletor de arquivos no menu de contexto de `Audio Assets` no `AuthoringTreeBrowser`.
- **Exclusões**: Não abrange transcodificação destrutiva de formatos no compilador `bankc` (os arquivos continuam sendo empacotados em seus formatos nativos/comprimidos no chunk DATA conforme especificação do banco v1). Não abrange formatos proprietários não livres (WMA, AAC/M4A).

## Capabilities

### New Capabilities
<!-- Nenhuma nova capacidade necessária; as alterações estendem o comportamento do Studio Shell. -->

### Modified Capabilities
- `studio-shell`: Adição de requisitos de decodificação e visualização de waveforms para formatos OGG, MP3 e FLAC, e requisitos de importação de áudio via drag-and-drop (na Timeline e na árvore de Assets) e ação explícita de menu.

## Impact

- **Código Afetado**:
  - `studio/Source/Timeline/WaveformCache.h`, `WaveformCache.cpp`: decodificação via `ma_decoder` para gerar resumos de waveform de WAV/OGG/MP3/FLAC.
  - `studio/Source/UI/PlaylistDeck.cpp`: cálculo de duração e leitura de metadados para todos os formatos suportados.
  - `studio/Source/UI/TimelineTrackLane.h`, `TimelineTrackLane.cpp`: suporte a `juce::FileDragAndDropTarget` e criação de Single Instrument no drop.
  - `studio/Source/UI/AuthoringTreeBrowser.h`, `AuthoringTreeBrowser.cpp`: suporte a `juce::FileDragAndDropTarget` para lote de assets e item de menu "Import Audio...".
  - `studio/Source/Project/ProjectDocument.h`, `ProjectDocument.cpp`: exposição e integração dos métodos de importação de assets à UI e disparo de Undo/Redo.
- **Dependências**: Nenhuma dependência externa adicional (`miniaudio.h` já está presente em `native/external/`).
- **Compatibilidade**: Mantém compatibilidade total com os schemas existentes de `project.json` e arquivos de banco `.bank`.
