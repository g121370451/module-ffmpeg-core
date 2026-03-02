import path from 'path'
import { MediaManager } from '../lib'
import type { PluginMetadata } from '../../../module-test/src/main/types/plugin-registry'

console.log('[module-ffmpeg-core] Worker starting...')

let discoveryPort: any = null
const connectedPlugins: Map<string, any> = new Map()

if (process.parentPort) {
  process.parentPort.on('message', (e) => {
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

function setupDiscoveryPort() {
  if (!discoveryPort) return

  discoveryPort.on('message', (e: any) => {
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
  if (!process.parentPort) return

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

  process.parentPort.postMessage({
    type: 'plugin-register',
    metadata
  })
}

function handleDiscovery(msg: any, ports: any[]) {
  const { plugins } = msg
  plugins.forEach((plugin: any, index: number) => {
    if (ports && ports[index]) {
      connectedPlugins.set(plugin.metadata.id, ports[index])
      console.log(`[module-ffmpeg-core] Connected to plugin: ${plugin.metadata.id}`)

      ports[index].on('message', (e: any) => {
        if (e.data.type === 'media-manager') {
          handleMediaManagerRequestFromPort(e, plugin.metadata.id, ports[index])
        }
      })
      ports[index].start()
    }
  })
}

function handleMediaManagerRequest(e: any) {
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
    } else if (process.parentPort) {
      process.parentPort.postMessage({
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
    } else if (process.parentPort) {
      process.parentPort.postMessage({
        type: 'media-manager-response',
        id,
        success: false,
        error: errorMsg
      })
    }
  }
}

function handleMediaManagerRequestFromPort(e: any, sourcePluginId: string, port: any) {
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
