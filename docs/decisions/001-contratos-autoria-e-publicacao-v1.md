# Decisão 001 — Contratos de autoria e publicação v1

**Aprovada pelo usuário em 2026-11-12.** A implementação e os gates têm estado separado em `docs/ai/memory.md`.

A revisão de T12 e T13 demonstrou perda de informações em bancos, entrada JSON ignorada e ausência de recuperação após interrupção. Foram avaliadas a continuidade do subconjunto atual, uma nova versão do banco e a correção preservando o layout OAEB v1.

A decisão adota a terceira opção. Os seguintes documentos definem o comportamento aprovado:

- [Contrato de autoria e schema JSON](../specs/project-json-v1-proposal.md), incluindo validação estrita, migração dos casos legados inequívocos e diagnóstico de dados sem representação.
- [Contrato binário v1](../specs/bank-v1-clarifications-proposal.md), incluindo índices de proprietário, campos ativos de referência, preservação de condições e amplitude RANDOM simétrica.
- [Contrato de publicação e recuperação](../specs/publication-recovery-proposal.md), incluindo trava por destino, diário, verificação dos arquivos reais, snapshots exclusivos e compatibilidade de catálogo por tamanho e CRC.

Bancos anteriores que perderam informação exigem recompilação a partir da fonte. A publicação mantém a ordem catálogo e banco e recupera interrupções de processo em sistema de arquivos local suportado. A semântica de HOLD permanece a do snippet canônico.
