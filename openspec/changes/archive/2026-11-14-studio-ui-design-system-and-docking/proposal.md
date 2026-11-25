# Proposal

## Why

A interface atual do OpenAudio Studio possui estilos e cores dispersos em código C++, posicionamentos rígidos por coordenadas manuais (`setBounds(x, y, w, h)`) e limitações na reorganização de painéis. Para atingir o padrão profissional de DAWs e middlewares modernos (FMOD Studio, Wwise, Ableton Live e FabFilter), é necessário estabelecer diretrizes visuais unificadas, um Design System vetorial fluido em JUCE e capacidades aprimoradas de docking e desacoplamento de painéis.

## What Changes

- **Governança de UI (`UI_GUIDELINES.md`)**: Criação de documento normativo na raiz (`docs/UI_GUIDELINES.md`) definindo tokens de cores hexadecimais, grade modular de espaçamentos em múltiplos de 4px, escala tipográfica e regras de interação de áudio.
- **Design System Centralizado (`Theme.h`)**: Introdução do cabeçalho `studio/Source/UI/Theme.h` com namespaces estruturados de cores de superfície, bordas, acentos de áudio funcionais, métricas de tipografia e espaçamentos.
- **Modernização do LookAndFeel Vetorial (`OaeDarkLookAndFeel`)**:
  - Renderização 100% vetorial de rotary sliders (knobs) com arco de valor proporcional moderno e indicador central nítido.
  - Medição de nível dBFS e picos com decaimento balístico em 30–60 FPS.
  - Suporte a ajuste fino via mouse com tecla `Shift` e retorno ao valor padrão por duplo clique em sliders/knobs.
- **Micro-Layout com FlexBox e Grid**: Eliminação de coordenadas fixas e fatiamento manual solto nos painéis internos (`TransportBar`, `InspectorPanel`, `MixerDeckPanel`, `ContextualBottomDeck`), substituindo-os por `juce::FlexBox` e `juce::Grid` nos métodos `resized()`.
- **Docking Flexível e Painéis Destacáveis (Pop-out)**:
  - Evolução do `DockingLayoutManager` para gerenciar slots reconfiguráveis com abas e visibilidade dinâmica.
  - Capacidade de destacar (*pop-out*) painéis pesados (como o Mixer e decks contextuais) em janelas flutuantes (`juce::DocumentWindow`), com retorno ao dock (*dock back*).

## Capabilities

### New Capabilities
<!-- Nenhuma nova capacidade necessária; todos os requisitos de casca e interface pertencem ao studio-shell. -->

### Modified Capabilities
- `studio-shell`: Adiciona requisitos observáveis para a governança visual do Design System, controles vetoriais com gestos de precisão (Shift-drag, duplo-clique), layout responsivo interno baseado em FlexBox/Grid e capacidades de ancoragem dinâmica com janelas flutuantes destacáveis (*pop-out*).

## Impact

- **Código Afetado**:
  - `docs/UI_GUIDELINES.md` (novo arquivo de diretrizes normativas).
  - `studio/Source/UI/Theme.h` (novo cabeçalho de design tokens).
  - `studio/Source/UI/OaeDarkLookAndFeel.h` e `.cpp` (refatoração para consumo estrito de `Theme.h` e novos desenhos vetoriais).
  - `studio/Source/UI/DockingLayoutManager.h` e `.cpp` (suporte a slots com abas e janelas destacáveis `juce::DocumentWindow`).
  - `studio/Source/UI/TransportBar.cpp`, `studio/Source/UI/InspectorPanel.cpp`, `studio/Source/UI/MixerDeckPanel.cpp`, `studio/Source/UI/ContextualBottomDeck.cpp` (adoção de `juce::FlexBox`/`juce::Grid`).
- **Dependências / Build**: Mantém JUCE 8 (`juce_gui_basics`). Sem novas dependências externas de terceiros.
- **Compatibilidade e ABI**: Não afeta o motor de áudio C++ nativo, ABI C ou compilador de bancos.
