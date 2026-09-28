// Entry point: node server/index.mjs
import { startServer } from './server.mjs';

const list = (v) => (v ? v.split(',').map((s) => s.trim()).filter(Boolean) : []);

startServer({
  port: Number(process.env.PORT ?? 8787),
  maxClients: Number(process.env.MAX_CLIENTS ?? 500),
  perIp: Number(process.env.MAX_PER_IP ?? 4),
  allowedOrigins: list(process.env.ALLOWED_ORIGINS),
  blockedWords: list(process.env.BLOCKED_WORDS),
});
