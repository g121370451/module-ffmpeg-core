// 1. 加载原生模块
const nativeAddon = require('./module-ffmplayer-core.node');

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

// 插件元数据类型
export interface PluginInterface {
  name: string
  description?: string
  params?: Record<string, any>
  returns?: any
}

export interface PluginMetadata {
  id: string
  name: string
  version: string
  type: 'ui' | 'functional'
  description?: string
  interfaces?: PluginInterface[]
  dependencies?: Record<string, string>
}

// 消息事件接口
interface MessageEvent {
  data: any
  ports?: any[]
}

// Worker 进程接口
interface WorkerProcess extends NodeJS.Process {
  parentPort: {
    on: (event: string, listener: (e: MessageEvent) => void) => void
    postMessage: (message: any, transferable?: any[]) => void
  }
}

const workerProcess = process as unknown as WorkerProcess

/**
 * 媒体流管理器
 * 对应 C++: MediaManagerWrapper 类
 * 提供 FFmpeg 音视频处理的核心功能
 */
export class MediaManager {
    private static instance: MediaManager
    
    /**
     * 获取 MediaManager 单例实例
     * @returns MediaManager 实例
     */
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
     * @param x 裁剪区域 X 坐标
     * @param y 裁剪区域 Y 坐标
     * @param sw 裁剪源宽度 (Source Width)
     * @param sh 裁剪源高度 (Source Height)
     * @param ow 输出宽度 (Output Width)
     * @param oh 输出高度 (Output Height)
     * @returns boolean 添加是否成功
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

    /**
     * 获取媒体文件信息
     * @param filePath 媒体文件路径
     * @returns MediaInfo 媒体信息对象
     */
    getMediaInfo(filePath: string): MediaInfo {
        return nativeAddon.getMediaInfo(filePath);
    }

    /**
     * 删除媒体源
     * @param devId 设备ID/唯一标识
     * @param index 通道索引
     * @returns boolean 删除是否成功
     */
    deleteMedia(devId: string, index: number): boolean {
        return this._instance.deleteMedia(devId, index);
    }

    /**
     * 更新感兴趣区域 (ROI)
     * @param devId 设备ID/唯一标识
     * @param index 通道索引
     * @param x 新的裁剪区域 X 坐标
     * @param y 新的裁剪区域 Y 坐标
     * @param sw 新的裁剪源宽度
     * @param sh 新的裁剪源高度
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
     * @param devId 设备ID/唯一标识
     * @param index 通道索引
     * @returns boolean 暂停是否成功
     */
    pause(devId: string, index: number): boolean {
        return this._instance.pause(devId, index);
    }

    /**
     * 恢复播放
     * @param devId 设备ID/唯一标识
     * @param index 通道索引
     * @returns boolean 恢复是否成功
     */
    resume(devId: string, index: number): boolean {
        return this._instance.resume(devId, index);
    }

    /**
     * 获取下一帧数据 (阻塞式)
     * @param devId 设备ID/唯一标识
     * @param index 通道索引
     * @returns FrameData 帧数据对象
     */
    getNextFrame(devId: string, index: number): FrameData {
        return this._instance.getNextFrame(devId, index);
    }
}

// Worker 功能
let discoveryPort: any = null
const connectedPlugins: Map<string, any> = new Map()

function setupDiscoveryPort() {
  if (!discoveryPort) return

  discoveryPort.on('message', (e: MessageEvent) => {
    const msg = e.data

    if (msg.type === 'plugin-discovery') {
      console.log('[module-ffmpeg-core] Received plugin discovery:', msg.plugins)
      handleDiscovery(msg, e.ports)
    }

    if (msg.type === 'media-manager') {
      handleMediaManagerRequest(e)
    }
  })

  discoveryPort.start()
}

function registerSelf() {
  if (!workerProcess.parentPort) return

  const metadata: PluginMetadata = {
    id: 'module-ffmpeg-core',
    name: 'FFmpeg 核心功能插件',
    version: '1.0.0',
    type: 'functional',
    description: 'FFmpeg 音视频处理功能插件',
    interfaces: [
      { name: 'getMediaInfo', description: '获取媒体文件信息' },
      { name: 'addMedia', description: '添加媒体源' },
      { name: 'deleteMedia', description: '删除媒体源' },
      { name: 'updateROI', description: '更新感兴趣区域' },
      { name: 'pause', description: '暂停播放' },
      { name: 'resume', description: '恢复播放' },
      { name: 'getNextFrame', description: '获取下一帧' }
    ]
  }

  workerProcess.parentPort.postMessage({
    type: 'plugin-register',
    metadata
  })
}

