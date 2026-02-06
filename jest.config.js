// jest.config.js
const path = require('path');

module.exports = {
  testEnvironment: 'node',

  // 1. 告诉 Jest 根目录是哪里（项目根目录）
  rootDir: '.',

  // 2. 告诉 Jest 测试文件去哪里找（编译后的目录）
  roots: ['<rootDir>/test_dist'],

  // 3. [关键修改] 匹配规则
  // 意思为：在 test_dist/test/ 目录下的所有 .js 文件，都当作测试文件运行
  // 这样 test_player.js 也能被识别了，不需要改成 test_player.test.js
  testMatch: [
    '<rootDir>/test_dist/test/**/*.js',
    '<rootDir>/test_dist/test/**/*.mp4'
  ],

  // 4. 忽略非测试逻辑的辅助文件（如果有 lib 混在里面的话）
  testPathIgnorePatterns: [
    '/node_modules/',
    '/test_dist/lib/' 
  ],

  moduleFileExtensions: ['js','mp4'],

  // 视频操作可能耗时，给 30秒
  testTimeout: 30000,
  
  verbose: true
};