# T01 — Plano de preparação do repositório e build

Status: T01 concluída. O resultado da validação está em `docs/ai/memory.md`.
Fonte: blueprint principal `_fix.md` na raiz, seções 7 e 8, e `docs/ai/rules.md`.

## Resultado esperado

Base nativa reproduzível em C++17/C11, com CMake, dependências vendorizadas, comando único de build e verificação Debug/Release. Nenhuma API funcional da T02 ou interface da DAW nesta etapa.

## Sequência de execução após autorização

1. Inspecionar o ambiente: Git, compilador MSVC com suporte C++17/C11, Windows SDK, CMake, gerador disponível e PowerShell. Registrar versões reais. Se faltar pré-requisito que exija escolha do usuário, parar e perguntar.
2. Definir e registrar revisões fixas, origem, licença e checksum de `miniaudio`, `stb_vorbis`, `doctest` e `nlohmann/json`. Verificar a compatibilidade entre miniaudio e stb_vorbis antes de vendorização; evitar referências flutuantes. JUCE pertence à T16a.
3. Criar estrutura mínima `native/include`, `native/src`, `native/external`, `native/tests`, `scripts` e documentação. Introduzir somente arquivos usados pela base de build; não antecipar o runtime.
4. Configurar CMake com C++17/C11 obrigatórios e extensões desabilitadas; `/W4 /permissive- /utf-8` para C++ MSVC e flags apropriadas por linguagem; `-Wall -Wextra` em GCC/Clang. Criar a unidade C11 única `miniaudio_impl.c`, evitando definições duplicadas.
5. Criar `scripts/build.ps1` com `-Config Debug|Release`, `-NoTests`, `-Clean`, `-TestFilter` e `-Full`; propagar falhas com código não zero e só emitir `RESULT: OK` quando as verificações solicitadas passarem. Não tratar build com `-NoTests` como aprovação do gate da tarefa. Limitar limpeza aos diretórios de build validados dentro do projeto.
6. Integrar doctest/CTest e uma verificação mínima significativa de compilação/link das dependências, sem exigir dispositivo de áudio. Não inventar `oae_system_init` ou outra API T02 para isso. Testes de renderização com `OAE_INIT_NO_DEVICE` começam quando a API existir.
7. Documentar pré-requisitos, comandos, opções, diretórios de saída e solução de falhas comuns. Preservar a prevenção do problema PowerShell 5.1 com `2>&1`/`ErrorRecord` e verificar códigos de saída dos processos.
8. Executar build e testes completos em Debug e Release, revisar warnings/erros, registrar evidências e atualizar memória. Avançar para T02 após aprovação do gate da T01, conforme autorização contínua registrada na memória.

## Comandos de validação

```powershell
./scripts/build.ps1 -Config Debug -Full
./scripts/build.ps1 -Config Release -Full
```

Resultado esperado após implementação: compilação e testes aprovados, zero warnings e `RESULT: OK`. A documentação final também deverá demonstrar `-Clean`, `-TestFilter` e `-NoTests`, explicando seu alcance.

## Critérios de aceitação

- C++17/C11 verificados na configuração; dependências fixadas e com avisos legais preservados.
- Debug e Release compilam sem warnings; smoke de integração/link e testes disponíveis passam sem dispositivo de áudio.
- Falha de configure/build/test retorna código não zero; filtros que não executem testes não podem aprovar o gate completo.
- Build repetido é incremental; limpeza tem escopo restrito e caminhos com espaços funcionam.
- Documentação, exemplos e memória refletem resultados observados, distinguindo checks indisponíveis de sucesso.

## Modelo, esforço e coordenação

Usar o modelo e o esforço da sessão, conforme as [regras operacionais](../ai/rules.md#modelo-e-esforço-da-sessão). Um agente implementador é suficiente para o caminho dependente da T01; revisão separada só se houver benefício concreto e suporte da ferramenta. Não gastar contexto de DSP/Unity nesta etapa.

## Pendências que não bloqueiam a T01

Contrato JSON completo e recuperação avançada da publicação (T13), indexação PARM e semântica de HOLD (T10b-b/T12b). Essas escolhas continuam abertas e serão perguntadas antes do trabalho afetado. Versões locais da toolchain e revisões vendorizadas serão verificadas durante a T01, não presumidas neste plano.
