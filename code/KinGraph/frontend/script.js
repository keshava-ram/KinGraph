/**
 * KinGraph — Frontend Interactive Engine
 * Integrates directly with the C Backend API via fetch()
 * Manual SVG layout, pan/zoom, graph traversal highlighting, and real-time DSA telemetry.
 */

const API_BASE = window.location.origin;

// State management
const state = {
    activeTab: 'home',
    people: [],
    parentChildEdges: [],
    spouseEdges: [],
    selectedPersonId: null,
    highlightMode: null, // 'ancestors' | 'descendants' | null
    highlightedNodeIds: new Set(),
    highlightedEdgeKeys: new Set(),
    zoom: 1,
    panX: 0,
    panY: 0,
    isPanning: false,
    startX: 0,
    startY: 0
};

// ================= INITIALIZATION =================
document.addEventListener('DOMContentLoaded', () => {
    initTabs();
    initSvgControls();
    loadAllData();
});

function initTabs() {
    const navButtons = document.querySelectorAll('.nav-btn');
    navButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const tabName = btn.dataset.tab;
            switchTab(tabName);
        });
    });
}

function switchTab(tabName) {
    state.activeTab = tabName;
    document.querySelectorAll('.nav-btn').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.tab === tabName);
    });
    document.querySelectorAll('.tab-pane').forEach(pane => {
        pane.classList.toggle('active', pane.id === `tab-${tabName}`);
    });

    if (tabName === 'tree') {
        setTimeout(() => fitView(), 50);
    } else if (tabName === 'dashboard') {
        fetchStats();
    }
}

// ================= DATA FETCHING =================
async function loadAllData() {
    try {
        await Promise.all([
            fetchFamilyTree(),
            fetchStats()
        ]);
        updateServerStatus(true);
    } catch (err) {
        console.error('Failed to communicate with C backend:', err);
        updateServerStatus(false);
        showToast('Unable to reach KinGraph C server at ' + API_BASE, 'error');
    }
}

function updateServerStatus(online) {
    const el = document.getElementById('serverStatusIndicator');
    if (!el) return;
    if (online) {
        el.className = 'server-status';
        el.innerHTML = '<span class="status-dot"></span><span class="status-label">C Server: Online</span>';
    } else {
        el.className = 'server-status server-offline';
        el.innerHTML = '<span class="status-dot" style="background:#ef4444;box-shadow:0 0 8px #ef4444;"></span><span class="status-label" style="color:#ef4444;">C Server: Offline</span>';
    }
}

async function fetchFamilyTree() {
    const res = await fetch(`${API_BASE}/api/family`);
    if (!res.ok) throw new Error('API /api/family error');
    const data = await res.json();

    state.people = data.people || [];
    state.parentChildEdges = data.parent_child_edges || [];
    state.spouseEdges = data.spouse_edges || [];

    // Populate dropdowns, roster table, and render SVG tree
    populatePersonDropdowns();
    renderRosterTable();
    renderTreeSvg();
    renderHeroPreview();
}

async function fetchStats() {
    try {
        const res = await fetch(`${API_BASE}/api/stats`);
        if (!res.ok) return;
        const data = await res.json();

        document.getElementById('statMembers').innerText = data.total_people || 0;
        document.getElementById('statGenerations').innerText = data.generations_count || 1;
        document.getElementById('statEdges').innerText = data.total_parent_child || 0;
        document.getElementById('statSpouses').innerText = data.total_spouses || 0;

        const heroCount = document.getElementById('heroNodeCount');
        if (heroCount) {
            heroCount.innerText = `${data.total_people} Vertices • ${(data.total_parent_child || 0) + (data.total_spouses || 0)} Edges`;
        }

        if (data.dsa_metrics) {
            document.getElementById('telCapacity').innerText = data.dsa_metrics.hash_table_capacity;
            document.getElementById('telSize').innerText = data.dsa_metrics.hash_table_size;
            document.getElementById('telCollisions').innerText = data.dsa_metrics.hash_collisions;
            document.getElementById('telMaxChain').innerText = data.dsa_metrics.hash_max_chain;
            document.getElementById('telLoadFactor').innerText = data.dsa_metrics.hash_load_factor.toFixed(3);
        }
    } catch (e) {
        console.warn('Stats fetch skipped:', e);
    }
}

