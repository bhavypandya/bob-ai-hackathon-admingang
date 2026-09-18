/* dashboard.js — Supply Chain Disruption Assistant Frontend  v2.1 */
'use strict';

const API = 'http://localhost:8080';

// ── State ─────────────────────────────────────────────────────────────────────
let state = {
  dashboard: null,
  shipments: [],
  allShipments: [],
  disruptions: [],
  fleet: [],
  coldAlerts: [],
  recommendations: [],
  tempChart: null,
  fleetChart: null,
  activeTempShipment: null,
  shipmentFilter: 'all',
  // Stores approve/reject decisions keyed by "route:SH1001" or "carrier:SH1001"
  decisions: {}
};

// ── API helpers ────────────────────────────────────────────────────────────────
async function apiGet(path) {
  const r = await fetch(API + path);
  if (!r.ok) throw new Error(`HTTP ${r.status}`);
  return r.json();
}

async function apiPost(path, body) {
  const r = await fetch(API + path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  return r.json();
}

// ── Init ──────────────────────────────────────────────────────────────────────
async function init() {
  try {
    await checkHealth();
    await refreshAll();
    hideLoading();
  } catch (e) {
    document.getElementById('statusText').textContent = 'Backend offline';
    document.getElementById('statusDot').className = 'status-dot offline';
    hideLoading();
    showOfflineMessage();
  }
}

async function checkHealth() {
  const h = await apiGet('/api/health');
  if (h.status === 'ok') {
    document.getElementById('statusText').textContent = 'Connected';
    document.getElementById('statusDot').className = 'status-dot online';
  }
}

function hideLoading() {
  document.getElementById('loadingOverlay').classList.add('hidden');
}

function showOfflineMessage() {
  const main = document.querySelector('.main-content');
  main.innerHTML = `
    <div style="padding:40px; text-align:center; color:#6b7280;">
      <div style="font-size:48px; margin-bottom:16px;">🔌</div>
      <h2 style="color:#1a1d23; margin-bottom:8px;">Backend Not Running</h2>
      <p>Start the C++ backend server to view the dashboard.</p>
      <code style="display:block; margin:16px auto; padding:12px; background:#f7f8fa; border-radius:8px; max-width:500px;">
        cd src/build && ./bin/supply_chain_backend
      </code>
      <p>Then refresh this page at <strong>http://localhost:8080</strong></p>
    </div>`;
}

// ── Refresh all data ──────────────────────────────────────────────────────────
async function refreshAll() {
  try {
    const [dash, ships, disrupts, fleet, cold, recs] = await Promise.all([
      apiGet('/api/dashboard'),
      apiGet('/api/shipments'),
      apiGet('/api/disruptions'),
      apiGet('/api/fleet'),
      apiGet('/api/cold-chain/alerts'),
      apiGet('/api/recommendations')
    ]);

    state.dashboard = dash;
    state.allShipments = ships;
    state.shipments = dash.affected_shipments || [];
    state.disruptions = disrupts;
    state.fleet = fleet;
    state.coldAlerts = cold;
    state.recommendations = recs;

    renderOverview();
    renderDisruptions();
    renderShipments();
    renderFleet();
    renderColdChain();
    renderRecommendations();
  } catch(e) {
    console.error('Refresh error:', e);
  }
}

// ── Navigation ─────────────────────────────────────────────────────────────────
function showSection(name, btn) {
  document.querySelectorAll('.section').forEach(s => s.classList.remove('active'));
  document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
  document.getElementById('section-' + name).classList.add('active');
  btn.classList.add('active');
}

// ── OVERVIEW ──────────────────────────────────────────────────────────────────
function renderOverview() {
  const d = state.dashboard;
  if (!d) return;

  setText('kpiDisruptions', d.kpi.active_disruptions);
  setText('kpiAffected', d.kpi.affected_shipments);
  setText('kpiUtilisation', d.kpi.fleet_utilisation_pct + '%');
  setText('kpiIdle', d.kpi.idle_assets);
  setText('kpiCold', d.kpi.cold_chain_alerts);
  setText('kpiRisks', d.kpi.high_critical_risks);

  // Disruption list
  const dl = document.getElementById('overviewDisruptions');
  if (!d.disruptions || d.disruptions.length === 0) {
    dl.innerHTML = '<p class="text-muted">No active disruptions.</p>';
  } else {
    dl.innerHTML = d.disruptions.map(dis => `
      <div class="disruption-item">
        <span class="badge badge-${dis.severity}">${dis.severity}</span>
        <span><strong>${dis.name}</strong> — ${dis.location}</span>
        <span class="text-muted" style="margin-left:auto; font-size:12px">${dis.type}</span>
      </div>`).join('');
  }

  // Top affected shipments
  const topShipments = (d.affected_shipments || []).slice(0, 8);
  const tbody = document.getElementById('overviewShipmentsBody');
  tbody.innerHTML = topShipments.map(s => `
    <tr>
      <td><strong>${s.id}</strong></td>
      <td>${s.origin} → ${s.destination}</td>
      <td><span class="badge badge-${s.priority}">${s.priority}</span></td>
      <td>${s.current_route}</td>
      <td>${s.expected_delay_hours > 0 ? '+' + s.expected_delay_hours + 'h' : '—'}</td>
      <td>${riskBadge(s.risk_level, s.impact_score)}</td>
      <td>${s.cold_chain ? '<span class="cold-chain-on">❄ Yes</span>' : '<span class="cold-chain-off">No</span>'}</td>
    </tr>`).join('');

  // Top recommendations
  const topRecs = (d.recommendations || []).slice(0, 5);
  document.getElementById('overviewRecs').innerHTML = topRecs.length
    ? topRecs.map(r => recItemHtml(r)).join('')
    : '<p class="text-muted">No priority recommendations.</p>';
}

// ── DISRUPTIONS ───────────────────────────────────────────────────────────────
function renderDisruptions() {
  const container = document.getElementById('disruptionCards');
  if (!state.disruptions.length) {
    container.innerHTML = '<p class="text-muted">No disruptions in database.</p>';
    return;
  }
  container.innerHTML = state.disruptions.map(d => `
    <div class="disruption-card severity-${d.severity}">
      <div class="disruption-name">${d.name}</div>
      <div class="disruption-meta">
        <span>📍 ${d.location}</span>
        <span class="badge badge-${d.severity}">${d.severity}</span>
        <span class="badge badge-${d.status}">${d.status}</span>
      </div>
      <div class="disruption-desc">${d.description}</div>
      <div class="disruption-routes">
        <strong>Type:</strong> ${d.type} &nbsp;|&nbsp;
        <strong>Affected routes:</strong> ${d.affected_routes.join(', ')} &nbsp;|&nbsp;
        <strong>Until:</strong> ${d.expected_end_time}
      </div>
    </div>`).join('');
}

// ── SHIPMENTS ─────────────────────────────────────────────────────────────────
function filterShipments(filter, btn) {
  document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  state.shipmentFilter = filter;
  renderShipments();
}

function renderShipments() {
  let ships = state.allShipments;
  const f = state.shipmentFilter;

  if (f === 'affected') {
    ships = ships.filter(s => s.is_affected);
  } else if (f === 'cold') {
    ships = ships.filter(s => s.cold_chain);
  } else if (f === 'CRITICAL' || f === 'HIGH') {
    ships = ships.filter(s => s.risk_level === f);
  }

  const tbody = document.getElementById('shipmentsTableBody');
  tbody.innerHTML = ships.map(s => {
    const rd = state.decisions['route:'   + s.id];
    const cd = state.decisions['carrier:' + s.id];
    const decBadge = (d) => d
      ? `<span class="table-decision-badge ${d.status === 'approved' ? 'tdb-approved' : 'tdb-rejected'}">${d.status === 'approved' ? '✓' : '✕'}</span>`
      : '';
    return `
    <tr style="cursor:pointer" onclick="showShipmentDetail('${s.id}')">
      <td><strong>${s.id}</strong></td>
      <td>${s.origin}</td>
      <td>${s.destination}</td>
      <td>${s.current_route} ${decBadge(rd)}</td>
      <td>${s.carrier} ${decBadge(cd)}</td>
      <td><span class="badge badge-${s.priority}">${s.priority}</span></td>
      <td><span class="badge badge-${s.status}">${s.status}</span></td>
      <td>${s.expected_delay_hours > 0 ? '+' + s.expected_delay_hours + 'h' : '—'}</td>
      <td>
        <div class="score-bar">
          <div class="score-fill ${scoreClass(s.impact_score)}" style="width:${Math.min(s.impact_score, 100)}px"></div>
          ${s.impact_score}
        </div>
      </td>
      <td>${riskBadge(s.risk_level, s.impact_score)}</td>
      <td>${s.cold_chain ? '<span class="cold-chain-on">❄</span>' : '<span class="text-muted">—</span>'}</td>
      <td>
        <button class="btn-sm" onclick="event.stopPropagation(); quickRouteRec('${s.id}')">Route ▸</button>
      </td>
    </tr>`;
  }).join('');
}

async function showShipmentDetail(id) {
  const s = state.allShipments.find(x => x.id === id);
  if (!s) return;

  const panel = document.getElementById('shipmentDetail');
  document.getElementById('detailTitle').textContent = 'Shipment ' + id;

  // Build decision history section for this shipment
  const routeDecision    = state.decisions['route:'   + id];
  const carrierDecision  = state.decisions['carrier:' + id];

  let decisionHistoryHtml = '';
  if (routeDecision || carrierDecision) {
    decisionHistoryHtml = `
      <div class="detail-decisions">
        <div class="detail-decisions-title">📋 Decision History</div>
        ${routeDecision   ? `<div class="detail-decision-item ${routeDecision.status === 'approved' ? 'ddi-approved' : 'ddi-rejected'}">
            <strong>Route Recommendation</strong>
            <span class="ddi-badge">${routeDecision.status === 'approved' ? '✓ Approved' : '✕ Rejected'}</span>
            <div class="ddi-note">${routeDecision.note}</div>
            <div class="ddi-time">at ${routeDecision.at}</div>
          </div>` : ''}
        ${carrierDecision ? `<div class="detail-decision-item ${carrierDecision.status === 'approved' ? 'ddi-approved' : 'ddi-rejected'}">
            <strong>Carrier Recommendation</strong>
            <span class="ddi-badge">${carrierDecision.status === 'approved' ? '✓ Approved' : '✕ Rejected'}</span>
            <div class="ddi-note">${carrierDecision.note}</div>
            <div class="ddi-time">at ${carrierDecision.at}</div>
          </div>` : ''}
      </div>`;
  }

  panel.style.display = 'block';
  document.getElementById('detailContent').innerHTML = `
    <div class="detail-field"><label>Origin</label><span>${s.origin}</span></div>
    <div class="detail-field"><label>Destination</label><span>${s.destination}</span></div>
    <div class="detail-field"><label>Carrier</label><span>${s.carrier}</span></div>
    <div class="detail-field"><label>Route</label><span>${s.current_route}</span></div>
    <div class="detail-field"><label>Priority</label><span class="badge badge-${s.priority}">${s.priority}</span></div>
    <div class="detail-field"><label>Status</label><span class="badge badge-${s.status}">${s.status}</span></div>
    <div class="detail-field"><label>Expected Delay</label><span>${s.expected_delay_hours > 0 ? '+' + s.expected_delay_hours + 'h' : 'None'}</span></div>
    <div class="detail-field"><label>Risk Score</label><span>${riskBadge(s.risk_level, s.impact_score)}</span></div>
    <div class="detail-field"><label>Cold Chain</label><span>${s.cold_chain ? '❄ Yes' : 'No'}</span></div>
    ${s.impact_reasons && s.impact_reasons.length ? `
    <div class="detail-field">
      <label>Impact Reasons</label>
      <ul class="reasons-list">${s.impact_reasons.map(r => `<li>${r}</li>`).join('')}</ul>
    </div>` : ''}
    ${decisionHistoryHtml}
    <div id="detailRouteRec" style="margin-top:12px"><p class="text-muted" style="font-size:12px">Loading recommendation...</p></div>
  `;

  // Load route recommendation
  try {
    const rec = await apiPost('/api/routes/recommend', { shipment_id: id });
    const el = document.getElementById('detailRouteRec');
    if (!el) return;
    if (!rec.found) {
      el.innerHTML = `<p style="font-size:12px; color:var(--muted)">${rec.reason}</p>`;
      return;
    }
    // Show rec with quick approve/reject if not yet decided
    const rd = state.decisions['route:' + id];
    el.innerHTML = `
      <div class="detail-field"><label>Recommended Route</label>
        <strong style="color:var(--accent)">${rec.recommended_route}</strong> — ${rec.recommended_route_name}
        ${rec.additional_hours > 0 ? `<br><small style="color:var(--muted)">+${rec.additional_hours}h extra time</small>` : ''}
      </div>
      <div class="detail-field" style="font-size:12px; color:var(--muted)">${rec.reason}</div>
      ${rd
        ? decisionBanner(rd.status, rd.label, rd.at, rd.note)
        : `<div class="rec-action-row">
             <button class="btn-approve btn-sm-approve" onclick="approveRec('route','${id}','${escapeAttr(rec.recommended_route)}','${escapeAttr(rec.recommended_route_name)}'); showShipmentDetail('${id}')">✓ Approve</button>
             <button class="btn-reject btn-sm-reject"  onclick="rejectRec('route','${id}'); showShipmentDetail('${id}')">✕ Reject</button>
           </div>`
      }`;
  } catch {}
}

async function quickRouteRec(id) {
  showSection('recommendations', document.querySelector('[data-section="recommendations"]'));
  document.getElementById('routeShipmentSelect').value = id;
  await loadRouteRecommendation();
}

// ── FLEET ─────────────────────────────────────────────────────────────────────
function renderFleet() {
  const util = state.dashboard ? state.dashboard.fleet_utilisation : null;
  if (util) {
    document.getElementById('fleetStats').innerHTML = `
      <div class="fleet-stat">
        <div class="fleet-stat-value">${util.total}</div>
        <div class="fleet-stat-label">Total Assets</div>
      </div>
      <div class="fleet-stat">
        <div class="fleet-stat-value" style="color:var(--success)">${util.active}</div>
        <div class="fleet-stat-label">In Transit</div>
      </div>
      <div class="fleet-stat">
        <div class="fleet-stat-value" style="color:var(--warning)">${util.idle}</div>
        <div class="fleet-stat-label">Idle</div>
      </div>
      <div class="fleet-stat">
        <div class="fleet-stat-value" style="color:var(--accent)">${util.average_pct}%</div>
        <div class="fleet-stat-label">Avg Utilisation</div>
      </div>
      <div class="fleet-stat">
        <div class="fleet-stat-value" style="color:var(--cold)">${util.redeployment_opportunities}</div>
        <div class="fleet-stat-label">Redeploy Opps</div>
      </div>`;
  }

  // Fleet table
  const tbody = document.getElementById('fleetTableBody');
  tbody.innerHTML = state.fleet.map(a => {
    const uc = a.utilisation_pct >= 80 ? 'full' : a.utilisation_pct >= 50 ? 'high' : 'low';
    return `<tr>
      <td><strong>${a.id}</strong></td>
      <td>${a.type}</td>
      <td>${a.location}</td>
      <td><span class="badge badge-${a.status}">${a.status}</span></td>
      <td>${a.capacity_kg.toLocaleString()}</td>
      <td>
        <div style="width:80px">
          <div style="font-size:11px; text-align:right">${a.utilisation_pct}%</div>
          <div class="utilisation-bar"><div class="utilisation-fill ${uc}" style="width:${a.utilisation_pct}%"></div></div>
        </div>
      </td>
      <td>${a.refrigerated ? '<span style="color:var(--cold)">❄ Yes</span>' : '—'}</td>
      <td style="font-size:12px">${a.current_assignment || '<span class="text-muted">—</span>'}</td>
    </tr>`;
  }).join('');

  // Redeployment recommendations
  const idle = state.dashboard ? state.dashboard.idle_fleet : [];
  const recs = state.recommendations.filter(r => r.type === 'REDEPLOY_FLEET');
  const redeployEl = document.getElementById('redeploymentList');
  redeployEl.innerHTML = recs.length ? recs.map(r => `
    <div class="redeploy-item">
      <strong>🚛 ${r.target}</strong>
      ${r.action}
      <div class="redeploy-reason">${r.reason}</div>
    </div>`).join('') : '<p class="text-muted">No redeployment opportunities identified.</p>';

  // Fleet pie chart
  renderFleetChart(util);
}

function renderFleetChart(util) {
  const ctx = document.getElementById('fleetChart');
  if (!ctx) return;
  if (state.fleetChart) state.fleetChart.destroy();
  if (!util) return;

  state.fleetChart = new Chart(ctx, {
    type: 'doughnut',
    data: {
      labels: ['In Transit', 'Idle', 'Maintenance'],
      datasets: [{
        data: [util.active, util.idle, util.maintenance_assets || 0],
        backgroundColor: ['#2563eb', '#f59e0b', '#ef4444'],
        borderWidth: 2,
        borderColor: '#ffffff'
      }]
    },
    options: {
      responsive: true,
      plugins: {
        legend: { position: 'bottom', labels: { font: { size: 12 } } },
        title: { display: true, text: 'Fleet Status Distribution', font: { size: 13 } }
      },
      cutout: '65%'
    }
  });
}

// ── COLD CHAIN ────────────────────────────────────────────────────────────────
function renderColdChain() {
  const alerts = state.coldAlerts;
  const container = document.getElementById('coldChainCards');
  const selector = document.getElementById('coldShipmentSelector');

  container.innerHTML = alerts.map(a => {
    const cls = tempClass(a.current_temp, a.config);
    return `
    <div class="cold-card severity-${a.severity}" onclick="loadTempChart('${a.shipment_id}')">
      <div class="cold-card-header">
        <div class="cold-card-id">${a.shipment_id}</div>
        <span class="badge badge-${a.severity}">${a.severity}</span>
      </div>
      <div class="cold-temp ${cls}">${a.current_temp.toFixed(1)}°C</div>
      <div class="cold-range">Range: ${a.config.min_temp}°C – ${a.config.max_temp}°C</div>
      <div class="cold-excursion">
        ${a.excursion_duration_minutes > 0
          ? `⚠ Excursion: ${a.excursion_duration_minutes} min | Peak: ${a.peak_temp.toFixed(1)}°C`
          : '✓ Within configured range'}
      </div>
      <div class="cold-action">${a.recommended_action}</div>
    </div>`;
  }).join('');

  selector.innerHTML = alerts.map(a => `
    <button class="shipment-pill ${a.shipment_id === state.activeTempShipment ? 'active' : ''}"
            onclick="loadTempChart('${a.shipment_id}')">${a.shipment_id}</button>`).join('');

  if (!state.activeTempShipment && alerts.length > 0) {
    loadTempChart(alerts[0].shipment_id);
  }
}

async function loadTempChart(shipmentId) {
  state.activeTempShipment = shipmentId;
  document.getElementById('tempChartLabel').textContent = shipmentId;

  // Update pill active state
  document.querySelectorAll('.shipment-pill').forEach(p => {
    p.classList.toggle('active', p.textContent === shipmentId);
  });

  try {
    const data = await apiGet('/api/cold-chain/' + shipmentId + '/temperature');
    renderTemperatureChart(data, shipmentId);
  } catch(e) {
    console.error('Failed to load temp chart:', e);
  }
}

function renderTemperatureChart(data, shipmentId) {
  const ctx = document.getElementById('temperatureChart');
  if (!ctx) return;
  if (state.tempChart) state.tempChart.destroy();

  const readings = data.readings || [];
  const cfg = data.config;
  const labels = readings.map(r => r.timestamp.substring(11, 16));
  const temps = readings.map(r => r.temperature);

  state.tempChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels,
      datasets: [
        {
          label: 'Temperature (°C)',
          data: temps,
          borderColor: '#2563eb',
          backgroundColor: 'rgba(37,99,235,0.08)',
          borderWidth: 2,
          fill: true,
          tension: 0.3,
          pointRadius: 4,
          pointBackgroundColor: temps.map(t =>
            t > cfg.max_temp || t < cfg.min_temp ? '#dc2626' : '#2563eb')
        },
        {
          label: `Max (${cfg.max_temp}°C)`,
          data: labels.map(() => cfg.max_temp),
          borderColor: '#dc2626',
          borderDash: [6, 4],
          borderWidth: 1.5,
          pointRadius: 0,
          fill: false
        },
        {
          label: `Min (${cfg.min_temp}°C)`,
          data: labels.map(() => cfg.min_temp),
          borderColor: '#2563eb',
          borderDash: [6, 4],
          borderWidth: 1.5,
          pointRadius: 0,
          fill: false
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: {
        title: {
          display: true,
          text: `Temperature History — ${shipmentId}`,
          font: { size: 13 }
        },
        legend: { labels: { font: { size: 12 } } }
      },
      scales: {
        y: {
          title: { display: true, text: 'Temperature (°C)' }
        },
        x: {
          title: { display: true, text: 'Time' }
        }
      }
    }
  });
}

