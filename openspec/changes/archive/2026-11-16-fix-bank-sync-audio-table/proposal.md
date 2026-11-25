# Proposal

## Why

A carga síncrona de banco pela API C (`bank_api.cpp`) reconstrói o `ModelSet` sem copiar a `audio_table` do banco para o builder, deixando a tabela vazia no runtime. O fallback do Programmer em `event_instance.cpp` não encontra a chave e o instrumento silencia na prévia, violando o requisito existente "Autoria — Compilação e persistência de Programmer Instrument e tabela de áudio". O teste atual de round-trip (`bank_compiler.cpp`) não detecta o problema porque lê o banco pelo `BankReader` sem passar pela reconstrução da API C. Descoberto na verificação da tarefa 1.4 de `studio-help-workflow-accuracy` (evidências em `../studio-help-workflow-accuracy/evidence/FINDINGS-1.4.md`).

## What Changes

- Copiar a `audio_table` do banco para o builder na reconstrução síncrona de `ModelSet` em `native/src/api/bank_api.cpp`.
- Adicionar cenário ao spec canônico cobrindo explicitamente o caminho de carga síncrona pela API C (não apenas o round-trip do compilador).
- Adicionar teste nativo que carregue o banco via `bank_api.cpp` (caminho de carga síncrona) e confirme a resolução de Programmer por tabela.
- Desbloqueia a tarefa 1.4 de `studio-help-workflow-accuracy` (exercício de Programmer na prévia).

## Capabilities

### New Capabilities

(nenhuma)

### Modified Capabilities

- `instruments`: o requisito "Autoria — Compilação e persistência de Programmer Instrument e tabela de áudio" ganha cenário cobrindo a carga síncrona do banco pela API C, garantindo que a `audio_table` sobreviva à reconstrução do `ModelSet` e o Programmer reproduza o sample na prévia sem callback.

## Impact

- **Motor (C/C++)**: `native/src/api/bank_api.cpp` (cópia da `audio_table` no builder); `native/tests/` (novo teste de carga síncrona com Programmer).
- **ABI/formato**: sem mudança — o `ATBL` já existe corretamente no `.bank` (confirmado via `oae_bankc --verbose`); a correção é no caminho de carga.
- **Studio/Unity**: sem mudança de código; comportamento observável passa a conformar o spec (prévia do Programmer audível).
- **Documentação**: nenhum arquivo nesta change; a revisão do manual fica na tarefa 1.4 de `studio-help-workflow-accuracy`.
