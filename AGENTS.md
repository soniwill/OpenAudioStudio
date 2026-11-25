# Instruções portáteis para agentes de IA

## Estado e autorização

O avanço automático pelo backlog está **suspenso** desde 2026-11-13. O usuário autoriza o trabalho caso a caso; não inicie nem continue tarefas do backlog sem autorização explícita na sessão atual. Diante de uma incerteza material, pare o trabalho afetado e pergunte.

## Fonte da verdade em transição (decisão de 2026-11-13)

- `openspec/specs/` é a fonte canônica dos requisitos de cada capacidade **já migrada**. Mudanças futuras nascem como changes em `openspec/changes/`.
- O [blueprint consolidado](docs/legacy/blueprint.md) e os [contratos legados](docs/specs/README.md) ficam em **somente leitura** até a migração de cada capacidade. Ao migrar uma capacidade, aponte no legado para o spec que o substitui.
- Capacidades organizadas de forma híbrida: specs de conceito (comportamento observável, alinhado aos conceitos do FMOD Studio) e specs de plataforma (mecanismos transversais, como ABI C e formato de banco).

## Entrada comum a qualquer ferramenta

Estas instruções são independentes de fornecedor e devem orientar Antigravity, Claude Code, Codex e outras ferramentas de IA. Não pressupor descoberta automática deste arquivo: quando necessário, o usuário deve pedir à ferramenta que leia `AGENTS.md` na raiz antes de trabalhar.

1. Leia este arquivo e `docs/ai/memory.md`.
2. Leia `docs/ai/rules.md` e somente as seções do blueprint e especificações pertinentes à tarefa.
3. Verifique o estado real dos arquivos e as evidências antes de confiar na memória.
4. Ao encerrar uma tarefa, atualize a memória com resultado, validação, pendências e próximo passo autorizado.

O blueprint consolidado preserva produto, arquitetura, contratos e backlog das capacidades ainda não migradas. A [versão original](docs/legacy/blueprint-original.md) é histórica. As [regras operacionais](docs/ai/rules.md) valem para todas as ferramentas. O [README](README.md) apresenta Engine, Studio, Unity e MCP e documenta os comandos de build e verificação.

## Navegação pelo código

- Em implementações e refatorações que cruzem módulos ou camadas, consulte o índice local do Graphify, quando disponível, antes de abrir muitos arquivos. Faça consultas curtas na raiz do repositório, como `graphify explain "native/src/core/event_instance.h::EventInstance"`. Use `caminho::símbolo` para nomes ambíguos.
- Use o resultado para selecionar arquivos e confirme as relações no código e os requisitos nas especificações canônicas. Para buscas pontuais, ou quando o índice estiver ausente, desatualizado ou inconclusivo, use `rg` e leia os arquivos pertinentes.
- Para gerar ou atualizar o índice local, execute `graphify extract . --code-only` na raiz. O arquivo `.graphifyignore` exclui a dependência `studio/juce/` do índice.

## Conduta

- Siga as instruções atuais do usuário e as restrições do ambiente. Havendo conflito entre documentos, interrompa o trabalho afetado e pergunte.
- Quando houver incerteza não resolvida pelas fontes disponíveis, pare e pergunte; não invente requisitos ou registre hipótese como decisão aprovada.
- Fora dessas situações, execute autonomamente o escopo autorizado até sua conclusão e validação.
- Preserve alterações do usuário. Não declare testes executados ou resultados sem evidência.
- Economia de tokens nunca justifica omitir requisitos, documentação, exemplos ou validação obrigatória.
