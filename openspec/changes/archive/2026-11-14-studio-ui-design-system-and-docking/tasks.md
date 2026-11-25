# Tasks

## 1. Governança e Tokens de Design

- [x] 1.1 Criar o documento normativo `docs/UI_GUIDELINES.md` contendo as regras de paleta hexadecimal, escala modular de 4px, tipografia, diretrizes de desenho vetorial, exigência de FlexBox/Grid em micro-layout e convenções de UX para áudio.
- [x] 1.2 Implementar o cabeçalho `studio/Source/UI/Theme.h` definindo a estrutura `OaeTheme` em `constexpr` com tokens de cores (superfícies, acentos de áudio, bordas), métricas de fontes, espaçamentos e parâmetros balísticos.
- [x] 1.3 Incluir `Theme.h` no alvo CMake do Studio e verificar compilação limpa do cabeçalho com `cmake --build build --config Debug --target OpenAudioStudio`.

## 2. LookAndFeel Vetorial e Controles de Áudio

- [x] 2.1 Refatorar `studio/Source/UI/OaeDarkLookAndFeel.h` e `.cpp` para derivar cores e dimensões exclusivamente de `Theme.h`, eliminando valores mágicos soltos.
- [x] 2.2 Implementar renderização 100% vetorial de rotary sliders (`drawRotarySlider`) com arco de valor proporcional, trilho sutil e indicador central nítido, além de atualizar faders lineares e scrollbars.
- [x] 2.3 Implementar suporte a sensibilidade reduzida com tecla `Shift` para ajuste fino e restauração de `default_value` via duplo-clique em sliders e knobs.
- [x] 2.4 Implementar componente vetorial de medição dBFS (`OaeMeterComponent`) com atualização contínua, balística de decaimento suave e retenção de pico (*peak hold*).
- [x] 2.5 Verificar compilação e testes de fumaça de interface do Studio via `powershell -File scripts/verify.ps1 -Layer studio`.

## 3. Micro-Layout FlexBox/Grid e Docking com Pop-out

- [x] 3.1 Refatorar `TransportBar::resized()` para utilizar estritamente `juce::FlexBox`, organizando controles de transporte, relógio timecode, botões de projeto e sliders de parâmetros de forma fluida.
- [x] 3.2 Refatorar o `InspectorPanel` e seus métodos de geração de linhas para utilizar `juce::FlexBox` e `juce::Grid` nos métodos `resized()`, eliminando `setBounds` manuais com coordenadas fixas.
- [x] 3.3 Refatorar `MixerDeckPanel` e `MixerStripItem::resized()` com `juce::FlexBox` vertical para fader, meter, solo, mute e rotatório de pan.
- [x] 3.4 Evoluir `DockingLayoutManager` para gerenciar slots com abas e implementar a janela flutuante `OaeFloatingPanelWindow` (`juce::DocumentWindow`) permitindo destacar (*pop-out*) e reintegrar (*dock back*) painéis.
- [x] 3.5 Executar validação completa do gate com `powershell -File scripts/verify.ps1 -Layer studio` e confirmar integridade de todos os smokes de interface.