function handleDiscovery(msg: any, ports: any[] | undefined) {
  const { plugins } = msg
  plugins.forEach((plugin: any, index: number) => {
    if (ports && ports[index]) {
      connectedPlugins.set(plugin.metadata.id, ports[index])
      console.log(`[module-ffmpeg-core] Connected to plugin: ${plugin.metadata.id}`)

      ports[index].on('message', (e: MessageEvent) => {
        if (e.data.type === 'media-manager') {
          handleMediaManagerRequestFromPort(e, plugin.metadata.id, ports[index])
        }
      })
      ports[index].start()
    }
  })
}

function handleMediaManagerRequest(e: MessageEvent) {
  const { action, payload, id } = e.data
  const mediaManager = MediaManager.getInstance()

  try {
    let result
    switch (action) {
      case 'getMediaInfo':
        result = mediaManager.getMediaInfo(payload.filePath)
        break
      case 'addMedia':
        result = mediaManager.addMedia(
          payload.devId,
          payload.index,
          payload.url,
          payload.x,
          payload.y,
          payload.sw,
          payload.sh,
          payload.ow,
          payload.oh
        )
        break
      case 'deleteMedia':
        result = mediaManager.deleteMedia(payload.devId, payload.index)
        break
      case 'updateROI':
        mediaManager.updateROI(
          payload.devId,
          payload.index,
          payload.x,
          payload.y,
          payload.sw,
          payload.sh
        )
        result = true
        break
      case 'pause':
        result = mediaManager.pause(payload.devId, payload.index)
        break
      case 'resume':
        result = mediaManager.resume(payload.devId, payload.index)
        break
      case 'getNextFrame':
        result = mediaManager.getNextFrame(payload.devId, payload.index)
        break
      default:
        throw new Error(`Unknown action: ${action}`)
    }

    if (e.ports && e.ports[0]) {
      e.ports[0].postMessage({
        type: 'media-manager-response',
        id,
        success: true,
        result
      })
    } else if (workerProcess.parentPort) {
      workerProcess.parentPort.postMessage({
        type: 'media-manager-response',
        id,
        success: true,
        result
      })
    }
  } catch (error) {
    const errorMsg = error instanceof Error ? error.message : String(error)
    if (e.ports && e.ports[0]) {
      e.ports[0].postMessage({
        type: 'media-manager-response',
        id,
        success: false,
        error: errorMsg
      })
    } else if (workerProcess.parentPort) {
      workerProcess.parentPort.postMessage({
        type: 'media-manager-response',
        id,
        success: false,
        error: errorMsg
      })
    }
  }
}

function handleMediaManagerRequestFromPort(e: MessageEvent, sourcePluginId: string, port: any) {
  const { action, payload, id } = e.data
  const mediaManager = MediaManager.getInstance()

  try {
    let result
    switch (action) {
      case 'getMediaInfo':
        result = mediaManager.getMediaInfo(payload.filePath)
        break
      case 'addMedia':
        result = mediaManager.addMedia(
          payload.devId,
          payload.index,
          payload.url,
          payload.x,
          payload.y,
          payload.sw,
          payload.sh,
          payload.ow,
          payload.oh
        )
        break
      case 'deleteMedia':
        result = mediaManager.deleteMedia(payload.devId, payload.index)
        break
      case 'updateROI':
        mediaManager.updateROI(
          payload.devId,
          payload.index,
          payload.x,
          payload.y,
          payload.sw,
          payload.sh
        )
        result = true
        break
      case 'pause':
        result = mediaManager.pause(payload.devId, payload.index)
        break
      case 'resume':
        result = mediaManager.resume(payload.devId, payload.index)
        break
      case 'getNextFrame':
        result = mediaManager.getNextFrame(payload.devId, payload.index)
        break
      default:
        throw new Error(`Unknown action: ${action}`)
    }

    port.postMessage({
      type: 'media-manager-response',
      id,
      success: true,
      result
    })
  } catch (error) {
    const errorMsg = error instanceof Error ? error.message : String(error)
    port.postMessage({
      type: 'media-manager-response',
      id,
      success: false,
      error: errorMsg
    })
  }
}

// 启动 worker 逻辑
console.log('[module-ffmpeg-core] Worker starting...')

if (workerProcess.parentPort) {
  workerProcess.parentPort.on('message', (e: MessageEvent) => {
    const msg = e.data

    if (msg.type === 'connect-discovery' && e.ports && e.ports[0]) {
      discoveryPort = e.ports[0]
      console.log('[module-ffmpeg-core] Discovery port connected')
      setupDiscoveryPort()
      registerSelf()
    }

    if (msg.type === 'init-functional') {
      console.log('[module-ffmpeg-core] Initializing functional plugin...')
    }

    if (msg.type === 'media-manager') {
      handleMediaManagerRequest(e)
    }
  })
}
