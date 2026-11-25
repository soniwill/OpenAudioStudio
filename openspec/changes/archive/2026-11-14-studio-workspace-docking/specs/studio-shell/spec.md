# Spec Delta

## ADDED Requirements

### Requirement: Layout de referência com Inspector lateral e logs independentes

O Studio SHALL disponibilizar `Diagnostics & Log` como painel encaixável com identidade própria e permitir montar o layout de referência com o Inspector em toda a altura da área de docking, à direita. À esquerda, os logs SHALL ocupar uma divisão imediatamente abaixo do Bottom Deck, com a mesma largura dele.

#### Scenario: Consultar logs enquanto usa o Bottom Deck

- **WHEN** o usuário encaixa o Inspector na borda direita externa e `Diagnostics & Log` abaixo do Bottom Deck no espaço restante à esquerda
- **THEN** o Inspector MUST continuar visível da parte superior à inferior da área de docking, abaixo da barra do workspace
- **AND** os limites esquerdo e direito de `Diagnostics & Log` MUST coincidir com os do Bottom Deck
- **AND** alternar entre Mixer, Effects ou outras abas internas MUST manter os logs visíveis.

#### Scenario: Ajustar o espaço dos logs

- **WHEN** o usuário arrasta o divisor acima dos logs no layout de referência
- **THEN** o Bottom Deck e os logs MUST redistribuir a altura respeitando os tamanhos mínimos aplicáveis
- **AND** a coluna do Inspector MUST conservar seus limites.

### Requirement: Cabeçalhos de docking em grafite

O Studio SHALL usar superfícies grafite nos cabeçalhos dos docks e nas abas internas do Bottom Deck. O cabeçalho com uma única aba SHALL integrar o título à barra. Em grupos de abas, a aba ativa SHALL ser reconhecível pela diferença de fundo e de texto, com indicador de foco de teclado perceptível.

#### Scenario: Distinguir título, seleção e foco

- **WHEN** o usuário compara um dock de aba única com um grupo de abas e navega pelo teclado
- **THEN** o título único MUST permanecer neutro, a aba ativa MUST se distinguir das inativas em escala de cinza e o foco de teclado MUST ser visível independentemente da aba ativa.

## MODIFIED Requirements

### Requirement: Reorganização de painéis por abas e janelas destacáveis (Pop-out)

O sistema de layout do OpenAudio Studio SHALL permitir reorganizar painéis por arraste do título, agrupá-los em abas ou criar divisões horizontais e verticais redimensionáveis e aninhadas. O Studio SHALL indicar a área de encaixe antes da soltura e permitir destacar painéis em janelas flutuantes independentes com retorno ao dock.

#### Scenario: Destacar painel em janela flutuante

- **WHEN** o usuário acionar o comando ou botão de destaque (*pop-out*) em um painel do Studio
- **THEN** o painel MUST ser removido do grupo ancorado e aberto em uma janela flutuante redimensionável e operável simultaneamente ao restante da aplicação.

#### Scenario: Retorno de janela destacada ao dock principal

- **WHEN** a janela flutuante destacada for fechada ou o comando de retorno for acionado
- **THEN** o painel MUST retornar à posição e ao grupo de ancoragem original, reconstituindo a divisão se necessário quando a estrutura de origem ainda estiver disponível.

#### Scenario: Agrupar ou reordenar abas pelo título

- **WHEN** o usuário arrasta o título de um painel sobre uma barra de abas e solta em uma posição válida
- **THEN** a prévia MUST indicar a posição de inserção e o painel MUST aparecer nessa posição no grupo de destino
- **AND** a instância do painel MUST sair da posição anterior e ocupar uma única posição no workspace.

#### Scenario: Dividir um painel ou a área de docking

- **WHEN** o usuário arrasta uma aba para uma borda de outro painel ou para uma borda externa da área de docking
- **THEN** o Studio MUST mostrar a área de encaixe proposta antes da soltura
- **AND** ao soltar, MUST criar uma divisão redimensionável no eixo correspondente, abrangendo o painel de destino ou a área de docking, respectivamente.

#### Scenario: Cancelar um arraste

- **WHEN** o usuário pressiona Esc durante o arraste ou solta sem um alvo de encaixe válido
- **THEN** a prévia MUST desaparecer e a posição, seleção e visibilidade dos painéis MUST permanecer como antes do gesto.

### Requirement: Reconfiguração de painéis, ocultação total e gerenciamento de templates e presets de layout

O sistema de docking do Studio SHALL permitir reconfigurar o encaixe de cada painel, fechar ou ocultar painéis individualmente ou todos simultaneamente mantendo uma barra mínima de restauração, alternar entre os templates nativos (`Default (Authoring)`, `Stage Focus` e `Mixing & Mastering`), criar e excluir layouts personalizados e restaurar o layout padrão via `Reset Layout`.

#### Scenario: Ocultação individual e ocultação total de painéis com restauração imediata

- **WHEN** o usuário fecha ou oculta um painel específico ou aciona a ação de ocultar todos os painéis na barra de workspace
- **THEN** os painéis ocultos deixam de ocupar área de tela, os painéis visíveis expandem-se para preencher o espaço livre e a barra superior compacta permanece acessível para reabrir qualquer painel com um clique
- **AND** a ação `Restore` após `Hide All` MUST recuperar as divisões e os grupos de abas anteriores.

#### Scenario: Aplicação de templates nativos, criação, exclusão e reset de layouts

- **WHEN** o usuário seleciona um template nativo, salva o workspace como layout personalizado, exclui um layout personalizado ou aciona `Reset Layout`
- **THEN** o workspace MUST aplicar o estado correspondente, registrar a configuração e os presets personalizados em `oae_studio_layout.json` e proteger os templates nativos contra exclusão.

#### Scenario: Salvar e restaurar o layout de referência

- **WHEN** o usuário monta o layout de referência, altera um divisor, salva como preset, troca de layout, reinicia o Studio e volta a selecionar o preset salvo
- **THEN** o Studio MUST restaurar as divisões, proporções, posições, visibilidade, ordem e aba ativa de cada grupo salvos, respeitando os tamanhos mínimos da janela atual
- **AND** Inspector, Bottom Deck e Diagnostics & Log MUST voltar a aparecer nas posições relativas do layout de referência.

#### Scenario: Abrir um layout de slots salvo antes da mudança

- **WHEN** o Studio abre um arquivo de layout válido de formato anterior à mudança
- **THEN** o workspace MUST restaurar o estado e os presets legíveis com geometria equivalente, incluindo dimensões, visibilidade e agrupamento conforme os dados disponíveis.
