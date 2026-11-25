# Tasks

## 1. Modelo de Autoria: Suporte a Posição Temporal em `add_instrument`

- [x] 1.1 Atualizar a assinatura de `AuthoringModel::add_instrument` em `studio/Source/Model/AuthoringModel.h` e a implementação em `AuthoringModel.cpp` adicionando o parâmetro opcional `double offset_ms = 0.0`, aplicando-o ao campo `offset_ms` do novo instrumento em vez do valor fixo `0.0`, e verificar compilação limpa.
- [x] 1.2 Validar que chamadas legadas de `add_instrument` continuam funcionando com valor padrão `0.0` e que novas chamadas com `offset_ms` persistam o valor correto no JSON do projeto.

## 2. InspectorPanel: Ordenação Hierárquica por Nível de Seleção e Contenção

- [x] 2.1 Refatorar `InspectorPanel::create_event_editor` em `studio/Source/UI/InspectorPanel.cpp` para remover o fallback forçado de `active_track = 0` quando `selection.track_index == -1`, mantendo `active_track = -1` quando nenhuma faixa foi explicitamente selecionada.
- [x] 2.2 Reestruturar a ordem de montagem dos cards no `InspectorPanel`: quando um Evento é selecionado (`track_index == -1`), adicionar apenas o `EventHeaderCard` no topo; quando uma Faixa é selecionada (`track_index >= 0`, `instrument_index == -1`), adicionar `TrackCard` no topo e `EventHeaderCard` recolhido abaixo; quando um Clipe é selecionado, adicionar `ClipCard` no topo com `TrackCard` e `EventHeaderCard` recolhidos abaixo.
- [x] 2.3 Assegurar a contenção hierárquica das ações de criação no Inspector: verificar que `EventHeaderCard` fornece `+ Add Track` e `+ Add Local Param`, e que `+ Add Inst` reside exclusivamente no `TrackCard`.

## 3. TimelineTrackLane: Seleção de Faixa e Menu Contextual com Posição Temporal

- [x] 3.1 Implementar tratamento de clique com botão esquerdo em área vazia de `TimelineTrackLane::mouseDown` para selecionar a faixa no `AuthoringModel` (`ItemSelection{ ItemType::Event, event_path_, static_cast<int>(track_index_), -1 }`), sincronizando o foco da interface com o Inspector.
- [x] 3.2 Implementar menu contextual de clique direito em área vazia da `TimelineTrackLane` (fora de `ClipComponent`), calculando `t_ms = std::max(0.0, px_to_time_(e.position.x))` e exibindo o submenu `Add Instrument` (`Single`, `Multi`, `Scatterer`, `Programmer`, `Event`, `Command`, `Silence`), invocando `AuthoringModel::add_instrument` com a posição temporal precisa.

## 4. Documentação, Verificação Integrada e Gates

- [x] 4.1 Atualizar o manual offline do Studio em `docs/manual/` (Capítulo 5 - Timeline e Capítulo 6 - Inspector) documentando a ordenação contextual do Inspector, o princípio do pai imediato para botões de criação e o atalho de clique direito na raia da Timeline.
- [x] 4.2 Executar os gates de compilação e smokes (`scripts/verify.ps1 -Layer studio` em Release), validação estrita do OpenSpec (`openspec validate --all --strict`) e coletar evidências de interface via script ou sonda MCP.

## Workflow follow-up

- Sincronizar os deltas com o spec canônico via `openspec-sync-specs`.
- Arquivar a change via `openspec-archive-change`.
- Atualizar a memória operacional em `docs/ai/memory.md`.
