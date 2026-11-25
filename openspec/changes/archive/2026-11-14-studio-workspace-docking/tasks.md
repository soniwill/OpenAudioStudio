# Tasks

## 1. Estrutura e persistência do workspace

- [x] 1.1 **Studio.** Representar grupos de abas e divisões aninhadas no `DockingLayoutManager`, com ordem, seleção, proporções, visibilidade e âncora de retorno. Exercitar no smoke de workspace a geometria de divisões aninhadas, mínimos, liberação de espaço e ausência de IDs duplicados.
- [x] 1.2 **Studio.** Serializar o estado corrente e presets na nova versão de `oae_studio_layout.json`, converter v1/v2 na leitura e conservar a cópia de retorno. Cobrir round-trip de divisões, ordem, seleção, visibilidade e proporções, conversão de dimensões e presets v1/v2 conforme disponíveis e fallback quando o estado ativo estiver inválido.

## 2. Interações e layout de referência

- [x] 2.1 **Studio.** Renderizar grupos e divisores em `MainComponent` e integrar os fluxos de visibilidade, `Hide All/Restore`, presets, reset, pop-out e retorno. Exercitar no Studio a reabertura de um painel, a restauração de sua posição e a liberação de espaço quando a área fica vazia.
- [x] 2.2 **Studio e documentação.** Implementar arraste pelo título com prévia de inserção na barra e de divisão nas bordas de painel e workspace. Exercitar reordenação no grupo, transferência entre grupos, encaixes horizontal e vertical e cancelamento por Esc e soltura inválida. Conferir que a divisão final corresponde à prévia e documentar os alvos de encaixe no manual do Studio.
- [x] 2.3 **Studio e documentação.** Tornar `Diagnostics & Log` encaixável reutilizando a instância atual e preservar o acesso aos logs no arranjo inicial e em layouts legados com o deck visível. Acrescentar ao manual os passos para montar o arranjo de referência, redimensionar a faixa dos logs e salvar e restaurar o preset. Executar o exemplo e verificar que os logs seguem visíveis ao alternar entre Mixer e Effects.

## 3. Cabeçalhos dos docks

- [x] 3.1 **Studio e documentação.** Aplicar tokens grafite aos cabeçalhos dos docks e abas do Bottom Deck. Atualizar o guia de UI e conferir, com capturas via `studio_ui_screenshot`, título de aba única, contraste entre abas em escala de cinza e indicação de foco por teclado, incluindo Browser e Inspector em largura mínima.

## 4. Verificação integrada

- [x] 4.1 **Studio.** Montar o layout documentado, ajustar o divisor acima dos logs, salvar um preset, trocar de layout, reiniciar o Studio e reaplicar o preset. Usar inspeção e capturas do MCP do Studio para comparar antes e depois na mesma dimensão e conferir as posições relativas em outra dimensão. Verificar que o Inspector preenche toda a altura da área de docking e que a faixa dos logs mantém a largura do Bottom Deck e permanece visível com outra aba interna ativa.
- [x] 4.2 **Gates.** Executar `scripts/verify.ps1 -Layer studio`, `scripts/verify.ps1 -Layer docs` e `openspec validate studio-workspace-docking --strict`. Registrar comandos, resultados e caminhos das capturas na evidência de implementação.
