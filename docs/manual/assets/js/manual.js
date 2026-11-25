// Índice offline para busca instantânea via file:// sem dependência de servidor
const MANUAL_INDEX = [
  { title: "Ouvir o primeiro evento", url: "chapters/03-transporte-e-audicao.html#primeira-audicao", tags: "primeiro som moeda play stop prévia audição" },
  { title: "Publicar um banco", url: "chapters/02-projetos-e-publicacao.html#publicar", tags: "build bank publicação compilar catálogo crc" },
  { title: "Criar uma transição musical", url: "chapters/11-tutoriais-praticos.html#logic-transition", tags: "transition region região de transição marker quantização condição crossfade" },
  { title: "Configurar um emissor 3D na Unity", url: "chapters/09-unity-componentes-inspector.html#listener", tags: "studio event emitter studio listener emissor ouvinte cone atenuação envelopment split-screen" },
  { title: "Vincular um instrumento com Trigger Sheet", url: "chapters/05-timeline-e-waveforms.html#parameter-sheets", tags: "parameter sheet folha de parâmetros sheet min sheet max psht intervalo rpm" },
  { title: "Testar Async com Sustain Point e Key Off", url: "chapters/11-tutoriais-praticos.html#async", tags: "async assíncrono sustentação liberação carga" },
  { title: "Configurar uma playlist para Multi ou Scatterer", url: "chapters/07-deck-inferior-mixer-efeitos.html#aba3-playlist", tags: "scatterer playlist weights pesos amostra evento aninhado polyphony spawn interval" },
  { title: "Exercitar Multi, Scatterer e Programmer", url: "chapters/11-tutoriais-praticos.html#playlist-programmer", tags: "multi scatterer programmer audio table tabela de áudio playlist weights pesos chave coin_demo" },
  { title: "Disparar um Command Instrument", url: "chapters/05-timeline-e-waveforms.html#command-instruments", tags: "command instrument instrumento de comando command type command target start_event stop_event snapshot_on snapshot_off" },
  { title: "Abrir Diagnostics & Log", url: "chapters/07-deck-inferior-mixer-efeitos.html#aba6-diagnostics", tags: "diagnostics & log logs diagnóstico erro publicação live linking build profiler" },
  { title: "Diagnostics Overlay e Contagem de Vozes", url: "chapters/09-unity-componentes-inspector.html#overlay", tags: "diagnostics overlay vozes reais virtuais telemetria f9 profiler unity" },
  { title: "Organizar a Workspace Bar", url: "chapters/01-visao-geral-interface.html#workspace-bar", tags: "workspace dock layout encaixe templates presets ocultar painéis restaurar hide all restore log" },
  { title: "Ajustar os modificadores de parâmetros", url: "chapters/06-inspetor-e-modulacao.html#pmod", tags: "pmod parâmetros velocity seek speed hold during playback retenção" },
  { title: "Fundamentos — A Teoria dos Eventos e Parâmetros", url: "chapters/fundamentos-eventos-e-parametros.html", tags: "teoria eventos parametros definicao instancia polifonia voice stealing desacoplamento built-in 3d distance speed" },
  { title: "Fundamentos — Topologia de Mixagem, Buses, VCAs e Snapshots", url: "chapters/fundamentos-mixagem-buses-vcas-snapshots.html", tags: "teoria mixagem bus vca voltage controlled amplifier snapshot signal flow headroom soma pcm override blending dsp inserts" },
  { title: "Cap. 0 — Conceitos Básicos de Áudio Interativo", url: "chapters/00-conceitos-basicos.html", tags: "iniciante teoria db pan semitons loop fade sample evento bus vca snapshot parametro bank" },
  { title: "Barramentos, VCAs e snapshots", url: "chapters/14-barramentos-vcas-snapshots.html", tags: "bus vca voltage controlled amplifier master mixer mixagem ganho efetivo fader roteamento headroom parent" },
  { title: "Cap. 1 — Visão Geral da Interface do Studio", url: "chapters/01-visao-geral-interface.html", tags: "janela paineis docking splitter atalhos ctrl+z ctrl+y spacebar" },
  { title: "Cap. 2 — Projetos Portáteis e Publicação (.bank)", url: "chapters/02-projetos-e-publicacao.html", tags: "criar projeto abrir renomear duplicar salvar new project open save build bank assets json master bank content bank catalog crc32" },
  { title: "Cap. 3 — Barra de Transporte e Audição", url: "chapters/03-transporte-e-audicao.html", tags: "play pause stop loop key off timecode status sliders parametros tempo real" },
  { title: "Cap. 4 — Navegador do Projeto (Tree Browser)", url: "chapters/04-navegador-projeto.html", tags: "event snapshot bus vca samples global parameters busca f2 dup del" },
  { title: "Cap. 5 — Timeline, Faixas e Waveforms", url: "chapters/05-timeline-e-waveforms.html", tags: "minimap regua snap magnetico compasso bpm track header volume pan mute solo clipe fade" },
  { title: "Cap. 6 — Inspetor de Propriedades e Modulação", url: "chapters/06-inspetor-e-modulacao.html", tags: "inspector random ahdsr attack hold decay sustain release automacao curvas linear s-curve" },
  { title: "Cap. 7 — Deck Inferior: Mixer, Efeitos, Playlist, Lógica e 3D", url: "chapters/07-deck-inferior-mixer-efeitos.html", tags: "mixer effects rack limiter compressor sidechain ducking eq3 lowpass highpass reverb delay playlist shuffle logic sustain transition spatializer 3d diagnostics" },
  { title: "Cap. 8 — Unity: Como Studio e Unity Trabalham Juntos", url: "chapters/08-unity-instalacao-e-workflow.html", tags: "unity upm com.openaudio.core streamingassets oae_audio.dll master bank workflow pipeline" },
  { title: "Cap. 9 — Unity: Componentes do Inspector", url: "chapters/09-unity-componentes-inspector.html", tags: "oaeaudiomanager studiobankloader studioeventemitter studiolistener eventref drawer diagnostics overlay" },
  { title: "Cap. 10 — Unity: API C# e Live-Linking", url: "chapters/10-unity-api-csharp-e-live.html", tags: "oaesystem bank eventinstance bus vca callbacks beat marker liveclient livebridgeserver" },
  { title: "Unity: Timeline, bancos e mixagem avançada", url: "chapters/13-unity-timeline-e-bancos.html", tags: "timeline addressables assetbundle banco sob demanda referências send return programmer instrument audio table tabela de audio oaeeventtrack oaeeventclip pré-requisitos pre-requisitos" },
  { title: "Cap. 11 — Tutoriais Práticos Ponta-a-Ponta", url: "chapters/11-tutoriais-praticos.html", tags: "tutorial passos musica interativa ducking snapshot unity audioplayground" },
  { title: "Cap. 12 — Glossário Unificado A–Z e Atalhos", url: "chapters/12-glossario-e-atalhos.html", tags: "glossario dicionario termos atalhos teclado referencia" }
];

