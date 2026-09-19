/* dashboard.js — Supply Chain Disruption Assistant Frontend */
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
  // Sorting state: { col: 'id', dir: 'asc' } per table
  shipmentSort: { col: null, dir: 'asc' },
  fleetSort: { col: null, dir: 'asc' },
  overviewSort: { col: null, dir: 'asc' },
  // Stores approve/reject decisions keyed by "route:SH1001" or "carrier:SH1001"
  decisions: {},
  // Driver requests
  driverRequests: [],
  driverReqFilter: 'all',
  // Currently viewed shipment for timeline
  currentDetailShipmentId: null,
  // Polling
  pollingInterval: null
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
    // Start polling every 5 seconds for driver requests
    state.pollingInterval = setInterval(pollDriverRequests, 5000);
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
    const [dash, ships, disrupts, fleet, cold, recs, driverReqs] = await Promise.all([
      apiGet('/api/dashboard'),
      apiGet('/api/shipments'),
      apiGet('/api/disruptions'),
      apiGet('/api/fleet'),
      apiGet('/api/cold-chain/alerts'),
      apiGet('/api/recommendations'),
      apiGet('/api/driver-requests')
    ]);

    state.dashboard = dash;
    state.allShipments = ships;
    state.shipments = dash.affected_shipments || [];
    state.disruptions = disrupts;
    state.fleet = fleet;
    state.coldAlerts = cold;
    state.recommendations = recs;
    state.driverRequests = driverReqs;

    renderOverview();
    renderDisruptions();
    renderShipments();
    renderFleet();
    renderColdChain();
    renderDriverRequests();
    updatePendingBadge();
  } catch(e) {
    console.error('Refresh error:', e);
  }
}

// ── Poll driver requests for real-time updates ────────────────────────────────
async function pollDriverRequests() {
  try {
    const reqs = await apiGet('/api/driver-requests');
    const prev = state.driverRequests;
    state.driverRequests = reqs;

    // Check if we're on driver requests section — refresh it
    const drSection = document.getElementById('section-driverrequests');
    if (drSection && drSection.classList.contains('active')) {
      renderDriverRequests();
    }
    updatePendingBadge();
  } catch(e) { /* silent */ }
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

  // Top affected shipments with sorting
  const cols = ['id','origin','destination','priority','route','delay','risk','cold_chain'];
  const sort = state.overviewSort;
  let topShipments = [...(d.affected_shipments || [])].slice(0, 10);
  if (sort.col) topShipments = sortArray(topShipments, sort.col, sort.dir);

  const sortIcon = (col) => sort.col === col ? (sort.dir === 'asc' ? ' ↑' : ' ↓') : '';
  const thClick = (col, label) =>
    `<th class="sortable-th" onclick="sortOverview('${col}')">${label}${sortIcon(col)}</th>`;

  const thead = document.querySelector('#overviewShipmentsTable thead tr');
  if (thead) {
    thead.innerHTML =
      thClick('id','SHIPMENT') +
      thClick('origin','ORIGIN → DEST') +
      thClick('priority','PRIORITY') +
      thClick('current_route','ROUTE') +
      thClick('expected_delay_hours','DELAY') +
      thClick('risk_level','RISK') +
      thClick('cold_chain','COLD CHAIN');
  }

  const tbody = document.getElementById('overviewShipmentsBody');
  tbody.innerHTML = topShipments.map(s => `
    <tr style="cursor:pointer" onclick="goToShipment('${s.id}')">
      <td><strong>${s.id}</strong></td>
      <td>${s.origin} → ${s.destination}</td>
      <td><span class="badge badge-${s.priority}">${s.priority}</span></td>
      <td>${s.current_route}</td>
      <td>${s.expected_delay_hours > 0 ? '+' + s.expected_delay_hours + 'h' : '—'}</td>
      <td>${riskBadge(s.risk_level, s.impact_score)}</td>
      <td>${s.cold_chain ? '<span class="cold-chain-on">❄ Yes</span>' : '<span class="cold-chain-off">No</span>'}</td>
    </tr>`).join('');

  // Top recommendations — clickable, go to shipments
  const topRecs = (d.recommendations || []).slice(0, 5);
  document.getElementById('overviewRecs').innerHTML = topRecs.length
    ? topRecs.map(r => recItemHtml(r, true)).join('')
    : '<p class="text-muted">No priority recommendations.</p>';
}

// Navigate to shipments tab and open detail panel for a shipment
function goToShipment(id) {
  const btn = document.querySelector('[data-section="shipments"]');
  showSection('shipments', btn);
  // Small delay to ensure section is visible before opening panel
  setTimeout(() => showShipmentDetail(id), 50);
}

function sortOverview(col) {
  if (state.overviewSort.col === col) {
    state.overviewSort.dir = state.overviewSort.dir === 'asc' ? 'desc' : 'asc';
  } else {
    state.overviewSort.col = col;
    state.overviewSort.dir = 'asc';
  }
  renderOverview();
}

// ── DISRUPTIONS ───────────────────────────────────────────────────────────────
function renderDisruptions() {
  const container = document.getElementById('disruptionCards');
  if (!state.disruptions.length) {
    container.innerHTML = '<p class="text-muted">No disruptions in database.</p>';
    return;
  }
  container.innerHTML = state.disruptions.map((d, idx) => `
    <div class="disruption-card severity-${d.severity}" onclick="openDisruptionModal(${idx})">
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
      <div style="margin-top:10px; font-size:12px; color:var(--accent); font-weight:600;">Click for full details →</div>
    </div>`).join('');
}

