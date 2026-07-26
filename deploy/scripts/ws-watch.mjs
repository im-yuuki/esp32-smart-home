// Prints every /topic/events STOMP frame — the terminal view of what the web UI sees.
// Setup once:  cd deploy/scripts && npm i
// Usage:       node ws-watch.mjs [ws://localhost:8080/ws/websocket]
// (Node 26 has native WebSocket; the raw-WebSocket SockJS transport lives at {endpoint}/websocket.)
import { Client } from '@stomp/stompjs';

const url = process.argv[2] ?? 'ws://localhost:8080/ws/websocket';
const c = new Client({
  brokerURL: url,
  reconnectDelay: 2000,
  onConnect: () => {
    console.log('connected', url);
    c.subscribe('/topic/events', (m) => console.log(new Date().toISOString(), m.body));
  },
  onStompError: (f) => console.error('STOMP error', f.headers, f.body),
  onWebSocketClose: () => console.log('socket closed, retrying...'),
});
c.activate();
