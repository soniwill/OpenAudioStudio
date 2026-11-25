# Proposta de concorrência e recuperação da publicação

**Aprovado em 2026-11-12.** Contrato de concorrência e recuperação da publicação. Ver [decisão 001](../decisions/001-contratos-autoria-e-publicacao-v1.md).

Esta proposta complementa a seção 4.5 do blueprint principal e os contratos [T13](T13-bank-compiler-contract.md) e [T16b](T16-portable-projects-contract.md). Os nomes de arquivos auxiliares, códigos de saída e políticas abaixo são recomendações novas, não garantias do código atual.

## 1. Problema e limites atuais

O protocolo vigente publica o catálogo primeiro e o banco depois, com troca atômica individual e rollback do catálogo se a segunda troca falhar. A correção desta revisão remove a exclusão destrutiva do destino na CLI e as cópias posteriores feitas pelo Studio. Isso não resolve as políticas que o blueprint deixou abertas:

- O staging `.bankc-stage` ainda é compartilhado. Compiladores podem sobrescrever ou remover os arquivos temporários uns dos outros.
- O backup do catálogo não possui diário de recuperação. A limpeza atual pode eliminar esse backup mesmo se o rollback falhar.
- O Studio ainda usa um único `.snapshot_staging.json` por projeto. Duas instâncias podem interferir no arquivo de entrada, mesmo publicando destinos diferentes.
- A interrupção entre as trocas deixa temporariamente catálogo novo e banco antigo; a atomicidade individual não impede esse estado.

Escopo recomendado: exclusão mútua de publicadores cooperantes, interrupção abrupta de processo, recuperação no próximo comando e diagnóstico de rollback malsucedido. Não prometer durabilidade contra queda de energia, falha física de disco, corrupção arbitrária, alteração externa durante a transação ou comportamento de sistemas de arquivos remotos. Essas garantias exigiriam contratos e validação adicionais. O formato público `.bank` v1 e o catálogo atual permanecem inalterados.

## 2. Política recomendada para concorrência

1. Resolver o diretório pai real do destino e normalizar o nome conforme as regras do sistema de arquivos. Essa identidade define uma chave de destino. Dois caminhos equivalentes devem disputar a mesma trava; aliases que não possam ser resolvidos com segurança devem ser rejeitados antes da publicação.
2. Adquirir uma trava exclusiva mantida pelo sistema operacional, por destino, antes de examinar recuperação pendente ou criar a transação. Manter a trava até finalizar publicação, rollback ou diagnóstico de recuperação necessária.
3. Não usar apenas a existência de um arquivo ou PID como prova de trava ativa. A posse termina quando o handle/processo termina; um arquivo de trava remanescente não deve bloquear sozinho a próxima execução. O arquivo usado para travar não deve ser removido enquanto processos puderem referenciá-lo.
4. Se a trava estiver ocupada, falhar imediatamente com diagnóstico `DESTINATION_BUSY`. Não esperar indefinidamente nem executar publicação concorrente. O Studio mostra o conflito e permite nova tentativa explícita.
5. Usar `.bankc-stage/<chave-destino>/<id-transacao>/`, no mesmo sistema de arquivos do destino. O identificador deve ser único e o diretório criado exclusivamente. Nunca remover recursivamente a raiz compartilhada `.bankc-stage`.
6. Destinos distintos podem compilar simultaneamente. Para o mesmo destino, a primeira execução que adquirir a trava conclui ou deixa um estado recuperável antes de outra avançar. A trava permanece durante geração e validação, simplificando o contrato inicial.

Exemplo: duas execuções para `build/master.bank` resultam em uma transação e um erro de destino ocupado. Execuções para `build/music.bank` e `build/sfx.bank` possuem staging e travas independentes. A exclusão mútua depende de todos os publicadores adotarem este protocolo; ferramentas externas que escrevam os mesmos arquivos não participam da garantia.

## 3. Diário e preparação

Cada transação mantém um diário auxiliar versionado, sem alterar a ABI ou o formato público do banco. Conteúdo mínimo recomendado:

