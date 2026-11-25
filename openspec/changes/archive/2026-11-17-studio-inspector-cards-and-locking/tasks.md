# Tasks

## 1. Infraestrutura Base e Cabeçalhos Ricos (Lock e IInspectorCard)

- [x] 1.1 Criar a interface abstrata `IInspectorCard` e o clipboard em memória `InspectorClipboard` em `studio/Source/UI/Inspector/` e verificar compilação limpa
- [x] 1.2 Implementar o componente `InspectorCardHeader` com chevron colapsável, badge semântica e menu contextual de 3 pontos (`...`) suportando *Reset to Default*, *Copy Values* e *Paste Values*, verificando renderização no tema dark
- [x] 1.3 Adicionar barra de ferramentas superior fixa no `InspectorPanel` com botão de Lock (ícone vetorial de cadeado) e filtro de congelamento em `on_selection_changed`, verificando que a seleção permanece fixa ao navegar na Timeline

## 2. Decomposição dos Cartões Modulares do Inspector

- [x] 2.1 Extrair `EventHeaderCard` para gerenciar modo 2D/3D, barramento master, notas e user tags em `studio/Source/UI/Inspector/EventHeaderCard.h/.cpp`, verificando mutações e Undo/Redo
- [x] 2.2 Extrair `TrackCard` para gerenciar volume, pan, nome e seletor `TrackColorSwatchStrip` em `studio/Source/UI/Inspector/TrackCard.h/.cpp`, verificando preservação de gestos de fader
- [x] 2.3 Extrair `ClipCard` para gerenciar instrumentos (`Single`, `Multi`, `Scatterer`, `Programmer`, `Command`, `Silence`), fades e AHDSR em `studio/Source/UI/Inspector/ClipCard.h/.cpp`, verificando integridade dos controles
- [x] 2.4 Extrair editores secundários (`BusCard`, `ParameterCard`, `SnapshotCard`, `VcaCard`, `SampleCard`) sob `studio/Source/UI/Inspector/`, enxugando `InspectorPanel.cpp` para orquestração de layout e mantendo compilação do Studio
- [x] 2.5 Conectar as ações de *Reset to Default* e *Copy / Paste Values* nos cartões desacoplados, verificando transações atômicas de Undo

## 3. Documentação, Verificação e Gates

- [x] 3.1 Atualizar o manual offline (`docs/manual/`) e o guia de UI (`docs/UI_GUIDELINES.md`) descrevendo o Lock Inspector, atalhos de cabeçalho e menu de contexto de cartões
- [x] 3.2 Executar os testes de fumaça do Studio Release (`scripts/verify.ps1 -Layer studio`) e validar os 11 smokes sem regressão
- [x] 3.3 Validar a integridade dos artefatos com `openspec validate studio-inspector-cards-and-locking --strict` e executar o gate de documentação (`scripts/verify.ps1 -Layer docs`)

## Workflow follow-up

- Arquivar a change com `openspec-archive-change` após validação completa e sincronizar requisitos em `openspec/specs/studio-shell/spec.md`.