function tempClass(temp, cfg) {
  if (temp > cfg.max_temp + cfg.critical_margin || temp < cfg.min_temp - cfg.critical_margin) return 'crit';
  if (temp > cfg.max_temp + cfg.warning_margin || temp < cfg.min_temp - cfg.warning_margin) return 'high';
  if (temp > cfg.max_temp || temp < cfg.min_temp) return 'warn';
  return 'ok';
}

// ── RECOMMENDATIONS ───────────────────────────────────────────────────────────
async function renderRecommendations() {
  // Populate ship selects — all shipments, affected first
  const sorted = [...state.allShipments].sort((a, b) => {
    if (a.is_affected && !b.is_affected) return -1;
    if (!a.is_affected && b.is_affected) return 1;
    return 0;
  });
  const opts = sorted
    .map(s => `<option value="${s.id}">${s.id} — ${s.origin}→${s.destination} (${s.risk_level})${s.is_affected ? ' ⚠' : ''}</option>`)
    .join('');
  document.getElementById('routeShipmentSelect').innerHTML =
    '<option value="">— choose shipment —</option>' + opts;
  document.getElementById('carrierShipmentSelect').innerHTML =
    '<option value="">— choose shipment —</option>' + opts;

  // All recommendations
  const recs = state.recommendations;
  document.getElementById('allRecommendations').innerHTML = recs.length
    ? recs.map(r => recItemHtml(r)).join('')
    : '<p class="text-muted">No recommendations at this time.</p>';
}