- Identidade do destino, identificador da transação e versão do diário.
- Existência inicial de banco e catálogo e fingerprints dos arquivos anteriores.
- Tamanho, CRC-32 e SHA-256 do banco novo; SHA-256 do catálogo novo e anterior. CRC continua sendo o contrato público; SHA-256 serve à identificação interna dos artefatos durante recuperação.
- Nomes relativos e fixos dos arquivos de staging/backup; nunca executar caminhos arbitrários vindos de um diário sem validá-los dentro da transação e do destino.
- Estado observado: `PREPARED`, `CATALOG_PUBLISHED`, `COMMITTED` ou `RECOVERY_REQUIRED`. O estado orienta o diagnóstico, mas os arquivos reais decidem a recuperação.

Antes de qualquer troca pública:

1. Recuperar uma transação anterior válida, se existir, sob a mesma trava. Se a recuperação falhar, bloquear nova publicação.
2. Exigir par anterior válido por tamanho/CRC ou ausência de ambos. Um par inconsistente sem diário conhecido requer intervenção; não sobrescrevê-lo silenciosamente. Diretórios nos destinos são erros de entrada preservados.
3. Gerar e validar o banco e o catálogo novos. Preservar cópia fechada do catálogo anterior, quando existir, e cópia fechada do catálogo novo que não seja consumida pela troca pública.
4. Gravar e fechar todos os arquivos, verificando erros. Gravar o diário por arquivo temporário e troca atômica individual. Só iniciar as trocas públicas após `PREPARED` estar completo e legível.

O arquivo que será movido para o catálogo público é uma cópia de publicação; o catálogo novo de recuperação permanece no staging. Não é necessário guardar o banco antigo: enquanto a troca do banco não aconteceu, ele permanece no destino; após a troca, a recuperação completa o par novo. Essa escolha limita o espaço adicional sem depender de uma operação de desfazer a troca do banco.

## 4. Publicação e rollback normal

Preservar a ordem da seção 4.5:

1. Substituir atomicamente o catálogo; registrar `CATALOG_PUBLISHED`.
2. Substituir atomicamente o banco; conferir o par publicado e registrar `COMMITTED`.
3. Reportar sucesso somente com os dois arquivos novos presentes e compatíveis.

Se a troca do banco retornar erro, restaurar atomicamente o catálogo anterior; se não havia catálogo, remover apenas o catálogo novo identificado como pertencente à transação. Retornar erro de publicação mesmo quando o rollback funcionar. Nunca apagar o banco anterior como fallback.

Se o rollback falhar, preservar diário, catálogo de backup, catálogo novo e quaisquer arquivos de staging restantes. Registrar `RECOVERY_REQUIRED` quando possível; emitir a falha original e a falha de rollback separadamente, com caminho da transação. Não limpar a evidência, não reportar sucesso e não continuar uma segunda publicação no mesmo destino. Mesmo que a atualização do diário falhe, o diário `PREPARED` e os fingerprints permitem nova inspeção.

Falha de limpeza após um commit verificado não invalida o par público: reportar sucesso com aviso de staging remanescente. A próxima execução reconhece os fingerprints novos e conclui a limpeza. A limpeza nunca deve incluir diretórios desconhecidos ou transações de outros destinos.

## 5. Recuperação determinística após interrupção

Recomendação: recuperação automática antes de nova publicação e comando explícito proposto `oae_bankc --recover <out.bank>`. Esse comando ainda não existe. Ambos adquirem a mesma trava e usam o mesmo algoritmo. Identificações abaixo significam igualdade de tamanho e fingerprint, não apenas existência ou timestamp.

| Arquivos reais após interrupção | Ação recomendada |
| --- | --- |
| Banco novo e catálogo novo | Confirmar commit e limpar somente a transação reconhecida. |
| Banco novo e catálogo anterior ou ausente | Repor atomicamente o catálogo novo preservado, validar o par e confirmar commit. |
| Banco anterior e catálogo novo | Restaurar catálogo anterior; encerrar transação como revertida. |
| Banco anterior e catálogo anterior | Confirmar rollback; limpar a transação. |
| Primeira publicação: banco ausente e catálogo novo | Remover o catálogo novo identificado; voltar à ausência de ambos. |
| Primeira publicação: ambos ausentes | Descartar a transação preparada, sem alterações públicas. |
| Banco ou catálogo diferente das versões registradas, diário inválido, backup necessário ausente | Preservar tudo, retornar `RECOVERY_REQUIRED` e bloquear publicação. |

