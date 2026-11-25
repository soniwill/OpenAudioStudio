# Tasks

## 1. Liberação nativa e regressão

- [x] 1.1 **Motor e testes nativos.** Corrigir `oae_system_release` para descarregar e invalidar os bancos de seu proprietário com o sistema e as dependências da descarga ainda vivos. Preservar as validações de reentrada, a descarga explícita e o isolamento entre sistemas. Usar os fixtures de banco dos testes nativos para verificar bancos síncronos, carga assíncrona e um segundo proprietário que permanece operável.
- [x] 1.2 **Verificação nativa.** Executar `scripts/verify.ps1 -Layer native` e repetir em processo isolado o caso mínimo `create → init sem dispositivo → load bank → release → sair`, comparando com o controle que descarrega o banco antes do release. Registrar comando e código de saída. Ambos os casos devem terminar com zero.

## 2. Aceite integrado

- [x] 2.1 **Studio.** Abrir uma cópia de teste de `example_project/project.json`, confirmar que a prévia está pronta, salvar um preset de workspace e fechar pela janela principal. Conferir o código de saída zero. Reabrir com o mesmo projeto, confirmar a prévia e selecionar o preset salvo para verificar sua restauração. Registrar os passos reproduzíveis, códigos de saída e observações em `verification.md`.
- [x] 2.2 **Gates.** Executar `scripts/verify.ps1 -Layer studio`, `scripts/verify.ps1 -Layer docs` e `openspec validate studio-bank-shutdown --strict`. Registrar os resultados em `verification.md`.
