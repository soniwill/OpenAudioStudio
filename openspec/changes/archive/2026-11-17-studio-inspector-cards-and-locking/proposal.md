# Proposal: Arquitetura Modular de Cards do Inspector e Bloqueio de Seleção (Lock)

## Why

O `InspectorPanel` do Studio atualmente acumula mais de 2.300 linhas em um único arquivo C++ monolítico e imperativo. Embora apresente visualmente três cartões sanfona (`CardStackEditor`), seus editores são fortemente acoplados, difíceis de estender e sem suporte a ações modulares de cabeçalho (como menu contextual, redefinição de parâmetros ou cópia/cola de atributos). Além disso, não há como fixar a inspeção em um elemento enquanto o usuário navega por outras entidades na Timeline ou no navegador de projeto, causando frequentes interrupções de fluxo de trabalho.

Esta mudança introduz uma arquitetura modular de cartões inspirada na filosofia do Inspector da Unity (Fase 1), fatiando o monólito em componentes desacoplados (`IInspectorCard`), padronizando cabeçalhos com ações de contexto e adicionando o recurso de bloqueio de inspeção (Lock Inspector).

## What Changes

- **Bloqueio de Inspeção (Lock Inspector)**:
  - Adição de um botão de alternância com ícone de cadeado no topo do `InspectorPanel`.
  - Quando bloqueado (*locked*), o painel congela a visualização atual e ignora notificações de mudança de seleção (`on_selection_changed`), permitindo navegar e disparar eventos sem perder o foco dos parâmetros em ajuste.
  - Indicador visual claro de estado bloqueado no cabeçalho do painel.

- **Arquitetura Modular de Cartões (`IInspectorCard`)**:
  - Extração dos editores monolíticos em cartões autônomos e reutilizáveis:
    - `EventHeaderCard`: propriedades gerais do evento (modo 2D/3D, routing do barramento master, user tags).
    - `TrackCard`: propriedades da pista selecionada (nome, volume, pan, seletor de cores `TrackColorSwatchStrip`).
    - `ClipCard`: propriedades do instrumento/clip ativo (Single, Multi, Scatterer, Programmer, Command, Silence, curvas AHDSR).
    - `BusCard`: propriedades de barramentos e slots de efeito DSP.
    - `ParamCard`, `SnapshotCard`, `VcaCard`, `SampleCard`: editores especializados para os demais tipos de itens.
  - Padronização de cabeçalhos de cartão: chevron expansível/colapsável vetorial, badge de tipo semântica e menu contextual (`...`).

- **Ações de Contexto por Cartão**:
  - Menu contextual (`...`) no cabeçalho dos cartões com suporte a:
    - *Reset to Default*: restaura os valores do cartão para os padrões de fábrica com uma única transação de Undo.
    - *Copy Values / Paste Values*: permite copiar parâmetros de um cartão (ex.: configurações de AHDSR ou curva de automação) e colar em outro clip/evento compatível.

- **Preservação de Semântica e Concorrência**:
  - Preservação estrita da gestão de gestos (`edit_depth_`, `begin_gesture()` e `end_gesture()`) e da posição de scroll na viewport durante atualizações assíncronas do modelo.

## Capabilities

### Modified Capabilities
- `studio-shell`: Adiciona requisitos para bloqueio de inspeção (Lock Inspector) e padronização do comportamento e ações de contexto dos cartões modulares do Inspector.

## Impact

- **Código Afetado**:
  - `studio/Source/UI/InspectorPanel.h` e `InspectorPanel.cpp` (refatoração e redução drástica do monólito).
  - Novos arquivos em `studio/Source/UI/Inspector/`: `IInspectorCard.h`, `EventHeaderCard.h/.cpp`, `TrackCard.h/.cpp`, `ClipCard.h/.cpp`, `BusCard.h/.cpp`, etc.
  - `studio/Source/UI/VectorIcons.h` (ícone de cadeado / lock).
- **APIs e Modelos**:
  - `AuthoringModel`: nenhuma alteração no esquema JSON do projeto ou no runtime nativo.
  - Compatibilidade binária total com projetos existentes e automação MCP.
