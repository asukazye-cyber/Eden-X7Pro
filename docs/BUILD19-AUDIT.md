# Build19 — auditoria e contrato de medição

Base preservada: Build18, commit do wrapper `fda2affdec4303af58760b760f730d07e9009d2c`,
Eden `5d150cac5c6624fa6a0e94266846b75d91a6a5d0`. Não há benchmark físico novo neste documento.

## O que os números da Build18 realmente significam

O fluxo auditado era `Composite → GetRenderFrame (espera) → DrawToFrame → RecordRealFrame →
ShouldGenerate → Process → EnsureReady → CopyCurrentFrame → GenerateInto → Flush sintético →
Present sintético → Flush real → Present real → fila PresentManager → AcquireNextImage →
cópia para swapchain → vkQueuePresentKHR`.

`ShouldGeneratePocoX7ProFrame` era o único lugar que incrementava o contador dropped:
intervalo real instantâneo maior que 33,333 / 36 / 34,5 ms, conforme o modo. Portanto, **1041
significa 1041 recusas desse gate**, não 1041 interpolados que comprovadamente chegaram à GPU
e foram descartados. A classificação retrospectiva CPU/GPU/shader/térmica não existe.

A recusa acontecia ANTES da interpolação, mas `Process` ainda executava `EnsureReady` e a cópia
do histórico mesmo com capacidade zero. O contador generated aumentava em `Process`, ANTES de
`GenerateInto`, submit, conclusão da GPU ou apresentação. Portanto os 218 não comprovam 218
quadros exibidos, nem justificam calcular taxa de sucesso de apresentação 218/1259.

O limite instantâneo pode rejeitar jitter normal: 33,36 > 33,333. O batch real só era enviado
depois do batch que também continha FG; o sintético usa a mesma fila gráfica. Esses problemas
são demonstrados pelo código; quanto cada um contribuiu aos 28,5 FPS exige a nova medição física.

## Contrato da Build19

- Detector/afinidade: mantidos. Sem mudar identificadores, extensões, clocks, resolução ou jogo.
- Gate antes de preparação/cópias; histórico invalidado ao pular, evitando interpolar fontes não adjacentes.
- GPU real enviada antes do batch separado de histórico/FG. Ordem de imagens preservada A, A/B, B;
  apresentar B antes de A/B seria temporalmente incorreto, não uma solução de prioridade.
- Reserva sintética sem espera: try-lock, fence signalled, pelo menos dois slots livres, um sintético
  em voo no máximo. Não aumenta filas. Ambos os conjuntos de descritores usam o índice protegido do destino.
- Sintético vencido antes da aquisição não vai para o swapchain: submit vazio consome render_ready
  e sinaliza present_done antes de reutilização. Isso não cancela compute que já foi enviado.
- Prazo local conservador: 16 ms após reserva; não é timestamp de scanout nem garante uma cadência física 60 Hz.
- Estados DISABLED_TEMPORARILY/PROBING/ACTIVE/CONSTRAINED; 15/60 recusas de cooldown e 30 intervalos
  de recuperação; 60 admissões medidas antes de qualidade plena. Limiar atual recusa intervalo >37 ms
  ou P99 >50 ms. Isto pode manter FG suspenso na Wild Area; é preferível a prometer 60 com regressão real.
- Orçamento 1000/30 ms: max(CPU núcleo mais ocupado P95, GPU real P95) + FG GPU/CPU P95 com
  multiplicador 1,25 + espera acquire/QueuePresent P95 + margem adaptativa 3–9 ms. Prova inicial
  exige 30 amostras CPU/GPU; FG não medido recebe reserva conservadora 6 ms, não um valor mostrado como medido.
- AUTO reduz apenas search_radius para 1; não muda escala de motion nem realoca imagens por quadro.
  Off do governor desativa adaptação/histerese, mas não libera violações dos guardas de segurança.

## Medições e limitações

CPU: CLOCK_THREAD_CPUTIME_ID ao redor de ARM interface RunThread, antes de HLE/scheduling/fibers.
Soma por núcleo em cada intervalo host de produção real; mostra máximo e soma. NÃO é o caminho
crítico exato de um frame guest e não inclui todo HLE, rasterizer ou workers. Composite é separado.

