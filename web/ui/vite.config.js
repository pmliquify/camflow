import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

const outputDir = process.env.CAMFLOW_WEB_OUT_DIR || 'dist';

export default defineConfig({
        plugins: [react()],
        build: {
                outDir: outputDir,
                emptyOutDir: true,
                sourcemap: false,
                cssCodeSplit: false,
                assetsInlineLimit: 0,
                rollupOptions: {
                        input: './index.html',
                        output: {
                                entryFileNames: 'app.js',
                                chunkFileNames: 'chunks/[name].js',
                                assetFileNames: (assetInfo) => {
                                        if (assetInfo.name && assetInfo.name.endsWith('.css')) {
                                                return 'app.css';
                                        }
                                        return 'assets/[name][extname]';
                                }
                        }
                }
        }
});
