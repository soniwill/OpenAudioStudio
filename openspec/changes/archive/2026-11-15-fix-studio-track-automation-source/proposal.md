# Proposal

## Why

A faixa `Volume Automation` da Timeline lê e edita a primeira automação cadastrada na pista. Se essa entrada for uma curva por parâmetro, a Timeline interpreta o domínio como milissegundos e o alvo como ganho em dB, podendo gravar pontos inválidos na curva que o Inspector edita.

## What Changes

- Restringir a leitura e a edição da faixa `Volume Automation` à curva temporal de volume da pista.
- Mostrar a grade vazia quando a pista só tiver automações de outras origens ou alvos. Inserir o primeiro ponto por duplo clique cria uma curva temporal de volume e preserva as entradas existentes.
- Cobrir pistas com curvas temporais e curvas por parâmetro em ambas as ordens. Atualizar os capítulos 5 e 6 do manual offline com os rótulos do Studio e o domínio de cada editor.
- Manter a abertura e o recolhimento pelo botão `A` conforme `fix-studio-track-automation-toggle`. Fades de clipe, mudanças de reprodução e reparo de curvas previamente alteradas ficam fora do escopo.

## Capabilities

### New Capabilities

### Modified Capabilities

- `studio-shell` — definir quais pontos a faixa de automação da Timeline pode exibir, criar e editar.

## Impact

- Studio — `AutomationLaneComponent`, operações de autoria de automação por pista, smoke integrado e manual offline.
- Engine, bancos e Unity — a mudança usa os campos já existentes `is_timeline` e `target`. O formato e a avaliação das curvas permanecem os mesmos.
