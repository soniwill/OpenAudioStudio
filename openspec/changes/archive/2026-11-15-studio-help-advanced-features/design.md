# Design: studio-help-advanced-features

## Context

O manual offline do OpenAudio Studio em `docs/manual/` foi recentemente reestruturado com páginas conceituais de fundamentos e detalhamento do Bottom Deck e da Workspace Bar. No entanto, funcionalidades maduras do motor e da interface — especificamente Command Instruments, Scatterer Instruments, Async Instruments, Parameter Sheets (`PSHT`), Modificadores (`PMOD`) e os componentes Unity `StudioEventEmitter`/`StudioListener` com seus 5 Built-in Parameters — ainda não estão cobertos nos capítulos correspondentes. Além disso, três capítulos contêm links para capturas de tela legadas criadas em 12/11 (`studio-deck-diagnostics.png`, `studio-deck-mixer.png` e `studio-mixer-controles.png`).

## Goals / Non-Goals

**Goals:**
- Substituir todas as referências a imagens legadas em `10-unity-api-csharp-e-live.html`, `13-unity-timeline-e-bancos.html` e `14-barramentos-vcas-snapshots.html` pelas capturas modernas já existentes em `docs/manual/assets/img/`.
- Integrar a documentação de **Command Instruments**, **Scatterer Instruments**, **Async Instruments**, **Parameter Sheets (`PSHT`)** e **Modificadores (`PMOD`)** aos capítulos 5 (Timeline) e 6 (Inspetor).
- Atualizar os capítulos 8 e 9 para cobrir `StudioEventEmitter`, `StudioListener`, cones de dispersão, atenuação física e os 5 Built-in Parameters automáticos (`Distance`, `Direction`, `Elevation`, `EventConeAngle`, `Speed`).
- Enriquecer os capítulos 11 (Tutoriais) e 12 (Glossário e Atalhos) com cenários práticos e terminologia moderna.
- Documentar as capacidades de automação externa via servidor MCP no manual.
- Validar a integridade estática e a ausência de links quebrados em todos os 18 arquivos HTML.

**Non-Goals:**
- Alterações em código C++, scripts C# da Unity ou binários executáveis.
- Criação de novos servidores externos ou dependências de rede (o manual deve continuar 100% offline).

## Decisions

### 1. Reutilização de Capturas Existentes vs Novas Capturas
- **Decisão**: Utilizar `deck-tab6-diagnostics.png` no Cap. 10 e `deck-tab1-mixer.png` no Cap. 13 e Cap. 14, aproveitando os PNGs gerados via MCP durante as fases 2 e 3.
- **Alternativa Rejeitada**: Gerar novos crops pontuais em SVG/PNG, o que aumentaria a fragmentação e complexidade de manutenção.

### 2. Estrutura Modular nos Capítulos 5 e 6
- **Decisão**: Adicionar seções dedicadas com tabelas de referência e callouts conceituais no Cap. 5 e Cap. 6, alinhadas ao design system de `manual.css`.
- **Alternativa Rejeitada**: Criar mais uma página dedicada de fundamentos; capítulos 5 e 6 são os locais naturais de autoria onde o usuário procura esses instrumentos.

### 3. Profundidade Técnica nos Capítulos Unity (8 e 9)
- **Decisão**: Explicar a matemática dos cones e a atualização em tempo real dos Built-in Parameters nos componentes Unity, conectando a autoria no Studio à execução na engine.
- **Alternativa Rejeitada**: Documentar apenas a lista de campos do Inspector sem explicar o impacto sonoro e como o runtime calcula os parâmetros relativos.

## Risks / Trade-offs

- **[Risco]** Tamanho excessivo de páginas HTML causando lentidão no carregamento local.
  - *Mitigação*: Páginas puramente HTML/CSS sem frameworks pesados pesam menos de 50 KB e carregam instantaneamente mesmo em máquinas modestas.
- **[Risco]** Links quebrados ou âncoras desajustadas no menu de navegação.
  - *Mitigação*: Executar o script de auditoria estrita de links em todos os 18 arquivos antes de concluir.