// ================= SVG FAMILY TREE RENDERER =================
function renderTreeSvg() {
    const group = document.getElementById('treeGroup');
    if (!group) return;
    group.innerHTML = '';

    if (state.people.length === 0) {
        group.innerHTML = '<text x="100" y="100" fill="#94a3b8" font-size="16">No family data recorded. Click "+ Member" to create.</text>';
        return;
    }

    // Node layout constants
    const cardWidth = 180;
    const cardHeight = 85;
    const ySpacing = 160;
    const xSpacing = 220;

    // 1. Group people by generation
    const genMap = new Map();
    state.people.forEach(p => {
        const gen = p.generation || 1;
        if (!genMap.has(gen)) genMap.set(gen, []);
        genMap.get(gen).push(p);
    });

    // 2. Compute (x, y) coordinates for each person
    const positions = new Map();
    const sortedGens = Array.from(genMap.keys()).sort((a, b) => a - b);

    // Group spouses side-by-side in each generation
    sortedGens.forEach((gen, gIdx) => {
        const list = genMap.get(gen);
        const y = 60 + gIdx * ySpacing;

        // Order spouses adjacent
        const ordered = [];
        const visitedInGen = new Set();

        list.forEach(p => {
            if (visitedInGen.has(p.id)) return;
            ordered.push(p);
            visitedInGen.add(p.id);

            // Find spouse in same generation
            const spEdge = state.spouseEdges.find(s => 
                (s.person_a === p.id && list.some(x => x.id === s.person_b)) ||
                (s.person_b === p.id && list.some(x => x.id === s.person_a))
            );
            if (spEdge) {
                const spId = spEdge.person_a === p.id ? spEdge.person_b : spEdge.person_a;
                const spPerson = list.find(x => x.id === spId);
                if (spPerson && !visitedInGen.has(spId)) {
                    ordered.push(spPerson);
                    visitedInGen.add(spId);
                }
            }
        });

        const totalWidth = (ordered.length - 1) * xSpacing;
        const startX = Math.max(100, 500 - totalWidth / 2);

        ordered.forEach((p, idx) => {
            positions.set(p.id, {
                x: startX + idx * xSpacing,
                y: y
            });
        });
    });

    // 3. Render Generation marker lines
    sortedGens.forEach((gen, gIdx) => {
        const y = 60 + gIdx * ySpacing;
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', '20');
        text.setAttribute('y', y + cardHeight / 2 + 5);
        text.setAttribute('fill', '#475569');
        text.setAttribute('font-size', '12');
        text.setAttribute('font-weight', '700');
        text.setAttribute('font-family', 'var(--font-mono)');
        text.textContent = `GEN ${gen}`;
        group.appendChild(text);
    });

    // 4. Render Parent-Child Edges (Bezier curves)
    state.parentChildEdges.forEach(edge => {
        const pPos = positions.get(edge.parent_id);
        const cPos = positions.get(edge.child_id);
        if (!pPos || !cPos) return;

        const x1 = pPos.x + cardWidth / 2;
        const y1 = pPos.y + cardHeight;
        const x2 = cPos.x + cardWidth / 2;
        const y2 = cPos.y;

        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        const cy1 = y1 + 45;
        const cy2 = y2 - 45;
        path.setAttribute('d', `M ${x1} ${y1} C ${x1} ${cy1}, ${x2} ${cy2}, ${x2} ${y2}`);
        path.setAttribute('class', 'tree-edge edge-parent-child');
        path.setAttribute('marker-end', 'url(#arrowhead)');

        const edgeKey = `${edge.parent_id}->${edge.child_id}`;
        if (state.highlightedEdgeKeys.has(edgeKey)) {
            if (state.highlightMode === 'ancestors') {
                path.classList.add('highlight-ancestor');
                path.setAttribute('marker-end', 'url(#arrowhead-ancestor)');
            } else if (state.highlightMode === 'descendants') {
                path.classList.add('highlight-descendant');
                path.setAttribute('marker-end', 'url(#arrowhead-descendant)');
            }
        }
        group.appendChild(path);
    });

    // 5. Render Spouse Edges (Horizontal dashed line)
    state.spouseEdges.forEach(edge => {
        const posA = positions.get(edge.person_a);
        const posB = positions.get(edge.person_b);
        if (!posA || !posB) return;

        const isALeft = posA.x < posB.x;
        const x1 = isALeft ? posA.x + cardWidth : posA.x;
        const y1 = posA.y + cardHeight / 2;
        const x2 = isALeft ? posB.x : posB.x + cardWidth;
        const y2 = posB.y + cardHeight / 2;

        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        path.setAttribute('d', `M ${x1} ${y1} L ${x2} ${y2}`);
        path.setAttribute('class', 'tree-edge edge-spouse');
        group.appendChild(path);
    });

    // 6. Render Person Node Cards
    state.people.forEach(person => {
        const pos = positions.get(person.id);
        if (!pos) return;

        const isSelected = state.selectedPersonId === person.id;
        const isHighlighted = state.highlightedNodeIds.has(person.id);
        const hasHighlightActive = state.highlightedNodeIds.size > 0 || isSelected;

        const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
        g.setAttribute('transform', `translate(${pos.x}, ${pos.y})`);
        g.setAttribute('class', 'tree-node-card');
        g.dataset.id = person.id;

        if (isSelected) {
            g.classList.add('selected');
        } else if (isHighlighted) {
            if (state.highlightMode === 'ancestors') g.classList.add('ancestor-highlight');
            if (state.highlightMode === 'descendants') g.classList.add('descendant-highlight');
        } else if (hasHighlightActive) {
            g.classList.add('dimmed');
        }

        // Card background
        const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        rect.setAttribute('width', cardWidth);
        rect.setAttribute('height', cardHeight);
        rect.setAttribute('class', 'node-rect');
        g.appendChild(rect);

        // Header strip
        const header = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        header.setAttribute('width', cardWidth);
        header.setAttribute('height', '26');
        header.setAttribute('class', 'node-header-bg');
        g.appendChild(header);

        // ID badge
        const idText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        idText.setAttribute('x', '10');
        idText.setAttribute('y', '17');
        idText.setAttribute('class', 'node-id-badge');
        idText.textContent = person.id;
        g.appendChild(idText);

        // Gen badge
        const genText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        genText.setAttribute('x', cardWidth - 10);
        genText.setAttribute('y', '17');
        genText.setAttribute('text-anchor', 'end');
        genText.setAttribute('class', 'node-gen-badge');
        genText.textContent = `GEN ${person.generation || 1}`;
        g.appendChild(genText);

        // Name text
        const nameText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        nameText.setAttribute('x', '12');
        nameText.setAttribute('y', '48');
        nameText.setAttribute('class', 'node-name');
        nameText.textContent = truncateString(person.name, 19);
        g.appendChild(nameText);

        // Subtext: Year & Living status
        const subText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        subText.setAttribute('x', '12');
        subText.setAttribute('y', '68');
        subText.setAttribute('class', 'node-subtext');
        let yearLabel = person.birth_year ? `b. ${person.birth_year}` : '';
        if (person.death_year > 0) {
            yearLabel = `${person.birth_year || '?'} – ${person.death_year}`;
        } else if (person.is_living) {
            yearLabel += ' (Living)';
        }
        subText.textContent = yearLabel || (person.gender || 'Person');
        g.appendChild(subText);

        // Click handler to inspect
        g.addEventListener('click', (e) => {
            e.stopPropagation();
            selectPerson(person.id);
        });

        group.appendChild(g);
    });

    applyTransform();
}

