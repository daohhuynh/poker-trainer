// Poker Trainer service worker.
//
// Versioned runtime cache of fetched static assets (ARCHITECTURE Module 3
// "Service Worker Caching"): after the first visit, subsequent visits serve
// cached assets, cutting time-to-interactive for returning users. The cache key
// is versioned, so bumping CACHE_VERSION on an app update invalidates every
// stale asset (the activate handler deletes prior caches).
//
// Caching strategy is split by request kind so a fresh build is never masked by
// a stale cache:
//
//   - Core documents (the HTML navigation, poker_trainer.js, poker_trainer.wasm)
//     are served NETWORK-FIRST. A rebuilt bundle reuses the same filenames, so a
//     pure cache-first policy would pin the old build forever under a constant
//     CACHE_VERSION. Network-first re-fetches the core docs every load, so a
//     fresh build loads on a plain reload; the cached copy is kept only as an
//     offline fallback.
//   - Static assets (PNGs, audio) are served CACHE-FIRST for the returning-user
//     speed the spec wants. They are invalidated the standard way: an app update
//     bumps CACHE_VERSION and the activate handler drops the prior cache.
//
// Together these preserve the spec's property — a versioned cache whose entries
// are invalidated by app updates — while guaranteeing the core bundle is never
// served stale.
//
// Registered on boot by Z05 (boot.cpp). The source of record lives under
// src/bridge/; the build/deploy step copies it next to the app bundle so it is
// served from the web root (a service worker can only control pages at or below
// its own scope).

const CACHE_VERSION = 'poker-trainer-v2';

self.addEventListener('install', (event) => {
  // Activate the new worker immediately rather than waiting for old tabs.
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  // Drop every cache that is not the current version, then take control.
  event.waitUntil(
    caches
      .keys()
      .then((keys) =>
        Promise.all(
          keys
            .filter((key) => key !== CACHE_VERSION)
            .map((key) => caches.delete(key))
        )
      )
      .then(() => self.clients.claim())
  );
});

// A core document is the HTML shell (any navigation) or the app bundle
// (poker_trainer.js / poker_trainer.wasm). These reuse the same filenames across
// builds, so they must come from the network to pick up a rebuild. Everything
// else (PNGs, audio) is a static asset served cache-first.
function is_core_document(request, url) {
  if (request.mode === 'navigate') {
    return true;
  }
  return /\.(?:js|wasm)$/.test(url.pathname);
}

// Network-first: prefer a fresh response, refresh the cache for offline use, and
// fall back to the cached copy only when the network is unreachable. A network
// response that the server actually returned (even an error status) is passed
// through unchanged so real failures are visible rather than masked by a stale
// cache.
function network_first(cache, request) {
  return fetch(request)
    .then((response) => {
      if (response && response.ok) {
        cache.put(request, response.clone());
      }
      return response;
    })
    .catch((err) =>
      cache.match(request).then((hit) => {
        if (hit) {
          return hit;
        }
        throw err;
      })
    );
}

// Cache-first: serve a cached copy when present, otherwise fetch from the
// network and populate the cache for next time.
function cache_first(cache, request) {
  return cache.match(request).then((hit) => {
    if (hit) {
      return hit;
    }
    return fetch(request).then((response) => {
      if (response && response.ok) {
        cache.put(request, response.clone());
      }
      return response;
    });
  });
}

self.addEventListener('fetch', (event) => {
  const request = event.request;
  const url = new URL(request.url);
  // Only cache same-origin GETs (the wasm/js bundle, PNGs, audio). Let the
  // browser handle everything else (POSTs, cross-origin, etc.) normally.
  if (request.method !== 'GET' || url.origin !== self.location.origin) {
    return;
  }
  event.respondWith(
    caches.open(CACHE_VERSION).then((cache) =>
      is_core_document(request, url)
        ? network_first(cache, request)
        : cache_first(cache, request)
    )
  );
});
