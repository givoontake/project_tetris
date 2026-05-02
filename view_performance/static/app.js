const maxUsers = 10000;
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
  meter("serverUsersMeter", fresh ? server.current_connected_users : 0, 10000);
}

function updateStressMetrics(stress, fresh) {
  elem("stressConnected").textContent = metricValue(stress.connected_client, fresh);
  elem("stressPlaying").textContent = metricValue(stress.playing_client, fresh);
  elem("stressCurrentRtt").textContent = metricValue(stress.current_latency_ms, fresh);
  elem("stressAvgRtt").textContent = metricValue(stress.average_latency_ms, fresh);
  elem("stressMaxRtt").textContent = metricValue(stress.max_latency_ms, fresh);
  elem("stressSamples").textContent = metricValue(stress.latency_sample_count, fresh);
  elem("stressCurrentLoginRtt").textContent = metricValue(stress.current_login_latency_ms, fresh);
  elem("stressAvgLoginRtt").textContent = metricValue(stress.average_login_latency_ms, fresh);
  elem("stressMaxLoginRtt").textContent = metricValue(stress.max_login_latency_ms, fresh);
  elem("stressLoginSamples").textContent = metricValue(stress.login_latency_sample_count, fresh);
  meter("stressConnectedMeter", fresh ? stress.connected_client : 0, maxUsers);
  meter("stressPlayingMeter", fresh ? stress.playing_client : 0, maxUsers);
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
  elem("logBox").textContent = [
    `server connected=${data.server_connected} users=${serverUsers} pending=${serverPending}`,
    `stress connected=${data.stress_connected} clients=${stressClients} playing=${stressPlaying} rtt=${stressRtt}ms login=${stressLoginRtt}ms`
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
connectEvents();
