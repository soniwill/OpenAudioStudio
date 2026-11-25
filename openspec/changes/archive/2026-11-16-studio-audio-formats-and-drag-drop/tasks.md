# Tasks

## 1. Decodificação Uniforme de Áudio no Studio (WAV, OGG, MP3, FLAC)

- [x] 1.1 [Studio] Atualizar `WaveformCache::decode_file` para utilizar `ma_decoder` (`miniaudio.h`) na leitura de frames PCM para arquivos `.wav`, `.ogg`, `.mp3` e `.flac`, gerando envelopes de pico e RMS idênticos aos do runtime nativo. Verificar via teste de decodificação headless e renderização de waveform em `build/Studio_Bundle_Release/oae_studio.exe`.
- [x] 1.2 [Studio] Atualizar `PlaylistDeck` para obter a duração exata e metadados de samples através de `ma_decoder`, eliminando o status "Unreadable" para arquivos OGG, MP3 e FLAC. Verificar via inspeção do `PlaylistDeck` com samples comprimidos.

## 2. Infraestrutura de Importação no Modelo e Documento

- [x] 2.1 [Studio] Implementar em `ProjectDocument` e `AuthoringModel` o método transacional para importação de arquivos de áudio externos, copiando-os de forma segura para a pasta `assets/` do projeto, tratando potenciais colisões de nomes e cadastrando os novos samples no `project.json` com suporte a Undo/Redo. Verificar através de teste ou execução em projeto temporário.
- [x] 2.2 [Studio] Implementar método no `AuthoringModel` para inserção atômica de `Single Instrument` em uma pista da Timeline a partir de um arquivo arrastado (cópia de asset + registro de sample + criação do clipe na posição do cursor com a duração real do áudio em um único comando de Undo). Verificar a consistência do documento `project.json`.

## 3. Drag-and-Drop e Ações na Interface

- [x] 3.1 [Studio] Implementar `juce::FileDragAndDropTarget` em `TimelineTrackLane` / `TimelinePanel`, filtrando arquivos por extensões válidas (`.wav`, `.ogg`, `.mp3`, `.flac`) em `isInterestedInFileDrag` e orquestrando a importação e criação de clipe no evento `filesDropped`. Verificar interativamente na Timeline.
- [x] 3.2 [Studio] Implementar `juce::FileDragAndDropTarget` em `AuthoringTreeBrowser` (nó de Audio Assets) para importação em lote e adicionar a ação "Import Audio..." com seletor nativo de arquivos `juce::FileChooser` no menu de contexto. Verificar via árvore de navegação de assets.

## 4. Documentação e Gates de Validação

- [x] 4.1 [Docs] Atualizar o manual offline do Studio (`docs/manual/` ou capítulos relevantes) e `UI_GUIDELINES.md` documentando o suporte nativo a formatos de áudio (.wav, .ogg, .mp3, .flac) e os atalhos de importação por drag-and-drop direto na Timeline e no navegador de Assets. Verificar links relativos.
- [x] 4.2 [Gates] Executar a suíte de verificação completa (`scripts/verify.ps1 -Layer studio`, `scripts/verify.ps1 -Layer native` e `openspec validate --change studio-audio-formats-and-drag-drop --strict`) e coletar evidências de aceite.

## Workflow follow-up

- Iniciar a implementação via `openspec-apply-change` após autorização explícita do usuário.
- Sincronizar os deltas da change com `openspec-sync-specs` e arquivar com `openspec-archive-change`.
