import { createLogger, defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

const outputDir = process.env.CAMFLOW_WEB_OUT_DIR || 'dist';
const devPort = Number(process.env.CAMFLOW_WEB_DEV_PORT || '8081');
const apiTarget = process.env.CAMFLOW_WEB_API_TARGET || 'http://127.0.0.1:8000';
const viteLogger = createLogger();
const viteLoggerError = viteLogger.error;

viteLogger.error = (msg, options) => {
        if (typeof msg === 'string' && msg.includes('[vite] http proxy error')) {
                return;
        }
        viteLoggerError(msg, options);
};

function isExpectedProxyDisconnect(error) {
        const code = String(error?.code || '').toUpperCase();
        const message = String(error?.message || '').toLowerCase();
        return code === 'ECONNREFUSED' || code === 'ECONNRESET' || message.includes('socket hang up');
}

function proxyErrorHandler(error, req, res) {
        if (isExpectedProxyDisconnect(error)) {
                if (res && !res.headersSent) {
                        res.writeHead(503, { 'Content-Type': 'application/json' });
                        res.end('{"error":"runtime-unreachable"}');
                }
                return;
        }

        if (res && !res.headersSent) {
                res.writeHead(502, { 'Content-Type': 'application/json' });
                res.end('{"error":"proxy-error"}');
        }
}

export default defineConfig({
        customLogger: viteLogger,
        plugins: [react()],
        server: {
                host: true,
                port: devPort,
                strictPort: true,
                proxy: {
                        '/api': {
                                target: apiTarget,
                                changeOrigin: true,
                                configure: (proxy) => {
                                        proxy.removeAllListeners('error');
                                        proxy.on('error', proxyErrorHandler);
                                }
                        },
                        '/ws': {
                                target: apiTarget.replace('http://', 'ws://').replace('https://', 'wss://'),
                                ws: true,
                                changeOrigin: true,
                                configure: (proxy) => {
                                        proxy.removeAllListeners('error');
                                        proxy.on('error', proxyErrorHandler);
                                }
                        }
                }
        },
        build: {
                outDir: outputDir,
                emptyOutDir: true,
                sourcemap: false,
                cssCodeSplit: false,
                assetsInlineLimit: 0,
                rollupOptions: {
                        input: './index.html',
                        output: {
                                entryFileNames: 'assets/app.js',
                                chunkFileNames: 'assets/[name].js',
                                assetFileNames: (assetInfo) => {
                                        if (assetInfo.name && assetInfo.name.endsWith('.css')) {
                                                return 'assets/app.css';
                                        }
                                        return 'assets/[name][extname]';
                                }
                        }
                }
        }
});
