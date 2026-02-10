import bindings from 'bindings';

// 1. 加载原生模块
const nativeAddon = bindings('module-ffmplayer-core');

// --- 类型定义 ---

declare interface FrameData {
    success: boolean;
    data?: Buffer; // 只有 success 为 true 时才有
    width?: number;
    height?: number;
    timestamp?: number;
}

declare interface MediaInfo {
    valid: boolean;
    type: string;     // e.g., "video", "audio"
    duration: number; // 秒
    width: number;
    height: number;
    sar: string;      // Sample Aspect Ratio
    dar: string;      // Display Aspect Ratio
}

// C++ 原生对象的接口契约 (不对外暴露，内部使用)
interface INativeMediaManager {
    new(): INativeMediaManager;
    addMedia(devId: string, index: number, url: string, x: number, y: number, sw: number, sh: number, ow: number, oh: number): boolean;
    deleteMedia(devId: string, index: number): boolean;
    updateROI(devId: string, index: number, x: number, y: number, sw: number, sh: number): void;
    pause(devId: string, index: number): boolean;
    resume(devId: string, index: number): boolean;
    getNextFrame(devId: string, index: number): FrameData;
}

// 2. 类接口 (包含静态方法)
declare interface IMediaManagerClass {
  getInstance(): MediaManager;
}

export interface IFFmpegModule {
  MediaManager: IMediaManagerClass;
}


/**
 * 媒体流管理器
 * 对应 C++: MediaManagerWrapper 类
 */
export class MediaManager {
    private static instance: MediaManager
    public static getInstance(): MediaManager {
        if (!MediaManager.instance) {
            MediaManager.instance = new MediaManager()
        }
        return MediaManager.instance
    }

    private _instance: INativeMediaManager;

    private constructor() {
        this._instance = new nativeAddon.MediaManager();
    }

    /**
     * 添加媒体源
     * @param devId 设备ID/唯一标识
     * @param index 通道索引
     * @param url 视频流地址或文件路径
     * @param x 裁剪区域 X
     * @param y 裁剪区域 Y
     * @param sw 裁剪源宽度 (Source Width)
     * @param sh 裁剪源高度 (Source Height)
     * @param ow 输出宽度 (Output Width)
     * @param oh 输出高度 (Output Height)
     */
    addMedia(
        devId: string,
        index: number,
        url: string,
        x: number, y: number, sw: number, sh: number,
        ow: number, oh: number
    ): boolean {
        return this._instance.addMedia(devId, index, url, x, y, sw, sh, ow, oh);
    }

    getMediaInfo(filePath: string): MediaInfo {
        return nativeAddon.getMediaInfo(filePath);
    }

    /**
     * 删除媒体源
     */
    deleteMedia(devId: string, index: number): boolean {
        return this._instance.deleteMedia(devId, index);
    }

    /**
     * 更新感兴趣区域 (ROI)
     */
    updateROI(
        devId: string,
        index: number,
        x: number, y: number, sw: number, sh: number
    ): void {
        this._instance.updateROI(devId, index, x, y, sw, sh);
    }

    /**
     * 暂停播放
     */
    pause(devId: string, index: number): boolean {
        return this._instance.pause(devId, index);
    }

    /**
     * 恢复播放
     */
    resume(devId: string, index: number): boolean {
        return this._instance.resume(devId, index);
    }

    /**
     * 获取下一帧数据 (阻塞式)
     */
    getNextFrame(devId: string, index: number): FrameData {
        return this._instance.getNextFrame(devId, index);
    }
}