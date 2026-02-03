/**
 * TypeLayout Video Generator
 * 
 * Generates a concept video from slides + TTS narration
 * 
 * Usage: node generate-video.js
 * 
 * Requirements:
 * - Node.js 18+
 * - FFmpeg installed and in PATH
 * - npm install (puppeteer, edge-tts)
 */

const puppeteer = require('puppeteer');
const { exec, execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const config = require('./scenes.config.js');

// Directories
const FRAMES_DIR = path.join(__dirname, 'frames');
const AUDIO_DIR = path.join(__dirname, 'audio');
const OUTPUT_DIR = path.join(__dirname, 'output');
const SLIDES_PATH = path.join(__dirname, '..', 'slides', 'index.html');

// Ensure directories exist
[FRAMES_DIR, AUDIO_DIR, OUTPUT_DIR].forEach(dir => {
    if (!fs.existsSync(dir)) {
        fs.mkdirSync(dir, { recursive: true });
    }
});

/**
 * Step 1: Capture slides as images using Puppeteer
 */
async function captureSlides() {
    console.log('📸 Capturing slides...');
    
    const browser = await puppeteer.launch({
        headless: 'new',
        args: ['--no-sandbox']
    });
    
    const page = await browser.newPage();
    await page.setViewport({
        width: config.resolution.width,
        height: config.resolution.height
    });
    
    // Load slides
    const slidesUrl = `file://${SLIDES_PATH}`;
    await page.goto(slidesUrl, { waitUntil: 'networkidle0' });
    
    // Wait for Reveal.js to initialize
    await page.waitForSelector('.reveal');
    await new Promise(r => setTimeout(r, 2000));
    
    // Get unique slide indices
    const slideIndices = [...new Set(config.scenes.map(s => s.slideIndex))].sort((a, b) => a - b);
    
    for (const index of slideIndices) {
        // Navigate to slide
        await page.evaluate((idx) => {
            Reveal.slide(idx);
        }, index);
        
        await new Promise(r => setTimeout(r, 500)); // Wait for transition
        
        // Capture screenshot
        const filename = path.join(FRAMES_DIR, `slide-${String(index).padStart(3, '0')}.png`);
        await page.screenshot({ path: filename, type: 'png' });
        console.log(`  ✓ Captured slide ${index}`);
    }
    
    await browser.close();
    console.log('✅ Slides captured\n');
}

/**
 * Step 2: Generate TTS audio for each scene
 */
async function generateTTS() {
    console.log('🎙️ Generating TTS audio...');
    
    const { voice, rate, pitch } = config.tts;
    
    // Try to find edge-tts executable
    const edgeTtsPaths = [
        'edge-tts',  // If in PATH
        'python -m edge_tts',  // Python module
        'C:\\Users\\gzsufanchen\\AppData\\Roaming\\Python\\Python311\\Scripts\\edge-tts.exe'  // Installed location
    ];
    
    let edgeTtsCmd = null;
    for (const p of edgeTtsPaths) {
        try {
            execSync(`${p} --version`, { stdio: 'pipe' });
            edgeTtsCmd = p;
            break;
        } catch (e) {
            // Try next
        }
    }
    
    if (!edgeTtsCmd) {
        console.error('  ✗ edge-tts not found. Please install: pip install edge-tts');
        console.log('  ℹ️ Skipping TTS generation, using scene durations only.\n');
        return;
    }
    
    console.log(`  Using: ${edgeTtsCmd}`);
    
    for (const scene of config.scenes) {
        const audioFile = path.join(AUDIO_DIR, `${scene.id}.mp3`);
        const text = scene.narration;
        
        // Use edge-tts CLI (Python version)
        const cmd = `${edgeTtsCmd} --voice "${voice}" --rate="${rate}" --pitch="${pitch}" --text "${text.replace(/"/g, '\\"')}" --write-media "${audioFile}"`;
        
        try {
            execSync(cmd, { stdio: 'pipe' });
            console.log(`  ✓ Generated audio for ${scene.id}`);
        } catch (err) {
            console.error(`  ✗ Failed to generate audio for ${scene.id}: ${err.message}`);
        }
    }
    
    console.log('✅ TTS audio generated\n');
}

/**
 * Step 3: Create video segments for each scene
 */
async function createSceneVideos() {
    console.log('🎬 Creating scene videos...');
    
    for (const scene of config.scenes) {
        const slideImage = path.join(FRAMES_DIR, `slide-${String(scene.slideIndex).padStart(3, '0')}.png`);
        const audioFile = path.join(AUDIO_DIR, `${scene.id}.mp3`);
        const outputFile = path.join(OUTPUT_DIR, `${scene.id}.mp4`);
        
        if (!fs.existsSync(slideImage)) {
            console.error(`  ✗ Missing slide image: ${slideImage}`);
            continue;
        }
        
        // Get audio duration or use scene duration
        let duration = scene.duration;
        if (fs.existsSync(audioFile)) {
            try {
                const probe = execSync(`ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "${audioFile}"`, { encoding: 'utf8' });
                const audioDuration = parseFloat(probe.trim());
                duration = Math.max(duration, audioDuration + (scene.pauseAfter || 0));
            } catch (e) {
                // Use default duration
            }
        }
        
        // Create video from image + audio
        let cmd;
        if (fs.existsSync(audioFile)) {
            cmd = `ffmpeg -y -loop 1 -i "${slideImage}" -i "${audioFile}" -c:v libx264 -tune stillimage -c:a aac -b:a 192k -pix_fmt yuv420p -t ${duration} -shortest "${outputFile}"`;
        } else {
            cmd = `ffmpeg -y -loop 1 -i "${slideImage}" -c:v libx264 -tune stillimage -pix_fmt yuv420p -t ${duration} -an "${outputFile}"`;
        }
        
        try {
            execSync(cmd, { stdio: 'pipe' });
            console.log(`  ✓ Created video for ${scene.id} (${duration.toFixed(1)}s)`);
        } catch (err) {
            console.error(`  ✗ Failed to create video for ${scene.id}`);
        }
    }
    
    console.log('✅ Scene videos created\n');
}

/**
 * Step 4: Concatenate all scene videos
 */
async function concatenateVideos() {
    console.log('🔗 Concatenating videos...');
    
    // Create file list
    const listFile = path.join(OUTPUT_DIR, 'files.txt');
    const fileList = config.scenes
        .map(scene => `file '${scene.id}.mp4'`)
        .join('\n');
    
    fs.writeFileSync(listFile, fileList);
    
    // Concatenate
    const finalOutput = path.join(OUTPUT_DIR, 'TypeLayout-Concept-Video.mp4');
    const cmd = `ffmpeg -y -f concat -safe 0 -i "${listFile}" -c copy "${finalOutput}"`;
    
    try {
        execSync(cmd, { stdio: 'pipe' });
        console.log(`✅ Final video created: ${finalOutput}\n`);
    } catch (err) {
        console.error('✗ Failed to concatenate videos:', err.message);
    }
    
    // Cleanup individual scene videos (optional)
    // config.scenes.forEach(scene => {
    //     fs.unlinkSync(path.join(OUTPUT_DIR, `${scene.id}.mp4`));
    // });
}

/**
 * Step 5: Add background music (optional)
 */
async function addBackgroundMusic() {
    const musicFile = path.join(__dirname, 'assets', 'background-music.mp3');
    if (!fs.existsSync(musicFile)) {
        console.log('ℹ️ No background music found, skipping...\n');
        return;
    }
    
    console.log('🎵 Adding background music...');
    
    const inputVideo = path.join(OUTPUT_DIR, 'TypeLayout-Concept-Video.mp4');
    const outputVideo = path.join(OUTPUT_DIR, 'TypeLayout-Concept-Video-Final.mp4');
    
    // Mix background music at lower volume
    const cmd = `ffmpeg -y -i "${inputVideo}" -i "${musicFile}" -filter_complex "[1:a]volume=0.15[bg];[0:a][bg]amix=inputs=2:duration=first:dropout_transition=2[a]" -map 0:v -map "[a]" -c:v copy -c:a aac "${outputVideo}"`;
    
    try {
        execSync(cmd, { stdio: 'pipe' });
        console.log(`✅ Background music added: ${outputVideo}\n`);
    } catch (err) {
        console.error('✗ Failed to add background music:', err.message);
    }
}

/**
 * Main execution
 */
async function main() {
    console.log('═══════════════════════════════════════════════════════════');
    console.log('  TypeLayout Video Generator');
    console.log('═══════════════════════════════════════════════════════════\n');
    
    const startTime = Date.now();
    
    try {
        await captureSlides();
        await generateTTS();
        await createSceneVideos();
        await concatenateVideos();
        await addBackgroundMusic();
        
        const elapsed = ((Date.now() - startTime) / 1000).toFixed(1);
        console.log('═══════════════════════════════════════════════════════════');
        console.log(`  ✅ Video generation complete! (${elapsed}s)`);
        console.log('═══════════════════════════════════════════════════════════\n');
        console.log(`Output: ${path.join(OUTPUT_DIR, 'TypeLayout-Concept-Video.mp4')}`);
        
    } catch (err) {
        console.error('❌ Video generation failed:', err);
        process.exit(1);
    }
}

main();
