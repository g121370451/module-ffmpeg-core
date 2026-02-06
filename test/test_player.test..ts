// test/test_player.ts
import { MediaManager, getMediaInfo, FrameData } from '../lib/index';
import path from 'path';

// 辅助等待函数
const sleep = (ms: number) => new Promise(resolve => setTimeout(resolve, ms));

// 定义测试套件
describe('FFmpeg Player Core Functional Tests', () => {
  let manager: MediaManager;
  const devId = "cam_test_01";
  const channel = 0;
  
  // 这里的路径需要注意：
  // 编译后运行在 test_dist/test/ 下，视频如果还在项目根目录的 res 下
  // 需要用 ../../res/xxx 找到它，最好用绝对路径
  const videoPath = "E:\\project\\deepcool\\nodejs\\module-ffmpeg-core\\test\\qqq.mp4"

  // --- 1. 全局初始化 ---
  beforeAll(() => {
    console.log(`[Setup] Testing with video: ${videoPath}`);
    manager = new MediaManager();
  });

  // --- 2. 全局清理 ---
  afterAll(() => {
    console.log('[Teardown] Cleaning up resources...');
    try {
      manager.deleteMedia(devId, channel);
    } catch (e) {
      // 忽略清理时的错误
    }
  });

  // --- 测试用例 1: 检查静态信息 ---
  test('Step 1: Get Media Info (Static)', () => {
    // 假设你的 getMediaInfo 是个纯函数，不需要实例化对象
    try {
        const info = getMediaInfo(videoPath);
        console.log('Media Info:', info);
        
        expect(info).toBeDefined();
        expect(info.duration).toBeGreaterThan(0);
        // 验证返回的对象结构
        expect(info).toHaveProperty('width');
        expect(info).toHaveProperty('height');
    } catch (e) {
        // 如果文件不存在，Jest 会捕获这个错误并判负
        throw new Error(`Failed to read media info: ${e}`);
    }
  });

  // --- 测试用例 2: 添加媒体源 ---
  test('Step 2: Add Media Source', () => {
    const success = manager.addMedia(
        devId, 
        channel, 
        videoPath, 
        0, 0, 1920, 1080, // Source Crop
        640, 360          // Output Size
    );
    expect(success).toBe(true);
  });

  // --- 测试用例 3: 播放并获取帧 ---
  test('Step 3: Play and Capture Frames', async () => {
    // 预热 200ms
    await sleep(200);

    let capturedFrames = 0;
    
    // 尝试获取 10 帧
    for (let i = 0; i < 10; i++) {
      const frame = manager.getNextFrame(devId, channel);
      if (frame.success) {
        capturedFrames++;
        // 验证帧数据的完整性
        expect(frame.width).toBe(640);
        expect(frame.height).toBe(360);
        expect(frame.data).toBeInstanceOf(Buffer);
      }
      await sleep(30); // 模拟 30fps 间隔
    }

    // 只要能抓到帧就算成功
    expect(capturedFrames).toBeGreaterThan(0);
  });

  // --- 测试用例 4: 暂停功能 ---
  test('Step 4: Pause Stream', async () => {
    const res = manager.pause(devId, channel);
    expect(res).toBe(true);

    // 等待 1 秒，确保底层状态切换
    await sleep(1000);
    
    // 理论上暂停期间如果去 getNextFrame，行为取决于你的 C++ 实现
    // 这里我们只验证 API 调用成功
  });

  // --- 测试用例 5: 恢复功能 ---
  test('Step 5: Resume Stream', async () => {
    const res = manager.resume(devId, channel);
    expect(res).toBe(true);

    // 恢复后等待一会
    await sleep(500);

    // 再次尝试获取帧，应该能获取到了
    let hasFrame = false;
    for(let i=0; i<5; i++) {
        const frame = manager.getNextFrame(devId, channel);
        if(frame.success) {
            hasFrame = true;
            break;
        }
        await sleep(50);
    }
    expect(hasFrame).toBe(true);
  });

  // --- 测试用例 6: 动态 ROI 修改 (可选) ---
  test('Step 6: Update ROI', async () => {
    // 不抛出异常即视为通过
    expect(() => {
        manager.updateROI(devId, channel, 100, 100, 800, 600);
    }).not.toThrow();
    
    await sleep(200);
  });

});