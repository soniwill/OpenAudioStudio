# Design: studio-global-parameters-deck-and-contextual-strip

## Context

Atualmente, `TransportBar` renderiza tanto parâmetros locais de eventos quanto todos os parâmetros globais do projeto dentro de um `parameterViewport_` embutido de 34 px de altura (ver `proposal.md - Why`). Isso causa estrangulamento horizontal, ausência de valores visíveis (`NoTextBox`), impossibilidade de leitura de parâmetros `labeled` e mistura conceitual de escopos.

O Studio já possui infraestrutura consolidada:
- `OaeSlider`, `OaeDarkLookAndFeel` e tokens de `Theme.h`.
- `ContextualBottomDeck` com sistema de abas e dimensões generosas (~240 px de altura).
- `DockingLayoutManager` com suporte a janelas flutuantes (`DockSlotId::Floating`, `Dock Back`).
- `AuthoringModel` e `AuditionTransport` para acesso a parâmetros locais e globais e despacho de valores ao runtime.

## Goals / Non-Goals

**Goals:**
- Separar o transporte de áudio, a edição contextual do evento e a simulação de parâmetros de mundo em 3 níveis ergonômicos.
- Manter a `TransportBar` enxuta e focada (controles de reprodução, timecode e status), com atalho rápido `[Params (N)]`.
- Fornecer fita contextual retrátil no `TimelinePanel` que exibe apenas os parâmetros relevantes para o evento aberto.
- Implementar `GlobalParametersDeck` no `ContextualBottomDeck` com grid responsivo de cards (2 a 3 colunas), busca textual, filtros e controles nativos para `continuous`, `discrete` e `labeled`.
- Suportar desacoplamento em janela flutuante com retorno via `Dock Back`.

**Non-Goals:**
- Não altera o schema binário ou o compilador `oae_bankc` (as definições de parâmetros em `project.json` e bancos compilados continuam 100% compatíveis).
- Não substitui as Parameter Sheets completas de tela cheia estilo FMOD (a Timeline continua no domínio temporal `time_ms`, e a fita atua como controle de teste/modulação em tempo de edição).
- Não cria novos tipos de parâmetros além de `continuous`, `discrete` e `labeled`.

## Decisions

### Decisão 1: Purificação da TransportBar e Ação Rápida [Params]
- **Abordagem**: Remover `parameterViewport_` de `TransportBar`. Em seu lugar, adicionar `paramsButton_` com ícone vetorial/texto e contagem de parâmetros globais (`[Params (N)]`).
- **Comportamento**: Ao clicar, notifica o `MainComponent` via callback `onOpenGlobalParameters` para focar/selecionar a aba de parâmetros no Bottom Deck (ou restaurar sua janela flutuante se estiver destacada).
- **Alternativas consideradas**:
  - *Manter sliders locais na barra e só tirar os globais*: Rejeitado porque a barra continua superlotada se o evento tiver múltiplos parâmetros locais e continua sofrendo de falta de espaço para rótulos.

### Decisão 2: Fita Retrátil de Parâmetros Contextuais na Timeline
- **Abordagem**: Criar um componente interno `TimelineParameterStrip` inserido entre a toolbar superior da Timeline e a régua `TimelineRuler`.
- **Detecção de Parâmetros**:
  - Local: `ev["parameters"]`.
  - Global referenciado: qualquer parâmetro global cujo nome coincida com `inst["sheet_parameter"]`, curvas de automação no evento ou condições lógicas.
- **Geometria**:
  - Quando o evento não consome parâmetros: altura `0 px` (invisível, não consome espaço útil).
  - Quando consome parâmetros: altura compacta (~34 px) com botão `[v] Parameters (N)` para colapso opcional.
  - Sliders com caixas numéricas legíveis (`TextBoxRight`, largura 120-140 px) e labels textuais para o tipo `labeled`.
- **Alternativas consideradas**:
  - *Abas de Parameter Sheet no topo da Timeline*: Rejeitado para esta fase pois mudaria o eixo de tempo para o domínio do parâmetro em toda a tela de pistas, sendo um esforço muito maior.

### Decisão 3: Deck de Parâmetros Globais no Bottom Deck (Grid de Cards)
- **Abordagem**: Implementar `GlobalParametersDeck` como a 7ª aba do `ContextualBottomDeck`.
- **Layout**:
  - Top bar com busca `juce::TextEditor` e botões de filtro (`Todos`, `Usados no Evento`, `Modificados`).
  - Viewport vertical contendo container com layout em colunas flexíveis/grid (cards de ~340-380 px de largura).
- **Cards Especializados**:
  - `continuous`: `OaeSlider` horizontal com display de valor numérico, min/max e reset por duplo-clique.
  - `discrete`: Slider com paradas em inteiros ou botões de incremento/decremento.
  - `labeled`: Grupo de botões segmentados com os nomes reais das opções (ex: `[ Grass | Wood | Metal | Water ]`).
  - Indicadores dinâmicos de modificadores: badge com `vel: +X/s` animado quando `velocity != 0` e ícone de cadeado para `hold_during_playback`.
- **Alternativas consideradas**:
  - *Lista vertical de 1 coluna*: Rejeitado porque desperdiça a largura horizontal de 1200+ px do deck inferior e força scroll excessivo para mais de 4 parâmetros.

### Decisão 4: Desacoplamento para Janela Flutuante com Dock Back
- **Abordagem**: O cabeçalho do `GlobalParametersDeck` inclui o botão `[Pop-Out ^]`. Ao acionar, utiliza o mecanismo existente de `OaeFloatingWindow` e `DockSlotId::Floating` no `DockingLayoutManager`.
- **Reintegração**: A janela flutuante exibe o botão `[Dock Back]` no topo e não possui botão 'X' de fechamento destrutivo do OS, garantindo persistência sem perda de estado.

## Risks / Trade-offs

- **[Risco] Consumo de altura vertical na Timeline**:
  - *Mitigação*: A fita possui altura `0 px` automaticamente quando o evento não possui parâmetros e possui botão de colapso rápido com memória de estado.
- **[Risco] Atualização contínua de modificadores (Velocity / Seek Speed) em tempo real**:
  - *Mitigação*: O `GlobalParametersDeck` registra um `juce::Timer` leve (30 Hz) que apenas lê os valores atuais do `AuditionTransport` quando a aba estiver visível e houver parâmetros com modificadores ativos.
- **[Risco] Regressão nos layouts salvos em `oae_studio_layout.json`**:
  - *Mitigação*: Adicionar a nova aba com ID estável `deck_parameters` e preservar compatibilidade com arquivos de layout existentes sem quebrar presets de fábrica.
