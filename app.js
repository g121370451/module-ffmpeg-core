const { MediaManager, getMediaInfo } = require('./build/Release/module-ffmplayer-core.node');

const path = process.argv[2] || "C:\\Users\\gpy\\Desktop\\480x480\\q.gif";

(async () => {
    const info = getMediaInfo(path);
    console.log("媒体信息:", JSON.stringify(info, null, 2));
    if (!info.valid) {
        console.error("无法解析媒体文件:", path);
        process.exit(1);
    }

    const manager = new MediaManager();
    const qualities = [1, 2, 5, 8, 10, 15, 20, 25, 31];
    const devId = "qtest";
    // 截取区域和输出尺寸，根据实际视频调整
    const srcW = Math.min(info.width, 640);
    const srcH = Math.min(info.height, 480);
    const outW = 320, outH = 240;

    console.log(`\n源: ${path} (${info.width}x${info.height})`);
    console.log(`ROI: (0,0,${srcW},${srcH}) -> ${outW}x${outH}\n`);

    // 为每个 quality 创建一个通道 (index = i)
    for (let i = 0; i < qualities.length; i++) {
        const q = qualities[i];
        // addMedia(devId, index, url, x, y, sw, sh, ow, oh, startTime, endTime, quality)
        const ok = manager.addMedia(devId, i, path, 0, 0, srcW, srcH, outW, outH, 0, 0, q);
        if (!ok) {
            console.error(`addMedia failed for q:v=${q}`);
            process.exit(1);
        }
    }

    // 等待所有通道解码出首帧
    await new Promise(r => setTimeout(r, 1500));

    console.log("===== Quality (-q:v) vs Frame Size =====");
    console.log(`${"q:v".padStart(6)}  ${"size(bytes)".padStart(12)}  ${"WxH".padStart(10)}`);
    console.log("-".repeat(34));

    const sizes = [];
    for (let i = 0; i < qualities.length; i++) {
        const frame = manager.getNextFrame(devId, i);
        if (frame.success) {
            sizes.push(frame.data.length);
            console.log(
                `${String(qualities[i]).padStart(6)}  ${String(frame.data.length).padStart(12)}  ${`${frame.width}x${frame.height}`.padStart(10)}`
            );
        } else {
            sizes.push(-1);
            console.log(`${String(qualities[i]).padStart(6)}  ${"FAILED".padStart(12)}`);
        }
    }

    console.log("========================================");

    // 检查单调递减
    let monotonic = true;
    for (let i = 1; i < sizes.length; i++) {
        if (sizes[i] > 0 && sizes[i - 1] > 0 && sizes[i] > sizes[i - 1]) {
            console.warn(`⚠ q:v=${qualities[i]}(${sizes[i]}B) > q:v=${qualities[i - 1]}(${sizes[i - 1]}B)`);
            monotonic = false;
        }
    }
    if (monotonic) console.log("✓ 帧大小随 q:v 增大单调递减，质量参数生效");

    // 清理所有通道
    for (let i = 0; i < qualities.length; i++) {
        manager.deleteMedia(devId, i);
    }
    process.exit(0);
})();