Quando banco anterior e novo tiverem exatamente os mesmos bytes, preferir completar o catálogo novo, desde que o catálogo real seja uma das versões conhecidas ou esteja ausente. Isso produz um par novo válido sem depender de inferir se o rename do banco aconteceu.

Não confiar no último estado do diário isoladamente: o processo pode morrer depois de um rename e antes da atualização do estado. Não sobrescrever arquivos desconhecidos para "consertar" o projeto. Se houver múltiplos diários preparados para o mesmo destino, tratar como conflito e exigir intervenção, pois isso viola o protocolo cooperante recomendado.

Exemplo: o processo morre após publicar catálogo B e antes de trocar banco A. A próxima execução identifica banco A e catálogo B, restaura o backup do catálogo A e só então inicia nova transação. Se o processo morreu após trocar o banco B, a recuperação confirma ou recompõe o catálogo B e mantém a publicação nova.

## 6. Consumidores e snapshots do Studio

Consumidores devem ler o catálogo e os bytes do banco que efetivamente usarão, conferir `bank_size`/`bank_crc32` e só aceitar a combinação válida. Não validar um arquivo e reabri-lo depois como se fosse necessariamente o mesmo conteúdo. Em divergência transitória, manter a última versão válida já carregada ou informar indisponibilidade e permitir nova tentativa; não apresentar o catálogo incompatível como eventos válidos. Não adicionar espera bloqueante ou acesso a disco à thread de áudio.

A trava de publicação não é exigida dos consumidores: eles continuam funcionando durante a janela entre as trocas mediante verificação de integridade. CRC é detector de inconsistência acidental, não autenticidade criptográfica. A API nativa que carrega apenas `.bank` continua sujeita à validação interna do banco; a verificação do par cabe aos componentes que usam catálogo.

O Studio deve escrever cada snapshot em arquivo exclusivo `.snapshot-<id>.json` na raiz do projeto, preservando a resolução atual dos caminhos relativos. O conteúdo JSON deve ser a cópia imutável já capturada na message thread. Fechar e verificar o arquivo antes de executar bankc; remover somente o snapshot pertencente à operação após o término do subprocesso. Não remover snapshots de outra instância por idade ou PID sem prova de posse/ausência de uso. Snapshots órfãos podem permanecer após crash; coleta segura é tarefa separada, sem risco de apagar uma compilação ativa.

Essa imutabilidade cobre o documento JSON. Ela não torna arquivos de áudio externos imutáveis durante compilação. Congelar assets ou definir consistência entre alterações concorrentes de samples deve ser uma decisão separada, se necessária; não ampliar silenciosamente a promessa de snapshot atual.

## 7. Diagnósticos e validação propostos

Recomendação de códigos novos, preservando a convenção existente de sucesso zero e falha não zero:

| Código | Resultado |
| --- | --- |
| `0` | Publicação ou recuperação concluída com estado público coerente. |
| `1` | Entrada inválida, geração falhou ou publicação falhou com estado anterior preservado/restaurado. |
| `2` | Destino ocupado por outro publicador. |
| `3` | Recuperação necessária ou impossível automaticamente; evidências preservadas. |

Os testes de aceitação devem incluir dois processos para o mesmo destino, destinos diferentes no mesmo diretório, aliases equivalentes, falha de staging, falha em cada troca e falha do próprio rollback. Injetar término de processo antes/depois de cada rename e atualização de diário, reiniciar e verificar a tabela de recuperação byte a byte. Incluir primeira publicação, republicação com banco idêntico, catálogo bloqueado, backup ausente, alteração externa desconhecida e limpeza interrompida. Validar consumidores recebendo par incompatível e dois snapshots Studio simultâneos sem interferência. Os testes não podem alegar resistência a queda de energia com base apenas em término de processo.

## 8. Decisões submetidas à aprovação

Aprovar ou ajustar: trava por destino com falha imediata; staging e snapshot exclusivos; diário com fingerprints e cópias de catálogo; recuperação que reverte quando o banco antigo permanece e conclui quando o novo está presente; preservação de evidências e bloqueio após falha de rollback; rejeição de par previamente inconsistente sem diário; códigos de saída propostos; garantia restrita a interrupção de processo em sistema de arquivos local suportado.

Nenhuma dessas políticas foi implementada por este documento. Após aprovação, atualizar o contrato canônico T13, implementar CLI/Studio/consumidores afetados e executar os gates e testes de recuperação antes de declarar a pendência encerrada.
