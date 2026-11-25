# Design

## Context

Consulte [proposal.md](proposal.md) para motivação e escopo geral.
O motor nativo (`oae_audio`) e o compilador de bancos (`oae_bankc`) já suportam e empacotam arquivos WAV, OGG Vorbis, MP3 e FLAC usando decodificadores `miniaudio` (`dr_wav`, `dr_flac`, `dr_mp3`, `stb_vorbis`). No entanto, o `WaveformCache` e o `PlaylistDeck` no Studio utilizavam apenas `formatManager_.registerBasicFormats()`, reconhecendo apenas WAV e AIFF.
Além disso, `ProjectDocument::import_audio_file(source, sample_id)` já implementa a cópia de arquivo e o registro em `assets/`, mas não havia conexão desse fluxo com a interface visual nem manipuladores de `juce::FileDragAndDropTarget`.

## Goals / Non-Goals

**Goals:**
- Prover decodificação uniforme de WAV, OGG Vorbis, MP3 e FLAC em `WaveformCache` e `PlaylistDeck` usando `ma_decoder` para paridade total com o motor nativo.
- Habilitar `juce::FileDragAndDropTarget` em `TimelineTrackLane` para que o drop de um arquivo de áudio importe o asset para o projeto e crie instantaneamente um `Single Instrument` na posição do tempo do mouse.
- Habilitar `juce::FileDragAndDropTarget` em `AuthoringTreeBrowser` para importação em lote de múltiplos arquivos de áudio para a pasta `assets/`.
- Adicionar opção "Import Audio..." com seletor de arquivos ao menu de contexto de `Audio Assets` no `AuthoringTreeBrowser`.
- Garantir comandos de Undo/Redo atômicos para importação e inserção de instrumentos.

**Non-Goals:**
- Não inclui transcodificação/conversão offline de codecs no empacotamento de bancos (mantém a especificação existente de empacotar os bytes brutos no chunk `DATA`).
- Não inclui suporte a formatos proprietários não-livres (WMA, AAC/M4A).

## Decisions

### Decisão 1: Decodificação na UI via `ma_decoder` (`miniaudio`) em vez de formatos JUCE adicionais
- **Abordagem escolhida**: Empregar diretamente a API `ma_decoder` (já disponível no include path do Studio via `native/external/miniaudio.h`) dentro do `WaveformCache::decode_file` e na consulta de duração no `PlaylistDeck`.
- **Racional**: Garante paridade de 100% com o runtime nativo de áudio (se o motor toca, a interface decodifica e desenha idêntico). Elimina dependências complexas de codecs do sistema operacional ou flags de compilação da JUCE (ex.: dependências de Media Foundation no Windows para `juce::MP3AudioFormat`).
- **Alternativas consideradas**: Registrar `juce::OggVorbisAudioFormat` e `juce::FlacAudioFormat` no `AudioFormatManager`. Descartado pois não cobria MP3 de forma uniforme e mantinha risco de divergência com o parser do `miniaudio`.

### Decisão 2: Fluxo dual de Drag-and-Drop (Timeline vs. Assets Tree)
- **Abordagem escolhida**:
  1. **Soltar na Timeline (`TimelineTrackLane`)**: Copia o arquivo para `<project_root>/assets/` caso não esteja contido, cria o sample no `project.json`, calcula a duração via leitor de áudio e insere um `Single Instrument` com `start_time` correspondente à posição do tempo do mouse e `length_ms = duration_ms`.
  2. **Soltar na Árvore (`AuthoringTreeBrowser`)**: Copia o arquivo (ou lista de arquivos) para `<project_root>/assets/` e cadastra no `project.json` como novos samples, atualizando o nó `Audio Assets`.
- **Racional**: Replica a ergonomia padrão de mercado (FMOD Studio / Ableton Live). Reduz a fricção ao mínimo para prototipagem rápida e mantém a opção de importação prévia em lote.

### Decisão 3: Reutilização do pipeline `ProjectDocument::import_audio_file` e `AuthoringModel`
- **Abordagem escolhida**: Expor método de alto nível no `AuthoringModel` e `ProjectDocument` para validar o arquivo com `ma_decoder`, copiar de forma segura para `assets/` e registrar no modelo em um único passo transacional.
- **Racional**: Centraliza a validação de formato e o cálculo de nomes de arquivo/caminhos relativos em um único local testável, evitando lógica duplicada entre a Timeline e a árvore de Assets.

## Risks / Trade-offs

- **[Risco] Arquivo de áudio com mesmo nome já existente em `assets/`**
  - *Mitigação*: Se o arquivo de origem já for o mesmo arquivo ou tiver conteúdo equivalente, reutilizar o asset existente. Se for um arquivo diferente com mesmo nome, gerar nome com sufixo numérico (ex: `sample_1.wav`) para evitar sobrescrita destrutiva.
- **[Risco] Latência na decodificação de waveforms de trilhas longas em MP3/OGG**
  - *Mitigação*: O `WaveformCache` executa em pool de threads assíncronas (`juce::ThreadPool`). A interface desenha um placeholder suave enquanto a waveform é calculada em segundo plano, sem travar a thread de UI.
- **[Risco] Usuário arrasta arquivos corrompidos ou com extensões não suportadas**
  - *Mitigação*: `isInterestedInFileDrag` filtra antecipadamente extensões `.wav`, `.ogg`, `.mp3` e `.flac`. Durante o drop, `ma_decoder_init_file` valida o cabeçalho real do áudio; caso inválido, a operação é rejeitada exibindo mensagem informativa no status bar sem alterar o projeto.
