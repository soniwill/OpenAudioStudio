# Tasks: studio-help-foundations

## 1. Captura de Evidências Visuais via MCP

- [x] 1.1 Iniciar o Studio com `example_project/project.json` e gerar capturas das áreas de fundamentos (Mixer Deck completo com faders de Bus/VCA/Snapshots e Inspector de Eventos/Parâmetros) usando a ferramenta MCP `studio_ui_screenshot`, gravando os arquivos PNG em `docs/manual/assets/img/` e validando a existência e legibilidade das imagens.

## 2. Redação da Página Mestra: Eventos e Parâmetros

- [x] 2.1 Criar `docs/manual/chapters/fundamentos-eventos-e-parametros.html` cobrindo a teoria do áudio interativo, o problema do áudio linear, desacoplamento semântico de eventos, ciclo de vida e camadas de instâncias, políticas de polifonia e roubo de vozes, e parâmetros locais vs. globais (com Built-in 3D), incorporando diagramas ASCII e as capturas MCP correspondentes, verificando a validação HTML local.

## 3. Redação da Página Mestra: Topologia de Mixagem (Buses, VCAs e Snapshots)

- [x] 3.1 Criar `docs/manual/chapters/fundamentos-mixagem-buses-vcas-snapshots.html` cobrindo o caminho completo do sinal (Signal Flow), soma amostra por amostra em buffers de Buses, inserts de DSP e envios auxiliares, controle escalar de ganho via VCAs sem fluxo de áudio, comparação direta de casos de uso (Bus vs VCA) e estados dinâmicos de Snapshots (override vs blending e transições de fade), com diagramas ASCII e capturas do Mixer Deck, verificando a consistência técnica e layout visual.

## 4. Integração de Navegação e Atualização de Capítulos Existentes

- [x] 4.1 Atualizar o índice `docs/manual/index.html` e o componente de barra lateral (`sidebar`) para apresentar as duas novas páginas mestras no topo de Fundamentos, e incluir callouts de contextualização teórica em `00-conceitos-basicos.html`, `04-navegador-projeto.html`, `06-inspetor-e-modulacao.html` e `14-barramentos-vcas-snapshots.html`, verificando que todos os links relativos e âncoras funcionam sem erros.

## 5. Validação e Gates de Documentação

- [x] 5.1 Executar a validação estrita da camada de documentação através de `scripts/verify.ps1 -Layer docs` (cobrindo `openspec validate --all --strict`, teste de integridade de links markdown locais e conferência de limite de 100 linhas em `docs/ai/memory.md`), garantindo aprovação de todos os gates com código de saída zero.