function truncateString(str, max) {
    if (!str) return '';
    return str.length > max ? str.substring(0, max - 1) + '…' : str;
}

// ================= ZOOM & PAN ENGINE =================
function initSvgControls() {
    const viewport = document.getElementById('svgViewport');
    if (!viewport) return;

    viewport.addEventListener('mousedown', (e) => {
        if (e.target.closest('.tree-node-card')) return;
        state.isPanning = true;
        state.startX = e.clientX - state.panX;
        state.startY = e.clientY - state.panY;
    });

    window.addEventListener('mousemove', (e) => {
        if (!state.isPanning) return;
        state.panX = e.clientX - state.startX;
        state.panY = e.clientY - state.startY;
        applyTransform();
    });

    window.addEventListener('mouseup', () => {
        state.isPanning = false;
    });

    viewport.addEventListener('wheel', (e) => {
        e.preventDefault();
        const factor = e.deltaY < 0 ? 1.1 : 0.9;
        const newZoom = Math.min(Math.max(0.3, state.zoom * factor), 2.5);
        state.zoom = newZoom;
        applyTransform();
    }, { passive: false });
}

function applyTransform() {
    const group = document.getElementById('treeGroup');
    if (group) {
        group.setAttribute('transform', `translate(${state.panX}, ${state.panY}) scale(${state.zoom})`);
    }
}

function zoomIn() {
    state.zoom = Math.min(state.zoom * 1.2, 2.5);
    applyTransform();
}

function zoomOut() {
    state.zoom = Math.max(state.zoom / 1.2, 0.3);
    applyTransform();
}

function resetZoom() {
    state.zoom = 1;
    state.panX = 40;
    state.panY = 30;
    applyTransform();
}

function fitView() {
    resetZoom();
}

