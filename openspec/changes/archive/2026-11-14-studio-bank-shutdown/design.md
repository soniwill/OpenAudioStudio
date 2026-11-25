# Design

## Context

`StudioEngine::shutdown` chama `oae_system_release`. Essa função libera a árvore de buses, sons, instâncias e descrições antes de destruir o `System`, enquanto `Runtime::banks` ainda pode conter objetos daquele proprietário. Na descarga da DLL, `Bank::~Bank` chama `Bank::unload`, que espera o worker assíncrono, retira o banco e o modelo do proprietário e libera os sons de amostra. A pilha simbólica em Debug confirmou a violação em `System::unregister_bank` durante esse acesso ao proprietário já destruído.

O caso mínimo pela API nativa usou `OAE_INIT_NO_DEVICE` e `example_project/build/master.bank`. Com o banco carregado, `oae_system_release` retornou `OAE_OK` e o processo terminou com `0xc0000005`. O controle com `oae_studio_unload_bank` antes da liberação terminou com código zero.

## Goals / Non-Goals

**Goals:** dar ao encerramento explícito do sistema um limite claro para a vida de seus bancos, inclusive em carga assíncrona, e assegurar que a descarga acontece com o proprietário vivo.

**Non-Goals:** revisão geral do teardown global do runtime, mudanças nas assinaturas C, na política de acesso concorrente à API ou no ciclo de vida de projetos no Studio.

## Decisions

### Liberar os bancos no limite de propriedade nativo

Depois das validações existentes de handle e reentrada em callback, a liberação deve retirar do pool apenas os bancos cujo proprietário é o sistema que está fechando. Reutilizar `Bank::unload` e a remoção do pool para esperar a carga pendente, liberar amostras e invalidar os handles. Posicionar essa etapa enquanto o `System`, os recursos usados pela descarga e o pool de sons ainda estão vivos. A sequência deve preservar a coordenação existente entre renderização, instâncias e retirada de amostras.

**Alternativa considerada:** descarregar apenas o banco de prévia no Studio. Isso cobriria o caminho da janela e deixaria o caso mínimo da API nativa e outros clientes expostos à mesma falha.

### Verificar o encerramento no processo e a propriedade no motor

Usar os testes nativos para verificar a invalidação dos bancos liberados, a preservação de outro sistema e a liberação após descarga explícita. Reaplicar o caso mínimo e conferir o código de saída real, pois `OAE_OK` sozinho também ocorre no binário com falha. No Studio, fechar a janela depois de confirmar a prévia ativa e reabrir o mesmo projeto. Selecionar o preset salvo permite verificar a persistência do workspace junto ao teste de fechamento.

## Risks / Trade-offs

- **Carga assíncrona em andamento** → exercitar a liberação com banco carregado de modo assíncrono e aguardar a conclusão do worker antes da destruição do proprietário.
- **Referências entre bancos, amostras e instâncias** → manter os recursos usados pela descarga até essa etapa e validar bancos de dois proprietários e o caminho de descarga explícita.

## Migration Plan

A correção usa o mesmo formato de banco, projeto e preset e a mesma assinatura da API. A atualização consiste em distribuir o motor recompilado junto ao Studio.
