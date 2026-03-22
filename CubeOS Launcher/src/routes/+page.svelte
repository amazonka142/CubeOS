<script lang="ts">
  import { invoke } from "@tauri-apps/api/core";
  import { onMount } from "svelte";

  type PrimarySectionId = "play" | "versions" | "news" | "storage";
  type SectionId = PrimarySectionId | "release";

  type ReleaseSummary = {
    tag: string;
    title: string;
    notes: string;
    sourceUrl: string;
    publishedAt?: string | null;
    macosAssetName: string;
    assetFormat: string;
    isPrerelease: boolean;
    installed: boolean;
    installedAppPath?: string | null;
  };

  type LauncherState = {
    installRoot: string;
    sharedDataRoot: string;
    releases: ReleaseSummary[];
  };

  type InstallResult = {
    tag: string;
    appName: string;
    appPath: string;
  };

  type NavItem = {
    id: PrimarySectionId;
    label: string;
    eyebrow: string;
    detail: string;
  };

  const navItems: NavItem[] = [
    {
      id: "play",
      label: "Играть",
      eyebrow: "Главная",
      detail: "Быстрый запуск текущей сборки",
    },
    {
      id: "versions",
      label: "Установки",
      eyebrow: "Библиотека",
      detail: "Управление версиями CubeOS",
    },
    {
      id: "news",
      label: "Новости",
      eyebrow: "Лента",
      detail: "Последние релизы и заметки",
    },
    {
      id: "storage",
      label: "Данные",
      eyebrow: "Папки",
      detail: "Версии, сейвы и служебные пути",
    },
  ];

  const SNAPSHOT_VISIBILITY_KEY = "cubeos-launcher.showSnapshots";

  let activeSection = $state<SectionId>("play");
  let launcher = $state<LauncherState | null>(null);
  let loading = $state(true);
  let error = $state("");
  let status = $state("Syncing CubeOS releases...");
  let showSnapshots = $state(false);
  let busyTag = $state<string | null>(null);
  let busyAction = $state<"install" | "launch" | "folder" | "data" | null>(null);
  let selectedTag = $state<string | null>(null);
  let releaseReturnSection = $state<PrimarySectionId>("news");

  const releases = $derived(launcher?.releases ?? []);
  const snapshotCount = $derived(releases.filter((release) => release.isPrerelease).length);
  const visibleReleases = $derived(
    showSnapshots ? releases : releases.filter((release) => !release.isPrerelease),
  );
  const latestRelease = $derived(visibleReleases[0] ?? null);
  const installedReleases = $derived(visibleReleases.filter((release) => release.installed));
  const installedCount = $derived(installedReleases.length);
  const selectedRelease = $derived(
    visibleReleases.find((release) => release.tag === selectedTag) ??
      installedReleases[0] ??
      latestRelease ??
      null,
  );
  const activeNav = $derived.by(() => {
    if (activeSection === "release") {
      return {
        id: "release" as SectionId,
        label: "Подробнее",
        eyebrow: "Релиз",
        detail: "Полное описание выбранной сборки",
      };
    }

    return navItems.find((item) => item.id === activeSection) ?? navItems[0];
  });

  function actionLabel(release: ReleaseSummary | null) {
    if (!release) {
      return "ВЫБЕРИ СБОРКУ";
    }

    return release.installed ? "ИГРАТЬ" : "УСТАНОВИТЬ И ИГРАТЬ";
  }

  function releaseStateLabel(release: ReleaseSummary | null) {
    if (!release) {
      return "Сборка не выбрана";
    }

    return release.installed ? "Установлена локально" : "Нужно установить";
  }

  function releaseChannelLabel(release: ReleaseSummary | null) {
    if (!release) {
      return "Stable";
    }

    return release.isPrerelease ? "Snapshot" : "Stable";
  }

  function releaseChannelDescription(release: ReleaseSummary | null) {
    if (!release) {
      return "Сборка не выбрана";
    }

    return `${releaseChannelLabel(release)} • ${releaseStateLabel(release)}`;
  }

  function releaseStateHint(release: ReleaseSummary | null) {
    if (!release) {
      return "Выбери версию в разделе «Установки».";
    }

    return release.installed
      ? "Выбрана для запуска"
      : `Установка из ${release.macosAssetName}.`;
  }

  function releaseFooterHint(release: ReleaseSummary | null) {
    if (!release || release.installed) {
      return "";
    }

    return `Будет установлена сборка из ${release.macosAssetName}.`;
  }

  function visibleReleaseHeadline() {
    if (!showSnapshots && snapshotCount > 0) {
      return "Стабильные релизы и заметки CubeOS";
    }

    return "Последние заметки CubeOS";
  }

  function snapshotSummary() {
    if (snapshotCount === 0) {
      return "GitHub pre-release сборки";
    }

    return `GitHub pre-release сборки (${snapshotCount})`;
  }

  function formatDate(value?: string | null) {
    if (!value) {
      return "дата неизвестна";
    }

    try {
      return new Intl.DateTimeFormat("ru-RU", {
        dateStyle: "medium",
        timeStyle: "short",
      }).format(new Date(value));
    } catch {
      return value;
    }
  }

  function notePreview(notes: string, maxLength = 220) {
    const collapsed = notes.replace(/\s+/g, " ").trim();
    if (!collapsed) {
      return "У релиза пока нет текста заметок, но сборка уже доступна для установки.";
    }

    return collapsed.length > maxLength ? `${collapsed.slice(0, maxLength - 3)}...` : collapsed;
  }

  type ReleaseNoteBlock =
    | { kind: "heading"; text: string }
    | { kind: "list"; items: string[] }
    | { kind: "paragraph"; text: string };

  function parseReleaseNotes(notes?: string | null): ReleaseNoteBlock[] {
    const normalized = (notes ?? "").replace(/\r\n/g, "\n").trim();
    if (!normalized) {
      return [{ kind: "paragraph", text: "У этого релиза пока нет опубликованного описания." }];
    }

    return normalized
      .split(/\n\s*\n/)
      .map((block) => block.trim())
      .filter(Boolean)
      .map((block) => {
        const lines = block
          .split("\n")
          .map((line) => line.trim())
          .filter(Boolean);

        if (lines.length === 1 && /^#{1,6}\s+/.test(lines[0])) {
          return {
            kind: "heading" as const,
            text: lines[0].replace(/^#{1,6}\s+/, "").trim(),
          };
        }

        if (lines.length > 0 && lines.every((line) => /^[-*]\s+/.test(line))) {
          return {
            kind: "list" as const,
            items: lines.map((line) => line.replace(/^[-*]\s+/, "").trim()),
          };
        }

        return {
          kind: "paragraph" as const,
          text: lines.join(" "),
        };
      });
  }

  function noteHeadline(notes?: string | null) {
    const [firstBlock] = parseReleaseNotes(notes);
    if (!firstBlock) {
      return "Краткое описание появится после публикации заметок.";
    }

    if (firstBlock.kind === "heading") {
      return firstBlock.text;
    }

    if (firstBlock.kind === "list") {
      return firstBlock.items[0] ? notePreview(firstBlock.items[0], 72) : "Список изменений";
    }

    return notePreview(firstBlock.text, 72);
  }

  function syncSelectedRelease(state: LauncherState) {
    if (selectedTag && state.releases.some((release) => release.tag === selectedTag)) {
      return;
    }

    selectedTag =
      state.releases.find((release) => release.installed)?.tag ??
      state.releases[0]?.tag ??
      null;
  }

  function chooseRelease(tag: string, nextSection: SectionId = activeSection) {
    selectedTag = tag;
    activeSection = nextSection;
  }

  function openReleaseDetails(tag: string, fromSection: PrimarySectionId = "news") {
    selectedTag = tag;
    releaseReturnSection = fromSection;
    activeSection = "release";
  }

  function returnFromReleaseDetails() {
    activeSection = releaseReturnSection;
  }

  async function refreshState(showLoading = true) {
    if (showLoading) {
      loading = true;
    }

    error = "";
    status = "Синхронизация релизов...";

    try {
      const next = await invoke<LauncherState>("load_launcher_state");
      launcher = next;
      syncSelectedRelease(next);

      const localInstalledCount = next.releases.filter((release) => release.installed).length;
      const stableCount = next.releases.filter((release) => !release.isPrerelease).length;
      const prereleaseCount = next.releases.filter((release) => release.isPrerelease).length;
      status =
        prereleaseCount > 0
          ? `${stableCount} stable • ${prereleaseCount} snapshot • установлено ${localInstalledCount}`
          : `${stableCount} macOS-релиза • установлено ${localInstalledCount}`;
    } catch (caught) {
      error = String(caught);
      status = "Не удалось загрузить список релизов CubeOS.";
    } finally {
      loading = false;
    }
  }

  async function installRelease(release: ReleaseSummary) {
    busyTag = release.tag;
    busyAction = "install";
    error = "";
    status = `Устанавливаю ${release.tag} из ${release.macosAssetName}...`;

    try {
      const result = await invoke<InstallResult>("install_release", {
        tag: release.tag,
        assetName: release.macosAssetName,
      });
      await refreshState(false);
      selectedTag = result.tag;
      status = `${result.appName} установлен и готов к запуску.`;
    } catch (caught) {
      error = String(caught);
      status = `Не удалось установить ${release.tag}.`;
    } finally {
      busyTag = null;
      busyAction = null;
    }
  }

  async function launchRelease(release: ReleaseSummary) {
    busyTag = release.tag;
    busyAction = "launch";
    error = "";
    status = `Запускаю ${release.tag}...`;

    try {
      await invoke("launch_release", { tag: release.tag });
      status = `${release.tag} запущена.`;
    } catch (caught) {
      error = String(caught);
      status = `Не удалось запустить ${release.tag}.`;
    } finally {
      busyTag = null;
      busyAction = null;
    }
  }

  async function handlePrimaryAction(release: ReleaseSummary | null) {
    if (!release) {
      return;
    }

    if (release.installed) {
      await launchRelease(release);
      return;
    }

    await installRelease(release);
  }

  async function openVersionsFolder() {
    busyTag = "__folder__";
    busyAction = "folder";
    error = "";
    status = "Открываю папку версий...";

    try {
      await invoke<string>("open_versions_directory");
      status = "Папка версий открыта.";
    } catch (caught) {
      error = String(caught);
      status = "Не удалось открыть папку версий.";
    } finally {
      busyTag = null;
      busyAction = null;
    }
  }

  async function openSharedDataFolder() {
    busyTag = "__data__";
    busyAction = "data";
    error = "";
    status = "Открываю папку данных...";

    try {
      await invoke<string>("open_shared_data_directory");
      status = "Папка данных открыта.";
    } catch (caught) {
      error = String(caught);
      status = "Не удалось открыть папку данных CubeOS.";
    } finally {
      busyTag = null;
      busyAction = null;
    }
  }

  onMount(() => {
    if (typeof localStorage !== "undefined") {
      const storedVisibility = localStorage.getItem(SNAPSHOT_VISIBILITY_KEY);
      if (storedVisibility !== null) {
        showSnapshots = storedVisibility === "1";
      }
    }

    void refreshState();
  });

  $effect(() => {
    if (typeof localStorage === "undefined") {
      return;
    }

    localStorage.setItem(SNAPSHOT_VISIBILITY_KEY, showSnapshots ? "1" : "0");
  });

  $effect(() => {
    if (selectedTag && !visibleReleases.some((release) => release.tag === selectedTag)) {
      selectedTag = installedReleases[0]?.tag ?? visibleReleases[0]?.tag ?? null;

      if (activeSection === "release") {
        activeSection = releaseReturnSection;
      }
    }
  });
</script>

<svelte:head>
  <title>CubeOS Launcher</title>
  <meta
    name="description"
    content="Minecraft-inspired launcher for installing and running CubeOS macOS builds."
  />
</svelte:head>

<div class="launcher-shell">
  <aside class="sidebar">
    <div class="profile-panel panel">
      <div class="cube-mark" aria-hidden="true">
        <span class="cube-face cube-top"></span>
        <span class="cube-face cube-left"></span>
        <span class="cube-face cube-right"></span>
      </div>

      <div class="profile-copy">
        <span class="profile-kicker">CubeOS Launcher</span>
        <strong>macOS release deck</strong>
        <span>
          {installedCount} ready • {visibleReleases.length} shown
        </span>
      </div>
    </div>

    <nav class="sidebar-nav">
      {#each navItems as item}
        <button
          class:selected={activeSection === item.id}
          class="nav-item"
          type="button"
          onclick={() => (activeSection = item.id)}
        >
          <span class={`nav-icon icon-${item.id}`} aria-hidden="true"></span>
          <span class="nav-copy">
            <span class="nav-eyebrow">{item.eyebrow}</span>
            <strong>{item.label}</strong>
            <span>{item.detail}</span>
          </span>
        </button>
      {/each}
    </nav>

    <div class="sidebar-tools panel">
      <button class="tool-button" type="button" onclick={openVersionsFolder} disabled={busyTag !== null}>
        {#if busyAction === "folder"}
          Открываю версии...
        {:else}
          Открыть версии
        {/if}
      </button>
      <button class="tool-button" type="button" onclick={openSharedDataFolder} disabled={busyTag !== null}>
        {#if busyAction === "data"}
          Открываю данные...
        {:else}
          Открыть сейвы и настройки
        {/if}
      </button>
      <button class="tool-button" type="button" onclick={() => refreshState()} disabled={busyTag !== null}>
        Обновить ленту
      </button>

      <label class="snapshot-toggle">
        <input type="checkbox" bind:checked={showSnapshots} />
        <span class="snapshot-toggle-copy">
          <strong>Показывать снапшоты</strong>
          <span>{snapshotSummary()}</span>
        </span>
      </label>
    </div>

    <div class="sidebar-footer panel">
      <span class="meta-label">Каталог версий</span>
      <strong>{launcher?.installRoot ?? "~/Library/Application Support/CubeOS Launcher/versions"}</strong>
      <span>Все сборки ставятся рядом и не перезаписывают друг друга.</span>
    </div>
  </aside>

  <main class="workspace">
    <header class:play-topbar={activeSection === "play"} class="topbar panel">
      {#if activeSection !== "play"}
        <div class="topbar-copy">
          <span class="topbar-kicker">CubeOS // launcher</span>
          <h1>{activeNav.label}</h1>
          <p class="topbar-summary">{activeNav.detail}</p>
        </div>
      {/if}

      <div class="topbar-side">
        <div class="selected-build-mini">
          <span class="meta-label">Текущая сборка</span>
          <strong>{selectedRelease?.tag ?? "Не выбрана"}</strong>
          <span>{releaseChannelDescription(selectedRelease)}</span>
        </div>

        <div class:error={!!error} class:busy={busyTag !== null} class="status-chip">
          {status}
        </div>
      </div>
    </header>

    {#if loading && !launcher}
      <section class="loading-layout">
        <div class="loading-hero panel"></div>
        <div class="loading-row">
          <div class="loading-card panel"></div>
          <div class="loading-card panel"></div>
          <div class="loading-card panel"></div>
        </div>
      </section>
    {:else if !launcher || visibleReleases.length === 0}
      <section class="empty-state panel">
        <div class="empty-copy">
          <span class="section-eyebrow">Нет релизов</span>
          <h2>Лаунчер не нашёл подходящие macOS-сборки CubeOS</h2>
          <p>
            {#if !showSnapshots && snapshotCount > 0}
              Стабильных релизов сейчас нет. Включи галочку <strong>«Показывать снапшоты»</strong>,
              чтобы увидеть GitHub pre-release сборки.
            {:else}
              Нужны релизы с ассетами формата <code>.dmg</code>, <code>.zip</code> или
              <code>.tar.gz</code>.
            {/if}
          </p>
        </div>
        <button class="tool-button" type="button" onclick={() => refreshState()} disabled={busyTag !== null}>
          Проверить снова
        </button>
      </section>
    {:else}
      {#if activeSection === "play"}
        <section class="launch-screen panel">
          <div class="launch-main">
            <div class="launch-head">
              <div class="launch-head-copy">
                <span class="section-eyebrow">Ready to launch</span>
                <h2>{selectedRelease?.tag ? `CubeOS ${selectedRelease.tag}` : "CubeOS"}</h2>
                <p class="launch-subtitle">
                  {selectedRelease?.title ?? "Выбери macOS-сборку CubeOS и запусти её отсюда."}
                </p>
              </div>

              <span class={`state-pill ${selectedRelease?.installed ? "installed" : "available"}`}>
                {selectedRelease ? releaseStateLabel(selectedRelease) : "Не выбрана"}
              </span>
            </div>

            <div class="launch-build-grid">
              <article class="launch-stat-card">
                <span class="meta-label">Текущая сборка</span>
                <strong>{selectedRelease?.tag ?? "Не выбрана"}</strong>
                <span>
                  {selectedRelease
                    ? `${releaseChannelLabel(selectedRelease)} • ${releaseStateHint(selectedRelease)}`
                    : "Открой «Установки», чтобы выбрать версию."}
                </span>
              </article>

              <article class="launch-stat-card">
                <span class="meta-label">Верхняя в списке</span>
                <strong>{latestRelease?.tag ?? "—"}</strong>
                <span>
                  {latestRelease
                    ? `${releaseChannelLabel(latestRelease)} • ${formatDate(latestRelease.publishedAt)}`
                    : "Дата неизвестна"}
                </span>
              </article>

              <article class="launch-stat-card">
                <span class="meta-label">Показано в лаунчере</span>
                <strong>{installedCount}</strong>
                <span>{visibleReleases.length} релизов в текущем канале</span>
              </article>
            </div>

            <div class="launch-action-row">
              <button
                class="play-button play-button-xl"
                type="button"
                onclick={() => handlePrimaryAction(selectedRelease)}
                disabled={!selectedRelease || (busyTag !== null && busyTag !== selectedRelease.tag)}
              >
                <span class="play-button-mark"></span>
                <span class="play-button-copy">
                  {#if selectedRelease && busyTag === selectedRelease.tag && busyAction === "install"}
                    УСТАНОВКА...
                  {:else if selectedRelease && busyTag === selectedRelease.tag && busyAction === "launch"}
                    ЗАПУСК...
                  {:else}
                    {actionLabel(selectedRelease)}
                  {/if}
                </span>
              </button>

              <div class="launch-quick-actions">
                <button
                  class="tool-button launch-secondary-primary"
                  type="button"
                  onclick={() => (activeSection = "versions")}
                >
                  Сменить версию
                </button>
                {#if selectedRelease}
                  <button
                    class="tool-button launch-secondary-muted"
                    type="button"
                    onclick={() => openReleaseDetails(selectedRelease.tag, "play")}
                  >
                    Подробнее
                  </button>
                {/if}
                <button
                  class="tool-button launch-secondary-muted"
                  type="button"
                  onclick={() => (activeSection = "news")}
                >
                  Новости
                </button>
                <button
                  class="tool-button launch-secondary-muted"
                  type="button"
                  onclick={openSharedDataFolder}
                >
                  Сейвы и настройки
                </button>
              </div>
            </div>

            <div class="launch-footer-row">
              {#if releaseFooterHint(selectedRelease)}
                <p class="launch-hint">{releaseFooterHint(selectedRelease)}</p>
              {/if}
              <div class="launch-footer-links">
                <button class="ghost-link" type="button" onclick={openVersionsFolder}>
                  Открыть папку версий
                </button>
                <button class="ghost-link" type="button" onclick={openSharedDataFolder}>
                  Открыть папку данных
                </button>
              </div>
            </div>
          </div>

          <div class="launch-side">
            <article class="launch-info-card">
              <span class="meta-label">Выбранная сборка</span>
              <strong>{selectedRelease?.title ?? "Сборка не выбрана"}</strong>
              <span>
                {selectedRelease
                  ? `${releaseChannelLabel(selectedRelease)} • ${selectedRelease.macosAssetName}`
                  : "Нет выбранного релиза"}
              </span>
              {#if selectedRelease}
                <button class="ghost-link" type="button" onclick={() => openReleaseDetails(selectedRelease.tag, "play")}>
                  Посмотреть описание
                </button>
              {/if}
            </article>

            <article class="launch-info-card">
              <span class="meta-label">Заметки к сборке</span>
              <strong>{selectedRelease ? noteHeadline(selectedRelease.notes) : "Ничего не выбрано"}</strong>
              <span>
                {selectedRelease
                  ? notePreview(selectedRelease.notes, 150)
                  : "После выбора релиза здесь появится короткое описание сборки."}
              </span>
            </article>

            <article class="launch-info-card">
              <span class="meta-label">Папка данных</span>
              <strong>Общие сейвы</strong>
              <span>Все версии используют одну и ту же папку CubeOS для сейвов и настроек.</span>
              <button class="ghost-link" type="button" onclick={openSharedDataFolder}>
                Открыть папку
              </button>
            </article>
          </div>
        </section>
      {/if}

      {#if activeSection === "versions"}
        <section class="section-frame">
          <div class="section-header">
            <div>
              <span class="section-eyebrow">Управление версиями</span>
              <h2>Локальные сборки и доступные релизы</h2>
            </div>
            <p>
              Выбери релиз, чтобы установить его рядом с остальными или сразу запустить уже
              скачанную версию.
            </p>
          </div>

          <div class="versions-layout">
            <div class="release-list panel">
              {#each visibleReleases as release}
                <button
                  class:selected={selectedRelease?.tag === release.tag}
                  class="release-row"
                  type="button"
                  onclick={() => chooseRelease(release.tag, "versions")}
                >
                  <span class="release-row-tag-stack">
                    <span class={`release-row-tag ${release.isPrerelease ? "snapshot-tag" : ""}`}>{release.tag}</span>
                    {#if release.isPrerelease}
                      <span class="channel-tag snapshot-tag">snapshot</span>
                    {/if}
                  </span>
                  <span class="release-row-copy">
                    <strong>{release.title}</strong>
                    <span>
                      {#if release.installed}
                        {releaseChannelLabel(release)} • Готова локально
                      {:else}
                        {releaseChannelLabel(release)} • {release.macosAssetName}
                      {/if}
                    </span>
                  </span>
                </button>
              {/each}
            </div>

            <div class="release-detail panel">
              {#if selectedRelease}
                <div class="detail-top">
                  <div>
                    <span class="section-eyebrow">Выбранный релиз</span>
                    <h3>{selectedRelease.title}</h3>
                  </div>
                  <span class={`state-pill ${selectedRelease.installed ? "installed" : "available"}`}>
                    {selectedRelease.installed ? "Installed" : "Available"}
                  </span>
                </div>

                <div class="detail-stats">
                  <article>
                    <span class="meta-label">Tag</span>
                    <strong>{selectedRelease.tag}</strong>
                  </article>
                  <article>
                    <span class="meta-label">Format</span>
                    <strong>{selectedRelease.assetFormat}</strong>
                  </article>
                  <article>
                    <span class="meta-label">Published</span>
                    <strong>{formatDate(selectedRelease.publishedAt)}</strong>
                  </article>
                  <article>
                    <span class="meta-label">Channel</span>
                    <strong>{releaseChannelLabel(selectedRelease)}</strong>
                  </article>
                </div>

                <p class="detail-notes">{notePreview(selectedRelease.notes, 460)}</p>

                {#if selectedRelease.installedAppPath}
                  <code>{selectedRelease.installedAppPath}</code>
                {/if}

                <div class="detail-actions">
                  <button
                    class="play-button detail-launch"
                    type="button"
                    onclick={() => handlePrimaryAction(selectedRelease)}
                    disabled={busyTag !== null && busyTag !== selectedRelease.tag}
                  >
                    <span class="play-button-mark"></span>
                    <span class="play-button-copy">
                      {#if busyTag === selectedRelease.tag && busyAction === "install"}
                        УСТАНОВКА...
                      {:else if busyTag === selectedRelease.tag && busyAction === "launch"}
                        ЗАПУСК...
                      {:else if selectedRelease.installed}
                        ИГРАТЬ
                      {:else}
                        УСТАНОВИТЬ
                      {/if}
                    </span>
                  </button>

                  {#if selectedRelease.installed}
                    <button
                      class="tool-button reinstall-action"
                      type="button"
                      onclick={() => installRelease(selectedRelease)}
                      disabled={busyTag !== null && busyTag !== selectedRelease.tag}
                    >
                      Переустановить
                    </button>
                  {/if}

                  <button
                    class="tool-button utility-action"
                    type="button"
                    onclick={() => openReleaseDetails(selectedRelease.tag, "versions")}
                  >
                    Подробнее
                  </button>

                  <a
                    class="tool-button link-button utility-action"
                    href={selectedRelease.sourceUrl}
                    target="_blank"
                    rel="noreferrer"
                  >
                    Открыть релиз
                  </a>
                </div>
              {/if}
            </div>
          </div>
        </section>
      {/if}

      {#if activeSection === "news"}
        <section class="section-frame">
          <div class="section-header">
            <div>
              <span class="section-eyebrow">Лента релизов</span>
              <h2>{visibleReleaseHeadline()}</h2>
            </div>
            <p>Релизы, changelog и быстрый переход к нужной macOS-сборке.</p>
          </div>

          <div class="news-desk panel">
            {#each visibleReleases as release, index}
              <article class:selected={selectedRelease?.tag === release.tag} class="news-row">
                <div class="news-row-slot">
                  <div class="timeline-index">0{index + 1}</div>
                  <span class={`news-tag ${release.isPrerelease ? "snapshot-tag" : ""}`}>{release.tag}</span>
                  {#if release.isPrerelease}
                    <span class="channel-tag snapshot-tag">snapshot</span>
                  {/if}
                </div>

                <div class="news-row-main">
                  <div class="news-row-head">
                    <h3>{release.title}</h3>
                    <span class="timeline-date">{formatDate(release.publishedAt)}</span>
                  </div>
                  <p>{notePreview(release.notes, 170)}</p>
                </div>

                <div class="news-row-actions">
                  <button class="tool-button compact primary-tool" type="button" onclick={() => chooseRelease(release.tag, "versions")}>
                    Открыть
                  </button>
                  <button class="tool-button compact" type="button" onclick={() => openReleaseDetails(release.tag, "news")}>
                    Подробнее
                  </button>
                  <a class="ghost-link news-external-link" href={release.sourceUrl} target="_blank" rel="noreferrer">
                    GitHub
                  </a>
                </div>
              </article>
            {/each}
          </div>
        </section>
      {/if}

      {#if activeSection === "release"}
        <section class="section-frame">
          <div class="section-header release-header">
            <div>
              <span class="section-eyebrow">Подробности релиза</span>
              <h2>{selectedRelease?.title ?? "Релиз не найден"}</h2>
            </div>
            <button class="tool-button compact" type="button" onclick={returnFromReleaseDetails}>
              Назад
            </button>
          </div>

          {#if selectedRelease}
            <section class="release-screen panel">
              <div class="release-screen-top">
                <div class="release-screen-copy">
                  <span class="spotlight-tag">{selectedRelease.tag}</span>
                  <h3>{selectedRelease.title}</h3>
                  <p>
                    Полное описание сборки, формат поставки и действия для установки или запуска
                    прямо из лаунчера.
                  </p>
                </div>

                <span class={`state-pill ${selectedRelease.installed ? "installed" : "available"}`}>
                  {selectedRelease.installed ? "Installed" : "Available"}
                </span>
              </div>

              <div class="release-screen-stats">
                <article>
                  <span class="meta-label">Tag</span>
                  <strong>{selectedRelease.tag}</strong>
                </article>
                <article>
                  <span class="meta-label">Format</span>
                  <strong>{selectedRelease.assetFormat}</strong>
                </article>
                <article>
                  <span class="meta-label">Published</span>
                  <strong>{formatDate(selectedRelease.publishedAt)}</strong>
                </article>
                <article>
                  <span class="meta-label">Asset</span>
                  <strong>{selectedRelease.macosAssetName}</strong>
                </article>
                <article>
                  <span class="meta-label">Channel</span>
                  <strong>{releaseChannelLabel(selectedRelease)}</strong>
                </article>
              </div>

              <div class="release-notes-full">
                {#each parseReleaseNotes(selectedRelease.notes) as block}
                  {#if block.kind === "heading"}
                    <h4>{block.text}</h4>
                  {:else if block.kind === "list"}
                    <ul>
                      {#each block.items as item}
                        <li>{item}</li>
                      {/each}
                    </ul>
                  {:else}
                    <p>{block.text}</p>
                  {/if}
                {/each}
              </div>

              {#if selectedRelease.installedAppPath}
                <code>{selectedRelease.installedAppPath}</code>
              {/if}

              <div class="detail-actions release-screen-actions">
                <button
                  class="play-button detail-launch"
                  type="button"
                  onclick={() => handlePrimaryAction(selectedRelease)}
                  disabled={busyTag !== null && busyTag !== selectedRelease.tag}
                >
                  <span class="play-button-mark"></span>
                  <span class="play-button-copy">
                    {#if busyTag === selectedRelease.tag && busyAction === "install"}
                      УСТАНОВКА...
                    {:else if busyTag === selectedRelease.tag && busyAction === "launch"}
                      ЗАПУСК...
                    {:else if selectedRelease.installed}
                      ИГРАТЬ
                    {:else}
                      УСТАНОВИТЬ
                    {/if}
                  </span>
                </button>

                {#if selectedRelease.installed}
                  <button
                    class="tool-button reinstall-action"
                    type="button"
                    onclick={() => installRelease(selectedRelease)}
                    disabled={busyTag !== null && busyTag !== selectedRelease.tag}
                  >
                    Переустановить
                  </button>
                {/if}

                <a
                  class="tool-button link-button utility-action"
                  href={selectedRelease.sourceUrl}
                  target="_blank"
                  rel="noreferrer"
                >
                  Открыть релиз
                </a>
              </div>
            </section>
          {/if}
        </section>
      {/if}

      {#if activeSection === "storage"}
        <section class="section-frame">
          <div class="section-header">
            <div>
              <span class="section-eyebrow">Папки и данные</span>
              <h2>Где живут версии, сейвы и настройки</h2>
            </div>
            <p>
              Сборки ставятся отдельно, а сами сохранения у всех версий общие. Это упрощает
              переключение между релизами.
            </p>
          </div>

          <div class="storage-grid">
            <article class="storage-card panel">
              <span class="meta-label">Установленные версии</span>
              <strong>{launcher.installRoot}</strong>
              <p>Здесь лаунчер держит каждую сборку CubeOS в собственной папке.</p>
              <button class="tool-button" type="button" onclick={openVersionsFolder} disabled={busyTag !== null}>
                Открыть папку версий
              </button>
            </article>

            <article class="storage-card panel">
              <span class="meta-label">Общие сейвы и настройки</span>
              <strong>{launcher.sharedDataRoot}</strong>
              <p>Один каталог для `saves` и `settings`, чтобы версии не конфликтовали между собой.</p>
              <button class="tool-button" type="button" onclick={openSharedDataFolder} disabled={busyTag !== null}>
                Открыть папку данных
              </button>
            </article>

            <article class="storage-card panel">
              <span class="meta-label">Сейчас выбрано</span>
              <strong>{selectedRelease?.tag ?? "Ничего не выбрано"}</strong>
              <p>
                {#if selectedRelease?.installedAppPath}
                  Приложение уже установлено и лежит по пути ниже.
                {:else}
                  Выбранный релиз ещё не установлен локально.
                {/if}
              </p>
              {#if selectedRelease?.installedAppPath}
                <code>{selectedRelease.installedAppPath}</code>
              {/if}
            </article>

            <article class="storage-card storage-card-wide panel">
              <span class="meta-label">Поведение лаунчера</span>
              <strong>Side-by-side installs</strong>
              <p>
                Новый релиз не перезаписывает другие сборки. Можно держать несколько версий CubeOS
                рядом и переключаться между ними из вкладки установок.
              </p>
            </article>
          </div>
        </section>
      {/if}

      {#if error}
        <section class="error-strip panel">
          <strong>Ошибка лаунчера</strong>
          <p>{error}</p>
        </section>
      {/if}
    {/if}
  </main>
</div>

<style>
  :global(:root) {
    --bg-top: #26221c;
    --bg-bottom: #171411;
    --panel-main: #2f2a26;
    --panel-elevated: #38312c;
    --panel-soft: #403833;
    --border-strong: #6d6257;
    --border-soft: #51473f;
    --text-main: #f0ece3;
    --text-soft: #c8c0b2;
    --accent-green: #64c13b;
    --accent-green-dark: #3e8f26;
    --accent-gold: #e0b25a;
    --accent-red: #a7483f;
    --shadow-hard: 0 0 0 2px rgba(0, 0, 0, 0.45), 0 18px 40px rgba(0, 0, 0, 0.28);
  }

  :global(html, body) {
    margin: 0;
    height: 100%;
    min-height: 100%;
    width: 100%;
    max-width: 100%;
    overflow: hidden;
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.03), transparent 18%),
      linear-gradient(180deg, var(--bg-top) 0%, var(--bg-bottom) 100%);
    color: var(--text-main);
    font-family: "Trebuchet MS", "Arial Narrow", sans-serif;
    text-rendering: optimizeLegibility;
    -webkit-font-smoothing: antialiased;
    -moz-osx-font-smoothing: grayscale;
  }

  :global(body) {
    height: 100vh;
    min-height: 100vh;
    max-width: 100vw;
    overflow: hidden;
  }

  :global(body > div) {
    height: 100%;
    max-width: 100%;
    overflow: hidden;
  }

  :global(*) {
    box-sizing: border-box;
  }

  .launcher-shell {
    position: relative;
    height: 100vh;
    min-height: 100vh;
    width: 100%;
    max-width: 100%;
    display: grid;
    grid-template-columns: 286px minmax(0, 1fr);
    overflow: hidden;
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.03), transparent 22%),
      linear-gradient(180deg, #24201a 0%, #161311 100%);
  }

  .panel {
    border: 1px solid var(--border-strong);
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.02), transparent 32%),
      linear-gradient(180deg, var(--panel-main) 0%, #26211d 100%);
    box-shadow: var(--shadow-hard);
  }

  .sidebar {
    position: fixed;
    inset: 0 auto 0 0;
    z-index: 10;
    width: 286px;
    height: 100vh;
    overflow-x: hidden;
    overflow-y: auto;
    min-width: 0;
    padding: 10px;
    display: flex;
    flex-direction: column;
    gap: 10px;
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.02), transparent 24%),
      linear-gradient(180deg, #332e2a 0%, #26211d 100%);
    border-right: 2px solid rgba(0, 0, 0, 0.35);
  }

  .profile-panel {
    min-height: 112px;
    padding: 16px;
    display: grid;
    grid-template-columns: 56px 1fr;
    gap: 14px;
    align-items: center;
  }

  .cube-mark {
    position: relative;
    width: 48px;
    height: 48px;
  }

  .cube-face {
    position: absolute;
    display: block;
    image-rendering: pixelated;
  }

  .cube-top {
    top: 0;
    left: 10px;
    width: 28px;
    height: 16px;
    background: linear-gradient(180deg, #8fe978, #5eb146);
    transform: skew(-35deg);
    box-shadow: inset 0 0 0 2px rgba(0, 0, 0, 0.25);
  }

  .cube-left {
    top: 12px;
    left: 0;
    width: 24px;
    height: 28px;
    background: linear-gradient(180deg, #8e6034, #5b3e23);
    transform: skewY(35deg);
    box-shadow: inset 0 0 0 2px rgba(0, 0, 0, 0.25);
  }

  .cube-right {
    top: 12px;
    right: 2px;
    width: 24px;
    height: 28px;
    background: linear-gradient(180deg, #73b35a, #468334);
    transform: skewY(-35deg);
    box-shadow: inset 0 0 0 2px rgba(0, 0, 0, 0.25);
  }

  .profile-copy {
    display: grid;
    gap: 4px;
  }

  .profile-kicker,
  .nav-eyebrow,
  .topbar-kicker,
  .section-eyebrow,
  .build-kicker,
  .meta-label {
    color: var(--text-soft);
    text-transform: uppercase;
    letter-spacing: 0.12em;
    font-size: 0.68rem;
    font-weight: 700;
  }

  .profile-copy strong {
    font-size: 1.15rem;
  }

  .profile-copy span:last-child {
    color: var(--text-soft);
    font-size: 0.9rem;
  }

  .sidebar-nav {
    display: grid;
    gap: 6px;
  }

  .nav-item {
    width: 100%;
    padding: 15px 14px;
    display: grid;
    grid-template-columns: 40px 1fr;
    gap: 12px;
    align-items: center;
    border: 1px solid #5d5147;
    background: linear-gradient(180deg, #3a332e 0%, #2f2925 100%);
    color: inherit;
    cursor: pointer;
    text-align: left;
    transition: transform 0.15s ease, border-color 0.15s ease, background 0.15s ease;
  }

  .nav-item:hover,
  .top-tab:hover,
  .tool-button:hover,
  .release-row:hover,
  .ghost-link:hover,
  .build-picker:hover,
  .link-button:hover {
    transform: translateY(-1px);
  }

  .nav-item.selected {
    border-color: #99d26d;
    background:
      linear-gradient(90deg, rgba(129, 209, 78, 0.2), transparent 24%),
      linear-gradient(180deg, #413a34 0%, #2c2723 100%);
  }

  .nav-copy {
    display: grid;
    gap: 4px;
  }

  .nav-copy strong {
    font-size: 1.02rem;
  }

  .nav-copy span:last-child {
    color: var(--text-soft);
    font-size: 0.86rem;
  }

  .nav-icon {
    width: 26px;
    height: 26px;
    border: 2px solid rgba(0, 0, 0, 0.3);
    background: linear-gradient(180deg, #6d635b, #413932);
    position: relative;
  }

  .icon-play::before,
  .icon-versions::before,
  .icon-news::before,
  .icon-storage::before {
    content: "";
    position: absolute;
  }

  .icon-play::before {
    top: 5px;
    left: 8px;
    border-top: 7px solid transparent;
    border-bottom: 7px solid transparent;
    border-left: 11px solid #9ee06a;
  }

  .icon-versions::before {
    inset: 5px;
    background:
      linear-gradient(180deg, transparent 0 6px, #dfd4c1 6px 8px, transparent 8px 12px, #dfd4c1 12px 14px, transparent 14px 100%);
  }

  .icon-news::before {
    inset: 4px;
    background:
      linear-gradient(90deg, #e1d6c1 0 4px, transparent 4px 100%),
      linear-gradient(180deg, transparent 0 4px, #e1d6c1 4px 6px, transparent 6px 10px, #e1d6c1 10px 12px, transparent 12px 100%);
  }

  .icon-storage::before {
    top: 5px;
    left: 5px;
    width: 12px;
    height: 12px;
    background: #e1d6c1;
    box-shadow: 8px 8px 0 #8fd861;
  }

  .sidebar-tools {
    padding: 12px;
    display: grid;
    gap: 8px;
  }

  .snapshot-toggle {
    padding: 10px 12px;
    display: grid;
    grid-template-columns: 16px 1fr;
    gap: 10px;
    align-items: start;
    border: 1px solid rgba(255, 255, 255, 0.08);
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.02), transparent 28%),
      linear-gradient(180deg, rgba(47, 41, 37, 0.92), rgba(36, 31, 27, 0.98));
    cursor: pointer;
  }

  .snapshot-toggle input {
    margin: 2px 0 0;
    width: 16px;
    height: 16px;
    accent-color: #78c75a;
  }

  .snapshot-toggle-copy {
    display: grid;
    gap: 3px;
    min-width: 0;
  }

  .snapshot-toggle-copy strong {
    font-size: 0.95rem;
    color: var(--text-main);
  }

  .snapshot-toggle-copy span {
    color: var(--text-soft);
    font-size: 0.82rem;
    line-height: 1.35;
  }

  .tool-button,
  .ghost-link,
  .top-tab,
  .release-row,
  .build-picker,
  .play-button,
  .link-button {
    appearance: none;
    font: inherit;
    text-decoration: none;
  }

  .tool-button,
  .ghost-link,
  .top-tab,
  .build-picker {
    border: 1px solid #74685b;
    background: linear-gradient(180deg, #4a423b 0%, #312b27 100%);
    color: var(--text-main);
    padding: 12px 14px;
    cursor: pointer;
    transition: transform 0.15s ease, opacity 0.15s ease, background 0.15s ease;
  }

  .tool-button:disabled,
  .play-button:disabled {
    cursor: not-allowed;
    opacity: 0.55;
    transform: none;
  }

  .tool-button.compact,
  .ghost-link {
    padding: 10px 12px;
    font-size: 0.92rem;
  }

  .sidebar-footer {
    margin-top: auto;
    padding: 12px 14px;
    display: grid;
    gap: 8px;
    color: var(--text-soft);
    font-size: 0.84rem;
  }

  .sidebar-footer strong {
    color: #efe5d4;
    overflow-wrap: anywhere;
    font-size: 0.94rem;
    line-height: 1.45;
  }

  .sidebar-footer span:last-child {
    line-height: 1.45;
  }

  .workspace {
    grid-column: 2;
    height: 100vh;
    min-height: 0;
    min-width: 0;
    max-width: 100%;
    padding: 12px;
    display: grid;
    gap: 12px;
    align-content: start;
    overflow-y: auto;
    overflow-x: hidden;
    overscroll-behavior: contain;
    scrollbar-gutter: stable;
  }

  .topbar {
    padding: 14px 18px;
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(280px, 380px);
    gap: 14px;
    align-items: center;
  }

  .topbar-copy,
  .topbar-side {
    min-width: 0;
  }

  .topbar-copy h1 {
    margin: 2px 0 0;
    font-size: 1.72rem;
    line-height: 1;
  }

  .topbar-summary {
    margin: 6px 0 0;
    color: var(--text-soft);
    font-size: 0.94rem;
    line-height: 1.38;
  }

  .topbar-side {
    display: grid;
    gap: 12px;
  }

  .play-topbar {
    grid-template-columns: minmax(0, 1fr);
    justify-items: end;
    padding: 10px 14px;
  }

  .play-topbar .topbar-side {
    width: min(100%, 720px);
    grid-template-columns: minmax(220px, 280px) minmax(220px, 1fr);
    align-items: stretch;
    gap: 10px;
  }

  .selected-build-mini {
    min-width: 0;
    padding: 12px 14px;
    border: 1px solid #716659;
    background: linear-gradient(180deg, rgba(255, 255, 255, 0.05), rgba(0, 0, 0, 0.12));
    display: grid;
    gap: 6px;
  }

  .selected-build-mini strong {
    font-size: 1.16rem;
  }

  .selected-build-mini span:last-child {
    color: var(--text-soft);
    overflow-wrap: anywhere;
  }

  .status-chip {
    min-width: 0;
    padding: 12px 14px;
    border: 1px solid #716659;
    background: linear-gradient(180deg, rgba(98, 185, 58, 0.16), rgba(0, 0, 0, 0.12));
    color: #d7efc7;
    font-size: 0.95rem;
    line-height: 1.45;
    overflow-wrap: anywhere;
  }

  .play-topbar .selected-build-mini,
  .play-topbar .status-chip {
    padding: 10px 12px;
  }

  .play-topbar .selected-build-mini strong {
    font-size: 1.06rem;
  }

  .play-topbar .status-chip {
    display: flex;
    align-items: center;
    min-height: 100%;
    font-size: 0.9rem;
    line-height: 1.35;
  }

  .status-chip.busy {
    background: linear-gradient(180deg, rgba(224, 178, 90, 0.16), rgba(0, 0, 0, 0.12));
    color: #f5deb0;
  }

  .status-chip.error {
    background: linear-gradient(180deg, rgba(167, 72, 63, 0.18), rgba(0, 0, 0, 0.12));
    color: #ffd4ce;
  }

  .loading-layout {
    display: grid;
    gap: 12px;
  }

  .loading-hero,
  .loading-card {
    position: relative;
    overflow: hidden;
    min-height: 160px;
  }

  .loading-hero::before,
  .loading-card::before {
    content: "";
    position: absolute;
    inset: 0;
    background:
      linear-gradient(110deg, transparent 12%, rgba(255, 255, 255, 0.1) 28%, transparent 44%),
      linear-gradient(180deg, #302b26 0%, #25201c 100%);
    background-size: 210% 100%;
    animation: shimmer 1.5s linear infinite;
  }

  .loading-hero {
    min-height: 320px;
  }

  .loading-row {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 12px;
  }

  .empty-state,
  .error-strip {
    padding: 22px;
    display: grid;
    gap: 14px;
  }

  .empty-copy,
  .error-strip {
    color: var(--text-main);
  }

  .empty-copy h2,
  .section-header h2,
  .release-detail h3 {
    margin: 0;
  }

  .empty-copy p,
  .error-strip p,
  .section-header p,
  .detail-notes,
  .storage-card p {
    margin: 0;
    color: var(--text-soft);
    line-height: 1.55;
  }

  .hero-stage,
  .launch-screen {
    position: relative;
    display: grid;
    grid-template-columns: minmax(0, 1fr) 320px;
    overflow: hidden;
    isolation: isolate;
  }

  .hero-visual,
  .launch-visual {
    position: absolute;
    inset: 0;
    background:
      linear-gradient(180deg, rgba(246, 205, 129, 0.18), transparent 26%),
      linear-gradient(180deg, #79aec7 0%, #ebb46d 47%, #668f46 48%, #4d6f32 100%);
  }

  .hero-visual::before,
  .launch-visual::before {
    content: "";
    position: absolute;
    inset: 0;
    background:
      linear-gradient(90deg, rgba(255, 255, 255, 0.06) 1px, transparent 1px),
      linear-gradient(180deg, rgba(255, 255, 255, 0.05) 1px, transparent 1px);
    background-size: 48px 48px;
    opacity: 0.14;
  }

  .launch-visual::after {
    content: "";
    position: absolute;
    inset: 0;
    background:
      linear-gradient(90deg, rgba(18, 16, 14, 0.54) 0%, rgba(18, 16, 14, 0.28) 38%, rgba(18, 16, 14, 0.08) 60%, transparent 78%),
      radial-gradient(circle at 28% 60%, rgba(14, 12, 11, 0.3), transparent 34%),
      linear-gradient(180deg, transparent 54%, rgba(18, 15, 12, 0.22) 100%);
  }

  .sun,
  .cloud,
  .mountain,
  .floating-block,
  .ground-strip {
    position: absolute;
    display: block;
  }

  .sun {
    top: 56px;
    right: 240px;
    width: 102px;
    height: 102px;
    background: radial-gradient(circle, rgba(255, 248, 212, 0.82), rgba(255, 206, 112, 0.56));
    box-shadow: 0 0 0 8px rgba(255, 233, 174, 0.12);
  }

  .cloud {
    background: rgba(255, 249, 233, 0.4);
    box-shadow:
      28px 10px 0 rgba(255, 249, 233, 0.45),
      56px 2px 0 rgba(255, 249, 233, 0.36),
      12px -12px 0 rgba(255, 249, 233, 0.28);
  }

  .cloud-a {
    top: 60px;
    left: 110px;
    width: 66px;
    height: 20px;
  }

  .cloud-b {
    top: 112px;
    left: 340px;
    width: 74px;
    height: 22px;
  }

  .mountain {
    bottom: 118px;
    clip-path: polygon(50% 0, 100% 100%, 0 100%);
    box-shadow: inset 0 -26px 0 rgba(0, 0, 0, 0.1);
    opacity: 0.78;
  }

  .mountain-a {
    left: 30px;
    width: 280px;
    height: 190px;
    background: linear-gradient(180deg, #8f653a 0%, #5d4026 100%);
  }

  .mountain-b {
    left: 250px;
    width: 350px;
    height: 250px;
    background: linear-gradient(180deg, #73a15b 0%, #4b7037 100%);
  }

  .mountain-c {
    right: 200px;
    width: 300px;
    height: 220px;
    background: linear-gradient(180deg, #6f9ecc 0%, #31507d 100%);
  }

  .floating-block {
    width: 48px;
    height: 48px;
    background:
      linear-gradient(180deg, #84d45d 0 26%, #8f6336 26% 100%);
    box-shadow:
      inset 0 0 0 3px rgba(0, 0, 0, 0.16),
      0 8px 14px rgba(0, 0, 0, 0.14);
    opacity: 0.76;
    transform: rotate(8deg);
  }

  .block-a {
    top: 138px;
    left: 190px;
  }

  .block-b {
    top: 96px;
    right: 344px;
    transform: rotate(-6deg);
  }

  .block-c {
    top: 220px;
    right: 112px;
    transform: rotate(11deg);
  }

  .ground-strip {
    left: 0;
    right: 0;
    bottom: 0;
    height: 76px;
    background:
      linear-gradient(180deg, #73b44c 0 20%, #6e4d2d 20% 100%);
    box-shadow: inset 0 8px 0 rgba(255, 255, 255, 0.06);
  }

  .hero-copy,
  .hero-spotlight,
  .launch-main,
  .launch-side {
    position: relative;
    z-index: 1;
  }

  .launch-screen {
    min-height: 0;
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.018), transparent 22%),
      linear-gradient(180deg, #2c2723 0%, #241f1b 100%);
  }

  .launch-main {
    padding: 20px;
    display: grid;
    align-content: start;
    gap: 14px;
    min-width: 0;
    max-width: none;
  }

  .launch-main::before {
    content: "";
    position: absolute;
    inset: 0;
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.025), transparent 18%),
      linear-gradient(180deg, rgba(0, 0, 0, 0.06), rgba(0, 0, 0, 0.12));
    border-right: 1px solid rgba(255, 255, 255, 0.05);
    pointer-events: none;
  }

  .launch-main > * {
    position: relative;
    z-index: 1;
  }

  .launch-head {
    display: flex;
    gap: 14px;
    align-items: start;
    justify-content: space-between;
    flex-wrap: wrap;
  }

  .launch-head-copy {
    min-width: 0;
    display: grid;
    gap: 8px;
  }

  .launch-main h2 {
    margin: 0;
    max-width: 14ch;
    font-size: clamp(1.8rem, 2.5vw, 2.45rem);
    line-height: 1.02;
    letter-spacing: 0.01em;
    text-transform: none;
    text-shadow: none;
  }

  .launch-subtitle,
  .launch-hint {
    margin: 0;
    max-width: 42rem;
    color: var(--text-soft);
    text-shadow: none;
    line-height: 1.48;
  }

  .launch-hint {
    font-size: 0.94rem;
    color: #d7cfbf;
  }

  .launch-build-grid {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: 8px;
  }

  .launch-stat-card {
    min-width: 0;
    padding: 12px 14px;
    display: grid;
    gap: 6px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.02), transparent 28%),
      linear-gradient(180deg, rgba(0, 0, 0, 0.14), rgba(0, 0, 0, 0.08));
  }

  .launch-stat-card strong {
    font-size: 1.25rem;
    overflow-wrap: anywhere;
  }

  .launch-stat-card span:last-child {
    color: var(--text-soft);
    line-height: 1.4;
    overflow-wrap: anywhere;
  }

  .launch-action-row {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 220px;
    gap: 10px;
    align-items: stretch;
  }

  .launch-quick-actions {
    display: grid;
    gap: 8px;
  }

  .launch-quick-actions .tool-button {
    width: 100%;
    padding: 11px 12px;
    border-color: rgba(255, 255, 255, 0.08);
    background:
      linear-gradient(180deg, rgba(58, 51, 46, 0.88), rgba(40, 35, 31, 0.94));
    color: #ece1cf;
    font-size: 0.88rem;
    box-shadow: none;
  }

  .launch-quick-actions .launch-secondary-primary {
    padding: 12px 13px;
    border-color: rgba(153, 210, 109, 0.52);
    background:
      linear-gradient(180deg, rgba(153, 210, 109, 0.2), rgba(153, 210, 109, 0.05)),
      linear-gradient(180deg, rgba(58, 51, 46, 0.94), rgba(37, 32, 29, 0.98));
    color: #f2f8e8;
    box-shadow:
      inset 0 -1px 0 rgba(0, 0, 0, 0.18),
      0 0 0 1px rgba(153, 210, 109, 0.1),
      0 8px 18px rgba(48, 84, 29, 0.08);
    font-weight: 700;
  }

  .launch-quick-actions .launch-secondary-muted {
    border-color: rgba(255, 255, 255, 0.06);
    background:
      linear-gradient(180deg, rgba(49, 43, 39, 0.9), rgba(37, 32, 29, 0.96));
    color: #d9cfbe;
  }

  .launch-side {
    padding: 20px;
    display: grid;
    align-content: start;
    gap: 10px;
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.02), transparent 20%),
      linear-gradient(180deg, #2a241f 0%, #211c18 100%);
    border-left: 1px solid rgba(255, 255, 255, 0.05);
  }

  .launch-info-card {
    min-width: 0;
    padding: 14px 16px;
    display: grid;
    gap: 8px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.025), transparent 24%),
      linear-gradient(180deg, rgba(0, 0, 0, 0.16), rgba(0, 0, 0, 0.08));
  }

  .launch-info-card strong {
    font-size: 1.06rem;
    overflow-wrap: anywhere;
  }

  .launch-info-card span:last-child {
    color: var(--text-soft);
    overflow-wrap: anywhere;
    line-height: 1.45;
  }

  .launch-info-card .ghost-link {
    justify-self: start;
  }

  .launch-footer-row {
    display: flex;
    gap: 16px;
    align-items: center;
    justify-content: space-between;
    flex-wrap: wrap;
    padding-top: 4px;
    border-top: 1px solid rgba(255, 255, 255, 0.05);
  }

  .launch-footer-links {
    display: flex;
    gap: 16px;
    flex-wrap: wrap;
    align-items: center;
    margin-left: auto;
  }

  .chip,
  .spotlight-tag,
  .news-tag,
  .state-pill,
  .release-row-tag,
  .timeline-index,
  .install-badge {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    padding: 8px 12px;
    font-size: 0.78rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.08em;
  }

  .spotlight-tag,
  .news-tag,
  .release-row-tag {
    background: rgba(109, 193, 63, 0.16);
    border: 1px solid rgba(154, 221, 102, 0.38);
    color: #d2f3b8;
  }

  .channel-tag,
  .release-row-tag-stack {
    display: grid;
    gap: 6px;
    justify-items: start;
  }

  .channel-tag {
    padding: 6px 10px;
    font-size: 0.68rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    border: 1px solid rgba(255, 255, 255, 0.12);
    color: #e7dbc2;
    background: rgba(255, 255, 255, 0.04);
  }

  .snapshot-tag {
    background: rgba(224, 178, 90, 0.18);
    border-color: rgba(224, 178, 90, 0.42);
    color: #ffe3ad;
  }

  .detail-stats {
    margin-top: 16px;
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 12px;
  }

  .detail-stats article {
    padding: 12px;
    background: rgba(255, 255, 255, 0.04);
    border: 1px solid rgba(255, 255, 255, 0.08);
    display: grid;
    gap: 6px;
  }

  .play-button {
    width: 100%;
    max-width: 100%;
    min-width: 0;
    border: 1px solid #69c447;
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.11), transparent 38%),
      linear-gradient(180deg, var(--accent-green) 0%, var(--accent-green-dark) 100%);
    color: #f4ffe9;
    display: grid;
    grid-template-columns: 30px 1fr;
    gap: 14px;
    align-items: center;
    padding: 16px 24px;
    cursor: pointer;
    box-shadow:
      inset 0 -4px 0 rgba(0, 0, 0, 0.25),
      0 0 0 2px rgba(0, 0, 0, 0.36);
    transition: transform 0.15s ease, opacity 0.15s ease, filter 0.15s ease;
  }

  .play-button:hover:not(:disabled) {
    transform: translateY(-1px);
    filter: brightness(1.05);
  }

  .play-button-mark {
    width: 38px;
    height: 38px;
    display: grid;
    place-items: center;
    border-radius: 10px;
    background: linear-gradient(180deg, rgba(255, 255, 255, 0.18), rgba(255, 255, 255, 0.05));
    box-shadow:
      inset 0 -2px 0 rgba(0, 0, 0, 0.18),
      0 0 0 1px rgba(255, 255, 255, 0.08);
  }

  .play-button-mark::before {
    content: "";
    margin-left: 3px;
    width: 0;
    height: 0;
    border-top: 9px solid transparent;
    border-bottom: 9px solid transparent;
    border-left: 15px solid #eef7e4;
    filter: drop-shadow(0 1px 0 rgba(0, 0, 0, 0.25));
  }

  .play-button-copy {
    font-size: 1.8rem;
    line-height: 1;
    letter-spacing: 0.08em;
    font-weight: 900;
  }

  .play-button-xl {
    width: 100%;
    min-height: 88px;
    grid-template-columns: 48px 1fr;
    padding: 18px 24px;
    box-shadow:
      inset 0 -4px 0 rgba(0, 0, 0, 0.25),
      0 0 0 2px rgba(0, 0, 0, 0.36),
      0 12px 22px rgba(22, 50, 12, 0.18);
  }

  .play-button-xl .play-button-copy {
    font-size: clamp(1.8rem, 4vw, 2.7rem);
  }

  .dashboard-grid,
  .storage-grid {
    display: grid;
    gap: 12px;
  }

  .dashboard-grid {
    grid-template-columns: repeat(3, minmax(0, 1fr));
  }

  .storage-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
    align-items: start;
  }

  .news-card,
  .storage-card {
    min-width: 0;
    padding: 18px;
    display: grid;
    gap: 14px;
  }

  .news-card-top,
  .detail-top,
  .timeline-head,
  .news-card-footer {
    display: flex;
    gap: 10px;
    align-items: center;
    justify-content: space-between;
    flex-wrap: wrap;
  }

  .inline-actions {
    display: flex;
    gap: 12px;
    flex-wrap: wrap;
    align-items: center;
  }

  .install-badge,
  .state-pill.installed {
    background: rgba(94, 196, 83, 0.18);
    border: 1px solid rgba(94, 196, 83, 0.36);
    color: #caf4ba;
  }

  .state-pill.available {
    background: rgba(224, 178, 90, 0.18);
    border: 1px solid rgba(224, 178, 90, 0.4);
    color: #ffe3ad;
  }

  .ghost-link {
    padding: 0;
    background: transparent;
    border: 0;
    color: #94d96a;
    font-weight: 700;
  }

  .section-frame {
    min-width: 0;
    display: grid;
    gap: 12px;
  }

  .release-header {
    display: flex;
    gap: 12px;
    align-items: center;
    justify-content: space-between;
    flex-wrap: wrap;
  }

  .section-header {
    padding: 8px 2px 2px;
    display: grid;
    gap: 10px;
  }

  .versions-layout {
    display: grid;
    grid-template-columns: 320px minmax(0, 1fr);
    gap: 12px;
  }

  .release-list,
  .release-detail {
    min-width: 0;
    padding: 14px;
    display: grid;
    gap: 10px;
    align-content: start;
  }

  .release-row {
    width: 100%;
    border: 1px solid #6d6257;
    background: linear-gradient(180deg, #433b35 0%, #302a26 100%);
    color: inherit;
    cursor: pointer;
    padding: 12px;
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 12px;
    align-items: center;
    text-align: left;
  }

  .release-row.selected {
    border-color: #9bd869;
    background:
      linear-gradient(90deg, rgba(94, 196, 83, 0.18), transparent 28%),
      linear-gradient(180deg, #443c36 0%, #302923 100%);
  }

  .release-row-copy {
    display: grid;
    gap: 4px;
  }

  .release-row-copy span {
    color: var(--text-soft);
    font-size: 0.88rem;
  }

  .detail-notes {
    margin-top: 4px;
  }

  .release-screen {
    padding: 22px;
    display: grid;
    gap: 18px;
  }

  .release-screen-top {
    display: flex;
    gap: 16px;
    align-items: start;
    justify-content: space-between;
    flex-wrap: wrap;
  }

  .release-screen-copy {
    min-width: 0;
    display: grid;
    gap: 10px;
  }

  .release-screen-copy h3 {
    margin: 0;
    font-size: clamp(2rem, 3vw, 3rem);
    line-height: 1.04;
  }

  .release-screen-copy p {
    margin: 0;
    color: var(--text-soft);
    line-height: 1.55;
  }

  .release-screen-stats {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 12px;
  }

  .release-screen-stats article {
    min-width: 0;
    padding: 16px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    background: rgba(255, 255, 255, 0.04);
    display: grid;
    gap: 6px;
  }

  .release-screen-stats strong {
    overflow-wrap: anywhere;
  }

  .release-notes-full {
    min-width: 0;
    padding: 18px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    background: rgba(0, 0, 0, 0.14);
    display: grid;
    gap: 14px;
  }

  .release-notes-full h4 {
    margin: 0;
    font-size: 1.3rem;
    line-height: 1.2;
  }

  .release-notes-full p,
  .release-notes-full li {
    margin: 0;
    color: var(--text-main);
    line-height: 1.65;
    overflow-wrap: anywhere;
  }

  .release-notes-full ul {
    margin: 0;
    padding-left: 20px;
    display: grid;
    gap: 8px;
  }

  .release-screen-actions {
    padding-top: 4px;
  }

  .detail-actions {
    display: flex;
    gap: 12px;
    flex-wrap: wrap;
    align-items: center;
  }

  .detail-actions > * {
    max-width: 100%;
  }

  .detail-launch {
    min-width: 0;
    width: min(100%, 320px);
  }

  .detail-actions .reinstall-action {
    border-color: rgba(224, 178, 90, 0.24);
    background:
      linear-gradient(180deg, rgba(94, 83, 64, 0.82), rgba(48, 42, 38, 0.95));
    color: #eadcc2;
  }

  .detail-actions .utility-action {
    border-color: rgba(255, 255, 255, 0.06);
    background:
      linear-gradient(180deg, rgba(46, 40, 36, 0.84), rgba(34, 30, 27, 0.96));
    color: #d4cab9;
  }

  .news-timeline {
    display: grid;
    gap: 12px;
  }

  .news-desk {
    overflow: hidden;
    display: grid;
  }

  .news-row {
    min-width: 0;
    padding: 18px 20px;
    display: grid;
    grid-template-columns: 118px minmax(0, 1fr) auto;
    gap: 18px;
    align-items: center;
    border-bottom: 1px solid rgba(255, 255, 255, 0.06);
    background: transparent;
  }

  .news-row:last-child {
    border-bottom: 0;
  }

  .news-row.selected {
    background:
      linear-gradient(90deg, rgba(94, 196, 83, 0.14), transparent 24%),
      linear-gradient(180deg, rgba(255, 255, 255, 0.02), transparent 40%);
  }

  .news-row-slot {
    display: grid;
    gap: 8px;
    align-content: start;
    justify-items: start;
  }

  .news-row-main {
    min-width: 0;
    display: grid;
    gap: 8px;
  }

  .news-row-head {
    display: flex;
    gap: 10px;
    align-items: center;
    justify-content: space-between;
    flex-wrap: wrap;
  }

  .news-row-head h3 {
    margin: 0;
    font-size: 1.16rem;
    line-height: 1.18;
  }

  .news-row-main p {
    margin: 0;
    color: var(--text-soft);
    line-height: 1.56;
  }

  .news-row-actions {
    display: grid;
    gap: 8px;
    justify-items: stretch;
    min-width: 132px;
  }

  .storage-card-wide {
    grid-column: 1 / -1;
  }

  .tool-button.primary-tool {
    border-color: #78c75a;
    background:
      linear-gradient(180deg, rgba(255, 255, 255, 0.08), transparent 36%),
      linear-gradient(180deg, rgba(88, 179, 58, 0.94), rgba(58, 127, 39, 0.98));
    color: #f4ffe8;
  }

  .news-external-link {
    justify-self: start;
    font-size: 0.92rem;
  }

  .timeline-card {
    padding: 18px;
    display: grid;
    grid-template-columns: 84px minmax(0, 1fr) auto;
    gap: 16px;
    align-items: start;
  }

  .timeline-index {
    background: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.09);
    font-size: 1rem;
    color: #f2e7d2;
  }

  .timeline-copy {
    display: grid;
    gap: 12px;
  }

  .timeline-date {
    color: var(--text-soft);
    font-size: 0.88rem;
  }

  .timeline-actions {
    display: grid;
    gap: 10px;
  }

  .timeline-copy,
  .timeline-actions,
  .release-row-copy strong {
    min-width: 0;
    overflow-wrap: anywhere;
  }

  .storage-card strong,
  .detail-stats strong {
    overflow-wrap: anywhere;
  }

  code {
    display: block;
    padding: 12px 14px;
    background: rgba(0, 0, 0, 0.22);
    border: 1px solid rgba(255, 255, 255, 0.08);
    color: #f7dca5;
    font-family: "Menlo", "Monaco", monospace;
    font-size: 0.82rem;
    overflow-wrap: anywhere;
  }

  @keyframes shimmer {
    from {
      background-position: 200% 0, 0 0;
    }

    to {
      background-position: -40% 0, 0 0;
    }
  }

  @media (max-width: 1320px) {
    .launcher-shell {
      grid-template-columns: 260px minmax(0, 1fr);
    }

    .sidebar {
      width: 260px;
    }

    .hero-stage,
    .launch-screen {
      grid-template-columns: minmax(0, 1fr);
    }

    .launch-build-grid,
    .launch-action-row {
      grid-template-columns: 1fr;
    }

    .launch-side {
      border-left: 0;
      border-top: 1px solid rgba(255, 255, 255, 0.05);
    }

    .hero-spotlight {
      margin: 0 20px 20px;
    }

    .play-bar {
      grid-template-columns: 1fr;
    }

    .dashboard-grid,
    .loading-row {
      grid-template-columns: 1fr;
    }

  }

  @media (max-width: 1080px) {
    :global(html, body) {
      overflow-y: auto;
    }

    :global(body) {
      height: auto;
      overflow: auto;
    }

    .launcher-shell {
      height: auto;
      grid-template-columns: 1fr;
      overflow: visible;
    }

    .sidebar {
      position: static;
      inset: auto;
      z-index: auto;
      width: auto;
      height: auto;
      overflow: visible;
      border-right: 0;
      border-bottom: 2px solid rgba(0, 0, 0, 0.35);
    }

    .workspace {
      grid-column: auto;
      height: auto;
      overflow: visible;
    }

    .topbar {
      grid-template-columns: 1fr;
    }

    .play-topbar .topbar-side {
      width: 100%;
      grid-template-columns: 1fr;
    }

    .launch-head,
    .launch-footer-row {
      flex-direction: column;
      align-items: stretch;
    }

    .news-row {
      grid-template-columns: 1fr;
      align-items: start;
    }

    .news-row-slot {
      grid-auto-flow: column;
      justify-content: start;
      align-items: center;
    }

    .news-row-actions {
      grid-template-columns: repeat(3, minmax(0, max-content));
      justify-items: start;
      min-width: 0;
    }

    .versions-layout,
    .timeline-card {
      grid-template-columns: 1fr;
    }

    .release-screen-stats {
      grid-template-columns: 1fr;
    }
  }

  @media (max-width: 720px) {
    .workspace,
    .sidebar {
      padding: 10px;
    }

    .profile-panel,
    .hero-copy,
    .hero-spotlight,
    .launch-main,
    .launch-side,
    .play-bar,
    .news-card,
    .release-list,
    .release-detail,
    .timeline-card,
    .storage-card,
    .topbar {
      padding: 14px;
    }

    .topbar-copy h1 {
      font-size: 1.6rem;
    }

    .launch-main h2 {
      font-size: 2.2rem;
    }

    .news-row-actions {
      grid-template-columns: 1fr;
    }

    .storage-grid {
      grid-template-columns: 1fr;
    }

    .play-button-copy {
      font-size: 1.35rem;
    }
  }
</style>