GPU: pool de 128 pares, timestampValidBits e timestampPeriod reais, resultados 64-bit + availability,
leitura após tick concluído e sem WAIT_BIT. Pool cheio invalida a amostra, nunca espera nem inventa zero.
Cada delta pertence ao mesmo command buffer; soma dos batches reais separados de histórico/FG.
Não mede utilização %, command buffer de upload, cópia de present, ou latência de exibição.

Shader: tradução/geração de SPIR-V medida separadamente da construção de pipeline. Eventos e
tempo CPU wall, total, maior evento e janelas ~10 s. Eventos podem ocorrer em paralelo;
a soma não é atraso de um único frame. GPU execution não é chamada de shader compilation.

Present: frame/fence wait, acquire (inclui espera de recurso do caminho existente), idade de fila,
e CPU wall de QueuePresent incluindo lock de submit. Presented FPS conta somente VK_SUCCESS/
VK_SUBOPTIMAL_KHR. Não comprova scanout; mailbox/surface podem descartar depois.

Accounting: produced no Composite; attempted = decisão de elegibilidade por fonte; submitted após
vkQueueSubmit com sucesso; generated quando observado fence de conclusão; presented ao aceitar
vkQueuePresent. Os últimos fences podem ser observados apenas quando reciclados. Skipped inclui
cooldown atribuído ao último motivo medido e warmup sem segunda fonte (Source Frame Invalid).
Synchronization e Duplicate/Stale têm contadores separados, mas não são incrementados sem evento
inequívoco. PresentDeadline é prazo local vencido ANTES de adquirir imagem, não medição do display.

P95/P99: nearest-rank, janela de até 300 intervalos REAIS. 1% low = 1000 / média dos ceil(1%)
mais lentos; não 1000/P99. Média ativa acumulada separada. Intervalos >=500 ms são pausas/surface
gaps excluídos, não gameplay. FPS de present não entra nas estatísticas reais.

Thermal: callback Android NDK carregado dinamicamente; leitura inicial na inicialização, nenhum
binder polling no renderer. Severe+ suspende, moderate reduz qualidade. Sem ADPF performance hints,
headroom forecast, root ou mudança térmica do sistema. API indisponível é N/A, não temperatura zero.

Classificador conservador por métricas: térmico, compile recente, espera, GPU, CPU, FG, balanced ou
unknown. É indicação com métricas assíncronas/rolling, não prova causal. Spikes >40 ms geram registro
limitado a um por segundo. Resumos `[X7PRO-B19-SUMMARY]` a cada ~10 s e no fim, no log existente do Eden.
Exportar esse log existente permite comparar testes; não foi criada tela nova de exportação.

## Não implementado / não comprovado

- Garantia absoluta de que compute já enviado nunca atrase o real: impossível impor no scheduler atual,
  sem preempção/filas independentes verificadas. A admissão reduz risco, não prevê um shader futuro.
- Apresentação física uniforme A/A-B/B a 16,67 ms: sem present timing/scanout comprovado.
- DRS, ADPF hints, GPU clocks/utilização, alteração do pacing Android da #18: não realizados.
- Inicialização/reconfiguração dos pipelines FG pode causar um spike CPU e usar o Finish existente
  ao substituir recursos. Não há WaitIdle de profiling nem Finish novo por quadro.
- Melhoria de FPS/P99 ou ausência de regressão no aparelho: aguardam teste físico.

## Fontes técnicas primárias

- [Khronos: timestamp queries](https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html)
- [Vulkan: queries e disponibilidade](https://docs.vulkan.org/spec/latest/chapters/queries.html)
- [Android NDK: Thermal](https://developer.android.com/ndk/reference/group/thermal)

## Teste no aparelho

Manter resolução, threads, modo portátil/docked, limite 100%, save/câmera e temperatura inicial iguais.
Ativar Detailed Performance Overlay. Aquecer shaders antes da medição. Testar FG OFF por 5 min,
depois FG ON + Governor AUTO por 5 min na mesma rota da Wild Area. Exportar o log do Eden em cada
teste (identificado pelos marcadores acima). Não comparar apenas o FPS instantâneo. Se o real/P99
piorarem, usar FG OFF e enviar os dois logs. Não aumentar resolução durante o teste A/B.
