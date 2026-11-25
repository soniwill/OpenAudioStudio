# Design

## Context

Atualmente o OpenAudio Studio possui a classe `OaeDarkLookAndFeel` que implementa um tema escuro básico, porém com cores e dimensões codificadas diretamente em vários componentes (`TransportBar`, `InspectorPanel`, `MixerDeckPanel`). Os métodos `resized()` usam cortes manuais de coordenadas (`setBounds` com valores absolutos), tornando a interface rígida em telas de diferentes resoluções ou quando os splitters são redimensionados. A pasta `docs/ui-references/` foi criada com 134 capturas de tela curadas servindo como base de referência estética. Veja `proposal.md` para a motivação completa.

## Goals / Non-Goals

**Goals:**
- Estabelecer `docs/UI_GUIDELINES.md` como guia normativo para qualquer agente e desenvolvedor que atue na interface.
- Criar `Theme.h` com constantes em tempo de compilação para cores (superfície, borda, acentos funcionais), tipografia, grade modular de 4px e tempos de balística.
- Modernizar `OaeDarkLookAndFeel` para desenhar controles vetoriais (knobs com arcos de valor limpos, sliders, faders, scrollbars e LEDs) sem depender de bitmaps.
- Implementar suporte a ajuste fino com `Shift` e reset para `default_value` por duplo clique em sliders/knobs.
- Criar medidor vetorial leve dBFS / VU com taxa de 30–60 FPS e decaimento balístico suave.
- Refatorar os métodos `resized()` dos painéis internos (`TransportBar`, `InspectorPanel`, `MixerDeckPanel`, `ContextualBottomDeck`) para utilizar estritamente `juce::FlexBox` e `juce::Grid`.
- Evoluir `DockingLayoutManager` para gerenciar slots reconfiguráveis com abas e suporte a destaque (*pop-out*) de painéis em `juce::DocumentWindow`.

**Non-Goals:**
- Não implementar uma árvore genérica de docking arbitrário por drag-and-drop livre (estilo docking avançado do Visual Studio) nesta fase: o foco é docking modular por slots de abas e janelas destacáveis (*pop-out*), que oferece 95% do benefício com alta estabilidade no JUCE.
- Não alterar nenhum aspecto do motor DSP nativo em C++, compilação de bancos ou pacote Unity.

## Decisions

### 1. `Theme.h` como estrutura de constantes em tempo de compilação
- **Decisão**: Modelar os tokens em `struct OaeTheme` com sub-namespaces (`Colors`, `Spacing`, `Typography`, `Animation`) usando `static constexpr uint32_t` e tipos JUCE.
- **Alternativa considerada**: Carregar tokens via arquivo JSON em runtime.
- **Justificativa**: `constexpr` garante segurança estrita de tipos, zero overhead de alocação de memória e validação em tempo de compilação pelo compilador C++.

### 2. Separação de Macro-Layout (Docking) e Micro-Layout (FlexBox/Grid)
- **Decisão**: A casca principal continua coordenada pelo `DockingLayoutManager` (controlando as proporções entre Left, Center, Right, Bottom e Top Bar com persistência em JSON), enquanto os filhos internos de cada painel são organizados exclusivamente via `juce::FlexBox` e `juce::Grid`.
- **Alternativa considerada**: Tentar aplicar `juce::FlexBox` na janela inteira incluindo os splitters móveis.
- **Justificativa**: `FlexBox` não foi desenhado para gerenciar divisores interativos com arraste de mouse em pixels contínuos. A separação macro/micro é a arquitetura padrão em DAWs profissionais.

### 3. Knobs e Sliders customizados via `LookAndFeel_V4`
- **Decisão**: Sobrescrever `drawRotarySlider` e `drawLinearSlider` no `OaeDarkLookAndFeel`, em vez de criar uma classe proprietária de knob derivada de `juce::Component`.
- **Alternativa considerada**: Criar um componente customizado `OaeKnobComponent`.
- **Justificativa**: Sobrescrever no LookAndFeel preserva compatibilidade imediata com todos os `juce::Slider` já existentes em toda a aplicação (Inspector, TransportBar, Mixer, Decks), atualizando o visual globalmente sem reescrever a hierarquia de componentes.

### 4. Destaque de Painéis (*Pop-out*) via `juce::DocumentWindow`
- **Decisão**: Criar uma classe leve `OaeFloatingPanelWindow` herdada de `juce::DocumentWindow` controlada pelo `DockingLayoutManager`. Ao destacar, o painel é transferido para a janela flutuante; ao fechar a janela, o painel é reintegrado ao seu slot no layout principal.
- **Alternativa considerada**: Criar janelas de sistema operacionais desacopladas sem vínculo com o `MainComponent`.
- **Justificativa**: Garante que o fechamento da aplicação principal feche todas as janelas filhas com segurança e preserve os ciclos de vida e listeners do `AuthoringModel`.

## Risks / Trade-offs

- **[Risco: Desempenho de layout com FlexBox em alta frequência de redimensionamento]**  
  *Mitigação*: Montar instâncias de `juce::FlexBox` somente no método `resized()`, nunca no ciclo de pintura `paint()`.
- **[Risco: Ciclo de vida e ponteiros em janelas destacadas (Pop-out)]**  
  *Mitigação*: O `AuthoringModel` e os nós principais mantêm a posse (*ownership*) dos painéis; as janelas flutuantes apenas abrigam o componente visualmente sem assumir a propriedade de destruição.
- **[Risco: Regressão em testes de fumaça de UI existentes]**  
  *Mitigação*: Preservar os `ComponentID` existentes (`btn_play`, `btn_new_project`, `inspector_command_type`, etc.) para manter 100% dos testes de smoke do dev-harness íntegros.
