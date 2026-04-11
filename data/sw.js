var CACHE_NAME = 'rmesh-v1';
var PRE_CACHE = [
  '/', '/index.html', '/mobile.html', '/gp.html', '/dos.html',
  '/manifest.json', '/app.css', '/i18n.js', '/clock.js', '/ui.js', '/socket.js', '/connection.js',
  '/192.png', '/rMesh.png', '/logo.svg', '/announce.svg', '/favicon.ico',
  '/dos.png', '/edit.png', '/lora.png', '/mh.png', '/monitor.png',
  '/network.png', '/setup.png', '/tune.png', '/ok.wav', '/help.txt',
  '/icons/icon-192.png', '/icons/icon-512.png'
];

self.addEventListener('install', function (e) {
  self.skipWaiting();
  e.waitUntil(
    caches.open(CACHE_NAME).then(function (cache) {
      return cache.addAll(PRE_CACHE);
    })
  );
});

self.addEventListener('activate', function (e) {
  e.waitUntil(
    caches.keys().then(function (names) {
      return Promise.all(
        names.filter(function (n) { return n !== CACHE_NAME; })
             .map(function (n) { return caches.delete(n); })
      );
    }).then(function () {
      return self.clients.claim();
    })
  );
});

self.addEventListener('fetch', function (e) {
  if (e.request.method !== 'GET') return;
  var url = e.request.url;
  if (url.indexOf('/socket') !== -1) return;
  if (url.indexOf('messages.json') !== -1) return;

  e.respondWith(
    caches.open(CACHE_NAME).then(function (cache) {
      return cache.match(e.request).then(function (cached) {
        var fetched = fetch(e.request).then(function (response) {
          if (response && response.status === 200) {
            cache.put(e.request, response.clone());
          }
          return response;
        }).catch(function () {
          return cached;
        });
        return cached || fetched;
      });
    })
  );
});

self.addEventListener('message', function (e) {
  if (e.data && e.data.type === 'NEW_MESSAGE') {
    self.registration.showNotification(e.data.title, {
      body: e.data.body || '',
      tag:  e.data.tag  || 'rmesh',
      icon: '/icons/icon-192.png'
    });
  }
});

self.addEventListener('notificationclick', function (e) {
  e.notification.close();
  e.waitUntil(
    self.clients.matchAll({ type: 'window' }).then(function (clients) {
      for (var i = 0; i < clients.length; i++) {
        if (clients[i].visibilityState === 'visible') {
          return clients[i].focus();
        }
      }
      return self.clients.openWindow('/');
    })
  );
});
