# Regras de engenharia e operação de IA

## Portabilidade e economia de tokens

- Manter instruções em Markdown versionável, com caminhos relativos e sem dependência de um provedor de IA.
- Carregar contexto progressivamente: índice, memória resumida, tarefa e apenas arquivos relevantes. Preferir buscas direcionadas a releituras integrais.
- Usar uma fonte canônica por regra/contrato; referenciar em vez de duplicar. Resumir logs, preservando comando, resultado e caminho da evidência.
- Planejar em unidades pequenas com critérios de aceitação. Não repetir investigações ou testes aprovados sem mudança, falha ou risco que justifique; os gates do blueprint continuam obrigatórios.
- Não economizar tokens à custa de correção, segurança de tempo real ou clareza. Não criar abstrações, arquivos ou agentes sem benefício concreto.

## Multiagente

- Usar múltiplos agentes quando existirem subtarefas independentes e o ganho superar custo de contexto, coordenação e integração. Tarefas simples ficam com um agente.
- Respeitar disponibilidade, autorização e restrições da ferramenta. Sem suporte a agentes, executar o mesmo fluxo sequencialmente.
- O coordenador define objetivo, entradas mínimas, arquivos sob responsabilidade, dependências, critérios de aceitação e orçamento de cada agente.
- Não permitir escrita concorrente nos mesmos arquivos. Contratos compartilhados devem ser estabilizados antes da implementação paralela; preservar a ordem e os gates do backlog.
- Cada agente retorna resumo, arquivos alterados, evidências de validação e dúvidas. O coordenador integra, revisa e valida o conjunto; relatos individuais não substituem testes de integração.
- Delegar contexto necessário, não o histórico completo por padrão. Evitar agentes duplicando a mesma investigação ou esperando dependências ainda não resolvidas.

## Memória persistente e aprendizado

- `docs/ai/memory.md` é o resumo canônico entre sessões e ferramentas: estado, autorização, decisões confirmadas, evidências, pendências e próximo passo.
- Registrar aprendizado reutilizável com contexto, causa, solução e teste/evidência; distinguir observação, hipótese e decisão aprovada.
- Atualizar ao concluir tarefa, descobrir falha relevante ou transferir trabalho. Consolidar entradas redundantes; manter o resumo curto e apontar detalhes para documentos específicos.
- Validar memórias contra código e especificações atuais. Marcar conclusões superadas; memória não tem autoridade para alterar requisitos.
- Não persistir segredos, credenciais, dados pessoais desnecessários ou transcrições extensas. Agentes auxiliares entregam notas ao coordenador, que consolida a memória.

## Modelo e esforço da sessão

- Usar o modelo e o esforço configurados na sessão aberta para o agente principal durante toda a execução.
- Por padrão, a delegação autorizada usa agentes que herdam modelo e esforço da sessão. Se a ferramenta não oferecer essa herança, executar a subtarefa na própria sessão.
- Ajustar a investigação e a validação à complexidade e ao risco da tarefa. Em caso de incerteza material, seguir a regra de esclarecimento do AGENTS.md.

## Implementação e comentários

- Aplicar responsabilidades claras, coesão, baixo acoplamento, composição e interfaces pequenas. Usar SOLID onde necessário, sem camadas artificiais ou abstrações especulativas.
- Separar DSP/runtime, API C, persistência, modelo de autoria e UI. Preservar ABI, ownership, ciclo de vida, validação de entradas e regras de tempo real do blueprint.
- Comentários no código somente para documentar classes e métodos. Não adicionar comentários narrativos, código comentado ou TODOs; registrar justificativas arquiteturais e pendências em Markdown.
- A documentação das funções livres da API C deve ficar nos documentos de API, com exemplos, respeitando a restrição de comentários. Preservar avisos legais e arquivos de terceiros sem modificá-los para adequação estilística.
- Documentar classes/métodos com propósito e, quando pertinente, parâmetros, retorno, erros, ownership e restrições de thread. Não repetir o que o nome já explica.

## Documentação, exemplos e conclusão

- Toda funcionalidade deve ter documentação e exemplo de uso correspondente, atualizados junto com a alteração: objetivo, pré-requisitos, entradas/saídas, erros, limitações e comportamento esperado.
- APIs: exemplo mínimo de ciclo de vida e tratamento de erros. CLI: comando e resultado esperado. UI: passos reproduzíveis. Integrações: configuração e fluxo completo. Recursos internos: contrato e exemplo em teste ou documento técnico.
- Indicar como executar exemplos e quais foram verificados. Não apresentar exemplo planejado como funcionalidade existente.
- Concluir apenas com critérios de aceitação atendidos, documentação/exemplos atualizados e verificações obrigatórias aprovadas. Registrar verificações indisponíveis como pendentes, nunca como sucesso.

## Especificações e regras adicionais

Propor novos requisitos e refinamentos em `openspec/changes/<change-id>/`. Incluir escopo, exclusões, entradas e saídas, invariantes, erros, critérios de aceitação, exemplos e questões abertas quando pertinentes. Consultar [o índice do legado](../specs/README.md) e [o blueprint consolidado](../legacy/blueprint.md) para capacidades ainda não migradas. Os contratos migrados são canônicos em `openspec/specs/`.

Aplicar os comandos por camada e o gate de arquivamento descritos no [README](../../README.md).

Registrar decisões arquiteturais significativas em `docs/decisions/<id>-<tema>.md`, com contexto, alternativas, decisão confirmada e consequências. Criar regras locais somente para restrições recorrentes específicas de um módulo; referenciá-las na entrada comum.

Nenhum documento novo autoriza implementação. Incertezas devem ser apresentadas ao usuário; fora delas, atuar autonomamente dentro do escopo já autorizado.