async function loadRouteRecommendation() {
  const sid = document.getElementById('routeShipmentSelect').value;
  if (!sid) return;
  const el = document.getElementById('routeRecResult');
  el.innerHTML = '<p class="text-muted" style="font-size:12px">Loading...</p>';

  const decisionKey = 'route:' + sid;
  const existing = state.decisions[decisionKey];

  try {
    const rec = await apiPost('/api/routes/recommend', { shipment_id: sid });
    const s = state.allShipments.find(x => x.id === sid);
    const currentRoute = rec.current_route || (s ? s.current_route : '');
    const recommendedRoute = rec.recommended_route || 'N/A';
    const routeName = rec.recommended_route_name || '';

    // Store rec data on the key so approve can reference it
    if (!existing) {
      state.decisions[decisionKey + '_data'] = { currentRoute, recommendedRoute, routeName, shipmentId: sid };
    }

    const beforeAfter = `
      <div class="rec-before-after">
        <div class="rec-box">
          <div style="font-size:11px; color:var(--muted)">CURRENT ROUTE</div>
          <strong>${currentRoute}</strong>
          ${s ? `<br><small class="text-muted">${s.origin} → ${s.destination}</small>` : ''}
        </div>
        <div class="rec-arrow">→</div>
        <div class="rec-box" style="border-color:var(--accent)">
          <div style="font-size:11px; color:var(--accent)">RECOMMENDED ROUTE</div>
          <strong style="color:var(--accent)">${recommendedRoute}</strong>
          <br><small>${routeName}</small>
        </div>
      </div>
      ${rec.additional_hours > 0 ? `<p style="font-size:12px">⏱ Additional time: <strong>+${rec.additional_hours}h</strong></p>` : ''}
      ${rec.cost_usd ? `<p style="font-size:12px">💰 Estimated cost: <strong>USD ${rec.cost_usd.toLocaleString()}</strong></p>` : ''}
      <p style="font-size:12.5px; margin-top:6px; color:var(--muted)">${rec.reason}</p>`;

    if (existing) {
      // Already decided — show banner, no buttons
      el.innerHTML = beforeAfter + decisionBanner(existing.status, existing.label, existing.at, existing.note);
    } else {
      // Show approve / reject buttons
      el.innerHTML = beforeAfter + `
        <div class="rec-action-row" id="routeActionRow_${sid}">
          <button class="btn-approve" onclick="approveRec('route','${sid}','${escapeAttr(recommendedRoute)}','${escapeAttr(routeName)}')">✓ Approve Route Change</button>
          <button class="btn-reject"  onclick="rejectRec('route','${sid}')">✕ Reject</button>
        </div>`;
    }
  } catch(e) {
    el.textContent = 'Error loading recommendation.';
  }
}

