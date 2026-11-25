# Spec Delta — dev-harness

## ADDED Requirements

### Requirement: Execução rápida dedicada e gate obrigatório da suíte de controles de UI do Studio
O repositório SHALL fornecer o script `scripts/test_studio_controls.ps1` para execução rápida e filtrável de `--test-ui-controls` sem rebuild por padrão, integrar a suíte ao gate `scripts/build_studio.ps1` (`-Layer studio` e `-All`) e documentar o fluxo em `docs/testing/studio-ui-controls.md` com referência no `README.md`.

#### Scenario: Reexecução rápida filtrada durante desenvolvimento
- **WHEN** um desenvolvedor executa `scripts/test_studio_controls.ps1 -Filter "inspector.*"` sobre um build existente do Studio
- **THEN** o script invoca o executável com `--test-ui-controls --filter=inspector.*` sem rodar os demais smokes nem reconstruir o bundle
- **AND** retorna código de saída `0` quando todos os contratos filtrados passam ou diferente de `0` quando há falha

#### Scenario: Bloqueio no gate do Studio diante de controle quebrado ou órfão
- **WHEN** `scripts/verify.ps1 -Layer studio` ou `scripts/verify.ps1 -All` é executado após alterações no Studio
- **THEN** `scripts/build_studio.ps1` executa a suíte `--test-ui-controls` completa após o build
- **AND** falha o gate com código diferente de `0` se qualquer controle falhar no contrato comportamental/acústico ou violar a auditoria Zero Controle Órfão
