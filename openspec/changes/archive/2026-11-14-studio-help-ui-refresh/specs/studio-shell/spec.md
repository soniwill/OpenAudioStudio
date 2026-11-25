# Spec Delta: studio-shell

## ADDED Requirements

### Requirement: Documentação atualizada da Workspace Bar, timeline e layout do Inspector
O manual offline do OpenAudio Studio SHALL documentar a barra de workspace (com seus seletores de templates, presets salvos, redefinição e modo foco), a navegação por breadcrumbs e cores de pista na timeline, a visualização em cards do Inspector com sliders 1:1 e ajuste fino por Shift, mantendo as capturas de tela alinhadas ao tema atual.

#### Scenario: Documentação da barra de workspace e templates
- **WHEN** o usuário consulta o capítulo 1 do manual sobre visão geral da interface
- **THEN** o manual descreve as ações da barra superior de workspace (`workspace_bar`), incluindo os templates nativos `Default (Authoring)`, `Stage Focus` e `Mixing & Mastering`, o salvamento e exclusão de presets personalizados, o botão `Reset Layout` e a ação `Hide All / Restore`
- **AND** a página apresenta captura de tela atualizada da janela e da barra de workspace

#### Scenario: Documentação de navegação por breadcrumbs e cores de pista
- **WHEN** o usuário consulta o capítulo 5 sobre timeline e waveforms
- **THEN** o manual orienta o uso do botão de retorno e trilha de breadcrumbs para eventos aninhados, a atribuição de cores de pista nos swatches de cabeçalho e a leitura dos waveforms poligonais em clipes `[EVT]`

#### Scenario: Documentação dos cards do Inspector e atalhos de slider
- **WHEN** o usuário consulta o capítulo 6 sobre o painel de propriedades
- **THEN** o manual descreve a organização modular em cards visuais, o ajuste fino de 0.1 dB/st com a tecla Shift e a restauração do valor padrão via duplo-clique com suporte a Undo/Redo
