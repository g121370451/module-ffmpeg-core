// NativeLoader.ts
import fs from 'fs';
import path from 'path';
import { createRequire } from 'module';

export interface LoaderResult<T> {
    instance: T;
    path: string;
}

export class NativeLoader {
    /**
     * 检查库的完整性
     * @param distPath dist 目录的绝对路径
     */
    public static checkIntegrity(distPath: string): boolean {
        const requiredFiles = ['index.js', 'module-ffmplayer-core.node'];
        return requiredFiles.every(file => {
            const p = path.join(distPath, file)
            return fs.existsSync(p);
        });
    }

    /**
     * 使用绝对路径动态加载
     * @param distPath dist 目录的绝对路径
     */
    public static load<T>(distPath: string): T {
        const entryPath = path.join(distPath, 'index.js');

        if (!this.checkIntegrity(distPath)) {
            throw new Error(`[Loader] 校验失败: 路径 ${distPath} 不完整`);
        }

        // 1. 清除 require 缓存以支持热更新
        if (require.cache[entryPath]) {
            delete require.cache[entryPath];
        }

        // 2. 在 entryPath 目录下创建一个 require 环境
        const customRequire = createRequire(entryPath);

        // 3. 执行加载
        // 这里返回的就是你看到的 { MediaManager: [class MediaManager] }
        const moduleExports = customRequire(entryPath);

        // 4. 强制断言为泛型 T 供外部使用
        return moduleExports as T;
    }
}