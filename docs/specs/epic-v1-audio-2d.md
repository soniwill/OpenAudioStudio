# Épico 11 — OpenAudioStudio V1 para jogos 2D

## Objetivo e escopo

Entregar o fluxo Studio → banco → jogo para Unity no Windows x64. A V1 deve permitir que um designer publique, ajuste e diagnostique o áudio de uma cena 2D usando o Studio e o pacote Unity distribuídos. O jogo informa eventos e mudanças de estado. O projeto de áudio define seleção de samples, condições, curvas, roteamento e transições.

A cena de aceite amplia o protótipo inicial de validação na Unity. Reaproveite os assets e componentes onde atendam aos casos da V1. A suíte de base comprova o fluxo fundamental e serve de referência para regressão.

O diagnóstico reaproveita a medição do Épico 7. Live Update completo, gravação de sessões de profiling, espacialização 3D avançada, novas plataformas e as expansões T18–T25 seguem em seus próprios marcos.

## Tarefas e gates

| Tarefa | Entrega e critério de aceite |
| --- | --- |
| **V1-01 — Paridade entre autoria, banco e jogo** | Exercitar, a partir do Studio distribuído, playlists com pesos e sem repetição imediata, condições, automações, snapshots, políticas de limite de instâncias, cooldown e loops de instrumento. Reproduzir o mesmo banco no preview e no jogo com os mesmos parâmetros. Comparar escolhas determinísticas quando aplicável e verificar resultados sonoros e de estado. Um campo inválido deve ser diagnosticado com o caminho correspondente e a publicação válida anterior deve continuar utilizável. Cobrir a regressão de instrumento em loop que fica em estado de reprodução após esgotar o áudio. |
| **V1-02 — Emissão e zonas 2D na Unity** | Disponibilizar gatilhos de entrada e saída com `Collider2D`, reprodução avulsa ou em loop, parâmetros configuráveis no Inspector e referência de escuta selecionável. Pan e atenuação no plano 2D devem ser opcionais. Validar parada com fade ao sair da zona ou descarregar a cena. Exercitar o pan de disparos sequenciais e a zona de ambiência da cena. |
| **V1-03 — Música adaptativa no relógio de áudio** | Permitir a autoria no Studio de loops contínuos, camadas, transições por tempo ou compasso e crossfade. Usar o relógio de áudio e a posição musical da instância em execução. Verificar no render headless o frame de transição esperado e a continuidade do sinal no loop e no crossfade. No player Unity, variar a cadência de frames e confirmar que a transição continua sincronizada. |
| **V1-04 — Streaming no conteúdo publicado** | Selecionar no Studio música e ambiência longas para streaming e SFX curtos para pré-carregamento. Usar o conteúdo publicado pelo fluxo normal do pacote Unity. Comparar o uso de memória de uma faixa longa em streaming com o da mesma faixa pré-carregada e verificar carregamento, repetição, parada e descarregamento. Um asset indisponível deve produzir diagnóstico acionável. |
| **V1-05 — Mix por categorias e diagnóstico em jogo** | Entregar volumes independentes de música, SFX, UI e ambiência, snapshot de pausa, ducking e políticas de prioridade e limite de instâncias configuradas no Studio. Em Development Build, expor bancos carregados, eventos ativos, vozes reais e virtuais e níveis por barramento. Validar uma rajada de efeitos acima do orçamento de vozes e a recuperação do mix após a pausa ou a fanfarra. |
| **V1-06 — Cena 2D de aceite em Unity** | Entregar projeto de autoria, assets, cena com resultados reproduzíveis, pacote de distribuição, instruções de publicação e roteiro de validação. Em um projeto Unity limpo, importar o pacote distribuído, gerar os bancos pelo Studio, selecionar eventos pelo catálogo, criar uma build Windows x64 e executar os cenários abaixo. |

## Cena 2D de aceite

A cena deve permitir acionar manualmente transição neutra, pico de tensão por progresso, recompensa simples e recompensa máxima. Cada estado exercita regras de áudio autoradas no Studio e pode ser repetido em sequência. Uma rotina automática com sequência predefinida facilita a comparação e o teste de carga.

| Cenário | Comportamento verificável |
| --- | --- |
| **Aproximar-se da área ativa e iniciar o ciclo** | Entrar numa zona `Collider2D` ativa a ambiência com fade e sair restaura o nível esperado. A música base e a ambiência usam assets longos em streaming. O botão de ação toca no barramento de UI e o loop mecânico sustenta enquanto o ciclo estiver ativo. |
| **Disparar impactos em sequência ou em rajada** | Os disparos percorrem o estéreo na ordem visual. A playlist usa pesos e evita repetir a mesma variante consecutivamente. O modo rajada excede o limite configurado de instâncias e permite observar cooldown, prioridade e roubo de vozes. O impacto final e os sinais prioritários da UI permanecem audíveis. |
| **Tensão crescente sem desfecho** | O parâmetro de progresso ativa uma camada de tensão por condição e automação. A conclusão resolve a tensão com fade e a música retorna ao estado base. Nenhuma voz de tensão fica ativa após a resolução. |
| **Recompensa simples e destaque principal** | O nível do resultado altera as camadas e o contador. O contador sustenta o loop de contagem até receber `KeyOff` e conclui a cauda. No destaque principal, o snapshot abre espaço para a fanfarra e a seção musical troca no próximo compasso com crossfade. Ao terminar, o mix e a música voltam ao estado esperado. |
| **Cena secundária com banco sob demanda** | Uma sequência especial muda a música e a ambiência por estado de jogo. Um banco de conteúdo separado fornece as variações dessa fase e compartilha o roteamento do Master Bank. A entrada aguarda o conteúdo ficar disponível e a saída descarrega o banco após as caudas, preservando o áudio do estado base. |
| **Pausar, retomar e sair da cena** | O snapshot de pausa age nas categorias previstas, com a UI audível. A retomada restaura o estado anterior, inclusive se a pausa ocorrer durante o destaque principal. Sair da cena libera as instâncias e os bancos de conteúdo carregados. Ao voltar, é possível repetir outro ciclo. |
| **Republicar um ajuste e validar erro de autoria** | Alterar no Studio um peso de playlist e uma curva de mix, publicar e verificar o efeito na build. Em seguida, publicar uma alteração inválida e confirmar que o diagnóstico aponta o campo e o par banco/catálogo anterior continua carregável. |

## Evidências para fechar o épico

- Testes nativos e headless para loop e liberação, transições no relógio de áudio, mix, vozes e streaming. Registrar configuração, resultado e localização da evidência.
- Testes Unity EditMode e PlayMode para seleção de eventos, física 2D, parâmetros e ciclo de vida. Executar os cenários da tabela também no player Windows x64, incluindo uma Development Build para conferir o diagnóstico.
- Registrar versão do Studio e do pacote, banco e catálogo usados, sequência de resultados, observações de áudio e medições de vozes e memória. Marcar verificações indisponíveis como pendentes.
- Documentar como reproduzir o fluxo com o pacote e o projeto de exemplo, incluindo a ordem de carregamento de Master Bank e bancos de conteúdo, mensagens de erro esperadas e como inspecionar o diagnóstico em jogo.