async function loadCarrierRecommendation() {
  const sid = document.getElementById('carrierShipmentSelect').value;
  if (!sid) return;
  const el = document.getElementById('carrierRecResult');
  el.innerHTML = '<p class="text-muted" style="font-size:12px">Loading...</p>';

  const decisionKey = 'carrier:' + sid;
  const existing = state.decisions[decisionKey];

  try {
    const rec = await apiPost('/api/carriers/recommend', { shipment_id: sid, required_capacity_kg: 5000 });

    const beforeAfter = `
      <div class="rec-before-after">
        <div class="rec-box">
          <div style="font-size:11px; color:var(--muted)">CURRENT CARRIER</div>
          <strong>${rec.current_carrier}</strong>
          <br><small>${rec.current_carrier_name}</small>
        </div>
        <div class="rec-arrow">→</div>
        <div class="rec-box" style="border-color:var(--accent)">
          <div style="font-size:11px; color:var(--accent)">RECOMMENDED CARRIER</div>
          <strong style="color:var(--accent)">${rec.recommended_carrier}</strong>
          <br><small>${rec.recommended_carrier_name}</small>
        </div>
      </div>
      <p style="font-size:12.5px; margin-top:6px; color:var(--muted)">${rec.reason}</p>
      ${rec.expected_impact ? `<p style="font-size:12px; color:var(--accent); margin-top:4px">📈 ${rec.expected_impact}</p>` : ''}`;

    if (existing) {
      el.innerHTML = beforeAfter + decisionBanner(existing.status, existing.label, existing.at, existing.note);
    } else {
      el.innerHTML = beforeAfter + `
        <div class="rec-action-row" id="carrierActionRow_${sid}">
          <button class="btn-approve" onclick="approveRec('carrier','${sid}','${escapeAttr(rec.recommended_carrier)}','${escapeAttr(rec.recommended_carrier_name)}')">✓ Approve Carrier Change</button>
          <button class="btn-reject"  onclick="rejectRec('carrier','${sid}')">✕ Reject</button>
        </div>`;
    }
  } catch(e) {
    el.textContent = 'Error loading recommendation.';
  }
}

