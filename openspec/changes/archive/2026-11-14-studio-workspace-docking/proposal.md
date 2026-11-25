# Proposal

## Why

O workspace atual distribui os painéis em cinco slots predefinidos e reserva toda a largura para o Bottom Deck. Isso impede que o Inspector ocupe toda a lateral enquanto o usuário acompanha os logs e trabalha nas outras abas do deck.

## What Changes

- Permitir mover e reordenar abas pelo título. Soltar sobre uma barra de abas agrupa o painel. Soltar em uma borda divide o painel de destino ou a área de docking disponível. Uma prévia indica a ação e o espaço resultante antes da soltura.
- Permitir divisões horizontais e verticais redimensionáveis e aninhadas. Dar ao `Diagnostics & Log` identidade própria no workspace para que possa compartilhar um grupo de abas ou permanecer visível em uma divisão independente.
- Cobrir um layout de referência montável pelo usuário com Inspector à direita em toda a altura da área de docking e, à esquerda, `Diagnostics & Log` logo abaixo do Bottom Deck, com a mesma largura dele.
- Adotar cabeçalhos grafite e distinguir aba ativa, inativa e foco de teclado também por contraste e forma. Aplicar o tratamento às abas do Bottom Deck.
- Salvar e restaurar divisões, proporções, grupos de abas e visibilidade com os controles de workspace existentes. Ler os layouts já salvos e preservar os fluxos de presets, ocultação e janelas destacáveis.

## Capabilities

### New Capabilities

### Modified Capabilities

- `studio-shell` — encaixe por arraste, divisões redimensionáveis, apresentação dos cabeçalhos e restauração do workspace.

## Impact

- **Studio** — `DockingLayoutManager`, `MainComponent`, `ContextualBottomDeck`, tema, persistência de `oae_studio_layout.json` e verificações de interface.
- **Documentação** — guia de UI e manual do Studio com exemplo reproduzível de montagem, salvamento e restauração.
- **Exclusões** — motor, ABI C, dados dos projetos, bancos, integração Unity, conteúdo dos logs e protocolo MCP. As capturas de aceite usam a ferramenta MCP existente.