function openDisruptionModal(idx) {
  const d = state.disruptions[idx];
  if (!d) return;

  // Count affected shipments
  const affectedShips = state.allShipments.filter(s =>
    d.affected_routes && d.affected_routes.some(r => s.current_route === r)
  );

  const severityColor = {
    CRITICAL: '#7c3aed', HIGH: '#dc2626', MEDIUM: '#d97706', LOW: '#16a34a'
  }[d.severity] || '#2563eb';

  const modal = document.getElementById('disruptionModal');
  document.getElementById('disruptionModalContent').innerHTML = `
    <div class="dis-modal-header" style="border-left:5px solid ${severityColor}">
      <div class="dis-modal-title">${d.name}</div>
      <div class="dis-modal-badges">
        <span class="badge badge-${d.severity}">${d.severity}</span>
        <span class="badge badge-${d.status}" style="margin-left:6px">${d.status}</span>
      </div>
    </div>

    <div class="dis-modal-grid">
      <div class="dis-modal-field">
        <div class="dis-modal-label">📍 Location</div>
        <div class="dis-modal-value">${d.location}</div>
      </div>
      <div class="dis-modal-field">
        <div class="dis-modal-label">🔖 Type</div>
        <div class="dis-modal-value">${d.type}</div>
      </div>
      <div class="dis-modal-field">
        <div class="dis-modal-label">⏳ Expected End</div>
        <div class="dis-modal-value">${d.expected_end_time}</div>
      </div>
      <div class="dis-modal-field">
        <div class="dis-modal-label">📦 Affected Shipments</div>
        <div class="dis-modal-value" style="color:${severityColor}; font-weight:700">${affectedShips.length}</div>
      </div>
    </div>

    <div class="dis-modal-field" style="margin-top:16px">
      <div class="dis-modal-label">📋 Description</div>
      <div class="dis-modal-desc">${d.description}</div>
    </div>

    <div class="dis-modal-field" style="margin-top:16px">
      <div class="dis-modal-label">🛣 Affected Routes</div>
      <div style="display:flex; gap:6px; flex-wrap:wrap; margin-top:6px">
        ${d.affected_routes.map(r => `<span class="badge badge-HIGH" style="font-size:12px; padding:4px 10px">${r}</span>`).join('')}
      </div>
    </div>

    ${affectedShips.length > 0 ? `
    <div class="dis-modal-field" style="margin-top:16px">
      <div class="dis-modal-label">📦 Impacted Shipments</div>
      <div class="table-wrapper" style="margin-top:8px">
        <table class="data-table">
          <thead><tr>
            <th>ID</th><th>ORIGIN → DEST</th><th>CARGO</th><th>STATUS</th><th>DELAY</th><th>RISK</th><th>ACTION</th>
          </tr></thead>
          <tbody>
            ${affectedShips.map(s => `
              <tr>
                <td><strong>${s.id}</strong></td>
                <td>${s.origin} → ${s.destination}</td>
                <td>${s.cargo_type || '—'}</td>
                <td><span class="badge badge-${s.status}">${s.status}</span></td>
                <td>${s.expected_delay_hours > 0 ? '+' + s.expected_delay_hours + 'h' : '—'}</td>
                <td>${riskBadge(s.risk_level, s.impact_score)}</td>
                <td><button class="btn-sm" onclick="closeDisruptionModal(); goToShipment('${s.id}')">View →</button></td>
              </tr>`).join('')}
          </tbody>
        </table>
      </div>
    </div>` : ''}

    <div class="dis-modal-field" style="margin-top:16px">
      <div class="dis-modal-label">💡 Recommended Actions</div>
      <div class="dis-modal-actions-list">
        ${d.severity === 'CRITICAL' || d.severity === 'HIGH' ? `
          <div class="dis-action-item">🔀 Reroute all affected shipments immediately</div>
          <div class="dis-action-item">📞 Alert carriers and logistics partners</div>
          <div class="dis-action-item">🚨 Escalate to operations management</div>
        ` : `
          <div class="dis-action-item">👁 Monitor situation closely</div>
          <div class="dis-action-item">📋 Review alternative routes as contingency</div>
        `}
        ${d.type === 'WEATHER' ? '<div class="dis-action-item">🌧 Check weather forecasts for next 48h</div>' : ''}
        ${d.type === 'PORT_STRIKE' ? '<div class="dis-action-item">⚓ Check alternative port availability</div>' : ''}
      </div>
    </div>
  `;
  modal.classList.add('active');
}

