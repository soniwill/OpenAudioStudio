# Tasks

## 1. Studio e manual

- [x] 1.1 Em `AutomationLaneComponent` e nas operações relacionadas do `AuthoringModel`, restringir leitura, arraste e duplo clique à curva temporal de volume. Na ausência dessa curva, preservar a grade vazia ao expandir e anexar uma curva temporal de volume ao inserir o primeiro ponto. Preservar geometria, Undo/Redo e demais entradas da pista, conforme o smoke da tarefa 1.2.
- [x] 1.2 No smoke integrado da Timeline, testar uma pista com curva temporal de volume e curva por parâmetro de volume em ambas as ordens. Conferir os pontos visíveis, arrastar e inserir na curva temporal e comparar os dados da curva por parâmetro antes e depois, inclusive após Undo/Redo. Cobrir também a grade vazia e a criação por duplo clique em pista com apenas curvas incompatíveis, incluindo um alvo diferente de volume.

- [x] 1.3 Atualizar os capítulos 5 e 6. Mostrar como expandir `Volume Automation`, inserir e arrastar pontos em tempo e dB, e como criar uma curva por parâmetro em `TRACK PARAM CURVES` com `+ Add Curve`. Conferir os rótulos e a navegação entre as seções no manual offline.
## 2. Verificação integrada

- [x] 2.1 No Studio Release, conferir em uma cópia de projeto via MCP a faixa vazia com apenas uma curva por parâmetro, a criação da curva temporal na Timeline e a preservação da curva do Inspector ao editar. Registrar as evidências e executar `./scripts/verify.ps1 -Layer studio`, `./scripts/verify.ps1 -Layer docs` e `openspec validate fix-studio-track-automation-source --type change --strict`.
