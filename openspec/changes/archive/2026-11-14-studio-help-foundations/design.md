# Design: studio-help-foundations

## Context

O sistema de ajuda do OpenAudio Studio é servido localmente através de arquivos HTML estáticos na pasta `docs/manual/`. O arquivo `docs/manual/index.html` atua como hub central, acompanhado por capítulos em `docs/manual/chapters/` e estilizado via `docs/manual/assets/css/manual.css`.

Atualmente, os capítulos misturam explicações rápidas com procedimentos de clique, resultando em conteúdo fragmentado. Para suprir a necessidade de compreensão teórica profunda sem sobrecarregar quem precisa apenas de uma consulta rápida na timeline ou inspetor, este design estabelece a criação de páginas mestras dedicadas de fundamentos.

## Goals / Non-Goals

**Goals:**
- Criar duas páginas mestras nobres de fundamentos conceituais em `docs/manual/chapters/`:
  - `fundamentos-eventos-e-parametros.html`
  - `fundamentos-mixagem-buses-vcas-snapshots.html`
- Padronizar uma estrutura pedagógica em 5 partes para cada conceito: (1) A Dor no Desenvolvimento, (2) O Modelo Mental / Metáfora, (3) A Arquitetura / Sob o Capô, (4) Casos de Uso Reais em Jogos e (5) Erros e Armadilhas Comuns.
- Ilustrar todos os conceitos com diagramas conceituais ASCII limpos em blocos semânticos e capturas reais renderizadas diretamente do Studio via MCP `studio_ui_screenshot`.
- Integrar a navegação no índice do manual (`index.html`) e na barra lateral comum (`sidebar`), adicionando pontes (*callouts*) nos capítulos procedurais já existentes.

**Non-Goals:**
- Não reescrever todo o manual nesta etapa (as etapas seguintes cobrirão o Bottom Deck, Timeline e Unity SDK em changes subsequentes).
- Não criar dependências de JavaScript externo, servidores web ou CDNs; a documentação continuará estritamente offline e estática.
- Não alterar a interface gráfica C++ do Studio ou a lógica do motor de áudio.

## Decisions

### 1. Páginas dedicadas no início do módulo vs. expansão dos capítulos existentes
- **Decisão:** Criar páginas dedicadas de fundamentos (`fundamentos-eventos-e-parametros.html` e `fundamentos-mixagem-buses-vcas-snapshots.html`) no topo da barra de navegação, referenciadas pelos capítulos práticos.
- **Racional:** Capítulos procedurais (como o `05-timeline-e-waveforms.html` ou `06-inspetor-e-modulacao.html`) precisam ser diretos e orientados à tarefa. Colocar 10 páginas de teoria dentro deles tornaria a navegação de referência pesada. A página dedicada dá espaço para desenvolver a teoria sem compromisso com restrição de tamanho.
- **Alternativas consideradas:**
  - *Expandir diretamente os capítulos atuais:* Rejeitado por misturar manuais de referência rápida com tratados conceituais.
  - *Criar um PDF separado:* Rejeitado porque quebra a navegação por F1 e a busca local instantânea em HTML do Studio.

### 2. Geração de capturas visuais reais via MCP Server (`studio_ui_screenshot`)
- **Decisão:** Utilizar a capacidade MCP `studio_ui_screenshot` para obter as imagens de interface diretamente do binário do Studio com o projeto de exemplo (`example_project/project.json`).
- **Racional:** Garante renderização perfeita, fidelidade de escala, fontes nítidas e alinhamento com a versão atual do tema do Studio, sem risco de prints cortados, de baixa resolução ou com marcas manuais.
- **Alternativas consideradas:**
  - *Captura manual de tela com ferramentas do sistema operacional:* Rejeitado por ser inconsistente, difícil de reproduzir e propenso a variações de resolução e DPI.

### 3. Diagramação visual híbrida (ASCII Semântico + SVG/PNG)
- **Decisão:** Incorporar diagramas de arquitetura e fluxo de sinal em ASCII limpo dentro de tags `<pre class="ascii-diagram">` estilizadas no CSS, complementadas pelas capturas PNG do Studio.
- **Racional:** Diagramas ASCII são imediatamente legíveis, compatíveis com leitores de tela, responsivos em qualquer tamanho de fonte, não dependem de renderizadores externos e mantêm o arquivo HTML totalmente autocontido.

## Risks / Trade-offs

- **[Risco]** Tamanho do arquivo HTML das páginas dedicadas aumentar o tempo de leitura do usuário.
  - *Mitigação:* Uso de índice interno de página (`<nav class="page-toc">`) no topo de cada capítulo de fundamentos, permitindo saltar diretamente para o conceito desejado (ex.: "Pular direto para VCA vs Bus").
- **[Risco]** Dessincronização entre as capturas do Studio e eventuais futuras mudanças cosméticas de UI.
  - *Mitigação:* Como as capturas são padronizadas e mapeadas para a pasta `docs/manual/assets/img/`, scripts de automação ou o servidor MCP podem regenerá-las deterministicamente a qualquer momento.
