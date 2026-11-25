# Design

## Context

A [proposta](proposal.md) trata do manual distribuído pelo Studio. O bundle copia `docs/manual/index.html`, `chapters/` e `assets/`. Help e F1 abrem essa cópia no navegador padrão, inclusive com `file://`. O índice atual consulta títulos e tags por capítulo. Há também duas representações da navegação, no HTML inicial e no script compartilhado.

## Goals / Non-Goals

**Goals**

- Manter a leitura offline e os endereços de capítulos e âncoras em circulação.
- Fazer os exercícios partirem de um estado conhecido e manter a evidência visual junto da instrução.
- Usar como referência o Studio Windows x64 e o pacote Unity da distribuição verificada, com a versão do Editor registrada na verificação.

**Non-Goals**

- Servidor de documentação ou serviço remoto de pesquisa.
- Mudança de comportamento no Studio, no pacote Unity ou no motor.
- Nova biblioteca de áudio ou novo formato de projeto para os tutoriais.

## Decisions

### Passos ancorados nos controles e nos estados do produto

Cada procedimento revisado terá objetivo, pré-requisitos, passos, resultado esperado e recuperação pertinente. Os detalhes conceituais permanecem vinculados nos pontos de decisão. Nas páginas de consulta, os nomes dos controles aparecem como no aplicativo. A rodada prioriza corrigir os procedimentos existentes e reaproveitar uma cópia de `example_project`, com `coin.wav`, `theme.wav` e o evento de sustentação Async. Um exercício que precisar criar uma pista, alvo, parâmetro ou snapshot ensina essa preparação antes do disparo.

Usar o [delta de studio-shell](specs/studio-shell/spec.md) para conferir as diferenças entre promessa editorial e recurso presente. O Inspector já usa `Trigger Sheet` e intervalos. O 3D Spatializer nativo aplica parâmetros à instância de prévia, enquanto o Unity configura o cone pelo emissor. O workspace permite Logs embutidos ou independentes. Esses comportamentos fixam os passos e a legenda de cada imagem.

### Busca estática por seção

Manter o JavaScript e os arquivos locais. Evoluir o índice para entradas com título de ação, URL para âncora estável e termos de busca, preservando a normalização de acentos. Reaproveitar os destinos do índice para evitar duplicar os procedimentos na página inicial. Adicionar a ligação dos capítulos às seções novas e preservar os destinos existentes. A indexação via servidor foi considerada pelo alcance, mas a integração por arquivo local exige resultados completos sem serviço.

### Capturas reproduzíveis de estados preenchidos

Abrir uma cópia de trabalho do projeto e preparar a seleção e os valores do procedimento antes de capturar. Usar a captura nativa de componentes onde ela mostra todas as partes necessárias e a captura da janela para docking e estados que cruzam painéis. Na Unity, capturar o Inspector e o estado da cena quando eles forem a evidência do procedimento. Registrar build ou Editor, projeto e estado observado na verificação. Usar recortes legíveis e legendas com o controle relevante. Diagramas permanecem identificados como explicações conceituais.

## Risks / Trade-offs

- **Um link válido pode descrever um fluxo incorreto.** Além de verificar arquivos e âncoras, executar os percursos revisados e registrar o resultado observado.
- **Capturas dependem da seleção, do layout e da versão.** Usar projeto e preset conhecidos, conferir a imagem final com a legenda e evitar caminhos pessoais nas mensagens capturadas.
- **Busca mais granular pode gerar destinos duplicados ou órfãos.** Validar os destinos no índice e conferir pesquisas representativas diretamente no bundle offline.
- **Integrações opcionais dependem do ambiente Unity.** Registrar versões e dependências e identificar os percursos efetivamente executados. Resultados dependentes de Editor, áudio e Live-Linking requerem evidência da execução correspondente.

## Migration Plan

O bundle continua a copiar o manual revisado pelo processo de empacotamento atual. A verificação usa uma cópia portátil fora da árvore de fontes para confirmar Help, F1 e a navegação sem rede. Uma eventual reversão pode restaurar os arquivos do manual correspondentes à versão anterior do bundle.
