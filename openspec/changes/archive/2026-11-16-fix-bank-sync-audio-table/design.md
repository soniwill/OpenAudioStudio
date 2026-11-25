# Design

## Context

`native/src/api/bank_api.cpp` reconstrói o `ModelSet` a partir do `BankReader` na carga síncrona, revinculando `sound_handle` de instrumentos e entradas de playlist, mas não copia a `audio_table` para o builder. O `ATBL` existe corretamente no binário; a perda é só nesse caminho. Ver bug 1 em `../studio-help-workflow-accuracy/evidence/FINDINGS-1.4.md`.

## Goals / Non-Goals

**Goals:**
- `audio_table` sobreviver à reconstrução do `ModelSet` na carga síncrona pela API C.
- Cobrir o caminho com teste que não passe pelo `BankReader` direto (lacuna do round-trip atual).

**Non-Goals:**
- Mudar formato `.bank`, ABI ou o `BankReader`.
- Carga assíncrona/streaming (sem evidência do mesmo problema; tratar se surgir).

## Decisions

1. **Copiar a tabela no ponto de reconstrução existente, junto das revinculações já feitas** — mesmo local e momento do builder onde `sound_handle` é revinculado, sem novo estágio de carga. Alternativa considerada: popular a tabela preguiçosamente no primeiro acesso do fallback do Programmer — descartada porque esconderia divergência entre vias de carga.
2. **Bancos v1 sem `ATBL` permanecem com tabela vazia** — compatibilidade retroativa é requisito do spec; ausência de seção não é erro.
3. **Teste pelo caminho da API C** — compilar banco de teste com `oae_bankc` (ou fixture existente), carregar pela API C síncrona e verificar entradas da tabela no `ModelSet` e resolução audível (estado de voz/amostra) do Programmer sem callback.

## Risks / Trade-offs

- [Outras vias de carga (assíncrona) podem ter a mesma lacuna] → Verificar por leitura se compartilham a reconstrução; se divergirem, registrar como pendência sem expandir esta change.
- [Reconstrução usada por mais chamadores] → A cópia é aditiva (campo antes vazio); nenhum chamador depende de tabela vazia.
