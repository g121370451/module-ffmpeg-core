import fs from 'fs'
import path from 'path'
import AdmZip from 'adm-zip'

const pluginJson = JSON.parse(fs.readFileSync('plugin.json', 'utf-8'))
const version = pluginJson.version
const id = pluginJson.id

const distDir = path.resolve('dist')

// 1. 校验必要文件
const requiredFiles = ['index.js']
for (const file of requiredFiles) {
  if (!fs.existsSync(path.join(distDir, file))) {
    console.error(`[pack] Missing required file: dist/${file}`)
    process.exit(1)
  }
}

const nodeFiles = fs.readdirSync(distDir).filter(f => f.endsWith('.node'))
if (nodeFiles.length === 0) {
  console.error('[pack] Missing .node native module in dist/')
  process.exit(1)
}

// 2. 打包 zip
console.log('[pack] Creating zip...')
const zip = new AdmZip()

const distFiles = fs.readdirSync(distDir)
for (const file of distFiles) {
  const filePath = path.join(distDir, file)
  if (fs.statSync(filePath).isFile()) {
    zip.addLocalFile(filePath)
  }
}

zip.addLocalFile(path.resolve('plugin.json'))

const zipName = `${id}-v${version}.zip`
zip.writeZip(path.resolve(zipName))

console.log(`[pack] Done! Output: ${zipName}`)