// ================= PERSON INSPECTION =================
async function selectPerson(personId) {
    state.selectedPersonId = personId;

    // Enable toolbar buttons
    const btnAnc = document.getElementById('btnHlAncestors');
    const btnDesc = document.getElementById('btnHlDescendants');
    if (btnAnc) btnAnc.disabled = false;
    if (btnDesc) btnDesc.disabled = false;

    // Fetch full details
    try {
        const res = await fetch(`${API_BASE}/api/person?id=${encodeURIComponent(personId)}`);
        if (!res.ok) throw new Error('Person not found');
        const data = await res.json();
        const p = data.person;

        document.getElementById('inspectorEmpty').style.display = 'none';
        document.getElementById('inspectorContent').style.display = 'block';

        document.getElementById('inspAvatar').innerText = p.name ? p.name.charAt(0) : '?';
        document.getElementById('inspName').innerText = p.name;
        document.getElementById('inspId').innerText = p.id;
        document.getElementById('inspGen').innerText = `Gen ${p.generation || 1}`;
        document.getElementById('inspGender').innerText = p.gender || 'Unknown';
        document.getElementById('inspBio').innerText = p.bio || 'No biography or notes on record.';
        document.getElementById('inspBirth').innerText = p.birth_year ? p.birth_year : 'Unknown';
        document.getElementById('inspStatus').innerText = p.death_year > 0 ? `Deceased (${p.death_year})` : (p.is_living ? 'Living' : 'Unknown');

        // Parents list
        const pList = document.getElementById('inspParentsList');
        document.getElementById('inspParentCount').innerText = (data.parents || []).length;
        if (!data.parents || data.parents.length === 0) {
            pList.innerHTML = '<span class="text-muted">None recorded (Root Founder)</span>';
        } else {
            pList.innerHTML = data.parents.map(par => `
                <div class="pill-item" onclick="selectPerson('${par.id}')">
                    <span>${par.name || par.id}</span>
                    <span class="badge badge-primary">${par.id}</span>
                </div>
            `).join('');
        }

        // Spouses list
        const sList = document.getElementById('inspSpouseList');
        document.getElementById('inspSpouseCount').innerText = (data.spouses || []).length;
        if (!data.spouses || data.spouses.length === 0) {
            sList.innerHTML = '<span class="text-muted">No spouse connected</span>';
        } else {
            sList.innerHTML = data.spouses.map(sp => `
                <div class="pill-item" onclick="selectPerson('${sp.id}')">
                    <span>${sp.name || sp.id}</span>
                    <span class="badge badge-purple">${sp.id}</span>
                </div>
            `).join('');
        }

        // Children list
        const cList = document.getElementById('inspChildrenList');
        document.getElementById('inspChildCount').innerText = (data.children || []).length;
        if (!data.children || data.children.length === 0) {
            cList.innerHTML = '<span class="text-muted">No descendants recorded</span>';
        } else {
            cList.innerHTML = data.children.map(ch => `
                <div class="pill-item" onclick="selectPerson('${ch.id}')">
                    <span>${ch.name || ch.id}</span>
                    <span class="badge badge-success">${ch.id}</span>
                </div>
            `).join('');
        }

        renderTreeSvg();
    } catch (err) {
        showToast('Error loading person details: ' + err.message, 'error');
    }
}

function clearTreeSelection() {
    state.selectedPersonId = null;
    state.highlightMode = null;
    state.highlightedNodeIds.clear();
    state.highlightedEdgeKeys.clear();

    const btnAnc = document.getElementById('btnHlAncestors');
    const btnDesc = document.getElementById('btnHlDescendants');
    if (btnAnc) btnAnc.disabled = true;
    if (btnDesc) btnDesc.disabled = true;

    document.getElementById('inspectorEmpty').style.display = 'block';
    document.getElementById('inspectorContent').style.display = 'none';

    renderTreeSvg();
}

// ================= ANCESTOR & DESCENDANT TRACING =================
async function toggleHighlightAncestors() {
    if (!state.selectedPersonId) return;
    await inspectAncestorsDFS();
}

async function toggleHighlightDescendants() {
    if (!state.selectedPersonId) return;
    await inspectDescendantsBFS();
}

async function inspectAncestorsDFS() {
    if (!state.selectedPersonId) return;

    try {
        const res = await fetch(`${API_BASE}/api/ancestors?id=${encodeURIComponent(state.selectedPersonId)}`);
        if (!res.ok) throw new Error('Ancestors retrieval failed');
        const data = await res.json();

        state.highlightMode = 'ancestors';
        state.highlightedNodeIds.clear();
        state.highlightedEdgeKeys.clear();

        (data.ancestors || []).forEach(a => {
            state.highlightedNodeIds.add(a.id);
        });

        // Highlight connecting parent edges
        state.parentChildEdges.forEach(e => {
            if (state.highlightedNodeIds.has(e.parent_id) && 
               (state.highlightedNodeIds.has(e.child_id) || e.child_id === state.selectedPersonId)) {
                state.highlightedEdgeKeys.add(`${e.parent_id}->${e.child_id}`);
            }
        });

        renderTreeSvg();
        displayDsaTrace(data.algorithm_used, data.dsa_trace || []);
        showToast(`Traced ${data.ancestors.length} ancestors for ${data.target_name} using Iterative DFS`, 'success');
    } catch (err) {
        showToast('Error tracing ancestors: ' + err.message, 'error');
    }
}