function closeDisruptionModal() {
  document.getElementById('disruptionModal').classList.remove('active');
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
    ships = ships.filter(s => s.status === 'DELAYED' || s.status === 'DISRUPTED' || s.status === 'AT_RISK');
  } else if (f === 'cold') {
    ships = ships.filter(s => s.cold_chain);
  }

  // Apply sort
  const sort = state.shipmentSort;
  if (sort.col) ships = sortArray([...ships], sort.col, sort.dir);

  // Re-render thead with sort indicators
  const sortIcon = (col) => sort.col === col ? (sort.dir === 'asc' ? ' ↑' : ' ↓') : '';
  const thClick = (col, label) =>
    `<th class="sortable-th" onclick="sortShipments('${col}')">${label}${sortIcon(col)}</th>`;
  const shipTableHead = document.querySelector('#shipmentsTable thead tr');
  if (shipTableHead) {
    shipTableHead.innerHTML =
      thClick('id','ID') +
      thClick('origin','ORIGIN') +
      thClick('destination','DESTINATION') +
      thClick('current_route','ROUTE') +
      thClick('carrier','CARRIER') +
      thClick('priority','PRIORITY') +
      thClick('status','STATUS') +
      thClick('expected_delay_hours','DELAY (H)') +
      thClick('impact_score','RISK SCORE') +
      thClick('risk_level','RISK') +
      thClick('cold_chain','COLD CHAIN') +
      '<th>ACTIONS</th>';
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
        <button class="btn-sm" onclick="event.stopPropagation(); showShipmentDetail('${s.id}')">Details ▸</button>
      </td>
    </tr>`;
  }).join('');
}

function sortShipments(col) {
  if (state.shipmentSort.col === col) {
    state.shipmentSort.dir = state.shipmentSort.dir === 'asc' ? 'desc' : 'asc';
  } else {
    state.shipmentSort.col = col;
    state.shipmentSort.dir = 'asc';
  }
  renderShipments();
}

async function showShipmentDetail(id) {
  const s = state.allShipments.find(x => x.id === id);
  if (!s) return;
  state.currentDetailShipmentId = id;

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

  const tMode = transportMode(s.fleet_asset, s.carrier);
  panel.style.display = 'flex';
  document.getElementById('detailContent').innerHTML = `
    <div class="detail-section">
      <div class="detail-section-title">📦 Shipment Info</div>
      <div class="detail-grid">
        <div class="detail-field"><label>Origin</label><span>${s.origin}</span></div>
        <div class="detail-field"><label>Destination</label><span>${s.destination}</span></div>
        <div class="detail-field"><label>Transport</label><span>${transportModeLabel(tMode)}</span></div>
        <div class="detail-field"><label>Carrier</label><span>${s.carrier}</span></div>
        <div class="detail-field"><label>Route</label><span>${s.current_route}</span></div>
        <div class="detail-field"><label>Priority</label><span class="badge badge-${s.priority}">${s.priority}</span></div>
        <div class="detail-field"><label>Status</label><span class="badge badge-${s.status}">${s.status}</span></div>
        <div class="detail-field"><label>Departure</label><span>${s.planned_departure || '—'}</span></div>
        <div class="detail-field"><label>Planned Arrival</label><span>${s.planned_arrival || '—'}</span></div>
        <div class="detail-field"><label>Current ETA</label><span style="color:${s.expected_delay_hours > 0 ? 'var(--warning)' : 'var(--success)'};font-weight:600">${s.current_eta || s.planned_arrival || '—'}</span></div>
        <div class="detail-field"><label>Expected Delay</label><span>${s.expected_delay_hours > 0 ? '+' + s.expected_delay_hours + 'h' : 'None'}</span></div>
        <div class="detail-field"><label>Risk Score</label><span>${riskBadge(s.risk_level, s.impact_score)}</span></div>
        <div class="detail-field"><label>Cold Chain</label><span>${s.cold_chain ? '❄ Yes' : 'No'}</span></div>
        ${s.cargo_type ? `<div class="detail-field"><label>Cargo Type</label><span>${s.cargo_type}</span></div>` : ''}
      </div>
    </div>
    ${s.impact_reasons && s.impact_reasons.length ? `
    <div class="detail-section">
      <div class="detail-section-title">⚠ Impact Reasons</div>
      <ul class="reasons-list">${s.impact_reasons.map(r => `<li>${r}</li>`).join('')}</ul>
    </div>` : ''}
    ${decisionHistoryHtml}
    <div class="detail-section">
      <div class="detail-section-title">🗺 Route Recommendation</div>
      <div id="detailRouteRec"><p class="text-muted" style="font-size:12px">Loading recommendation...</p></div>
    </div>
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

  // Fleet table with sorting
  const sort = state.fleetSort;
  const sortIcon = (col) => sort.col === col ? (sort.dir === 'asc' ? ' ↑' : ' ↓') : '';
  const thClick = (col, label) =>
    `<th class="sortable-th" onclick="sortFleet('${col}')">${label}${sortIcon(col)}</th>`;
  const fleetTableHead = document.querySelector('#fleetTable thead tr');
  if (fleetTableHead) {
    fleetTableHead.innerHTML =
      thClick('id','ASSET') +
      thClick('type','TYPE') +
      thClick('location','LOCATION') +
      thClick('status','STATUS') +
      thClick('capacity_kg','CAPACITY (KG)') +
      thClick('utilisation_pct','UTILISATION') +
      thClick('refrigerated','REFRIGERATED') +
      '<th>ASSIGNMENT</th>';
  }

  let fleetData = [...state.fleet];
  if (sort.col) fleetData = sortArray(fleetData, sort.col, sort.dir);

  const tbody = document.getElementById('fleetTableBody');
  tbody.innerHTML = fleetData.map(a => {
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

function sortFleet(col) {
  if (state.fleetSort.col === col) {
    state.fleetSort.dir = state.fleetSort.dir === 'asc' ? 'desc' : 'asc';
  } else {
    state.fleetSort.col = col;
    state.fleetSort.dir = 'asc';
  }
  renderFleet();
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

// ── Generic sort helper ────────────────────────────────────────────────────────
function sortArray(arr, col, dir) {
  return arr.sort((a, b) => {
    let va = a[col], vb = b[col];
    if (typeof va === 'string') va = va.toLowerCase();
    if (typeof vb === 'string') vb = vb.toLowerCase();
    if (va < vb) return dir === 'asc' ? -1 : 1;
    if (va > vb) return dir === 'asc' ? 1 : -1;
    return 0;
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

  // Re-render table immediately with the badge
  renderShipments();
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

  // Re-render table immediately with the badge
  renderShipments();
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

  // Highlight the clicked card
  document.querySelectorAll('.sim-card').forEach(c => c.classList.remove('sim-active'));

  try {
    const body = target ? { scenario, target } : { scenario };
    const result = await apiPost('/api/simulation', body);

    // Apply extra local effects on top of backend result
    applySimulationEffects(scenario);

    titleEl.textContent = result.success ? '✓ Simulation Applied' : '✗ Simulation Failed';

    const before = result.before || {};
    const after  = result.after  || {};

    // Compute enhanced deltas
    const affDelta  = (after.affected_shipments  || 0) - (before.affected_shipments  || 0);
    const coldDelta = (after.cold_chain_alerts    || 0) - (before.cold_chain_alerts   || 0);
    const idleDelta = (after.idle_assets          || 0) - (before.idle_assets         || 0);

    contentEl.innerHTML = `
      <p style="margin-bottom:12px; font-size:13px">${result.message}</p>
      ${result.before ? `
      <div class="sim-compare">
        <div class="sim-col">
          <h4>Before</h4>
          <div class="sim-metric">Affected Shipments <span>${before.affected_shipments}</span></div>
          <div class="sim-metric">Idle Assets <span>${before.idle_assets}</span></div>
          <div class="sim-metric">Cold-Chain Alerts <span>${before.cold_chain_alerts}</span></div>
          <div class="sim-metric">High/Critical Risks <span>${before.high_critical_risks || '—'}</span></div>
        </div>
        <div class="sim-col">
          <h4>After</h4>
          <div class="sim-metric">Affected Shipments <span class="${affDelta > 0 ? 'delta-up' : 'delta-down'}">${after.affected_shipments} ${affDelta !== 0 ? '(' + (affDelta > 0 ? '+' : '') + affDelta + ')' : ''}</span></div>
          <div class="sim-metric">Idle Assets <span class="${idleDelta > 0 ? 'delta-up' : ''}">${after.idle_assets}</span></div>
          <div class="sim-metric">Cold-Chain Alerts <span class="${coldDelta > 0 ? 'delta-up' : ''}">${after.cold_chain_alerts} ${coldDelta !== 0 ? '(' + (coldDelta > 0 ? '+' : '') + coldDelta + ')' : ''}</span></div>
          <div class="sim-metric">High/Critical Risks <span class="${(after.high_critical_risks || 0) > (before.high_critical_risks || 0) ? 'delta-up' : 'delta-down'}">${after.high_critical_risks || '—'}</span></div>
        </div>
      </div>` : ''}
      <div class="sim-impact-list" id="simImpactList"></div>`;

    // Refresh data to reflect simulation
    await refreshAll();
    renderSimImpactSummary(scenario);
  } catch(e) {
    contentEl.textContent = 'Error running simulation: ' + e.message;
  }
}

// Apply local effects to shipments & fleet data to make simulations more visible
function applySimulationEffects(scenario) {
  if (scenario === 'reset') return;

  const ships = state.allShipments;

  if (scenario === 'weather_disruption') {
    // Gujarat corridor routes R01, R05 — mark more shipments as DISRUPTED
    ships.forEach(s => {
      if (['R01','R05','R08'].includes(s.current_route) && s.status !== 'DISRUPTED') {
        s.status = 'DISRUPTED';
        s.risk_level = s.risk_level === 'LOW' ? 'MEDIUM' : 'HIGH';
        s.expected_delay_hours = Math.max(s.expected_delay_hours, 18 + Math.floor(Math.random() * 24));
        s.impact_score = Math.min(s.impact_score + 30, 100);
        s.is_affected = true;
      }
    });
  } else if (scenario === 'road_closure') {
    // Delhi-Jaipur R06 — close it, delay all on R10, R12 too
    ships.forEach(s => {
      if (['R06','R10','R12'].includes(s.current_route)) {
        s.status = 'DELAYED';
        s.expected_delay_hours = Math.max(s.expected_delay_hours, 12 + Math.floor(Math.random() * 12));
        s.risk_level = 'HIGH';
        s.impact_score = Math.min(s.impact_score + 25, 100);
        s.is_affected = true;
      }
    });
    // Make some fleet idle
    state.fleet.forEach(a => {
      if (a.location === 'Delhi' && a.status === 'IN_TRANSIT') {
        a.status = 'IDLE';
        a.utilisation_pct = Math.max(0, a.utilisation_pct - 40);
      }
    });
  } else if (scenario === 'port_strike') {
    // Chennai port — affect all vessel routes from/to Chennai
    ships.forEach(s => {
      if ((s.origin === 'Chennai' || s.destination === 'Chennai') && s.vehicle_id && s.vehicle_id.startsWith('V')) {
        s.status = 'DISRUPTED';
        s.expected_delay_hours = Math.max(s.expected_delay_hours, 48 + Math.floor(Math.random() * 48));
        s.risk_level = 'CRITICAL';
        s.impact_score = Math.min(s.impact_score + 40, 100);
        s.is_affected = true;
      }
    });
    state.fleet.forEach(a => {
      if (a.location === 'Chennai' && a.type === 'VESSEL') {
        a.status = 'IDLE';
        a.utilisation_pct = 0;
      }
    });
  } else if (scenario === 'carrier_unavailable') {
    // PrimeFreight = C01 — all C01 shipments become AT_RISK
    ships.forEach(s => {
      if (s.carrier === 'C01') {
        s.status = s.status === 'ON_TIME' ? 'AT_RISK' : s.status;
        s.risk_level = s.risk_level === 'LOW' ? 'MEDIUM' : 'HIGH';
        s.impact_score = Math.min(s.impact_score + 20, 100);
        s.is_affected = true;
      }
    });
    // Redeploy some idle fleet
    state.fleet.forEach(a => {
      if (a.status === 'IDLE') {
        a.status = 'IN_TRANSIT';
        a.utilisation_pct = 70 + Math.floor(Math.random() * 20);
      }
    });
  } else if (scenario === 'demand_increase') {
    // Mumbai-Delhi corridor R01, R11 — push all to HIGH risk
    ships.forEach(s => {
      if (['R01','R11'].includes(s.current_route)) {
        s.risk_level = 'HIGH';
        s.impact_score = Math.min(s.impact_score + 15, 100);
      }
    });
    // All trucks near full capacity
    state.fleet.forEach(a => {
      if (a.type === 'TRUCK') {
        a.utilisation_pct = Math.min(95, a.utilisation_pct + 25);
      }
    });
  } else if (scenario === 'temperature_excursion') {
    // SH1016 becomes CRITICAL cold chain
    const s = ships.find(x => x.id === 'SH1016');
    if (s) {
      s.status = 'DISRUPTED';
      s.risk_level = 'CRITICAL';
      s.impact_score = 95;
      s.expected_delay_hours = Math.max(s.expected_delay_hours, 24);
      s.is_affected = true;
    }
  }
}

function renderSimImpactSummary(scenario) {
  const el = document.getElementById('simImpactList');
  if (!el) return;

  const impactMap = {
    weather_disruption: ['Routes R01, R05, R08 affected — expect 18–42h delays', 'Gujarat corridor shipments rerouted via alternative highways', 'Cold-chain shipments on affected routes flagged for monitoring'],
    road_closure: ['Delhi–Jaipur highway (R06) closed — vehicles rerouted via R10', 'Fleet assets in Delhi area idled awaiting rerouting orders', 'Estimated impact: 12–24h additional transit time on affected routes'],
    port_strike: ['Chennai port operations suspended — all vessel departures halted', 'Container and vessel fleet in Chennai marked idle', 'Sea freight affected: expect 48–96h delays on Chennai-origin shipments'],
    carrier_unavailable: ['PrimeFreight (C01) fleet recalled — shipments at risk', 'Alternative carriers being evaluated for affected shipments', 'Idle fleet assets being considered for redeployment'],
    demand_increase: ['5 urgent shipments added to Mumbai–Delhi corridor', 'Truck fleet utilisation near capacity — limited slack available', 'Consider activating standby carriers for overflow capacity'],
    temperature_excursion: ['SH1016 vaccine shipment: critical temperature breach detected', 'Cold-chain integrity compromised — immediate intervention required', 'Notify quality control and destination pharmacy immediately']
  };

  const impacts = impactMap[scenario] || [];
  if (!impacts.length) return;

  el.innerHTML = `
    <div style="margin-top:14px; padding-top:14px; border-top:1px solid var(--border)">
      <strong style="font-size:12px; color:var(--muted); text-transform:uppercase; letter-spacing:0.5px">Impact Summary</strong>
      ${impacts.map(i => `<div class="sim-impact-item">⚡ ${i}</div>`).join('')}
    </div>`;
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
      <div class="bob-avatar bob-avatar-user">U</div>
      <div class="bob-bubble bob-bubble-user">${escapeHtml(q)}</div>
    </div>`;

  // Thinking indicator
  const thinkId = 'bob-think-' + Date.now();
  chat.innerHTML += `
    <div class="bob-msg" id="${thinkId}">
      <div class="bob-avatar">✦</div>
      <div class="bob-bubble bob-thinking">
        <span class="bob-dot"></span><span class="bob-dot"></span><span class="bob-dot"></span>
        Analysing supply chain data...
      </div>
    </div>`;
  chat.scrollTop = chat.scrollHeight;

  try {
    const resp = await apiPost('/api/bob/query', { question: q });
    const thinking = document.getElementById(thinkId);
    if (thinking) thinking.remove();
    chat.innerHTML += `
      <div class="bob-msg">
        <div class="bob-avatar">✦</div>
        <div class="bob-bubble">${escapeHtml(resp.answer)}</div>
      </div>`;
  } catch(e) {
    const thinking = document.getElementById(thinkId);
    if (thinking) thinking.remove();
    chat.innerHTML += `
      <div class="bob-msg">
        <div class="bob-avatar">✦</div>
        <div class="bob-bubble bob-bubble-error">Error connecting to backend: ${escapeHtml(e.message)}</div>
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

function recItemHtml(r, clickable) {
  const shipmentId = r.target || '';
  const clickAttr = clickable && shipmentId
    ? `style="cursor:pointer" onclick="goToShipment('${shipmentId}')" title="Click to view shipment and take action"`
    : '';
  return `
    <div class="rec-item priority-${r.priority}" ${clickAttr}>
      <div class="rec-icon">${recTypeIcon(r.type)}</div>
      <div class="rec-content">
        <div class="rec-action">${r.action}</div>
        <div class="rec-reason">${r.reason}</div>
        ${r.expected_impact ? `<div class="rec-impact">${r.expected_impact}</div>` : ''}
        ${clickable && shipmentId ? `<div style="font-size:11px; color:var(--accent); margin-top:4px; font-weight:600;">→ Click to approve/reject in Shipments tab</div>` : ''}
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

// ── DRIVER REQUESTS (Admin) ──────────────────────────────────────────────────
function updatePendingBadge() {
  const pending = state.driverRequests.filter(r => r.status === 'PENDING').length;
  const badge = document.getElementById('pendingReqBadge');
  if (badge) {
    badge.textContent = pending;
    badge.style.display = pending > 0 ? 'inline-block' : 'none';
  }
}

function filterDriverReqs(filter, btn) {
  document.querySelectorAll('#section-driverrequests .filter-btn').forEach(b => b.classList.remove('active'));
  if (btn) btn.classList.add('active');
  state.driverReqFilter = filter;
  renderDriverRequests();
}

function renderDriverRequests() {
  let reqs = state.driverRequests;
  if (state.driverReqFilter !== 'all') {
    reqs = reqs.filter(r => r.status === state.driverReqFilter);
  }

  const container = document.getElementById('driverRequestsContainer');
  if (!container) return;

  if (!reqs.length) {
    container.innerHTML = '<p class="text-muted" style="padding:16px 0">No driver requests found.</p>';
    return;
  }

  container.innerHTML = reqs.map(r => {
    const cls = r.status === 'PENDING' ? 'pending-req' : r.status === 'APPROVED' ? 'approved-req' : 'rejected-req';
    const statusColor = r.status === 'APPROVED' ? 'SUCCESS' : r.status === 'REJECTED' ? 'DISRUPTED' : 'AT_RISK';
    return `
    <div class="driver-req-card ${cls}">
      <div class="req-card-header-row">
        <span class="req-card-id-big">${r.id}</span>
        <div class="req-card-badges">
          <span class="badge badge-${statusColor}">${r.status}</span>
          <span class="badge" style="background:#f1f5f9;color:#475569;">${r.request_type}</span>
        </div>
      </div>
      <div class="req-driver-info">👤 ${r.driver_name || r.driver_id} &nbsp;·&nbsp; ${r.driver_id}</div>
      <div class="req-shipment-info">📦 Shipment: <strong>${r.shipment_id}</strong> &nbsp;|&nbsp; 🕐 ${r.created_at}</div>
      ${r.location ? `<div class="req-location-tag">📍 ${escapeHtml(r.location)}</div>` : ''}
      <div class="req-message-box">${escapeHtml(r.message)}</div>
      ${r.status === 'PENDING' ? `
        <div class="req-action-row">
          <button class="btn-approve-req" onclick="approveDriverRequest('${escapeAttr(r.id)}')">✓ Approve</button>
          <button class="btn-reject-req"  onclick="openRejectDialog('${escapeAttr(r.id)}')">✕ Reject</button>
          <button class="btn-sm" onclick="openShipmentTimelineById('${escapeAttr(r.shipment_id)}')">🗺 View Timeline</button>
        </div>` : `
        <div class="req-action-row">
          <button class="btn-sm" onclick="openShipmentTimelineById('${escapeAttr(r.shipment_id)}')">🗺 View Timeline</button>
        </div>`}
      ${r.admin_response ? `
        <div class="req-response-box ${r.status === 'REJECTED' ? 'rejected-resp' : ''}">
          <strong>Admin response:</strong> ${escapeHtml(r.admin_response)}
          ${r.updated_at ? `<span style="font-size:10px;color:#94a3b8;margin-left:6px">${r.updated_at}</span>` : ''}
        </div>` : ''}
    </div>`;
  }).join('');
}

async function approveDriverRequest(requestId) {
  try {
    const result = await apiPost('/api/driver-requests/' + requestId + '/approve',
      { admin_response: 'Request approved by operations team.' });
    if (result.status === 'ok') {
      await refreshDriverAndShipments();
    }
  } catch(e) { alert('Error: ' + e.message); }
}

function openRejectDialog(requestId) {
  document.getElementById('rejectRequestId').value = requestId;
  document.getElementById('rejectReason').value = '';
  document.getElementById('rejectDialog').classList.add('active');
}

function closeRejectDialog() {
  document.getElementById('rejectDialog').classList.remove('active');
}

async function confirmReject() {
  const requestId = document.getElementById('rejectRequestId').value;
  const reason = document.getElementById('rejectReason').value.trim() || 'Request rejected.';
  closeRejectDialog();
  try {
    const result = await apiPost('/api/driver-requests/' + requestId + '/reject',
      { admin_response: reason });
    if (result.status === 'ok') {
      await refreshDriverAndShipments();
    }
  } catch(e) { alert('Error: ' + e.message); }
}

// Refresh driver requests + shipments after approve/reject
async function refreshDriverAndShipments() {
  try {
    const [reqs, ships] = await Promise.all([
      apiGet('/api/driver-requests'),
      apiGet('/api/shipments')
    ]);
    state.driverRequests = reqs;
    state.allShipments = ships;
    renderDriverRequests();
    renderShipments();
    updatePendingBadge();
  } catch(e) { console.error('Refresh error:', e); }
}

// ── SHIPMENT TIMELINE MODAL ──────────────────────────────────────────────────
function openShipmentTimeline() {
  const id = state.currentDetailShipmentId;
  if (!id) return;
  openShipmentTimelineById(id);
}

async function openShipmentTimelineById(shipmentId) {
  if (!shipmentId) return;
  const modal = document.getElementById('timelineModal');
  const content = document.getElementById('timelineModalContent');
  modal.classList.add('active');
  content.innerHTML = '<p class="text-muted" style="padding:20px">Loading timeline for ' + shipmentId + '...</p>';

  try {
    const tl = await apiGet('/api/shipments/' + shipmentId + '/timeline');
    content.innerHTML = buildTimelineHtml(tl);
    // Load verification data asynchronously after HTML is in DOM
    await loadVerifAfterTimeline(tl);
  } catch(e) {
    content.innerHTML = '<p style="color:var(--danger);padding:20px">Failed to load timeline: ' + e.message + '</p>';
  }
}

function closeTimelineModal() {
  document.getElementById('timelineModal').classList.remove('active');
}

function buildTimelineHtml(tl) {
  const checkpoints = tl.checkpoints || [];
  const disEvents   = tl.disruption_events || [];
  const reroutes    = tl.reroutes || [];
  const loc         = tl.current_location || {};
  const drRequests  = tl.driver_requests  || [];

  // Derive transport mode from carrier_id and fleet_asset embedded in timeline data
  const tMode = transportMode(tl.fleet_asset || '', tl.carrier || '');
  const tLabel = transportModeLabel(tMode);
  const tIcon  = tMode === 'SEA' ? '🚢' : tMode === 'RAIL' ? '🚂' : tMode === 'AIR' ? '✈' : '🚛';

  // If no checkpoints, build synthetic ones from origin → destination
  let sortedCPs = checkpoints.slice().sort((a,b) => parseInt(a.sequence)-parseInt(b.sequence));
  if (sortedCPs.length === 0 && tl.origin && tl.destination) {
    sortedCPs = buildSyntheticCheckpoints(tl);
  }

  let html = `<div style="padding:20px;">
    <div style="margin-bottom:16px;">
      <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;">
        <h3 style="font-size:16px;font-weight:700;">Shipment ${tl.shipment_id}</h3>
        <span style="font-size:12px;font-weight:600;padding:4px 10px;border-radius:6px;background:${tMode==='SEA'?'#0e4166':tMode==='RAIL'?'#3b1f6e':tMode==='AIR'?'#1a3a1a':'#1a2a3a'};color:${tMode==='SEA'?'#38bdf8':tMode==='RAIL'?'#c4b5fd':tMode==='AIR'?'#4ade80':'#60a5fa'}">${tIcon} ${tLabel}</span>
      </div>
      <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:10px;background:var(--surface2);border:1px solid var(--border);border-radius:8px;padding:12px;">
        <div><div style="font-size:10px;color:var(--muted);margin-bottom:2px;">ORIGIN</div><strong>${tl.origin||'—'}</strong></div>
        <div><div style="font-size:10px;color:var(--muted);margin-bottom:2px;">DESTINATION</div><strong>${tl.destination||'—'}</strong></div>
        <div><div style="font-size:10px;color:var(--muted);margin-bottom:2px;">DEPARTURE</div>${tl.planned_departure||'—'}</div>
        <div><div style="font-size:10px;color:var(--muted);margin-bottom:2px;">CURRENT ETA</div><strong style="color:${tl.expected_delay_hours>0?'var(--warning)':'var(--success)'}">${tl.current_eta||tl.planned_arrival||'—'}</strong></div>
        <div><div style="font-size:10px;color:var(--muted);margin-bottom:2px;">STATUS</div><span class="badge badge-${tl.status||''}">${tl.status||'—'}</span></div>
        ${tl.expected_delay_hours>0?`<div><div style="font-size:10px;color:var(--muted);margin-bottom:2px;">DELAY</div><strong style="color:var(--danger)">+${tl.expected_delay_hours}h</strong></div>`:''}
      </div>
    </div>`;

  if (loc.available) {
    html += `<div class="tl-cur-location">
      <div class="tl-cur-location-label">📍 CURRENT LOCATION</div>
      <div class="tl-cur-location-name">${loc.location_name||'Unknown'}</div>
      <div class="tl-cur-location-meta">Last update: ${loc.timestamp||'—'} · Source: ${loc.accuracy_status||'—'}</div>
    </div>`;
  }

  // Special SEA/AIR mode message
  if (tMode === 'SEA') {
    html += `<div style="background:#0e2a40;border:1px solid #0369a1;border-radius:8px;padding:10px 14px;margin-bottom:14px;font-size:12px;color:#38bdf8;">
      🚢 <strong>Sea Freight Shipment</strong> — This shipment is transported by vessel. Port of loading: ${tl.origin}. Port of discharge: ${tl.destination}.
      ${disEvents.length > 0 ? ' ⚠ Active maritime disruption detected on this route.' : ''}
    </div>`;
  } else if (tMode === 'AIR') {
    html += `<div style="background:#0a2a0a;border:1px solid #166534;border-radius:8px;padding:10px 14px;margin-bottom:14px;font-size:12px;color:#4ade80;">
      ✈ <strong>Air Freight Shipment</strong> — Transported by air cargo. Origin airport: ${tl.origin}. Destination airport: ${tl.destination}.
    </div>`;
  }

  html += '<div class="tl-section-label">Journey Timeline</div>';

  // Build merged event sequence: checkpoints interleaved with disruption events and reroutes
  // Disruption events inject between the checkpoint where disruption occurs and the next one
  sortedCPs.forEach((cp, idx) => {
    const dotClass = tlDotClass(cp.status);
    const icon     = tlIcon(cp.status);
    const badgeCls = 'tlb-' + cp.status.toLowerCase();

    // Are there disruptions/reroutes that follow this checkpoint?
    const cpHasDisruption = cp.status === 'DISRUPTED' || cp.status === 'REROUTED';
    const afterDisruption = disEvents.length > 0 && cpHasDisruption;
    const afterReroute    = reroutes.length > 0 && cpHasDisruption;
    const isLast   = idx === sortedCPs.length - 1;
    const hasMore  = !isLast || afterDisruption || afterReroute;

    html += tlItem(dotClass, icon, cp.name,
      `<div class="tl-times">
        ${cp.expected_arrival ? `<div class="tl-time"><strong>Expected:</strong> ${cp.expected_arrival}</div>` : ''}
        ${cp.actual_arrival   ? `<div class="tl-time"><strong>Arrived:</strong> ${cp.actual_arrival}</div>`   : ''}
        ${cp.departure_time   ? `<div class="tl-time"><strong>Departed:</strong> ${cp.departure_time}</div>`   : ''}
      </div>
      <span class="tl-badge ${badgeCls}">${cp.status}</span>`,
      hasMore);

    // Inject disruption events and reroutes right after the disrupted checkpoint
    if (cpHasDisruption) {
      disEvents.forEach((de, di) => {
        const hasMoreAfterDis = reroutes.length > 0 || !isLast || di < disEvents.length - 1;
        html += tlItem('tl-dot-dis-event', '⚠', `<span class="tl-name-dis">⚠ DISRUPTION AT ${de.location}</span>`,
          `<div class="tl-times">
            <div class="tl-time"><strong>Type:</strong> ${de.type}</div>
            <div class="tl-time"><strong>Severity:</strong> <span style="color:${de.severity==='HIGH'||de.severity==='CRITICAL'?'#f87171':'#fbbf24'}">${de.severity}</span></div>
            <div class="tl-time"><strong>Detected:</strong> ${de.detected_at}</div>
          </div>
          <div class="tl-time" style="margin-bottom:6px;color:#f87171;font-weight:600;">📍 Disruption Point: ${de.location}</div>
          <div class="tl-desc">${escapeHtml(de.description)}</div>
          ${buildVerifInlineHtml(de.id)}`,
          hasMoreAfterDis);
      });
      reroutes.forEach((rr, ri) => {
        const hasMoreAfterRR = !isLast || ri < reroutes.length - 1;
        html += tlItem('tl-dot-rr-event', '↪', '<span class="tl-name-rr">↪ REROUTE APPLIED FROM DISRUPTION POINT</span>',
          `<div style="font-size:11px;color:#94a3b8;margin-bottom:6px;">Rerouted from: <strong style="color:#e8eaf0">${rr.from_location}</strong> (where disruption detected)</div>
          <div class="tl-times">
            <div class="tl-time"><strong>Disruption at:</strong> ${rr.disruption_location}</div>
            <div class="tl-time"><strong>Applied:</strong> ${rr.created_at}</div>
          </div>
          <div style="margin:6px 0 4px;font-size:11px;color:#94a3b8;">New alternate route:</div>
          <div class="tl-desc" style="color:#60a5fa;font-weight:600;">${escapeHtml(rr.alternate_route)}</div>
          ${rr.new_eta ? `<div style="margin-top:6px;font-size:12px;font-weight:700;color:var(--warning);">🕐 New ETA: ${rr.new_eta}</div>` : ''}`,
          hasMoreAfterRR || !isLast);
      });
    }
  });

  if (sortedCPs.length === 0) {
    html += `<div style="padding:20px;text-align:center;color:var(--muted);font-size:13px;">No checkpoint data available.</div>`;
  }

  html += '</div>';

  // Driver Requests section
  if (drRequests.length) {
    html += `<div style="padding:0 20px 16px;">
      <div class="tl-section-label">Driver Requests</div>`;
    drRequests.forEach(r => {
      const scls = r.status === 'APPROVED' ? 'SUCCESS' : r.status === 'REJECTED' ? 'DISRUPTED' : 'AT_RISK';
      html += `<div style="background:var(--surface2);border:1px solid var(--border);border-radius:6px;padding:10px 12px;margin-bottom:8px;">
        <div style="display:flex;justify-content:space-between;margin-bottom:6px;">
          <strong>${r.id}</strong>
          <span class="badge badge-${scls}">${r.status}</span>
        </div>
        <div style="font-size:12px;color:var(--muted);margin-bottom:4px;">
          👤 ${r.driver_name||r.driver_id} &nbsp;·&nbsp; ${r.request_type} &nbsp;·&nbsp; ${r.created_at}
        </div>
        <div style="font-size:12px;color:var(--text)">${escapeHtml(r.message)}</div>
        ${r.admin_response ? `<div style="margin-top:6px;font-size:11px;color:var(--muted);border-left:3px solid ${r.status==='APPROVED'?'var(--success)':'var(--danger)'};padding-left:8px;">
          <strong>Admin:</strong> ${escapeHtml(r.admin_response)}
        </div>` : ''}
      </div>`;
    });
    html += '</div>';
  }

  html += '</div>';
  return html;
}

// Generate synthetic checkpoints for shipments with no checkpoint data
function buildSyntheticCheckpoints(tl) {
  const status = tl.status || 'ON_TIME';
  const cpStatus = status === 'ON_TIME' ? 'UPCOMING' :
                   status === 'DISRUPTED' ? 'DISRUPTED' :
                   status === 'DELAYED'   ? 'CURRENT'  : 'UPCOMING';
  return [
    { name: tl.origin, sequence: '1', expected_arrival: tl.planned_departure || '', actual_arrival: tl.planned_departure || '', departure_time: tl.planned_departure || '', status: 'COMPLETED' },
    { name: 'En Route', sequence: '2', expected_arrival: '', actual_arrival: '', departure_time: '', status: cpStatus === 'DISRUPTED' ? 'DISRUPTED' : 'CURRENT' },
    { name: tl.destination, sequence: '3', expected_arrival: tl.current_eta || tl.planned_arrival || '', actual_arrival: '', departure_time: '', status: 'UPCOMING' }
  ];
}

// Derive transport mode from fleet asset prefix + carrier prefix
function transportMode(fleetAsset, carrierId) {
  if (!fleetAsset && !carrierId) return 'ROAD';
  const fa = (fleetAsset || '').toUpperCase();
  if (fa.startsWith('V')) return 'SEA';          // Vessel
  if (fa.startsWith('CN')) return 'RAIL';         // Container/rail
  // Carrier-based fallback
  const c = (carrierId || '').toUpperCase();
  if (c === 'C03' || c === 'C09') return 'SEA';  // OceanWave, IndoShip
  if (c === 'C05') return 'RAIL';                 // RailFreight India
  if (c === 'C04') return 'AIR';                  // AirCargo Express
  return 'ROAD';
}

function transportModeLabel(mode) {
  return mode === 'SEA'  ? '🚢 Sea Freight'  :
         mode === 'RAIL' ? '🚂 Rail Freight'  :
         mode === 'AIR'  ? '✈ Air Freight'    :
                           '🚛 Road Freight';
}

// Build inline verification box by loading synchronously from pre-fetched data
function buildVerifInlineHtml(disruptionEventId) {
  // This returns a placeholder that gets filled asynchronously
  return `<div id="verif-${disruptionEventId}" class="verif-box">
    <div class="verif-title">🔍 Disruption Verification</div>
    <div style="font-size:12px;color:var(--muted)">Loading verification data...</div>
  </div>`;
}

// Called after timeline modal is populated to load verif data async
async function loadVerificationForDisruption(disruptionEventId) {
  const el = document.getElementById('verif-' + disruptionEventId);
  if (!el) return;
  try {
    const v = await apiGet('/api/disruptions/' + disruptionEventId + '/verification');
    let statusHtml;
    if (v.verification_status === 'MULTI_SOURCE')
      statusHtml = `<span class="verif-status-multi">✓ ${v.verification_label}</span>`;
    else if (v.verification_status === 'SINGLE_SOURCE')
      statusHtml = `<span class="verif-status-single">⚠ ${v.verification_label}</span>`;
    else
      statusHtml = `<span class="verif-status-none">✕ ${v.verification_label}</span>`;

    const sourcesHtml = (v.sources || []).map(s => `
      <div class="verif-source">
        <div>
          <span class="verif-source-name">${escapeHtml(s.source_name)}</span>
          <span class="verif-source-type vst-${s.source_type.toLowerCase()}">${s.source_type}</span>
        </div>
        <div class="verif-source-summary">${escapeHtml(s.summary)}</div>
        <div class="verif-source-time">Updated: ${s.published_at} &nbsp;·&nbsp; Status: ${s.status}</div>
      </div>`).join('');

    el.innerHTML = `<div class="verif-title">🔍 Disruption Verification</div>
      ${statusHtml}
      <div style="font-size:11px;color:var(--muted);margin-bottom:8px;">
        ${v.supported_sources} supporting source(s) of ${v.total_sources} found
      </div>
      ${sourcesHtml || '<div style="font-size:11px;color:var(--muted)">No external verification sources available.</div>'}`;
  } catch(e) {
    el.innerHTML = '<div class="verif-title">🔍 Disruption Verification</div><div style="font-size:11px;color:var(--muted)">External verification unavailable.</div>';
  }
}

async function loadVerifAfterTimeline(tl) {
  const disEvents = tl.disruption_events || [];
  for (const de of disEvents) {
    await loadVerificationForDisruption(de.id);
  }
}

// ── Timeline helper functions ─────────────────────────────────────────────────
function tlDotClass(st) {
  const m = {COMPLETED:'tl-dot-completed',CURRENT:'tl-dot-current',UPCOMING:'tl-dot-upcoming',
             DISRUPTED:'tl-dot-disrupted',REROUTED:'tl-dot-rerouted',SKIPPED:'tl-dot-upcoming'};
  return 'tl-dot ' + (m[st] || 'tl-dot-upcoming');
}
function tlIcon(st) {
  const m = {COMPLETED:'✓',CURRENT:'📍',UPCOMING:'○',DISRUPTED:'⚠',REROUTED:'↪',SKIPPED:'—'};
  return m[st] || '○';
}
function tlItem(dotClass, icon, name, bodyHtml, hasLine) {
  return `<div class="tl-item">
    <div class="tl-spine">
      <div class="${dotClass}">${icon}</div>
      ${hasLine ? '<div class="tl-line"></div>' : ''}
    </div>
    <div class="tl-content">
      <div class="tl-name">${name}</div>
      ${bodyHtml}
    </div>
  </div>`;
}

// ── Start ─────────────────────────────────────────────────────────────────────
window.addEventListener('DOMContentLoaded', init);
