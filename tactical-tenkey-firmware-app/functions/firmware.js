export async function onRequestGet(context) {
  const REPO = 'sammothxc/tactical-tenkey';

  // GitHub API REQUIRES a User-Agent or it 403s — the browser sends one
  // automatically, but a Worker/Pages fetch does not.
  const ghHeaders = {
    'Accept': 'application/vnd.github+json',
    'User-Agent': 'tactical-tenkey-firmware-app'
  };

  const rel = await fetch(
    `https://api.github.com/repos/${REPO}/releases/latest`,
    { headers: ghHeaders, cf: { cacheTtl: 300 } }
  );
  if (!rel.ok) return new Response(`GitHub API ${rel.status}`, { status: 502 });

  const release = await rel.json();
  const asset = release.assets.find(a => a.name === 'firmware.bin');
  if (!asset) return new Response('firmware.bin not found', { status: 404 });

  // fetch() follows the 302 to the storage bucket server-side
  const bin = await fetch(asset.browser_download_url, {
    headers: { 'User-Agent': 'tactical-tenkey-firmware-app' }
  });
  if (!bin.ok) return new Response(`asset fetch ${bin.status}`, { status: 502 });

  return new Response(bin.body, {
    status: 200,
    headers: {
      'Content-Type': 'application/octet-stream',
      'Cache-Control': 'public, max-age=300'
    }
  });
}