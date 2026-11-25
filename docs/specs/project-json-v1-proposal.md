# Proposta conjunta para project.json e bancos v1

**Aprovado em 2026-11-12.** Contrato de autoria v1. A aprovação e a implementação são registradas separadamente na [decisão 001](../decisions/001-contratos-autoria-e-publicacao-v1.md) e na memória do projeto.

O [schema estrutural](project-v1.schema.proposed.json) e o [projeto de exemplo](../examples/project-v1.proposed.json) descrevem a entrada de autoria. O [contrato binário proposto](bank-v1-clarifications-proposal.md) descreve como preservar essa entrada após salvar e carregar. A [proposta de recuperação](publication-recovery-proposal.md) trata do protocolo de publicação.

## Estrutura e compatibilidade

- Preservar as chaves de raiz da seção 4.4, incluindo `mixer.master`, e `name` usado pelo Studio. Exigir `version: 1` e tratar listas de raiz ausentes como vazias. O schema descreve os tipos e campos permitidos por variante. Defaults anotados no schema devem ser aplicados pelo compilador e pelo Studio quando omitidos.
- Preservar nomes já lidos pelo CLI quando a semântica coincide. A proposta explicita `kind` de eventos, instrumentos e parâmetros. Adotar enumerações em minúsculas com sublinhado. Modo de playlist, escopo, quantização, subtipo de comando e alvo são dados de autoria.
- Usar `param: {scope, name}` em condições e automações. O escopo é obrigatório e a resolução é exata. Propor migração do `param_name` legado para o escopo que contém o nome quando só um escopo corresponder. Se houver duas correspondências ou nenhuma, retornar diagnóstico para correção manual.
- Usar `range` simétrico em RANDOM. O exemplo histórico `min: -3, max: 3` migra para `range: 3`. Valores assimétricos exigem revisão de formato. Não reinterpretar um mínimo assimétrico ou ignorar a informação.
- O CLI futuro rejeita campos desconhecidos, variantes incompletas e tipos incorretos com JSON Pointer e código diferente de zero, antes de abrir o destino. Um migrador ou passo explícito de importação pode preencher discriminadores legados comprováveis pelo tipo do objeto e corrigir aliases inequívocos. Um projeto anteriormente aceito pode exigir ajustes, pois a versão atual do CLI permite omissões e campos ignorados.

## Validação semântica

O schema é a primeira etapa. A segunda resolve relações entre registros e verifica limites de representação e de runtime.

| Área | Regras propostas |
| --- | --- |
| Arquivos e IDs | JSON UTF-8 sem chaves duplicadas ou NUL. IDs de sample, caminhos de cada categoria, nomes de parâmetros por escopo e marcadores por evento são únicos, com comparação exata. Rejeitar segmentos vazios, `.` ou `..` nas identidades. Samples usam caminhos relativos ao manifesto e barras `/`. Resolver o caminho real e exigir arquivo suportado dentro da raiz do projeto. Verificar formato por decodificação e exigir sample rate e comprimento positivos. |
| Buses e mixer | `bus:/` é implícito. `mixer.master` configura esse bus. Rejeitar declaração duplicada em `buses` e pais inexistentes ou cíclicos. Resolver o pai por caminho e emitir ordem topológica para BUSS. O parser atual depende da ordem de entrada. Até oito efeitos por posição por bus. |
| Efeitos | `position` 0 é pós-fader e 1 pré-fader. `params` usa a ordem e os limites nativos documentados na API e no contrato T07. Sidechain apenas no compressor, com bus existente e diferente do proprietário. Referências de efeito usam a posição e o índice nessa posição, com verificação do tipo e do ID do parâmetro. |
| Parâmetros | Aplicar `min <= default_value <= max`. Discreto usa inteiros e rotulado usa índice de 0 a `labels.length - 1`. As labels são distintas. Referências locais resolvem apenas no evento proprietário. Para intensidade de snapshot, exigir parâmetro local contínuo de 0 a 100. |
| Condições e curvas | De zero a quatro termos AND e probabilidade em 0–100. Intervalos ordenados dentro do domínio referenciado. Curvas têm de 1 a 32 pontos com X estritamente crescente. Timeline usa milissegundos. Curvas por parâmetro usam o domínio do parâmetro. Validar Y nas unidades do alvo. Preservar a fronteira HOLD do snippet atual, inclusive o valor anterior no ponto interno exato. |
| Eventos e lógica | Evento e snapshot exigem prefixo correspondente ao tipo. Todo bus, sample, evento filho, alvo de comando, VCA e marcador referenciado deve existir e ser do tipo correto. Detectar ciclos em eventos aninhados, inclusive entradas de playlist. Tratar `start_event` como aresta de disparo para impedir ciclos de comandos. Para regiões, `end_ms = start_ms + length_ms` e ambos os limites devem caber no domínio da timeline. |
| Modulação | RANDOM aceita amplitudes de 0 a 80 dB, 48 semitons ou 1 de pan conforme alvo. AHDSR atua em volume, com tempos em ms e sustain em 0–1. O contrato binário detalha os proprietários suportados. |
| Identidade e representabilidade | Resolver referências por nome/caminho na autoria e por índice/GUID no banco. Gerar GUIDs únicos e determinísticos quando ausentes no modelo de autoria. Uma renomeação deve atualizar todas as referências afetadas no mesmo gesto. Qualquer campo de autoria sem representação no banco precisa de mapeamento aprovado ou erro explícito antes da publicação. |

A combinação `mixer.master.mute: true` requer mapeamento para o estado inicial do runtime. Como BUSS v1 não armazena um bit de mute, propor erro explícito enquanto não houver mapeamento que preserve mute e fader de forma independente. O default `false` continua representável. `user_props` usa os pares chave/valor de EVTS, com suporte correspondente a adicionar ao modelo. Nomes exibidos, caminhos e outras informações de autoria que não afetam reprodução podem permanecer apenas no manifesto, desde que essa classificação esteja documentada.

## Exemplo e critérios de aceitação

O exemplo contém duas faixas, playlist, parâmetro rotulado, condição, curva, efeito, comando e snapshot. Preparar `assets/tone.wav` copiando o fixture `native/tests/fixtures/tone.wav` para a raiz do projeto de exemplo. Depois da implementação, `oae_bankc project.json -o build/demo.bank` deve gerar o banco e catálogo, e o teste deve carregar e disparar `event:/demo`. O CLI atual pode aceitar um subconjunto e perder dados. O exemplo passa pelo validador de schema como documento de autoria, e a execução no engine pertence ao futuro gate de integração.

Validação executada com PowerShell `Test-Json`: o exemplo foi aceito. Uma cópia do exemplo com peso zero na primeira entrada da playlist foi rejeitada. As verificações semânticas da tabela acima pertencem ao futuro compilador.

Para aceitar a implementação, testar entrada válida por variante, rejeição de campo desconhecido e tipo inválido, diagnóstico com ponteiro para referência/ciclo, preservação do par publicado em cada falha e round-trip do exemplo com comportamento equivalente no engine. Incluir teste de migração com o exemplo histórico da T13. Para a divergência de CLI, recomenda-se preservar `-v/--version` por compatibilidade com o executável e reservar `--verbose` para saída detalhada.
