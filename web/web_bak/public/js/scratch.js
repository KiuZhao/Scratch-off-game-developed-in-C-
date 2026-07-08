// ============================================================
//  Canvas 刮涂层引擎 — 纯前端, 零依赖
// ============================================================

const CELL_W = 150, CELL_H = 100;
const BRUSH_R = 35;  // 笔刷半径

// 涂层颜色
const COATING_COLOR = '#B4AAA0';

// 初始化所有 cell canvas (Vue mounted 后调用)
function initCellCanvases(cellCount, cellCanvases) {
    for (let i = 0; i < cellCount; i++) {
        let cvs = cellCanvases[i];
        if (!cvs) continue;
        cvs.width  = CELL_W;
        cvs.height = CELL_H;
        let ctx = cvs.getContext('2d');
        ctx.fillStyle = COATING_COLOR;
        ctx.fillRect(0, 0, CELL_W, CELL_H);
        // 画装饰横线
        ctx.strokeStyle = '#9A9080';
        ctx.lineWidth = 1;
        ctx.strokeRect(0, 0, CELL_W, CELL_H);
    }
}

// 单个格子刮开百分比
function getCellRevealPct(cvs) {
    if (!cvs) return 0;
    let ctx = cvs.getContext('2d');
    let imgData = ctx.getImageData(0, 0, CELL_W, CELL_H).data;
    let total = CELL_W * CELL_H;
    let transparent = 0;
    for (let i = 3; i < imgData.length; i += 4) {
        if (imgData[i] < 128) transparent++;
    }
    return transparent / total;
}

// 刮开操作
function scratchAt(cvs, x, y, radius) {
    let ctx = cvs.getContext('2d');
    ctx.globalCompositeOperation = 'destination-out';
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fill();
}

// 全刮开
function scratchAll(cvs) {
    let ctx = cvs.getContext('2d');
    ctx.globalCompositeOperation = 'destination-out';
    ctx.fillRect(0, 0, CELL_W, CELL_H);
}
