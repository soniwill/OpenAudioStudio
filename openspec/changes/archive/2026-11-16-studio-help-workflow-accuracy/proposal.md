# Proposal

## Why

O manual offline já acompanha parte da interface atual, mas alguns exercícios direcionam o leitor a controles ausentes ou atribuem à prévia do Studio efeitos que dependem da configuração na Unity. As buscas por rótulos como `Trigger Sheet` e `Scatterer` também precisam levar o leitor à instrução correspondente.

## What Changes

- Corrigir os fluxos de Parameter Sheets, reprodução `Async`, espacialização 3D e Command Instruments. Usar assets incluídos na distribuição ou identificar explicitamente a origem e a preparação dos assets necessários.
- Explicar a recuperação e o encaixe de painéis pelo workspace, os dois estados de `Diagnostics & Log` e as mensagens disponíveis durante publicação e Live-Linking.
- Ensinar operações de autoria com exemplos verificáveis de transições, playlists, Scatterer, Programmer e parâmetros. Atualizar as capturas para mostrar os estados selecionados que cada procedimento usa.
- Dar acesso às tarefas comuns pela página inicial e pesquisar seções offline pelos rótulos exatos, equivalentes em português e termos usados nos capítulos.
- Revisar as instruções de integração Unity envolvidas, incluindo a instalação do emissor e do listener, a distinção entre tabela de áudio e callback do Programmer e as evidências dos fluxos exercitados.

## Capabilities

### New Capabilities

### Modified Capabilities

- `studio-shell` — o manual entregue com o Studio deverá descrever os controles que estão disponíveis, oferecer percursos verificáveis e localizar procedimentos específicos pela busca offline.

## Impact

- **Manual offline do Studio.** Alterações na página inicial, nos capítulos pertinentes, no índice de busca, nas capturas e no plano editorial em `docs/manual/`.
- **Unity.** Exemplos e capturas da integração que já existe, com validação no Editor para os procedimentos revisados. AssetBundle e Addressables ficam com seus pré-requisitos explícitos e fora da rodada de novas capturas.
- **Studio, motor e formato de banco.** O escopo é documental. Alterações na UI nativa, API, ABI, serialização e novas capacidades como profiler em tempo real ou mixagem para múltiplos listeners ficam fora desta change.
