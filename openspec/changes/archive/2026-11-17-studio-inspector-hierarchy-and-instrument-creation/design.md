# Design

## Context

Consulte [proposal.md](proposal.md) para a motivação e escopo desta mudança.

Atualmente, `AuthoringModel` define `ItemSelection` com três níveis de granularidade:
* Evento (`track_index == -1`, `instrument_index == -1`)
* Faixa (`track_index >= 0`, `instrument_index == -1`)
* Clipe/Instrumento (`track_index >= 0`, `instrument_index >= 0`)

Contudo, `InspectorPanel::create_event_editor` impõe `active_track = 0` sempre que `track_index < 0` e empilha os cards de baixo para cima (`ClipCard` -> `TrackCard` -> `EventHeaderCard`), ocultando o card do evento no rodapé. Além disso, `TimelineTrackLane::mouseDown` ignora cliques contextuais com o botão direito (`if (e.mods.isPopupMenu()) return;`), e `AuthoringModel::add_instrument` fixa `offset_ms: 0.0`.

## Goals / Non-Goals

**Goals:**
* Ordenação limpa e orientada ao foco ativo no `InspectorPanel`:
  * Evento selecionado: exibe estritamente o `EventHeaderCard` no topo.
  * Faixa selecionada: exibe `TrackCard` no topo com `EventHeaderCard` recolhido abaixo.
  * Clipe selecionado: exibe `ClipCard` no topo com `TrackCard` e `EventHeaderCard` recolhidos abaixo.
* Respeito estrito à contenção hierárquica das ações de criação:
  * Evento contém `+ Add Track` e `+ Add Local Param`.
  * Faixa contém `+ Add Inst`.
  * Instrumento contém controles de playlist/sons.
* Seleção de faixa por clique com botão esquerdo em área vazia da `TimelineTrackLane`.
* Menu contextual com botão direito na raia da `TimelineTrackLane` para criar instrumentos no tempo exato (`t_ms`) do cursor.
* Atualização de `AuthoringModel::add_instrument` com parâmetro `offset_ms`.

**Non-Goals:**
* Seleção múltipla de faixas ou múltiplos clipes.
* Modificações no runtime nativo C++ ou formato binário de bancos (`oae_bank`).
* Alteração da ABI C pública.

## Decisions

### Decisão 1: Empilhamento Contextual Dinâmico no `InspectorPanel`
* **Escolha**: O card correspondente ao nível de seleção ativo é posicionado no **topo** da pilha e em estado expandido. Os cards ancestrais (pais diretos) são renderizados abaixo em estado recolhido (*collapsed*), permitindo consulta ou edição rápida sem poluir o espaço vertical.
* **Justificativa**: Em telas compactas, exibir simultaneamente todas as propriedades abertas do evento, da faixa e do clipe gera rolagem excessiva. Manter o alvo no topo com ancestrais recolhidos elimina o atrito de scroll e preserva o contexto hierárquico.
* **Alternativas consideradas**:
  * *Ordem Top-Down Fixa (Evento sempre em 1º, Faixa em 2º, Clipe em 3º)*: Rejeitada porque força o usuário a rolar dezenas de pixels de propriedades do evento toda vez que clica em um clipe para editar um fade ou pitch.
  * *Contexto Estrito (Apenas 1 card exibido por vez)*: Rejeitada porque inviabiliza conferir rapidamente a rota ou volume da faixa pai enquanto edita o clipe.

### Decisão 2: Captura de Coordenada Temporal e Menu na `TimelineTrackLane`
* **Escolha**: Utilizar o functor existente `px_to_time_` em `TimelineTrackLane` para mapear `e.position.x` em `t_ms = std::max(0.0, px_to_time_(e.position.x))`. No evento de clique direito em área vazia (onde não há intersecção com `ClipComponent`), apresentar um menu `juce::PopupMenu` com o submenu `Add Instrument` listando os 7 tipos disponíveis (`Single`, `Multi`, `Scatterer`, `Programmer`, `Event`, `Command`, `Silence`).
* **Justificativa**: O usuário trabalha com precisão temporal na Timeline. Criar instrumentos no ponto clicado elimina o retrabalho de mover o clipe da posição `0.0s`.
* **Alternativas consideradas**:
  * *Criação apenas na posição do Playhead*: Rejeitada porque exige mover o playhead antes de criar, quebrando o fluxo visual de trabalho com o mouse.

### Decisão 3: Extensão de `AuthoringModel::add_instrument` com `offset_ms`
* **Escolha**: Atualizar a assinatura para:
  ```cpp
  bool add_instrument(const std::string& event_path, size_t track_index, const std::string& kind = "single",
                      const std::string& target = "", double offset_ms = 0.0);
  ```
* **Justificativa**: Mantém total retrocompatibilidade com chamadas existentes no Studio e nos testes (que omitem `offset_ms`), permitindo que a Timeline passe o tempo real calculado.

## Risks / Trade-offs

* **[Risco]** Conflito entre menu contextual do clipe e da raia vazia.
  * *Mitigação*: O `ClipComponent` consome o evento de mouse e exibe seu próprio menu (`Duplicate Clip`, `Delete Clip`). A raia (`TimelineTrackLane`) só processa o menu quando o clique ocorre fora de qualquer clipe filho.
* **[Risco]** Persistência do estado expandido/recolhido dos cards ao alternar seleção.
  * *Mitigação*: Inicializar `event_card_expanded_` como `true` quando o evento é o alvo principal e `false` quando for ancestral de uma faixa ou clipe, permitindo que o usuário expanda sob demanda.
