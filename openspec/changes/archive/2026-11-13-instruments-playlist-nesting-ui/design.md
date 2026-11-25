# Design

## Context

Veja `proposal.md` para motivação e escopo. No estado atual:
- O runtime (`native/src/core/studio_model.h` e `event_instance.cpp`) já implementa `InstrumentKind::Programmer`, `programmer_key` e `ModelSet::find_audio_table_entry`.
- O `AuthoringModel` já valida ciclos, renomeia e limpa `target_event` em `playlist` de `multi` e `scatterer`.
- O compilador (`native/tools/bankc/project_parser.h`) já lê `target_event` em `playlist`, mas ainda não lê `kind == "programmer"`, `programmer_key` nem `audio_table`; o formato `.bank` (`native/src/bank/`) também não serializa esses campos.

## Goals / Non-Goals

**Goals:**
- Fechar o ciclo ponta a ponta de playlist mista (`sample_id` + `target_event`) no `PlaylistDeck`.
- Fechar o ciclo ponta a ponta de `Programmer Instrument` + `audio_table` entre Studio (`AuthoringModel`, `InspectorPanel`, `TimelineTrackLane`), compilador (`project_parser.h`) e formato `.bank` (`native/src/bank/`).
- Preservar retrocompatibilidade ao carregar bancos `.bank` v1 já compilados sem `audio_table`.

**Non-Goals:**
- Não alterar a ABI C pública (`oae_studio_*` / `oae_event_callback_*`), que já cobre callbacks de programmer sound.
- Não implementar localização multi-idioma (locales) de tabelas de áudio nesta change.

## Decisions

1. **Menu unificado de adição no `PlaylistDeck` (`+` com submenus Samples / Events)**
   - Em vez de adicionar um segundo botão na barra do `PlaylistDeck`, manter o botão `+` existente abrindo um `PopupMenu` com submenus **Add Sample** e **Add Event** (ou lista direta quando houver poucos itens, mantendo IDs disjuntos no callback).
   - **Racional:** evita poluir o layout compacto do `PlaylistDeck` e reutiliza `AuthoringModel::get_instrument_targets(event_path, "event")` para excluir ciclos automaticamente.

2. **Edição de `audio_table` no `AuthoringModel` e `InspectorPanel` (contexto de projeto/programmer)**
   - Armazenar `audio_table` na raiz do JSON do projeto como array de objetos `{ "key": "...", "sample_id": "..." }`.
   - Expor métodos no `AuthoringModel` (`set_audio_table_entry`, `remove_audio_table_entry`) com Undo/Redo e limpeza automática se o `sample_id` referenciado for removido.
   - No `InspectorPanel` de um `Programmer Instrument`, oferecer edição de `programmer_key` (combo editável ou seletor de chaves da `audio_table` + campo texto) e um bloco compacto para adicionar/remover mapeamentos `key -> sample_id` na `audio_table` sem exigir um novo painel modal pesado.
   - **Alternativa considerada:** criar uma aba dedicada no `ContextualBottomDeck`; descartada por aumentar acoplamento de UI para um recurso simples de chave-valor nesta fase.

3. **Serialização em `.bank` mantendo compatibilidade v1**
   - Estender o writer/reader de `.bank` em `native/src/bank/` (e `project_parser.h`) para gravar/ler `programmer_key` nos instrumentos e a seção `audio_table` (`key` + `sample_index`).
   - Se a extensão for feita com campo/seção opcional ou bump de sub-versão retrocompatível no reader v1, bancos antigos continuam carregando normalmente. (Verificar o layout exato de `native/src/bank/` durante a implementação para manter leitura retrocompatível.)

## Risks / Trade-offs

- **[Risco] Quebra de leitura em `.bank` v1 antigos** -> **Mitigação:** manter leitura condicional ou compatível com arquivos v1 existentes e cobrir roundtrip antigo + novo em `native/tests/bank_compiler.cpp`.
- **[Risco] Renomeio de sample ou evento quebrar referências** -> **Mitigação:** propagar renomeio/exclusão de `sample_id` em `audio_table` no `AuthoringModel`, seguindo o mesmo padrão já usado para `playlist`.
