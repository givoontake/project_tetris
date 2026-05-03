const maxUsers = 30000;
const hostStorageKeys = {
  server: "tetris.performance.server_host",
  stress: "tetris.performance.stress_host"
};

function elem(id) {
  return document.getElementById(id);
}

function setChip(el, ok, label) {
  el.textContent = label;
  el.classList.toggle("ok", ok === true);
  el.classList.toggle("bad", ok === false);
}

function fmt(n) {
  if (n === null || n === undefined) {
    return "?";
  }
  return Number(n).toLocaleString();
}

function mb(bytes) {
  return (Number(bytes || 0) / 1024 / 1024).toFixed(1);
}

function meter(id, value, max) {
  const width = Math.min(100, Math.max(0, (Number(value || 0) / max) * 100));
  elem(id).style.width = `${width}%`;
}

function metricValue(value, fresh) {
  return fresh ? fmt(value) : "?";
}

function updateServerMetrics(server, fresh) {
  elem("serverCurrentUsers").textContent = metricValue(server.current_connected_users, fresh);
  elem("serverTotalUsers").textContent = metricValue(server.total_connected_users, fresh);
  elem("serverPending").textContent = metricValue(server.current_pending_count, fresh);
  elem("serverRooms").textContent = metricValue(server.active_room_count, fresh);
  elem("serverPendingDone").textContent = metricValue(server.completed_pending_total, fresh);
  elem("serverMemory").textContent = fresh ? mb(server.server_memory_bytes) : "?";
  elem("serverThreads").textContent = metricValue(server.created_thread_count, fresh);
  elem("processorCount").textContent = metricValue(server.logical_processor_count, fresh);
  updateProcessorBars(server.logical_processor_usage || [], fresh);
  updateTickWorkerBars(server.tick_worker_ticks_per_second || [], fresh);
  meter("serverUsersMeter", fresh ? server.current_connected_users : 0, maxUsers);
}

function updateProcessorBars(usages, fresh) {
  const box = elem("processorBars");
  box.innerHTML = "";
  if (!fresh || usages.length === 0) {
    const empty = document.createElement("div");
    empty.className = "processor-empty";
    empty.textContent = "?";
    box.appendChild(empty);
    return;
  }

  usages.forEach((usage, index) => {
    const item = document.createElement("div");
    item.className = "processor-item";

    const label = document.createElement("span");
    label.textContent = `P${index}`;

    const value = document.createElement("strong");
    value.textContent = `${fmt(usage)}%`;

    const meter = document.createElement("i");
    meter.style.width = `${Math.min(100, Math.max(0, Number(usage || 0)))}%`;

    item.appendChild(label);
    item.appendChild(value);
    item.appendChild(meter);
    box.appendChild(item);
  });
}

function updateTickWorkerBars(ticks, fresh) {
  const box = elem("tickWorkerBars");
  box.innerHTML = "";
  if (!fresh || ticks.length === 0) {
    const empty = document.createElement("div");
    empty.className = "processor-empty";
    empty.textContent = "?";
    box.appendChild(empty);
    return;
  }

  ticks.forEach((tick, index) => {
    const item = document.createElement("div");
    item.className = "processor-item";

    const label = document.createElement("span");
    label.textContent = `T${index}`;

    const value = document.createElement("strong");
    value.textContent = fmt(tick);

    const meter = document.createElement("i");
    meter.style.width = `${Math.min(100, Math.max(0, (Number(tick || 0) / 50) * 100))}%`;

    item.appendChild(label);
    item.appendChild(value);
    item.appendChild(meter);
    box.appendChild(item);
  });
}

function updateStressMetrics(stress, fresh) {
  const connectEnabled = Number(stress.connect_enabled || 0) !== 0;
  elem("stressConnectStatus").textContent = fresh ? (connectEnabled ? "ON" : "STOP") : "?";
  elem("stressConnected").textContent = metricValue(stress.connected_client, fresh);
  elem("stressPlaying").textContent = metricValue(stress.playing_client, fresh);
  elem("stressCurrentRtt").textContent = metricValue(stress.current_latency_ms, fresh);
  elem("stressAvgRtt").textContent = metricValue(stress.average_latency_ms, fresh);
  elem("stressMaxRtt").textContent = metricValue(stress.max_latency_ms, fresh);
  elem("stressCurrentLoginRtt").textContent = metricValue(stress.current_login_latency_ms, fresh);
  elem("stressAvgLoginRtt").textContent = metricValue(stress.average_login_latency_ms, fresh);
  elem("stressMaxLoginRtt").textContent = metricValue(stress.max_login_latency_ms, fresh);
  meter("stressConnectedMeter", fresh ? stress.connected_client : 0, maxUsers);
  meter("stressPlayingMeter", fresh ? stress.playing_client : 0, maxUsers);
}

