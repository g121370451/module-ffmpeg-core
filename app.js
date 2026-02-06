const { MediaManager, getMediaInfo } = require('./build/Release/module-ffmplayer-core.node');
console.log(MediaManager);
let fs = require('fs');
var showMem = function () {
    var mem = process.memoryUsage();
    var format = function (bytes) {
        return (bytes / 1024 / 1024).toFixed(2) + 'MB';
    };
    console.log('Process1: heapTotal ' + format(mem.heapTotal) + ' heapUsed ' + format(mem.heapUsed) + ' rss ' + format(mem.rss));
};
(async () => {
    const manager = new MediaManager();
    let save = false;
    // const path = "C:\\Users\\gpy\\Desktop\\480x480\\xx.jpg";
    const path = "C:\\Users\\gpy\\Desktop\\480x480\\q.gif";
    // const path = "C:\\Users\\gpy\\Desktop\\480x480\\541.txt";

    console.log("--- 正在获取媒体元数据 ---");
    const info = getMediaInfo(path);
    console.log(JSON.stringify(info, null, 4));

    if (info.valid) {
        console.log("\n--- 元数据校验通过，准备启动流管理 ---");
        // const path = "C:\\Users\\gpy\\Desktop\\480x480\\test.mp4";
        // 1. 添加媒体流，设置初始截取区域 (0,0,640,480) 缩放到 (320,240)
        const success = manager.addMedia("cam_01", 1001, path, 169, 0, 162, 348, 224, 480);
        console.log('success', success)
        if (success) {
            console.log("媒体流已启动...");

            // 2. 每 40ms 轮询一次 A 指针缓冲区
            setInterval(() => {
                let frame = manager.getNextFrame("cam_01", 1001);
                // showMem();
                if (frame.success) {
                    // console.log(`收到新帧: ${frame.width}x${frame.height}, 大小: ${frame.data.length} bytes,shijian: ${frame.timestamp} ms`);
                    // frame.data 现在是一个标准的 Node.js Buffer
                    // console.log(`收到 MJPEG 帧: ${frame.data.length} bytes, 尺寸: ${frame.width}x${frame.height}`);
                    // 这里可以将 Buffer 通过 WebSocket 发送或写入文件
                    if (save && frame.data.length != 0) {
                        fs.writeFile('./demo.jpg', frame.data, (err) => {
                            if (err) {
                                console.error('保存失败:', err);
                                return;
                            }
                            console.log('图片保存成功！');
                            save = false;
                        });
                    }
                }
            }, 33);

            // 3. 5秒后动态修改截取 ROI (向右偏移 100 像素)
            setTimeout(() => {
                console.log("动态更新 ROI...");
                const pauseStatus = manager.pause("cam_01", 1001)
                console.log('pauseStatus', pauseStatus)
                // save = true
            }, 5000);
        }
    } else {
        console.error("无法解析媒体文件，请检查路径。");
    }
})()