// ── Approve / Reject logic ────────────────────────────────────────────────────
function approveRec(type, shipmentId, value, label) {
  const decisionKey = type + ':' + shipmentId;
  const now = new Date().toLocaleTimeString();
  state.decisions[decisionKey] = {
    status: 'approved',
    label: label || value,
    value,
    type,
    shipmentId,
    at: now,
    note: type === 'route'
      ? `Route changed to ${value} (${label})`
      : `Carrier changed to ${value} (${label})`
  };

  // Patch the local shipment so the Shipments tab reflects it instantly
  const s = state.allShipments.find(x => x.id === shipmentId);
  if (s) {
    if (type === 'route') {
      s.current_route = value;
      s.status = 'ON_TIME';
      s.expected_delay_hours = 0;
      s.risk_level = 'LOW';
      s.impact_score = 0;
    } else {
      s.carrier = value;
    }
  }

  // Re-render table row + refresh the result panel
  renderShipments();
  if (type === 'route')   loadRouteRecommendation();
  else                    loadCarrierRecommendation();
}

function rejectRec(type, shipmentId) {
  const decisionKey = type + ':' + shipmentId;
  const now = new Date().toLocaleTimeString();
  state.decisions[decisionKey] = {
    status: 'rejected',
    label: '',
    value: '',
    type,
    shipmentId,
    at: now,
    note: type === 'route' ? 'Route change rejected — keeping current route.' : 'Carrier change rejected — keeping current carrier.'
  };

  if (type === 'route')   loadRouteRecommendation();
  else                    loadCarrierRecommendation();
}

