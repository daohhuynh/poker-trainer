// Poker Trainer service worker.
//
// Versioned runtime cache of fetched static assets (ARCHITECTURE Module 3
// "Service Worker Caching"): after the first visit, subsequent visits serve
// cached assets, cutting time-to-interactive for returning users. The cache key
// is versioned, so bumping CACHE_VERSION on an app update invalidates every
// stale asset (the activate handler deletes prior caches).
//
// Registered on boot by Z05 (boot.cpp). The source of record lives under
// src/bridge/; the build/deploy step copies it next to the app bundle so it is
// served from the web root (a service worker can only control pages at or below
// its own scope).

const CACHE_VERSION = 'poker-trainer-v1';

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

self.addEventListener('fetch', (event) => {
  const request = event.request;
  // Only cache same-origin GETs (the wasm/js bundle, PNGs, audio). Let the
  // browser handle everything else (POSTs, cross-origin, etc.) normally.
  if (
    request.method !== 'GET' ||
    new URL(request.url).origin !== self.location.origin
  ) {
    return;
  }
  // Cache-first: serve a cached copy when present, otherwise fetch from the
  // network and populate the cache for next time.
  event.respondWith(
    caches.open(CACHE_VERSION).then((cache) =>
      cache.match(request).then((hit) => {
        if (hit) {
          return hit;
        }
        return fetch(request).then((response) => {
          if (response && response.ok) {
            cache.put(request, response.clone());
          }
          return response;
        });
      })
    )
  );
});
