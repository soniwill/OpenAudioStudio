# Proposal

## Why

Ao fechar o Studio com o projeto de exemplo aberto, o processo termina com `0xc0000005`. A investigação reproduziu a falha pela API nativa e confirmou em Debug que um banco ainda registrado acessa seu sistema proprietário após a destruição dele.

## What Changes

- Corrigir a liberação do sistema nativo para descarregar seus bancos e invalidar seus handles enquanto o proprietário e os recursos usados na descarga ainda estão vivos.
- Preservar as regras existentes de liberação explícita e o isolamento entre sistemas. Confirmar que o Studio fecha normalmente com o projeto de exemplo e o banco de prévia carregados.
- Cobrir a regressão na API nativa e validar o fechamento e a reabertura do Studio com o mesmo projeto e um preset de workspace salvo.

## Capabilities

### New Capabilities

### Modified Capabilities

- `studio-shell` — encerramento normal do Studio com um projeto aberto e o banco de prévia carregado.

## Impact

- **Motor nativo** — ciclo de vida em `oae_system_release`, descarga de bancos por proprietário e testes nativos. A assinatura e os tipos públicos da ABI são preservados.
- **Studio** — validação do caminho de fechamento real da janela, com projeto de exemplo e persistência de workspace.
- **Exclusões** — formatos de banco e projeto, comportamento de docking, reabertura automática de projetos e reformulação geral do gerenciamento de handles.
