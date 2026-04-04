import sharp from "sharp";
import { join, dirname } from "path";
import { fileURLToPath } from "url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const pub = join(__dirname, "..", "public");

const orbSvg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <defs>
    <linearGradient id="g" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#5A9A30"/>
      <stop offset="100%" stop-color="#47802A"/>
    </linearGradient>
    <linearGradient id="e" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" stop-color="#ffffff"/>
      <stop offset="100%" stop-color="#f0f0f0"/>
    </linearGradient>
  </defs>
  <circle cx="50" cy="50" r="40" fill="url(#g)"/>
  <ellipse cx="36" cy="47" rx="8" ry="10" fill="url(#e)"/>
  <circle cx="34" cy="43" r="2" fill="white"/>
  <circle cx="36" cy="45" r="0.8" fill="white" opacity="0.7"/>
  <ellipse cx="64" cy="47" rx="8" ry="10" fill="url(#e)"/>
  <circle cx="62" cy="43" r="2" fill="white"/>
  <circle cx="64" cy="45" r="0.8" fill="white" opacity="0.7"/>
</svg>`;

const whiteOrb = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <circle cx="50" cy="50" r="40" fill="white"/>
  <ellipse cx="36" cy="47" rx="8" ry="10" fill="#0a0a0a"/>
  <ellipse cx="64" cy="47" rx="8" ry="10" fill="#0a0a0a"/>
</svg>`;

async function generateFavicons() {
  for (const size of [16, 32, 48]) {
    await sharp(Buffer.from(orbSvg))
      .resize(size, size)
      .png()
      .toFile(join(pub, `favicon-${size}.png`));
  }
  await sharp(Buffer.from(orbSvg))
    .resize(180, 180)
    .png()
    .toFile(join(pub, "apple-touch-icon.png"));
  console.log("Favicons generated.");
}

async function generateOgImage() {
  const w = 1200,
    h = 630;
  const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}">
    <defs>
      <radialGradient id="g1" cx="30%" cy="40%" r="60%">
        <stop offset="0%" stop-color="rgba(122,182,72,0.15)"/>
        <stop offset="100%" stop-color="transparent"/>
      </radialGradient>
      <radialGradient id="g2" cx="75%" cy="65%" r="50%">
        <stop offset="0%" stop-color="rgba(59,130,246,0.08)"/>
        <stop offset="100%" stop-color="transparent"/>
      </radialGradient>
    </defs>
    <rect width="${w}" height="${h}" fill="#0a0a0a"/>
    <rect width="${w}" height="${h}" fill="url(#g1)"/>
    <rect width="${w}" height="${h}" fill="url(#g2)"/>

    <g transform="translate(100,180)">
      ${whiteOrb.replace('viewBox="0 0 100 100"', 'viewBox="0 0 100 100" width="80" height="80"')}
    </g>

    <text x="210" y="220" fill="#f5f5f7" font-family="system-ui,-apple-system,sans-serif" font-size="56" font-weight="700" letter-spacing="-2">Bring AI to every</text>
    <text x="210" y="285" fill="#f5f5f7" font-family="system-ui,-apple-system,sans-serif" font-size="56" font-weight="700" letter-spacing="-2">device on Earth.</text>

    <text x="210" y="345" fill="#a1a1a6" font-family="system-ui,-apple-system,sans-serif" font-size="24" letter-spacing="1.5">not quite human.</text>

    <text x="210" y="400" fill="#86868b" font-family="system-ui,-apple-system,sans-serif" font-size="20">~1.7 MB binary · 50+ providers · 38 channels · 83+ tools</text>

    <text x="210" y="450" fill="#7AB648" font-family="system-ui,-apple-system,sans-serif" font-size="20" font-weight="600">h-uman</text>
    <text x="340" y="450" fill="#6e6e73" font-family="system-ui,-apple-system,sans-serif" font-size="18">Pure C11 · Zero dependencies · MIT licensed</text>

    <line x1="210" y1="490" x2="1000" y2="490" stroke="rgba(255,255,255,0.06)" stroke-width="1"/>
    <text x="210" y="530" fill="#86868b" font-family="ui-monospace,monospace" font-size="14">h-uman.ai</text>
  </svg>`;

  await sharp(Buffer.from(svg)).png().toFile(join(pub, "og-image.png"));
  console.log("OG image generated.");
}

await generateFavicons();
await generateOgImage();
console.log("All assets generated.");