(function () {
  const isChapter = window.location.pathname.replace(/\\/g, "/").includes("/chapters/");
  const prefix = isChapter ? "../" : "";

  // 0. Renderiza sidebar compartilhada se houver #manual-sidebar vazio
  const sidebarEl = document.getElementById("manual-sidebar");
  if (sidebarEl && sidebarEl.children.length === 0) {
    sidebarEl.innerHTML = `
      <div class="brand">
        <div class="brand-badge">OA</div>
        <div>
          <div class="brand-title">OpenAudio Studio</div>
          <div class="brand-sub">Manual Completo + Unity</div>
        </div>
      </div>
      <div class="search-box">
        <input id="manual-search" type="search" aria-label="Buscar no manual" placeholder="Buscar controle, termo ou capítulo..." autocomplete="off">
        <div id="manual-search-results" class="search-results" aria-live="polite"></div>
      </div>
      <div class="nav-group-label">Fundamentos &amp; Studio</div>
      <ul class="nav-list">
        <li><a href="${prefix}index.html">Início &amp; Mapa do Manual</a></li>
        <li><a href="${prefix}chapters/fundamentos-eventos-e-parametros.html">Fundamentos: Eventos &amp; Parâmetros</a></li>
        <li><a href="${prefix}chapters/fundamentos-mixagem-buses-vcas-snapshots.html">Fundamentos: Buses, VCAs &amp; Snapshots</a></li>
        <li><a href="${prefix}chapters/00-conceitos-basicos.html">Cap. 0 — Conceitos Básicos</a></li>
        <li><a href="${prefix}chapters/14-barramentos-vcas-snapshots.html">Barramentos, VCAs e snapshots</a></li>
        <li><a href="${prefix}chapters/01-visao-geral-interface.html">Cap. 1 — Visão Geral da Interface</a></li>
        <li><a href="${prefix}chapters/02-projetos-e-publicacao.html">Cap. 2 — Projetos e Publicação</a></li>
        <li><a href="${prefix}chapters/03-transporte-e-audicao.html">Cap. 3 — Transporte e Audição</a></li>
        <li><a href="${prefix}chapters/04-navegador-projeto.html">Cap. 4 — Navegador do Projeto</a></li>
        <li><a href="${prefix}chapters/05-timeline-e-waveforms.html">Cap. 5 — Timeline e Waveforms</a></li>
        <li><a href="${prefix}chapters/06-inspetor-e-modulacao.html">Cap. 6 — Inspetor e Modulação</a></li>
        <li><a href="${prefix}chapters/07-deck-inferior-mixer-efeitos.html">Cap. 7 — Bottom Deck e Logs</a></li>
      </ul>
      <div class="nav-group-label">Integração Unity</div>
      <ul class="nav-list">
        <li><a href="${prefix}chapters/08-unity-instalacao-e-workflow.html">Cap. 8 — Studio ↔ Unity</a></li>
        <li><a href="${prefix}chapters/09-unity-componentes-inspector.html">Cap. 9 — Componentes na Unity</a></li>
        <li><a href="${prefix}chapters/10-unity-api-csharp-e-live.html">Cap. 10 — API C# e Live-Linking</a></li>
        <li><a href="${prefix}chapters/13-unity-timeline-e-bancos.html">Unity — Integração avançada</a></li>
      </ul>
      <div class="nav-group-label">Prática &amp; Referência</div>
      <ul class="nav-list">
        <li><a href="${prefix}chapters/11-tutoriais-praticos.html">Cap. 11 — Tutoriais Práticos</a></li>
        <li><a href="${prefix}chapters/12-glossario-e-atalhos.html">Cap. 12 — Glossário A–Z e Atalhos</a></li>
      </ul>`;
  }

  // 1. Busca global
  const searchInput = document.getElementById("manual-search");
  const searchResults = document.getElementById("manual-search-results");
  if (searchInput && searchResults) {
    const normalize = (value) => value.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase();
    searchInput.addEventListener("input", () => {
      const q = normalize(searchInput.value).trim();
      if (!q) {
        searchResults.classList.remove("open");
        searchResults.innerHTML = "";
        return;
      }
      const matches = MANUAL_INDEX.filter(
        (item) =>
          q.split(/\s+/).every((word) => normalize(item.title + " " + item.tags).includes(word))
      );
      if (matches.length === 0) {
        const message = document.createElement("div");
        message.className = "search-result-item";
        message.textContent = "Nenhum resultado encontrado.";
        searchResults.replaceChildren(message);
      } else {
        searchResults.replaceChildren(...matches.map((entry) => {
          const link = document.createElement("a");
          link.className = "search-result-item";
          link.href = prefix + entry.url;
          link.textContent = entry.title;
          return link;
        }));
      }
      searchResults.classList.add("open");
    });

    searchInput.closest(".search-box")?.addEventListener("keydown", (event) => {
      if (event.key === "Escape" && searchResults.classList.contains("open")) {
        searchResults.classList.remove("open");
        searchInput.focus();
      }
    });

    document.addEventListener("click", (e) => {
      if (!searchInput.contains(e.target) && !searchResults.contains(e.target)) {
        searchResults.classList.remove("open");
      }
    });
  }

  // 2. Destaque ativo na barra lateral
  const currentFile = window.location.pathname.replace(/\\/g, "/").split("/").pop() || "index.html";
  document.querySelectorAll(".nav-list a").forEach((link) => {
    const href = link.getAttribute("href") || "";
    if (href.endsWith(currentFile)) {
      link.classList.add("active");
      link.setAttribute("aria-current", "page");
    }
  });

  // 3. Interação legenda <-> destaque SVG
  document.querySelectorAll(".callout-figure").forEach((fig) => {
    const legendItems = fig.querySelectorAll(".callout-legend li[data-callout]");
    legendItems.forEach((li) => {
      const id = li.getAttribute("data-callout");
      const toggle = (on) => {
        li.classList.toggle("is-highlighted", on);
        fig.querySelectorAll(`.callout-group[data-callout="${id}"]`).forEach((g) => {
          g.classList.toggle("is-highlighted", on);
        });
      };
      li.addEventListener("mouseenter", () => toggle(true));
      li.addEventListener("mouseleave", () => toggle(false));
    });
  });

  const lightbox = document.createElement("dialog");
  lightbox.className = "lightbox";
  lightbox.setAttribute("aria-label", "Visualização ampliada");
  lightbox.innerHTML = '<div class="lightbox-inner"><button class="lightbox-close" aria-label="Fechar" type="button">×</button><img alt=""></div>';
  document.body.appendChild(lightbox);

  const lbImg = lightbox.querySelector("img");
  lightbox.querySelector("button").addEventListener("click", () => lightbox.close());
  lightbox.addEventListener("click", (event) => {
    if (event.target === lightbox) lightbox.close();
  });

  document.querySelectorAll(".callout-stage img").forEach((img) => {
    const zoom = document.createElement("button");
    zoom.type = "button";
    zoom.className = "image-zoom";
    zoom.setAttribute("aria-label", "Ampliar imagem: " + img.alt);
    img.before(zoom);
    zoom.append(img);
    zoom.addEventListener("click", () => {
      lbImg.src = img.src;
      lbImg.alt = img.alt;
      lightbox.showModal();
    });
  });

  // 5. Botão de copiar em blocos <pre>
  document.querySelectorAll("pre").forEach((pre) => {
    const btn = document.createElement("button");
    btn.className = "copy-btn";
    btn.type = "button";
    btn.textContent = "Copiar";
    btn.addEventListener("click", async () => {
      const code = pre.querySelector("code")?.innerText || pre.innerText;
      try {
        await navigator.clipboard.writeText(code);
        btn.textContent = "Copiado!";
        setTimeout(() => (btn.textContent = "Copiar"), 1500);
      } catch {
        btn.textContent = "Erro";
      }
    });
    pre.appendChild(btn);
  });
})();