async function inspectDescendantsBFS() {
    if (!state.selectedPersonId) return;

    try {
        const res = await fetch(`${API_BASE}/api/descendants?id=${encodeURIComponent(state.selectedPersonId)}`);
        if (!res.ok) throw new Error('Descendants retrieval failed');
        const data = await res.json();

        state.highlightMode = 'descendants';
        state.highlightedNodeIds.clear();
        state.highlightedEdgeKeys.clear();

        (data.descendants || []).forEach(d => {
            state.highlightedNodeIds.add(d.id);
        });

        // Highlight connecting child edges
        state.parentChildEdges.forEach(e => {
            if ((state.highlightedNodeIds.has(e.parent_id) || e.parent_id === state.selectedPersonId) && 
                state.highlightedNodeIds.has(e.child_id)) {
                state.highlightedEdgeKeys.add(`${e.parent_id}->${e.child_id}`);
            }
        });

        renderTreeSvg();
        displayDsaTrace(data.algorithm_used, data.dsa_trace || []);
        showToast(`Explored ${data.descendants.length} descendants for ${data.target_name} using Queue BFS`, 'success');
    } catch (err) {
        showToast('Error exploring descendants: ' + err.message, 'error');
    }
}

function displayDsaTrace(algoName, traceLines) {
    const nameEl = document.getElementById('traceAlgoName');
    const logBox = document.getElementById('dsaTraceLog');
    if (nameEl) nameEl.innerText = algoName || 'Manual C Traversal Trace';
    if (!logBox) return;

    if (!traceLines || traceLines.length === 0) {
        logBox.innerHTML = '<div class="log-line text-muted">// No traversal steps logged.</div>';
        return;
    }

    logBox.innerHTML = traceLines.map((line, idx) => 
        `<div class="log-line"><span style="color:#64748b;">[Step ${idx+1}]</span> ${escapeHtml(line)}</div>`
    ).join('');
}

// ================= KINSHIP EXPLORER =================
function populatePersonDropdowns() {
    const selects = [
        document.getElementById('selectPersonA'),
        document.getElementById('selectPersonB'),
        document.getElementById('newParent'),
        document.getElementById('newSpouse'),
        document.getElementById('connectPersonA'),
        document.getElementById('connectPersonB')
    ];

    selects.forEach(sel => {
        if (!sel) return;
        const currentVal = sel.value;
        const defaultOption = sel.firstElementChild ? sel.firstElementChild.outerHTML : '';
        sel.innerHTML = defaultOption;

        state.people.forEach(p => {
            const opt = document.createElement('option');
            opt.value = p.id;
            opt.textContent = `${p.name} (${p.id}) [Gen ${p.generation || 1}]`;
            sel.appendChild(opt);
        });

        if (currentVal && Array.from(sel.options).some(o => o.value === currentVal)) {
            sel.value = currentVal;
        }
    });
}

function swapExplorerPeople() {
    const a = document.getElementById('selectPersonA');
    const b = document.getElementById('selectPersonB');
    if (a && b) {
        const temp = a.value;
        a.value = b.value;
        b.value = temp;
    }
}

function exploreWithSelected() {
    if (!state.selectedPersonId) return;
    switchTab('explorer');
    const a = document.getElementById('selectPersonA');
    if (a) a.value = state.selectedPersonId;
}

