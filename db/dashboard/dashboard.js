(() => {
  const api = '../api/';
  const pageError = document.querySelector('#page-error');
  const tbody = document.querySelector('#records');
  const jobIdInput = document.querySelector('#job-id');
  let offset = 0;

  async function request(path, options = {}) {
    const response = await fetch(api + path, { credentials: 'same-origin', ...options });
    const data = await response.json();
    if (!response.ok) throw new Error(data.message || data.error || 'Request failed');
    return data;
  }

  function cell(row, value, code = false) {
    const td = document.createElement('td');
    const node = code ? document.createElement('code') : td;
    node.textContent = value ?? '—';
    if (code) td.append(node);
    row.append(td);
  }

  async function loadRecords(reset = false) {
    if (reset) {
      offset = 0;
      tbody.replaceChildren();
    }
    const parameters = new URLSearchParams({ limit: '25', offset: String(offset) });
    if (jobIdInput.value.trim()) parameters.set('job_id', jobIdInput.value.trim());
    const data = await request(`history.php?${parameters}`);
    for (const record of data.records) {
      const row = document.createElement('tr');
      cell(row, record.captured_at_utc);
      cell(row, record.record_type);
      cell(row, record.provider_name);
      cell(row, record.session_id);
      cell(row, record.job_id, true);
      cell(row, record.sync_state || 'unknown');
      cell(row, record.record_id, true);
      tbody.append(row);
    }
    offset += data.records.length;
    document.querySelector('#more').hidden = data.records.length < 25;
  }

  async function refresh() {
    pageError.textContent = '';
    try {
      const health = await request('dashboard-status.php');
      document.querySelector('#health').textContent =
        `Database ${health.database.status} · ${health.checked_at}`;
      await loadRecords(true);
      const diagnostics = await request('diagnostics.php');
      document.querySelector('#diagnostics').textContent = JSON.stringify(diagnostics, null, 2);
    } catch (error) {
      pageError.textContent = error.message;
    }
  }

  document.querySelector('#job-filter').addEventListener('submit', async event => {
    event.preventDefault();
    await loadRecords(true).catch(error => {
      pageError.textContent = error.message;
    });
  });
  document.querySelector('#clear-job-filter').addEventListener('click', () => {
    jobIdInput.value = '';
    loadRecords(true).catch(error => {
      pageError.textContent = error.message;
    });
  });
  document.querySelector('#refresh').addEventListener('click', refresh);
  document.querySelector('#more').addEventListener('click', () => loadRecords().catch(error => {
    pageError.textContent = error.message;
  }));

  refresh();
})();
