# Tasks

## 1. Studio e manual

- [x] 1.1 Sincronizar o estado do botão `A` com a visibilidade e a geometria do editor na pista correspondente. No smoke de integração da timeline, acionar o botão pelo cabeçalho e verificar editor visível com área positiva ao expandir, oculto ao recolher e alinhamento entre cabeçalho e pista nos dois estados. Usar duas pistas para confirmar a independência do estado e conferir que os cliques preservam os dados de automação.
- [x] 1.2 Atualizar o capítulo 5 do manual offline, na seção de automação, com um exemplo de expandir e recolher pelo botão `A`, incluindo o que aparece sem curva. Conferir os passos no Studio Release e a navegação da seção pelo gate de documentação.

## 2. Verificação integrada

- [x] 2.1 Em uma cópia isolada do projeto de exemplo, abrir `event:/music/theme` no Studio Release com uma curva de volume de dois pontos numa pista e outra pista sem curva. Usar `studio_ui_inspect` para localizar os botões, `studio_ui_mouse` para acionar os cliques e `studio_ui_screenshot` para registrar os estados recolhido, expandido e novamente recolhido. Confirmar a curva e seus pontos, a grade vazia e a preservação do estado da outra pista. Registrar as evidências na change.
- [x] 2.2 Executar `./scripts/verify.ps1 -Layer studio`, `./scripts/verify.ps1 -Layer docs` e `openspec validate fix-studio-track-automation-toggle --type change --strict`. Registrar o resultado dos três comandos.