function decisionBanner(status, label, at, note) {
  const isApproved = status === 'approved';
  return `
    <div class="decision-banner ${isApproved ? 'decision-approved' : 'decision-rejected'}">
      <span class="decision-icon">${isApproved ? '✓' : '✕'}</span>
      <div class="decision-text">
        <strong>${isApproved ? 'Approved' : 'Rejected'}</strong> at ${at}
        <div class="decision-note">${note}</div>
      </div>
      <span class="decision-locked">🔒 Locked</span>
    </div>`;
}

function escapeAttr(str) {
  return String(str || '').replace(/'/g, "\\'").replace(/"/g, '&quot;');
}

// ── SIMULATION ────────────────────────────────────────────────────────────────
async function runSimulation(scenario, target = '') {
  const resultEl = document.getElementById('simResult');
  const titleEl = document.getElementById('simResultTitle');
  const contentEl = document.getElementById('simResultContent');

  titleEl.textContent = 'Running simulation...';
  resultEl.style.display = 'block';
  contentEl.innerHTML = '<div class="loading-spinner" style="width:24px;height:24px;border-width:2px;margin:12px auto"></div>';

  try {
    const body = target ? { scenario, target } : { scenario };
    const result = await apiPost('/api/simulation', body);

    titleEl.textContent = result.success ? '✓ Simulation Applied' : '✗ Simulation Failed';
    contentEl.innerHTML = `
      <p style="margin-bottom:12px; font-size:13px">${result.message}</p>
      ${result.before ? `
      <div class="sim-compare">
        <div class="sim-col">
          <h4>Before</h4>
          <div class="sim-metric">Affected Shipments <span>${result.before.affected_shipments}</span></div>
          <div class="sim-metric">Idle Assets <span>${result.before.idle_assets}</span></div>
          <div class="sim-metric">Cold-Chain Alerts <span>${result.before.cold_chain_alerts}</span></div>
        </div>
        <div class="sim-col">
          <h4>After</h4>
          <div class="sim-metric">Affected Shipments <span class="${result.after.affected_shipments > result.before.affected_shipments ? 'delta-up' : 'delta-down'}">${result.after.affected_shipments}</span></div>
          <div class="sim-metric">Idle Assets <span>${result.after.idle_assets}</span></div>
          <div class="sim-metric">Cold-Chain Alerts <span class="${result.after.cold_chain_alerts > result.before.cold_chain_alerts ? 'delta-up' : ''}">${result.after.cold_chain_alerts}</span></div>
        </div>
      </div>` : ''}`;

    // Refresh data to reflect simulation
    await refreshAll();
  } catch(e) {
    contentEl.textContent = 'Error running simulation: ' + e.message;
  }
}

