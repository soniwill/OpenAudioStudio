# Spec Delta

## ADDED Requirements

### Requirement: Exibição da faixa de automação pelo cabeçalho da pista
O botão `A` de cada pista da timeline SHALL alternar a visibilidade da sua faixa de automação abaixo dos clipes. A timeline SHALL manter o destaque do botão e a altura do cabeçalho e da pista sincronizados com essa visibilidade, sem alterar os dados do projeto.

#### Scenario: Expandir e recolher uma pista com curva existente
- **WHEN** o usuário clica no botão `A` de uma pista recolhida com uma curva de automação de volume cadastrada
- **THEN** o botão fica destacado e a faixa de automação aparece abaixo dos clipes, com a curva e seus pontos editáveis
- **AND** as outras pistas mantêm seu estado de expansão
- **WHEN** o usuário clica novamente no mesmo botão
- **THEN** o destaque é removido, a faixa de automação é ocultada e a pista volta à altura recolhida
- **AND** os pontos da curva permanecem disponíveis ao expandir novamente

#### Scenario: Expandir uma pista sem curva
- **WHEN** o usuário clica no botão `A` de uma pista recolhida sem curva de automação
- **THEN** a faixa de automação aparece com rótulo e grade vazia
- **AND** o clique não cria pontos ou curvas no projeto
