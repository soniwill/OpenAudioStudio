# Design: studio-help-bottom-deck

## Context

O Bottom Deck do OpenAudio Studio é o painel inferior encaixável que reúne seis modos de operação essenciais: Mixer, Effects, Playlist, Logic, Spatializer e Diagnostics. Atualmente, o arquivo `docs/manual/chapters/07-deck-inferior-mixer-efeitos.html` apresenta uma visão rápida dessas ferramentas, mas carece da profundidade teórica necessária para o sound designer tomar decisões embasadas em acústica, física e engenharia de software.

## Goals / Non-Goals

**Goals:**
- Reescrever e expandir profundamente `docs/manual/chapters/07-deck-inferior-mixer-efeitos.html`, estruturando cada uma das seis abas com:
  1. **Propósito e Motivação:** Qual dor de game design ou acústica a ferramenta resolve.
  2. **Teoria Acústica / Computacional:** Como funciona o algoritmo por baixo dos panos (balística de medição, equações de DSP, estocástica, física de ondas).
  3. **Inventário de Controles e Parâmetros:** Nomes, unidades (dB, ms, Hz, ratio), faixas típicas e comportamento.
  4. **Exemplos Reais de Produção:** Casos práticos em jogos de tiro, RPG, corrida e aventura.
  5. **Erros e Armadilhas Comuns:** Evitar clipping, problemas de fase, quantização incorreta e loops infinitos.
- Gerar seis capturas de tela nativas e fiéis ao tema do Studio usando as ferramentas MCP `studio_trigger_control` e `studio_ui_screenshot`, uma para cada aba ativa.
- Adicionar diagramas ASCII conceituais para cadeias de efeitos, transições temporais de lógica e envelopes psicoacústicos 3D.

**Non-Goals:**
- Não alterar a implementação C++ dos DSPs ou dos componentes JUCE do deck.
- Não dividir o Bottom Deck em 6 arquivos HTML diferentes; a coesão de manter as seis abas no mesmo capítulo favorece a compreensão integrada de que o deck é uma unidade contextual intercambiável.

## Decisions

### 1. Manter as 6 abas no Capítulo 7 com índice interno rico (ToC)
- **Decisão:** Consolidar o aprofundamento das seis abas no `07-deck-inferior-mixer-efeitos.html`, com um menu interno no topo (`<nav aria-label="Nesta página">`) permitindo saltar diretamente para qualquer aba.
- **Racional:** O Bottom Deck é uma entidade visual única que alterna abas no mesmo espaço da tela. O usuário frequentemente transita entre a Timeline e o Deck selecionando itens. Ter todas as abas no mesmo capítulo facilita a consulta e a busca rápida de termos via `manual.js`.
- **Alternativas consideradas:**
  - *Criar 6 páginas HTML separadas:* Rejeitado por fragmentar excessivamente a navegação e dificultar a visão de conjunto do deck contextual.

### 2. Comutação de abas e capturas via MCP Server
- **Decisão:** Disparar o clique nos botões das abas (`btn_tab_mixer`, `btn_tab_effects`, `btn_tab_playlist`, `btn_tab_logic`, `btn_tab_spatializer`, `btn_tab_diagnostics`) via `studio_trigger_control` e capturar a área do componente `bottom_deck` via `studio_ui_screenshot`.
- **Racional:** Elimina prints manuais e garante que cada aba seja renderizada no estado real do aplicativo com o tema escuro oficial.

## Risks / Trade-offs

- **[Risco]** Volume de texto do Capítulo 7 se tornar extenso.
  - *Mitigação:* Uso extensivo de caixas semânticas (`theory-box`, `example-box`, `warning-box`, `purpose-box`), tabelas de parâmetros e seções colapsáveis ou claramente demarcadas com âncoras.