// ── BOB AI ────────────────────────────────────────────────────────────────────
function askBob(question) {
  document.getElementById('bobInput').value = question;
  sendBobMessage();
}

async function sendBobMessage() {
  const input = document.getElementById('bobInput');
  const q = input.value.trim();
  if (!q) return;
  input.value = '';

  const chat = document.getElementById('bobChat');

  // User message
  chat.innerHTML += `
    <div class="bob-msg bob-msg-user">
      <div class="bob-avatar" style="background:var(--surface2); color:var(--text)">👤</div>
      <div class="bob-bubble">${escapeHtml(q)}</div>
    </div>`;

  // Thinking indicator
  const thinkId = 'bob-think-' + Date.now();
  chat.innerHTML += `
    <div class="bob-msg" id="${thinkId}">
      <div class="bob-avatar">🤖</div>
      <div class="bob-bubble" style="color:var(--muted)">Analysing supply chain data...</div>
    </div>`;
  chat.scrollTop = chat.scrollHeight;

  try {
    const resp = await apiPost('/api/bob/query', { question: q });
    const thinking = document.getElementById(thinkId);
    if (thinking) thinking.remove();
    chat.innerHTML += `
      <div class="bob-msg">
        <div class="bob-avatar">🤖</div>
        <div class="bob-bubble">${escapeHtml(resp.answer)}</div>
      </div>`;
  } catch(e) {
    const thinking = document.getElementById(thinkId);
    if (thinking) thinking.remove();
    chat.innerHTML += `
      <div class="bob-msg">
        <div class="bob-avatar">🤖</div>
        <div class="bob-bubble" style="color:var(--danger)">Error connecting to backend: ${escapeHtml(e.message)}</div>
      </div>`;
  }
  chat.scrollTop = chat.scrollHeight;
}

