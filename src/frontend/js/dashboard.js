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

  panel.style.display = 'flex';
  document.getElementById('detailContent').innerHTML = `
    <div class="detail-section">
      <div class="detail-section-title">📦 Shipment Info</div>
      <div class="detail-grid">
        <div class="detail-field"><label>Origin</label><span>${s.origin}</span></div>
        <div class="detail-field"><label>Destination</label><span>${s.destination}</span></div>
        <div class="detail-field"><label>Carrier</label><span>${s.carrier}</span></div>
        <div class="detail-field"><label>Route</label><span>${s.current_route}</span></div>
        <div class="detail-field"><label>Priority</label><span class="badge badge-${s.priority}">${s.priority}</span></div>
        <div class="detail-field"><label>Status</label><span class="badge badge-${s.status}">${s.status}</span></div>
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

// ── Start ─────────────────────────────────────────────────────────────────────
window.addEventListener('DOMContentLoaded', init);
