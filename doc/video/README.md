# TypeLayout Video Generator

自动生成 TypeLayout 概念视频的工具。

## 前置条件

### 1. 安装 Node.js
确保已安装 Node.js 18+：
```bash
node --version  # 应显示 v18.x.x 或更高
```

### 2. 安装 FFmpeg
FFmpeg 必须在系统 PATH 中：

**Windows (使用 Chocolatey):**
```bash
choco install ffmpeg
```

**Windows (手动安装):**
1. 从 https://ffmpeg.org/download.html 下载
2. 解压到 `C:\ffmpeg`
3. 将 `C:\ffmpeg\bin` 添加到系统 PATH

验证安装：
```bash
ffmpeg -version
```

### 3. 安装 edge-tts
```bash
pip install edge-tts
```

验证安装：
```bash
edge-tts --list-voices
```

### 4. 安装项目依赖
```bash
cd doc/video
npm install
```

## 使用方法

### 一键生成视频
```bash
npm run generate
```

这将执行以下步骤：
1. 📸 使用 Puppeteer 截取幻灯片
2. 🎙️ 使用 Edge TTS 生成配音
3. 🎬 使用 FFmpeg 合成场景视频
4. 🔗 拼接所有场景为完整视频
5. 🎵 添加背景音乐（如果有）

### 输出文件
```
doc/video/output/
├── TypeLayout-Concept-Video.mp4      # 最终视频
├── TypeLayout-Concept-Video-Final.mp4 # 带背景音乐版本（如果有）
├── hook-1.mp4                        # 各场景片段
├── hook-2.mp4
└── ...
```

## 配置

### 修改场景内容
编辑 `scenes.config.js`：
- `duration`: 场景时长（秒）
- `narration`: 配音文本
- `slideIndex`: 使用的幻灯片索引

### 修改 TTS 配音
在 `scenes.config.js` 中调整：
```javascript
tts: {
    voice: "en-US-GuyNeural",  // 语音选择
    rate: "+0%",               // 语速 (-50% ~ +50%)
    pitch: "+0Hz"              // 音调
}
```

可用语音列表：
```bash
edge-tts --list-voices | grep en-US
```

推荐语音：
- `en-US-GuyNeural` - 男声，专业
- `en-US-JennyNeural` - 女声，自然
- `en-US-AriaNeural` - 女声，新闻风格

### 添加背景音乐
将 MP3 文件放置为：
```
doc/video/assets/background-music.mp3
```

视频生成时会自动混入（音量降至 15%）。

## 目录结构

```
doc/video/
├── generate-video.js    # 主生成脚本
├── scenes.config.js     # 场景配置
├── package.json         # 依赖管理
├── README.md            # 本文件
├── script.md            # 详细脚本（参考用）
├── assets/              # 静态资源
│   ├── memory-layout.svg
│   ├── cross-platform.svg
│   └── background-music.mp3 (可选)
├── frames/              # 生成的幻灯片截图
├── audio/               # 生成的 TTS 音频
└── output/              # 最终视频输出
```

## 故障排除

### "edge-tts: command not found"
确保 Python Scripts 目录在 PATH 中：
```bash
pip show edge-tts  # 查看安装位置
```

### "ffmpeg: command not found"
重新安装 FFmpeg 或手动添加到 PATH。

### 幻灯片截图失败
检查 `../slides/index.html` 是否存在且可访问。

### 视频无声音
检查 `audio/` 目录是否有 MP3 文件生成。

## 手动流程（备选）

如果自动生成失败，可以手动执行：

```bash
# 1. 截取幻灯片（手动用浏览器打开 slides/index.html 并截图）

# 2. 生成配音
edge-tts --voice "en-US-GuyNeural" --text "Your text" --write-media audio.mp3

# 3. 合成视频
ffmpeg -loop 1 -i slide.png -i audio.mp3 -c:v libx264 -c:a aac -t 10 output.mp4

# 4. 拼接视频
ffmpeg -f concat -i files.txt -c copy final.mp4
```

## 自定义扩展

### 添加转场效果
在场景之间添加淡入淡出：
```bash
ffmpeg -i input.mp4 -vf "fade=in:0:30,fade=out:270:30" output.mp4
```

### 添加字幕
1. 创建 SRT 文件
2. 使用 FFmpeg 烧录：
```bash
ffmpeg -i video.mp4 -vf subtitles=subs.srt output.mp4
```

### 调整分辨率
在 `scenes.config.js` 中修改：
```javascript
resolution: { width: 1280, height: 720 }  // 720p
```