async function analyzeRelationship() {
    const aId = document.getElementById('selectPersonA').value;
    const bId = document.getElementById('selectPersonB').value;

    if (!aId || !bId) {
        showToast('Please select both Person A and Person B.', 'error');
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/relationship?a=${encodeURIComponent(aId)}&b=${encodeURIComponent(bId)}`);
        if (!res.ok) throw new Error('Relationship analysis failed');
        const data = await res.json();

        const card = document.getElementById('explorerResultCard');
        card.style.display = 'block';

        document.getElementById('resRelationshipTitle').innerText = data.relationship || 'Unrelated';
        document.getElementById('resDegreeBadge').innerText = data.degree >= 0 
            ? `${data.degree} Degree${data.degree === 1 ? '' : 's'} of Kinship`
            : 'Unconnected Lineages';

        // Render timeline path
        const timeline = document.getElementById('resPathTimeline');
        if (!data.path || data.path.length === 0) {
            timeline.innerHTML = '<span class="text-muted">No connecting relationship path exists between these members.</span>';
        } else {
            timeline.innerHTML = data.path.map((step, idx) => {
                let edgeHtml = '';
                if (idx < data.path.length - 1) {
                    const nextStep = data.path[idx + 1];
                    let relName = nextStep.edge_type || 'connects to';
                    if (relName === 'parent_of') relName = 'Parent of ➔';
                    else if (relName === 'child_of') relName = 'Child of ➔';
                    else if (relName === 'spouse_of') relName = 'Spouse of ⇄';

                    edgeHtml = `<div class="path-step-edge"><span>${relName}</span></div>`;
                }
                return `
                    <div class="path-step-node" onclick="selectPersonAndInspect('${step.node_id}')">
                        <span class="step-name">${step.node_name}</span>
                        <span class="step-id">${step.node_id}</span>
                    </div>
                    ${edgeHtml}
                `;
            }).join('');
        }

        // Render common ancestors
        const caGrid = document.getElementById('resCommonAncestorsGrid');
        if (!data.common_ancestors || data.common_ancestors.length === 0) {
            caGrid.innerHTML = '<div class="col-span-full text-muted">No shared biological ancestors identified.</div>';
        } else {
            caGrid.innerHTML = data.common_ancestors.map(ca => `
                <div class="common-ancestor-card">
                    <div class="ca-name">${ca.name}</div>
                    <div class="badge badge-purple">${ca.id}</div>
                    <div class="ca-distance mt-2">
                        <span>Distance: ${ca.total_distance} steps</span><br>
                        <small class="text-muted">(${ca.distance_from_a} from A &bull; ${ca.distance_from_b} from B)</small>
                    </div>
                </div>
            `).join('');
        }

        showToast(`Analyzed kinship: ${data.person_a_name} ➔ ${data.person_b_name} = ${data.relationship}`, 'success');
    } catch (err) {
        showToast('Kinship analysis error: ' + err.message, 'error');
    }
}

function selectPersonAndInspect(id) {
    switchTab('tree');
    selectPerson(id);
}

// ================= ROSTER TABLE =================
function renderRosterTable(filterText = '') {
    const tbody = document.getElementById('rosterTableBody');
    if (!tbody) return;

    const filtered = state.people.filter(p => {
        if (!filterText) return true;
        const q = filterText.toLowerCase();
        return p.id.toLowerCase().includes(q) || p.name.toLowerCase().includes(q);
    });

    if (filtered.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="text-center text-muted">No matching members found.</td></tr>';
        return;
    }

    tbody.innerHTML = filtered.map(p => `
        <tr>
            <td><strong class="font-mono text-accent">${p.id}</strong></td>
            <td><strong>${p.name}</strong></td>
            <td>${p.gender || 'Unknown'}</td>
            <td>${p.birth_year ? p.birth_year : '?'}${p.death_year > 0 ? ' – ' + p.death_year : ''}</td>
            <td><span class="badge badge-purple">Gen ${p.generation || 1}</span></td>
            <td>${p.death_year > 0 ? '<span class="badge badge-danger">Deceased</span>' : (p.is_living ? '<span class="badge badge-success">Living</span>' : 'Unknown')}</td>
            <td>
                <button class="btn btn-secondary btn-sm" onclick="selectPersonAndInspect('${p.id}')">View in Tree</button>
            </td>
        </tr>
    `).join('');
}

function filterRoster() {
    const query = document.getElementById('rosterSearch').value;
    renderRosterTable(query);
}

// ================= MODAL OPERATIONS =================
function openModal(id) {
    const m = document.getElementById(id);
    if (m) m.classList.add('active');
}

function closeModal(id) {
    const m = document.getElementById(id);
    if (m) m.classList.remove('active');
}

function openAddMemberModal() {
    // Generate next ID suggestion like P16
    let nextNum = state.people.length + 1;
    let candidate = `P${nextNum < 10 ? '0' : ''}${nextNum}`;
    while (state.people.some(p => p.id === candidate)) {
        nextNum++;
        candidate = `P${nextNum < 10 ? '0' : ''}${nextNum}`;
    }

    document.getElementById('newId').value = candidate;
    document.getElementById('newName').value = '';
    document.getElementById('newBirth').value = '';
    document.getElementById('newDeath').value = '';
    document.getElementById('newBio').value = '';
    populatePersonDropdowns();
    openModal('addMemberModal');
}

async function handleAddMemberSubmit(e) {
    e.preventDefault();
    const payload = {
        id: document.getElementById('newId').value.trim(),
        name: document.getElementById('newName').value.trim(),
        gender: document.getElementById('newGender').value,
        birth_year: parseInt(document.getElementById('newBirth').value) || 0,
        death_year: parseInt(document.getElementById('newDeath').value) || 0,
        is_living: parseInt(document.getElementById('newDeath').value) > 0 ? 0 : 1,
        bio: document.getElementById('newBio').value.trim(),
        parent_id: document.getElementById('newParent').value,
        spouse_id: document.getElementById('newSpouse').value
    };

    try {
        const res = await fetch(`${API_BASE}/api/person`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'Failed to create person');

        closeModal('addMemberModal');
        showToast(`Created family member: ${payload.name} (${payload.id})`, 'success');
        await loadAllData();
        selectPerson(payload.id);
    } catch (err) {
        showToast(err.message, 'error');
    }
}

function openConnectModal() {
    populatePersonDropdowns();
    document.getElementById('connectWarning').style.display = 'none';
    if (state.selectedPersonId) {
        const selA = document.getElementById('connectPersonA');
        if (selA) selA.value = state.selectedPersonId;
    }
    openModal('connectModal');
}

function updateConnectFormLabels() {
    const isParentChild = document.querySelector('input[name="relType"]:checked').value === 'parent_child';
    document.getElementById('lblConnectA').innerText = isParentChild ? 'Parent (Progenitor)' : 'Person A';
    document.getElementById('lblConnectB').innerText = isParentChild ? 'Child (Descendant)' : 'Person B';
}

async function handleConnectSubmit(e) {
    e.preventDefault();
    const relType = document.querySelector('input[name="relType"]:checked').value;
    const aId = document.getElementById('connectPersonA').value;
    const bId = document.getElementById('connectPersonB').value;

    const payload = {
        type: relType,
        parent_id: aId,
        child_id: bId,
        person_a: aId,
        person_b: bId
    };

    try {
        const res = await fetch(`${API_BASE}/api/relationship`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (!res.ok || !data.success) {
            const warnBox = document.getElementById('connectWarning');
            warnBox.innerText = `⛔ Validation Error: ${data.error}`;
            warnBox.style.display = 'block';
            throw new Error(data.error);
        }

        closeModal('connectModal');
        showToast('Relationship successfully connected in C graph!', 'success');
        await loadAllData();
        if (state.selectedPersonId) selectPerson(state.selectedPersonId);
    } catch (err) {
        showToast(err.message, 'error');
    }
}

function openEditSelectedModal() {
    if (!state.selectedPersonId) return;
    const p = state.people.find(x => x.id === state.selectedPersonId);
    if (!p) return;

    document.getElementById('editId').value = p.id;
    document.getElementById('editIdDisplay').value = p.id;
    document.getElementById('editName').value = p.name;
    document.getElementById('editGender').value = p.gender || 'Unknown';
    document.getElementById('editBirth').value = p.birth_year || '';
    document.getElementById('editDeath').value = p.death_year || '';
    document.getElementById('editBio').value = p.bio || '';

    openModal('editMemberModal');
}

async function handleEditMemberSubmit(e) {
    e.preventDefault();
    const id = document.getElementById('editId').value;
    const payload = {
        id: id,
        name: document.getElementById('editName').value.trim(),
        gender: document.getElementById('editGender').value,
        birth_year: parseInt(document.getElementById('editBirth').value) || 0,
        death_year: parseInt(document.getElementById('editDeath').value) || 0,
        is_living: parseInt(document.getElementById('editDeath').value) > 0 ? 0 : 1,
        bio: document.getElementById('editBio').value.trim()
    };

    try {
        const res = await fetch(`${API_BASE}/api/person`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'Failed to update');

        closeModal('editMemberModal');
        showToast('Member details updated.', 'success');
        await loadAllData();
        selectPerson(id);
    } catch (err) {
        showToast(err.message, 'error');
    }
}

async function deleteSelectedPerson() {
    if (!state.selectedPersonId) return;
    const p = state.people.find(x => x.id === state.selectedPersonId);
    const pName = p ? p.name : state.selectedPersonId;

    if (!confirm(`Are you sure you want to delete ${pName} (${state.selectedPersonId})?\n\nThis will remove all associated parent-child and spouse connections while preserving graph consistency.`)) {
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/person?id=${encodeURIComponent(state.selectedPersonId)}`, {
            method: 'DELETE'
        });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'Delete failed');

        showToast(`Removed ${pName} and cascaded graph edge updates.`, 'success');
        clearTreeSelection();
        await loadAllData();
    } catch (err) {
        showToast(err.message, 'error');
    }
}

