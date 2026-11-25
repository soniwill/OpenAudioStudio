# Proposal

## Why

O runtime nativo e o `AuthoringModel` já suportam playlists mistas com `target_event` em instrumentos `Multi`/`Scatterer`, e o motor já executa `Programmer Instrument` com `programmer_key` e `AudioTableEntry`. Contudo, o OpenAudio Studio e o pipeline de projeto/banco (`project_parser` e formato `.bank`) ainda não expõem esses fluxos ponta a ponta na interface de autoria, impedindo que o usuário monte playlists aninhadas ou crie e pré-visualize `Programmer Instruments` sem edição manual de JSON ou código C++.

## What Changes

- **Playlist mista (`Multi` / `Scatterer`) no `PlaylistDeck`**: adicionar opção de inserir eventos (`target_event`) no menu de adição da playlist (filtrados via `AuthoringModel::get_instrument_targets` para evitar ciclos) e exibir identificação visual `[EVT]` para entradas de evento na lista.
- **Autoria de `Programmer Instrument` no Studio**: expor `Programmer Instrument` no menu `+ Add Instrument`, permitir editar `programmer_key` no `InspectorPanel` (com seleção rápida das chaves da `audio_table` ou texto livre), renderizar o clipe `[PRG]` na `TimelineTrackLane` e suportar edição simples da `audio_table` (`key` -> `sample_id`) no `AuthoringModel` e na UI.
- **Persistência e compilação (`bankc` e `.bank`)**: atualizar `native/tools/bankc/project_parser.h` e a serialização/carregamento de `.bank` em `native/src/bank/` para ler e gravar `kind == "programmer"`, `programmer_key` e `audio_table`, garantindo que a prévia local e a publicação em `.bank` resolvam entradas da tabela de áudio.
- **Exclusões**: não inclui edição de curvas de modulação/automação (`modulation-automation-ui`), lógica avançada de timeline (`timeline-logic-ui`), nem tabelas de localização/idiomas (locales) completas (`banks-assets-streaming-codecs`).

## Capabilities

### New Capabilities

### Modified Capabilities
- `instruments`: ampliar os requisitos de autoria e publicação de `Programmer Instrument` (resolução de `programmer_key` e `audio_table` no projeto/banco) e de entradas `target_event` em playlists de `Multi`/`Scatterer`.
- `studio-shell`: especificar a adição/visualização de `target_event` no `PlaylistDeck`, a criação/edição de `Programmer Instrument` (`+ Add Instrument`, `InspectorPanel`, `TimelineTrackLane`) e a edição de `audio_table` no Studio.

## Impact

- **Camada Studio (`studio/`)**: `AuthoringModel`, `PlaylistDeck`, `InspectorPanel`, `TimelineTrackLane`, testes/smokes em `studio/Source/Main.cpp`.
- **Camada Banco/Compilador (`native/src/bank/`, `native/tools/bankc/`)**: `project_parser.h`, leitor/escritor de `.bank` (v1 compatível) e testes em `native/tests/bank_compiler.cpp`.
- **Camada Runtime (`native/src/core/`)**: sem quebra de ABI C; `ModelSet` e `EventInstance` já possuem suporte a `InstrumentKind::Programmer` e `AudioTableEntry`.