// ── Helpers ───────────────────────────────────────────────────────────────────
function setText(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}

function riskBadge(level, score) {
  return `<span class="badge badge-${level}">${level} ${score > 0 ? '(' + score + ')' : ''}</span>`;
}

function scoreClass(score) {
  if (score >= 75) return 'critical';
  if (score >= 50) return 'high';
  if (score >= 25) return 'medium';
  return 'low';
}

function recTypeIcon(type) {
  const icons = {
    'REROUTE_SHIPMENT': '🗺',
    'CHANGE_CARRIER': '🚚',
    'REDEPLOY_FLEET': '↔',
    'PRIORITISE_SHIPMENT': '⬆',
    'COLD_CHAIN_REVIEW': '❄',
    'MONITOR_DISRUPTION': '👁'
  };
  return icons[type] || '📋';
}

function recItemHtml(r) {
  return `
    <div class="rec-item priority-${r.priority}">
      <div class="rec-icon">${recTypeIcon(r.type)}</div>
      <div class="rec-content">
        <div class="rec-action">${r.action}</div>
        <div class="rec-reason">${r.reason}</div>
        ${r.expected_impact ? `<div class="rec-impact">${r.expected_impact}</div>` : ''}
      </div>
      <span class="badge badge-${r.priority}" style="align-self:flex-start; flex-shrink:0">${r.priority}</span>
    </div>`;
}

function escapeHtml(str) {
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/\n/g, '<br>');
}

// ── Start ─────────────────────────────────────────────────────────────────────
window.addEventListener('DOMContentLoaded', init);