// ================= STORAGE OPERATIONS =================
async function saveToDisk() {
    try {
        const res = await fetch(`${API_BASE}/api/save`, { method: 'POST' });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'Save failed');
        showToast('Family records persistently written to C data files!', 'success');
    } catch (err) {
        showToast('Save error: ' + err.message, 'error');
    }
}

async function loadFromDisk() {
    try {
        const res = await fetch(`${API_BASE}/api/load`, { method: 'POST' });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'Load failed');
        showToast('Reloaded family tree from persistent files.', 'success');
        clearTreeSelection();
        await loadAllData();
    } catch (err) {
        showToast('Load error: ' + err.message, 'error');
    }
}

async function resetToSample() {
    if (!confirm('Reset family tree to the default Montgomery-Sterling Dynasty sample dataset?')) return;
    try {
        const res = await fetch(`${API_BASE}/api/reset`, { method: 'POST' });
        const data = await res.json();
        if (!res.ok || !data.success) throw new Error(data.error || 'Reset failed');
        showToast('Reset to rich 4-generation sample dataset.', 'success');
        clearTreeSelection();
        await loadAllData();
    } catch (err) {
        showToast('Reset error: ' + err.message, 'error');
    }
}

// ================= HERO MINI PREVIEW =================
function renderHeroPreview() {
    const svg = document.getElementById('heroSvg');
    if (!svg) return;
    svg.innerHTML = '';

    const nodes = [
        { id: 'P01', name: 'Arthur', x: 260, y: 35, role: 'Patriarch' },
        { id: 'P02', name: 'Eleanor', x: 420, y: 35, role: 'Matriarch' },
        { id: 'P03', name: 'Julian', x: 200, y: 125, role: 'Son' },
        { id: 'P05', name: 'Beatrice', x: 480, y: 125, role: 'Daughter' },
        { id: 'P08', name: 'Lucas', x: 140, y: 205, role: 'Grandson' },
        { id: 'P09', name: 'Maya', x: 270, y: 205, role: 'Granddaughter' },
        { id: 'P10', name: 'Sophia', x: 480, y: 205, role: 'Granddaughter' }
    ];

    const edges = [
        { from: 'P01', to: 'P02', type: 'spouse' },
        { from: 'P01', to: 'P03', type: 'pc' },
        { from: 'P02', to: 'P03', type: 'pc' },
        { from: 'P01', to: 'P05', type: 'pc' },
        { from: 'P02', to: 'P05', type: 'pc' },
        { from: 'P03', to: 'P08', type: 'pc' },
        { from: 'P03', to: 'P09', type: 'pc' },
        { from: 'P05', to: 'P10', type: 'pc' }
    ];

    // Render preview edges
    edges.forEach(e => {
        const n1 = nodes.find(n => n.id === e.from);
        const n2 = nodes.find(n => n.id === e.to);
        if (!n1 || !n2) return;

        const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
        if (e.type === 'spouse') {
            path.setAttribute('d', `M ${n1.x + 35} ${n1.y + 15} L ${n2.x - 35} ${n2.y + 15}`);
            path.setAttribute('stroke', '#f59e0b');
            path.setAttribute('stroke-dasharray', '4, 4');
            path.setAttribute('stroke-width', '2');
        } else {
            path.setAttribute('d', `M ${n1.x} ${n1.y + 20} C ${n1.x} ${n1.y + 60}, ${n2.x} ${n2.y - 60}, ${n2.x} ${n2.y - 20}`);
            path.setAttribute('stroke', '#6366f1');
            path.setAttribute('stroke-width', '1.8');
            path.setAttribute('fill', 'none');
        }
        svg.appendChild(path);
    });

    // Render preview nodes
    nodes.forEach(n => {
        const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
        g.setAttribute('transform', `translate(${n.x}, ${n.y})`);
        g.style.cursor = 'pointer';

        const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        circle.setAttribute('r', '22');
        circle.setAttribute('fill', '#1e293b');
        circle.setAttribute('stroke', '#6366f1');
        circle.setAttribute('stroke-width', '2');

        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('text-anchor', 'middle');
        text.setAttribute('y', '5');
        text.setAttribute('fill', '#ffffff');
        text.setAttribute('font-size', '11');
        text.setAttribute('font-weight', '700');
        text.textContent = n.name;

        g.appendChild(circle);
        g.appendChild(text);
        g.addEventListener('click', () => {
            switchTab('tree');
            selectPerson(n.id);
        });
        svg.appendChild(g);
    });
}

// ================= UTILITIES =================
function showToast(msg, type = 'info') {
    const container = document.getElementById('toastContainer');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `<span>${type === 'success' ? '✓' : type === 'error' ? '⚠' : 'ℹ'}</span> <span>${escapeHtml(msg)}</span>`;

    container.appendChild(toast);
    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateY(8px)';
        setTimeout(() => toast.remove(), 300);
    }, 3800);
}

function escapeHtml(text) {
    if (!text) return '';
    return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}
