# Spec Delta — studio-shell

## ADDED Requirements

### Requirement: Camada visual de destaque de tutorial (`TutorialHighlightOverlay`) no `MainComponent`
O `MainComponent` SHALL hospedar o componente filho não-interativo `TutorialHighlightOverlay` cobrindo `getLocalBounds()` e mantido no topo da ordem Z mesmo após `layout_workspace_components()` e `resized()`, permanecendo oculto quando inativo e desenhando máscara de escurecimento, contorno pelos tokens de `Theme.h` e badge sem recorte quando ativo.

#### Scenario: Transparência total de entrada sobre controles subjacentes
- **WHEN** o usuário ou as ferramentas `studio_ui_mouse`, `studio_ui_set_control` ou `studio_ui_key` interagem com controles sob o destaque ativo
- **THEN** o `TutorialHighlightOverlay` não intercepta cliques nem captura foco de teclado
- **AND** todas as interações alcançam diretamente os controles reais da Timeline, da `LayerStackView`, do Inspector ou dos Decks

#### Scenario: Preservação de ordem Z e recálculo de área em redimensionamento ou troca de modo
- **WHEN** a janela do Studio é redimensionada, splitters de docking são recriados ou o palco alterna entre os modos `Timeline` e `Layers`
- **THEN** o overlay permanece no topo da ordem Z sobre toda a área de `MainComponent`, recalcula os limites do alvo ao redesenhar e se oculta com segurança se o componente alvo ficar oculto ou com área nula

#### Scenario: Ocultamento limpo no estado inativo
- **WHEN** nenhum destaque de tutorial está ativo no `MainComponent`
- **THEN** o `TutorialHighlightOverlay` permanece invisível, não altera a aparência padrão da interface e não aparece em resultados de `studio_ui_inspect` com `visible_only` verdadeiro
