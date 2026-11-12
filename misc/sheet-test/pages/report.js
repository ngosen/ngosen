// Posts the sheet's filled cells (row by row, joined by "|") to the test server whenever they change,
// with the screen positions of cells A1 and A3 so the runner can click them. A page sets
// window.arrowsCommit when an arrow key ends the edit and moves, as in Excel.
(function () {
  const id = Math.random().toString(36).slice(2);
  let last = null;

  // Firefox gives the content's screen offset; elsewhere the runner opens the browser in kiosk mode.
  function toScreen(p) {
    if (!p) return null;
    const x = window.mozInnerScreenX !== undefined ? window.mozInnerScreenX : window.screenX;
    const y = window.mozInnerScreenY !== undefined ? window.mozInnerScreenY : window.screenY;
    const r = window.devicePixelRatio || 1;
    return [Math.round((x + p.x) * r), Math.round((y + p.y) * r)];
  }

  // getCells returns rows of cell values, or null while the sheet is loading.
  // cellCenter(row, col) returns the cell's centre in page coordinates.
  window.report = function (name, getCells, cellCenter) {
    setInterval(() => {
      let t, click = null;
      try {
        const rows = getCells();
        t = rows === null || rows === undefined ? 'ERR not ready'
          : rows.flat().map((v) => (v === null || v === undefined ? '' : String(v).trim())).filter((v) => v).join('|');
        click = cellCenter ? { a1: toScreen(cellCenter(0, 0)), a3: toScreen(cellCenter(2, 0)) } : null;
      } catch (e) {
        t = 'ERR ' + e;
      }
      const body = JSON.stringify({ id, name, t, click, arrows: !!window.arrowsCommit });
      if (body !== last) {
        last = body;
        fetch('/r', { method: 'POST', body }).catch(() => {});
      }
    }, 250);
  };

  window.centerOf = function (el) {
    const b = el.getBoundingClientRect();
    return { x: b.left + b.width / 2, y: b.top + b.height / 2 };
  };
})();
