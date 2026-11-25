# Design

## Context

`DockingLayoutManager::compute_slot_rects` reserva topo e rodapé antes de dividir a área central em colunas. `MainComponent::SlotHeaderBar` organiza as abas dos cinco slots. `ContextualBottomDeck` registra `Diagnostics & Log` no seu `TabSlot`. A persistência em `oae_studio_layout.json` já tem leitura de v1, escrita de v2 e cobertura de round-trip no smoke visual de workspace. A [proposta](proposal.md) define a motivação.

## Goals / Non-Goals

**Goals:** representar cada arranjo e suas operações em um único estado de workspace, preservando os componentes de painel, os controles de preset e os fluxos de pop-out e retorno.

**Non-Goals:** janelas flutuantes com árvore própria de divisões, editor de layouts separado e acoplamento entre layout e dados de autoria.

## Decisions

### Representar grupos de abas como folhas de divisões aninhadas

O gerenciador mantém uma árvore de divisões binárias com eixo e proporção. Cada folha contém um grupo de IDs estáveis, sua ordem e a aba ativa. `MainComponent` projeta essa árvore em cabeçalhos, grupos e divisores JUCE. O arranjo abaixo é um exemplo montável e salvável como preset personalizado. Os três templates nativos continuam representando seus arranjos atuais.

```text
Divisao esquerda / direita
  Esquerda
    Transport
    Browser | Timeline
    Bottom Deck
    Diagnostics & Log
  Direita
    Inspector
```

A área de docking começa abaixo da `WorkspaceBar`. Os divisores aplicam os mínimos dos subgrupos e preservam a proporção solicitada quando a janela encolhe. A apresentação libera espaço de ramos ocultos ou vazios, enquanto o estado mantém âncoras suficientes para reabrir ou devolver painéis. Comandos de menu, ocultação e restauração usam as mesmas operações de layout que o arraste.

**Alternativa considerada.** Acrescentar uma faixa fixa para logs e uma opção de altura para a coluna direita resolveria o desenho inicial. O usuário ainda dependeria de opções específicas para cada combinação de divisões.

### Calcular a prévia e a soltura a partir do mesmo alvo

O título inicia o arraste após um limiar de movimento. O alvo contém a posição de inserção na barra ou a borda do grupo ou do workspace e a geometria prevista. A prévia usa esse alvo sem alterar o estado. A soltura válida aplica uma única movimentação, ajusta a seleção e libera o espaço que ficou vazio. Esc e a soltura sem alvo válido restauram o estado anterior. A geometria da prévia deve refletir as restrições de tamanho aplicadas na soltura. O comando já existente de pop-out continua disponível. [Manual de layout do Unity](https://docs.unity.com/en-us/engine/6000.0/manual/unity-editor/editor-customize/customizing-your-workspace)

**Alternativa considerada.** Mover o painel real durante o arraste daria retorno imediato, com mudanças de geometria em cada movimento e recuperação mais complexa em cancelamentos.

### Reutilizar a instância dos logs e o estilo de abas

Registrar `Diagnostics & Log` como painel do workspace e reutilizar sua única instância e o ID `deck_diagnostics`. No arranjo inicial e nos presets legados com Bottom Deck visível, os logs continuam acessíveis pelo agrupamento do deck até que o usuário os mova. Mixer, Playlist, Effects, Logic e Spatializer continuam como abas contextuais do Bottom Deck e conservam os comandos existentes. A integração dos logs com essa barra deve evitar uma aba duplicada quando os logs forem movidos para outro grupo.

Usar tokens semânticos do tema para superfícies grafite, texto de abas e estados de foco e hover. Incorporar o título de aba única à faixa e dar à aba ativa, em grupos, preenchimento e contraste de texto perceptíveis. Compartilhar o tratamento entre cabeçalhos dos docks e abas internas do Bottom Deck. Manter a interação e a identidade acessível das ações.

**Alternativas consideradas.** Uma segunda instância dos logs exigiria sincronizar conteúdo e seleção. Alterar a cor global dos botões ativos afetaria controles de autoria além das barras de docking.

### Persistir o estado da árvore e converter os slots na leitura

Versionar o JSON do workspace. Salvar árvore, proporções, ordem, aba ativa, visibilidade e âncoras de retorno conforme aplicável tanto no estado corrente quanto nos presets personalizados. Ler v1 e v2 convertendo a ordem de recorte atual em uma árvore equivalente, inclusive para presets nomeados e estados ocultos. Validar os IDs, a unicidade de painéis e as proporções antes de assumir um estado carregado. Usar o layout padrão como fallback quando o estado é inválido e preservar presets válidos. Publicar o arquivo completo por substituição segura depois de gravá-lo com sucesso.

**Alternativa considerada.** Salvar retângulos absolutos simplificaria a gravação inicial, com perda dos vínculos entre painéis ao redimensionar e aplicar o preset em outra janela.

## Risks / Trade-offs

- **Reposicionamento do componente durante callbacks.** Testar a movimentação entre grupos, cancelamento e pop-out/retorno, incluindo a preservação da seleção e uma única instância de cada painel.
- **Prévia divergente da divisão final em janelas estreitas.** Testar a geometria antes e depois da soltura e o ajuste dos divisores com tamanhos mínimos.
- **Conversão incompleta de presets legados.** Exercitar fixtures de v1 e v2, incluindo presets personalizados, tamanhos e abas selecionadas conforme o formato de origem.

## Migration Plan

Carregar os formatos v1 e v2 pelo mesmo caminho de conversão para o estado em memória. O arquivo passa à nova versão na próxima gravação bem-sucedida do workspace. Conservar uma cópia do arquivo anterior à primeira conversão persistida para retorno a um binário com o leitor antigo. A validação compara a geometria do preset salvo com a geometria restaurada em janela de mesmo tamanho e confere a posição relativa em outra dimensão de janela.
