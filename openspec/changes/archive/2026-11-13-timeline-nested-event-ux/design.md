## Context

Atualmente `TimelineTrackLane::resized()` posiciona todos os `ClipComponent` da pista na mesma área vertical (`getLocalBounds()` menos a régua de automação quando expandida), fazendo com que clipes adicionados posteriormente cubram integralmente clipes anteriores no mesmo intervalo de tempo. Além disso, `ClipComponent::paint()` só consulta `WaveformCache` quando `sample_id_` não está vazio, deixando clipes `event` como retângulos sólidos com o título prefixado por `"EVENT ("`, que trunca em clipes curtos.

## Goals / Non-Goals

**Goals:**
- Evitar oclusão visual entre instrumentos na mesma pista usando alocação determinística de sub-lanes por sobreposição temporal.
- Exibir identificação clara (`[EVT] <nome>`) e waveform resumo do evento filho dentro de clipes `Event Instrument`.
- Permitir navegação rápida do clipe `Event Instrument` para o evento filho por duplo-clique.

**Non-Goals:**
- Expansão inline editável das pistas do evento filho dentro da timeline do pai.
- Alteração de altura dinâmica do cabeçalho de pista (`TrackHeaderPanel`) além da divisão proporcional da altura disponível da lane.
- Mudanças em `Command Instrument` (além de respeitar a mesma regra de sub-lanes caso sobreponha outro clipe).

## Decisions

1. **Alocação de sub-lanes por intervalo (greedy interval coloring):**
   - Em `TimelineTrackLane::resized()`, ordenar os índices dos clipes por `offset_ms` (desempate por `instrument_index`).
   - Para cada clipe com intervalo `[start, end)` (`end = start + max(length_ms, 50.0)` para clipes pontuais como `command`), atribuir o menor índice de sub-lane `lane` cujo último término `lane_end[lane] <= start + 1e-6`.
   - Dividir a altura útil da pista (`lane_area.getHeight()`) por `total_lanes = max(1, max_lane + 1)` e posicionar cada clipe em sua faixa vertical correspondente (com pequeno espaçamento de 1–2 px entre sub-lanes quando `total_lanes > 1`).
   - Expor helper/método testável em `TimelineTrackLane` (ou consulta de bounds por índice) para verificação direta nos smoke tests.

2. **Resolução de waveform resumo do evento filho:**
   - Em `ClipComponent` (helper local em `TimelineTrackLane.cpp`), quando `kind_ == "event"` e `!target_path_.empty()`, percorrer as pistas/instrumentos do evento alvo no `AuthoringModel` (com guarda simples contra ciclos usando `target_path_ != event_path_`) e coletar o primeiro `sample_id` de instrumento `Single` encontrado (ou até os primeiros samples representativos, usando o `WaveformCache` existente).
   - Renderizar o waveform com paleta âmbar/clara contrastante sobre `0xffd06224` dentro de `bounds.reduced(2.0f, 14.0f)`, reutilizando `WaveformEnvelope::render_clip`.

3. **Rótulo curto `[EVT] <nome>` e duplo-clique:**
   - Extrair o último segmento após `/` de `target_path_` (ex.: `event:/sfx/coin` -> `coin`) e exibir `[EVT] coin` (ou `[EVT] (No target)` se vazio).
   - Em `ClipComponent::mouseDoubleClick()`, quando `kind_ == "event"` e `!target_path_.empty()`, chamar `model_.set_selection({ ItemType::Event, target_path_, -1, -1 })` (verificando antes se o evento existe no `root`).

## Risks / Trade-offs

- **Muitos clipes simultâneos na mesma pista:** com muitas sub-lanes na mesma altura fixa de pista, cada clipe fica mais fino; manter a altura da pista intacta evita dessincronizar com `TrackHeaderPanel` nesta entrega, ficando como evolução futura se necessário.
- **Eventos filhos com múltiplas pistas:** usar o primeiro `Single Instrument` encontrado (ou sobreposição leve dos primeiros) é suficiente como resumo visual sem custo excessivo de decodificação em `paint()`.