async function controlStressConnect(enabled) {
  const pauseBtn = elem("pauseConnectBtn");
  const resumeBtn = elem("resumeConnectBtn");
  pauseBtn.disabled = true;
  resumeBtn.disabled = true;
  setChip(elem("messageChip"), null, enabled ? "stress connect resume..." : "stress connect pause...");

  try {
    const res = await fetch("/api/stress/connect-control", {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify({enabled})
    });
    const data = await res.json();
    if (!data.ok) {
      throw new Error(data.error || "connect control failed");
    }
    setChip(elem("messageChip"), true, enabled ? "stress connect resumed" : "stress connect paused");
  } catch (err) {
    setChip(elem("messageChip"), false, err.message);
  } finally {
    pauseBtn.disabled = false;
    resumeBtn.disabled = false;
  }
}

function updateLog(data) {
  const server = data.server || {};
  const stress = data.stress || {};
  const serverUsers = data.server_fresh ? server.current_connected_users || 0 : "?";
  const serverPending = data.server_fresh ? server.current_pending_count || 0 : "?";
  const stressClients = data.stress_fresh ? stress.connected_client || 0 : "?";
  const stressPlaying = data.stress_fresh ? stress.playing_client || 0 : "?";
  const stressRtt = data.stress_fresh ? stress.current_latency_ms || 0 : "?";
  const stressLoginRtt = data.stress_fresh ? stress.current_login_latency_ms || 0 : "?";
  const stressConnect = data.stress_fresh ? (Number(stress.connect_enabled || 0) !== 0 ? "on" : "off") : "?";
  elem("logBox").textContent = [
    `server connected=${data.server_connected} users=${serverUsers} pending=${serverPending}`,
    `stress connected=${data.stress_connected} clients=${stressClients} playing=${stressPlaying} rtt=${stressRtt}ms login=${stressLoginRtt}ms connect=${stressConnect}`
  ].join("\n");
}

function loadSavedHosts() {
  elem("serverHost").value = localStorage.getItem(hostStorageKeys.server) || "";
  elem("stressHost").value = localStorage.getItem(hostStorageKeys.stress) || "";
}

async function connectTarget(target) {
  const isServer = target === "server";
  const connectBtn = elem(isServer ? "connectServerBtn" : "connectStressBtn");
  const hostId = isServer ? "serverHost" : "stressHost";
  const bodyKey = isServer ? "server_host" : "stress_host";
  const hostValue = elem(hostId).value.trim();
  connectBtn.disabled = true;
  setChip(elem("messageChip"), null, `${target} connecting...`);

  try {
    const res = await fetch(`/api/connect/${target}`, {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify({[bodyKey]: hostValue})
    });
    const data = await res.json();
    if (!data.ok) {
      throw new Error(data.error || "connect failed");
    }

    localStorage.setItem(hostStorageKeys[target], hostValue);
    setChip(elem("messageChip"), true, `${target} connected`);
  } catch (err) {
    setChip(elem("messageChip"), false, err.message);
  } finally {
    connectBtn.disabled = false;
  }
}

function connectEvents() {
  const events = new EventSource("/api/events");
  events.onmessage = (event) => {
    const data = JSON.parse(event.data);
    const server = data.server || {};
    const stress = data.stress || {};

    setChip(elem("serverChip"), data.server_connected, `server: ${data.server_connected ? "connected" : "lost"}`);
    setChip(elem("stressChip"), data.stress_connected, `stress: ${data.stress_connected ? "connected" : "lost"}`);
    elem("dashboard").classList.add("open");
    updateServerMetrics(server, data.server_fresh === true);
    updateStressMetrics(stress, data.stress_fresh === true);
    updateLog(data);
  };
}

loadSavedHosts();
elem("connectServerBtn").addEventListener("click", () => connectTarget("server"));
elem("connectStressBtn").addEventListener("click", () => connectTarget("stress"));
elem("pauseConnectBtn").addEventListener("click", () => controlStressConnect(false));
elem("resumeConnectBtn").addEventListener("click", () => controlStressConnect(true));
connectEvents();
