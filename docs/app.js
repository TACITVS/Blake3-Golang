(() => {
  const palette = {
    'Ref C': '#1f7a8c',
    'Go asm': '#e07a5f',
    'Go purego': '#4a7c59',
    'FP C': '#f2cc8f'
  };

  const versionOrder = ['ref_c', 'go_asm', 'go_purego', 'fp_c'];

  const el = (id) => document.getElementById(id);

  function computeStats(values) {
    const nums = values.filter((v) => Number.isFinite(v));
    if (!nums.length) {
      return { min: 0, max: 0, avg: 0, std: 0 };
    }
    const sum = nums.reduce((a, b) => a + b, 0);
    const avg = sum / nums.length;
    const min = Math.min(...nums);
    const max = Math.max(...nums);
    const variance = nums.reduce((a, b) => a + (b - avg) ** 2, 0) / nums.length;
    const std = Math.sqrt(variance);
    return { min, max, avg, std };
  }

  function safeText(node, text) {
    if (node) node.textContent = text;
  }

  async function loadData() {
    if (window.BENCH_DATA) {
      return window.BENCH_DATA;
    }
    try {
      const resp = await fetch('data/bench.json', { cache: 'no-store' });
      if (!resp.ok) throw new Error('Fetch failed');
      return await resp.json();
    } catch (err) {
      return null;
    }
  }

  function renderSpecs(data) {
    const grid = el('specs-grid');
    if (!grid) return;
    const specs = [
      { label: 'OS', value: data.machine?.os || 'unknown' },
      { label: 'CPU', value: data.machine?.cpu || 'unknown' },
      { label: 'Cores', value: data.machine?.cores || 'unknown' },
      { label: 'RAM (GB)', value: data.machine?.ram_gb || 'unknown' },
      { label: 'Go', value: data.toolchain?.go || 'unknown' },
      { label: 'GOOS/GOARCH', value: `${data.toolchain?.goos || '?'} / ${data.toolchain?.goarch || '?'}` },
      { label: 'GOMAXPROCS', value: data.toolchain?.gomaxprocs || 'default' },
      { label: 'GCC', value: data.toolchain?.gcc || 'unknown' },
      { label: 'NASM', value: data.toolchain?.nasm || 'unknown' }
    ];

    grid.innerHTML = '';
    specs.forEach((spec) => {
      const card = document.createElement('div');
      card.className = 'spec-card';
      const span = document.createElement('span');
      span.textContent = spec.label;
      const value = document.createElement('strong');
      value.textContent = spec.value;
      card.appendChild(span);
      card.appendChild(value);
      grid.appendChild(card);
    });
  }

  function renderMeta(data) {
    safeText(el('meta-line'), `Runs: ${data.runs} - Commit: ${data.git_commit?.slice(0, 8) || 'unknown'}`);
    safeText(el('generated-at'), data.generated_at || 'unknown');
  }

  function getVersions(data) {
    return versionOrder
      .map((key) => {
        const entry = data.versions?.[key];
        if (!entry) return null;
        return { key, label: entry.label || key, sizes: entry.sizes || {} };
      })
      .filter(Boolean);
  }

  function renderSummary(data) {
    const grid = el('summary-cards');
    if (!grid) return;
    const versions = getVersions(data);
    grid.innerHTML = '';
    versions.forEach((v) => {
      const stats = computeStats(v.sizes['1M'] || []);
      const card = document.createElement('div');
      card.className = 'summary-card';
      card.style.borderTop = `4px solid ${palette[v.label] || '#ccc'}`;
      card.innerHTML = `<h3>${v.label} (1M avg)</h3><div class="value">${stats.avg.toFixed(2)} MB/s</div>`;
      grid.appendChild(card);
    });
  }

  function renderLegend(versions) {
    const legend = el('legend');
    if (!legend) return;
    legend.innerHTML = '';
    versions.forEach((v) => {
      const item = document.createElement('div');
      item.className = 'legend-item';
      const swatch = document.createElement('span');
      swatch.className = 'legend-swatch';
      swatch.style.background = palette[v.label] || '#999';
      const label = document.createElement('span');
      label.textContent = v.label;
      item.appendChild(swatch);
      item.appendChild(label);
      legend.appendChild(item);
    });
  }

  function svgEl(name, attrs = {}) {
    const node = document.createElementNS('http://www.w3.org/2000/svg', name);
    Object.entries(attrs).forEach(([k, v]) => node.setAttribute(k, v));
    return node;
  }

  function renderBarChart(data) {
    const svg = el('bar-chart');
    if (!svg) return;
    while (svg.firstChild) svg.removeChild(svg.firstChild);

    const versions = getVersions(data);
    const sizes = data.sizes || ['1K', '8K', '1M'];
    const width = 900;
    const height = 420;
    const pad = { left: 60, right: 20, top: 20, bottom: 50 };
    const chartW = width - pad.left - pad.right;
    const chartH = height - pad.top - pad.bottom;

    const values = [];
    versions.forEach((v) => sizes.forEach((s) => values.push(computeStats(v.sizes[s] || []).avg)));
    const maxVal = Math.max(1, ...values);

    const groupW = chartW / sizes.length;
    const gap = 8;
    const barW = (groupW - gap * 2) / versions.length;

    const bg = svgEl('rect', { x: pad.left, y: pad.top, width: chartW, height: chartH, fill: '#fffaf2' });
    svg.appendChild(bg);

    for (let i = 0; i <= 5; i++) {
      const y = pad.top + (chartH * i) / 5;
      const line = svgEl('line', { x1: pad.left, y1: y, x2: pad.left + chartW, y2: y, stroke: '#e5d8c9', 'stroke-width': 1 });
      svg.appendChild(line);
      const labelVal = Math.round(maxVal - (maxVal * i) / 5);
      const label = svgEl('text', { x: pad.left - 10, y: y + 4, 'text-anchor': 'end', 'font-size': '12', fill: '#5a5a5a' });
      label.textContent = labelVal;
      svg.appendChild(label);
    }

    sizes.forEach((size, gi) => {
      const gx = pad.left + gi * groupW + gap;
      versions.forEach((v, vi) => {
        const avg = computeStats(v.sizes[size] || []).avg;
        const h = (avg / maxVal) * chartH;
        const x = gx + vi * barW;
        const y = pad.top + (chartH - h);
        const rect = svgEl('rect', {
          x,
          y,
          width: barW - 4,
          height: h,
          rx: 4,
          fill: palette[v.label] || '#999'
        });
        svg.appendChild(rect);
      });
      const label = svgEl('text', { x: pad.left + gi * groupW + groupW / 2, y: pad.top + chartH + 30, 'text-anchor': 'middle', 'font-size': '13', fill: '#3a3a3a' });
      label.textContent = size;
      svg.appendChild(label);
    });

    const axis = svgEl('line', { x1: pad.left, y1: pad.top + chartH, x2: pad.left + chartW, y2: pad.top + chartH, stroke: '#1b1b1b', 'stroke-width': 1.2 });
    svg.appendChild(axis);
  }

  function renderTable(data) {
    const table = el('stats-table');
    if (!table) return;
    const versions = getVersions(data);
    const sizes = data.sizes || ['1K', '8K', '1M'];

    const thead = document.createElement('thead');
    const headRow = document.createElement('tr');
    ['Version', 'Size', 'Avg MB/s', 'Min', 'Max', 'Std'].forEach((h) => {
      const th = document.createElement('th');
      th.textContent = h;
      headRow.appendChild(th);
    });
    thead.appendChild(headRow);

    const tbody = document.createElement('tbody');
    versions.forEach((v) => {
      sizes.forEach((s) => {
        const stats = computeStats(v.sizes[s] || []);
        const row = document.createElement('tr');
        const values = [
          v.label,
          s,
          stats.avg.toFixed(2),
          stats.min.toFixed(2),
          stats.max.toFixed(2),
          stats.std.toFixed(2)
        ];
        values.forEach((val) => {
          const td = document.createElement('td');
          td.textContent = val;
          row.appendChild(td);
        });
        tbody.appendChild(row);
      });
    });

    table.innerHTML = '';
    table.appendChild(thead);
    table.appendChild(tbody);
  }

  function renderRunChart(data, size) {
    const canvas = el('run-chart');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const versions = getVersions(data);
    const allValues = versions.flatMap((v) => v.sizes[size] || []);
    const maxVal = Math.max(1, ...allValues);

    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    ctx.scale(dpr, dpr);

    const width = rect.width;
    const height = rect.height;
    const pad = { left: 50, right: 20, top: 20, bottom: 40 };
    const chartW = width - pad.left - pad.right;
    const chartH = height - pad.top - pad.bottom;

    ctx.clearRect(0, 0, width, height);
    ctx.fillStyle = '#fffaf2';
    ctx.fillRect(pad.left, pad.top, chartW, chartH);

    ctx.strokeStyle = '#e5d8c9';
    ctx.lineWidth = 1;
    for (let i = 0; i <= 5; i++) {
      const y = pad.top + (chartH * i) / 5;
      ctx.beginPath();
      ctx.moveTo(pad.left, y);
      ctx.lineTo(pad.left + chartW, y);
      ctx.stroke();
      const label = Math.round(maxVal - (maxVal * i) / 5);
      ctx.fillStyle = '#5a5a5a';
      ctx.font = '12px "JetBrains Mono", monospace';
      ctx.textAlign = 'right';
      ctx.fillText(label, pad.left - 8, y + 4);
    }

    ctx.strokeStyle = '#1b1b1b';
    ctx.beginPath();
    ctx.moveTo(pad.left, pad.top + chartH);
    ctx.lineTo(pad.left + chartW, pad.top + chartH);
    ctx.stroke();

    const runs = Math.max(...versions.map((v) => (v.sizes[size] || []).length));
    const stepX = runs > 1 ? chartW / (runs - 1) : chartW;

    versions.forEach((v) => {
      const values = v.sizes[size] || [];
      ctx.strokeStyle = palette[v.label] || '#555';
      ctx.lineWidth = 2;
      ctx.beginPath();
      values.forEach((val, idx) => {
        const x = pad.left + idx * stepX;
        const y = pad.top + chartH - (val / maxVal) * chartH;
        if (idx === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();

      ctx.fillStyle = palette[v.label] || '#555';
      values.forEach((val, idx) => {
        const x = pad.left + idx * stepX;
        const y = pad.top + chartH - (val / maxVal) * chartH;
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, Math.PI * 2);
        ctx.fill();
      });
    });

    ctx.fillStyle = '#3a3a3a';
    ctx.font = '12px "JetBrains Mono", monospace';
    ctx.textAlign = 'center';
    ctx.fillText('Run index', pad.left + chartW / 2, height - 12);
    ctx.save();
    ctx.translate(16, pad.top + chartH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('MB/s', 0, 0);
    ctx.restore();
  }

  function setupTabs(data) {
    const tabs = el('size-tabs');
    if (!tabs) return;
    const sizes = data.sizes || ['1K', '8K', '1M'];
    tabs.innerHTML = '';

    let current = sizes[0];
    const render = () => renderRunChart(data, current);

    sizes.forEach((size, idx) => {
      const button = document.createElement('button');
      button.className = 'tab' + (idx === 0 ? ' active' : '');
      button.textContent = size;
      button.addEventListener('click', () => {
        current = size;
        tabs.querySelectorAll('.tab').forEach((t) => t.classList.remove('active'));
        button.classList.add('active');
        render();
      });
      tabs.appendChild(button);
    });

    window.addEventListener('resize', () => render());
    render();
  }

  function showError(message) {
    const main = document.querySelector('main');
    if (!main) return;
    const div = document.createElement('div');
    div.className = 'notice';
    div.textContent = message;
    main.prepend(div);
  }

  loadData().then((data) => {
    if (!data) {
      showError('Benchmark data could not be loaded. Make sure data/bench.js or data/bench.json exists.');
      return;
    }
    renderSpecs(data);
    renderMeta(data);
    renderSummary(data);
    renderLegend(getVersions(data));
    renderBarChart(data);
    renderTable(data);
    setupTabs(data);
  });
})();
