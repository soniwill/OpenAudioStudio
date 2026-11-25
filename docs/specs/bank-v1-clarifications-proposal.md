# Proposta de esclarecimento do formato binário v1

**Aprovado em 2026-11-12.** Complementa a seção 4.3 do blueprint e preserva o layout de bytes, magic OAEB e versão. Ver [decisão 001](../decisions/001-contratos-autoria-e-publicacao-v1.md).

## Proprietários no PARM

Todos os índices começam em zero e são relativos ao evento. Emitir os registros em ordem de evento, track e instrumento, preservando a ordem dos registros dentro de cada proprietário.

| Proprietário | `owner_index` | Restauração |
| --- | --- | --- |
| EVENT | 0 | Vetor do evento |
| TRACK | Índice da track em EVTS | Vetor da track correspondente |
| INSTRUMENT | Soma das contagens de instrumentos das tracks anteriores mais índice dentro da track | Vetor do instrumento correspondente |

Exemplo: tracks com 2 e 3 instrumentos dão índices 0–1 e 2–4. O segundo instrumento da segunda track tem índice 3. O writer percorre as coleções da hierarquia e deriva os índices. O reader valida limites e repõe cada registro na coleção do proprietário. Automação de evento e track e modulador de evento e instrumento correspondem às coleções atuais. Um proprietário previsto no wire, mas sem suporte no modelo/runtime para aquele tipo, deve gerar diagnóstico até implementar a semântica correspondente.

## Referências e campos inativos

| Uso | `global` | `local_index` | `global_id` |
| --- | --- | --- | --- |
| Parâmetro local | 0 | Índice na tabela local do evento | GUID nulo |
| Parâmetro global | 1 | 0 | GUID não nulo presente na tabela global |
| Automação por timeline | 0 | 0 | GUID nulo |

O campo `source` identifica se a referência é ativa. Índice local zero é válido. O writer resolve nomes no escopo declarado e grava zeros nos campos inativos. O reader verifica o booleano, resolve o campo ativo e repõe `ParamRef.name` para consultas do runtime. A automação de efeito usa o GUID global previsto no seu próprio registro e resolve o nome pela mesma tabela. Não usar fallback entre escopos ao serializar. Uma referência não resolvida deve impedir a publicação ou retornar `OAE_ERR_FORMAT` durante a leitura.

## Condições e RANDOM

Criar uma tabela de até 64 condições por evento. `-1` indica ausência. Emitir condição de instrumento na posição correspondente do vetor achatado e condição de lógica em LOGC. Uma condição explícita de 0% continua sendo um registro real. Validar quantidade achatada igual à soma de instrumentos, índices dentro da tabela, probabilidade finita e até quatro termos com intervalos válidos. Reconstruir a condição efetiva no modelo para cada referência.

O valor `range` do registro RANDOM representa a amplitude simétrica. Para `range = 3`, recuperar mínimo −3 e máximo +3. A correção desta revisão já recupera o mínimo das factories simétricas. A política proposta para intervalos assimétricos no modelo é rejeitar a escrita com diagnóstico. Usar um novo formato caso seja necessário representar mínimo e máximo independentes. AHDSR usa os campos de envelope com `range` inativo igual a zero.

## Compatibilidade e validação

- Writers novos emitem somente a forma canônica. Recomenda-se que o reader v1 aceite e normalize campos inativos legados quando o campo ativo e o restante do registro forem válidos. Validar sempre tamanho, CRC, enumerações, índices ativos, cardinalidade, finitude e limites antes de alocar ou usar os dados.
- Bancos existentes que perderam condições, nomes ou dados de track/instrumento não contêm informação suficiente para reconstrução fiel. Recompilar a partir da origem revisada. Em referências ambíguas produzidas pelo writer antigo, o índice local zero é indistinguível de uma referência legítima ao primeiro parâmetro. A migração deve verificar o manifesto ou o modelo original.
- Testar round-trip estrutural e o comportamento após carregar. Casos mínimos incluem instrumento com condição de 0%, transição condicionada, escopos local e global com o mesmo nome, automação de track, AHDSR de instrumento, valores simétricos de RANDOM e rejeição de índices inválidos com CRC recalculado. Cobrir limites de `owner_index`, parâmetros globais inexistentes e campos inativos na leitura de legado.

As decisões submetidas são a indexação por proprietário, a escrita canônica com zeros e leitura compatível de campos inativos, a reconstrução dos nomes e condições, e a rejeição de RANDOM assimétrico. O gate T12 volta a ser integral após aplicar as decisões aprovadas e comprovar a fidelidade dos casos hoje perdidos.
