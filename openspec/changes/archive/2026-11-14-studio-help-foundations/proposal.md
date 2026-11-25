# Proposal: studio-help-foundations

## Why

O manual offline do OpenAudio Studio (`docs/manual/`), acessível pelo menu Help e pela tecla F1, atualmente foca predominantemente em instruções procedurais de interface (onde clicar e quais campos preencher), apresentando lacunas conceituais e teóricas profundas sobre *por que* cada funcionalidade existe e qual problema de engenharia ou de design de som ela resolve. Usuários iniciantes e sound designers que migram de outras ferramentas necessitam de uma base ontológica sólida para compreender o modelo mental do áudio interativo — especialmente a razão de ser dos eventos (em contraste com o playback linear de `.wav`), a distinção matemática e funcional entre Buses e VCAs, a composição dinâmica de Snapshots e a escala de atuação dos Parâmetros Locais vs. Globais.

Esta mudança estabelece a primeira fase da reestruturação conceitual do manual, criando páginas dedicadas de Fundamentos Arquiteturais no início da documentação e ilustrando os conceitos com diagramas conceituais ASCII e capturas de tela reais geradas via automação MCP (`studio_ui_screenshot`).

## What Changes

- **Páginas Mestras Dedicadas de Fundamentos:**
  - Criação de `docs/manual/chapters/fundamentos-eventos-e-parametros.html`:
    - Teoria do áudio reativo/procedural vs. áudio linear.
    - Motivação dos Eventos: desacoplamento do código do jogo, camadas (layering), prevenção do efeito metralhadora, ciclo de vida de voz (Start, KeyOff, Release) e políticas de roubo de vozes (*voice stealing*).
    - Distinção ontológica entre Definição (banco/molde) e Instância (tempo de execução/mundo 3D).
    - Parâmetros: Locais (escopo de instância) vs. Globais (escopo do sistema), tipos de dados (contínuo, discreto, rotulado) e parâmetros automáticos Built-in 3D (`Distance`, `Elevation`, `Direction`, `EventConeAngle`, `Speed`).
  - Criação de `docs/manual/chapters/fundamentos-mixagem-buses-vcas-snapshots.html`:
    - Caminho completo do sinal de áudio (*Signal Flow*).
    - Buses: Soma real de amostras PCM em buffers de memória, topologia hierárquica em árvore, processamento coletivo em inserts de DSP e envio auxiliar (*Sends/Returns*).
    - VCAs: Controle remoto multiplicador de ganho sem passagem de áudio, controle transversal entre categorias heterogêneas sem quebra de topologia, zero alocação de buffer e zero custo de DSP.
    - Snapshots: Estados declarativos temporários de mixagem, tempos de transição (fade-in/fade-out), modos de resolução de conflito (*Override* por prioridade vs. *Blending* aditivo).
- **Integração Visual e Navegação:**
  - Atualização do índice principal (`docs/manual/index.html`) e da barra lateral (`sidebar`) para destacar as novas páginas mestras no topo da seção de Fundamentos.
  - Atualização das referências cruzadas nos capítulos operacionais (`00-conceitos-basicos.html`, `04-navegador-projeto.html`, `06-inspetor-e-modulacao.html` e `14-barramentos-vcas-snapshots.html`) com callouts direcionando para os fundamentos.
  - Inclusão de capturas reais do Studio geradas pela ferramenta MCP `studio_ui_screenshot` para ilustrar tiras de mixer, rotas de barramento, parâmetros e instâncias.

## Capabilities

### New Capabilities
*(Nenhuma nova capacidade de plataforma ou conceito introduzida; a documentação integra-se às capacidades existentes).*

### Modified Capabilities
- `studio-shell`: Especifica os requisitos de documentação integrada e navegação do manual offline com suporte a páginas conceituais de fundamentos, acessíveis pelo Studio e estruturadas para navegação autônoma sem rede.

## Impact

- **Documentação e Ativos:** Novos arquivos HTML e imagens capturadas em `docs/manual/`.
- **Camadas do Repositório:** Afeta exclusivamente a camada `docs` (`scripts/verify.ps1 -Layer docs`).
- **Código e Binários:** Zero impacto em C++, C# ou binários do motor/Studio/Unity; nenhuma ABI ou contrato de dados é alterado.
- **Dependências Externas:** Nenhuma. O manual permanece estritamente estático, autocontido e funcional offline.
